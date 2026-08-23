// 4klang synth - C port of 4klang.asm
//
// The original is 32-bit x87 assembly. Two pieces of that machine model have
// to survive the port:
//
//   * The FPU register stack. Every unit reads and writes it, and the FOP unit
//     exists purely to shuffle it, so it is emulated here (fpush/fpop/fst/...)
//     and used exactly where the assembly used it.
//
//   * The 80-bit intermediates. x87 registers carry a 64-bit mantissa while
//     memory operands are 32-bit floats, and the synth leans on the gap: the
//     LFO phase accumulators add increments around 1e-5 to a value that has
//     just had 1.0 added to it, which in float would quantise the increment
//     away entirely. So intermediates here are double, and values are rounded
//     to float only where the assembly actually stored a dword. Those stores
//     are called out with a `// stored as float` comment, because whether a
//     later read sees the rounded or the unrounded value is audible.
//
// The one deliberate resource change: the delay buffer holds MAX_DELAY_LINES
// lines rather than the original's blanket 16*16, which at MAX_DELAY samples
// each came to 67 MB of bss.

#include <math.h>
#include <string.h>
#include <stdint.h>

#include "4klang_song.h"

#ifndef SAMPLE_TYPE
	#ifdef GO4K_USE_16BIT_OUTPUT
		#define SAMPLE_TYPE short
	#else
		#define SAMPLE_TYPE float
	#endif
#endif

// One delay line costs MAX_DELAY floats, so this is sized to the song instead.
// It needs 25 lines, plus one more for the trailing dc filter, which reads the
// line just past the last one used.
#ifndef MAX_DELAY_LINES
	#define MAX_DELAY_LINES 32
#endif

// A synth workspace dword is read as float or as int depending on the unit
// that owns it, exactly as in the assembly. A union keeps that legal in C.
typedef union {
	float	f;
	int32_t	i;
} dword_t;

// ---------------------------------------------------------------------------
// constants, named after the assembly's data labels
// ---------------------------------------------------------------------------
static const float c_i128			= 0.0078125f;
static const float c_0_5			= 0.5f;
static const float c_24				= 24.0f;
static const float c_32767			= 32767.0f;
// R = 1 - (pi*2 * frequency / samplerate)
static const float c_dc_const		= 0.99609375f;
// 0x3DAAAAAA, the assembly's float literal for 1/12
static const float c_i12			= 0.0833333284f;
// 220.0 / (2^(69/12)) / 44100.0
static const float FREQ_NORMALIZE	= 0.000092696138f;
static const double c_2pi			= 6.283185307179586476925286766559;

// exported by the assembly for tools to poke at; kept mutable for the same reason
float	LFO_NORMALIZE	= DEF_LFO_NORMALIZE;
int32_t	RandSeed		= 1;

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------
static float	go4k_transformed_values[16];	// the assembly stored these as dwords
static dword_t	go4k_synth_wrk[GO4K_SYNTH_DWORDS];
static dword_t	go4k_delay_buffer[MAX_DELAY_LINES * GO4K_DLL_WRK_DWORDS];
static int		go4k_delay_buffer_ofs;			// in dwords, reset every sample

static const uint8_t *COM;						// opcode cursor    (ebx)
static const uint8_t *VAL;						// parameter cursor (esi)

// ---------------------------------------------------------------------------
// the x87 register stack
// ---------------------------------------------------------------------------
#define GO4K_FPU_STACK_SIZE 16
static double	fpu[GO4K_FPU_STACK_SIZE];
static int		fpu_depth;

static void   fpush(double v)		{ fpu[fpu_depth++] = v; }
static double fpop(void)			{ return fpu[--fpu_depth]; }
static double fst(int i)			{ return fpu[fpu_depth - 1 - i]; }
static void   fset(int i, double v)	{ fpu[fpu_depth - 1 - i] = v; }

// `fxch` - swap st0 and st1
static void fxch(void)
{
	double t = fpu[fpu_depth - 1];
	fpu[fpu_depth - 1] = fpu[fpu_depth - 2];
	fpu[fpu_depth - 2] = t;
}

