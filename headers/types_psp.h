#pragma once

#define delay(us) sceKernelDelayThread(us?us:1)
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
