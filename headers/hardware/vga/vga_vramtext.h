#ifndef __VGA_VRAMTEXT_H
#define __VGA_VRAMTEXT_H

#include "headers/types.h" //Basic types!
#include "headers/hardware/vga/vga.h" //VGA basic types!

byte getcharxy(VGA_Type *VGA, byte attribute, byte character, byte x, byte y);
#endif