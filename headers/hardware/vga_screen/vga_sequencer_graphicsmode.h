#ifndef VGA_SEQUENCER_GRAPHICSMODE_H
#define VGA_SEQUENCER_GRAPHICSMODE_H

#include "headers/types.h" //Basic types!
#include "headers/hardware/vga.h" //VGA subset!
#include "headers/hardware/vga_screen/vga_sequencer.h" //Sequencer!

//void VGA_Sequencer_GraphicsMode(VGA_Type *VGA,VGA_AttributeInfo *Sequencer_Attributeinfo, word tempx,word tempy,word x,word Scanline,uint_32 bytepanning); //Render graphics mode screen!
void VGA_Sequencer_GraphicsMode(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo);
#endif