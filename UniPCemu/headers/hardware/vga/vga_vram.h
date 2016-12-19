#ifndef VGA_VRAM_H
#define VGA_VRAM_H

#include "headers/types.h" //Basic types!
#include "headers/hardware/vga/vga.h" //VGA basics!

byte readVRAMplane(VGA_Type *VGA, byte plane, uint_32 offset, uint_32 bank); //Read from a VRAM plane!
void writeVRAMplane(VGA_Type *VGA, byte plane, uint_32 offset, uint_32 bank, byte value); //Write to a VRAM plane!

//Direct access to 32-bit VRAM planes!
#define VGA_VRAMDIRECTPLANAR(VGA,vramlocation,bank) *((uint_32 *)((byte *)&VGA->VRAM[((vramlocation<<2)+bank)&VGA->precalcs.VMemMask]))
#define VGA_VRAMDIRECT(VGA,vramlocation,bank) VGA->VRAM[(vramlocation+bank)&VGA->precalcs.VMemMask]

void updateVGAMMUAddressMode(); //Update the currently assigned memory mode for mapping memory by address!

#endif