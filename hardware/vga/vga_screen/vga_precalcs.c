#include "headers/hardware/vga.h" //VGA support (plus precalculation!)
#include "headers/hardware/vga_screen/vga_precalcs.h" //Precalculation typedefs etc.
#include "headers/hardware/vga_rest/colorconversion.h" //Color conversion for DAC precalculation!
#include "headers/emu/gpu/gpu.h" //Relative conversion!
#include "headers/hardware/vga_screen/vga_crtcontroller.h"
#include "headers/hardware/vga_screen/vga_attributecontroller.h" //Attribute controller support!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/vga_screen/vga_sequencer.h" //Sequencer render counter support!
//Works!
OPTINLINE uint_32 getcol256(VGA_Type *VGA, byte color) //Convert color to RGB!
{
	DACEntry colorEntry; //For getcol256!
	readDAC(VGA,(color&VGA->registers->DACMaskRegister),&colorEntry); //Read the DAC entry, masked on/off by the DAC Mask Register!
	return RGB(convertrel(colorEntry.r,0x3F,0xFF),convertrel(colorEntry.g,0x3F,0xFF),convertrel(colorEntry.b,0x3F,0xFF)); //Convert using DAC (Scale of DAC is RGB64, we use RGB256)!
}

//Register has been updated?
#define REGISTERUPDATED(whereupdated,controller,reg,fullupdated) ((whereupdated==(controller|reg))||fullupdated)
#define SECTIONUPDATEDFULL(whereupdated,section,fullupdated) (((whereupdated&WHEREUPDATED_AREA)==section)||fullupdated)
#define SECTIONUPDATED(whereupdated,section) ((whereupdated&WHEREUPDATED_AREA)==section)

extern byte VGA_LOGPRECALCS; //Are we manually updated to log?

OPTINLINE void VGA_calcprecalcs_CRTC(VGA_Type *VGA) //Precalculate CRTC precalcs!
{
	uint_32 current;
	byte charsize;
	uint_32 realtiming;
	//Column and row status for each pixel on-screen!
	charsize = getcharacterheight(VGA); //First, based on height!
	current = 0; //Init!
	for (;current<NUMITEMS(VGA->CRTC.rowstatus);) //All available resolutions!
	{
		VGA->CRTC.charrowstatus[current<<1] = current/charsize;
		VGA->CRTC.charrowstatus[(current<<1)|1] = current%charsize;
		VGA->CRTC.rowstatus[current] = get_display_y(VGA,current); //Translate!
		++current; //Next!
	}

	//Horizontal coordinates!
	charsize = getcharacterwidth(VGA); //Now, based on width!
	current = 0; //Init!
	for (;current<NUMITEMS(VGA->CRTC.colstatus);)
	{
		VGA->CRTC.charcolstatus[current<<1] = current/charsize;
		VGA->CRTC.charcolstatus[(current<<1)|1] = current%charsize;
		realtiming = current; //Same rate as the basic rate!
		realtiming >>= VGA->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.DCR; //Apply dot clock rate!
		VGA->CRTC.colstatus[current] = get_display_x(VGA,realtiming); //Translate to display rate!
		++current; //Next!
	}
	
	SEQ_DATA *Sequencer = GETSEQUENCER(VGA); //Our sequencer!
	
	//Clear our timing debugger information: we're invalid now!
	Sequencer->totalrenders = 0; //Clear total render counting!
	Sequencer->totalrendertime = 0; //Clear time passed
}

