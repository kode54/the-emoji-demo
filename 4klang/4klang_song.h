// 4klang song data - C port of 4klang.inc
//
// The original was a yasm include built out of `struc` and `%macro`
// definitions that emitted packed byte streams. Here the same streams come out
// of plain preprocessor macros, so the GO4K_USE_* switches below still decide
// the exact byte layout the way they did for the assembler.
//
// Every GO4K_* parameter macro emits its own trailing comma, which lets the
// ones that expand to nothing (GO4K_PAN, with GO4K_USE_PAN off) vanish from an
// initializer list without leaving a stray separator behind.

#ifndef FOURKLANG_SONG_H
#define FOURKLANG_SONG_H

#include <stdint.h>

#define SAMPLE_RATE			44100
#define MAX_INSTRUMENTS		10
#define MAX_VOICES			1
#define HLD					1
#define BPM					174.000000
#define MAX_PATTERNS		123
#define PATTERN_SIZE_SHIFT	4
#define PATTERN_SIZE		(1 << PATTERN_SIZE_SHIFT)
#define MAX_TICKS			(MAX_PATTERNS * PATTERN_SIZE)
#define SAMPLES_PER_TICK	3801
#define DEF_LFO_NORMALIZE	0.0000657596f
#define MAX_SAMPLES			(SAMPLES_PER_TICK * MAX_TICKS)

#define SINGLE_TICK_RENDERING

// how many patterns go4k_patterns actually holds. The pattern *lists* are
// MAX_PATTERNS entries long, which is a separate (and larger) number.
#define NUM_PATTERNS		88

#define GO4K_USE_16BIT_OUTPUT
//#define GO4K_USE_GROOVE_PATTERN
//#define GO4K_USE_ENVELOPE_RECORDINGS
//#define GO4K_USE_NOTE_RECORDINGS
#define GO4K_CLIP_OUTPUT
#define GO4K_USE_DST
#define GO4K_USE_DLL
#define GO4K_USE_GLOBAL_DLL
#define GO4K_USE_FSTG
#define GO4K_USE_ENV_CHECK
#define GO4K_USE_ENV_MOD_GM
#define GO4K_USE_ENV_MOD_ADR
#define GO4K_USE_VCO_CHECK
#define GO4K_USE_VCO_PHASE_OFFSET
#define GO4K_USE_VCO_SHAPE
#define GO4K_USE_VCO_MOD_PM
#define GO4K_USE_VCO_MOD_TM
#define GO4K_USE_VCO_MOD_DM
#define GO4K_USE_VCO_MOD_CM
#define GO4K_USE_VCF_CHECK
#define GO4K_USE_VCF_MOD_FM
#define GO4K_USE_VCF_MOD_RM
#define GO4K_USE_VCF_HIGH
#define GO4K_USE_VCF_BAND
#define GO4K_USE_VCF_PEAK
#define GO4K_USE_DST_CHECK
#define GO4K_USE_DST_SH
#define GO4K_USE_DST_MOD_DM
#define GO4K_USE_DLL_CHORUS_CLAMP
#define GO4K_USE_DLL_DAMP
#define GO4K_USE_DLL_DC_FILTER
#define GO4K_USE_FSTG_CHECK
#define GO4K_USE_PAN_MOD
#define GO4K_USE_WAVESHAPER_CLIP

#define MAX_DELAY			65536
#define MAX_UNITS			48
#define MAX_UNIT_SLOTS		9

// derived switches, mirroring the top of 4klang.asm
#if defined(GO4K_USE_VCO_SHAPE) || defined(GO4K_USE_DST)
	#define INCLUDE_WAVESHAPER
#endif
#if defined(GO4K_USE_VCO_MOD_PM) || defined(GO4K_USE_VCO_PHASE_OFFSET)
	#define PHASE_RENORMALIZE
#endif

// ---------------------------------------------------------------------------
// unit ids - the opcode stream indexes the VM dispatch table with these
// ---------------------------------------------------------------------------
#define GO4K_ENV_ID		1
#define GO4K_VCO_ID		2
#define GO4K_VCF_ID		3
#define GO4K_DST_ID		4
#define GO4K_DLL_ID		5
#define GO4K_FOP_ID		6
#define GO4K_FST_ID		7
#define GO4K_PAN_ID		8
#define GO4K_OUT_ID		9
#define GO4K_ACC_ID		10
#define GO4K_FLD_ID		11
#define GO4K_FSTG_ID	12

// ---------------------------------------------------------------------------
// unit workspaces. The VM hands each unit a MAX_UNIT_SLOTS-dword slice of the
// instrument workspace; these name the dwords inside one slice. They stay
// dword *indices* rather than byte offsets because that is how FST and FSTG
// destinations address them.
// ---------------------------------------------------------------------------
enum { go4kENV_wrk_state = 0, go4kENV_wrk_level, go4kENV_wrk_gm,
       go4kENV_wrk_am, go4kENV_wrk_dm, go4kENV_wrk_sm, go4kENV_wrk_rm };

enum { ENV_STATE_ATTAC = 0, ENV_STATE_DECAY, ENV_STATE_SUSTAIN,
       ENV_STATE_RELEASE, ENV_STATE_OFF };

enum { go4kVCO_wrk_phase = 0, go4kVCO_wrk_tm, go4kVCO_wrk_dm, go4kVCO_wrk_fm,
       go4kVCO_wrk_pm, go4kVCO_wrk_cm, go4kVCO_wrk_sm, go4kVCO_wrk_gm,
       go4kVCO_wrk_phase2 };

enum { go4kVCF_wrk_low = 0, go4kVCF_wrk_high, go4kVCF_wrk_band,
       go4kVCF_wrk_freq, go4kVCF_wrk_fm, go4kVCF_wrk_rm,
       go4kVCF_wrk_low2, go4kVCF_wrk_high2, go4kVCF_wrk_band2 };

enum { go4kDST_wrk_out = 0, go4kDST_wrk_snhphase, go4kDST_wrk_dm,
       go4kDST_wrk_sm, go4kDST_wrk_out2 };

enum { go4kPAN_wrk_pm = 0 };
enum { go4kOUT_wrk_am = 0, go4kOUT_wrk_gm };

// DLL keeps its ring buffer in the shared delay buffer, not in the instrument
// workspace; only its modulation inputs live in the workspace slice.
enum { go4kDLL_wrk2_pm = 0, go4kDLL_wrk2_fm, go4kDLL_wrk2_im,
       go4kDLL_wrk2_dm, go4kDLL_wrk2_sm, go4kDLL_wrk2_am };

// one delay line's state inside go4k_delay_buffer
enum { go4kDLL_wrk_index = 0, go4kDLL_wrk_store, go4kDLL_wrk_dcin,
       go4kDLL_wrk_dcout,
#ifdef GO4K_USE_DLL_CHORUS
       go4kDLL_wrk_phase,
#endif
       go4kDLL_wrk_buffer };
