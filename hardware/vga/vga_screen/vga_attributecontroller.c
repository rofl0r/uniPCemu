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
	return ((attr>>4)&filter); //Need attribute registers below used!
}

//This is a slow function:
OPTINLINE byte getColorPlaneEnableMask(VGA_Type *VGA, VGA_AttributeInfo *Sequencer_attributeinfo) //Determine the bits to process!
{
	byte result=0; //Init result!
	byte bit=0; //Init bit!
	byte bitval=1; //Our bit value!
	byte bitplane;
	uint_32 colorplanes = Sequencer_attributeinfo->attributesource; //What's our source?
	byte enableplanes = VGA->registers->AttributeControllerRegisters.REGISTERS.COLORPLANEENABLEREGISTER.DATA; //What planes to enable?
	enableplanes &= 0xF; //Only the low 4 bits are used!
	nextbit:
		bitplane = colorplanes; //The color planes, low byte!
		bitplane &= 0xF; //Only the low 4 bits are used!
		if ((bitplane&enableplanes)==bitplane) //Planes all valid for this bit?
		{
			result |= bitval; //Enable the bit!
		}
		if (++bit&8) return result; //Done!
		bitval <<= 1; //Shift bit to next bit value!
		colorplanes >>= 4; //Shift to next bit planes!
		goto nextbit; //Process next bit!
}

//Dependant on mode control register and underline location register
void VGA_AttributeController_calcPixels(VGA_Type *VGA)
{
	byte processpixel, charinnery, currentblink;
	int DACIndex; //This changes!
	
	byte enableblink=0;
	enableblink = VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.BlinkEnable; //Enable blink?	
	
	byte underlinelocation=0;
	underlinelocation = VGA->registers->CRTControllerRegisters.REGISTERS.UNDERLINELOCATIONREGISTER.UnderlineLocation; //Underline location!	
	
	byte graphicsdisabled;
	graphicsdisabled = !VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.AttributeControllerGraphicsEnable; //Text mode?

	byte *processpixelprecalcs;
	processpixelprecalcs = &VGA->precalcs.processpixelprecalcs[0]; //The pixel precalcs!	
	
	for (processpixel=0;processpixel<2;processpixel++) //All values of processpixel!
	{
		for (currentblink=0;currentblink<2;currentblink++) //All values of currentblink!
		{
			for (charinnery=0;charinnery<32;charinnery++)
			{
				for (DACIndex=0;DACIndex<256;DACIndex++)
				{
					byte allowfont = 1; //Allow font color? Default: yes!
					//Blinking capability!
					if (enableblink) //Enable blink affects allowfont?
					{
						allowfont = currentblink; //To enable using blink, else force disabled?
						if (DACIndex&0x80) //Blink enabled?
						{
							processpixel &= allowfont; //Need blink on to show!
						}
					}
					
					//Underline capability!
					if (!processpixel && graphicsdisabled) //Not already foreground and allowed to check for underline?
					{
						if (charinnery==underlinelocation) //Underline (Non-graphics mode only)?
						{
							if ((DACIndex&0x73)==1) //Underline?
							{
								processpixel = allowfont; //Force font color for underline WHEN FONT ON (either <blink enabled and blink ON> or <blink disabled>)!
							}
						}
					}
					processpixelprecalcs[(DACIndex<<7)|(currentblink<<6)|(charinnery<<1)|processpixel] = processpixel; //Our result for this combination!
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
	
	int DACIndex;
	byte processpixel;
	for (processpixel=0;processpixel<2;processpixel++)
	{
		for (DACIndex=0;DACIndex<256;DACIndex++)
		{
			byte CurrentDAC;
			//Determine pixel font or back color to PAL index!
			if (processpixel)
			{
				CurrentDAC = getattributefont(DACIndex); //Font!
			}
			else
			{
				CurrentDAC = getattributeback(DACIndex,backgroundfilter); //Back!
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
			colorlogicprecalcs[(DACIndex<<1)|processpixel] = CurrentDAC; //Save the translated value!
		}
	}
}

//Translate 4-bit or 8-bit color to 256 color DAC Index through palette!
OPTINLINE void VGA_AttributeController(VGA_AttributeInfo *Sequencer_attributeinfo, VGA_Type *VGA, word Sequencer_x, word Screenx, word Screeny) //Process attribute to DAC index!
{
	//Originally: VGA_Type *VGA, word Scanline, word x, VGA_AttributeInfo *info

	//Our changing variables that are required!
	word y=0;
	if (VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.ColorEnable8Bit) //Attribute controller disabled?
	{
		return; //Take raw!
	}

	byte defaultDAC = Sequencer_attributeinfo->attribute; //Default DAC index!
	
	word pixellookup = defaultDAC;
	pixellookup <<= 1; //Make room for 1 bit!
	pixellookup |= VGA->TextBlinkOn;
	pixellookup <<= 5; //Make room for 5 bits!
	//pixellookup |= Sequencer_attributeinfo.charinnery; //Don't do this? Always 0 by default (processed by the current row)?
	
	//Now, process all pixels!
	//First, process attribute!
	byte processpixel = VGA->CurrentScanLine[y]; //The pixel to process: font(1) or back(0)!
	
	word lookup;
	lookup = pixellookup; //Load defaults!
	lookup += y; //Add the current row for the final row!
	lookup <<= 1; //Make room for the processpixel!
	lookup |= processpixel; //Generate the lookup value!
	processpixel = VGA->precalcs.processpixelprecalcs[lookup]; //Look our pixel font/back up!
	
	byte DACIndex; //Current DAC Index from 256-color by default!
	DACIndex = defaultDAC; //Load DAC defaults!
	DACIndex &= getColorPlaneEnableMask(VGA,Sequencer_attributeinfo); //Mask color planes off if needed!
	
	DACIndex <<= 1; //Make room for the processpixel!
	DACIndex |= processpixel; //Generate the DAC Index lookup!
	DACIndex = VGA->precalcs.colorlogicprecalcs[DACIndex]; //Look the DAC Index up!

	Sequencer_attributeinfo->attribute = DACIndex; //Give the DAC index!
	//DAC Index loaded for this row!
	//Done, all row's pixel(s) loaded!
}