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

//Are we disabled?
#define HW_DISABLED 0

extern GPU_type GPU; //GPU!
extern VGA_Type *ActiveVGA; //Active VGA!

typedef void (*DisplayRenderHandler)(SEQ_DATA *Sequencer, VGA_Type *VGA); //Our rendering handler for all signals!

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

OPTINLINE void drawPixel(VGA_Type *VGA, uint_32 pixel)
{
	if ((VGA->CRTC.y<EMU_MAX_Y) && (VGA->CRTC.x<EMU_MAX_X)) //Valid pixel to render?
	{
		register uint_32 old;
		uint_32 *screenpixel = &EMU_BUFFER(VGA->CRTC.x,VGA->CRTC.y); //Pointer to our pixel!
		old = *screenpixel; //Read old!
		old ^= pixel; //Check for differences!
		if (old) //Changed anything?
		{
			GPU.emu_buffer_dirty |= 1; //Update, set changed bits when changed!
			*screenpixel = pixel; //Update whether it's needed or not!
		}
	}
	++VGA->CRTC.x; //Next x!
}

OPTINLINE void VGA_Sequencer_calcScanlineData(VGA_Type *VGA) //Recalcs all scanline data for the sequencer!
{
	//First, all our variables!
	uint_32 bytepanning;
	byte allow_pixelshiftcount;
	
	SEQ_DATA *Sequencer;
	Sequencer = GETSEQUENCER(VGA); //Our sequencer!

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
}

typedef void (*Sequencer_pixelhandler)(VGA_Type *VGA,VGA_AttributeInfo *Sequencer_Attributeinfo, word tempx,word tempy,word x,word Scanline,uint_32 bytepanning); //Pixel(s) handler!

byte Sequencer_Break; //Sequencer breaked (loop exit)?

//Special states!
byte blanking = 0; //Are we blanking!
byte retracing = 0; //Allow rendering by retrace!
byte totalling = 0; //Allow rendering by total!
byte totalretracing = 0; //Combined flags of retracing/totalling!

byte hblank = 0, hretrace = 0; //Horizontal blanking/retrace?
byte vblank = 0, vretrace = 0; //Vertical blanking/retrace?

void VGA_NOPT(SEQ_DATA *Sequencer, VGA_Type *VGA) //TRUE NO-OP!
{
	totalling = 0; //Not totalling anymore!
}

void VGA_NOP(SEQ_DATA *Sequencer, VGA_Type *VGA) //NOP with quit!
{}

//Total handlers!
void VGA_VTotal(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//First, render ourselves to the screen!
	GPU.xres = Sequencer->xres; //Apply x resolution!
	GPU.yres = Sequencer->yres; //Apply y resolution!
	VGA_VBlankHandler(VGA); //Handle all VBlank stuff!
	
	Sequencer->Scanline = 0; //Reset for the next scanline!
	Sequencer->yres = 0; //Reset Y resolution next frame if not specified (like a real screen)!
	Sequencer->xres = 0; //Reset X resolution next frame if not specified (like a real screen)!
	
	VGA_Sequencer_TextMode_updateRow(VGA, Sequencer); //Scanline has been changed!
	Sequencer_Break = 1; //Not running anymore!
}

void VGA_HTotal(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//Process HBlank: reload display data for the next scanline!
	//Sequencer itself
	Sequencer->x = 0; //Reset for the next scanline!
	++Sequencer->Scanline; //Next scanline to process!
	
	//CRT
	++VGA->CRTC.y; //Next row on-screen!
	
	//Sequencer rendering data
	Sequencer->tempx = 0; //Reset the rendering position from the framebuffer!
	VGA_Sequencer_calcScanlineData(VGA);
	VGA_Sequencer_TextMode_updateRow(VGA, Sequencer); //Scanline has been changed!
	
	//Stop running!
	Sequencer_Break = 1; //Not running anymore!
}

//Retrace handlers!
void VGA_VRetrace(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	Sequencer->yres = VGA->CRTC.y; //Update Y resolution!
	VGA->CRTC.y = 0; //Reset destination row!
}

void VGA_HRetrace(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	Sequencer->xres = VGA->CRTC.x; //Update X resolution!
	VGA->CRTC.x = 0; //Reset destination column!
}

//All renderers for active display parts:

typedef void (*VGA_Sequencer_Mode)(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo); //Render an active display pixel!

//Blank handler!
void VGA_Blank(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	drawPixel(VGA,RGB(0x00,0x00,0x00)); //Draw blank!
}

