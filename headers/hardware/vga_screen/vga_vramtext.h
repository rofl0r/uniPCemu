#ifndef __VGA_VRAMTEXT_H
#define __VGA_VRAMTEXT_H

#include "headers/types.h" //Basic types!
#include "headers/hardware/vga.h" //VGA basic types!

OPTINLINE byte getcharxy_8(byte character, int x, int y);
OPTINLINE byte getcharxy(VGA_Type *VGA, byte attribute, byte character, byte x, byte y);
void VGALoadCharTable(VGA_Type *VGA,int rows, word startaddr); //Load a character table from ROM to VRAM!
#endif