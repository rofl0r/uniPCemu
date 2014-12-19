#include "headers/emu/gpu/gpu.h" //GPU!
#include "headers/hardware/vga.h" //VGA!
#include "headers/hardware/vga_screen/vga_attributecontroller.h" //Attribute controller!
#include "headers/hardware/vga_screen/vga_sequencer_graphicsmode.h" //Text mode!
#include "headers/hardware/vga_screen/vga_sequencer_textmode.h" //Text mode!
#include "headers/hardware/vga_screen/vga_sequencer_textmode_cursor.h" //Text mode cursor!
#include "headers/hardware/vga_screen/vga_crtcontroller.h" //CRT Controller for finishing up!
#include "headers/hardware/vga_screen/vga_dac.h" //DAC support!
#include "headers/hardware/pic.h" //IRQ support!
#include "headers/cpu/interrupts.h" //For get/putpixel variant!
#include "headers/support/highrestimer.h" //High resolution clock!
#include "headers/support/zalloc.h" //Memory protection support!

#include "headers/emu/gpu/gpu_text.h" //Text support!

extern GPU_type GPU; //GPU!
extern VGA_Type *ActiveVGA; //Active VGA!

//Free time rendering pixels to the sound engine?
//#define FREE_PIXELTIME
//Debug total pixel speed?
//#define DEBUG_PIXEL_SPEED
//Debug pixel stage speed?
//#define DEBUG_PIXEL_STAGE_SPEED

//Newline/scanline speed debugging
//#define DEBUG_SCANLINE_SPEED
//#define DEBUG_NEWLINE_SPEED

//Log pixel speed?
//#define LOG_PIXEL_SPEED

//Sequencer variables for subs:

extern float curscanlinepercentage; //Current scanline percentage (0.0-1.0)!
extern byte EMU_RUNNING; //Emulator running? 0=Not running, 1=Running, Active CPU, 2=Running, Inactive CPU (BIOS etc.)

OPTINLINE float VGA_VerticalRefreshRate(VGA_Type *VGA) //Scanline speed for one line in Hz!
{
//Horizontal Refresh Rate=Clock Frequency (in Hz)/horizontal pixels
//Vertical Refresh rate=Horizontal Refresh Rate/total scan lines!
	if (!memprotect(VGA,sizeof(*VGA),NULL)) //No VGA?
	{
		return 0; //Remove VGA Scanline counter: nothing to render!
	}
	
//So: Vertical Refresh (One ScanLine)=Clock Frequency (in Hz)/total scan lines! for one scan line!
	switch (VGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.ClockSelect)
	{
	case 0: //25 MHz clock?
		return 25000000.0/getHorizontalTotal(VGA); //25 MHz scanline clock!
	case 1: //28 MHz clock?
		return 28000000.0/getHorizontalTotal(VGA); //28 MHz scanline clock!
	case 2: //Unknown?
	case 3: //Unknown?
	default:
		return 0; //None: not supported=Disable timer in the end!
	}
}

//Main rendering routine: renders pixels to the emulated screen.

void drawPixel(word x, word y, uint_32 pixel)
{
	if ((y<EMU_MAX_Y) && (x<EMU_MAX_X)) //Valid pixel to render?
	{
		register uint_32 old;
		old = EMU_BUFFER(x,y); //Read old!
		old ^= pixel; //Check for differences!
		GPU.emu_buffer_dirty |= old; //Update, set changed bits when changed!
		EMU_BUFFER(x,y) = pixel; //Update whether it's needed or not!
	}
}

extern uint_32 totaltime_audio_avg; //Time the audio thread needs. Use this to give it enough time!
OPTINLINE void recalc_pixelsrendered(SEQ_DATA *Sequencer)
{
	Sequencer->pixelstorender = SAFEDIVUINT32(totaltime_audio_avg,SAFEDIV(Sequencer->totalpixeltime,Sequencer->totalpixels));
}