extern byte LOG_RENDER_BYTES; //From graphics mode operations!
void VGA_ActiveDisplay_noblanking(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	//Active display!
	//if (LOG_RENDER_BYTES && !Sequencer->Scanline) dolog("VGA","Rendering DAC: %i=%02X; DP:%i",Sequencer->x,attributeinfo->attribute,VGA->precalcs.doublepixels); //Log the rendered DAC index!
	drawPixel(VGA,VGA_DAC(VGA,attributeinfo->attribute)); //Render through the DAC!
}

void VGA_Overscan_noblanking(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	//Overscan!
	drawPixel(VGA,VGA_DAC(VGA,VGA->precalcs.overscancolor)); //Draw overscan!
}

//Active display handler!
void VGA_ActiveDisplay(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	word tempxbackup = Sequencer->tempx;
	//Render our active display here! Start with text mode!		
	static VGA_AttributeInfo attributeinfo; //Our collected attribute info!
	static VGA_Sequencer_Mode activemode[2] = {VGA_Sequencer_TextMode,VGA_Sequencer_GraphicsMode}; //Our display modes!
	word activex; //Active X!
	othernibble:
	Sequencer->activex = Sequencer->tempx++; //Active X!
	activemode[VGA->precalcs.graphicsmode](VGA,Sequencer,&attributeinfo); //Get the color to render!
	if (VGA_AttributeController(&attributeinfo,VGA,Sequencer)) goto othernibble; //Apply the attribute through the attribute controller!
	static VGA_Sequencer_Mode activedisplayhandlers[2] = {VGA_ActiveDisplay_noblanking,VGA_Blank}; //For giving the correct output sub-level!
	activedisplayhandlers[blanking](VGA,Sequencer,&attributeinfo); //Blank or active display!
	if (VGA->precalcs.doublepixels)
	{
		Sequencer->doublepixels = !Sequencer->doublepixels;
		if (Sequencer->doublepixels)
		{
			Sequencer->tempx = tempxbackup; //Draw same pixel twice!
		}
	}
}
//Overscan handler!
void VGA_Overscan(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	static VGA_Sequencer_Mode activemode[2] = {VGA_Overscan_noblanking,VGA_Blank};
	activemode[blanking](VGA,Sequencer,NULL); //Attribute info isn't used!
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
	blanking = hblank|vblank; //Process vblank!
	
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
	
	
	retracing = hretrace|vretrace; //We're retracing?
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
	
	totalretracing = totalling;
	totalretracing <<= 1; //1 bit needed more!
	totalretracing |= retracing; //Are we retracing?
}

//Combination functions of the above:

//Horizontal before vertical, retrace before total.

typedef void (*DisplaySignalHandler)(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal);

//displayrenderhandler[total][retrace][signal]
DisplayRenderHandler displayrenderhandler[4][0x10000]; //Our handlers for all pixels!
DisplaySignalHandler displaysignalhandler[0x10000]; //Our rendering handlers! Executed before all states!

//Initialise all handlers!
void initStateHandlers()
{
	uint_32 i;
	//Default uninitialised entries!
	displayrenderhandler[0][0] = &VGA_NOP; //Default: no action!
	displayrenderhandler[1][0] = &VGA_NOP; //Default: no action!
	displayrenderhandler[2][0] = &VGA_NOP; //Default: no action!
	displayrenderhandler[3][0] = &VGA_NOP; //Default: no action!

	displaysignalhandler[0] = &VGA_SIGNAL_NOPQ; //No signal at all: simply abort!
	
	for (i=1;i<0x10000;i++) //Fill the normal entries!
	{
		//Total handler for total handlers!
		displayrenderhandler[1][i] = &VGA_NOPT; //Do nothing when disabled: retrace does no output!
		displayrenderhandler[2][i] = &VGA_NOPT; //Do nothing when disabled: total handler!
		displayrenderhandler[3][i] = &VGA_NOPT; //Do nothing when disabled: total&retrace handler!
		
		//Rendering handler without retrace AND total!
		displayrenderhandler[0][i] = (((i&VGA_DISPLAYMASK)==VGA_DISPLAYACTIVE)?&VGA_ActiveDisplay:&VGA_Overscan); //Not retracing or any total handler = display/overscan!
		
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
	
	for (;;) //New CRTC constrolled way!
	{
		displaystate = get_display(VGA,Sequencer->Scanline,Sequencer->x++); //Current display state!
		displaysignalhandler[displaystate](Sequencer,VGA,displaystate); //Handle any change in display state first!
		displayrenderhandler[totalretracing][displaystate](Sequencer,VGA); //Execute our signal!
		if (Sequencer_Break) return; //Abort when done!
	}
}