#include <OpenGL/OpenGL.h>
#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/glu.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/CGLContext.h>
#include <Carbon/Carbon.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <OpenAL/al.h>
#include <pthread.h>
#include "alut.h"
#include "4klang/4klang.inh"
#include "shader.minified.frag"

const char* ansiEscapeClearScreen = "\033[2J";
const char* ansiEscapeCursorReset = "\033[0;0H";
const char* ansiEscapeQueryCursor = "\033[6n";

const char* iEmojiAtlas[]={
/*  0 */ "🔴", // rainbow
/*  1 */ "😡", // rainbow
/*  2 */ "🤗", // rainbow
/*  3 */ "🌕", // rainbow
/*  4 */ "🎾", // rainbow
/*  5 */ "🤢", // rainbow
/*  6 */ "🌎", // rainbow
/*  7 */ "🔵", // rainbow
/*  8 */ "😈", // rainbow
/*  9 */ "🍇", // rainbow
/* 10 */ "⚪", // fire plasma
/* 11 */ "🌕", // fire plasma
/* 12 */ "😡", // fire plasma
/* 13 */ "🍇", // fire plasma
/* 14 */ "😈", // fire plasma
/* 15 */ "🌑", // fire plasma
/* 16 */ "🏿", // skintone cube
/* 17 */ "🏾", // skintone cube
/* 18 */ "🏽", // skintone cube
/* 19 */ "🏼", // skintone cube
/* 20 */ "🏻", // skintone cube
/* 21 */ "🌑", // moons
/* 22 */ "🌒", // moons
/* 23 */ "🌓", // moons
/* 24 */ "🌔", // moons
/* 25 */ "🌕", // moons
/* 26 */ "🌖", // moons
/* 27 */ "🌗", // moons
/* 28 */ "🌘", // moons
/* 29 */ "⚫", // grey square tunnel / fizzer
/* 30 */ "🔘", // grey square tunnel / fizzer
/* 31 */ "⚪", // grey square tunnel / fizzer
/* 32 */ "　", // red sdf / constant black
/* 33 */ "💮", // red sdf
/* 34 */ "⭕", // red sdf
/* 35 */ "🔴", // red sdf
/* 36 */ "　", // opening
/* 37 */ "🔸", // opening
/* 38 */ "🌕", // opening
/* 39 */ "⬜", // credits char
/* 40 */ "📕", // rainbow square tunnel
/* 41 */ "📙", // rainbow square tunnel
/* 42 */ "📒", // rainbow square tunnel
/* 43 */ "📗", // rainbow square tunnel
/* 44 */ "📘", // rainbow square tunnel
/* 45 */ "　", // free ?
/* 46 */ "　", // free ?
/* 47 */ "　", // free ?
/* 48 */ "　", // free ?
/* 49 */ "　", // free ?
/* 50 */ "　", // free ?
/* 51 */ "　", // free
/* 52 */ "　", // outrun grid
/* 53 */ "😈", // outrun grid
/* 54 */ "💖", // outrun grid
/* 55 */ "🌸", // outrun grid
/* 56 */ "🍀", // road
/* 57 */ "🥦", // road
/* 58 */ "🎛", // road
/* 59 */ "🗑", // road
/* 60 */ "🌐", // road
/* 61 */ "🎽", // road
/* 62 */ "🔵", // road
/* 63 */ "🌞", // road
};

#define ATLAS_SIZE (sizeof(iEmojiAtlas)/sizeof(iEmojiAtlas[0]))
#if defined(DEBUG)
static_assert(ATLAS_SIZE==64,"Atlas must be 64 entries");
#endif
char* iFixedEmojiAtlas[ATLAS_SIZE];

GLint shaderCompile(const char* fragmentSource)
{
	#if defined(DEBUG)
		if(!strstr(fragmentSource, "void main()"))
		{
			puts("could not find a main() function in shader - did you lazily paste this from shadertoy?");
			exit(1);
		}
	#endif

	// compile shader
	GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(shader, 1, &fragmentSource, 0);
	glCompileShader(shader);

	// shader compiler errors
	#if defined(DEBUG)
		GLint isCompiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			const int maxLength = 2048;
			GLchar errorLog[maxLength];
			glGetShaderInfoLog(shader, maxLength, 0, errorLog);
			puts(errorLog);
			glDeleteShader(shader);
			exit(1);
		}
	#endif

	// link shader
	GLuint program = glCreateProgram();
	glAttachShader(program, shader);
	glLinkProgram(program);

	return program;
}

