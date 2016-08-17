#ifndef VGA_DAC_H
#define VGA_DAC_H
#include "headers/types.h" //Basic types!
#include "headers/hardware/vga/vga.h" //VGA support!
#include "headers/hardware/vga/vga_sequencer.h" //Sequencer!

void DAC_updateEntry(VGA_Type *VGA, byte entry); //Update a DAC entry for rendering!
void DAC_updateEntries(VGA_Type *VGA); //Update all DAC entries for rendering!

void VGA_DUMPColors(); //Dumps the full DAC and Attribute colors!

void VGA_initBWConversion(); //Init B/W conversion data!
byte DAC_Use_BWMonitor(byte use); //Use B/W monitor?
byte DAC_BWColor(byte use); //What B/W color to use?
#endif
