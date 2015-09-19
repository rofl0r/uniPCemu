#ifndef VGA_VRAM_H
#define VGA_VRAM_H

#include "headers/types.h" //Basic types!
#include "headers/hardware/vga/vga.h" //VGA basics!

byte readVRAMplane(VGA_Type *VGA, byte plane, word offset, byte mode); //Read from a VRAM plane!
void writeVRAMplane(VGA_Type *VGA, byte plane, word offset, byte value, byte mode); //Write to a VRAM plane!

#endif