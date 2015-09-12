#ifndef __EMUTYPES_LINUX_H
#define __EMUTYPES_LINUX_H

#include <sys/stat.h>

#define realdelay(x) ((x)?(x):1)

#define delay(us) SDL_Delay(realdelay((uint_32)((us)/1000)))
#define sleep() for (;;) delay(1000000)

#define domkdir(path) mkdir(path, 0755)

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
#define ENUM8 :byte
#define ENUMS8 :sbyte
#define ENUM16 :word
#define ENUMS16 :sword
#define ENUM32 :uint_32
#define ENUMS32 :int_32

#endif