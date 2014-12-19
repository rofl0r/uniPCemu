#ifndef VGA_ATTRIBUTECONTROLLER_H
#define VGA_ATTRIBUTECONTROLLER_H

#include "headers/types.h" //Basic types!
#include "headers/hardware/vga.h" //VGA!

OPTINLINE byte getHorizontalPixelPanning(VGA_Type *VGA); //Active horizontal pixel panning when enabled?
OPTINLINE byte getOverscanColor(VGA_Type *VGA); //Get the active overscan color (256 color byte!)

typedef struct
{
	int attribute_graphics; //Use graphics attribute: attribute is raw index into table? 0=Normal operation, 1=Font only, 2=Attribute controller disabled!
	byte attribute; //Attribute for the character!
	byte charx; //Character x!
	byte chary; //Character y!
	byte charinner_x; //Inner x base of character!
	uint_32 attributesource; //What's the source plane of the attribute bits (plane bits set)?
} VGA_AttributeInfo; //Attribute info!

OPTINLINE void VGA_AttributeController(VGA_AttributeInfo *Sequencer_attributeinfo, VGA_Type *Sequencer_VGA, word Sequencer_x, word Screenx, word Screeny); //Process attribute!

//Precalcs!
#ifdef VGA_Type
void VGA_AttributeController_calcColorLogic(VGA_Type *VGA);
void VGA_AttributeController_calcPixels(VGA_Type *VGA);
#endif

#endif