void dump_CRTCTiming()
{
	uint_32 i;
	char information[0x1000];
	memset(&information,0,sizeof(information)); //Init!
	for (i=0;i<NUMITEMS(getActiveVGA()->CRTC.rowstatus);i++)
	{
		sprintf(information,"Row #%i=",i); //Current row!
		word status;
		status = getActiveVGA()->CRTC.rowstatus[i]; //Read the status for the row!
		if (status&VGA_SIGNAL_VTOTAL)
		{
			sprintf(information,"%s+VTOTAL",information); //Add!
		}
		if (status&VGA_SIGNAL_VRETRACESTART)
		{
			sprintf(information,"%s+VRETRACESTART",information); //Add!
		}
		if (status&VGA_SIGNAL_VRETRACEEND)
		{
			sprintf(information,"%s+VRETRACEEND",information); //Add!
		}
		if (status&VGA_SIGNAL_VBLANKSTART)
		{
			sprintf(information,"%s+VBLANKSTART",information); //Add!
		}
		if (status&VGA_SIGNAL_VBLANKEND)
		{
			sprintf(information,"%s+VBLANKEND",information); //Add!
		}
		if (status&VGA_VACTIVEDISPLAY)
		{
			sprintf(information,"%s+VACTIVEDISPLAY",information); //Add!
		}
		if (status&VGA_OVERSCAN)
		{
			sprintf(information,"%s+OVERSCAN",information); //Add!
		}
		dolog("VGA","%s",information);
		if (status&VGA_SIGNAL_VTOTAL) break; //Total reached? Don't look any further!
	}

	for (i=0;i<NUMITEMS(getActiveVGA()->CRTC.colstatus);i++)
	{
		sprintf(information,"Col #%i=",i); //Current row!
		word status;
		status = getActiveVGA()->CRTC.colstatus[i]; //Read the status for the row!
		if (status&VGA_SIGNAL_HTOTAL)
		{
			sprintf(information,"%s+HTOTAL",information); //Add!
		}
		if (status&VGA_SIGNAL_HRETRACESTART)
		{
			sprintf(information,"%s+HRETRACESTART",information); //Add!
		}
		if (status&VGA_SIGNAL_HRETRACEEND)
		{
			sprintf(information,"%s+HRETRACEEND",information); //Add!
		}
		if (status&VGA_SIGNAL_HBLANKSTART)
		{
			sprintf(information,"%s+HBLANKSTART",information); //Add!
		}
		if (status&VGA_SIGNAL_HBLANKEND)
		{
			sprintf(information,"%s+HBLANKEND",information); //Add!
		}
		if (status&VGA_HACTIVEDISPLAY)
		{
			sprintf(information,"%s+HACTIVEDISPLAY",information); //Add!
		}
		if (status&VGA_OVERSCAN)
		{
			sprintf(information,"%s+OVERSCAN",information); //Add!
		}
		dolog("VGA","%s",information);
		if (status&VGA_SIGNAL_HTOTAL) return; //Total reached? Don't look any further!
	}
}

void VGA_LOGCRTCSTATUS()
{
	if (!getActiveVGA()) return; //No VGA available!
	//Log all register info:
	dolog("VGA","CRTC Info:");
	dolog("VGA","HDispStart:%i",getActiveVGA()->precalcs.horizontaldisplaystart); //Horizontal start
	dolog("VGA","HDispEnd:%i",getActiveVGA()->precalcs.horizontaldisplayend); //Horizontal End of display area!
	dolog("VGA","HBlankStart:%i",getActiveVGA()->precalcs.horizontalblankingstart); //When to start blanking horizontally!
	dolog("VGA","HBlankEnd:~%i",getActiveVGA()->precalcs.horizontalblankingend); //When to stop blanking horizontally after starting!
	dolog("VGA","HRetraceStart:%i",getActiveVGA()->precalcs.horizontalretracestart); //When to start vertical retrace!
	dolog("VGA","HRetraceEnd:~%i",getActiveVGA()->precalcs.horizontalretraceend); //When to stop vertical retrace.
	dolog("VGA","HTotal:%i",getActiveVGA()->precalcs.horizontaltotal); //Horizontal total (full resolution plus horizontal retrace)!
	dolog("VGA","VDispEnd:%i",getActiveVGA()->precalcs.verticaldisplayend); //Vertical Display End Register value!
	dolog("VGA","VBlankStart:%i",getActiveVGA()->precalcs.verticalblankingstart); //Vertical Blanking Start value!
	dolog("VGA","VBlankEnd:~%i",getActiveVGA()->precalcs.verticalblankingend); //Vertical Blanking End value!
	dolog("VGA","VRetraceStart:%i",getActiveVGA()->precalcs.verticalretracestart); //When to start vertical retrace!
	dolog("VGA","VRetraceEnd:~%i",getActiveVGA()->precalcs.verticalretraceend); //When to stop vertical retrace.
	dolog("VGA","VTotal:%i",getActiveVGA()->precalcs.verticaltotal); //Full resolution plus vertical retrace!
}