// need to disable line-buffering for the size measuring to work
struct termios old_tio, new_tio;
void disableLineBuffering(){
	// http://shtrom.ssji.net/skb/getc.html
	unsigned char c;
	tcgetattr(STDIN_FILENO,&old_tio);
	new_tio=old_tio;
	new_tio.c_lflag &=(~ICANON & ~ECHO);
	tcsetattr(STDIN_FILENO,TCSANOW,&new_tio);
}

void restoreLineBuffering(){
	// http://shtrom.ssji.net/skb/getc.html
	tcsetattr(STDIN_FILENO,TCSANOW,&old_tio);
}

int queryRenderedWidth(const char* str){
	// awful hack to special-case for U+3000 Ideographic Space
	if(*(uint32_t*)str == 0x008080E3)
		return 2;
	printf("%s%s%s\n",ansiEscapeCursorReset,str,ansiEscapeQueryCursor);
	int x, y;
	scanf("\033[%d;%1dR", &y, &x);
	return x - 1;
}

void unpack1bit(const uint8_t* src, uint8_t* dst, int length)
{
	for(int i=0;i<length;++i){
		dst[i] = ((src[i/8]>>(i%8))&1) ? 255 : 0;
	}
}

#define NUM_BUFFERS 4 

typedef struct _play_context
{
	SAMPLE_TYPE buffer4kl[SAMPLES_PER_TICK*2];
	ALuint source,buffers[NUM_BUFFERS];
	int running, complete;
} _play_context;

void audio_play(void * context){
	struct _play_context * ctx = (_play_context *) context;

	_4klang_current_tick = 0;

	while (!ctx->running) {
		usleep(10000);
	}

	for (unsigned int i = 0; i < NUM_BUFFERS; ++i) {
		_4klang_render(ctx->buffer4kl);
		alBufferData(ctx->buffers[i], AL_FORMAT_STEREO16, ctx->buffer4kl, sizeof(SAMPLE_TYPE)*SAMPLES_PER_TICK*2, 44100);
		alSourceQueueBuffers(ctx->source, 1, &ctx->buffers[i]);

		if (_4klang_current_tick == 0)
			break;
	}

	alSourcePlay(ctx->source);

	if (_4klang_current_tick == 0) {
		ctx->complete = 1;
	}

	unsigned int current_buffer = 0;

	while (!ctx->complete) {
		ALint number_processed = 0;

		while (number_processed < 1) {
			alGetSourcei(ctx->source, AL_BUFFERS_PROCESSED, &number_processed);
			usleep(1000);
		}

		{
			_4klang_render(ctx->buffer4kl);

			ALuint uiBuffer = 0;

			alSourceUnqueueBuffers(ctx->source, 1, &uiBuffer);
			alBufferData(uiBuffer, AL_FORMAT_STEREO16, ctx->buffer4kl, sizeof(SAMPLE_TYPE)*SAMPLES_PER_TICK*2, 44100);
			alSourceQueueBuffers(ctx->source, 1, &uiBuffer);

			++current_buffer;

			if (_4klang_current_tick == 0) {
				ctx->complete = 1;
				break;
			}
		}
	}
}

