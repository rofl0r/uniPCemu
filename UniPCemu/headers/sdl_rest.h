#ifdef _WIN32
#ifndef __GNUC__
#ifndef __MINGW32__
//We're protected here against GNU C!
#pragma once
#ifndef VISUALC
#define VISUALC
#define IS_WINDOWS
#endif
#ifdef SDL2
//Basic SDL for rest platforms!
#include "SDL2/SDL.h" //SDL library!
#include "SDL2/SDL_events.h" //SDL events!
#else
//Basic SDL for rest platforms!
#include "SDL/SDL.h" //SDL library!
#include "SDL/SDL_events.h" //SDL events!
#endif
#endif
#endif
#endif

#ifndef VISUALC
#ifdef SDL2
#ifdef ANDROID
#include "SDL.h" //SDL library!
#include "SDL_events.h" //SDL events!
#else
//Basic SDL for rest platforms!
#include <SDL2/SDL.h> //SDL library!
#include <SDL2/SDL_events.h> //SDL events!
#endif
#else
#ifdef ANDROID
#include "SDL.h" //SDL library!
#include "SDL_events.h" //SDL events!
#else
#include <SDL/SDL.h> //SDL library!
#include <SDL/SDL_events.h> //SDL events!
#endif
#endif
#endif