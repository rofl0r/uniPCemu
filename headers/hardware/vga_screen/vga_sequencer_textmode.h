#ifndef VGA_SEQUENCER_TEXTMODE_H
#define VGA_SEQUENCER_TEXTMODE_H

#include "headers/types.h"
#include "headers/hardware/vga.h" //For VGA_Info!
#include "headers/hardware/vga_screen/vga_attributecontroller.h" //For attribute info result!
#include "headers/hardware/vga_screen/vga_sequencer.h" //For attribute info result!

//Character sizes in pixels!
OPTINLINE byte getcharacterwidth(VGA_Type *VGA);
OPTINLINE byte getcharacterheight(VGA_Type *VGA);

//OPTINLINE void VGA_Sequencer_TextMode(VGA_Type *Sequencer_VGA,word Sequencer_Scanline, word Sequencer_x, word Sequencer_tempx, word Sequencer_tempy, byte Sequencer_bytepanning, VGA_AttributeInfo *Sequencer_attributeinfo); //Process text-data line! Returns attribute info!
//void VGA_Sequencer_TextMode(VGA_Type *VGA,VGA_AttributeInfo *Sequencer_Attributeinfo, word tempx,word tempy,word x,word Scanline,uint_32 bytepanning); //Process text-data line! Returns attribute info!
void VGA_Sequencer_TextMode(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo); //Render a text mode pixel!
#endif