#ifndef VGA_SEQUENCER_TEXTMODE_CURSOR_H
#define VGA_SEQUENCER_TEXTMODE_CURSOR_H

#include "headers/types.h"
#include "headers/hardware/vga.h" //VGA!
#include "headers/hardware/vga_screen/vga_sequencer.h" //Sequencer!

OPTINLINE uint_32 getcursorlocation(VGA_Type *VGA); //Location of the cursor!
//OPTINLINE byte is_cursorscanline(VGA_Type *Sequencer_VGA,word Sequencer_x,byte Rendery,uint_32 Sequencer_textmode_charindex,VGA_AttributeInfo *Sequencer_attributeinfo); //Cursor scanline within character is cursor?

#endif