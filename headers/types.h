#ifndef TYPESEMU_H
#define TYPESEMU_H

#include <stdlib.h>

#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h> //C type!
#include <stdlib.h>
#include <float.h> //FLT_MAX support!
#include <math.h>
#include <limits.h>
#include <stdio.h>
#ifdef _WIN32
#include "SDL.h" //SDL library for windows!
#include "SDL_events.h" //SDL events!
#else
#include "SDL/SDL.h" //SDL library!
#include "SDL/SDL_events.h" //SDL events!
#endif

//Enable inlining if set!
#define __ENABLE_INLINE

//Our basic functionality we need for running this program!
//We have less accuracy using SDL delay: ms instead of us. Round to 1ms if needed!
#define delay(us) SDL_Delay(((us)>=1000)?((us)/1000):1)
//Sleep is an infinite delay
#define sleep() for (;;) SDL_Delay(1000)
#define halt SDL_Quit

#ifndef uint_64
#define uint_64 uint64_t
#define int_64 int64_t
#endif

#ifndef uint_32
#define uint_32 uint32_t
#define int_32 int32_t
#endif

//Short versions of 64-bit integers!
#define u64 uint_64
#define s64 int_64

#ifdef _WIN32
//Windows-specific headers!
#include <direct.h> //For mkdir!
#define mkdir _mkdir
#else
//Basic PSP headers!
/*
#define delay sceKernelDelayThread
#define sleep sceKernelSleepThread
#define halt sceKernelExitGame
#define mkdir(dir) sceIoMkdir(dir,0777)
*/
#endif

#define bzero(v,size) memset(v,0,size)

#define EXIT_PRIORITY 0x11
//Exit priority, higest of all!

typedef uint8_t byte;
typedef uint16_t word;
typedef int8_t sbyte; //Signed!
typedef int16_t sword; //Signed!

#define TRUE 1
#define FALSE 0

typedef uint_64 FILEPOS;

//RGB, with and without A (full)
#define RGBA(r, g, b, a) ((r)|((g)<<8)|((b)<<16)|((a)<<24))
//RGB is by default fully opaque
#define RGB(r, g, b) RGBA(r,g,b,0xFF)
//Special transparent pixel!
#define TRANSPARENTPIXEL RGBA(0x00,0x00,0x00,0x00)
//Same, but reversed!
#define GETR(x) ((x)&0xFF)
#define GETG(x) (((x)>>8)&0xFF)
#define GETB(x) (((x)>>16)&0xFF)
#define GETA(x) (((x)>>24)&0xFF)

typedef void (*Handler)(void);    /* A pointer to a handler function */

//Ammount of items in a buffer!
#define NUMITEMS(buffer) (sizeof(buffer)/sizeof(buffer[0]))
//Timeout for shutdown: force shutdown!
//When set to 0, shutdown immediately shuts down, ignoring the emulated machine!
#define SHUTDOWN_TIMEOUT 0

//MIN/MAX: East calculation of min/max data!
#define MIN(a,b) ((a<b)?a:b)
#define MAX(a,b) ((a>b)?a:b)


//Optimized DIV/MUL when possible.
//SAFEDIV/MOD: Safe divide/modulo function. Divide by 0 is caught into becoming 0!
#define SAFEDIVUINT(x,divideby) ((!divideby)?0:OPTDIV(x,divideby))
#define SAFEMODUINT(x,divideby) ((!divideby)?0:OPTMOD(x,divideby))
#define SAFEDIVUINT32(x,divideby) ((!divideby)?0:OPTDIV32(x,divideby))
#define SAFEMODUINT32(x,divideby) ((!divideby)?0:OPTMOD32(x,divideby))
#define SAFEDIV(x,divideby) ((!divideby)?0:(x/divideby))
#define SAFEMOD(x,divideby) ((!divideby)?0:(x%divideby))

//Bit manipulation!
//Turn multiple bits on!
#define BITON(x,bit) ((x)|(bit))
//Turn multiple bits off!
#define BITOFF(x,bit) ((x)&(~(bit)))

//Get a bit value (0 or 1))
#define GETBIT(x,bitnr) (((x)>>(bitnr))&1)

//Set a bit on!
#define SETBIT1(x,bitnr) BITON((x),(1<<(bitnr)))
//Set a bit off!
#define SETBIT0(x,bitnr) BITOFF((x),(1<<(bitnr)))

//Easy rotating!
#define ror(x,moves) ((x >> moves) | (x << (sizeof(x)*8 - moves)))
#define rol(x,moves) ((x << moves) | (x >> (sizeof(x)*8 - moves)))

//Emulator itself:
#define VIDEOMODE_EMU 0x03

//GPU debugging:
//Row with info about the CPU etc.
#define GPU_TEXT_INFOROW 3
//Row with the current CPU ASM command.
#define GPU_TEXT_DEBUGGERROW 4

void BREAKPOINT(); //Safe breakpoint function!

int convertrel(int src, int fromres, int tores); //Relative convert!
uint_32 safe_strlen(char *str, int limit); //Safe safe_strlen function!
char *constsprintf(char *str1, ...); //Concatinate strings (or constants)!

void EMU_Shutdown(int doshutdown); //Shut down the emulator?
void raiseError(char *source, char *text, ...); //Raises an error!
void printmsg(byte attribute, char *text, ...); //Prints a message to the screen!
void delete_file(char *directory, char *filename); //Delete one or more files!
int file_exists(char *filename); //File exists?
byte emu_use_profiler(); //To use the profiler?
unsigned int OPTDIV(unsigned int val, unsigned int division);
unsigned int OPTMOD(unsigned int val, unsigned int division);
unsigned int OPTMUL(unsigned int val, unsigned int multiplication);
uint_32 OPTDIV32(uint_32 val, uint_32 division);
uint_32 OPTMOD32(uint_32 val, uint_32 division);
uint_32 OPTMUL32(uint_32 val, uint_32 multiplication);

void debugrow(char *text); //Log a row to debugrow log!

void speakerOut(word frequency); //Set the PC speaker to a sound or 0 for none!

//INLINE options!
#ifdef OPTINLINE
#undef OPTINLINE
#endif

#ifdef __ENABLE_INLINE
#ifdef _WIN32
//Windows?
//For some reason inline functions don't work on windows?
#define OPTINLINE __inline
#else
//PSP?
#define OPTINLINE inline
#endif
#else
#define OPTINLINE
#endif

OPTINLINE double getCurrentClockSpeed(); //Retrieves the current clock speed!
#endif
