/*

Copyright (C) 2019  Superfury

This file is part of UniPCemu.

UniPCemu is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

UniPCemu is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with UniPCemu.  If not, see <https://www.gnu.org/licenses/>.
*/

#define VGA_DACRENDERER

#include "headers/types.h" //Basic types!
#include "headers/hardware/vga/vga.h" //VGA support!
#include "headers/hardware/vga/vga_precalcs.h" //Precalculation typedefs etc.
#include "headers/hardware/vga/vga_dacrenderer.h" //Our defs!
#include "headers/support/bmp.h" //BMP support for dumping color information!

uint_32 DACBitmap[0x8000]; //Full DAC 1-row bitmap!
extern char capturepath[256]; //Capture path!

void VGA_DUMPColors() //Dumps the full DAC and Color translation tables!
{
	char filename[256];
	cleardata(&filename[0],sizeof(filename)); //Init
	domkdir(capturepath); //Make sure our directory exists!
	safestrcpy(filename,sizeof(filename), capturepath); //Capture path!
	safestrcat(filename,sizeof(filename), "/");
	safestrcat(filename,sizeof(filename), "VGA_DAC"); //Generate log of this mode!
	int c,r;
	INLINEREGISTER uint_32 DACVal;
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
	safestrcpy(filename,sizeof(filename), capturepath); //Capture path!
	safestrcat(filename,sizeof(filename), "/");
	safestrcat(filename,sizeof(filename), "VGA_ATT"); //Generate log of this mode!
	writeBMP(filename,&DACBitmap[0],256,4*0x20,0,0,256); //Simple 4-row dump of every scanline of the attributes through the DAC!
}

byte DAC_whatBWColor = 0; //Default: none!
typedef uint_32(*BWconversion_handler)(uint_32 color); //A B/W conversion handler, if used!

byte BWconversion_ready = 0; //Are we to initialise our tables?
uint_32 BWconversion_white[0x10000]; //Conversion table for b/w totals(white)!
uint_32 BWconversion_green[0x10000]; //Green channel conversion!
uint_32 BWconversion_amber[0x10000]; //Amber channel conversion!
uint_32* BWconversion_palette = &BWconversion_white[0];


byte DAC_BWColor(byte use) //What B/W color to use?
{
	if (use < 4)
	{
		DAC_whatBWColor = use; //Use?
		switch (DAC_whatBWColor) //What color scheme?
		{
		case BWMONITOR_WHITE: //Black/white?
			BWconversion_palette = &BWconversion_white[0]; //RGB Greyscale!
			break;
		case BWMONITOR_GREEN: //Green?
			BWconversion_palette = &BWconversion_green[0]; //RGB Green monitor!
			break;
		case BWMONITOR_AMBER: //Brown?
			BWconversion_palette = &BWconversion_amber[0]; //RGB Amber monitor!
			break;
		default: //Unknown scheme?
			BWconversion_palette = &BWconversion_white[0]; //RGB Greyscale!
			break;
		}
	}
	return DAC_whatBWColor;
}

void VGA_initBWConversion()
{
	if (BWconversion_ready) return; //Abort when already ready!
	BWconversion_ready = 1; //We're ready after this!
	const float amber_red = ((float)0xB9 / (float)255); //Red part!
	const float amber_green = ((float)0x80 / (float)255); //Green part!
	INLINEREGISTER word a,b; //16-bit!
	INLINEREGISTER uint_32 n; //32-bit!
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

uint_32 color2bw(uint_32 color) //Convert color values to b/w values!
{
	INLINEREGISTER word a; //Our registers we use!
	a = GETR(color); //Load Red channel!
	a += GETG(color); //Load Green channel!
	a += GETB(color); //Load Blue channel!

	return BWconversion_palette[a]; //Convert using the current palette!
}

uint_32 leavecoloralone(uint_32 color) //Leave color values alone!
{
	return color; //Normal color mode!
}

BWconversion_handler currentcolorconversion = &leavecoloralone; //Color to B/W handler!

uint_32 GA_color2bw(uint_32 color) //Convert color values to b/w values!
{
	return currentcolorconversion(color);
}

byte DAC_whatBWMonitor = 0; //Default: color monitor!

byte DAC_Use_BWMonitor(byte use)
{
	if (use < 2)
	{
		DAC_whatBWMonitor = use; //Use?
		if (use) //Used?
		{
			currentcolorconversion = &color2bw; //Use B/W conversion!
		}
		else //Not used?
		{
			currentcolorconversion = &leavecoloralone; //Leave the color alone!
		}
	}
	return DAC_whatBWMonitor; //Give the data!
}

void DAC_updateEntry(VGA_Type *VGA, byte entry) //Update a DAC entry for rendering!
{
	VGA->precalcs.effectiveDAC[entry] = GA_color2bw(VGA->precalcs.DAC[entry]); //Set the B/W or color entry!
}

void DAC_updateEntries(VGA_Type *VGA)
{
	int i;
	for (i=0;i<0x100;i++) //Process all entries!
	{
		DAC_updateEntry(VGA,i); //Update this entry with current values!
		VGA->precalcs.effectiveMDADAC[i] = GA_color2bw(RGB(i,i,i)); //Update the MDA DAC!
	}
}