#define GO4K_DLL_WRK_DWORDS	(go4kDLL_wrk_buffer + MAX_DELAY)

// ---------------------------------------------------------------------------
// indices into the transformed-value array a unit gets handed
// ---------------------------------------------------------------------------
enum { go4kENV_val_attac = 0, go4kENV_val_decay, go4kENV_val_sustain,
       go4kENV_val_release, go4kENV_val_gain, go4kENV_val_NUM };

enum { go4kVCO_val_transpose = 0, go4kVCO_val_detune,
#ifdef GO4K_USE_VCO_PHASE_OFFSET
       go4kVCO_val_phaseofs,
#endif
#ifdef GO4K_USE_VCO_GATE
       go4kVCO_val_gate,
#endif
       go4kVCO_val_color,
#ifdef GO4K_USE_VCO_SHAPE
       go4kVCO_val_shape,
#endif
       go4kVCO_val_gain, go4kVCO_val_flags, go4kVCO_val_NUM };

enum { go4kVCF_val_freq = 0, go4kVCF_val_res, go4kVCF_val_type,
       go4kVCF_val_NUM };

// note: with GO4K_USE_DST_STEREO off the assembler macro never emitted the
// flags byte, so DST only ever consumes drive and snhfreq
enum { go4kDST_val_drive = 0,
#ifdef GO4K_USE_DST_SH
       go4kDST_val_snhfreq,
#endif
       go4kDST_val_NUM };

enum { go4kDLL_val_pregain = 0, go4kDLL_val_dry, go4kDLL_val_feedback,
#ifdef GO4K_USE_DLL_DAMP
       go4kDLL_val_damp,
#endif
#ifdef GO4K_USE_DLL_CHORUS
       go4kDLL_val_freq, go4kDLL_val_depth,
#endif
       go4kDLL_val_delay, go4kDLL_val_count, go4kDLL_val_NUM };

enum { go4kFST_val_amount = 0, go4kFST_val_NUM };
enum { go4kFOP_val_op = 0, go4kFOP_val_NUM };
enum { go4kACC_val_acctype = 0, go4kACC_val_NUM };
enum { go4kFLD_val_value = 0, go4kFLD_val_NUM };
#ifdef GO4K_USE_PAN
enum { go4kPAN_val_panning = 0, go4kPAN_val_NUM };
#endif
enum { go4kOUT_val_gain = 0,
#ifdef GO4K_USE_GLOBAL_DLL
       go4kOUT_val_auxsend,
#endif
       go4kOUT_val_NUM };

// ---------------------------------------------------------------------------
// go4k_instrument, in dwords: release, note, workspace[], dlloutl, dlloutr,
// outl, outr. FSTG destinations are absolute dword indices into the synth
// workspace, so this has to keep the assembler struc's exact layout.
// ---------------------------------------------------------------------------
#define GO4K_INSTRUMENT_RELEASE		0								/* dword index */
#define GO4K_INSTRUMENT_NOTE		1
#define GO4K_INSTRUMENT_WORKSPACE	2
#define GO4K_INSTRUMENT_DLLOUTL		(GO4K_INSTRUMENT_WORKSPACE + MAX_UNITS * MAX_UNIT_SLOTS)
#define GO4K_INSTRUMENT_DLLOUTR		(GO4K_INSTRUMENT_DLLOUTL + 1)
#define GO4K_INSTRUMENT_OUTL		(GO4K_INSTRUMENT_DLLOUTL + 2)
#define GO4K_INSTRUMENT_OUTR		(GO4K_INSTRUMENT_DLLOUTL + 3)
#define GO4K_INSTRUMENT_DWORDS		(GO4K_INSTRUMENT_DLLOUTL + 4)
#define GO4K_INSTRUMENT_SIZE		(GO4K_INSTRUMENT_DWORDS * 4)	/* bytes */

// instrument voices plus the one global instrument
#define GO4K_SYNTH_VOICES			(MAX_INSTRUMENTS * MAX_VOICES + MAX_VOICES)
#define GO4K_SYNTH_DWORDS			(GO4K_INSTRUMENT_DWORDS * GO4K_SYNTH_VOICES)

// The DEST() expressions below are written the way the .inc wrote them, using
// the assembler struc member names. These aliases keep that spelling working.
#define go4k_instrument_size		GO4K_INSTRUMENT_SIZE
#define go4k_instrument_workspace	(GO4K_INSTRUMENT_WORKSPACE * 4)

// ---------------------------------------------------------------------------
// value wrappers - these were `%define ATTAC(val) val` and friends, kept so
// the parameter tables read exactly like the original
// ---------------------------------------------------------------------------
#define ATTAC(val)		(val)
#define DECAY(val)		(val)
#define SUSTAIN(val)	(val)
#define RELEASE(val)	(val)
#define GAIN(val)		(val)
#define TRANSPOSE(val)	(val)
#define DETUNE(val)		(val)
#define PHASE(val)		(val)
#define GATES(val)		(val)
#define COLOR(val)		(val)
#define SHAPE(val)		(val)
#define FLAGS(val)		(val)
#define FREQUENCY(val)	(val)
#define RESONANCE(val)	(val)
#define VCFTYPE(val)	(val)
#define DRIVE(val)		(val)
#define SNHFREQ(val)	(val)
#define PREGAIN(val)	(val)
#define DRY(val)		(val)
#define FEEDBACK(val)	(val)
#define DEPTH(val)		(val)
#define DAMP(val)		(val)
#define DELAY(val)		(val)
#define COUNT(val)		(val)
#define OP(val)			(val)
#define AMOUNT(val)		(val)
#define DEST(val)		(val)
#define PANNING(val)	(val)
#define AUXSEND(val)	(val)
#define VALUE(val)		(val)
#define ACCTYPE(val)	(val)

// VCO waveform / mode flags
#define SINE		0x01
#define TRISAW		0x02
#define PULSE		0x04
#define NOISE		0x08
#define LFO			0x10
#define GATE		0x20
#define VCO_STEREO	0x40

// VCF type flags
#define LOWPASS		0x1
#define HIGHPASS	0x2
#define BANDPASS	0x4
#define BANDSTOP	0x3
#define ALLPASS		0x7
#define PEAK		0x8
#define STEREO		0x10

// FOP opcodes
#define FOP_POP			0x1
#define FOP_ADDP		0x2
#define FOP_MULP		0x3
#define FOP_PUSH		0x4
#define FOP_XCH			0x5
#define FOP_ADD			0x6
#define FOP_MUL			0x7
#define FOP_ADDP2		0x8
#define FOP_LOADNOTE	0x9
#define FOP_MULP2		0xa

// FST/FSTG store mode, packed into the top bits of the destination word
#define FST_SET			0x0000
#define FST_ADD			0x4000
#define FST_POP			0x8000

