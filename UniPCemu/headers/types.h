#ifndef TYPESEMU_H
#define TYPESEMU_H

//Windows safety!
#ifdef RGB
//We overwrite this!
#undef RGB
#endif

//Default used libraries!
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

//Enable inlining if set!
#ifndef _DEBUG
//Disable inlining when debugging!
#define __ENABLE_INLINE
#endif

//Default long long(uint_64) definition!
#define LONGLONGSPRINTF "%llu"

//Platform specific stuff!
#ifdef _WIN32
//Windows?
#include "headers/types_win.h" //Windows specific stuff!
#else
#include "headers/sdl_rest.h" //Rest SDL support!
#ifdef __psp__
//PSP?
#include "headers/types_psp.h" //PSP specific stuff!
#else
//Linux?
#include "headers/types_linux.h" //Linux specific stuff!
#endif
#endif

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
//We're compiling for a Big-Endian CPU!
#define IS_BIG_ENDIAN
#endif

//Univeral 8-bit character type? Given as a define!
#define CharacterType char
//Our basic functionality we need for running this program!
//We have less accuracy using SDL delay: ms instead of us. Round to 0ms(minimal time) if needed!

//Semaphores not defined yet?
#ifndef WaitSem
#define WaitSem(s) SDL_SemWait(s);
#define PostSem(s) SDL_SemPost(s);
#endif

//Halt is redirected to the exit function!
#define quitemu exit

//The port used for emulator callbacks! Must be DWORD-aligned to always archieve correct behaviour!
#define IO_CALLBACKPORT 0xEC

//Short versions of 64-bit integers!
#define u64 uint_64
#define s64 int_64

#ifndef bzero
//PSP and Android already have this?
#define bzero(v,size) memset(v,0,size)
#endif

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

#ifndef IS_GPU
//Pixel component information as determined by the system!
extern byte rshift, gshift, bshift, ashift; //All shift values!
extern uint_32 rmask, gmask, bmask, amask; //All mask values!
#endif

#define RGBA(r, g, b, a) (((a)<<ashift)|((b)<<bshift)|((g)<<gshift)|((r)<<rshift))
#define GETR(x) (((x)&rmask)>>rshift)
#define GETG(x) (((x)&gmask)>>gshift)
#define GETB(x) (((x)&bmask)>>bshift)
#define GETA(x) (((x)&amask)>>ashift)

#ifdef RGB
//We're overwriting default RGB functionality, so remove RGB definition!
#undef RGB
#endif

//RGB is by default fully opaque
#define RGB(r, g, b) RGBA((r),(g),(b),SDL_ALPHA_OPAQUE)

#ifndef IS_GPU
extern uint_32 transparentpixel; //Our define!
#endif

//Special transparent pixel!
#define TRANSPARENTPIXEL transparentpixel

typedef void (*Handler)();    /* A pointer to a handler function */

//Ammount of items in a buffer!
#define NUMITEMS(buffer) (sizeof(buffer)/sizeof(buffer[0]))
//Timeout for shutdown: force shutdown!
//When set to 0, shutdown immediately shuts down, ignoring the emulated machine!
#define SHUTDOWN_TIMEOUT 0

//MIN/MAX: Easy calculation of min/max data!
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
//Range limiter!
#define LIMITRANGE(v,min,max)(((v)<(min))?min:(((v)>(max))?(max):(v)))

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

//Getting/setting bitfields as byte/word/doubleword values!
#define GETBITS(x,shift,mask) ((x&(mask<<shift))>>shift)
#define SETBITS(x,shift,mask,val) x=((x&(~(mask<<shift)))|(((val)&mask)<<shift))

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

void EMU_setDiskBusy(byte disk, byte busy); //Are we busy?
void EMU_Shutdown(byte execshutdown); //Shut down the emulator?
byte shuttingdown(); //Shutting down?
void raiseError(char *source, const char *text, ...); //Raises an error!
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

double getCurrentClockSpeed(); //Retrieves the current clock speed!

void updateInputMain(); //Update input before an instruction (main thread only!)!

//One Megabyte of Memory!
#define MBMEMORY 0x100000
//Exact 14Mhz clock used on a PC!
#define MHZ14 ((15.75/1.1)*1000000.0)

//Inline register usage when defined.
#define INLINEREGISTER register

#if defined(IS_PSP) || defined(ANDROID)
//We're using a static, unchanging screen!
#define STATICSCREEN
#endif

#endif