// ---------------------------------------------------------------------------
// crt emulation
// ---------------------------------------------------------------------------
static double FloatRandomNumber(void)
{
	RandSeed = (int32_t)((uint32_t)RandSeed * 16007u);
	// c_RandDiv is 65536*32768 == 0x80000000, and `fidiv` reads that dword as
	// a *signed* integer, so the divisor is really INT_MIN. The resulting sign
	// flip does not matter for noise, but the port keeps it.
	return (double)RandSeed / -2147483648.0;
}

// Power@0 - 2^x
static double go4kPower(double x)
{
	return exp2(x);
}

// ---------------------------------------------------------------------------
// unit values preparation/transform
// ---------------------------------------------------------------------------
// Consumes `count` raw parameter bytes and scales them into 0..2. Callers that
// need a parameter's raw byte read it back through VAL[-n], the way the
// assembly re-read it relative to esi.
static const float *go4kTransformValues(int count)
{
	for (int i = 0; i < count; ++i)
		go4k_transformed_values[i] = (float)(*VAL++) * c_i128;
	return go4k_transformed_values;
}

#ifdef INCLUDE_WAVESHAPER
// ---------------------------------------------------------------------------
// Waveshaper
// ---------------------------------------------------------------------------
// In : st0 = shaping coeff, st1 = input.  Out: st0 = result.
static void go4kWaveshaper(void)
{
	double amnt = fpop();
	double in   = fpop();

#ifdef GO4K_USE_WAVESHAPER_CLIP
	// `jbe` also fires when the compare is unordered, so nan clips high
	if (!(1.0 > in))
		in = 1.0;
	else if (!(-1.0 < in))
		in = -1.0;
#endif

	amnt = (amnt - c_0_5) * 2.0;
	// the assembly spilled amnt to a dword and reloaded it for the divisor
	// only, so the numerator keeps the unrounded value
	float amnt_stored = (float)amnt;					// stored as float
	double k = (amnt + amnt) / (1.0 - amnt_stored);

	fpush(in * (1.0 + k) / (1.0 + k * fabs(in)));
}
#endif

// ---------------------------------------------------------------------------
// ENV
// ---------------------------------------------------------------------------
static double go4kENVMap(const float *val, const dword_t *wrk, int index)
{
	double x = val[index];
#ifdef GO4K_USE_ENV_MOD_ADR
	// am, dm, sm, rm sit consecutively, so the parameter index doubles as the
	// modulation slot index
	x += wrk[go4kENV_wrk_am + index].f;
#endif
	return go4kPower(-(x * c_24));
}

static void go4kENV_func(dword_t *wrk, dword_t *base)
{
	const float *val = go4kTransformValues(go4kENV_val_NUM);

#ifdef GO4K_USE_ENV_CHECK
	// check if current note still active
	if (base[-1].i == 0) {
		fpush(0.0);
		return;
	}
#endif

	// is the instrument in release mode (note off)?
	if (base[-2].i != 0)
		wrk[go4kENV_wrk_state].i = ENV_STATE_RELEASE;

	int    state = wrk[go4kENV_wrk_state].i;
	double level = wrk[go4kENV_wrk_level].f;
	double newval;

	switch (state) {
	case ENV_STATE_SUSTAIN:
		// holds, and deliberately leaves the stored level alone
		fpush(level);
		goto apply_gain;

	case ENV_STATE_ATTAC:
		newval = level + go4kENVMap(val, wrk, go4kENV_val_attac);
		level  = (1.0 > newval) ? newval : 1.0;
		if (!(1.0 > newval))
			++state;
		break;

	case ENV_STATE_DECAY: {
		double sustain = val[go4kENV_val_sustain];
		newval = level - go4kENVMap(val, wrk, go4kENV_val_decay);
		level  = (sustain < newval) ? newval : sustain;
		if (!(sustain < newval))
			++state;
		break;
	}

	case ENV_STATE_RELEASE:
		newval = level - go4kENVMap(val, wrk, go4kENV_val_release);
		level  = (0.0 < newval) ? newval : 0.0;
		if (!(0.0 < newval))
			++state;
		break;

	default:
		// ENV_STATE_OFF. The assembly reached its `fstp st1` here having pushed
		// only one value, so it dropped the caller's top of stack instead of a
		// candidate of its own - reproduced by overwriting rather than pushing.
		// Unreachable in practice: go4kRenderVoices clears the note as soon as
		// a voice turns OFF, and the note check above then short-circuits.
		fset(0, level);
		wrk[go4kENV_wrk_level].f = (float)level;		// stored as float
		goto apply_gain;
	}

	wrk[go4kENV_wrk_state].i = state;
	wrk[go4kENV_wrk_level].f = (float)level;			// stored as float
	fpush(level);										// but passed on unrounded

apply_gain:;
	double gain = val[go4kENV_val_gain];
#ifdef GO4K_USE_ENV_MOD_GM
	gain += wrk[go4kENV_wrk_gm].f;
#endif
	fset(0, fst(0) * gain);
}

