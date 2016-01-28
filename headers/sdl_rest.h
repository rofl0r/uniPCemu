#ifdef _WIN32
#ifndef __GNUC__
#ifndef __MINGW32__
//We're protected here against GNU C!
#pragma once

//Basic SDL for rest platforms!
#include "SDL/SDL.h" //SDL library!
#include "SDL/SDL_events.h" //SDL events!
#endif