/*OPTINLINE void VGA_Sequencer_calcScanlineData(VGA_Type *VGA) //Recalcs all scanline data for the sequencer!
{
	//First, all our variables!
	word originalscanline; //Original scanline!
	word scanline; //Active scanline on-screen!
	uint_32 bytepanning;
	byte allow_pixelshiftcount;
	word pixelshift;
	word horizontalstart;
	uint_32 pixelmove;
	uint_32 rowstart;
	
	SEQ_DATA *Sequencer;
	Sequencer = GETSEQUENCER(VGA); //Our sequencer!

	SEQ_PRECALCS *precalcs; //Precalcs for the current row!
	for (originalscanline=0;originalscanline<0x400;originalscanline++) //Process all original scanlines!
	{
		precalcs = &Sequencer->precalcs[originalscanline]; //Our current precalcs to calculate!
		
		//Calculate current scanline in the precalcs!
		scanline = originalscanline; //Load original scanline to calculate first!
		scanline -= VGA->registers->CRTControllerRegisters.REGISTERS.PRESETROWSCANREGISTER.PresetRowScan; //From presetrowscan rows are not rendered!
		if (VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.SLDIV) //Divide scanline clock by 2?
		{
			scanline &= 0xFFFE; //Clear bit 0!
			scanline >>= 1; //Shift to divide by 2!
		}
		
		precalcs->precalcs_scanline = scanline; //Save the new scanline!
		
		//Determine panning
		bytepanning = VGA->registers->CRTControllerRegisters.REGISTERS.PRESETROWSCANREGISTER.BytePanning; //Byte panning for Start Address Register for characters or 0,0 pixel!

		//Determine shifts and reset the start map if needed!
		byte reset_startmap;
		reset_startmap = 0; //Default: don't reset!
		
		allow_pixelshiftcount = 1; //Allow by default!
		if (originalscanline>=VGA->precalcs.topwindowstart) //Top window reached?
		{
			reset_startmap = 1; //Enforce start of map to 0 for the top window!
			if (VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.PixelPanningMode)
			{
				bytepanning = 0; //Act like no byte panning is enabled!
				allow_pixelshiftcount = 0; //Don't allow it anymore!
			}
		}

		precalcs->startmap = VGA->precalcs.startaddress[reset_startmap]; //What start address to use?
		
		//Determine byte panning and pixel shift count!
		precalcs->bytepanning = bytepanning; //Pass!

		if (allow_pixelshiftcount) //Allowing pixel shift count?
		{
			pixelshift = getHorizontalPixelPanning(VGA); //Get horizontal pixel panning!
		}
		else
		{
			pixelshift = 0; //Default: no pixel shift!	
		}

		//Horizontal start
		horizontalstart = getHorizontalStart(VGA); //For each calculation!

		//Calculate pixel move with shift and start!
		pixelmove = pixelshift;
		pixelmove -= horizontalstart; //Ready to add!
		precalcs->pixelmove = pixelmove; //Set pixel move!

		//Current block start!
		precalcs->CurrentScanLineStart = OPTMUL32(originalscanline,VGA->LinesToRender); //The start of the scanline block on-screen!
		
		//Row start!
		rowstart = precalcs->startmap; //Start address of current display!
		rowstart += getVRAMScanlineStart(VGA,scanline); //Calculate row start!
		precalcs->rowstart = rowstart; //Save the start of the row!
		
		//Scan counter setting and tempy.
		precalcs->rowscancounter = scanline; //Load the current scanline into the row!
		
		//First scanline of the screen!
		if (!originalscanline) //First scanline starting to render?
		{
			VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.CRTInterruptPending = 0; //VBlank not taking place anymore!
		}
	}
}*/

/*OPTINLINE void calcScanlineData(VGA_Type *VGA)
{
	#ifdef DEBUG_SCANLINE_SPEED
		TicksHolder ticks; //Our ticks holder!
		getmspassed(&ticks); //Init!
	#endif

	//Load&save scanline!
	word originalscanline = VGA->registers->Scanline; //The scanline to render (unmodified to screen)!
	originalscanline &= 0x3FF; //Make sure we're within range!
	SEQ_DATA *Sequencer = GETSEQUENCER(VGA); //Retrieve the sequencer!
	Sequencer->currentPrecalcs = &Sequencer->precalcs[originalscanline]; //Load the current precalcs!
	VGA->CurrentScanLineStart = Sequencer->currentPrecalcs->CurrentScanLineStart; //Load current scanline start!
	
	//First scanline of the screen!
	VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.CRTInterruptPending &= !originalscanline; //VBlank not taking place anymore!

	#ifdef DEBUG_SCANLINE_SPEED
		//For the average line preparation time!
		uint_64 lasttime;
		lasttime = getmspassed(&ticks);
		Sequencer->lastscanlinetime = lasttime;
		Sequencer->totalscanlinetime += lasttime;
		++Sequencer->totalscanlines; //One line further!
	#endif
}*/