// ---------------------------------------------------------------------------
// VCO
// ---------------------------------------------------------------------------
// Each waveform helper consumes (st0 = colour, st1 = phase) and leaves one
// value, so several flags at once chain the way the assembly chained them.
static void go4kVCO_sine(void)
{
	double c = fpop(), p = fpop();
	if (!(c >= p)) {
		fpush(0.0);
		return;
	}
	fpush(sin(c_2pi * (p / c)));
}

static void go4kVCO_trisaw(void)
{
	double c = fpop(), p = fpop();
	if (!(c >= p)) {
		p = 1.0 - p;
		c = 1.0 - c;
	}
	double v = p / c;
	fpush((v + v) - 1.0);
}

static void go4kVCO_pulse(void)
{
	double c = fpop(), p = fpop();
	fpush((c >= p) ? 1.0 : -1.0);
}

static void go4kVCO_noise(void)
{
	double r = FloatRandomNumber();
	fpop();
	fpop();
	fpush(r);
}

static void go4kVCO_func(dword_t *wrk, dword_t *base)
{
	const float *val = go4kTransformValues(go4kVCO_val_NUM);
	int flags = VAL[-1];

#ifdef GO4K_USE_VCO_CHECK
	// check if current note still active
	if (base[-1].i == 0) {
		fpush(0.0);
		return;
	}
#endif

	double x = val[go4kVCO_val_transpose] - c_0_5;
#ifdef GO4K_USE_VCO_MOD_TM
	x += wrk[go4kVCO_wrk_tm].f;
#endif
	x /= c_i128;
	x += (val[go4kVCO_val_detune] - c_0_5) * 2.0;
#ifdef GO4K_USE_VCO_MOD_DM
	x += wrk[go4kVCO_wrk_dm].f;
#endif
	// x is now the transpose+detune offset, in semitones
	if (!(flags & LFO))
		x += (double)base[-1].i;

	double freq = go4kPower(x * c_i12);
	freq *= (flags & LFO) ? LFO_NORMALIZE : FREQ_NORMALIZE;

	double phase = freq + wrk[go4kVCO_wrk_phase].f;
#ifdef GO4K_USE_VCO_MOD_FM
	phase += wrk[go4kVCO_wrk_fm].f;
#endif
	// `fprem` against 1.0. The +1 pulls a slightly negative phase back into
	// range, and is why this has to be done wider than float: an LFO increment
	// is small enough that float would round it straight back off again.
	phase = fmod(phase + 1.0, 1.0);
	wrk[go4kVCO_wrk_phase].f = (float)phase;			// stored as float

#ifdef GO4K_USE_VCO_MOD_PM
	phase += wrk[go4kVCO_wrk_pm].f;
#endif
#ifdef GO4K_USE_VCO_PHASE_OFFSET
	phase += val[go4kVCO_val_phaseofs];
#endif
#ifdef PHASE_RENORMALIZE
	phase = fmod(phase + 1.0, 1.0);
#endif

	double color = val[go4kVCO_val_color];
#ifdef GO4K_USE_VCO_MOD_CM
	color += wrk[go4kVCO_wrk_cm].f;
#endif

	fpush(phase);
	fpush(color);
	if (flags & SINE)	go4kVCO_sine();
	if (flags & TRISAW)	go4kVCO_trisaw();
	if (flags & PULSE)	go4kVCO_pulse();
	if (flags & NOISE)	go4kVCO_noise();

#ifdef GO4K_USE_VCO_SHAPE
	double shape = val[go4kVCO_val_shape];
	#ifdef GO4K_USE_VCO_MOD_SM
	shape += wrk[go4kVCO_wrk_sm].f;
	#endif
	fpush(shape);
	go4kWaveshaper();
#endif

	double gain = val[go4kVCO_val_gain];
#ifdef GO4K_USE_VCO_MOD_GM
	gain += wrk[go4kVCO_wrk_gm].f;
#endif
	fset(0, fst(0) * gain);
}

