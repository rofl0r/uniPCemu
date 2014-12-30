#include "headers/emu/gpu/gpu.h" //GPU!
#include "headers/hardware/vga.h" //VGA!
#include "headers/hardware/vga_screen/vga_attributecontroller.h" //Attribute controller!
#include "headers/hardware/vga_screen/vga_sequencer_graphicsmode.h" //Text mode!
#include "headers/hardware/vga_screen/vga_sequencer_textmode.h" //Text mode!
#include "headers/hardware/vga_screen/vga_crtcontroller.h" //CRT Controller for finishing up!
#include "headers/hardware/vga_screen/vga_dac.h" //DAC support!
#include "headers/hardware/pic.h" //IRQ support!
#include "headers/cpu/interrupts.h" //For get/putpixel variant!
#include "headers/support/highrestimer.h" //High resolution clock!
#include "headers/support/zalloc.h" //Memory protection support!

#include "headers/emu/gpu/gpu_text.h" //Text support!

extern GPU_type GPU; //GPU!
extern VGA_Type *ActiveVGA; //Active VGA!

//Are we disabled?
#define HW_DISABLED 0

typedef void (*DisplayRenderHandler)(SEQ_DATA *Sequencer, VGA_Type *VGA);

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

OPTINLINE void drawPixel(word x, word y, uint_32 pixel)
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

OPTINLINE void VGA_Sequencer_calcScanlineData(VGA_Type *VGA) //Recalcs all scanline data for the sequencer!
{
	//First, all our variables!
	//word originalscanline; //Original scanline!
	//word scanline; //Active scanline on-screen!
	uint_32 bytepanning;
	byte allow_pixelshiftcount;
	//word pixelshift;
	//word horizontalstart;
	//uint_32 pixelmove;
	//uint_32 rowstart;
	
	SEQ_DATA *Sequencer;
	Sequencer = GETSEQUENCER(VGA); //Our sequencer!

	/*for (originalscanline=0;originalscanline<0x400;originalscanline++) //Process all original scanlines!
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
		*/
		//Determine panning
		bytepanning = VGA->registers->CRTControllerRegisters.REGISTERS.PRESETROWSCANREGISTER.BytePanning; //Byte panning for Start Address Register for characters or 0,0 pixel!

		//Determine shifts and reset the start map if needed!
		byte reset_startmap;
		reset_startmap = 0; //Default: don't reset!
		
		allow_pixelshiftcount = 1; //Allow by default!
		if (Sequencer->Scanline>=VGA->precalcs.topwindowstart) //Top window reached?
		{
			reset_startmap = 1; //Enforce start of map to 0 for the top window!
			if (VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.PixelPanningMode)
			{
				bytepanning = 0; //Act like no byte panning is enabled!
				allow_pixelshiftcount = 0; //Don't allow it anymore!
			}
		}

		Sequencer->startmap = VGA->precalcs.startaddress[reset_startmap]; //What start address to use?
		
		//Determine byte panning and pixel shift count!
		Sequencer->bytepanning = bytepanning; //Pass!

		/*if (allow_pixelshiftcount) //Allowing pixel shift count?
		{
			pixelshift = getHorizontalPixelPanning(VGA); //Get horizontal pixel panning!
		}
		else
		{
			pixelshift = 0; //Default: no pixel shift!	
		}*/

		/*
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
	}*/
}

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
		Sequencer->lastpixeltimedac = getuspassed(&ticks2); //Save last!
		Sequencer->totalpixeltimedac += Sequencer->lastpixeltimedac; //Count total ticks!
		Sequencer->totalpixelsdac += VGA->LinesToRender; //Count total pixels!
	#endif
	return result; //Give the result!
}

byte Sequencer_Break; //Sequencer breaked (loop exit)?

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

//Total handlers!
void VGA_VTotal(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	Sequencer->Scanline = 0; //Reset for the next scanline!
	VGA_Sequencer_TextMode_updateRow(VGA, Sequencer); //Scanline has been changed!
	VGA_VBlankHandler(VGA); //Handle all VBlank stuff!
	Sequencer_Break = 1; //Not running anymore!
}