// ACC source, as a byte offset back from the end of an instrument struct
#define OUTPUT			0
#define AUX				8

// ---------------------------------------------------------------------------
// parameter emitters. Each expands to the packed bytes the assembler macro
// emitted, trailing comma included.
// ---------------------------------------------------------------------------
#define GO4K_ENV(a, d, s, r, g)		(a), (d), (s), (r), (g),

#ifdef GO4K_USE_VCO_PHASE_OFFSET
	#define GO4K_VCO_PHASEOFS(p)	(p),
#else
	#define GO4K_VCO_PHASEOFS(p)
#endif
#ifdef GO4K_USE_VCO_GATE
	#define GO4K_VCO_GATES(g)		(g),
#else
	#define GO4K_VCO_GATES(g)
#endif
#ifdef GO4K_USE_VCO_SHAPE
	#define GO4K_VCO_SHAPE(s)		(s),
#else
	#define GO4K_VCO_SHAPE(s)
#endif
#define GO4K_VCO(t, d, p, g, c, s, ga, f)	\
	(t), (d), GO4K_VCO_PHASEOFS(p) GO4K_VCO_GATES(g) (c), GO4K_VCO_SHAPE(s) (ga), (f),

#define GO4K_VCF(f, r, t)			(f), (r), (t),

#ifdef GO4K_USE_DST_SH
	#define GO4K_DST_SNH(s)			(s),
#else
	#define GO4K_DST_SNH(s)
#endif
#ifdef GO4K_USE_DST_STEREO
	#define GO4K_DST_FLAGS(f)		(f),
#else
	#define GO4K_DST_FLAGS(f)		/* the flags byte is stereo-only */
#endif
#define GO4K_DST(d, s, f)			(d), GO4K_DST_SNH(s) GO4K_DST_FLAGS(f)

#ifdef GO4K_USE_DLL_DAMP
	#define GO4K_DLL_DAMP(d)		(d),
#else
	#define GO4K_DLL_DAMP(d)
#endif
#ifdef GO4K_USE_DLL_CHORUS
	#define GO4K_DLL_CHORUS(f, d)	(f), (d),
#else
	#define GO4K_DLL_CHORUS(f, d)
#endif
#define GO4K_DLL(pg, dry, fb, damp, freq, depth, del, cnt)	\
	(pg), (dry), (fb), GO4K_DLL_DAMP(damp) GO4K_DLL_CHORUS(freq, depth) (del), (cnt),

#define GO4K_FOP(op)				(op),

// db amount, dw dest - little endian, as on the x86 the original targeted
#define GO4K_FST(amt, dest)			(amt), ((dest) & 0xff), (((dest) >> 8) & 0xff),
#define GO4K_FSTG(amt, dest)		GO4K_FST(amt, dest)

#ifdef GO4K_USE_PAN
	#define GO4K_PAN(p)				(p),
#else
	#define GO4K_PAN(p)				/* PAN is hardwired to centre */
#endif

#ifdef GO4K_USE_GLOBAL_DLL
	#define GO4K_OUT(g, aux)		(g), (aux),
#else
	#define GO4K_OUT(g, aux)		(g),
#endif

#define GO4K_ACC(t)					(t),
#define GO4K_FLD(v)					(v),

