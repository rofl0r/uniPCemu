#ifndef VGA_PRECALCS_H
#define VGA_PRECALCS_H

#include "headers/hardware/vga.h" //VGA basics!

//Where were we updated (what values to update?)
//ALL update? (for init)
#define WHEREUPDATED_ALL 0x0000
//Section update (from section below), flag
#define WHEREUPDATED_ALL_SECTION 0x10000

//Section (used when not all). This is OR-ed with the data index!
#define WHEREUPDATED_GRAPHICSCONTROLLER 0x1000
#define WHEREUPDATED_SEQUENCER 0x2000
#define WHEREUPDATED_CRTCONTROLLER 0x3000
#define WHEREUPDATED_ATTRIBUTECONTROLLER 0x4000
#define WHEREUPDATED_DAC 0x5000

//Rest registers (used when not all)
#define WHEREUPDATED_MISCOUTPUTREGISTER 0x6000
#define WHEREUPDATED_FEATURECONTROLREGISTER 0x7000
#define WHEREUPDATED_INPUTSTATUS0REGISTER 0x8000
#define WHEREUPDATED_INPUTSTATUS1REGISTER 0x9000
#define WHEREUPDATED_DACMASKREGISTER 0xA000
#define WHEREUPDATED_INDEX 0xB000

//All index registers that can be updated!
#define INDEX_GRAPHICSCONTROLLER 0x1
#define INDEX_SEQUENCER 0x2
#define INDEX_CRTCONTROLLER 0x3
#define INDEX_ATTRIBUTECONTROLLER 0x4
#define INDEX_DACWRITE 0x5
#define INDEX_DACREAD 0x6

//Filter to get all above!
//The area where it was updated:
//Section update?
#define UPDATE_SECTION(val) ((val&WHEREUPDATED_ALL_SECTION)==WHEREUPDATED_ALL_SECTION)
//The area/section to update?
#define WHEREUPDATED_AREA 0xF000
//The updated register:
#define WHEREUPDATED_REGISTER 0x0FFF

typedef struct //Contains the precalculated values!
{
	byte graphicsmode; //Are we a graphics mode?
	
	word scanline; //Current scanline rendering after all extra effects!
	
	byte overscancolor; //Default overscan color!
	
	byte characterwidth; //Character width!
	byte characterheight; //Character height!

	word startaddress[2]; //Combination of start address high&low register for normal and top screen (reset) operations!
	
	//CRT Controller registers:
	//Horizontal timing information
	word horizontaldisplaystart;
	word horizontaldisplayend;
	word horizontalblankingstart;
	word horizontalblankingend;
	word horizontalretracestart;
	word horizontalretraceend;
	word horizontaltotal;
	
	//Vertical timing information
	word verticaldisplayend;
	word verticalblankingstart;
	word verticalblankingend;
	word verticalretracestart;
	word verticalretraceend;
	word verticaltotal;
	
	//Total resolution
	word xres;
	word yres;
	
	byte DotClockSpeed; //0 for 1:1 mapping of pixels, 1 for 1:2 mapping etc. (every pixel takes up 2 pixels on-screen), 3 for 1:4 mapping.
	byte characterclockshift; //Division 0,1 or 2 for the horizontal character clock!
	byte BWDModeShift; //Memory mode shift for the horizontal character clock in B/W/DW modes!

	//Extra information
	word rowsize;
	word topwindowstart;
	byte scandoubling;
	uint_32 scanlinesize; //Scanline size!
	//Sequencer_textmode_cursor (CRTC):
	word cursorlocation; //Cursor location!
	byte pixelshiftcount; //Save our precalculated value!
	byte presetrowscan; //Row scanning boost!
	byte colorselect54; //Precalculate!
	byte colorselect76; //Precalculate!
	uint_32 DAC[0x100]; //Full DAC saved lookup table!
	byte lastDACMask; //To determine if the DAC Mask is updated or not!
	
	byte renderedlines; //Actual ammount of lines rendered, graphics mode included!
	
	//Attribute controller precalcs!
	byte attributeprecalcs[0x8000]; //All attribute precalcs!

	//Rest!
	word clockselectrows; //Rows determined by clock select!
	word verticalcharacterclocks; //Ammount of vertical character clocks! (VerticalBlankingStart/(Max scanline-1)) OR 1)
	float scanlinepercentage; //The ammount of percentage each scanline represents related to the total ammount of scanlines.
	
	//Extra info for debugging!
	uint_32 mainupdate; //Main update counter for debugging updates to VRAMMode!
} VGA_PRECALCS; //VGA pre-calculations!

void VGA_calcprecalcs(void *VGA, uint_32 whereupdated); //Calculate them!
void VGA_LOGCRTCSTATUS(); //Log the current CRTC precalcs status!
void dump_CRTCTiming(); //Dump the full CRTC timing calculated from the precalcs!
#endif