/*OPTINLINE void CalcScanlinePixel(VGA_Type *VGA) //Calculate the current pixel!
{
	SEQ_DATA *Sequencer;
	Sequencer = GETSEQUENCER(VGA);
	word tempx;
	tempx = Sequencer->precalcs->pixelmove; //Extra pixel shift to the left (start further at the right for 0,Y)!
	tempx += Sequencer->x; //Use the current pixel!

	if (VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.DIV2) //Divide pixel by 2?
	{
		tempx &= 0xFFFE; //Disable bit 1!
		tempx >>= 1; //Divide by 2, multiplying all pixels with 2!
	}
	
	Sequencer->tempx = tempx; //Save the temporary x!
}

OPTINLINE void VGA_Sequencer_newline(VGA_Type *VGA, byte currentscreenbottom)
{
	#ifdef DEBUG_NEWLINE_SPEED
		TicksHolder ticks; //Our ticks holder!
		getmspassed(&ticks); //Init!
	#endif

	SEQ_DATA *Sequencer;
	Sequencer = GETSEQUENCER(VGA);

	//plot scanline is changed to plot pixel after the DAC step!
	curscanlinepercentage += VGA->precalcs.scanlinepercentage; //Add to ammount of scanlines processed relatively!

	Sequencer->currentPrecalcs = NULL; //No current loaded yet!
	Sequencer->x = 0; //Reset x&newline_ready for next row!
	//We're a newline, so we're not ready to render yet: we need data loading!
	++VGA->registers->Scanline; //Next scanline!
	
	byte totalreached;
	totalreached = VGA->registers->VerticalDisplayTotalReached; //Total reached?
	totalreached |= (OPTMUL32(VGA->registers->Scanline,VGA->LinesToRender)>=VGA->precalcs.verticalcharacterclocks); //Normal vblank rule!

	//Total for timing information!
	#ifdef DEBUG_NEWLINE_SPEED
		uint_64 lasttime;
		lasttime = getmspassed(&ticks);
		++Sequencer->totalnewlines; //One line further!
	#endif
	
	if (totalreached) //VBlank occurred?
	{
		VGA->registers->Scanline = VGA->registers->VerticalDisplayTotalReached = 0; //Reset scan line to the top of the screen!
		if (VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.CRTInterruptPending) //Pending interrupt?
		{
			VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.CRTInterruptPending = 0; //Not pending anymore!
			if (EMU_RUNNING&1) //CPU is running (IRQs emulated)?
			{
				doirq(2); //Trigger interrupt 2: VBlank interrupt!
			}
		}
		#ifdef DEBUG_NEWLINE_SPEED
			lasttime += getmspassed(&ticks);
		#endif
		
		if (!VGA->registers->SequencerRegisters.REGISTERS.DISABLERENDERING) //Rendering enabled?
		{
			#ifdef DEBUG_NEWLINE_SPEED
				lasttime += getmspassed(&ticks);
			#endif
			VGA_VBlankHandler(VGA); //Handle all VBlank stuff!
		}
		#ifdef DEBUG_NEWLINE_SPEED
			else
			{
				lasttime += getmspassed(&ticks);
			}
		#endif
	}
	#ifdef DEBUG_NEWLINE_SPEED
		Sequencer->lastnewlinetime = lasttime;
		Sequencer->totalnewlinetime += lasttime;
	#endif
}*/

typedef void (*Sequencer_pixelhandler)(VGA_Type *VGA,VGA_AttributeInfo *Sequencer_Attributeinfo, word tempx,word tempy,word x,word Scanline,uint_32 bytepanning); //Pixel(s) handler!

