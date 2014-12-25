#include "headers/hardware/vga.h" //VGA support!
#include "headers/hardware/vga_screen/vga_precalcs.h" //Precalculation typedefs etc.
#include "headers/hardware/vga_screen/vga_dac.h" //Our defs!
#include "headers/header_dosbox.h" //Screen modes from DOSBox!
#include "headers/support/log.h" //Logging support!

extern VGA_Type *ActiveVGA; //Active VGA!
extern VideoModeBlock *CurMode; //Current int10 video mode!
byte BWmonitor = 0; //Force black/white monitor instead of color graphics?

void VGA_DUMPDAC() //Dumps the full DAC!
{
	char filename[256];
	bzero(filename,sizeof(filename)); //Init
	sprintf(&filename[0],"DAC_%02X",CurMode->mode); //Generate log of this mode!
	dolog(filename,"*** START OF DUMP ***");
	int c;
	for (c=0;c<256;c++)
	{
		uint_32 DACVal = ActiveVGA->precalcs.DAC[c]; //The DAC value!
		if (DACVal!=RGB(0x00,0x00,0x00) && ((DACVal&0xFF000000)!=0)) //Not black and filled?
		{
			dolog(filename,"DAC %02X is %08X",c,ActiveVGA->precalcs.DAC[c]); //Updated!
		}
	}
	dolog(filename,"*** END OF DUMP ***");
}

static OPTINLINE uint_32 color2bw(uint_32 color) //Convert color values to b/w values!
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

static byte DAC_BWmonitor(VGA_Type *VGA, byte DACValue)
{
	return color2bw(VGA->precalcs.DAC[DACValue]); //Lookup!
}

static byte DAC_colorMonitor(VGA_Type *VGA,byte DACValue)
{
	return VGA->precalcs.DAC[DACValue]; //Lookup!
}

typedef byte (*DAC_monitor)(VGA_Type *VGA, byte DACValue); //Monitor handler!

OPTINLINE uint_32 VGA_DAC(VGA_Type *VGA, byte DACValue) //Originally: VGA_Type *VGA, word x
{
	DAC_monitor monitors[2] = {DAC_colorMonitor,DAC_BWmonitor}; //What kind of monitor?
	return monitors[BWmonitor](VGA,DACValue); //Do color mode or B/W mode!
}