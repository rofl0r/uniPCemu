#include "headers/hardware/vga/vga.h" //VGA support (plus precalculation!)
#include "headers/hardware/vga/vga_precalcs.h" //Precalculation typedefs etc.
#include "headers/hardware/vga/colorconversion.h" //Color conversion for DAC precalculation!
#include "headers/emu/gpu/gpu.h" //Relative conversion!
#include "headers/hardware/vga/vga_crtcontroller.h"
#include "headers/hardware/vga/vga_attributecontroller.h" //Attribute controller support!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/vga/vga_sequencer.h" //Sequencer render counter support!
#include "headers/hardware/vga/vga_vramtext.h" //VRAM text support!
#include "headers/hardware/vga/vga_dacrenderer.h" //B/W detection support!

void VGA_updateVRAMmaps(VGA_Type *VGA); //VRAM map updater prototype!

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
	byte pixelrate=0;
	word extrastatus;
	for (;current<NUMITEMS(VGA->CRTC.colstatus);)
	{
		VGA->CRTC.charcolstatus[current<<1] = current/charsize;
		VGA->CRTC.charcolstatus[(current<<1)|1] = current%charsize;
		realtiming = current; //Same rate as the basic rate!
		realtiming >>= VGA->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.DCR; //Apply dot clock rate!
		VGA->CRTC.colstatus[current] = get_display_x(VGA,realtiming); //Translate to display rate!
		//Determine some extra information!
		extrastatus = 0; //Initialise extra horizontal status!
		
		if (VGA->registers->specialCGAflags&1) //Affect by 620x200/320x200 mode?
		{
			++pixelrate;
			if (VGA->registers->Compatibility_CGAModeControl&0x2) //Graphics mode?
			{
				if (VGA->registers->Compatibility_CGAModeControl&0x10) //640x200?
				{
					extrastatus |= 1; //Reset for the new block/next pixel!
				}
				else //320x200?
				{
					if (pixelrate>1) //Increase by 2!
					{
						extrastatus |= 1; //Reset for the new block/next pixel!					
						pixelrate = 0; //Reset every 2!
					}
				}
			}
			else //Text mode?
			{
				if (VGA->registers->Compatibility_CGAModeControl&0x1) //640x200?
				{
					extrastatus |= 1; //Reset for the new block/next pixel!
				}
				else //320x200?
				{
					if (pixelrate>1) //Increase by 2!
					{
						extrastatus |= 1; //Reset for the new block/next pixel!					
						pixelrate = 0; //Reset every 2!
					}
				}
			}
		}
		else //Normal VGA?
		{
			if (++pixelrate>VGA->precalcs.ClockingModeRegister_DCR) //To write back the pixel clock every or every other pixel(forced every clock in CGA mode)?
			{
				extrastatus |= 1; //Reset for the new block/next pixel!
				pixelrate = 0; //Reset!
			}
		}
		VGA->CRTC.extrahorizontalstatus[current] = extrastatus; //Extra status to apply!

		//Finished horizontal timing!
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
	lockVGA(); //We don't want to corrupt the renderer's data!
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
		if (status&VGA_SIGNAL_HTOTAL)
		{
			unlockVGA(); //We're finished with the VGA!
			return; //Total reached? Don't look any further!
		}
	}
	unlockVGA(); //We're finished with the VGA!
}

void VGA_LOGCRTCSTATUS()
{
	lockVGA(); //We don't want to corrupt the renderer's data!
	if (!getActiveVGA())
	{
		unlockVGA(); //We're finished with the VGA!
		return; //No VGA available!
	}
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
	unlockVGA(); //We're finished with the VGA!
}

