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

//Precalcs!
OPTINLINE byte getHorizontalPixelPanning(VGA_Type *VGA); //Active horizontal pixel panning when enabled?
void VGA_AttributeController_calcAttributes(VGA_Type *VGA); //Update attributes!
//Seperate attribute modes!
typedef byte(*VGA_AttributeController_Mode)(VGA_AttributeInfo *Sequencer_attributeinfo, VGA_Type *VGA, void *Sequencer); //An attribute controller mode!
byte VGA_AttributeController_8bit(VGA_AttributeInfo *Sequencer_attributeinfo, VGA_Type *VGA, void *Sequencer);
byte VGA_AttributeController_4bit(VGA_AttributeInfo *Sequencer_attributeinfo, VGA_Type *VGA, void *Sequencer);
//Translate 4-bit or 8-bit color to 256 color DAC Index through palette!
OPTINLINE byte VGA_AttributeController(VGA_AttributeInfo *Sequencer_attributeinfo, VGA_Type *VGA, void *Sequencer) //Process attribute to DAC index!
{
	//Originally: VGA_Type *VGA, word Scanline, word x, VGA_AttributeInfo *info
	static VGA_AttributeController_Mode attributecontroller_modes[2] = { VGA_AttributeController_4bit, VGA_AttributeController_8bit }; //Both modes we use!

	//Our changing variables that are required!
	return attributecontroller_modes[VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.ColorEnable8Bit](Sequencer_attributeinfo, VGA, Sequencer); //Passthrough!

}
#endif