// ---------------------------------------------------------------------------
// VCF - a state variable filter, all four taps available at once
// ---------------------------------------------------------------------------
static void go4kVCF_func(dword_t *wrk, dword_t *base)
{
	const float *val = go4kTransformValues(go4kVCF_val_NUM);

#ifdef GO4K_USE_VCF_CHECK
	// check if current note still active - note this leaves the input alone
	// rather than pushing silence, unlike ENV and VCO
	if (base[-1].i == 0)
		return;
#endif

	int type = VAL[-1];

	double res = val[go4kVCF_val_res];
#ifdef GO4K_USE_VCF_MOD_RM
	res += wrk[go4kVCF_wrk_rm].f;
#endif
	double freq = val[go4kVCF_val_freq];
#ifdef GO4K_USE_VCF_MOD_FM
	freq += wrk[go4kVCF_wrk_fm].f;
#endif
	// squared so it never goes negative, and so the low end behaves more
	// smoothly. Both coefficients get spilled to dwords before the filter runs.
	float res_c  = (float)res;							// stored as float
	float freq_c = (float)(freq * freq);				// stored as float

	double in   = fpop();
	float  band = wrk[go4kVCF_wrk_band].f;

	double low  = (double)wrk[go4kVCF_wrk_low].f + (double)freq_c * band;
	wrk[go4kVCF_wrk_low].f  = (float)low;				// stored as float
	double high = (in - low) - (double)res_c * band;
	wrk[go4kVCF_wrk_high].f = (float)high;				// stored as float
	wrk[go4kVCF_wrk_band].f = (float)(high * freq_c + band);

	// the output taps re-read the stored dwords rather than the wide values
	double out = 0.0;
	if (type & LOWPASS)
		out += wrk[go4kVCF_wrk_low].f;
#ifdef GO4K_USE_VCF_HIGH
	if (type & HIGHPASS)
		out += wrk[go4kVCF_wrk_high].f;
#endif
#ifdef GO4K_USE_VCF_BAND
	if (type & BANDPASS)
		out += wrk[go4kVCF_wrk_band].f;
#endif
#ifdef GO4K_USE_VCF_PEAK
	if (type & PEAK)
		out += (double)wrk[go4kVCF_wrk_low].f - wrk[go4kVCF_wrk_high].f;
#endif
	fpush(out);
}

// ---------------------------------------------------------------------------
// DST - waveshaping distortion with optional sample & hold
// ---------------------------------------------------------------------------
#ifdef GO4K_USE_DST
static void go4kDST_func(dword_t *wrk, dword_t *base)
{
	const float *val = go4kTransformValues(go4kDST_val_NUM);

#ifdef GO4K_USE_DST_CHECK
	// check if current note still active
	if (base[-1].i == 0)
		return;
#endif

#ifdef GO4K_USE_DST_SH
	double snh = val[go4kDST_val_snhfreq];
	#ifdef GO4K_USE_DST_MOD_SH
	snh += wrk[go4kDST_wrk_sm].f;
	#endif
	// squared for the same reason as the VCF frequency, then run backwards
	snh = wrk[go4kDST_wrk_snhphase].f - snh * snh;
	wrk[go4kDST_wrk_snhphase].f = (float)snh;			// stored as float

	if (0.0 < snh) {
		// still holding - throw the input away and repeat the stored output.
		// With SNHFREQ(0) the phase latches after the first sample, which turns
		// this unit into a once-per-note sample of the drive stage.
		fpop();
		fpush(wrk[go4kDST_wrk_out].f);
		return;
	}
	wrk[go4kDST_wrk_snhphase].f = (float)(snh + 1.0);	// stored as float
#endif

	double drive = val[go4kDST_val_drive];
#ifdef GO4K_USE_DST_MOD_DM
	drive += wrk[go4kDST_wrk_dm].f;
#endif
	fpush(drive);
	go4kWaveshaper();
#ifdef GO4K_USE_DST_SH
	wrk[go4kDST_wrk_out].f = (float)fst(0);				// stored as float
#endif
}
#endif