void VGA_calcprecalcs(void *useVGA, uint_32 whereupdated) //Calculate them, whereupdated: where were we updated?
{
	//All our flags for updating sections related!
	byte recalcScanline = 0, recalcAttr = 0, VerticalClocksUpdated = 0, updateCRTC = 0, charwidthupdated = 0, underlinelocationupdated = 0; //Default: don't update!
	byte pattern; //The pattern to use!
	VGA_Type *VGA = (VGA_Type *)useVGA; //The VGA!
	byte FullUpdate = (whereupdated==0); //Fully updated?
//Calculate the precalcs!
	//Sequencer_Textmode: we update this always!
	byte CRTUpdated=0;
	byte CRTUpdatedCharwidth=0;
	byte overflowupdated=0;

	if ((whereupdated == (WHEREUPDATED_MISCOUTPUTREGISTER)) || FullUpdate) //Misc output register updated?
	{
		VGA_updateVRAMmaps(VGA); //Update the active VRAM maps!

		recalcAttr |= (VGA->precalcs.LastMiscOutputRegister^VGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.DATA)&1; //We've updated bit 1 of the misc output register? Then update monochrome vs color emulation mode!
		VGA->precalcs.LastMiscOutputRegister = VGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.DATA; //Save the last version of us!

		//Update our dipswitches according to the emulated monitor!
		//Dipswitch source: https://groups.google.com/d/msg/comp.sys.ibm.pc.classic/O-oivadTYck/kLe4xxf7wDIJ
		pattern = 0x6; //Pattern 0110: Enhanced Color - Enhanced Mode, 0110 according to Dosbox's VGA
		/*if (DAC_Use_BWMonitor(0xFF)) //Are we using a non-color monitor?
		{
			pattern |= 1; //Bit 1=Monochrome?, originally 0010 for Monochrome!
		}*/ //Not working correctly yet, so disable this!

		//Set the dipswitches themselves!
		VGA->registers->switches = pattern; //Set the pattern to use!
	}

	if ((whereupdated==(WHEREUPDATED_SEQUENCER|0x01)) || FullUpdate || !VGA->precalcs.characterwidth) //Sequencer register updated?
	{
		//lockVGA(); //We don't want to corrupt the renderer's data!
		//dolog("VGA","VTotal before charwidth: %i",VGA->precalcs.verticaltotal);
		//CGA forces character width to 8 wide!
		if (VGA->precalcs.characterwidth != (VGA->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.DotMode8&((~VGA->registers->specialCGAflags)&1))?8:9) adjustVGASpeed(); //Auto-adjust our VGA speed!
		VGA->precalcs.characterwidth = (VGA->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.DotMode8&((~VGA->registers->specialCGAflags)&1))?8:9; //Character width!
		if (VGA->precalcs.ClockingModeRegister_DCR != VGA->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.DCR) adjustVGASpeed(); //Auto-adjust our VGA speed!
		VGA->precalcs.ClockingModeRegister_DCR = VGA->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.DCR; //Dot Clock Rate!
		updateCRTC = 1; //We need to update the CRTC!
		whereupdated = WHEREUPDATED_CRTCONTROLLER; //We affect the CRTController fully too with above!
		//dolog("VGA","VTotal after charwidth: %i",VGA->precalcs.verticaltotal); //Log it!
		//unlockVGA(); //We're finished with the VGA!
		charwidthupdated = 1; //The character width has been updated, so update the corresponding registers too!
	}

	if ((whereupdated==(WHEREUPDATED_SEQUENCER|0x03)) || (whereupdated==(WHEREUPDATED_SEQUENCER|0x04)) || FullUpdate) //Sequencer character map register updated?
	{
		VGA_charsetupdated(VGA); //The character sets have been updated! Apply all changes to the active characters!
	}
	
	if (FullUpdate || (whereupdated == (WHEREUPDATED_GRAPHICSCONTROLLER | 0x5))) //Graphics mode register?
	{
		//lockVGA(); //We don't want to corrupt the renderer's data!
		if (VGA->precalcs.GraphicsModeRegister_ShiftRegister!=VGA->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.ShiftRegister) adjustVGASpeed(); //Auto-adjust our VGA speed!
		VGA->precalcs.GraphicsModeRegister_ShiftRegister = VGA->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.ShiftRegister; //Update shift mode!
		//unlockVGA(); //We're finished with the VGA!
	}

	if ((whereupdated==(WHEREUPDATED_GRAPHICSCONTROLLER|0x06)) || FullUpdate) //Misc graphics register?
	{
		//lockVGA(); //We don't want to corrupt the renderer's data!
		if (VGA->precalcs.graphicsmode != VGA->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER.AlphaNumericModeDisable) adjustVGASpeed(); //Auto-adjust VGA speed!
		VGA->precalcs.graphicsmode = VGA->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER.AlphaNumericModeDisable; //Update Graphics mode!
		VGA_updateVRAMmaps(VGA); //Update the active VRAM maps!
		//dolog("VGA","VTotal after gm: %i",VGA->precalcs.verticaltotal); //Log it!
		//unlockVGA(); //We're finished with the VGA!
		VerticalClocksUpdated = 1; //Update vertical clocks!
		adjustVGASpeed(); //Auto-adjust our VGA speed!
	}

	if (SECTIONUPDATED(whereupdated,WHEREUPDATED_CGACRTCONTROLLER_HORIZONTAL)) //CGA horizontal timing updated?
	{
		updateCRTC = 1; //Update the CRTC!
		adjustVGASpeed(); //Auto-adjust our VGA speed!
	}

	if (SECTIONUPDATED(whereupdated,WHEREUPDATED_CGACRTCONTROLLER_VERTICAL)) //CGA vertical timing updated?
	{
		updateCRTC = 1; //Update the CRTC!
		if (whereupdated==(WHEREUPDATED_CGACRTCONTROLLER_VERTICAL|0x9)) //Character height updated?
		{
			VGA->registers->CRTControllerRegisters.REGISTERS.MAXIMUMSCANLINEREGISTER.MaximumScanLine = (VGA->registers->CGARegisters[9]&0x1F); //Character height is set!
			adjustVGASpeed(); //Auto-adjust our VGA speed!
			goto updatecharheight;
		}
		adjustVGASpeed(); //Auto-adjust our VGA speed!
	}
	
	if (SECTIONUPDATED(whereupdated,WHEREUPDATED_CGACRTCONTROLLER)) //CGA CRT misc. stuff updated?
	{
		//Don't handle these registers just yet!
		if (whereupdated==(WHEREUPDATED_CGACRTCONTROLLER|0xA)) //Cursor Start Register updated?
		{
			getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.CURSORSTARTREGISTER.CursorScanLineStart = (getActiveVGA()->registers->CGARegisters[0xA]&0x1F); //Cursor scanline start!
			getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.CURSORSTARTREGISTER.CursorDisable = ((~getActiveVGA()->registers->CGARegisters[0xA])&0x40)>>6; //Disable the cursor?
			goto updateCursorStart; //Update us!
		}
		if (whereupdated==(WHEREUPDATED_CGACRTCONTROLLER|0xA)) //Cursor Start Register updated?
		{
			getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.CURSORENDREGISTER.CursorScanLineEnd = (getActiveVGA()->registers->CGARegisters[0xB]&0x1F); //Cursor scanline end!
			goto updateCursorEnd; //Update us!
		}

		if (whereupdated==(WHEREUPDATED_CGACRTCONTROLLER|0xC)) //Start address High register updated?
		{
			getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.STARTADDRESSHIGHREGISTER = (getActiveVGA()->registers->CGARegisters[0xC]&0x1F); //Apply the start address high register!
			goto updateStartAddress;
		}

		if (whereupdated==(WHEREUPDATED_CGACRTCONTROLLER|0xD)) //Start address Low register updated?
		{
			getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.STARTADDRESSLOWREGISTER = getActiveVGA()->registers->CGARegisters[0xD]; //Apply the start address low register!
			goto updateStartAddress;
		}

		if (whereupdated==(WHEREUPDATED_CGACRTCONTROLLER|0xE)) //Start address High register updated?
		{
			getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.CURSORLOCATIONHIGHREGISTER = (getActiveVGA()->registers->CGARegisters[0xE]&0x1F); //Apply the start address high register!
			goto updateCursorLocation;
		}

		if (whereupdated==(WHEREUPDATED_CGACRTCONTROLLER|0xF)) //Start address Low register updated?
		{
			getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.CURSORLOCATIONLOWREGISTER = getActiveVGA()->registers->CGARegisters[0xF]; //Apply the start address low register!
			goto updateCursorLocation;
		}		

		adjustVGASpeed(); //Auto-adjust our VGA speed!
	}

	if (SECTIONUPDATED(whereupdated,WHEREUPDATED_CRTCONTROLLER) || FullUpdate || charwidthupdated) //(some) CRT Controller values need to be updated?
	{
		CRTUpdated = UPDATE_SECTION(whereupdated)||FullUpdate; //Fully updated?
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x9))) //We have been updated?
		{
			updatecharheight:
			//lockVGA(); //We don't want to corrupt the renderer's data!
			if (VGA->precalcs.characterheight != VGA->registers->CRTControllerRegisters.REGISTERS.MAXIMUMSCANLINEREGISTER.MaximumScanLine+1) adjustVGASpeed(); //Auto-adjust our VGA speed!
			VGA->precalcs.characterheight = VGA->registers->CRTControllerRegisters.REGISTERS.MAXIMUMSCANLINEREGISTER.MaximumScanLine+1; //Character height!
			//dolog("VGA","VTotal after charheight: %i",VGA->precalcs.verticaltotal); //Log it!
			//unlockVGA(); //We're finished with the VGA!
		}

		CRTUpdatedCharwidth = CRTUpdated||charwidthupdated; //Character width has been updated, for following registers using those?
		overflowupdated = FullUpdate||(whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x7)); //Overflow register has been updated?
		
		if (CRTUpdated || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x8))) //Preset row scan?
		{
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.PresetRowScanRegister_BytePanning = VGA->registers->CRTControllerRegisters.REGISTERS.PRESETROWSCANREGISTER.BytePanning; //Update byte panning!
			//unlockVGA(); //We're finished with the VGA!
		}

		if (CRTUpdated || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0xA))) //Cursor start register?
		{
			updateCursorStart:
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.CursorStartRegister_CursorScanLineStart = VGA->registers->CRTControllerRegisters.REGISTERS.CURSORSTARTREGISTER.CursorScanLineStart; //Update!
			if (VGA->precalcs.CursorStartRegister_CursorDisable != VGA->registers->CRTControllerRegisters.REGISTERS.CURSORSTARTREGISTER.CursorDisable) adjustVGASpeed(); //Changed speed!
			VGA->precalcs.CursorStartRegister_CursorDisable = VGA->registers->CRTControllerRegisters.REGISTERS.CURSORSTARTREGISTER.CursorDisable; //Update!
			//unlockVGA(); //We're finished with the VGA!
		}

		if (CRTUpdated || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0xB))) //Cursor end register?
		{
			updateCursorEnd:
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.CursorEndRegister_CursorScanLineEnd = VGA->registers->CRTControllerRegisters.REGISTERS.CURSORENDREGISTER.CursorScanLineEnd; //Update!
			//unlockVGA(); //We're finished with the VGA!
		}

		//CRT Controller registers:
		byte hendstartupdated = 0;
		if (CRTUpdatedCharwidth || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x3))) //Updated?
		{
			word hstart;
			//lockVGA(); //We don't want to corrupt the renderer's data!
			hstart = VGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALBLANKINGREGISTER.DisplayEnableSkew;
			hstart *= VGA->precalcs.characterwidth; //We're a character width!
			hendstartupdated = (VGA->precalcs.horizontaldisplaystart != hstart); //Update!
			if (VGA->precalcs.horizontaldisplaystart != hstart) adjustVGASpeed(); //Auto-adjust our speed!
			VGA->precalcs.horizontaldisplaystart = hstart; //Load!
			//dolog("VGA","HStart updated: %i",hstart);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			//unlockVGA(); //We're finished with the VGA!
			recalcScanline |= hendstartupdated; //Update!
			updateCRTC |= hendstartupdated; //Update!
		}
		
		if (CRTUpdatedCharwidth || (whereupdated==WHEREUPDATED_CRTCONTROLLER)) //Updated?
		{
			word htotal;
			//lockVGA(); //We don't want to corrupt the renderer's data!
			htotal = VGA->registers->CRTControllerRegisters.REGISTERS.HORIZONTALTOTALREGISTER;
			htotal += 5;
			htotal *= VGA->precalcs.characterwidth; //We're character units!
			//dolog("VGA","HTotal updated: %i",htotal);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			if (htotal!=VGA->precalcs.horizontaltotal) adjustVGASpeed(); //Update our speed!
			updateCRTC |= (VGA->precalcs.horizontaltotal != htotal); //Update!
			VGA->precalcs.horizontaltotal = htotal; //Load!
			//unlockVGA(); //We're finished with the VGA!
		}
		
		if (CRTUpdatedCharwidth || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x1))) //Updated?
		{
			word hdispend;
			//lockVGA(); //We don't want to corrupt the renderer's data!
			hdispend = VGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALDISPLAYREGISTER;
			++hdispend; //Stop after this character!
			hdispend *= VGA->precalcs.characterwidth; //Original!
			//dolog("VGA","HDispEnd updated: %i",hdispend);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			if (VGA->precalcs.horizontaldisplayend != hdispend) adjustVGASpeed(); //Update our speed!
			hendstartupdated |= (VGA->precalcs.horizontaldisplayend != hdispend); //Update!
			updateCRTC |= (VGA->precalcs.horizontaldisplayend != hdispend); //Update!
			VGA->precalcs.horizontaldisplayend = hdispend; //Load!
			//unlockVGA(); //We're finished with the VGA!
		}
		
		if (CRTUpdatedCharwidth || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x2))) //Updated?
		{
			word hblankstart;
			//lockVGA(); //We don't want to corrupt the renderer's data!
			hblankstart = VGA->registers->CRTControllerRegisters.REGISTERS.STARTHORIZONTALBLANKINGREGISTER;
			++hblankstart; //Start after this character!
			hblankstart *= VGA->precalcs.characterwidth;
			//dolog("VGA","HBlankStart updated: %i",hblankstart);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			if (VGA->precalcs.horizontalblankingstart != hblankstart) adjustVGASpeed(); //Update our speed!
			updateCRTC |= (VGA->precalcs.horizontalblankingstart != hblankstart); //Update!
			VGA->precalcs.horizontalblankingstart = hblankstart; //Load!
			//unlockVGA(); //We're finished with the VGA!
		}

		if (CRTUpdatedCharwidth || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x3)) || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x5))) //Updated?
		{
			word hblankend;
			//lockVGA(); //We don't want to corrupt the renderer's data!
			hblankend = VGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALRETRACEREGISTER.EHB5;
			hblankend <<= 5; //Move to bit 6!
			hblankend |= VGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALBLANKINGREGISTER.EndHorizontalBlanking;
			//dolog("VGA","HBlankEnd updated: %i",hblankend);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			if (VGA->precalcs.horizontalblankingend != hblankend) adjustVGASpeed(); //Update our speed!
			updateCRTC |= (VGA->precalcs.horizontalblankingend != hblankend); //Update!
			VGA->precalcs.horizontalblankingend = hblankend; //Load!
			//unlockVGA(); //We're finished with the VGA!
		}
		
		if (CRTUpdatedCharwidth || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x4)))
		{
			word hretracestart;
			//lockVGA(); //We don't want to corrupt the renderer's data!
			hretracestart = VGA->registers->CRTControllerRegisters.REGISTERS.STARTHORIZONTALRETRACEREGISTER;
			hretracestart *= VGA->precalcs.characterwidth; //We're character units!
			++hretracestart; //We start after this!
			//dolog("VGA","HRetStart updated: %i",hretracestart);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			if (VGA->precalcs.horizontalretracestart != hretracestart) adjustVGASpeed(); //Update our speed!
			updateCRTC |= (VGA->precalcs.horizontalretracestart != hretracestart); //Update!
			VGA->precalcs.horizontalretracestart = hretracestart; //Load!
			//unlockVGA(); //We're finished with the VGA!
		}
		
		if (CRTUpdatedCharwidth || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x5))) 
		{
			//dolog("VGA","HRetEnd updated: %i",VGA->precalcs.horizontalretraceend);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			//lockVGA(); //We don't want to corrupt the renderer's data!
			if (VGA->precalcs.horizontalretraceend != VGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALRETRACEREGISTER.EndHorizontalRetrace) adjustVGASpeed();
			updateCRTC |= (VGA->precalcs.horizontalretraceend != VGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALRETRACEREGISTER.EndHorizontalRetrace); //Update!
			VGA->precalcs.horizontalretraceend = VGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALRETRACEREGISTER.EndHorizontalRetrace; //Load!
			//unlockVGA(); //We're finished with the VGA!
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
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.yres = vdispend;
			//dolog("VGA","VDispEnd updated: %i",vdispend);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			if (VGA->precalcs.verticaldisplayend != vdispend) adjustVGASpeed(); //Update our speed?
			updateCRTC |= (VGA->precalcs.verticaldisplayend != vdispend); //Update!
			VGA->precalcs.verticaldisplayend = vdispend;
			//unlockVGA(); //We're finished with the VGA!
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
			//lockVGA(); //We don't want to corrupt the renderer's data!
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			if (VGA->precalcs.verticalblankingstart != vblankstart) adjustVGASpeed(); //Update our speed?
			updateCRTC |= (VGA->precalcs.verticalblankingstart != vblankstart); //Update!
			VGA->precalcs.verticalblankingstart = vblankstart;
			//unlockVGA(); //We're finished with the VGA!
		}
		
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x16)))
		{
			if (VGA->precalcs.verticalblankingend != VGA->registers->CRTControllerRegisters.REGISTERS.ENDVERTICALBLANKINGREGISTER.EndVerticalBlanking) adjustVGASpeed(); //Update our speed?
			updateCRTC |= (VGA->precalcs.verticalblankingend != VGA->registers->CRTControllerRegisters.REGISTERS.ENDVERTICALBLANKINGREGISTER.EndVerticalBlanking); //Update!
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.verticalblankingend = VGA->registers->CRTControllerRegisters.REGISTERS.ENDVERTICALBLANKINGREGISTER.EndVerticalBlanking;
			//dolog("VGA","VBlankEnd updated: %i",VGA->precalcs.verticalblankingend);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			//unlockVGA(); //We're finished with the VGA!
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
			//lockVGA(); //We don't want to corrupt the renderer's data!
			if (VGA->precalcs.verticalretracestart != vretracestart) adjustVGASpeed(); //Update our speed?
			updateCRTC |= (VGA->precalcs.verticalretracestart != vretracestart); //Update!
			VGA->precalcs.verticalretracestart = vretracestart;
			//unlockVGA(); //We're finished with the VGA!
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
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VerticalClocksUpdated |= (VGA->precalcs.verticaltotal != vtotal);
			if (VGA->precalcs.verticaltotal != vtotal) adjustVGASpeed(); //Update our speed?
			updateCRTC |= (VGA->precalcs.verticaltotal != vtotal); //Update!
			VGA->precalcs.verticaltotal = vtotal;
			//unlockVGA(); //We're finished with the VGA!
		}
		
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x11))) //Updated?
		{
			if (VGA->precalcs.verticalretraceend != VGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.VerticalRetraceEnd) adjustVGASpeed(); //Update our speed?
			updateCRTC |= (VGA->precalcs.verticalretraceend != VGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.VerticalRetraceEnd); //Update!
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.verticalretraceend = VGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.VerticalRetraceEnd; //Load!
			//dolog("VGA","VRetraceEnd updated: %i",VGA->precalcs.verticalretraceend);
			//dolog("VGA","VTotal after: %i",VGA->precalcs.verticaltotal); //Log it!
			//unlockVGA(); //We're finished with the VGA!
		}
		
		if (CRTUpdated || hendstartupdated) //Updated?
		{
			word xres;
			//lockVGA(); //We don't want to corrupt the renderer's data!
			xres = VGA->precalcs.horizontaldisplayend;
			xres -= VGA->precalcs.horizontaldisplaystart;
			++xres;
			VGA->precalcs.xres = xres;
			//unlockVGA(); //We're finished with the VGA!
			//dolog("VGA","VTotal after xres: %i",VGA->precalcs.verticaltotal); //Log it!
		}
		
		byte scanlinesizeupdated = 0; //We need to update the scan line size?
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x13))) //Updated?
		{
			word rowsize;
			if ((VGA->registers->specialCGAflags&0xD)==0xD) //Ignore the row size?
			{
				rowsize = 0; //Ignore us!
			}
			else //Apply normally?
			{
				rowsize = VGA->registers->CRTControllerRegisters.REGISTERS.OFFSETREGISTER;
			}
			rowsize <<= 1;
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.rowsize = rowsize; //=Offset*2
			//dolog("VGA","VTotal after rowsize: %i",VGA->precalcs.verticaltotal); //Log it!
			//unlockVGA(); //We're finished with the VGA!
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
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.topwindowstart = topwindowstart;
			//dolog("VGA","VTotal after topwindowstart: %i",VGA->precalcs.verticaltotal); //Log it!
			//unlockVGA(); //We're finished with the VGA!
			recalcScanline = 1; //Recalc scanline data!
		}

		if (CRTUpdated || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x17))) //Mode control updated?
		{
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.CRTCModeControlRegister_SLDIV = VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.SLDIV; //Update!
			//unlockVGA(); //We're finished with the VGA!
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

			byte characterclockshift = 0;
			//This applies to the address counter (renderer):
			if (VGA->registers->CRTControllerRegisters.REGISTERS.UNDERLINELOCATIONREGISTER.DIV4)
			{
				characterclockshift = 2; //Shift right 2 bits: divide by 4!
			}
			else if (VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.DIV2)
			{
				characterclockshift = 1; //Shift right 1 bit more on top of DIV4: divide by 2!
			}

			//lockVGA(); //We don't want to corrupt the renderer's data!
			updateCRTC |= (VGA->precalcs.BWDModeShift != BWDModeShift); //Update the CRTC!
			VGA->precalcs.BWDModeShift = BWDModeShift;

			updateCRTC |= (VGA->precalcs.characterclockshift != characterclockshift); //Update the CRTC!
			VGA->precalcs.characterclockshift = characterclockshift; //Apply character clock shift!
			//unlockVGA(); //We're finished with the VGA!

			underlinelocationupdated = 1; //We need to update the attribute controller!
			scanlinesizeupdated = 1; //We need to update this too!
			//dolog("VGA","VTotal after VRAMMemAddrSize: %i",VGA->precalcs.verticaltotal); //Log it!
		}
		
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x9))) //Updated?
		{
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.scandoubling = VGA->registers->CRTControllerRegisters.REGISTERS.MAXIMUMSCANLINEREGISTER.ScanDoubling & (~(((VGA->registers->specialCGAflags&2)>>1)&(VGA->registers->specialCGAflags&1))); //Scan doubling enabled? CGA disables scanline doubling for compatibility.
			//dolog("VGA","VTotal after SD: %i",VGA->precalcs.verticaltotal); //Log it!
			//unlockVGA(); //We're finished with the VGA!
		}
		
		if (CRTUpdated || scanlinesizeupdated) //Updated?
		{
			word scanlinesize;
			//lockVGA(); //We don't want to corrupt the renderer's data!
			scanlinesize = VGA->precalcs.rowsize;
			scanlinesize <<= VGA->precalcs.BWDModeShift; //B/W/DWord mode shift!
			VGA->precalcs.scanlinesize = scanlinesize; //Scanline size!
			//unlockVGA(); //We're finished with the VGA!
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
			updateCursorLocation:
			cursorlocation = VGA->registers->CRTControllerRegisters.REGISTERS.CURSORLOCATIONHIGHREGISTER;
			cursorlocation <<= 8;
			cursorlocation |= VGA->registers->CRTControllerRegisters.REGISTERS.CURSORLOCATIONLOWREGISTER;
			cursorlocation += VGA->registers->CRTControllerRegisters.REGISTERS.CURSORENDREGISTER.CursorSkew;
			//lockVGA(); //We don't want to corrupt the renderer's data!
			cursorlocation >>= VGA->precalcs.characterclockshift; //Apply VGA shift: the shift is the ammount to move at a time!
			cursorlocation <<= VGA->precalcs.BWDModeShift; //Apply byte/word/doubleword mode at the character level!

			VGA->precalcs.cursorlocation = cursorlocation; //Cursor location!
			//unlockVGA(); //We're finished with the VGA!
			//dolog("VGA","VTotal after cursorlocation: %i",VGA->precalcs.verticaltotal); //Log it!
		}

		if (CRTUpdated || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x8))) //Preset row scan updated?
		{
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.presetrowscan = VGA->registers->CRTControllerRegisters.REGISTERS.PRESETROWSCANREGISTER.PresetRowScan; //Apply new preset row scan!
			//unlockVGA(); //We're finished with the VGA!
		}
		
		if (CRTUpdated || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0xC))
						|| (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0xD))) //Updated?
		{
			word startaddress;
			updateStartAddress:
			startaddress = VGA->registers->CRTControllerRegisters.REGISTERS.STARTADDRESSHIGHREGISTER;
			startaddress <<= 8;
			startaddress |= VGA->registers->CRTControllerRegisters.REGISTERS.STARTADDRESSLOWREGISTER;
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.startaddress[0] = startaddress; //Updated start address!
			//unlockVGA(); //We're finished with the VGA!
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
			byte csel,csel2;
			
			csel = VGA->registers->AttributeControllerRegisters.REGISTERS.COLORSELECTREGISTER.ColorSelect54;
			csel <<= 4;
			
			csel2 = VGA->registers->AttributeControllerRegisters.REGISTERS.COLORSELECTREGISTER.ColorSelect76;
			csel2 <<= 6;

			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.colorselect54 = csel; //Precalculate!
			VGA->precalcs.colorselect76 = csel2; //Precalculate!
			//unlockVGA(); //We're finished with the VGA!

			//dolog("VGA","VTotal after colorselect: %i",VGA->precalcs.verticaltotal); //Log it!
			recalcAttr = 1; //We've been updated: update the color logic!
		}

		if (AttrUpdated || (whereupdated==(WHEREUPDATED_ATTRIBUTECONTROLLER|0x11))) //Overscan?
		{
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.overscancolor = VGA->registers->AttributeControllerRegisters.REGISTERS.OVERSCANCOLORREGISTER; //Update the overscan color!
			//dolog("VGA","VTotal after overscancolor: %i",VGA->precalcs.verticaltotal); //Log it!
			//unlockVGA(); //We're finished with the VGA!
		}

		if (AttrUpdated || (whereupdated == (WHEREUPDATED_ATTRIBUTECONTROLLER | 0x10))) //Mode control updated?
		{
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.AttributeModeControlRegister_ColorEnable8Bit = VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.ColorEnable8Bit;
			VGA->precalcs.AttributeModeControlRegister_PixelPanningMode = VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.PixelPanningMode;
			//unlockVGA(); //We're finished with the VGA!
		}

		if (AttrUpdated || (whereupdated==(WHEREUPDATED_ATTRIBUTECONTROLLER|0x13))
			|| (whereupdated==(WHEREUPDATED_ATTRIBUTECONTROLLER|0x10))
			|| charwidthupdated) //Updated?
		{
			//Precalculate horizontal pixel panning:
			byte pixelboost = 0; //Actual pixel boost!
			byte possibleboost; //Possible value!
			possibleboost = VGA->registers->AttributeControllerRegisters.REGISTERS.HORIZONTALPIXELPANNINGREGISTER.PixelShiftCount; //Possible value, to be determined!
			//lockVGA(); //We don't want to corrupt the renderer's data!
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
			//unlockVGA(); //We're finished with the VGA!
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
			if (UPDATE_SECTION(whereupdated) || ((whereupdated==WHEREUPDATED_DACMASKREGISTER) && VGA->precalcs.lastDACMask!=VGA->registers->DACMaskRegister) || FullUpdate) //DAC Mask changed only?
			{
				int colorval;
				colorval = 0; //Init!
				//lockVGA(); //We don't want to corrupt the renderer's data!
				for (;;) //Precalculate colors for DAC!
				{
					VGA->precalcs.DAC[colorval] = getcol256(VGA,colorval); //Translate directly through DAC for output!
					DAC_updateEntry(VGA,colorval); //Update a DAC entry for rendering!
					if (++colorval&0xFF00) break; //Overflow?
				}
				VGA->precalcs.lastDACMask = VGA->registers->DACMaskRegister; //Save the DAC mask for future checking if it's changed!
				//unlockVGA(); //We're finished with the VGA!
			}
		}
		else //Single register updated, no mask register updated?
		{
			//lockVGA(); //We don't want to corrupt the renderer's data!
			VGA->precalcs.DAC[whereupdated&0xFF] = getcol256(VGA,whereupdated&0xFF); //Translate directly through DAC for output, single color only!
			DAC_updateEntry(VGA,whereupdated&0xFF); //Update a DAC entry for rendering!
			//unlockVGA(); //We're finished with the VGA!
		}
		//dolog("VGA","VTotal after DAC: %i",VGA->precalcs.verticaltotal); //Log it!
	}

	if (VerticalClocksUpdated) //Ammount of vertical clocks have been updated?
	{
		//Character height / vertical character clocks!
		//lockVGA(); //We don't want to corrupt the renderer's data!
		VGA->precalcs.clockselectrows = VGA->precalcs.verticalcharacterclocks = (VGA->precalcs.verticaltotal+1); //Use the same value!
		
		VGA->precalcs.scanlinepercentage = SAFEDIV(1.0f,VGA->precalcs.verticalcharacterclocks); //Re-calculate scanline percentage!
		//unlockVGA(); //We're finished with the VGA!
		if (VGA==getActiveVGA()) //Active VGA?
		{
			changeRowTimer(VGA,VGA->precalcs.clockselectrows); //Make sure the display scanline refresh rate is OK!		
		}
		recalcScanline = 1; //Recalc scanline data!
	}
	
	//Recalculate all our lookup tables when needed!
	if (recalcScanline) //Update scanline information?
	{
		//lockVGA(); //We don't want to corrupt the renderer's data!
		VGA_Sequencer_calcScanlineData(VGA); //Recalculate all scanline data!
		//unlockVGA(); //We're finished with the VGA!
	}
	
	if (updateCRTC) //Update CRTC?
	{
		//lockVGA(); //We don't want to corrupt the renderer's data!
		VGA_calcprecalcs_CRTC(VGA); //Update the CRTC timing data!
		//unlockVGA(); //We're finished with the VGA!
		adjustVGASpeed(); //Auto-adjust our VGA speed!
	}
	
	if (recalcAttr) //Update attribute controller?
	{
		//lockVGA(); //We don't want to corrupt the renderer's data!
		VGA_AttributeController_calcAttributes(VGA); //Recalc pixel logic!	
		//unlockVGA(); //We're finished with the VGA!
		adjustVGASpeed(); //Auto-adjust our VGA speed!
	}
}
