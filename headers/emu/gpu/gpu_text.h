#ifndef GPU_TEXT_H
#define GPU_TEXT_H

#include "headers/types.h" //Basic types!
#include "headers/emu/gpu/gpu.h" //GPU support!

//We're using a 8x8 font!
#define GPU_TEXTSURFACE_HEIGHT (PSP_SCREEN_ROWS>>3)
#define GPU_TEXTSURFACE_WIDTH (PSP_SCREEN_COLUMNS>>3)
#define GPU_TEXTPIXELSY (GPU_TEXTSURFACE_HEIGHT<<3)
#define GPU_TEXTPIXELSX (GPU_TEXTSURFACE_WIDTH<<3)

//Flags!
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
byte dirty[GPU_TEXTPIXELSY][GPU_TEXTPIXELSX]; //This is dirty and needs to be updated?
uint_32 notdirty[GPU_TEXTPIXELSY][GPU_TEXTPIXELSX]; //This is non-dirty, so use this then!

//List for checking for borders, set by allocator!
BACKLISTITEM backlist[8]; //List of border background positions candidates!
int x,y; //Coordinates currently!
byte flags; //Extra flags for a surface!
} PSP_TEXTSURFACE;

//Allocation/deallocation!
PSP_TEXTSURFACE *alloc_GPUtext(); //Allocates a GPU text overlay!
void free_GPUtext(PSP_TEXTSURFACE **surface); //Frees an allocated GPU text overlay!

//Normal rendering/text functions!
uint_64 GPU_textrenderer(void *surface); //Run the text rendering on pspsurface, result is the ammount of ms taken!
int GPU_textgetxy(PSP_TEXTSURFACE *surface,int x, int y, byte *character, uint_32 *font, uint_32 *border); //Read a character+attribute!
int GPU_textsetxy(PSP_TEXTSURFACE *surface,int x, int y, byte character, uint_32 font, uint_32 border); //Write a character+attribute!
void GPU_textprintf(PSP_TEXTSURFACE *surface, uint_32 font, uint_32 border, char *text, ...); //Write a string on the debug screen!
void GPU_textgotoxy(PSP_TEXTSURFACE *surface,int x, int y); //Goto coordinates!
void GPU_textclearrow(PSP_TEXTSURFACE *surface, int y); //Clear a row!
void GPU_textclearscreen(PSP_TEXTSURFACE *surface); //Clear a text screen!
OPTINLINE byte GPU_textdirty(void *surface); //Are we dirty?
#endif