OPTINLINE byte VGA_Sequencer_ProcessDAC(VGA_Type *VGA, byte DACValue) //Process DAC!
{
	byte result;
	SEQ_DATA *Sequencer;
	Sequencer = GETSEQUENCER(VGA); //Our sequencer!
	//DAC
	#ifdef DEBUG_PIXEL_STAGE_SPEED
		TicksHolder ticks2; //Our ticks to count!
		startHiresCounting(&ticks2); //Init ticks taken!
	#endif
	result = VGA_DAC(VGA,DACValue); //DAC final processing! Used to be: VGA,x
	#ifdef DEBUG_PIXEL_STAGE_SPEED
		Sequencer->lastpixeltimedac = getmspassed(&ticks2); //Save last!
		Sequencer->totalpixeltimedac += Sequencer->lastpixeltimedac; //Count total ticks!
		Sequencer->totalpixelsdac += VGA->LinesToRender; //Count total pixels!
	#endif
	return result; //Give the result!
}

byte Sequencer_Running; //Sequencer running?

//Special states!
byte blanking = 0; //Are we blanking!
byte retracing = 0; //Allow rendering by retrace!
byte totalling = 0; //Allow rendering by total!

byte hblank = 0, hretrace = 0; //Horizontal blanking/retrace?
byte vblank = 0, vretrace = 0; //Vertical blanking/retrace?

void VGA_NOPT(SEQ_DATA *Sequencer, VGA_Type *VGA) //TRUE NO-OP!
{
	//dolog("VGA","NOPT->%i,%i",Sequencer->x,Sequencer->Scanline);
	totalling = 0; //Not totalling anymore!
}

void VGA_NOP(SEQ_DATA *Sequencer, VGA_Type *VGA) //NOP with quit!
{}

byte flashing = 0; //Are we flash status when used?

//Total handlers!
void VGA_VTotal(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	Sequencer->Scanline = 0; //Reset for the next scanline!
	VGA_VBlankHandler(VGA); //Handle all VBlank stuff!
	Sequencer_Running = 0; //Not running anymore!
	flashing = !flashing; //Reverse flashing!
}

void VGA_HTotal(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//Process HBlank: reload display data for the next scanline!
	//Sequencer itself
	Sequencer->x = 0; //Reset for the next scanline!
	++Sequencer->Scanline; //Next scanline to process!
	
	//CRT
	++VGA->CRTC.y; //Next row on-screen!
	
	//Stop running!
	Sequencer_Running = 0; //Not running anymore!
}

//Retrace handlers!
void VGA_VRetrace(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	GPU.yres = VGA->CRTC.y; //Update Y resolution!
	VGA->CRTC.y = 0; //Reset destination row!
}

void VGA_HRetrace(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	GPU.xres = VGA->CRTC.x; //Update X resolution!
	VGA->CRTC.x = 0; //Reset destination column!
}

void render_nextPixel(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	++VGA->CRTC.x; //Next column on-screen!
	++Sequencer->x; //Next column in-buffer!
}

//Blank handler!
OPTINLINE void VGA_Blank(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	drawPixel(VGA->CRTC.x,VGA->CRTC.y,RGB(0x00,0x00,0x00)); //Draw blank!
	render_nextPixel(Sequencer,VGA);
}

//Active display handler!
void VGA_ActiveDisplay(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	if (blanking) //Are we blanking?
	{
		VGA_Blank(Sequencer,VGA); //Blank redirect!
		return;
	}
	
	/*
	SEQ_DATA *Sequencer = GETSEQUENCER(VGA); //Our current sequencer!
	VGA_AttributeInfo *Attributeinfo_ptr = &Sequencer->Attributeinfo; //Kept pointer to the attribute info!
	Sequencer_pixelhandler pixelhandler[2] = {VGA_Sequencer_TextMode,VGA_Sequencer_GraphicsMode}; //Handlers for pixels!
	*/

	uint_32 color;
	color = flashing?RGB(0xFF,0x00,0x00):RGB(0x00,0xFF,0x00); //Flash in two colors to detect refresh!

	//Active display!
	drawPixel(VGA->CRTC.x,VGA->CRTC.y,color); //Green display area for now!
	render_nextPixel(Sequencer,VGA);
}

//Overscan handler!
void VGA_Overscan(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	if (blanking) //Are we blanking?
	{
		VGA_Blank(Sequencer,VGA); //Blank redirect!
		return;
	}
	
	//Overscan!
	drawPixel(VGA->CRTC.x,VGA->CRTC.y,VGA_Sequencer_ProcessDAC(VGA,VGA->precalcs.overscancolor)); //Draw overscan!
	render_nextPixel(Sequencer,VGA);
}