void VGA_calcprecalcs(void *useVGA, uint_32 whereupdated) //Calculate them, whereupdated: where were we updated?
{
	//All our flags for updating sections related!
	byte recalcScanline = 0, recalcAttr = 0, VerticalClocksUpdated = 0, updateCRTC = 0, charwidthupdated = 0, underlinelocationupdated = 0; //Default: don't update!
	
	VGA_Type *VGA = (VGA_Type *)useVGA; //The VGA!
	byte FullUpdate = (whereupdated==0); //Fully updated?
//Calculate the precalcs!
	//Sequencer_Textmode: we update this always!

	if ((whereupdated==(WHEREUPDATED_SEQUENCER|0x01)) || FullUpdate || !VGA->precalcs.characterwidth) //Sequencer register updated?
	{
		//dolog("VGA","VTotal before charwidth: %i",VGA->precalcs.verticaltotal);
		VGA->precalcs.characterwidth = VGA->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.DotMode8?8:9; //Character width!
		whereupdated = WHEREUPDATED_CRTCONTROLLER; //We affect the CRTController fully too with above!
		//dolog("VGA","VTotal after charwidth: %i",VGA->precalcs.verticaltotal); //Log it!
		charwidthupdated = 1; //The character width has been updated, so update the corresponding registers too!
	}
	
	if ((whereupdated==(WHEREUPDATED_GRAPHICSCONTROLLER|0x06)) || FullUpdate) //Misc graphics register?
	{
		VGA->precalcs.graphicsmode = VGA->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER.AlphaNumericModeDisable; //Update Graphics mode!
		//dolog("VGA","VTotal after gm: %i",VGA->precalcs.verticaltotal); //Log it!
		VerticalClocksUpdated = 1; //Update vertical clocks!
	}

	if (SECTIONUPDATED(whereupdated,WHEREUPDATED_CRTCONTROLLER) || FullUpdate || charwidthupdated) //(some) CRT Controller values need to be updated?
	{
		byte CRTUpdated = UPDATE_SECTION(whereupdated)||FullUpdate; //Fully updated?
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x9))) //We have been updated?
		{
			VGA->precalcs.characterheight = VGA->registers->CRTControllerRegisters.REGISTERS.MAXIMUMSCANLINEREGISTER.MaximumScanLine+1; //Character height!
			//dolog("VGA","VTotal after charheight: %i",VGA->precalcs.verticaltotal); //Log it!
		}

		byte CRTUpdatedCharwidth = CRTUpdated||charwidthupdated; //Character width has been updated, for following registers using those?

		byte overflowupdated = FullUpdate||(whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x7)); //Overflow register has been updated?
		
		//CRT Controller registers:
		byte hendstartupdated = 0;
		if (CRTUpdatedCharwidth || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x3))) //Updated?
		{
			word hstart;
			hstart = VGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALBLANKINGREGISTER.DisplayEnableSkew;
			hstart *= VGA->precalcs.characterwidth; //We're a character width!
			hendstartupdated = (VGA->precalcs.horizontaldisplaystart != hstart); //Update!
			VGA->precalcs.horizontaldisplaystart = hstart; //Load!
			//dolog("VGA","HStart updated: %i",hstart);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			recalcScanline |= hendstartupdated; //Update!
			updateCRTC |= hendstartupdated; //Update!
		}
		
		if (CRTUpdatedCharwidth || (whereupdated==WHEREUPDATED_CRTCONTROLLER)) //Updated?
		{
			word htotal;
			htotal = VGA->registers->CRTControllerRegisters.REGISTERS.HORIZONTALTOTALREGISTER;
			htotal += 5;
			htotal *= VGA->precalcs.characterwidth; //We're character units!
			//dolog("VGA","HTotal updated: %i",htotal);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			updateCRTC |= (VGA->precalcs.horizontaltotal != htotal); //Update!
			VGA->precalcs.horizontaltotal = htotal; //Load!
		}
		
		if (CRTUpdatedCharwidth || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x1))) //Updated?
		{
			word hdispend;
			hdispend = VGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALDISPLAYREGISTER;
			++hdispend; //Stop after this character!
			hdispend *= VGA->precalcs.characterwidth; //Original!
			//dolog("VGA","HDispEnd updated: %i",hdispend);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			hendstartupdated |= (VGA->precalcs.horizontaldisplayend != hdispend); //Update!
			updateCRTC |= (VGA->precalcs.horizontaldisplayend != hdispend); //Update!
			VGA->precalcs.horizontaldisplayend = hdispend; //Load!
		}
		
		if (CRTUpdatedCharwidth || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x2))) //Updated?
		{
			word hblankstart;
			hblankstart = VGA->registers->CRTControllerRegisters.REGISTERS.STARTHORIZONTALBLANKINGREGISTER;
			++hblankstart; //Start after this character!
			hblankstart *= VGA->precalcs.characterwidth;
			//dolog("VGA","HBlankStart updated: %i",hblankstart);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			updateCRTC |= (VGA->precalcs.horizontalblankingstart != hblankstart); //Update!
			VGA->precalcs.horizontalblankingstart = hblankstart; //Load!
		}

		if (CRTUpdatedCharwidth || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x3)) || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x5))) //Updated?
		{
			word hblankend;
			hblankend = VGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALRETRACEREGISTER.EHB5;
			hblankend <<= 5; //Move to bit 6!
			hblankend |= VGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALBLANKINGREGISTER.EndHorizontalBlanking;
			//dolog("VGA","HBlankEnd updated: %i",hblankend);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			updateCRTC |= (VGA->precalcs.horizontalblankingend != hblankend); //Update!
			VGA->precalcs.horizontalblankingend = hblankend; //Load!
		}
		
		if (CRTUpdatedCharwidth || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x4)))
		{
			word hretracestart;
			hretracestart = VGA->registers->CRTControllerRegisters.REGISTERS.STARTHORIZONTALRETRACEREGISTER;
			hretracestart *= VGA->precalcs.characterwidth; //We're character units!
			++hretracestart; //We start after this!
			//dolog("VGA","HRetStart updated: %i",hretracestart);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			updateCRTC |= (VGA->precalcs.horizontalretracestart != hretracestart); //Update!
			VGA->precalcs.horizontalretracestart = hretracestart; //Load!
		}
		
		if (CRTUpdatedCharwidth || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x5))) 
		{
			//dolog("VGA","HRetEnd updated: %i",VGA->precalcs.horizontalretraceend);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			updateCRTC |= (VGA->precalcs.horizontalretraceend != VGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALRETRACEREGISTER.EndHorizontalRetrace); //Update!
			VGA->precalcs.horizontalretraceend = VGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALRETRACEREGISTER.EndHorizontalRetrace; //Load!
		}
		
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x12)) || overflowupdated) //Updated?
		{
			word vdispend;
			vdispend = VGA->registers->CRTControllerRegisters.REGISTERS.OVERFLOWREGISTER.VerticalDisplayEnd9;
			vdispend <<= 1;
			vdispend |= VGA->registers->CRTControllerRegisters.REGISTERS.OVERFLOWREGISTER.VerticalDisplayEnd8;
			vdispend <<= 8;
			vdispend |= VGA->registers->CRTControllerRegisters.REGISTERS.VERTICALDISPLAYENDREGISTER;
			++vdispend; //Stop one scanline later: we're the final scanline!
			VGA->precalcs.yres = vdispend;
			//dolog("VGA","VDispEnd updated: %i",vdispend);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			updateCRTC |= (VGA->precalcs.verticaldisplayend != vdispend); //Update!
			VGA->precalcs.verticaldisplayend = vdispend;
		}
		
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x15)) || overflowupdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x9))) //Updated?
		{
			word vblankstart;
			vblankstart = VGA->registers->CRTControllerRegisters.REGISTERS.MAXIMUMSCANLINEREGISTER.StartVerticalBlanking9;
			vblankstart <<= 1;
			vblankstart |= VGA->registers->CRTControllerRegisters.REGISTERS.OVERFLOWREGISTER.StartVerticalBlanking8;
			vblankstart <<= 8;
			vblankstart |= VGA->registers->CRTControllerRegisters.REGISTERS.STARTVERTICALBLANKINGREGISTER;
			//dolog("VGA","VBlankStart updated: %i",vblankstart);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			updateCRTC |= (VGA->precalcs.verticalblankingstart != vblankstart); //Update!
			VGA->precalcs.verticalblankingstart = vblankstart;
		}
		
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x16)))
		{
			updateCRTC |= (VGA->precalcs.verticalblankingend != VGA->registers->CRTControllerRegisters.REGISTERS.ENDVERTICALBLANKINGREGISTER.EndVerticalBlanking); //Update!
			VGA->precalcs.verticalblankingend = VGA->registers->CRTControllerRegisters.REGISTERS.ENDVERTICALBLANKINGREGISTER.EndVerticalBlanking;
			//dolog("VGA","VBlankEnd updated: %i",VGA->precalcs.verticalblankingend);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
		}

		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x10)) || overflowupdated) //Updated?
		{
			word vretracestart;
			vretracestart = VGA->registers->CRTControllerRegisters.REGISTERS.OVERFLOWREGISTER.VerticalRetraceStart9;
			vretracestart <<= 1;
			vretracestart |= VGA->registers->CRTControllerRegisters.REGISTERS.OVERFLOWREGISTER.VerticalRetraceStart8;
			vretracestart <<= 8;
			vretracestart |= VGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACESTARTREGISTER;
			//dolog("VGA","VRetraceStart updated: %i",vretracestart);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			updateCRTC |= (VGA->precalcs.verticalretracestart != vretracestart); //Update!
			VGA->precalcs.verticalretracestart = vretracestart;
		}
		
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x6)) || overflowupdated) //Updated?
		{
			word vtotal;
			vtotal = VGA->registers->CRTControllerRegisters.REGISTERS.OVERFLOWREGISTER.VerticalTotal9;
			vtotal <<= 1;
			vtotal |= VGA->registers->CRTControllerRegisters.REGISTERS.OVERFLOWREGISTER.VerticalTotal8;
			vtotal <<= 8;
			vtotal |= VGA->registers->CRTControllerRegisters.REGISTERS.VERTICALTOTALREGISTER;
			++vtotal; //We end after the line specified, so specify the line to end at!
			//dolog("VGA","VTotal updated: %i",vtotal);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			VerticalClocksUpdated |= (VGA->precalcs.verticaltotal != vtotal);
			updateCRTC |= (VGA->precalcs.verticaltotal != vtotal); //Update!
			VGA->precalcs.verticaltotal = vtotal;
		}
		
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x11))) //Updated?
		{
			updateCRTC |= (VGA->precalcs.verticalretraceend != VGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.VerticalRetraceEnd); //Update!
			VGA->precalcs.verticalretraceend = VGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.VerticalRetraceEnd; //Load!
			//dolog("VGA","VRetraceEnd updated: %i",VGA->precalcs.verticalretraceend);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
		}
		
		if (CRTUpdated || hendstartupdated) //Updated?
		{
			word xres;
			xres = VGA->precalcs.horizontaldisplayend;
			xres -= VGA->precalcs.horizontaldisplaystart;
			++xres;
			VGA->precalcs.xres = xres;
			//dolog("VGA","VTotal after xres: %i",VGA->precalcs.verticaltotal); //Log it!
		}
		
		byte scanlinesizeupdated = 0; //We need to update the scan line size?
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x13))) //Updated?
		{
			word rowsize;
			rowsize = VGA->registers->CRTControllerRegisters.REGISTERS.OFFSETREGISTER;
			rowsize <<= 1;
			VGA->precalcs.rowsize = rowsize; //=Offset*2
			//dolog("VGA","VTotal after rowsize: %i",VGA->precalcs.verticaltotal); //Log it!
			scanlinesizeupdated = 1; //Updated!
		}
		
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x18))
			       || overflowupdated
			       || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x9))) //Updated?
		{
			word topwindowstart;
			topwindowstart = VGA->registers->CRTControllerRegisters.REGISTERS.MAXIMUMSCANLINEREGISTER.LineCompare9;
			topwindowstart <<= 1;
			topwindowstart |= VGA->registers->CRTControllerRegisters.REGISTERS.OVERFLOWREGISTER.LineCompare8;
			topwindowstart <<= 8;
			topwindowstart |= VGA->registers->CRTControllerRegisters.REGISTERS.LINECOMPAREREGISTER;
			VGA->precalcs.topwindowstart = topwindowstart;
			//dolog("VGA","VTotal after topwindowstart: %i",VGA->precalcs.verticaltotal); //Log it!
			recalcScanline = 1; //Recalc scanline data!
		}
		
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x14))
			       || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x17))) //Updated?
		{
			//This applies to the Frame buffer:
			byte BWDModeShift = 1; //Default: word mode!
			if (VGA->registers->CRTControllerRegisters.REGISTERS.UNDERLINELOCATIONREGISTER.DW)
			{
				BWDModeShift = 2; //Shift by 2!
			}
			else if (VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.UseByteMode)
			{
				BWDModeShift = 0; //Shift by 0! We're byte mode!
			}
			updateCRTC |= (VGA->precalcs.BWDModeShift != BWDModeShift); //Update the CRTC!
			VGA->precalcs.BWDModeShift = BWDModeShift;

			byte characterclockshift=0;
			//This applies to the address counter (renderer):
			if (VGA->registers->CRTControllerRegisters.REGISTERS.UNDERLINELOCATIONREGISTER.DIV4)
			{
				characterclockshift = 2; //Shift right 2 bits: divide by 4!
			}
			else if (VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.DIV2)
			{
				characterclockshift = 1; //Shift right 1 bit more on top of DIV4: divide by 2!
			}

			updateCRTC |= (VGA->precalcs.characterclockshift != characterclockshift); //Update the CRTC!
			VGA->precalcs.characterclockshift = characterclockshift; //Apply character clock shift!

			underlinelocationupdated = 1; //We need to update the attribute controller!
			scanlinesizeupdated = 1; //We need to update this too!
			//dolog("VGA","VTotal after VRAMMemAddrSize: %i",VGA->precalcs.verticaltotal); //Log it!
		}
		
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x9))) //Updated?
		{
			VGA->precalcs.scandoubling = VGA->registers->CRTControllerRegisters.REGISTERS.MAXIMUMSCANLINEREGISTER.ScanDoubling;
			//dolog("VGA","VTotal after SD: %i",VGA->precalcs.verticaltotal); //Log it!
		}
		
		if (CRTUpdated || scanlinesizeupdated) //Updated?
		{
			word scanlinesize;
			scanlinesize = VGA->precalcs.rowsize;
			scanlinesize <<= VGA->precalcs.BWDModeShift; //B/W/DWord mode shift!
			VGA->precalcs.scanlinesize = scanlinesize; //Scanline size!
			recalcScanline = 1; //Recalc scanline data!
			//dolog("VGA","VTotal after scanlinesize: %i",VGA->precalcs.verticaltotal); //Log it!
		}
		
		//Sequencer_textmode_cursor (CRTC):
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0xE))
			       || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0xF))
			       || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0xB))
				   
				   || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x14))
				   || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x17)) //Also update on B/W/DW mode changes!
				   ) //Updated?
		{
			word cursorlocation;
			cursorlocation = VGA->registers->CRTControllerRegisters.REGISTERS.CURSORLOCATIONHIGHREGISTER;
			cursorlocation <<= 8;
			cursorlocation |= VGA->registers->CRTControllerRegisters.REGISTERS.CURSORLOCATIONLOWREGISTER;
			cursorlocation += VGA->registers->CRTControllerRegisters.REGISTERS.CURSORENDREGISTER.CursorSkew;
			cursorlocation >>= VGA->precalcs.characterclockshift; //Apply VGA shift: the shift is the ammount to move at a time!
			cursorlocation <<= VGA->precalcs.BWDModeShift; //Apply byte/word/doubleword mode at the character level!

			VGA->precalcs.cursorlocation = cursorlocation; //Cursor location!
			//dolog("VGA","VTotal after cursorlocation: %i",VGA->precalcs.verticaltotal); //Log it!
		}

		if (CRTUpdated || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x8))) //Preset row scan updated?
		{
			VGA->precalcs.presetrowscan = VGA->registers->CRTControllerRegisters.REGISTERS.PRESETROWSCANREGISTER.PresetRowScan; //Apply new preset row scan!
		}
		
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0xC))
						|| (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0xD))) //Updated?
		{
			word startaddress;
			startaddress = VGA->registers->CRTControllerRegisters.REGISTERS.STARTADDRESSHIGHREGISTER;
			startaddress <<= 8;
			startaddress |= VGA->registers->CRTControllerRegisters.REGISTERS.STARTADDRESSLOWREGISTER;
			VGA->precalcs.startaddress[0] = startaddress; //Updated start address!
			recalcScanline = 1; //Recalc scanline data!
			//dolog("VGA","VTotal after startaddress: %i",VGA->precalcs.verticaltotal); //Log it!
		}
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x14))) //Underline location updated?
		{
			recalcAttr = 1; //Recalc attribute pixels!
		}

		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|VGA_CRTC_ATTRIBUTECONTROLLERTOGGLEREGISTER))) //Attribute controller toggle register updated?
		{
			recalcAttr = 1; //We've been updated: update the color logic!
		}
	}

	byte AttrUpdated = 0; //Fully updated?
	if (SECTIONUPDATED(whereupdated,WHEREUPDATED_ATTRIBUTECONTROLLER) || FullUpdate || underlinelocationupdated || (whereupdated==(WHEREUPDATED_INDEX|INDEX_ATTRIBUTECONTROLLER))) //Attribute Controller updated?
	{
		AttrUpdated = UPDATE_SECTION(whereupdated)||FullUpdate; //Fully updated?

		if (AttrUpdated || (whereupdated==(WHEREUPDATED_ATTRIBUTECONTROLLER|0x14)))
		{
			byte csel;
			
			csel = VGA->registers->AttributeControllerRegisters.REGISTERS.COLORSELECTREGISTER.ColorSelect54;
			csel <<= 4;
			VGA->precalcs.colorselect54 = csel; //Precalculate!
			
			csel = VGA->registers->AttributeControllerRegisters.REGISTERS.COLORSELECTREGISTER.ColorSelect76;
			csel <<= 6;
			VGA->precalcs.colorselect76 = csel; //Precalculate!
			//dolog("VGA","VTotal after colorselect: %i",VGA->precalcs.verticaltotal); //Log it!
			recalcAttr = 1; //We've been updated: update the color logic!
		}

		if (AttrUpdated || (whereupdated==(WHEREUPDATED_ATTRIBUTECONTROLLER|0x11))) //Overscan?
		{
			VGA->precalcs.overscancolor = VGA->registers->AttributeControllerRegisters.REGISTERS.OVERSCANCOLORREGISTER; //Update the overscan color!
			//dolog("VGA","VTotal after overscancolor: %i",VGA->precalcs.verticaltotal); //Log it!
		}
		
		if (AttrUpdated || (whereupdated==(WHEREUPDATED_ATTRIBUTECONTROLLER|0x13))
			|| (whereupdated==(WHEREUPDATED_ATTRIBUTECONTROLLER|0x10))
			|| charwidthupdated) //Updated?
		{
			//Precalculate horizontal pixel panning:
			byte pixelboost = 0; //Actual pixel boost!
			byte possibleboost; //Possible value!
			possibleboost = VGA->registers->AttributeControllerRegisters.REGISTERS.HORIZONTALPIXELPANNINGREGISTER.PixelShiftCount; //Possible value, to be determined!
			if (VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.ColorEnable8Bit) //8-bit colors?
			{
				if ((possibleboost%2)==0) //Enabled?
				{
					possibleboost = pixelboost;
					possibleboost >>= 1; //Bit 2 only!
					if (possibleboost<4) //Valid?
					{
						pixelboost = possibleboost; //Use this boost!
					}
				}
			}
			else //Determine by character width!
			{
				if (VGA->precalcs.characterwidth==9) //9 dot mode?
				{
					if (possibleboost<8) //1-8?
					{
						pixelboost = possibleboost;
						++pixelboost; //Enable with +1!
					} //Else 0!
				}
				else //8 dot mode?
				{
					if (possibleboost<8) //Enable?
					{
						pixelboost = possibleboost; //Enable normally!
					} //Else 0!
				}
			}
			//dolog("VGA","VTotal after pixelboost: %i",VGA->precalcs.verticaltotal); //Log it!
			recalcScanline |= (VGA->precalcs.pixelshiftcount!=pixelboost); //Recalc scanline data when needed!
			VGA->precalcs.pixelshiftcount = pixelboost; //Save our precalculated value!
		}
		
		//Simple attribute controller updates?

		if (AttrUpdated || (whereupdated==(WHEREUPDATED_ATTRIBUTECONTROLLER|0x10))) //Mode control register updated?
		{
			recalcAttr = 1; //We've been updated: update the color logic and pixels!
		}
		else if (whereupdated==(WHEREUPDATED_ATTRIBUTECONTROLLER|0x12)) //Color planes enable register?
		{
			recalcAttr = 1; //We've been updated: update the color logic!
		}
		else if (SECTIONUPDATED(whereupdated,WHEREUPDATED_ATTRIBUTECONTROLLER) && ((whereupdated&WHEREUPDATED_REGISTER)<0x10)) //Pallette updated?
		{
			recalcAttr = 1; //We've been updated: update the color logic!
		}
	}

	if (SECTIONUPDATED(whereupdated,WHEREUPDATED_DAC) || SECTIONUPDATED(whereupdated,WHEREUPDATED_DACMASKREGISTER) || FullUpdate) //DAC Updated?
	{
		if (UPDATE_SECTION(whereupdated) || (whereupdated==WHEREUPDATED_DACMASKREGISTER) || FullUpdate) //DAC Fully needs to be updated?
		{
			if (VGA->precalcs.lastDACMask!=VGA->registers->DACMaskRegister) //DAC Mask changed only?
			{
				int colorval;
				colorval = 0; //Init!
				for (;;) //Precalculate colors for DAC!
				{
					VGA->precalcs.DAC[colorval] = getcol256(VGA,colorval); //Translate directly through DAC for output!
					if (++colorval&0xFF00) break; //Overflow?
				}
				VGA->precalcs.lastDACMask = VGA->registers->DACMaskRegister; //Save the DAC mask for future checking if it's changed!
			}
		}
		else //Single register updated, no mask register updated?
		{
			VGA->precalcs.DAC[whereupdated&0xFF] = getcol256(VGA,whereupdated&0xFF); //Translate directly through DAC for output, single color only!
		}
		//dolog("VGA","VTotal after DAC: %i",VGA->precalcs.verticaltotal); //Log it!
	}

	if (VerticalClocksUpdated) //Ammount of vertical clocks have been updated?
	{
		//Character height / vertical character clocks!
		VGA->precalcs.clockselectrows = VGA->precalcs.verticalcharacterclocks = (VGA->precalcs.verticaltotal+1); //Use the same value!
		
		VGA->precalcs.scanlinepercentage = SAFEDIV(1.0f,VGA->precalcs.verticalcharacterclocks); //Re-calculate scanline percentage!
		if (VGA==getActiveVGA()) //Active VGA?
		{
			changeRowTimer(VGA,VGA->precalcs.clockselectrows); //Make sure the display scanline refresh rate is OK!		
		}
		recalcScanline = 1; //Recalc scanline data!
	}
	
	//Recalculate all our lookup tables when needed!
	if (recalcScanline) //Update scanline information?
	{
		VGA_Sequencer_calcScanlineData(VGA); //Recalculate all scanline data!
	}
	
	if (updateCRTC) //Update CRTC?
	{
		VGA_calcprecalcs_CRTC(VGA); //Update the CRTC timing data!
	}
	
	if (recalcAttr) //Update attribute controller?
	{
		VGA_AttributeController_calcAttributes(VGA); //Recalc pixel logic!	
	}
}