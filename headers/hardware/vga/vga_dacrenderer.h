#ifndef VGA_DAC_H
#define VGA_DAC_H
#include "headers/types.h" //Basic types!
#include "headers/hardware/vga/vga.h" //VGA support!
#include "headers/hardware/vga/vga_sequencer.h" //Sequencer!

void VGA_DUMPColors(); //Dumps the full DAC and Attribute colors!

void VGA_initBWConversion(); //Init B/W conversion data!
uint_32 getcol256(VGA_Type *VGA,byte color); //Convert color to RGB!
uint_32 DAC_BWmonitor(VGA_Type *VGA, byte DACValue);
uint_32 DAC_colorMonitor(VGA_Type *VGA, byte DACValue);
byte DAC_Use_BWMonitor(byte use);
byte DAC_BWColor(byte use); //What B/W color to use?
#endif