//All different signals!

void VGA_SIGNAL_NOP(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal)
{
	//We're a NOP operation for the signal: do nothing, also don't abort since we're not the renderer!
}

void VGA_SIGNAL_NOPQ(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal) //Nothing to do yet?
{
	Sequencer_Running = 0; //Not running anymore: we can't do anything without a signal!
}

void VGA_SIGNAL_HANDLER(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal)
{
	//Blankings
	if (signal&VGA_SIGNAL_HBLANKSTART) //HBlank start?
	{
		hblank = 1; //We're blanking!
	}
	if (signal&VGA_SIGNAL_HBLANKEND) //HBlank end?
	{
		hblank = 0; //We're not blanking anymore!
	}
	
	if (signal&VGA_SIGNAL_VBLANKSTART) //VBlank start?
	{
		vblank = 1; //We're blanking!
	}
	if (signal&VGA_SIGNAL_VBLANKEND) //VBlank end?
	{
		vblank = 0; //We're not blanking anymore!
	}
	
	//Both H&VBlank count!
	blanking = hblank; //Process hblank!
	blanking |= vblank; //Process vblank!
	
	byte oldretrace;
	oldretrace = hretrace; //Save old!
	//Retraces
	if (signal&VGA_SIGNAL_HRETRACESTART) //HRetrace start?
	{
		hretrace = 1; //We're retracing!
	}
	if (hretrace && !oldretrace) //Retracing horizontal and wasn't retracing yet?
	{
		VGA_HRetrace(Sequencer,VGA); //HRetrace!	
	}
	if (signal&VGA_SIGNAL_HRETRACEEND) //HRetrace end?
	{
		hretrace = 0; //We're not retracing!
	}
	
	oldretrace = vretrace; //Save old!
	if (signal&VGA_SIGNAL_VRETRACESTART) //VRetrace start?
	{
		vretrace = 1; //We're retracing!
	}
	if (vretrace && !oldretrace) //Retracing vertical?
	{
		VGA_VRetrace(Sequencer,VGA); //VRetrace!
	}
	if (signal&VGA_SIGNAL_VRETRACEEND) //VRetrace end?
	{
		vretrace = 0; //We're not retracing!
	}
	
	
	retracing = hretrace; //We're retracing!
	retracing |= vretrace; //We're retracing?
	//Retracing disables output!

	ActiveVGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.VRetrace = vretrace; //Vertical retrace?
	ActiveVGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.DisplayDisabled = retracing; //Vertical or horizontal retrace?

	totalling = 0; //Default: Not totalling!
	//Totals
	if (signal&VGA_SIGNAL_HTOTAL) //HTotal?
	{
		VGA_HTotal(Sequencer,VGA); //Process HTotal!
		totalling = 1; //Total reached!
	}
	if (signal&VGA_SIGNAL_VTOTAL) //VTotal?
	{
		VGA_VTotal(Sequencer,VGA); //Process VTotal!
		totalling = 1; //Total reached!
	}
}

//Combination functions of the above:

//Horizontal before vertical, retrace before total.

typedef void (*DisplayRenderHandler)(SEQ_DATA *Sequencer, VGA_Type *VGA);
typedef void (*DisplaySignalHandler)(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal);

//displayrenderhandler[total][retrace][signal]
DisplayRenderHandler displayrenderhandler[2][2][0x10000]; //Our handlers for all pixels!
DisplaySignalHandler displaysignalhandler[0x10000]; //Our rendering handlers! Executed before all states!

