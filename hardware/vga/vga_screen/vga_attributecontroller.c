#include "headers/hardware/vga.h"
#include "headers/hardware/vga_screen/vga_attributecontroller.h" //Our own typedefs!
#include "headers/hardware/vga_screen/vga_dac.h" //DAC support!
#include "headers/hardware/vga_screen/vga_sequencer_graphicsmode.h" //For masking planes!
#include "headers/hardware/vga_screen/vga_precalcs.h" //Precalculation typedefs etc.

extern byte LOG_RENDER_BYTES; //vga_screen/vga_sequencer_graphicsmode.c

OPTINLINE byte getHorizontalPixelPanning(VGA_Type *VGA) //Active horizontal pixel panning when enabled?
{
	return VGA->precalcs.pixelboost; //Precalculated pixel boost!
}

OPTINLINE byte getOverscanColor(VGA_Type *VGA) //Get the active overscan color (256 color byte!)
{
	return VGA->registers->AttributeControllerRegisters.REGISTERS.OVERSCANCOLORREGISTER; //Take the overscan color!
}

OPTINLINE byte getcolorselect54(VGA_Type *VGA)
{
	return VGA->precalcs.colorselect54; //Bits 5-4 of the color select register!
}

OPTINLINE byte getcolorselect76(VGA_Type *VGA)
{
	return VGA->precalcs.colorselect76; //Bits 7-6 of color select register!
}

OPTINLINE byte getattributefont(byte attr)
{
	return (attr&0xF); //Font color!
}

OPTINLINE byte getattributeback(byte attr,byte filter)
{
	byte temp = attr;
	temp >>= 4; //Shift!
	temp &= filter; //Apply filter!
	return temp; //Need attribute registers below used!
}

//This is a slow function:
OPTINLINE byte getColorPlaneEnableMask(VGA_Type *VGA, VGA_AttributeInfo *Sequencer_attributeinfo) //Determine the bits to process!
{
	register byte result=0; //Init result!
	register byte bitval=1; //Our bit value!
	register uint_32 bitplane;
	register uint_32 colorplanes = Sequencer_attributeinfo->attributesource; //What's our source?
	register uint_32 enableplanes = VGA->registers->AttributeControllerRegisters.REGISTERS.COLORPLANEENABLEREGISTER.DATA; //What planes to enable?
	enableplanes &= 0xF; //Only the low 4 bits are used!
	for (;;) //Loop bits!
	{
		bitplane = colorplanes; //The color planes, low byte!
		bitplane &= 0xF; //Only the low 4 bits are used!
		if ((bitplane&enableplanes)==bitplane) //Planes all valid for this bit?
		{
			result |= bitval; //Enable the bit!
		}
		if (bitval==0x80) return result; //Done!
		bitval <<= 1; //Shift bit to next bit value!
		colorplanes >>= 4; //Shift to next bit planes!
	}
}

//Dependant on mode control register and underline location register
void VGA_AttributeController_calcPixels(VGA_Type *VGA)
{
	byte pixelon, charinnery, currentblink;
	int Attribute; //This changes!
	
	byte enableblink = VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.BlinkEnable; //Enable blink?	
	
	byte underlinelocation = VGA->registers->CRTControllerRegisters.REGISTERS.UNDERLINELOCATIONREGISTER.UnderlineLocation; //Underline location!	
	
	byte graphicsdisabled;
	graphicsdisabled = !VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.AttributeControllerGraphicsEnable; //Text mode?

	byte *processpixelprecalcs = &VGA->precalcs.processpixelprecalcs[0]; //The pixel precalcs!	
	
	for (pixelon=0;pixelon<2;pixelon++) //All values of pixelon!
	{
		for (currentblink=0;currentblink<2;currentblink++) //All values of currentblink!
		{
			for (charinnery=0;charinnery<0x20;charinnery++)
			{
				for (Attribute=0;Attribute<0x100;Attribute++)
				{
					byte fontstatus = pixelon; //What font status? By default this is the font/back status!
					//Blinking capability!
					
					//Underline capability!
					if (!fontstatus) //Not already foreground?
					{
						if ((charinnery==underlinelocation) && graphicsdisabled) //Underline (Non-graphics mode only)?
						{
							if ((Attribute&0x73)==0x01) //Underline?
							{
								fontstatus = 1; //Force font color for underline WHEN FONT ON (either <blink enabled and blink ON> or <blink disabled>)!
							}
						}
					}
					
					if (enableblink) //Blink affects font?
					{
						if (Attribute&0x80) //Blink enabled?
						{
							fontstatus &= currentblink; //Need blink on to show!
						}
					}
					
					uint_32 pos = Attribute;
					pos <<= 5; //Create room!
					pos |= charinnery; //Add!
					pos <<= 1; //Create room!
					pos |= currentblink;
					pos <<= 1; //Create room!
					pos |= pixelon;
					//attribute,charinnery,currentblink,pixelon: 8,5,1,1: Less shifting at a time=More speed!
					processpixelprecalcs[pos] = fontstatus; //Our result for this combination!
				}
			}
		}
	}
}