// ---------------------------------------------------------------------------
// pattern data
// ---------------------------------------------------------------------------
static const uint8_t go4k_patterns[NUM_PATTERNS * PATTERN_SIZE] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	12, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	60, HLD, HLD, HLD, HLD, HLD, HLD, HLD, 72, HLD, HLD, HLD, 79, HLD, HLD, HLD,
	HLD, HLD, 82, HLD, HLD, HLD, HLD, HLD, 81, HLD, HLD, HLD, 77, HLD, HLD, HLD,
	70, HLD, 0, 0, 70, HLD, HLD, HLD, 75, HLD, 75, HLD, HLD, HLD, 77, HLD,
	75, HLD, 0, 0, 75, HLD, HLD, HLD, 70, HLD, 70, HLD, HLD, HLD, 65, HLD,
	72, HLD, 0, 0, 72, HLD, 0, 0, 84, HLD, 0, 0, 72, HLD, 82, HLD,
	0, 0, 72, HLD, 81, HLD, 72, HLD, 79, HLD, 0, 0, 77, HLD, 0, 0,
	76, HLD, 0, 0, 72, HLD, 0, 0, 72, HLD, 0, 0, 0, 0, 0, 0,
	72, HLD, HLD, HLD, HLD, HLD, 0, 0, 84, HLD, HLD, HLD, 91, HLD, HLD, HLD,
	HLD, HLD, 94, HLD, HLD, HLD, HLD, HLD, 93, HLD, HLD, HLD, 89, HLD, 0, 0,
	HLD, HLD, 94, HLD, HLD, HLD, HLD, HLD, 93, HLD, HLD, HLD, 89, HLD, HLD, HLD,
	82, HLD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	82, HLD, 0, 0, 82, HLD, HLD, HLD, 87, HLD, 87, HLD, HLD, HLD, 89, HLD,
	HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, 0, 0,
	70, HLD, HLD, HLD, HLD, HLD, 0, 0, 84, HLD, HLD, HLD, 91, HLD, HLD, HLD,
	87, HLD, 0, 0, 87, HLD, HLD, HLD, 82, HLD, 82, HLD, HLD, HLD, 77, HLD,
	96, 94, 96, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, 0, 0, 0, 0,
	89, HLD, HLD, HLD, HLD, HLD, 93, HLD, HLD, HLD, HLD, HLD, 94, HLD, HLD, HLD,
	94, 93, 94, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, 0, 0, 0, 0,
	89, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, 0, 0, 0, 0,
	89, 87, 89, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, 0, 0, 0, 0,
	82, HLD, HLD, HLD, HLD, HLD, 86, HLD, HLD, HLD, HLD, HLD, 87, HLD, HLD, HLD,
	84, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, 0, 0, 0, 0,
	84, HLD, HLD, HLD, HLD, HLD, 87, HLD, HLD, HLD, HLD, HLD, 89, HLD, HLD, HLD,
	93, 89, 93, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, 0, 0, 0, 0,
	94, HLD, 0, 0, 94, HLD, HLD, HLD, 89, HLD, 89, HLD, HLD, HLD, 84, HLD,
	36, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	39, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	34, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	29, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	24, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	27, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	53, HLD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 53, HLD, 0, 0, 0, 0, 0, 0,
	53, HLD, 0, 0, 0, 0, 0, 0, 0, 0, 53, HLD, 0, 0, 0, 0,
	53, HLD, 0, 0, 0, 0, 0, 0, 0, 0, 53, HLD, 0, 0, 53, HLD,
	0, 0, 53, HLD, 0, 0, 0, 0, 0, 0, 53, HLD, 0, 0, 0, 0,
	0, 0, 53, HLD, 0, 0, 0, 0, 53, HLD, 53, HLD, 0, 0, 0, 0,
	53, HLD, 0, 0, 53, HLD, 0, 0, 53, HLD, 0, 0, 53, HLD, 0, 0,
	53, HLD, 53, HLD, 53, HLD, 53, HLD, 0, 0, 0, 0, 0, 0, 0, 0,
	53, HLD, 0, 0, 0, 0, 0, 0, 53, HLD, 0, 0, 0, 0, 53, HLD,
	53, HLD, 0, 0, 0, 0, 53, HLD, 53, HLD, 53, HLD, 0, 0, 0, 0,
	65, HLD, 65, HLD, HLD, HLD, 60, HLD, HLD, HLD, 60, HLD, 53, HLD, 53, HLD,
	0, 0, 0, 0, 53, HLD, HLD, HLD, 0, 0, 0, 0, 53, HLD, HLD, HLD,
	0, 0, 0, 0, 53, HLD, HLD, HLD, 0, 0, 0, 0, 53, HLD, 53, HLD,
	0, 0, 0, 0, 53, HLD, HLD, HLD, 0, 0, 0, 0, 53, HLD, 53, 53,
	0, 0, 0, 0, 0, 0, 0, 0, 65, 60, 53, 53, 65, 60, 53, 53,
	0, 0, 65, HLD, HLD, HLD, 65, HLD, HLD, HLD, 65, HLD, HLD, HLD, 65, HLD,
	HLD, HLD, 65, HLD, HLD, HLD, 65, HLD, HLD, HLD, 65, HLD, HLD, HLD, 65, HLD,
	HLD, HLD, 65, HLD, HLD, HLD, 65, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	HLD, HLD, 65, 65, HLD, HLD, 65, 65, HLD, HLD, 65, 65, HLD, HLD, 65, 65,
	48, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	51, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	53, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	58, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	60, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	63, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	65, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	70, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, 0, 0, 0, 0, 0, 0, 0, 0,
	60, HLD, HLD, HLD, HLD, HLD, 0, 0, 72, HLD, HLD, HLD, 79, HLD, HLD, HLD,
	70, HLD, HLD, HLD, HLD, HLD, 0, 0, 72, HLD, HLD, HLD, 79, HLD, HLD, HLD,
	72, HLD, HLD, HLD, HLD, HLD, 0, 0, 72, HLD, HLD, HLD, 79, HLD, HLD, HLD,
	48, HLD, 60, HLD, HLD, 0, 48, HLD, 60, HLD, HLD, 0, 48, HLD, 60, HLD,
	HLD, 0, 60, HLD, HLD, 0, 48, HLD, 60, HLD, HLD, 0, 60, HLD, HLD, 0,
	51, HLD, 63, HLD, HLD, 0, 51, HLD, 63, HLD, HLD, 0, 51, HLD, 63, HLD,
	HLD, 0, 63, HLD, HLD, 0, 51, HLD, 63, HLD, HLD, 0, 63, HLD, HLD, 0,
	46, HLD, 58, HLD, HLD, 0, 46, HLD, 58, HLD, HLD, 0, 46, HLD, 58, HLD,
	HLD, 0, 58, HLD, HLD, 0, 46, HLD, 58, HLD, HLD, 0, 58, HLD, HLD, 0,
	53, HLD, 65, HLD, HLD, 0, 53, HLD, 65, HLD, HLD, 0, 53, HLD, 65, HLD,
	HLD, 0, 65, HLD, HLD, 0, 53, HLD, 65, HLD, HLD, 0, 65, HLD, HLD, 0,
	72, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	0, 0, 0, 0, 0, 0, 51, HLD, 63, HLD, HLD, 0, 63, HLD, 0, 0,
	0, 0, 0, 0, 0, 0, 53, HLD, 65, HLD, HLD, 0, 65, HLD, 0, 0,
	60, HLD, HLD, HLD, HLD, HLD, HLD, HLD, 72, HLD, HLD, HLD, 60, HLD, 72, HLD,
	HLD, HLD, 72, HLD, HLD, HLD, 60, HLD, 72, HLD, HLD, HLD, 72, HLD, HLD, HLD,
	63, HLD, 75, HLD, HLD, HLD, 63, HLD, 75, HLD, HLD, HLD, 63, HLD, 75, HLD,
	HLD, HLD, 75, HLD, HLD, HLD, 63, HLD, 75, HLD, HLD, HLD, 75, HLD, HLD, HLD,
	58, HLD, HLD, HLD, HLD, HLD, HLD, HLD, 70, HLD, HLD, HLD, 58, HLD, 70, HLD,
	HLD, HLD, 70, HLD, HLD, HLD, 58, HLD, 70, HLD, HLD, HLD, 70, HLD, HLD, HLD,
	65, HLD, 77, HLD, HLD, HLD, 65, HLD, 77, HLD, HLD, HLD, 65, HLD, 77, HLD,
	HLD, HLD, 77, HLD, HLD, HLD, 65, HLD, 77, HLD, HLD, HLD, 77, HLD, HLD, 0,
	60, HLD, 72, HLD, HLD, HLD, 60, HLD, 72, HLD, HLD, HLD, 60, HLD, 72, HLD,
	HLD, HLD, 72, HLD, HLD, HLD, 60, HLD, 72, HLD, HLD, HLD, 72, HLD, HLD, 0,
	101, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD, HLD,
	65, HLD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const uint8_t go4k_pattern_lists[MAX_INSTRUMENTS][MAX_PATTERNS] = {
	/* Instrument0List */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0 },
	/* Instrument1List */ { 3, 4, 5, 2, 3, 4, 6, 2, 3, 4, 5, 2, 3, 4, 6, 2, 3, 4, 5, 2, 3, 4, 6, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 7, 8, 9, 0, 7, 8, 9, 0, 7, 8, 9, 0, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 4, 5, 2, 3, 4, 6, 2, 3, 4, 5, 2, 3, 4, 6, 2, 7, 8, 9, 0, 7, 8, 9, 0, 7, 8, 9, 0, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* Instrument2List */ { 0, 0, 0, 0, 0, 0, 0, 0, 10, 11, 0, 0, 10, 11, 0, 0, 10, 11, 0, 0, 10, 12, 13, 0, 10, 12, 14, 15, 16, 12, 17, 15, 10, 12, 14, 15, 16, 12, 17, 15, 18, 19, 20, 21, 22, 23, 22, 24, 18, 19, 20, 21, 22, 25, 26, 21, 10, 12, 14, 15, 16, 12, 17, 15, 10, 12, 14, 15, 16, 12, 17, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 18, 19, 20, 21, 22, 23, 22, 24, 18, 19, 20, 21, 22, 25, 26, 21, 10, 12, 14, 15, 16, 12, 17, 15, 10, 12, 14, 15, 16, 12, 27, 15, 0, 0, 0 },
	/* Instrument3List */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 28, 2, 29, 2, 30, 2, 31, 2, 28, 2, 29, 2, 30, 2, 31, 2, 28, 2, 29, 2, 30, 2, 31, 2, 28, 2, 29, 2, 30, 2, 31, 2, 28, 2, 29, 2, 30, 2, 31, 2, 28, 2, 29, 2, 30, 2, 31, 2, 32, 2, 33, 2, 31, 2, 30, 2, 32, 2, 33, 2, 31, 2, 30, 2, 28, 2, 29, 2, 30, 2, 31, 2, 28, 2, 29, 2, 30, 2, 31, 2, 28, 2, 29, 2, 30, 2, 31, 2, 28, 2, 29, 2, 30, 2, 28, 2, 0, 0, 0 },
	/* Instrument4List */ { 0, 0, 0, 0, 0, 0, 0, 0, 34, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 35, 36, 36, 37, 38, 36, 36, 37, 39, 36, 36, 37, 38, 36, 36, 40, 41, 36, 36, 37, 38, 36, 36, 37, 39, 36, 36, 37, 38, 36, 36, 42, 43, 36, 36, 37, 38, 36, 36, 37, 39, 36, 36, 37, 38, 36, 36, 40, 41, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 36, 36, 37, 38, 36, 36, 37, 39, 36, 36, 37, 38, 36, 36, 42, 43, 36, 36, 37, 38, 36, 36, 37, 39, 36, 36, 37, 38, 36, 36, 40, 41, 0, 0, 0 },
	/* Instrument5List */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 44, 45, 45, 45, 46, 45, 45, 45, 47, 45, 45, 45, 46, 45, 46, 0, 48, 45, 45, 45, 46, 45, 45, 45, 47, 45, 45, 45, 46, 45, 45, 45, 47, 45, 45, 45, 46, 45, 45, 45, 47, 45, 45, 45, 46, 45, 46, 0, 48, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 45, 45, 45, 46, 45, 45, 45, 47, 45, 45, 45, 46, 45, 45, 45, 47, 45, 45, 45, 46, 45, 45, 45, 47, 45, 45, 45, 46, 45, 46, 0, 48, 2, 2, 0 },
	/* Instrument6List */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 49, 50, 50, 50, 50, 50, 50, 51, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 2, 2, 2, 2, 2, 2, 2, 2, 50, 50, 50, 50, 50, 50, 50, 50, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 2, 2, 0 },
	/* Instrument7List */ { 0, 0, 0, 0, 0, 0, 0, 0, 53, 2, 54, 2, 55, 2, 56, 2, 57, 2, 58, 2, 59, 2, 60, 61, 62, 4, 5, 15, 63, 4, 6, 15, 64, 4, 5, 15, 63, 4, 6, 15, 65, 66, 67, 68, 69, 70, 71, 72, 65, 66, 67, 68, 69, 70, 71, 72, 73, 2, 5, 15, 60, 2, 6, 15, 73, 2, 5, 15, 60, 2, 6, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 74, 0, 0, 0, 75, 0, 0, 0, 74, 0, 0, 0, 75, 76, 77, 78, 79, 80, 81, 82, 83, 76, 77, 78, 79, 80, 81, 84, 85, 0, 0, 0 },
	/* Instrument8List */ { 86, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 86, 2, 2, 2, 2, 2, 2, 61, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 86, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 86, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 86, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0 },
	/* Instrument10List */ { 0, 0, 0, 0, 0, 0, 0, 0, 87, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 87, 0, 0, 0, 0, 0, 0, 0, 87, 0, 0, 0, 0, 0, 0, 0, 87, 0, 0, 0, 0, 0, 0, 0, 87, 0, 0, 0, 0, 0, 0, 0, 87, 0, 0, 0, 0, 0, 0, 0, 87, 0, 0, 0, 0, 0, 0, 0, 87, 0, 0, 0, 0, 0, 0, 0, 87, 0, 0, 0, 0, 0, 0, 0, 87, 0, 0, 0, 0, 0, 0, 0, 87, 0, 0, 0, 0, 0, 0, 0, 87, 0, 0, 0, 0, 0, 0, 0, 87, 0, 0, 0, 0, 0, 0, 0, 87, 0, 0 },
};

