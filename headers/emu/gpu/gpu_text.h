#ifndef GPU_TEXT_H
#define GPU_TEXT_H

#include "headers/types.h" //Basic types!
#include "headers/emu/gpu/gpu.h" //GPU support!

//Allocation/deallocation!
PSP_TEXTSURFACE *alloc_GPUtext(); //Allocates a GPU text overlay!
void free_GPUtext(PSP_TEXTSURFACE **surface); //Frees an allocated GPU text overlay!

//Normal rendering/text functions!
uint_64 GPU_textrenderer(PSP_TEXTSURFACE *surface); //Run the text rendering on pspsurface, result is the ammount of ms taken!
int GPU_textgetxy(PSP_TEXTSURFACE *surface,int x, int y, byte *character, uint_32 *font, uint_32 *border); //Read a character+attribute!
int GPU_textsetxy(PSP_TEXTSURFACE *surface,int x, int y, byte character, uint_32 font, uint_32 border); //Write a character+attribute!
void GPU_textprintf(PSP_TEXTSURFACE *surface, uint_32 font, uint_32 border, char *text, ...); //Write a string on the debug screen!
void GPU_textgotoxy(PSP_TEXTSURFACE *surface,int x, int y); //Goto coordinates!
void GPU_textclearrow(PSP_TEXTSURFACE *surface, int y); //Clear a row!
void GPU_textclearscreen(PSP_TEXTSURFACE *surface); //Clear a text screen!
#endif