void VGA_HTotal(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//Process HBlank: reload display data for the next scanline!
	//Sequencer itself
	Sequencer->x = 0; //Reset for the next scanline!
	++Sequencer->Scanline; //Next scanline to process!
	VGA_Sequencer_TextMode_updateRow(VGA, Sequencer); //Scanline has been changed!
	
	//CRT
	++VGA->CRTC.y; //Next row on-screen!
	
	//Sequencer rendering data
	Sequencer->tempx = 0; //Reset the rendering position from the framebuffer!
	VGA_Sequencer_calcScanlineData(VGA);
	
	//Stop running!
	Sequencer_Break = 1; //Not running anymore!
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
OPTINLINE void VGA_Blank(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	drawPixel(VGA->CRTC.x,VGA->CRTC.y,RGB(0x00,0x00,0x00)); //Draw blank!
}

typedef void (*VGA_Sequencer_Mode)(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo); //Render an active display pixel!

void VGA_ActiveDisplay_noblanking(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	//Active display!
	drawPixel(VGA->CRTC.x,VGA->CRTC.y,VGA_Sequencer_ProcessDAC(VGA,attributeinfo->attribute)); //Render through the DAC!
}

//Active display handler!
void VGA_ActiveDisplay(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//Render our active display here! Start with text mode!		
	VGA_AttributeInfo attributeinfo; //Our collected attribute info!
	static VGA_Sequencer_Mode activemode[2] = {VGA_Sequencer_TextMode,VGA_Sequencer_GraphicsMode}; //Our display modes!
	othernibble: //Process the next nibble!
	activemode[VGA->precalcs.graphicsmode](VGA,Sequencer,&attributeinfo); //Get the color to render!
	if (VGA_AttributeController(&attributeinfo,VGA,Sequencer)) //Apply the attribute through the attribute controller!
	{
		++Sequencer->tempx; //Next horizontal pixel: we should also count blank pixels: the pixels are normally drawn, but the DAC is set to blanking state, clearing output!
		goto othernibble; //Process the next nibble!
	}
	++Sequencer->tempx; //Next horizontal pixel: we should also count blank pixels: the pixels are normally drawn, but the DAC is set to blanking state, clearing output!

	static VGA_Sequencer_Mode activedisplayhandlers[2] = {VGA_ActiveDisplay_noblanking,VGA_Blank}; //For giving the correct output sub-level!
	activedisplayhandlers[blanking](VGA,Sequencer,&attributeinfo); //Blank or active display!
	render_nextPixel(Sequencer,VGA); //Common: Goto next pixel!
	++Sequencer->totalpixels; //A pixel has been processed!
}

void VGA_Overscan_noblanking(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	//Overscan!
	drawPixel(VGA->CRTC.x,VGA->CRTC.y,VGA_Sequencer_ProcessDAC(VGA,VGA->precalcs.overscancolor)); //Draw overscan!
}

//Overscan handler!
void VGA_Overscan(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	static VGA_Sequencer_Mode activemode[2] = {VGA_Overscan_noblanking,VGA_Blank};
	activemode[blanking](VGA,Sequencer,NULL); //Attribute info isn't used!
	render_nextPixel(Sequencer,VGA);
}

//All different signals!

void VGA_SIGNAL_NOP(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal)
{
	//We're a NOP operation for the signal: do nothing, also don't abort since we're not the renderer!
}

void VGA_SIGNAL_NOPQ(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal) //Nothing to do yet?
{
	Sequencer_Break = 1; //Not running anymore: we can't do anything without a signal!
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
	if (HW_DISABLED) return;

	TicksHolder ticks;
	SEQ_DATA *Sequencer;
	word displaystate; //Current display state!
	Sequencer = GETSEQUENCER(VGA); //Our sequencer!

	//All possible states!
	if (!displaysignalhandler[0]) //Nothing set?
	{
		initStateHandlers(); //Init our display states for usage!
	}
	
	Sequencer_Break = 0; //Start running!
	
	initTicksHolder(&ticks);
	getuspassed(&ticks); //Discard our initial value!
	getuspassed(&ticks); //Bugfix: make sure we're run multiple times before running normally!

	for (;;) //New CRTC constrolled way!
	{
		displaystate = get_display(VGA,Sequencer->Scanline,Sequencer->x); //Current display state!
		displaysignalhandler[displaystate](Sequencer,VGA,displaystate); //Handle any change in display state first!
		displayrenderhandler[totalling][retracing][displaystate](Sequencer,VGA); //Execute our signal!
		if (Sequencer_Break) break; //Abort when done!
	}

	uint_64 passed;
	passed = getpspassed(&ticks); //Log the ammount of time passed!
	Sequencer->totalpixeltime += passed; //Log the ammount of time passed!

	++Sequencer->totalrenders; //Increase total render counting!
	Sequencer->totalrendertime += passed; //Idem!
}