// ---------------------------------------------------------------------------
// DLL - a bank of COUNT comb filters sharing one global delay buffer
// ---------------------------------------------------------------------------
#ifdef GO4K_USE_DLL
static void go4kDLL_func(dword_t *wrk, dword_t *base)
{
	(void)base;
#if !defined(GO4K_USE_DLL_MOD_IM) && !defined(GO4K_USE_DLL_MOD_PM) && \
    !defined(GO4K_USE_DLL_MOD_DM) && !defined(GO4K_USE_DLL_MOD_FM)
	(void)wrk;
#endif
	const float *val = go4kTransformValues(go4kDLL_val_NUM);
	int count = VAL[-1];					// COUNT(), the number of combs
	int index = VAL[-2];					// DELAY(), the first comb size index

	dword_t *dll = go4k_delay_buffer + go4k_delay_buffer_ofs;

	double in  = fst(0);
	double out = in;
#ifdef GO4K_USE_DLL_MOD_IM
	out *= val[go4kDLL_val_dry] + wrk[go4kDLL_wrk2_im].f;
#else
	out *= val[go4kDLL_val_dry];
#endif
#ifdef GO4K_USE_DLL_MOD_PM
	double pregain = val[go4kDLL_val_pregain] + wrk[go4kDLL_wrk2_pm].f;
	in *= pregain * pregain;
#else
	in *= val[go4kDLL_val_pregain];
	in *= val[go4kDLL_val_pregain];
#endif

	while (count--) {
		int size = go4k_delay_times[index];
		int i    = dll[go4kDLL_wrk_index].i;

		float cout = dll[go4kDLL_wrk_buffer + i].f;
		out += cout;

#ifdef GO4K_USE_DLL_DAMP
		double damp = val[go4kDLL_val_damp];
	#ifdef GO4K_USE_DLL_MOD_DM
		double dm = wrk[go4kDLL_wrk2_dm].f;
		double store = cout * (1.0 - damp - dm) + (damp + dm) * dll[go4kDLL_wrk_store].f;
	#else
		double store = cout * (1.0 - damp) + damp * dll[go4kDLL_wrk_store].f;
	#endif
		dll[go4kDLL_wrk_store].f = (float)store;		// stored as float
#else
		double store = cout;
#endif

#ifdef GO4K_USE_DLL_MOD_FM
		store *= val[go4kDLL_val_feedback] + wrk[go4kDLL_wrk2_fm].f;
#else
		store *= val[go4kDLL_val_feedback];
#endif

#ifdef GO4K_USE_DLL_DC_FILTER
		dll[go4kDLL_wrk_buffer + i].f = (float)(store + in);
#else
		dll[go4kDLL_wrk_buffer + i].f = (float)(store - in);
		in = -in;							// the assembly's trailing `fneg`
#endif

		if (++i >= size)
			i = 0;
		dll[go4kDLL_wrk_index].i = i;

		++index;
		dll += GO4K_DLL_WRK_DWORDS;
		go4k_delay_buffer_ofs = (int)(dll - go4k_delay_buffer);
	}

#ifdef GO4K_USE_DLL_DC_FILTER
	// y(n) = x(n) - x(n-1) + R * y(n-1), to keep reverb from drifting off
	// centre. This borrows the dc state of the *next* delay line, which is
	// harmless because that line's own filter never runs on the same sample.
	double y = out + ((double)dll[go4kDLL_wrk_dcout].f * c_dc_const
	                  - dll[go4kDLL_wrk_dcin].f);
	dll[go4kDLL_wrk_dcin].f  = (float)out;				// stored as float
	#ifdef GO4K_USE_UNDENORMALIZE
	// add and sub a small offset to prevent denormalization
	y = (y + c_0_5) - c_0_5;
	#endif
	dll[go4kDLL_wrk_dcout].f = (float)y;				// stored as float
	out = y;
#endif

	fset(0, out);
}
#endif