//Dependant on pallete, index register, mode control register, colorselect54&76.
void VGA_AttributeController_calcColorLogic(VGA_Type *VGA)
{
	byte enableblink=0;
	enableblink = VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.BlinkEnable; //Enable blink?	

	byte palletteenable=0;
	byte pallette54=0; //Pallette 5-4 used?
	byte colorselect54=0; //Color select bits 5-4!
	byte colorselect76=0; //Color select bits 7-6!
	byte backgroundfilter=0;
	
	palletteenable = VGA->registers->CRTControllerRegisters.REGISTERS.ATTRIBUTECONTROLLERTOGGLEREGISTER.PAL; //Internal palette enabled?
	if (palletteenable) //Precalcs for palette?
	{
		pallette54 = VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.PaletteBits54Select; //Use pallette bits 5-4?
		if (pallette54) //Use pallette bits 5-4?
		{
			colorselect54 = getcolorselect54(VGA); //Retrieve pallete bits 5-4!
		}
		colorselect76 = getcolorselect76(VGA); //Retrieve pallete bits 7-6!
	}
	backgroundfilter = (~(enableblink<<3))&0xF; //Background filter depends on blink & full background!

	byte *colorlogicprecalcs;
	colorlogicprecalcs = &VGA->precalcs.colorlogicprecalcs[0]; //The color logic precalcs!	
	
	word Attribute;
	byte pixelon;
	for (pixelon=0;pixelon<2;pixelon++)
	{
		for (Attribute=0;Attribute<0x100;Attribute++)
		{
			byte CurrentDAC;
			//Determine pixel font or back color to PAL index!
			if (pixelon)
			{
				CurrentDAC = getattributefont(Attribute); //Font!
			}
			else
			{
				CurrentDAC = getattributeback(Attribute,backgroundfilter); //Back!
			}

			if (palletteenable) //Internal palette enable?
			{
				CurrentDAC &= 0xF; //We only have 16 indexes!
				//Use original 16 color palette!
				CurrentDAC = VGA->registers->AttributeControllerRegisters.REGISTERS.PALETTEREGISTERS[CurrentDAC].InternalPaletteIndex; //Translate base index into DAC Base index!

				//First, bit 4&5 processing if needed!
				if (pallette54) //Bit 4&5 map to the C45 field of the Color Select Register, determined by bit 7?
				{
					CurrentDAC &= 0xF; //Take only the first 4 bits!
					CurrentDAC |= colorselect54; //Use them as 4th&5th bit!
				}
				//Else: already 6 bits wide fully!

				//Finally, bit 6&7 always processing!
				CurrentDAC |= colorselect76; //Apply bits 6&7!
			}
			uint_32 pos = Attribute; //Attribute!
			pos <<= 1; //Make room!
			pos |= pixelon; //Pixel on?
			colorlogicprecalcs[pos] = CurrentDAC; //Save the translated value!
		}
	}
}

//Translate 4-bit or 8-bit color to 256 color DAC Index through palette!
OPTINLINE void VGA_AttributeController(VGA_AttributeInfo *Sequencer_attributeinfo, VGA_Type *VGA) //Process attribute to DAC index!
{
	//Originally: VGA_Type *VGA, word Scanline, word x, VGA_AttributeInfo *info

	//Our changing variables that are required!
	if (VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.ColorEnable8Bit) //Attribute controller disabled?
	{
		return; //Take raw!
	}

	//pixellookup |= Sequencer_attributeinfo.charinnery; //Don't do this? Always 0 by default (processed by the current row)?
	
	//Now, process all pixels!
	//First, process attribute!
	register uint_32 lookup = Sequencer_attributeinfo->attribute;
	lookup <<= 5; //Make room!
	lookup |= Sequencer_attributeinfo->charinner_y;
	lookup <<= 1; //Make room!
	lookup |= VGA->TextBlinkOn; //Blink!
	lookup <<= 1; //Make room for the pixelon!
	lookup |= Sequencer_attributeinfo->fontpixel; //Generate the lookup value!
	lookup = VGA->precalcs.processpixelprecalcs[lookup]; //Look our pixel font/back up!
	
	register uint_32 Attribute; //Current DAC Index from 256-color by default!
	Attribute = Sequencer_attributeinfo->attribute; //Load DAC defaults!
	Attribute &= getColorPlaneEnableMask(VGA,Sequencer_attributeinfo); //Mask color planes off if needed!
	Attribute <<= 1; //Make room for the active pixel!
	Attribute |= lookup; //Generate the DAC Index lookup using the information plus On/Off information!
	Sequencer_attributeinfo->attribute = VGA->precalcs.colorlogicprecalcs[Attribute]; //Look the DAC Index up!

	//DAC Index loaded for this pixel!
}