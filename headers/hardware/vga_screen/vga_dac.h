#ifndef VGA_DAC_H
#define VGA_DAC_H
#include "headers/types.h" //Basic types!
#include "headers/hardware/vga.h" //VGA support!
#include "headers/hardware/vga_screen/vga_sequencer.h" //Sequencer!

//void plotScanPixel(VGA_Type *VGA, byte currentscreenbottom, word PixelToRender); //Plot a scanline from the VGA to the rendering buffer, located in the vga_screen root module!
void VGA_DUMPDAC(); //Dumps the full DAC!

#endif