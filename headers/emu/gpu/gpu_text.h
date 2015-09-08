#ifndef GPU_TEXT_H
#define GPU_TEXT_H

#include "headers/types.h" //Basic types!
#include "headers/emu/gpu/gpu.h" //GPU support!
#include "headers/support/zalloc.h" //Memory protections for our inline function!

//We're using a 8x8 font!
#define GPU_TEXTSURFACE_HEIGHT (int)(PSP_SCREEN_ROWS/10)
#define GPU_TEXTSURFACE_WIDTH (int)(PSP_SCREEN_COLUMNS/10)
#define GPU_TEXTPIXELSY (GPU_TEXTSURFACE_HEIGHT*10)
#define GPU_TEXTPIXELSX (GPU_TEXTSURFACE_WIDTH*10)

//Flags:
//Dirty flag!
#define TEXTSURFACE_FLAG_DIRTY 1

typedef struct
{
	int x;
	int y;
} BACKLISTITEM; //Background list coordinate!

typedef struct
{
//First, the text data (modified during write/read)!
byte text[GPU_TEXTSURFACE_HEIGHT][GPU_TEXTSURFACE_WIDTH]; //Text surface text!
uint_32 font[GPU_TEXTSURFACE_HEIGHT][GPU_TEXTSURFACE_WIDTH]; //Text surface font!
uint_32 border[GPU_TEXTSURFACE_HEIGHT][GPU_TEXTSURFACE_WIDTH]; //Text surface border!

//Dirty flags and rendered data (internal).
uint_32 notdirty[GPU_TEXTPIXELSY][GPU_TEXTPIXELSX]; //This is non-dirty, so use this then!

//List for checking for borders, set by allocator!
BACKLISTITEM backlist[8]; //List of border background positions candidates!
int x,y; //Coordinates currently!
byte flags; //Extra flags for a surface!
byte xdelta, ydelta; //Enable delta coordinates during plotting?
SDL_sem *lock; //For locking the surface!
} GPU_TEXTSURFACE;

//Allocation/deallocation!
GPU_TEXTSURFACE *alloc_GPUtext(); //Allocates a GPU text overlay!
void free_GPUtext(GPU_TEXTSURFACE **surface); //Frees an allocated GPU text overlay!

//Normal rendering/text functions!
uint_64 GPU_textrenderer(void *surface); //Run the text rendering on pspsurface, result is the ammount of ms taken!
int GPU_textgetxy(GPU_TEXTSURFACE *surface,int x, int y, byte *character, uint_32 *font, uint_32 *border); //Read a character+attribute!
int GPU_textsetxy(GPU_TEXTSURFACE *surface,int x, int y, byte character, uint_32 font, uint_32 border); //Write a character+attribute!
void GPU_textprintf(GPU_TEXTSURFACE *surface, uint_32 font, uint_32 border, char *text, ...); //Write a string on the debug screen!
void GPU_textgotoxy(GPU_TEXTSURFACE *surface,int x, int y); //Goto coordinates!
void GPU_textclearrow(GPU_TEXTSURFACE *surface, int y); //Clear a row!
void GPU_textclearscreen(GPU_TEXTSURFACE *surface); //Clear a text screen!
#define GPU_textdirty(surface) memprotect(surface, sizeof(GPU_TEXTSURFACE), NULL)?(((GPU_TEXTSURFACE *)surface)->flags&TEXTSURFACE_FLAG_DIRTY):0
void GPU_enableDelta(GPU_TEXTSURFACE *surface, byte xdelta, byte ydelta); //Enable delta coordinates on the x/y axis!
void GPU_text_updatedelta(SDL_Surface *surface); //Update delta!

void GPU_text_locksurface(GPU_TEXTSURFACE *surface); //Lock a surface for usage!
void GPU_text_releasesurface(GPU_TEXTSURFACE *surface); //Unlock a surface when we're done with it!

#endif