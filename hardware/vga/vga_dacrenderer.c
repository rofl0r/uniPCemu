#define VGA_DACRENDERER

#include "headers/types.h" //Basic types!
//Our signaling!
#include "headers/hardware/vga/vga.h" //VGA support!
#include "headers/hardware/vga/vga_precalcs.h" //Precalculation typedefs etc.
#include "headers/hardware/vga/vga_dacrenderer.h" //Our defs!
#include "headers/header_dosbox.h" //Screen modes from DOSBox!
#include "headers/support/log.h" //Logging support!
#include "headers/support/bmp.h" //BMP support for dumping color information!

extern VideoModeBlock *CurMode; //Current int10 video mode!

void VGA_DUMPDAC() //Dumps the full DAC!
{
	char filename[256];
	bzero(filename,sizeof(filename)); //Init
	sprintf(&filename[0],"DAC_%02X",CurMode->mode); //Generate log of this mode!
	int c;
	uint_32 DACBitmap[0x400]; //Full DAC 1-row bitmap!
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
	for (c=0;c<0x400;c++) //All possible attributes (font and back color)!
	{
		word lookup;
		lookup = (c>>1); //What attribute!
		lookup <<= 5; //Make room!
		//No charinner_y (fixed to row #0)!
		lookup <<= 1; //Make room!
		lookup |= ((c&0x200)>>9); //Blink OFF/ON!
		lookup <<= 1; //Make room!
		lookup |= (c&1); //Font?
		//The lookup points to the index!
		register uint_32 DACVal;
		DACVal = getActiveVGA()->precalcs.DAC[getActiveVGA()->precalcs.attributeprecalcs[lookup]]; //The DAC value looked up!
		if ((DACVal==(uint_32)RGB(0x00,0x00,0x00)) || (!(DACVal&0xFF000000))) //Black or unfilled?
		{
			DACBitmap[c] = 0; //Clear entry!
		}
		else
		{
			DACBitmap[c] = DACVal; //Load the DAC value!
		}		
	}
	//Attributes are in order: attribute foreground, attribute background for all attributes!
	filename[0] = 'A';
	filename[1] = 'T';
	filename[2] = 'T'; //Attribute controller translations!
	writeBMP(filename,&DACBitmap[0],16,32,4,4,16); //Simple 1-row dump of the attributes through the DAC!
	
	/*char cs[256];
	memset(&cs,0,sizeof(cs));
	sprintf(&filename[0],"CHARX_%02X.DAT",CurMode->mode); //Generate log of this mode!
	
	FILE *f;
	f = fopen(filename,"wb");
	fwrite(&getActiveVGA()->CRTC.charcolstatus,1,sizeof(getActiveVGA()->CRTC.charcolstatus),f); //Write the column statuses!
	fclose(f); //Close the file!
	*/
}

byte DAC_whatBWColor = 0; //Default: none!

byte DAC_BWColor(byte use) //What B/W color to use?
{
	if (use < 4) DAC_whatBWColor = use; //Use?
	return DAC_whatBWColor;
}

byte BWconversion_ready = 0; //Are we to initialise our tables?
word BWconversion[0x10000]; //Conversion table for b/w totals!
word BWconversion_brown[0x10000]; //Brown channel conversion!

void VGA_initBWConversion()
{
	if (BWconversion_ready) return; //Abort when already ready!
	BWconversion_ready = 1; //We're ready after this!
	const float brown_red = ((float)0xAA / (float)255); //Red part!
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
		BWconversion[n] = a;
		BWconversion_brown[n] = (byte)(((float)a)*brown_red); //Apply basic color: Create yellow tint (R/G)!
	}
}

OPTINLINE uint_32 color2bw(uint_32 color) //Convert color values to b/w values!
{
	register word a, b; //Our registers we use!
	a = GETR(color); //Load Red channel!
	a += GETG(color); //Load Green channel!
	a += GETB(color); //Load Blue channel!
	
	switch (DAC_whatBWColor) //What color scheme?
	{
	case BWMONITOR_BLACK: //Back/white?
		a = BWconversion[a]; //Translate using our lookup table into a 8-bit value!
		return RGB(a, a, a); //RGB Greyscale!
	case BWMONITOR_GREEN: //Green?
		a = BWconversion[a]; //Translate using our lookup table into a 8-bit value!
		return RGB(0, a, 0); //Green scheme!
	case BWMONITOR_BROWN: //Brown?
		b = a = BWconversion_brown[a]; //Translate using our lookup table into a 8-bit value for red&green!
		b >>= 1; //Green is halved to create brown
		return RGB(a, b, 0); //Brown scheme!
	default:
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