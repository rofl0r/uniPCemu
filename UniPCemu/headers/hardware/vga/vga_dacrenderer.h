#ifndef VGA_DACRENDERER_H
#define VGA_DACRENDERER_H
#include "headers/types.h" //Basic types!
#include "headers/hardware/vga/vga.h" //VGA support!
#include "headers/hardware/vga/vga_renderer.h" //Sequencer!

void DAC_updateEntry(VGA_Type *VGA, byte entry); //Update a DAC entry for rendering!
void DAC_updateEntries(VGA_Type *VGA); //Update all DAC entries for rendering!
uint_32 GA_color2bw(uint_32 color); //Convert color values to b/w values!

void VGA_DUMPColors(); //Dumps the full DAC and Attribute colors!

void VGA_initBWConversion(); //Init B/W conversion data!
byte DAC_Use_BWMonitor(byte use); //Use B/W monitor?
byte DAC_BWColor(byte use); //What B/W color to use?
#endif
