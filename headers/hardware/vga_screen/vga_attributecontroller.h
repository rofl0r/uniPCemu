#ifndef VGA_ATTRIBUTECONTROLLER_H
#define VGA_ATTRIBUTECONTROLLER_H

#include "headers/types.h" //Basic types!
#include "headers/hardware/vga.h" //VGA!

typedef struct
{
	int attribute_graphics; //Use graphics attribute: attribute is raw index into table? 0=Normal operation, 1=Font only, 2=Attribute controller disabled!
	byte attribute; //Attribute for the character!
	byte fontpixel; //Are we a front pixel?
	word charx; //Character x!
	word charinner_x; //Inner x base of character!
	uint_32 attributesource; //What's the source plane of the attribute bits (plane bits set)?
} VGA_AttributeInfo; //Attribute info!

#endif

#include "headers/hardware/vga_screen/vga_sequencer.h" //Sequencer support!

//Precalcs!
#ifdef VGA_Type
OPTINLINE byte getHorizontalPixelPanning(VGA_Type *VGA); //Active horizontal pixel panning when enabled?
OPTINLINE byte getOverscanColor(VGA_Type *VGA); //Get the active overscan color (256 color byte!)
void VGA_AttributeController_calcAttributes(VGA_Type *VGA);
OPTINLINE byte VGA_AttributeController(VGA_AttributeInfo *Sequencer_attributeinfo, VGA_Type *Sequencer_VGA, SEQ_DATA *VGA_Sequencer); //Process attribute: 1=Loaded DAC, 0=Process next for DAC!
#endif