#ifndef TYPESEMU_H
#define TYPESEMU_H

#include <psptypes.h> //For long long etc. (u64 etc).
#include <stdlib.h>
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdebugkb.h> //Keyboard!
#include <stdint.h>
#include <pspdisplay.h>
#include <stdarg.h>
#include <string.h>
#include <pspctrl.h>
#include <psppower.h>
#include <ctype.h> //C type!
#include <stdlib.h>
#include <dirent.h>
#include <pspthreadman.h> //For threads!
#include <float.h> //FLT_MAX support!

//For speaker!
#include <pspaudiolib.h>
#include <math.h>
#include <limits.h>

//For timing!
#include <psprtc.h> //Real Time Clock!
#include <stdio.h>

#define EXIT_PRIORITY 0x11
//Exit priority, higest of all!

typedef uint64_t uint_64; //Take over using new name!
typedef int64_t int_64; //Take over using new name!
typedef uint32_t uint_32; //Take over using new name!
typedef int32_t int_32; //Take over using new name!
typedef unsigned char byte;
typedef unsigned short word;
typedef signed char sbyte; //Signed!
typedef signed short sword; //Signed!

#define TRUE 1
#define FALSE 0

#define printf pspDebugScreenPrintf
#define wherex pspDebugScreenGetX
#define wherey pspDebugScreenGetY
#define gotoxy pspDebugScreenSetXY
#define fontcolor pspDebugScreenSetTextColor
#define backcolor pspDebugScreenSetBackColor
//Real clearscreen:
#define realclrscr pspDebugScreenClear
#define delay sceKernelDelayThread
#define sleep sceKernelSleepThread
#define halt sceKernelExitGame
#define mkdir(dir) sceIoMkdir(dir,0777)

typedef s64 int64;
typedef u64 uint64;
typedef uint64 FILEPOS;

//64-bit file support (or not)!
/*#ifdef __FILE_SUPPORT_64
//64-bits wide file support!
#define fopen64 fopen64
#define fseek64 fseeko64
#define ftell64 ftello64
#define fread64 fread64
#define fwrite64 fwrite64
#define fclose64 fclose64
#else
*/
//32-bits wide file support!
#define fopen64 fopen64
#define fseek64 fseek64
#define ftell64 ftell64
#define feof64 feof64
#define fread64 fread64
#define fwrite64 fwrite64
#define fclose64 fclose64
//#endif

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

typedef int bool; //Booleans are ints!

typedef void (*Handler)(void);    /* A pointer to a handler function */

//Number of characters
#define DEBUGSCREEN_WIDTH 67
#define DEBUGSCREEN_HEIGHT 34

//Ammount of items in a buffer!
#define NUMITEMS(buffer) (sizeof(buffer)/sizeof(buffer[0]))
//Timeout for shutdown: force shutdown!
//When set to 0, shutdown immediately shuts down, ignoring the emulated machine!
#define SHUTDOWN_TIMEOUT 0


//Different kinds of pointers!

typedef struct
{
	union
	{
		struct
		{
			uint_32 offset; //Default: 0
			unsigned int segment; //Default: 0
		};
		unsigned char data[6]; //46-bits as bytes!
	};
} ptr48; //48-bit pointer Adress (32-bit mode)!

typedef struct
{
	union
	{
		struct
		{
			unsigned int offset; //Default: 0
			unsigned int segment; //Default: 0
		};
		unsigned char data[4]; //32-bits as bytes!
	};
} ptr32; //32-bit pointer Adress (16-bit mode)!

typedef struct
{
	union
	{
		struct
		{
			unsigned short limit; //Limit!
			unsigned int base; //Base!
		};
		unsigned char data[6]; //46-bit adress!
	};
} GDTR_PTR;

typedef struct
{
	union
	{
		struct
		{
			unsigned int limit;
			uint_32 base;
		};
		unsigned char data[6]; //46-bit adress!
	};
} IDTR_PTR;

//NULL pointer definition
#define NULLPTR(x) ((x.segment==0) && (x.offset==0))
//Same, but for pointer dereference
#define NULLPTR_PTR(x,location) (ANTINULL(x,location)?((x->segment==0) && (x->offset==0)):1)

#define NULLPROTECT(ptr) ANTINULL(ptr,constsprintf("%s at %i",__FILE__,__LINE__))

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
#define VIDEOMODE_EMU 0x02

//GPU debugging:
//Row with info about the CPU etc.
#define GPU_TEXT_INFOROW 3
//Row with the current CPU ASM command.
#define GPU_TEXT_DEBUGGERROW 4

void BREAKPOINT(); //Safe breakpoint function!

int convertrel(int src, int fromres, int tores); //Relative convert!
uint_32 safe_strlen(char *str, int limit); //Safe safe_strlen function!
char *constsprintf(char *str1, ...); //Concatinate strings (or constants)!
void *ANTINULL(void *ptr, char *location); //ANTI NULL Dereference!

void EMU_Shutdown(int doshutdown); //Shut down the emulator?
void addtimer(float frequency, Handler timer, char *name); //Add a timer!
void removetimer(char *name); //Removes a timer!
void startTimers(); //Start timers!
void stopTimers(); //Stop timers!
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
#define OPTINLINE inline

#include "headers/fopen64.h" //64-bit fopen support!

OPTINLINE double getCurrentClockSpeed(); //Retrieves the current clock speed!
#endif
