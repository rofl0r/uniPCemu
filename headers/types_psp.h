#ifndef __EMU_PSP_H
#define __EMU_PSP_H

#include <pspkernel.h>
#include "headers/types_base.h" //Base types!

#define delay(us) sceKernelDelayThread(us)
#define sleep sceKernelSleepThread

#define domkdir(dir) sceIoMkdir(dir,0777)

//INLINE options!
#ifdef OPTINLINE
#undef OPTINLINE
#endif

#ifdef __ENABLE_INLINE
#define OPTINLINE inline
#else
#define OPTINLINE
#endif

//Enum safety
#define ENUM8
#define ENUMS8
#define ENUM16
#define ENUMS16
#define ENUM32
#define ENUMS32

//PSP pointers are always 32-bit!
typedef uint_32 ptrnum;

#endif