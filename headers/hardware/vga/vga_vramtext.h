#ifndef __VGA_VRAMTEXT_H
#define __VGA_VRAMTEXT_H

#include "headers/types.h" //Basic types!
#include "headers/hardware/vga/vga.h" //VGA basic types!

void VGA_charsetupdated(VGA_Type *VGA); //The character set settings have been updated?
byte getcharxy(VGA_Type *VGA, byte attribute, byte character, byte x, byte y);
void dumpVGATextFonts(); //Dump the active fontsets!
void fillCGAfont();
byte getcharxy_CGA(byte character, byte x, byte y); //Retrieve a characters x,y pixel on/off from the unmodified 8x8 table!

#endif