int main(){
	const int kWidth = 40;
	const int kHeight = 24;

#if defined(DEBUG) && 0
	fputs(ansiEscapeClearScreen,stdout);  // clear full screen
	fputs(ansiEscapeCursorReset,stdout);  // reset cursor
	printf("atlas is %lu chars\n", ATLAS_SIZE);
	printf("display is %d x %d\n", kWidth, kHeight);
	printf("press enter to continue...\n");
	getchar();
#endif

	// fix up emoji widths
	disableLineBuffering();
	for(int i=0;i<ATLAS_SIZE;++i){
		int w = queryRenderedWidth(iEmojiAtlas[i]);
		//printf("%s|%d\n", iEmojiAtlas[i], w);
		int length = strlen(iEmojiAtlas[i]) + (2-w);
		iFixedEmojiAtlas[i] = (char*)malloc(length + 1);
		memset(iFixedEmojiAtlas[i], ' ', length);
		memcpy(iFixedEmojiAtlas[i], iEmojiAtlas[i], strlen(iEmojiAtlas[i]));
		iFixedEmojiAtlas[i][length] = 0;
	}
	restoreLineBuffering();

	const CGLPixelFormatAttribute attribs[]={(CGLPixelFormatAttribute)0};			// anything goes.
	CGLPixelFormatObj formats;
	GLint num_pix;
	CGLChoosePixelFormat(attribs,&formats,&num_pix);
	CGLContextObj ctx;
	CGLCreateContext(formats,0,&ctx);					// first hit is good enough for us.
	CGLSetCurrentContext(ctx);

	GLint shaderProgram;
	uint8_t cpuFramebuffer[kWidth*kHeight];

	// create framebuffer - can't remember which tutorial i stole this from lmao
	GLuint FramebufferName = 0;
	GLuint renderedTexture;
	{
		// The framebuffer, which regroups 0, 1, or more textures, and 0 or 1 depth buffer.
		glGenFramebuffers(1, &FramebufferName);
		glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);

		// The texture we're going to render to
		glGenTextures(1, &renderedTexture);

		// "Bind" the newly created texture : all future texture functions will modify this texture
		glBindTexture(GL_TEXTURE_2D, renderedTexture);

		// Give an empty image to OpenGL ( the last "0" )
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, kWidth, kHeight, 0, GL_RED, GL_UNSIGNED_BYTE, 0);

		// Poor filtering. Needed !
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		// Set "renderedTexture" as our colour attachement #0
		glFramebufferTextureEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, renderedTexture, 0);

		// Set the list of draw buffers.
		GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
		glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers
	}

	// create credits texture
	{
		const uint8_t packedCredits[]={
			#include "creds.txt"
		};
		GLuint texid;
		glGenTextures(1,&texid);
		const int w=40,h=24;
		glBindTexture(GL_TEXTURE_2D, texid);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		uint8_t pixels[w*h];
		unpack1bit(packedCredits, pixels, w*h);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texid);
	}

	shaderProgram = shaderCompile(shader_frag);

	static _play_context play_context = {0};

	alutInit(0,0);
	alGenSources(1,&play_context.source);
	alGenBuffers(NUM_BUFFERS,&play_context.buffers);
	play_context.running = 0;
	play_context.complete = 0;

#if defined(HAS_AUDIO)
	static pthread_t synthRenderThread; 
	if (pthread_create(&synthRenderThread, NULL, audio_play, &play_context)) { 
		fprintf(stderr, "pthread_create() failed\n");
		exit(1);
	}
#endif

	// set background black - this chokes performance massively on vscode for some reason
	//fputs(ansiEscapeBackgroundBlack,stdout);

	fputs(ansiEscapeClearScreen,stdout);  // clear full screen
	fputs(ansiEscapeCursorReset,stdout);  // reset cursor

	play_context.running = 1;
	float lastBeat;
	for(int iFrame=0;;++iFrame)
	{
		float iTime_offset;
		int samplePosition_offset;
		alGetSourcef(play_context.source, AL_SEC_OFFSET, &iTime_offset);
		alGetSourcei(play_context.source, AL_SAMPLE_OFFSET, &samplePosition_offset);

		float iTime;
		int samplePosition;
		long current_tick = _4klang_current_tick;
		if (current_tick < 4) current_tick = 0;
		else current_tick -= 4;
		samplePosition = SAMPLES_PER_TICK * current_tick;
		iTime = (float)(samplePosition) / 44100.0;

		samplePosition += samplePosition_offset;
		iTime += iTime_offset;

		float iBeat = (float)((double)samplePosition / SAMPLES_PER_BEAT);

		glUseProgram(shaderProgram);
		glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);
		glViewport(0,0,kWidth,kHeight);
		glTexCoord4f(kWidth, kHeight, iTime, iBeat);
		glRecti(-1,-1,1,1);
		glSwapAPPLE();

		glReadPixels(0,0,kWidth,kHeight,GL_RED,GL_UNSIGNED_BYTE,cpuFramebuffer);

		char framebuffer[1048576]; // 1MB should be enough for anyone..
		char* writePtr = framebuffer;
		memcpy(writePtr, ansiEscapeCursorReset, strlen(ansiEscapeCursorReset)); // reset cursor position
		writePtr += strlen(ansiEscapeCursorReset);
		for(int y=kHeight;y-->0;){ // OpenGL-style inverted y axis
			for(int x=0;x<kWidth;++x){
				uint8_t value = cpuFramebuffer[y*kWidth+x];
				const char* pixel = iFixedEmojiAtlas[value >> 2];
				memcpy(writePtr, pixel, strlen(pixel));
				writePtr += strlen(pixel);
			}
			*writePtr++ = '\n';
		}
		*writePtr = 0;
		fputs(framebuffer,stdout);
		fflush(stdout);
		usleep(10000);

		lastBeat = iBeat;

		if (play_context.complete)
			break;
	}
}
