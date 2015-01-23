#include "headers/hardware/vga.h" //VGA support!
#include "headers/hardware/vga_screen/vga_precalcs.h" //Precalculation typedefs etc.
#include "headers/hardware/vga_screen/vga_dacrenderer.h" //Our defs!
#include "headers/header_dosbox.h" //Screen modes from DOSBox!
#include "headers/support/log.h" //Logging support!
#include "headers/support/bmp.h" //BMP support for dumping color information!

extern VGA_Type *ActiveVGA; //Active VGA!
extern VideoModeBlock *CurMode; //Current int10 video mode!
byte BWmonitor = 0; //Force black/white monitor instead of color graphics?

void VGA_DUMPDAC() //Dumps the full DAC!
{
	char filename[256];
	bzero(filename,sizeof(filename)); //Init
	sprintf(&filename[0],"DAC_%02X",CurMode->mode); //Generate log of this mode!
	int c;
	uint_32 DACBitmap[0x200]; //Full DAC 1-row bitmap!
	register uint_32 DACVal;
	for (c=0;c<0x100;c++)
	{
		DACVal = ActiveVGA->precalcs.DAC[c]; //The DAC value!
		if ((DACVal==RGB(0x00,0x00,0x00)) || (!(DACVal&0xFF000000))) //Black or unfilled?
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
	for (c=0;c<0x200;c++) //All possible attributes (font and back color)!
	{
		word lookup;
		lookup = (c>>1); //What attribute!
		lookup <<= 5; //Make room!
		//No charinner_y (fixed to row #0)!
		lookup <<= 1; //Make room!
		lookup |= 1; //Blink ON!
		lookup <<= 1; //Make room!
		lookup |= (c&1); //Font?
		//The lookup points to the index!
		register uint_32 DACVal;
		DACVal = ActiveVGA->precalcs.DAC[ActiveVGA->precalcs.attributeprecalcs[lookup]]; //The DAC value looked up!
		if ((DACVal==RGB(0x00,0x00,0x00)) || (!(DACVal&0xFF000000))) //Black or unfilled?
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
	writeBMP(filename,&DACBitmap[0],16,16,4,4,16); //Simple 1-row dump of the attributes through the DAC!
	
	/*char cs[256];
	memset(&cs,0,sizeof(cs));
	sprintf(&filename[0],"CHARX_%02X.DAT",CurMode->mode); //Generate log of this mode!
	
	FILE *f;
	f = fopen(filename,"wb");
	fwrite(&ActiveVGA->CRTC.charcolstatus,1,sizeof(ActiveVGA->CRTC.charcolstatus),f); //Write the column statuses!
	fclose(f); //Close the file!
	*/
}

OPTINLINE uint_32 color2bw(uint_32 color) //Convert color values to b/w values!
{
	word n = GETR(color); //Red channel!
	n += GETG(color); //Green channel!
	n += GETB(color); //Blue channel!
	
	//Optimized way of dividing?
	int a,b;
    a = n >> 2;
    b = (a >> 2);
    a += b;
    b = (b >> 2);
    a += b;
    b = (b >> 2);
    a += b;
    b = (b >> 2);
    a += b;
	return RGB(a,a,a); //RGB Greyscale!
}

uint_32 DAC_BWmonitor(VGA_Type *VGA, byte DACValue)
{
	return color2bw(VGA->precalcs.DAC[DACValue]); //Lookup!
}

uint_32 DAC_colorMonitor(VGA_Type *VGA,byte DACValue)
{
	return VGA->precalcs.DAC[DACValue]; //Lookup!
}

typedef uint_32 (*DAC_monitor)(VGA_Type *VGA, byte DACValue); //Monitor handler!

OPTINLINE uint_32 VGA_DAC(VGA_Type *VGA, byte DACValue) //Originally: VGA_Type *VGA, word x
{
	static const DAC_monitor monitors[2] = {DAC_colorMonitor,DAC_BWmonitor}; //What kind of monitor?
	return monitors[BWmonitor](VGA,DACValue); //Do color mode or B/W mode!
}