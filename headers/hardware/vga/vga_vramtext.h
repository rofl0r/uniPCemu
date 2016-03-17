#ifndef __VGA_VRAMTEXT_H
#define __VGA_VRAMTEXT_H

#include "headers/types.h" //Basic types!
#include "headers/hardware/vga/vga.h" //VGA basic types!

void VGA_charsetupdated(VGA_Type *VGA); //The character set settings have been updated?
byte getcharxy(VGA_Type *VGA, byte attribute, byte character, byte x, byte y);
#endif