// ---------------------------------------------------------------------------
// FOP - the stack shuffling opcodes
// ---------------------------------------------------------------------------
static void go4kFOP_func(dword_t *wrk, dword_t *base)
{
	(void)wrk;
	go4kTransformValues(go4kFOP_val_NUM);

	double a, b;
	switch (VAL[-1]) {
	case FOP_POP:
		fpop();
		break;
	case FOP_ADDP:
		a = fpop();
		fset(0, fst(0) + a);
		break;
	case FOP_MULP:
		a = fpop();
		fset(0, fst(0) * a);
		break;
	case FOP_PUSH:
		fpush(fst(0));
		break;
	case FOP_XCH:
		fxch();
		break;
	case FOP_ADD:
		fset(0, fst(0) + fst(1));
		break;
	case FOP_MUL:
		fset(0, fst(0) * fst(1));
		break;
	case FOP_ADDP2:
		// adds the top stereo pair into the pair below it
		a = fpop();
		fset(1, fst(1) + a);
		b = fpop();
		fset(1, fst(1) + b);
		break;
	case FOP_LOADNOTE:
		fpush((double)base[-1].i * c_i128);
		break;
	case FOP_MULP2:
		a = fpop();
		fset(1, fst(1) * a);
		b = fpop();
		fset(1, fst(1) * b);
		break;
	}
}

// ---------------------------------------------------------------------------
// FST - store a scaled copy of the current value into the local workspace
// ---------------------------------------------------------------------------
static void go4kFST_func(dword_t *wrk, dword_t *base)
{
	(void)wrk;
	const float *val = go4kTransformValues(go4kFST_val_NUM);

	double amount = (val[go4kFST_val_amount] - c_0_5) * 2.0;
	double value  = amount * fst(0);

	// the destination is a raw little endian word following the amount byte
	unsigned op = (unsigned)VAL[0] | ((unsigned)VAL[1] << 8);
	VAL += 2;
	int dest = op & 0x3fff;

	if (op & FST_ADD)
		value += base[dest].f;
	base[dest].f = (float)value;						// stored as float

	if (op & FST_POP)
		fpop();
}

// ---------------------------------------------------------------------------
// FLD - load a value on the stack, optionally modulated
// ---------------------------------------------------------------------------
#ifdef GO4K_USE_FLD
static void go4kFLD_func(dword_t *wrk, dword_t *base)
{
	(void)base;
	const float *val = go4kTransformValues(go4kFLD_val_NUM);
	double value = (val[go4kFLD_val_value] - c_0_5) * 2.0;
	#ifdef GO4K_USE_FLD_MOD_VM
	value += wrk[go4kFLD_wrk_vm].f;
	#else
	(void)wrk;
	#endif
	fpush(value);
}
#endif

// ---------------------------------------------------------------------------
// FSTG - like FST, but the destination is anywhere in the whole synth
// ---------------------------------------------------------------------------
#ifdef GO4K_USE_FSTG
static void go4kFSTG_func(dword_t *wrk, dword_t *base)
{
	(void)wrk;
	const float *val = go4kTransformValues(go4kFST_val_NUM);

#ifdef GO4K_USE_FSTG_CHECK
	// check if current note still active - the destination word is still
	// consumed so the parameter stream stays in step
	if (base[-1].i == 0) {
		unsigned op = (unsigned)VAL[0] | ((unsigned)VAL[1] << 8);
		VAL += 2;
		if (op & FST_POP)
			fpop();
		return;
	}
#endif

	double amount = (val[go4kFST_val_amount] - c_0_5) * 2.0;
	double value  = amount * fst(0);

	unsigned op = (unsigned)VAL[0] | ((unsigned)VAL[1] << 8);
	VAL += 2;
	int dest = op & 0x3fff;

	if (op & FST_ADD)
		value += go4k_synth_wrk[dest].f;
	go4k_synth_wrk[dest].f = (float)value;				// stored as float
#if MAX_VOICES > 1
	go4k_synth_wrk[dest + GO4K_INSTRUMENT_DWORDS].f = (float)value;
#endif

	if (op & FST_POP)
		fpop();
}
#endif

