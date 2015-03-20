#define VGA_ATTRIBUTECONTROLLER

#include "headers/hardware/vga.h"
#include "headers/hardware/vga_screen/vga_attributecontroller.h" //Our own typedefs!
#include "headers/hardware/vga_screen/vga_sequencer_graphicsmode.h" //For masking planes!
#include "headers/hardware/vga_screen/vga_precalcs.h" //Precalculation typedefs etc.

#include "headers/support/log.h" //Debugger!
extern byte LOG_RENDER_BYTES; //vga_screen/vga_sequencer_graphicsmode.c

#define CURRENTBLINK(VGA) VGA->TextBlinkOn

OPTINLINE byte getattributeback(byte textmode, byte attr,byte filter)
{
	register byte temp = attr;
	//Only during text mode: shift!
	if (textmode) //Take the BG nibble!
	{
		temp >>= 4; //Shift high nibble to low nibble!
	}
	temp &= filter; //Apply filter!
	return temp; //Need attribute registers below used!
}

//Dependant on mode control register and underline location register
void VGA_AttributeController_calcAttributes(VGA_Type *VGA)
{
	byte pixelon, charinnery, currentblink;
	word Attribute; //This changes!
	
	byte color256;
	color256 = VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.ColorEnable8Bit; //8-bit colors?

	byte enableblink;
	enableblink = VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.BlinkEnable; //Enable blink?	
	
	byte underlinelocation;
	underlinelocation = VGA->registers->CRTControllerRegisters.REGISTERS.UNDERLINELOCATIONREGISTER.UnderlineLocation; //Underline location!	
	
	byte textmode;
	textmode = !VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.AttributeControllerGraphicsEnable; //Text mode?

	byte *attributeprecalcs;
	attributeprecalcs = &VGA->precalcs.attributeprecalcs[0]; //The attribute precalcs!	

	byte fontstatus;
	
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
			colorselect54 = VGA->precalcs.colorselect54; //Retrieve pallete bits 5-4!
		}
		colorselect76 = VGA->precalcs.colorselect76; //Retrieve pallete bits 7-6!
	}
	backgroundfilter = (~(enableblink<<3))&0xF; //Background filter depends on blink & full background!

	byte colorplanes;
	colorplanes = VGA->registers->AttributeControllerRegisters.REGISTERS.COLORPLANEENABLEREGISTER.DATA; //Read colorplane 256-color!
	colorplanes &= 0xF; //Only 4 bits can be used!

	register byte CurrentDAC; //Current DAC to use!

	for (pixelon=0;pixelon<2;pixelon++) //All values of pixelon!
	{
		for (currentblink=0;currentblink<2;currentblink++) //All values of currentblink!
		{
			for (charinnery=0;charinnery<0x20;charinnery++)
			{
				for (Attribute=0;Attribute<0x100;Attribute++)
				{
					fontstatus = pixelon; //What font status? By default this is the font/back status!
					//Blinking capability!
					
					//Underline capability!
					if (!fontstatus) //Not already foreground?
					{
						if ((charinnery==underlinelocation) && textmode) //Underline (Non-graphics mode only)?
						{
							if ((Attribute&0x73)==0x01) //Underline?
							{
								fontstatus = 1; //Force font color for underline WHEN FONT ON (either <blink enabled and blink ON> or <blink disabled>)!
							}
						}
					}
					
					if (enableblink) //Blink affects font?
					{
						if (getattributeback(textmode,Attribute,0x8)) //Blink enabled?
						{
							fontstatus &= currentblink; //Need blink on to show!
						}
					}
					
					//Determine pixel font or back color to PAL index!
					if (fontstatus)
					{
						CurrentDAC = Attribute; //Load attribute!
					}
					else
					{
						CurrentDAC = getattributeback(textmode,Attribute,backgroundfilter); //Back!
					}

					CurrentDAC &= colorplanes; //Apply color planes!

					if (palletteenable) //Internal palette enable?
					{
						//Use original 16 color palette!
						CurrentDAC = VGA->registers->AttributeControllerRegisters.REGISTERS.PALETTEREGISTERS[CurrentDAC].InternalPaletteIndex; //Translate base index into DAC Base index!

						if (color256) //8-bit colors?
						{
							CurrentDAC &= 0xF; //Take 4 bits only!
						}
						else //Process fully to a DAC index!
						{
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
					}

					word pos = Attribute;
					pos <<= 5; //Create room!
					pos |= charinnery; //Add!
					pos <<= 1; //Create room!
					pos |= currentblink;
					pos <<= 1; //Create room!
					pos |= pixelon;
					//attribute,charinnery,currentblink,pixelon: 8,5,1,1: Less shifting at a time=More speed!
					attributeprecalcs[pos] = CurrentDAC; //Our result for this combination!
				}
			}
		}
	}
}

void VGA_DUMPATTR()
{
	FILE *f;
	f = fopen("attributelogic.dat","wb");
	fwrite(&getActiveVGA()->precalcs.attributeprecalcs,1,sizeof(getActiveVGA()->precalcs.attributeprecalcs),f);
	fclose(f);
}

OPTINLINE byte VGA_getAttributeDACIndex(byte attribute, VGA_AttributeInfo *Sequencer_attributeinfo, VGA_Type *VGA, void *Sequencer)
{
	register word lookup;
	lookup = Sequencer_attributeinfo->attribute; //Take the latched nibbles as attribute!
	lookup <<= 5; //Make room!
	lookup |= ((SEQ_DATA *)Sequencer)->charinner_y;
	lookup <<= 1; //Make room!
	lookup |= CURRENTBLINK(VGA); //Blink!
	lookup <<= 1; //Make room for the pixelon!
	lookup |= Sequencer_attributeinfo->fontpixel; //Generate the lookup value!
	return VGA->precalcs.attributeprecalcs[lookup]; //Give the data from the lookup table!
}

byte VGA_AttributeController_8bit(VGA_AttributeInfo *Sequencer_attributeinfo, VGA_Type *VGA, void *Sequencer)
{
	static byte curnibble = 0;
	static byte latchednibbles = 0; //What nibble are we currently?

	//First, execute the shift and add required in this mode!
	latchednibbles <<= 4; //Shift high!
	latchednibbles |= VGA_getAttributeDACIndex(Sequencer_attributeinfo->attribute,Sequencer_attributeinfo,VGA,Sequencer); //Latch to DAC Nibble!

	curnibble ^= 1; //Reverse current nibble!
	Sequencer_attributeinfo->attribute = latchednibbles; //Look the DAC Index up!
	return curnibble; //Give us the next nibble, when needed, please!
}

byte VGA_AttributeController_4bit(VGA_AttributeInfo *Sequencer_attributeinfo, VGA_Type *VGA, void *Sequencer)
{
	Sequencer_attributeinfo->attribute = VGA_getAttributeDACIndex(Sequencer_attributeinfo->attribute, Sequencer_attributeinfo, VGA, Sequencer); //Look the DAC Index up!
	return 0; //We're ready to execute: we contain a pixel to plot!
}

byte VGA_AttributeController(VGA_AttributeInfo *Sequencer_attributeinfo, VGA_Type *VGA, void *Sequencer) //Process attribute to DAC index!
{
	//Originally: VGA_Type *VGA, word Scanline, word x, VGA_AttributeInfo *info
	static VGA_AttributeController_Mode attributecontroller_modes[2] = { VGA_AttributeController_4bit, VGA_AttributeController_8bit }; //Both modes we use!

	//Our changing variables that are required!
	return attributecontroller_modes[VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.ColorEnable8Bit](Sequencer_attributeinfo, VGA, Sequencer); //Passthrough!
}
