#ifndef VGA_SEQUENCER_TEXTMODE_H
#define VGA_SEQUENCER_TEXTMODE_H

#include "headers/types.h"
#include "headers/hardware/vga/vga.h" //For VGA_Info!
#include "headers/hardware/vga/vga_attributecontroller.h" //For attribute info result!
#include "headers/hardware/vga/vga_sequencer.h" //For attribute info result!

void VGA_Sequencer_TextMode(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo); //Render a text mode pixel!
void VGA_TextDecoder(VGA_Type *VGA, word loadedlocation);
#endif