// ---------------------------------------------------------------------------
// PAN - mono to stereo, leaving left on top of right
// ---------------------------------------------------------------------------
static void go4kPAN_func(dword_t *wrk, dword_t *base)
{
	(void)base;
#ifdef GO4K_USE_PAN
	const float *val = go4kTransformValues(go4kPAN_val_NUM);
	double pan = val[go4kPAN_val_panning];
	#ifdef GO4K_USE_PAN_MOD
	pan += wrk[go4kPAN_wrk_pm].f;
	#endif
	double in = fst(0);
	double r  = pan * in;
	fset(0, in - r);									// left
	fpush(r);
	fxch();												// leave left on top
#else
	// GO4K_USE_PAN is off, so panning is hardwired to centre and the PANNING()
	// parameter is not even emitted into the byte stream
	(void)wrk;
	fset(0, fst(0) * c_0_5);
	fpush(fst(0));
#endif
}

// ---------------------------------------------------------------------------
// OUT - park the stereo pair in the instrument's output slots
// ---------------------------------------------------------------------------
static void go4kOUT_func(dword_t *wrk, dword_t *base)
{
#if !defined(GO4K_USE_OUT_MOD_GM) && !defined(GO4K_USE_OUT_MOD_AM)
	(void)wrk;
#endif
	const float *val = go4kTransformValues(go4kOUT_val_NUM);
	dword_t *out = base + MAX_UNITS * MAX_UNIT_SLOTS;

	double l = fst(0), r = fst(1);

	double gain = val[go4kOUT_val_gain];
#ifdef GO4K_USE_OUT_MOD_GM
	gain += wrk[go4kOUT_wrk_gm].f;
#endif

#ifdef GO4K_USE_GLOBAL_DLL
	double aux = val[go4kOUT_val_auxsend];
	#ifdef GO4K_USE_OUT_MOD_AM
	aux += wrk[go4kOUT_wrk_am].f;
	#endif
	out[0].f = (float)(l * aux);						// dlloutl
	out[1].f = (float)(r * aux);						// dlloutr
	out[2].f = (float)(l * gain);						// outl
	out[3].f = (float)(r * gain);						// outr
#else
	// as in the assembly, the non-aux build writes the first output pair,
	// which is where dlloutl/dlloutr live
	out[0].f = (float)(l * gain);
	out[1].f = (float)(r * gain);
#endif

	fpop();
	fpop();
}

// ---------------------------------------------------------------------------
// ACC - sum one stereo output pair across every instrument voice
// ---------------------------------------------------------------------------
static void go4kACC_func(dword_t *wrk, dword_t *base)
{
	(void)wrk;
	(void)base;
	go4kTransformValues(go4kACC_val_NUM);

	// ACCTYPE() is a byte offset back from the end of an instrument struct:
	// OUTPUT(0) lands on outl/outr, AUX(8) on dlloutl/dlloutr
	int acctype = VAL[-1];
	dword_t *p = go4k_synth_wrk + GO4K_INSTRUMENT_DWORDS - acctype / 4;

	double l = 0.0, r = 0.0;
	for (int i = 0; i < MAX_INSTRUMENTS * MAX_VOICES; ++i) {
		l += p[-2].f;
		r += p[-1].f;
		p += GO4K_INSTRUMENT_DWORDS;
	}
	fpush(r);
	fpush(l);
}

// ---------------------------------------------------------------------------
// the vm
// ---------------------------------------------------------------------------
typedef void (*go4k_unit_func)(dword_t *wrk, dword_t *base);

static const go4k_unit_func go4k_synth_commands[] = {
	0,
	go4kENV_func,
	go4kVCO_func,
	go4kVCF_func,
#ifdef GO4K_USE_DST
	go4kDST_func,
#else
	0,
#endif
#ifdef GO4K_USE_DLL
	go4kDLL_func,
#else
	0,
#endif
	go4kFOP_func,
	go4kFST_func,
	go4kPAN_func,
	go4kOUT_func,
	go4kACC_func,
#ifdef GO4K_USE_FLD
	go4kFLD_func,
#else
	0,
#endif
#ifdef GO4K_USE_FSTG
	go4kFSTG_func,
#endif
};

