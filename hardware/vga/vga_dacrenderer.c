#define VGA_DACRENDERER

#include "headers/types.h" //Basic types!
//Our signaling!
#include "headers/hardware/vga/vga.h" //VGA support!
#include "headers/hardware/vga/vga_precalcs.h" //Precalculation typedefs etc.
#include "headers/hardware/vga/vga_dacrenderer.h" //Our defs!
#include "headers/support/bmp.h" //BMP support for dumping color information!

void VGA_DUMPColors() //Dumps the full DAC and Color translation tables!
{
	char filename[256];
	bzero(filename,sizeof(filename)); //Init
	mkdir("captures"); //Make sure our directory exists!
	strcpy(&filename[0],"captures/VGA_DAC"); //Generate log of this mode!
	int c,r;
	uint_32 DACBitmap[0x8000]; //Full DAC 1-row bitmap!
	register uint_32 DACVal;
	for (c=0;c<0x100;c++)
	{
		DACVal = getActiveVGA()->precalcs.DAC[c]; //The DAC value!
		if ((DACVal==(uint_32)RGB(0x00,0x00,0x00)) || (!(DACVal&0xFF000000))) //Black or unfilled?
		{
			DACBitmap[c] = 0; //Clear entry!
		}
		else
		{
			DACBitmap[c] = DACVal; //Load the DAC value!
		}
	}
	writeBMP(filename,&DACBitmap[0],16,16,4,4,16); //Simple 1-row dump of the DAC results!
	//Now, write the Attribute results through the DAC pallette!
	for (r=0;r<0x20;r++) //Show all lines available to render!
	{
		for (c=0;c<0x400;c++) //All possible attributes (font and back color)!
		{
			word lookup;
			word ordering;
			lookup = (c&0xFF); //What attribute!
			lookup <<= 5; //Make room for charinner_y and blink/font!
			lookup |= r; //Add the row to dump!
			lookup <<= 2; //Generate room for the ordering!
			ordering = 3; //Load for ordering(blinking on with foreground by default, giving foreground when possible)!
			if (c&0x200) ordering &= 1; //3nd row+? This is our unblinked(force background) value!
			if (c&0x100) ordering &= 2; //Odd row? This is our background pixel!
			lookup |= ordering; //Apply the font/back and blink status!
			//The lookup points to the index!
			register uint_32 DACVal;
			DACVal = getActiveVGA()->precalcs.DAC[getActiveVGA()->precalcs.attributeprecalcs[lookup]]; //The DAC value looked up!
			if ((DACVal==(uint_32)RGB(0x00,0x00,0x00)) || (!(DACVal&0xFF000000))) //Black or unfilled?
			{
				DACBitmap[(r<<10)|c] = 0; //Clear entry!
			}
			else
			{
				DACBitmap[(r<<10)|c] = DACVal; //Load the DAC value!
			}		
		}
	}
	//Attributes are in order top to bottom: attribute foreground, attribute background, attribute foreground blink, attribute background blink affected for all attributes!
	strcpy(&filename[0],"captures/VGA_ATT"); //Generate log of this mode!
	writeBMP(filename,&DACBitmap[0],256,4*0x20,0,0,256); //Simple 4-row dump of every scanline of the attributes through the DAC!
}

byte DAC_whatBWColor = 0; //Default: none!

byte DAC_BWColor(byte use) //What B/W color to use?
{
	if (use < 4) DAC_whatBWColor = use; //Use?
	return DAC_whatBWColor;
}

byte BWconversion_ready = 0; //Are we to initialise our tables?
uint_32 BWconversion_white[0x10000]; //Conversion table for b/w totals(white)!
uint_32 BWconversion_green[0x10000]; //Green channel conversion!
uint_32 BWconversion_amber[0x10000]; //Amber channel conversion!

void VGA_initBWConversion()
{
	if (BWconversion_ready) return; //Abort when already ready!
	BWconversion_ready = 1; //We're ready after this!
	const float amber_red = ((float)0xB9 / (float)255); //Red part!
	const float amber_green = ((float)0x80 / (float)255); //Green part!
	register word a,b; //16-bit!
	register uint_32 n; //32-bit!
	for (n=0;n<0x10000;n++) //Process all possible values!
	{
		//Optimized way of dividing?
		a = n >> 2;
		b = (a >> 2);
		a += b;
		b >>= 2;
		a += b;
		b >>= 2;
		a += b;
		b >>= 2;
		a += b;
		//Now store the results for greyscale and brown!
		BWconversion_white[n] = RGB(a,a,a); //Normal 0-255 scale for White and Green monochrome!
		BWconversion_green[n] = RGB(0,a,0); //Normal 0-255 scale for White and Green monochrome!
		BWconversion_amber[n] = RGB((byte)(((float)a)*amber_red), (byte)(((float)a)*amber_green), 0); //Apply basic color: Create RGB in amber!
	}
}

OPTINLINE uint_32 color2bw(uint_32 color) //Convert color values to b/w values!
{
	register word a; //Our registers we use!
	a = GETR(color); //Load Red channel!
	a += GETG(color); //Load Green channel!
	a += GETB(color); //Load Blue channel!
	
	switch (DAC_whatBWColor) //What color scheme?
	{
	case BWMONITOR_WHITE: //Black/white?
		return BWconversion_white[a]; //RGB Greyscale!
	case BWMONITOR_GREEN: //Green?
		return BWconversion_green[a]; //RGB Green monitor!
	case BWMONITOR_AMBER: //Brown?
		return BWconversion_amber[a]; //RGB Amber monitor!
	default: //Unknown scheme?
		return color; //Can't convert due to an invalid scheme: take the original color!
		break; //Use default!
	}
	return color; //Can't convert: take the original color!
}

uint_32 DAC_BWmonitor(VGA_Type *VGA, byte DACValue)
{
	return color2bw(VGA->precalcs.DAC[DACValue]); //Lookup!
}

uint_32 DAC_colorMonitor(VGA_Type *VGA,byte DACValue)
{
	return VGA->precalcs.DAC[DACValue]; //Lookup!
}

byte DAC_whatBWMonitor = 0; //Default: color monitor!

byte DAC_Use_BWMonitor(byte use)
{
	if (use<2) DAC_whatBWMonitor = use; //Use?
	return DAC_whatBWMonitor; //Give the data!
}