#ifndef GPU_H
#define GPU_H

#include "headers/types.h" //Global types
#include "headers/bios/bios.h" //BIOS Settings support (for VIDEO_DIRECT)

#ifdef _WIN32
#include "SDL_rotozoom.h"
#include "SDL_gfxPrimitives.h"
#else
#include "SDL/SDL_rotozoom.h" //Rotate&Zoom package for SDL!
#include <SDL/SDL.h>
#ifdef __psp__
//PSP only, doesn't compile under linux!
#include "SDL/SDL_gfxPrimitives.h" //Graphics primitives (plot pixel)
#endif
#endif
//Resolution of PSP Screen!
#define PSP_SCREEN_ROWS 272
#define PSP_SCREEN_COLUMNS 480

//Maximum ammount of display pages used by the GPU
#define PC_MAX_DISPLAYPAGES 8
//Maximum resolution X (row size)
#define EMU_MAX_X 800
//Maximum resolution Y (number of rows max)
#define EMU_MAX_Y 600
//We're emulating a VGA screen adapter?
#define EMU_VGA 1

//Enable graphics?
#define ALLOW_GPU_GRAPHICS 1

//Enable GPU textmode & graphics(if ALLOW_GPU_GRAPHICS==1)?
#define ALLOW_VIDEO 1

//Allow direct plotting (1:1 plotting)?
//Direct plot forced?
#define VIDEO_DFORCED (BIOS_Settings.VGA_AllowDirectPlot==2)
//Normal dynamic direct plot according to resolution?
#define VIDEO_DIRECT (((GPU.xres<=PSP_SCREEN_COLUMNS) && (GPU.yres<=PSP_SCREEN_ROWS) && (BIOS_Settings.VGA_AllowDirectPlot==1))||VIDEO_DFORCED)

//Start address of real device (PSP) VRAM!
//#define VRAM_START (0x40000000 | 0x04000000)
#define VRAM_START 0x44000000

//Give the pixel from our real screen (after filled a scanline at least)!
#define PSP_SCREEN(x,y) GPU.vram[(y*512)+x]
//Give the pixel from our psp screen we're rendering!
#define PSP_BUFFER(x,y) PSP_SCREEN(x,y)
//Give the pixel from our emulator we're buffering!
#define EMU_BUFFER(x,y) GPU.emu_screenbuffer[(y*EMU_MAX_X)+x]

#define EMU_SCREENBUFFERSIZE (EMU_MAX_Y*EMU_MAX_X) //Video buffer (of max 640x480 pixels!)
#define PSP_SCREENBUFFERSIZE (PSP_SCREEN_ROWS*512) //The PSP's screen buffer we're rendering!

//Show the framerate?
#define SHOW_FRAMERATE (GPU.show_framerate>0)

//Multithread divider for scanlines (on the real screen) (Higher means faster drawing)
//90=1sec.
#define SCANLINE_MULTITHREAD_DIVIDER 90
//Allow rendering (renderer enabled?) (Full VGA speed!)
#define ALLOW_RENDERING 1
//Allow maximum VGA speed (but minimum rendering speed!)
//#define SCANLINE_MULTITHREAD_DIVIDER 1

//U can find info here: http://www.ift.ulaval.ca/~marchand/ift17583/dosints.pdf

#define GPU_GETPIXEL(x,y) EMU_BUFFER(x,y)

//Divide data for fuse_pixelarea!
#define DIVIDED(v,n) (byte)SAFEDIV((double)v,(double)n)
#define CONVERTREL(src,srcfac,dest) SAFEDIV((double)src,(double)srcfac)*(double)dest

typedef struct
{
//Now normal stuff:

	int video_on; //Video on?

	int showpixels; //To show the pixels?
	uint_32* vram; //Direct pointer to REAL vram of the PSP!
//Visual screen to render after VGA etc.!
	uint_32 *emu_screenbuffer; //Dynamic pointer to the emulator screen buffer!

	//Display resolution:
	word xres; //X size of screen
	word yres; //Y size of screen
	byte aspectratio; //Enable GPU letterbox (keeping aspect ratio) while rendering?

	//Extra tricks:
	byte doublewidth; //Double x resolution by duplicating every pixel horizontally!
	byte doubleheight; //Double y resolution by duplicating every pixel vertically!

	//Emulator support!
//Coordinates of the emulator output!
	byte GPU_EMU_color; //Font color for emulator output!

	//Framerate!
	byte show_framerate; //Show the framerate?
	byte frameskip; //Frameskip!
	uint_32 framenr; //Current frame number (for Frameskip, kept 0 elsewise.)

	uint_32 emu_buffer_dirty; //Emu screenbuffer dirty: needs re-rendering?

	//Text surface support!
	Handler textrenderers[10]; //Every surface can have a handler to draw!
	void *textsurfaces[10]; //Up to 10 text surfaces available!

	byte fullscreen; //Are we in fullscreen view?
	byte forceRedraw; //We need a redraw explicitly?
} GPU_type; //GPU data

void initVideoLayer(); //We're for allocating the main video layer, only deallocated using SDL_Quit (when quitting the application)!
void resetVideo(); //Resets the screen (clears); used at start of emulator/reset!
void initVideo(int show_framerate); //Resets the screen (clears); used at start of emulation only!
void doneVideo(); //We're done with video operations?
void startVideo(); //Turn video on!
void stopVideo(); //Turn video off!
void GPU_AspectRatio(byte aspectratio); //Set aspect ratio!

void initVideoMain(); //Resets the screen (clears); used at start of emulator only!
void doneVideoMain(); //Resets the screen (clears); used at end of emulator only!

void GPU_addTextSurface(void *surface, Handler handler); //Register a text surface for usage with the GPU!
void GPU_removeTextSurface(void *surface); //Unregister a text surface (removes above added surface)!

void updateVideo(); //Update the screen resolution on change!
void CPU_updateVideo(); //Actual video update from the CPU side of things!

#define lockGPU() lock(LOCK_GPU)
#define unlockGPU() unlock(LOCK_GPU)
#endif
