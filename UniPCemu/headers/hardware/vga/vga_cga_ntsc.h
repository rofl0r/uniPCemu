#ifndef VGA_NTSC_H
#define VGA_NTSC_H

#include "headers/types.h" //Basic types!

void RENDER_convertCGAOutput(byte *pixels, uint_32 *renderdestination, uint_32 size); //Convert a row of data to NTSC output!
void RENDER_updateCGAColors(); //Update CGA rendering NTSC vs RGBI conversion!

#endif