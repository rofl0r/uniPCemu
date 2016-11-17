#ifndef __EMU_LINUX_H
#define __EMU_LINUX_H

#include "headers/types_base.h" //Base types!

#include <sys/stat.h> //Directory listing & file check support!

#define realdelay(x) ((x)?(x):1)

#define delay(us) SDL_Delay(realdelay((uint_32)((us)/1000)))
#define sleep() for (;;) delay(1000000)

#define domkdir(path) mkdir(path, 0755)

#define removedirectory(dir) remove(dir)

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

#ifdef _LP64
#undef LONG64SPRINTF
#ifdef ANDROID
#define LONG64SPRINTF long long
#else
#define LONG64SPRINTF uint_64
#endif
typedef uint_64 ptrnum;
#else
typedef uint_32 ptrnum;
#endif

//We're Linux!
#define IS_LINUX

#endif
