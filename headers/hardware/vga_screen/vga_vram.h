#ifndef VGA_VRAM_H
#define VGA_VRAM_H

#include "headers/types.h" //Basic types!
#include "headers/hardware/vga.h" //VGA basics!

byte readVRAMplane(VGA_Type *VGA, byte plane, word offset, byte is_renderer); //Read from a VRAM plane!
void writeVRAMplane(VGA_Type *VGA, byte plane, word offset, byte value); //Write to a VRAM plane!
byte getBitPlaneBit(VGA_Type *VGA, byte plane, word offset, byte bit, byte is_renderer);

#endif