#ifndef __EMU_PSP_H
#define __EMU_PSP_H

#include <pspkernel.h> //PSP kernel support!
#include "headers/types_base.h" //Base types!

//We disable semaphores on the PSP: we're a single-tasking system which is otherwise too slow!
#define WaitSem(sem) {}
#define PostSem(sem) {}

#define delay(us) sceKernelDelayThread(us?us:1)
#define sleep sceKernelSleepThread

#define domkdir(dir) sceIoMkdir(dir,0777)
#define removedirectory(dir) sceIoRmdir(dir)

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

//We're PSP!
#define IS_PSP

#endif