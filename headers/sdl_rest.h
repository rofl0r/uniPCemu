#ifdef _WIN32
#ifndef __GNUC__
#ifndef __MINGW32__
//We're protected here against GNU C!
#pragma once
#ifndef VISUALC
#define VISUALC
#endif
//Basic SDL for rest platforms!
#include "SDL/SDL.h" //SDL library!
#include "SDL/SDL_events.h" //SDL events!
#endif
#endif
#endif

#ifndef VISUALC
#include <SDL/SDL.h> //SDL library!
#include <SDL/SDL_events.h> //SDL events!
#endif