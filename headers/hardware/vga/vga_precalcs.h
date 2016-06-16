#ifndef VGA_PRECALCS_H
#define VGA_PRECALCS_H

#include "headers/hardware/vga/vga.h" //VGA basics!

//Where were we updated (what values to update?)
//ALL update? (for init)
#define WHEREUPDATED_ALL 0x0000
//Section update (from section below), flag. This updates all register within the section!
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

//CGA horizontal/vertical timing
#define WHEREUPDATED_CGACRTCONTROLLER_HORIZONTAL 0xC000
#define WHEREUPDATED_CGACRTCONTROLLER_VERTICAL 0xD000
//CGA misc CRT registers!
#define WHEREUPDATED_CGACRTCONTROLLER 0xE000

//All index registers that can be updated!
#define INDEX_GRAPHICSCONTROLLER 0x1
#define INDEX_SEQUENCER 0x2
#define INDEX_CRTCONTROLLER 0x3
#define INDEX_ATTRIBUTECONTROLLER 0x4
#define INDEX_DACWRITE 0x5
#define INDEX_DACREAD 0x6

//Filter to get all above!
//The area where it was updated:
//The area/section to update?
#define WHEREUPDATED_AREA 0xF000
//The updated register:
#define WHEREUPDATED_REGISTER 0x0FFF

//Register/section/all has been updated?
#define SECTIONISUPDATED(whereupdated,section) ((whereupdated&WHEREUPDATED_AREA)==section)
#define SECTIONISUPDATEDFULL(whereupdated,section,fullupdated) (SECTIONISUPDATED(whereupdated,section)||fullupdated)

//Simple register updated?
#define REGISTERUPDATED(whereupdated,section,reg,fullupdated) ((whereupdated==(section|reg))||fullupdated)

//Section update entirely, this section only?
#define UPDATE_SECTION(val,section) (((val&WHEREUPDATED_ALL_SECTION)==WHEREUPDATED_ALL_SECTION) && (val==section))
#define UPDATE_SECTIONFULL(val,section,fullupdate) (UPDATE_SECTION(val,section)||fullupdate)

typedef struct //Contains the precalculated values!
{
	byte graphicsmode; //Are we a graphics mode?
	byte textmode; //Are we a text mode?
	
	word scanline; //Current scanline rendering after all extra effects!
	
	byte overscancolor; //Default overscan color!
	
	byte characterwidth; //Character width!
	byte characterheight; //Character height!

	uint_32 startaddress[2]; //Combination of start address high&low register for normal and top screen (reset) operations!
	
	//CRT Controller registers:
	//Horizontal timing information
	uint_32 horizontaldisplaystart;
	uint_32 horizontaldisplayend;
	uint_32 horizontalblankingstart;
	uint_32 horizontalblankingend;
	uint_32 horizontalretracestart;
	uint_32 horizontalretraceend;
	uint_32 horizontaltotal;
	
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
	
	byte characterclockshift; //Division 0,1 or 2 for the horizontal character clock!
	byte BWDModeShift; //Memory mode shift for the horizontal character clock in B/W/DW modes!

	//Extra information
	word rowsize;
	word topwindowstart;
	byte scandoubling;
	uint_32 scanlinesize; //Scanline size!
	//Sequencer_textmode_cursor (CRTC):
	uint_32 cursorlocation; //Cursor location!
	byte pixelshiftcount; //Save our precalculated value!
	byte presetrowscan; //Row scanning boost!
	byte colorselect54; //Precalculate!
	byte colorselect76; //Precalculate!
	uint_32 DAC[0x100]; //Full DAC saved lookup table!
	uint_32 effectiveDAC[0x100]; //The same DAC as above, but with color conversions applied for rendering!
	uint_32 effectiveMDADAC[0x100]; //The same DAC as above, but with b/w conversions applied for rendering, also it's index is changed to the R/G/B 256-color greyscale index!
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

	//Register data used during rendering and barely updated at all:
	byte AttributeModeControlRegister_ColorEnable8Bit;
	byte CursorStartRegister_CursorScanLineStart;
	byte CursorEndRegister_CursorScanLineEnd;
	byte CursorStartRegister_CursorDisable;
	byte GraphicsModeRegister_ShiftRegister;
	byte PresetRowScanRegister_BytePanning;
	byte AttributeModeControlRegister_PixelPanningMode;
	byte CRTCModeControlRegister_SLDIV; //Scanline divisor!
	byte ClockingModeRegister_DCR; //Dot Clock Rate!
	byte LastMiscOutputRegister; //Last value written to the Misc Output Register!
	byte LastCGAFlags; //Last used CGA flags!
	byte LastMDAFlags; //Last used MDA flags!
	byte graphicsmode_nibbled; //Allow nibbled reversal mask this must allow values 1&2 to be decreased, else 0 with text modes!
	uint_32 VRAMmask; //The mask used for accessing VRAM!
	uint_32 extrasignal; //Graphics mode display bit!
	byte AttributeController_16bitDAC; //Enable the 16-bit/8-bit DAC color formation in the Attribute Controller?
	byte VideoLoadRateMask; //When to load the new pixels (bitmask to result in zero to apply)!
	byte BypassPalette; //Bypass the palette?
	byte linearmode; //Linear mode enabled (linear memory window)? Bit 1=1: Use high 4 bits for bank, else bank select. Bit0=1: Use contiguous memory, else VGA mapping.
	byte graphicsReloadMask; //Graphics reload mask (results in zero to apply)
	byte DACmode; //The current DAC mode: Bits 0-1: 3=16-bit, 2=15-bit, 1/0: 8-bit(normal VGA DAC). Bit 4: 1=Latch every two pixel clocks, else every pixel clock.
} VGA_PRECALCS; //VGA pre-calculations!

typedef void (*VGA_calcprecalcsextensionhandler)(void *VGA, uint_32 whereupdated); //Calculate them!

void VGA_calcprecalcs(void *VGA, uint_32 whereupdated); //Calculate them!
void VGA_LOGCRTCSTATUS(); //Log the current CRTC precalcs status!
void dump_CRTCTiming(); //Dump the full CRTC timing calculated from the precalcs!

void VGA_calcprecalcs_CRTC(void *VGA); //Precalculate CRTC precalcs!

#endif