//Initialise all handlers!
void initStateHandlers()
{
	uint_32 i;
	//Default uninitialised entries!
	displayrenderhandler[0][0][0] = &VGA_NOP; //Default: no action!
	displayrenderhandler[0][1][0] = &VGA_NOP; //Default: no action!
	displayrenderhandler[1][0][0] = &VGA_NOP; //Default: no action!
	displayrenderhandler[1][1][0] = &VGA_NOP; //Default: no action!

	displaysignalhandler[0] = &VGA_SIGNAL_NOPQ; //No signal at all: simply abort!
	
	for (i=1;i<0x10000;i++) //Fill the normal entries!
	{
		//Total handler for total handlers!
		displayrenderhandler[0][1][i] = &VGA_Overscan; //Do nothing when disabled: retrace does overscan!
		displayrenderhandler[1][0][i] = &VGA_NOPT; //Do nothing when disabled: total handler!
		displayrenderhandler[1][1][i] = &VGA_NOPT; //Do nothing when disabled: total&retrace handler!
		
		//Rendering handler without retrace AND total!
		displayrenderhandler[0][0][i] = (((i&VGA_DISPLAYMASK)==VGA_DISPLAYACTIVE)?&VGA_ActiveDisplay:&VGA_Overscan); //Not retracing or any total handler = display/overscan!
		
		//Signal handler, if any!
		displaysignalhandler[i] = (i&VGA_SIGNAL_HASSIGNAL)?&VGA_SIGNAL_HANDLER:&VGA_SIGNAL_NOP; //Signal handler if needed!
	}
}