// ---------------------------------------------------------------------------
// per-instrument opcode streams, terminated by a 0 byte
// ---------------------------------------------------------------------------
static const uint8_t go4k_synth_instructions[] = {
	/* Instrument0 */
		GO4K_ENV_ID,
		GO4K_VCO_ID,
		GO4K_VCO_ID,
		GO4K_FOP_ID,
		GO4K_VCO_ID,
		GO4K_FOP_ID,
		GO4K_FOP_ID,
		GO4K_VCO_ID,
		GO4K_FST_ID,
		GO4K_FOP_ID,
		GO4K_PAN_ID,
		GO4K_OUT_ID,
		0,
	/* Instrument1 */
		GO4K_ENV_ID,
		GO4K_VCO_ID,
		GO4K_FOP_ID,
		GO4K_VCF_ID,
		GO4K_VCO_ID,
		GO4K_DST_ID,
		GO4K_FST_ID,
		GO4K_FOP_ID,
		GO4K_PAN_ID,
		GO4K_DLL_ID,
		GO4K_FOP_ID,
		GO4K_DLL_ID,
		GO4K_FOP_ID,
		GO4K_OUT_ID,
		0,
	/* Instrument2 */
		GO4K_ENV_ID,
		GO4K_VCO_ID,
		GO4K_FST_ID,
		GO4K_FOP_ID,
		GO4K_VCO_ID,
		GO4K_FST_ID,
		GO4K_FST_ID,
		GO4K_FOP_ID,
		GO4K_VCO_ID,
		GO4K_FST_ID,
		GO4K_FOP_ID,
		GO4K_VCO_ID,
		GO4K_VCO_ID,
		GO4K_VCO_ID,
		GO4K_FOP_ID,
		GO4K_FOP_ID,
		GO4K_FOP_ID,
		GO4K_VCF_ID,
		GO4K_DLL_ID,
		GO4K_PAN_ID,
		GO4K_OUT_ID,
		0,
	/* Instrument3 */
		GO4K_ENV_ID,
		GO4K_FOP_ID,
		GO4K_FSTG_ID,
		GO4K_FSTG_ID,
		GO4K_FSTG_ID,
		GO4K_FOP_ID,
		GO4K_FOP_ID,
		0,
	/* Instrument4 */
		GO4K_ENV_ID,
		GO4K_FST_ID,
		GO4K_ENV_ID,
		GO4K_DST_ID,
		GO4K_FST_ID,
		GO4K_FOP_ID,
		GO4K_VCO_ID,
		GO4K_FOP_ID,
		GO4K_ENV_ID,
		GO4K_FSTG_ID,
		GO4K_FSTG_ID,
		GO4K_FOP_ID,
		GO4K_PAN_ID,
		GO4K_OUT_ID,
		0,
	/* Instrument5 */
		GO4K_ENV_ID,
		GO4K_FST_ID,
		GO4K_ENV_ID,
		GO4K_DST_ID,
		GO4K_FST_ID,
		GO4K_FOP_ID,
		GO4K_VCO_ID,
		GO4K_VCO_ID,
		GO4K_FOP_ID,
		GO4K_FOP_ID,
		GO4K_VCF_ID,
		GO4K_ENV_ID,
		GO4K_FSTG_ID,
		GO4K_FSTG_ID,
		GO4K_FOP_ID,
		GO4K_PAN_ID,
		GO4K_OUT_ID,
		0,
	/* Instrument6 */
		GO4K_ENV_ID,
		GO4K_VCO_ID,
		GO4K_FOP_ID,
		GO4K_VCF_ID,
		GO4K_PAN_ID,
		GO4K_DLL_ID,
		GO4K_FOP_ID,
		GO4K_DLL_ID,
		GO4K_FOP_ID,
		GO4K_OUT_ID,
		0,
	/* Instrument7 */
		GO4K_ENV_ID,
		GO4K_VCO_ID,
		GO4K_VCO_ID,
		GO4K_FOP_ID,
		GO4K_ENV_ID,
		GO4K_FST_ID,
		GO4K_FOP_ID,
		GO4K_FOP_ID,
		GO4K_DST_ID,
		GO4K_DLL_ID,
		GO4K_PAN_ID,
		GO4K_OUT_ID,
		0,
	/* Instrument8 */
		GO4K_ENV_ID,
		GO4K_FOP_ID,
		GO4K_FOP_ID,
		GO4K_FST_ID,
		GO4K_FOP_ID,
		GO4K_ENV_ID,
		GO4K_FST_ID,
		GO4K_FOP_ID,
		GO4K_ENV_ID,
		GO4K_VCO_ID,
		GO4K_FOP_ID,
		GO4K_VCF_ID,
		GO4K_DLL_ID,
		GO4K_PAN_ID,
		GO4K_OUT_ID,
		0,
	/* Instrument10 */
		GO4K_ENV_ID,
		GO4K_VCO_ID,
		GO4K_FOP_ID,
		GO4K_VCF_ID,
		GO4K_ENV_ID,
		GO4K_FST_ID,
		GO4K_FOP_ID,
		GO4K_VCO_ID,
		GO4K_FST_ID,
		GO4K_FOP_ID,
		GO4K_FOP_ID,
		GO4K_FST_ID,
		GO4K_FOP_ID,
		GO4K_PAN_ID,
		GO4K_DLL_ID,
		GO4K_FOP_ID,
		GO4K_DLL_ID,
		GO4K_FOP_ID,
		GO4K_OUT_ID,
		0,
	/* Global */
		GO4K_ACC_ID,
		GO4K_DLL_ID,
		GO4K_FOP_ID,
		GO4K_DLL_ID,
		GO4K_FOP_ID,
		GO4K_ACC_ID,
		GO4K_FOP_ID,
		GO4K_OUT_ID,
		0,
};

