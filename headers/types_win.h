#ifndef __EMUTYPES_WIN_H
#define __EMUTYPES_WIN_H

//Normal SDL libraries
#include "SDL.h" //SDL library for windows!
#include "SDL_events.h" //SDL events!
//Windows specific structures!
#include <direct.h>
#include <windows.h>
#include <tchar.h> 
#include <strsafe.h>
#pragma comment(lib, "User32.lib")

#define realdelay(x) (x)

#define delay(us) SDL_Delay(realdelay((uint_32)((us)/1000)))
#define sleep() for (;;) delay(1000000)

#include <direct.h> //For mkdir!
#define domkdir _mkdir

//INLINE options!
#ifdef OPTINLINE
#undef OPTINLINE
#endif

#ifdef __ENABLE_INLINE
//For some reason inline functions don't work on windows?
#define OPTINLINE __inline
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