void VGA_Sequencer(VGA_Type *VGA, byte currentscreenbottom)
{
	SEQ_DATA *Sequencer;
	Sequencer = GETSEQUENCER(VGA); //Our sequencer!

	/*	#ifdef DEBUG_PIXEL_SPEED
			TicksHolder ticks; //All possible ticks holders!
		#else
		#ifdef FREE_PIXELTIME
			TicksHolder ticks; //All possible ticks holders!
		#endif
		#endif
	*/
	
	//Render the line!
	/*uint_32 pixelsrendered; //Ammount of horizontal pixels rendered atm!
	pixelsrendered = 0; //Init!

	for (;;) //Process next pixel!
	{
		#ifdef DEBUG_PIXEL_SPEED
			startHiresCounting(&ticks); //Init ticks taken!
		#else
		#ifdef FREE_PIXELTIME
			startHiresCounting(&ticks); //Init ticks taken!			
		#endif
		#endif

		if (!VGA->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.ScreenDisable) //Drawing (enabled)?
		{
			#ifdef DEBUG_PIXEL_STAGE_SPEED
				TicksHolder ticks2;
			#endif
			//Prepare our info!
			if (!memprotect(Sequencer->currentPrecalcs,sizeof(SEQ_PRECALCS),NULL)) //Not ready to go?
			{
				calcScanlineData(VGA); //Refresh data when needed!
			}
			CalcScanlinePixel(VGA); //Calculate pixel!

			if (memprotect(Sequencer->currentPrecalcs,sizeof(*Sequencer->currentPrecalcs),NULL)) //Valid precalcs?
			{
				//if (is_activedisplay(VGA,VGA->CurrentScanLineStart,Sequencer->x)) //Active display?
				//{
					//We're processing 1 (character sub-)pixel!
					/
					
					Simplified list:
					VGA
					Sequencer_Attributeinfo_ptr
					Sequencer->tempx
					Sequencer->rowscancounter
					Sequencer->x
					Sequencer->Scanline
					Sequencer->bytepanning
					
					/

					//Pixel(s)
					#ifdef DEBUG_PIXEL_STAGE_SPEED
						startHiresCounting(&ticks2); //Init ticks taken!
					#endif
					pixelhandler[VGA->precalcs.graphicsmode](VGA,Attributeinfo_ptr,Sequencer->tempx,Sequencer->currentPrecalcs->rowscancounter,Sequencer->x,Sequencer->currentPrecalcs->Scanline,Sequencer->currentPrecalcs->bytepanning); //Retrieve the pixel from VRAM!
					#ifdef DEBUG_PIXEL_STAGE_SPEED
						Sequencer->lastpixeltimepixel = getmspassed(&ticks2);
						Sequencer->totalpixeltimepixel += Sequencer->lastpixeltimepixel; //Count total ticks!
						Sequencer->totalpixelspixel += VGA->LinesToRender; //Count total pixels!
					#endif
					
					//Attribute
					#ifdef DEBUG_PIXEL_STAGE_SPEED
						startHiresCounting(&ticks2); //Init ticks taken!
					#endif
					VGA_AttributeController(Attributeinfo_ptr, VGA, Sequencer->x,Sequencer->tempx,Sequencer->currentPrecalcs->rowscancounter); //Apply attribute to generate DAC Index! Used to be: VGA,Scanline,x,&characterinfo
					#ifdef DEBUG_PIXEL_STAGE_SPEED
						Sequencer->lastpixeltimeattribute = getmspassed(&ticks2);
						Sequencer->totalpixeltimeattribute += Sequencer->lastpixeltimeattribute; //Count total ticks!
						Sequencer->totalpixelsattribute += VGA->LinesToRender; //Count total pixels!
					#endif
				/}
				else if (is_overscan(VGA,VGA->CurrentScanLineStart,Sequencer->x)) //Overscan?
				{
					byte y2=VGA->precalcs.renderedlines; //Total ammount to process!
					byte overscancolor;
					overscancolor = VGA->precalcs.overscancolor; //Load once only!
					for (;;) //Process all overscan!
					{
						--y2; //Decrease first!
						VGA->CurrentScanLine[y2] = overscancolor; //Overscan! Already DAC Index!
						if (!y2) goto finishdac; //Break out if nothing is left!
					}
				}/

				finishdac: //Finish with the DAC always!
				//DAC
				#ifdef DEBUG_PIXEL_STAGE_SPEED
					startHiresCounting(&ticks2); //Init ticks taken!
				#endif
				VGA_DAC(VGA,Sequencer->x,currentscreenbottom); //DAC final processing! Used to be: VGA,x
				#ifdef DEBUG_PIXEL_STAGE_SPEED
					Sequencer->lastpixeltimedac = getmspassed(&ticks2); //Save last!
					Sequencer->totalpixeltimedac += Sequencer->lastpixeltimedac; //Count total ticks!
					Sequencer->totalpixelsdac += VGA->LinesToRender; //Count total pixels!
				#endif
			}

			//Total time!
			#ifdef DEBUG_PIXEL_SPEED
				Sequencer->totalpixeltime += getmspassed(&ticks); //Count total ticks!
				Sequencer->totalpixels += VGA->LinesToRender; //Count total pixels!
			#else
				#ifdef FREE_PIXELTIME
					Sequencer->totalpixeltime += getmspassed(&ticks); //Count total ticks!
					Sequencer->totalpixels += VGA->LinesToRender; //Count total pixels!
				#endif
			#endif
		} //Valid precalcs?
		#ifdef FREE_PIXELTIME //Freeing up pixel time?
			if (++Sequencer->pixelsrendered>=Sequencer->pixelstorender) //Overflow timing?
			{
				delay(totaltime_audio_avg); //Give other threads some time!
				getmspassed(&ticks); //Update to current time, skipped!!
				Sequencer->pixelsrendered = 0; //Reset!
				recalc_pixelsrendered(VGA); //Recalc next portion!
			}
		#endif

		if (++Sequencer->x>=GPU.xres) //End of line reached?
		{
			VGA_Sequencer_newline(VGA,currentscreenbottom); //Process newline!
		}

		//Update the time for the current pixel with newline(s)!
		#ifdef DEBUG_PIXEL_SPEED
			Sequencer->totalpixeltime += getmspassed(&ticks); //Count total ticks!
		#else
			#ifdef FREE_PIXELTIME
				Sequencer->totalpixeltime += getmspassed(&ticks); //Count total ticks!
			#endif
		#endif
		
		if (++pixelsrendered>=GPU.xres) //Enough rendered (one line at a time)?
		{
			#ifdef LOG_PIXEL_SPEED
				dolog("timing","AVG Pixel rendering time: %ius",SAFEDIV(Sequencer->totalpixeltime,Sequencer->totalpixels)); //Log it!	
			#endif
			return; //Stop processing!
		}
	}*/ //Process next!
	
	//All possible states!
	if (!displaysignalhandler[0]) //Nothing set?
	{
		initStateHandlers(); //Init our display states for usage!
	}
	
	Sequencer_Running = 1; //Start running!
	
	word displaystate; //Current display state!
	for (;Sequencer_Running;) //New CRTC constrolled way!
	{
		displaystate = get_display(VGA,Sequencer->Scanline,Sequencer->x); //Current display state!
		displaysignalhandler[displaystate](Sequencer,VGA,displaystate); //Handle any change in display state first!
		displayrenderhandler[totalling][retracing][displaystate](Sequencer,VGA); //Execute our signal!
	}
}