// Runs one instrument's opcode stream, then advances `instrument` to the next
// voice. COM and VAL walk forward across instruments and are only rewound at
// the top of each sample.
static void go4k_VM_process(dword_t **instrument)
{
	dword_t *base = *instrument + GO4K_INSTRUMENT_WORKSPACE;
	dword_t *wrk  = base;

	for (;;) {
		int command = *COM++;
		if (command == 0)
			break;
		go4k_synth_commands[command](wrk, base);
		wrk += MAX_UNIT_SLOTS;
	}

	*instrument += GO4K_INSTRUMENT_DWORDS;
}

// ---------------------------------------------------------------------------
// Update Instrument (allocate voices, set voice to release)
// ---------------------------------------------------------------------------
static void go4kUpdateInstrument(int instrument, int tick, dword_t *inst)
{
	int pattern = go4k_pattern_lists[instrument][tick >> PATTERN_SIZE_SHIFT];
	int note    = go4k_patterns[(pattern << PATTERN_SIZE_SHIFT) + (tick & (PATTERN_SIZE - 1))];

	// anything but hold causes action
	if (note == HLD)
		return;

	++inst[GO4K_INSTRUMENT_RELEASE].i;

	// check for new note
	if (note < HLD)
		return;

	// clear only release, note and workspace
	memset(inst, 0, (GO4K_INSTRUMENT_WORKSPACE + MAX_UNITS * MAX_UNIT_SLOTS) * sizeof(dword_t));
	inst[GO4K_INSTRUMENT_NOTE].i = note;
}

// ---------------------------------------------------------------------------
// Render Voices
// ---------------------------------------------------------------------------
static void go4kRenderVoices(dword_t **instrument)
{
	dword_t *base = *instrument + GO4K_INSTRUMENT_WORKSPACE;

	go4k_VM_process(instrument);

	// every instrument starts with an ENV, so slot 0 of its workspace is the
	// main envelope state
	if ((base[go4kENV_wrk_state].i & 0xff) == ENV_STATE_OFF)
		base[-1].i = 0;								// kill note if voice is done
}

// ---------------------------------------------------------------------------
// the entry point for the synth
// ---------------------------------------------------------------------------
#ifdef SINGLE_TICK_RENDERING
int _4klang_current_tick = 0;
#endif

void *_4klang_render(void *buffer)
{
	SAMPLE_TYPE *dest = (SAMPLE_TYPE *)buffer;

#ifdef SINGLE_TICK_RENDERING
	int tick = _4klang_current_tick;
	if (++_4klang_current_tick == MAX_TICKS) {
		_4klang_current_tick = 0;
	}
	{
#else
	for (int tick = 0; tick < MAX_TICKS; ++tick) {
#endif
		for (int sample = 0; sample < SAMPLES_PER_TICK; ++sample) {
			COM = go4k_synth_instructions;
			VAL = go4k_synth_parameter_values;
			go4k_delay_buffer_ofs = 0;

			dword_t *inst = go4k_synth_wrk;
			for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
				// tick change? (first sample in current tick)
				if (sample == 0)
					go4kUpdateInstrument(i, tick, inst);
				go4kRenderVoices(&inst);
			}

			// move a value != 0 into the global's note slot, so the unit note
			// checks let it through
			inst[GO4K_INSTRUMENT_NOTE].i = MAX_INSTRUMENTS;
			go4k_VM_process(&inst);

			// go4k_VM_process has stepped past the global, so its output pair
			// is the two dwords behind the cursor
			double l = inst[-2].f;
			double r = inst[-1].f;

#ifdef GO4K_CLIP_OUTPUT
			if (!(1.0 > l)) l = 1.0; else if (!(-1.0 < l)) l = -1.0;
			if (!(1.0 > r)) r = 1.0; else if (!(-1.0 < r)) r = -1.0;
#endif

#ifdef GO4K_USE_16BIT_OUTPUT
			*dest++ = (SAMPLE_TYPE)(int16_t)lrint(l * c_32767);
			*dest++ = (SAMPLE_TYPE)(int16_t)lrint(r * c_32767);
#else
			*dest++ = (SAMPLE_TYPE)l;
			*dest++ = (SAMPLE_TYPE)r;
#endif
		}
	}

	return buffer;
}