// ---------------------------------------------------------------------------
// per-instrument parameter byte stream, walked in lockstep with the opcodes
// ---------------------------------------------------------------------------
static const uint8_t go4k_synth_parameter_values[] = {
	/* Instrument0 */
		GO4K_ENV(ATTAC(32),DECAY(32),SUSTAIN(128),RELEASE(64),GAIN(128))
		GO4K_VCO(TRANSPOSE(64),DETUNE(64),PHASE(0),GATES(0),COLOR(128),SHAPE(72),GAIN(128),FLAGS(SINE))
		GO4K_VCO(TRANSPOSE(64),DETUNE(64),PHASE(64),GATES(0),COLOR(128),SHAPE(80),GAIN(128),FLAGS(SINE))
		GO4K_FOP(OP(FOP_ADDP))
		GO4K_VCO(TRANSPOSE(76),DETUNE(64),PHASE(0),GATES(0),COLOR(128),SHAPE(64),GAIN(128),FLAGS(SINE))
		GO4K_FOP(OP(FOP_ADDP))
		GO4K_FOP(OP(FOP_MULP))
		GO4K_VCO(TRANSPOSE(52),DETUNE(64),PHASE(0),GATES(0),COLOR(128),SHAPE(64),GAIN(128),FLAGS(SINE|LFO))
		GO4K_FST(AMOUNT(96),DEST(2*MAX_UNIT_SLOTS+4+FST_SET))
		GO4K_FOP(OP(FOP_POP))
		GO4K_PAN(PANNING(64))
		GO4K_OUT(GAIN(128), AUXSEND(0))
	/* Instrument1 */
		GO4K_ENV(ATTAC(32),DECAY(64),SUSTAIN(0),RELEASE(0),GAIN(128))
		GO4K_VCO(TRANSPOSE(88),DETUNE(64),PHASE(0),GATES(85),COLOR(112),SHAPE(64),GAIN(128),FLAGS(PULSE))
		GO4K_FOP(OP(FOP_MULP))
		GO4K_VCF(FREQUENCY(96),RESONANCE(128),VCFTYPE(LOWPASS))
		GO4K_VCO(TRANSPOSE(64),DETUNE(64),PHASE(0),GATES(0),COLOR(64),SHAPE(64),GAIN(128),FLAGS(NOISE|LFO))
		GO4K_DST(DRIVE(64), SNHFREQ(0), FLAGS(0))
		GO4K_FST(AMOUNT(96),DEST(8*MAX_UNIT_SLOTS+0+FST_SET))
		GO4K_FOP(OP(FOP_POP))
		GO4K_PAN(PANNING(64))
		GO4K_DLL(PREGAIN(64),DRY(64),FEEDBACK(64),DAMP(64),FREQUENCY(0),DEPTH(0),DELAY(17),COUNT(1))
		GO4K_FOP(OP(FOP_XCH))
		GO4K_DLL(PREGAIN(64),DRY(64),FEEDBACK(64),DAMP(64),FREQUENCY(0),DEPTH(0),DELAY(17),COUNT(1))
		GO4K_FOP(OP(FOP_XCH))
		GO4K_OUT(GAIN(64), AUXSEND(32))
	/* Instrument2 */
		GO4K_ENV(ATTAC(0),DECAY(70),SUSTAIN(128),RELEASE(80),GAIN(128))
		GO4K_VCO(TRANSPOSE(69),DETUNE(64),PHASE(0),GATES(85),COLOR(128),SHAPE(64),GAIN(128),FLAGS(SINE|LFO))
		GO4K_FST(AMOUNT(72),DEST(11*MAX_UNIT_SLOTS+2+FST_SET))
		GO4K_FOP(OP(FOP_POP))
		GO4K_VCO(TRANSPOSE(73),DETUNE(64),PHASE(0),GATES(85),COLOR(128),SHAPE(64),GAIN(128),FLAGS(SINE|LFO))
		GO4K_FST(AMOUNT(56),DEST(12*MAX_UNIT_SLOTS+2+FST_SET))
		GO4K_FST(AMOUNT(56),DEST(12*MAX_UNIT_SLOTS+5+FST_SET))
		GO4K_FOP(OP(FOP_POP))
		GO4K_VCO(TRANSPOSE(77),DETUNE(64),PHASE(0),GATES(85),COLOR(128),SHAPE(64),GAIN(128),FLAGS(SINE|LFO))
		GO4K_FST(AMOUNT(72),DEST(13*MAX_UNIT_SLOTS+2+FST_SET))
		GO4K_FOP(OP(FOP_POP))
		GO4K_VCO(TRANSPOSE(64),DETUNE(64),PHASE(0),GATES(85),COLOR(0),SHAPE(64),GAIN(128),FLAGS(TRISAW))
		GO4K_VCO(TRANSPOSE(64),DETUNE(64),PHASE(0),GATES(85),COLOR(0),SHAPE(64),GAIN(128),FLAGS(TRISAW))
		GO4K_VCO(TRANSPOSE(64),DETUNE(64),PHASE(0),GATES(85),COLOR(0),SHAPE(64),GAIN(128),FLAGS(TRISAW))
		GO4K_FOP(OP(FOP_ADDP))
		GO4K_FOP(OP(FOP_ADDP))
		GO4K_FOP(OP(FOP_MULP))
		GO4K_VCF(FREQUENCY(100),RESONANCE(96),VCFTYPE(LOWPASS))
		GO4K_DLL(PREGAIN(64),DRY(64),FEEDBACK(64),DAMP(0),FREQUENCY(0),DEPTH(0),DELAY(18),COUNT(1))
		GO4K_PAN(PANNING(64))
		GO4K_OUT(GAIN(48), AUXSEND(64))
	/* Instrument3 */
		GO4K_ENV(ATTAC(0),DECAY(128),SUSTAIN(128),RELEASE(0),GAIN(128))
		GO4K_FOP(OP(FOP_LOADNOTE))
		GO4K_FSTG(AMOUNT(128),DEST((0*go4k_instrument_size*MAX_VOICES/4)+(1*MAX_UNIT_SLOTS+1)+(go4k_instrument_workspace/4)+FST_SET))
		GO4K_FSTG(AMOUNT(128),DEST((0*go4k_instrument_size*MAX_VOICES/4)+(2*MAX_UNIT_SLOTS+1)+(go4k_instrument_workspace/4)+FST_SET))
		GO4K_FSTG(AMOUNT(128),DEST((0*go4k_instrument_size*MAX_VOICES/4)+(4*MAX_UNIT_SLOTS+1)+(go4k_instrument_workspace/4)+FST_SET))
		GO4K_FOP(OP(FOP_POP))
		GO4K_FOP(OP(FOP_POP))
	/* Instrument4 */
		GO4K_ENV(ATTAC(0),DECAY(32),SUSTAIN(96),RELEASE(64),GAIN(128))
		GO4K_FST(AMOUNT(128),DEST(0*MAX_UNIT_SLOTS+2+FST_SET))
		GO4K_ENV(ATTAC(0),DECAY(70),SUSTAIN(0),RELEASE(0),GAIN(128))
		GO4K_DST(DRIVE(54), SNHFREQ(128), FLAGS(0))
		GO4K_FST(AMOUNT(80),DEST(6*MAX_UNIT_SLOTS+1+FST_SET))
		GO4K_FOP(OP(FOP_POP))
		GO4K_VCO(TRANSPOSE(45),DETUNE(64),PHASE(0),GATES(85),COLOR(64),SHAPE(96),GAIN(128),FLAGS(TRISAW))
		GO4K_FOP(OP(FOP_MULP))
		GO4K_ENV(ATTAC(32),DECAY(76),SUSTAIN(0),RELEASE(0),GAIN(128))
		GO4K_FSTG(AMOUNT(0),DEST((0*go4k_instrument_size*MAX_VOICES/4)+(0*MAX_UNIT_SLOTS+2)+(go4k_instrument_workspace/4)+FST_SET))
		GO4K_FSTG(AMOUNT(0),DEST(7*4+go4k_instrument_workspace))
		GO4K_FOP(OP(FOP_POP))
		GO4K_PAN(PANNING(64))
		GO4K_OUT(GAIN(128), AUXSEND(0))
	/* Instrument5 */
		GO4K_ENV(ATTAC(0),DECAY(82),SUSTAIN(0),RELEASE(0),GAIN(32))
		GO4K_FST(AMOUNT(128),DEST(0*MAX_UNIT_SLOTS+2+FST_SET))
		GO4K_ENV(ATTAC(0),DECAY(70),SUSTAIN(0),RELEASE(0),GAIN(128))
		GO4K_DST(DRIVE(14), SNHFREQ(128), FLAGS(0))
		GO4K_FST(AMOUNT(76),DEST(7*MAX_UNIT_SLOTS+1+FST_SET))
		GO4K_FOP(OP(FOP_POP))
		GO4K_VCO(TRANSPOSE(64),DETUNE(64),PHASE(64),GATES(85),COLOR(64),SHAPE(116),GAIN(86),FLAGS(NOISE))
		GO4K_VCO(TRANSPOSE(59),DETUNE(64),PHASE(64),GATES(85),COLOR(128),SHAPE(64),GAIN(128),FLAGS(SINE))
		GO4K_FOP(OP(FOP_ADDP))
		GO4K_FOP(OP(FOP_MULP))
		GO4K_VCF(FREQUENCY(13),RESONANCE(128),VCFTYPE(BANDSTOP))
		GO4K_ENV(ATTAC(32),DECAY(76),SUSTAIN(0),RELEASE(0),GAIN(128))
		GO4K_FSTG(AMOUNT(0),DEST((0*go4k_instrument_size*MAX_VOICES/4)+(0*MAX_UNIT_SLOTS+2)+(go4k_instrument_workspace/4)+FST_SET))
		GO4K_FSTG(AMOUNT(0),DEST(7*4+go4k_instrument_workspace))
		GO4K_FOP(OP(FOP_POP))
		GO4K_PAN(PANNING(64))
		GO4K_OUT(GAIN(80), AUXSEND(0))
	/* Instrument6 */
		GO4K_ENV(ATTAC(0),DECAY(64),SUSTAIN(0),RELEASE(0),GAIN(128))
		GO4K_VCO(TRANSPOSE(64),DETUNE(64),PHASE(64),GATES(85),COLOR(64),SHAPE(127),GAIN(128),FLAGS(NOISE))
		GO4K_FOP(OP(FOP_MULP))
		GO4K_VCF(FREQUENCY(128),RESONANCE(128),VCFTYPE(BANDPASS))
		GO4K_PAN(PANNING(64))
		GO4K_DLL(PREGAIN(96),DRY(128),FEEDBACK(96),DAMP(64),FREQUENCY(0),DEPTH(0),DELAY(19),COUNT(1))
		GO4K_FOP(OP(FOP_XCH))
		GO4K_DLL(PREGAIN(96),DRY(128),FEEDBACK(96),DAMP(64),FREQUENCY(0),DEPTH(0),DELAY(20),COUNT(1))
		GO4K_FOP(OP(FOP_XCH))
		GO4K_OUT(GAIN(64), AUXSEND(0))
	/* Instrument7 */
		GO4K_ENV(ATTAC(0),DECAY(128),SUSTAIN(128),RELEASE(32),GAIN(128))
		GO4K_VCO(TRANSPOSE(64),DETUNE(64),PHASE(0),GATES(0),COLOR(128),SHAPE(64),GAIN(128),FLAGS(SINE))
		GO4K_VCO(TRANSPOSE(52),DETUNE(72),PHASE(0),GATES(0),COLOR(128),SHAPE(64),GAIN(128),FLAGS(SINE))
		GO4K_FOP(OP(FOP_ADDP))
		GO4K_ENV(ATTAC(48),DECAY(128),SUSTAIN(128),RELEASE(128),GAIN(127))
		GO4K_FST(AMOUNT(96),DEST(8*MAX_UNIT_SLOTS+2+FST_SET))
		GO4K_FOP(OP(FOP_POP))
		GO4K_FOP(OP(FOP_MULP))
		GO4K_DST(DRIVE(64), SNHFREQ(128), FLAGS(0))
		GO4K_DLL(PREGAIN(64),DRY(128),FEEDBACK(64),DAMP(96),FREQUENCY(0),DEPTH(0),DELAY(18),COUNT(1))
		GO4K_PAN(PANNING(64))
		GO4K_OUT(GAIN(64), AUXSEND(48))
	/* Instrument8 */
		GO4K_ENV(ATTAC(0),DECAY(128),SUSTAIN(128),RELEASE(64),GAIN(128))
		GO4K_FOP(OP(FOP_POP))
		GO4K_FOP(OP(FOP_LOADNOTE))
		GO4K_FST(AMOUNT(128),DEST(5*MAX_UNIT_SLOTS+3+FST_SET))
		GO4K_FOP(OP(FOP_POP))
		GO4K_ENV(ATTAC(0),DECAY(128),SUSTAIN(128),RELEASE(64),GAIN(128))
		GO4K_FST(AMOUNT(128),DEST(11*MAX_UNIT_SLOTS+4+FST_SET))
		GO4K_FOP(OP(FOP_POP))
		GO4K_ENV(ATTAC(0),DECAY(128),SUSTAIN(128),RELEASE(64),GAIN(128))
		GO4K_VCO(TRANSPOSE(64),DETUNE(64),PHASE(0),GATES(0),COLOR(64),SHAPE(64),GAIN(128),FLAGS(NOISE))
		GO4K_FOP(OP(FOP_MULP))
		GO4K_VCF(FREQUENCY(0),RESONANCE(64),VCFTYPE(LOWPASS))
		GO4K_DLL(PREGAIN(64),DRY(128),FEEDBACK(64),DAMP(64),FREQUENCY(0),DEPTH(0),DELAY(17),COUNT(1))
		GO4K_PAN(PANNING(64))
		GO4K_OUT(GAIN(64), AUXSEND(0))
	/* Instrument10 */
		GO4K_ENV(ATTAC(32),DECAY(128),SUSTAIN(0),RELEASE(78),GAIN(128))
		GO4K_VCO(TRANSPOSE(64),DETUNE(64),PHASE(0),GATES(0),COLOR(64),SHAPE(127),GAIN(128),FLAGS(NOISE))
		GO4K_FOP(OP(FOP_MULP))
		GO4K_VCF(FREQUENCY(64),RESONANCE(16),VCFTYPE(PEAK))
		GO4K_ENV(ATTAC(0),DECAY(128),SUSTAIN(128),RELEASE(64),GAIN(128))
		GO4K_FST(AMOUNT(128),DEST(7*MAX_UNIT_SLOTS+1+FST_SET))
		GO4K_FOP(OP(FOP_POP))
		GO4K_VCO(TRANSPOSE(64),DETUNE(64),PHASE(0),GATES(0),COLOR(128),SHAPE(64),GAIN(12),FLAGS(SINE|LFO))
		GO4K_FST(AMOUNT(128),DEST(3*MAX_UNIT_SLOTS+5+FST_SET))
		GO4K_FOP(OP(FOP_POP))
		GO4K_FOP(OP(FOP_LOADNOTE))
		GO4K_FST(AMOUNT(128),DEST(3*MAX_UNIT_SLOTS+4+FST_SET))
		GO4K_FOP(OP(FOP_POP))
		GO4K_PAN(PANNING(64))
		GO4K_DLL(PREGAIN(96),DRY(128),FEEDBACK(96),DAMP(64),FREQUENCY(0),DEPTH(0),DELAY(17),COUNT(1))
		GO4K_FOP(OP(FOP_XCH))
		GO4K_DLL(PREGAIN(96),DRY(128),FEEDBACK(96),DAMP(64),FREQUENCY(0),DEPTH(0),DELAY(18),COUNT(1))
		GO4K_FOP(OP(FOP_XCH))
		GO4K_OUT(GAIN(24), AUXSEND(16))
	/* Global */
		GO4K_ACC(ACCTYPE(AUX))
		GO4K_DLL(PREGAIN(40),DRY(128),FEEDBACK(125),DAMP(64),FREQUENCY(0),DEPTH(0),DELAY(1),COUNT(8))
		GO4K_FOP(OP(FOP_XCH))
		GO4K_DLL(PREGAIN(40),DRY(128),FEEDBACK(125),DAMP(64),FREQUENCY(0),DEPTH(0),DELAY(9),COUNT(8))
		GO4K_FOP(OP(FOP_XCH))
		GO4K_ACC(ACCTYPE(OUTPUT))
		GO4K_FOP(OP(FOP_ADDP2))
		GO4K_OUT(GAIN(56), AUXSEND(0))
};

// ---------------------------------------------------------------------------
// comb sizes in samples, indexed by a DLL unit's DELAY() parameter
// ---------------------------------------------------------------------------
static const uint16_t go4k_delay_times[] = {
	0,
	1116,
	1188,
	1276,
	1356,
	1422,
	1492,
	1556,
	1618,
	1140,
	1212,
	1300,
	1380,
	1446,
	1516,
	1580,
	1642,
	15206,
	22810,
	7603,
	11405,
};

#endif // FOURKLANG_SONG_H
