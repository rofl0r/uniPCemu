#ifndef VGA_DAC_H
#define VGA_DAC_H
#include "headers/types.h" //Basic types!
#include "headers/hardware/vga.h" //VGA support!
#include "headers/hardware/vga_screen/vga_sequencer.h" //Sequencer!

OPTINLINE uint_32 getcol256(VGA_Type *VGA,byte color); //Convert color to RGB!
OPTINLINE uint_32 VGA_DAC(VGA_Type *VGA, byte DACValue); //Process DAC in scanline!
//void plotScanPixel(VGA_Type *VGA, byte currentscreenbottom, word PixelToRender); //Plot a scanline from the VGA to the rendering buffer, located in the vga_screen root module!

#endif