#define VGA_SEQUENCER

#include "headers/types.h" //Basic types!
#include "headers/emu/gpu/gpu.h" //GPU!
#include "headers/hardware/vga/vga.h" //VGA!
#include "headers/hardware/vga/vga_sequencer.h" //Ourselves!
#include "headers/hardware/vga/vga_sequencer_graphicsmode.h" //Text mode!
#include "headers/hardware/vga/vga_sequencer_textmode.h" //Text mode!
#include "headers/hardware/vga/vga_attributecontroller.h" //Attribute controller!
#include "headers/hardware/vga/vga_crtcontroller.h" //CRT Controller for finishing up!
#include "headers/hardware/vga/vga_dacrenderer.h" //DAC support!
#include "headers/hardware/pic.h" //IRQ support!
#include "headers/cpu/interrupts.h" //For get/putpixel variant!
#include "headers/support/highrestimer.h" //High resolution clock!
#include "headers/support/zalloc.h" //Memory protection support!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/vga/vga_vram.h" //VGA VRAM support!

#include "headers/emu/gpu/gpu_text.h" //Text support!

word signal_x, signal_scanline; //Signal location!

//Are we disabled?
#define HW_DISABLED 0

#define CURRENTBLINK(VGA) VGA->TextBlinkOn

#define PIXELBLOCKSIZE 1024

typedef uint_32(*DAC_monitor)(VGA_Type *VGA, byte DACValue); //Monitor handler!
extern byte DAC_whatBWMonitor; //Default: color monitor!
OPTINLINE uint_32 VGA_DAC(VGA_Type *VGA, byte DACValue) //Originally: VGA_Type *VGA, word x
{
	static const DAC_monitor monitors[2] = { DAC_colorMonitor, DAC_BWmonitor }; //What kind of monitor?
	return monitors[DAC_whatBWMonitor](VGA, DACValue); //Do color mode or B/W mode!
}

extern GPU_type GPU; //GPU!

typedef void (*DisplayRenderHandler)(SEQ_DATA *Sequencer, VGA_Type *VGA); //Our rendering handler for all signals!

float VGA_VerticalRefreshRate(VGA_Type *VGA) //Scanline speed for one line in Hz!
{
//Horizontal Refresh Rate=Clock Frequency (in Hz)/horizontal pixels
//Vertical Refresh rate=Horizontal Refresh Rate/total scan lines!
	if (!memprotect(VGA,sizeof(*VGA),NULL)) //No VGA?
	{
		return 0.0f; //Remove VGA Scanline counter: nothing to render!
	}
	
	float result;
	switch (VGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.ClockSelect)
	{
	case 0: //25 MHz clock?
		result = 25175000.0; //25MHz clock!
		break;
	case 1: //28 MHz clock?
		result = 28322000.0; //28 MHz clock!
		break;
	default: //Unknown clock?
		result = 0.0f; //Nothing!
		break;
	}

	return (result/PIXELBLOCKSIZE); //Calculate the ammount of horizontal clocks per second!
}

//Main rendering routine: renders pixels to the emulated screen.

OPTINLINE void drawPixel(VGA_Type *VGA, uint_32 pixel)
{
	register uint_32 old;
	uint_32 *screenpixel = &EMU_BUFFER(VGA->CRTC.x,VGA->CRTC.y); //Pointer to our pixel!
	if (screenpixel>=&EMU_BUFFER(EMU_MAX_X,EMU_MAX_Y)) return; //Out of bounds?
	old = *screenpixel; //Read old!
	old ^= pixel; //Check for differences!
	GPU.emu_buffer_dirty |= old; //Update, set changed bits when changed!
	*screenpixel = pixel; //Update whether it's needed or not!
}

OPTINLINE void VGA_Sequencer_calcScanlineData(VGA_Type *VGA) //Recalcs all scanline data for the sequencer!
{
	//First, all our variables!
	uint_32 bytepanning;
	byte allow_pixelshiftcount;
	
	SEQ_DATA *Sequencer;
	Sequencer = GETSEQUENCER(VGA); //Our sequencer!

	//Determine panning
	bytepanning = VGA->precalcs.PresetRowScanRegister_BytePanning; //Byte panning for Start Address Register for characters or 0,0 pixel!

	//Determine shifts and reset the start map if needed!
	byte reset_startmap;
	reset_startmap = 0; //Default: don't reset!
	
	allow_pixelshiftcount = 1; //Allow by default!
	if (Sequencer->Scanline>VGA->precalcs.topwindowstart) //Top window reached?
	{
		reset_startmap = 1; //Enforce start of map to 0 for the top window!
		if (VGA->precalcs.AttributeModeControlRegister_PixelPanningMode)
		{
			bytepanning = 0; //Act like no byte panning is enabled!
			allow_pixelshiftcount = 0; //Don't allow it anymore!
		}
	}

	Sequencer->startmap = VGA->precalcs.startaddress[reset_startmap]; //What start address to use?
	
	//Determine byte panning and pixel shift count!
	Sequencer->bytepanning = bytepanning; //Pass!

	if (allow_pixelshiftcount) //Allow pixel shift count to be applied?
	{
		Sequencer->pixelshiftcount = VGA->precalcs.pixelshiftcount; //Allowable pixel shift count!
		Sequencer->presetrowscan = VGA->precalcs.presetrowscan; //Preset row scan!
	}
	else
	{
		Sequencer->pixelshiftcount = Sequencer->presetrowscan = 0; //Nothing to shift!
	}
}

typedef void (*Sequencer_pixelhandler)(VGA_Type *VGA,VGA_AttributeInfo *Sequencer_Attributeinfo, word tempx,word tempy,word x,word Scanline,uint_32 bytepanning); //Pixel(s) handler!

byte planesbuffer[4]; //All read planes for the current processing!

typedef void (*VGA_Sequencer_planedecoder)(VGA_Type *VGA, word loadedlocation);

VGA_AttributeInfo attributeinfo; //Our current collected attribute info!

OPTINLINE void VGA_loadcharacterplanes(VGA_Type *VGA, SEQ_DATA *Sequencer, word x) //Load the planes!
{
	register word loadedlocation; //The location we load at!
	//Horizontal logic
	VGA_Sequencer_planedecoder planesdecoder[2] = { VGA_TextDecoder, VGA_GraphicsDecoder }; //Use the correct decoder!
	loadedlocation = x; //X!
	loadedlocation &= 0xFFF; //Limit!
	if (VGA->precalcs.graphicsmode) //Graphics mode?
	{
		loadedlocation >>= 3; //We take portions of 8 pixels, so increase our location every 8 pixels!
	}
	else //Gotten character width? Just here for safety!
	{
		loadedlocation <<= 1; //Multiply by 2 for our index!
		loadedlocation = VGA->CRTC.charcolstatus[loadedlocation]; //Divide by character width in text mode to get the correct character by using the horizontal clock!
	}

	loadedlocation >>= VGA->precalcs.characterclockshift; //Apply VGA shift: the shift is the ammount to move at a time!
	loadedlocation <<= VGA->precalcs.BWDModeShift; //Apply byte/word/doubleword mode at the character level!

	//Row logic
	loadedlocation += Sequencer->charystart; //Apply the line and start map to retrieve!

	//Now calculate and give the planes to be used!
	planesbuffer[0] = readVRAMplane(VGA, 0, loadedlocation, 0x81); //Read plane 0!
	planesbuffer[1] = readVRAMplane(VGA, 1, loadedlocation, 0x81); //Read plane 1!
	planesbuffer[2] = readVRAMplane(VGA, 2, loadedlocation, 0x81); //Read plane 2!
	planesbuffer[3] = readVRAMplane(VGA, 3, loadedlocation, 0x81); //Read plane 3!
	//Now the buffer is ready to be processed into pixels!

	planesdecoder[VGA->precalcs.graphicsmode](VGA,loadedlocation); //Use the decoder to get the pixels or characters!

	register byte lookupprecalcs;
	lookupprecalcs = (byte)((SEQ_DATA *)Sequencer)->charinner_y;
	lookupprecalcs <<= 1; //Make room!
	lookupprecalcs |= CURRENTBLINK(VGA); //Blink!
	lookupprecalcs <<= 1; //Make room for the pixelon!
	attributeinfo.lookupprecalcs = lookupprecalcs; //Save the looked up precalcs, this never changes during a processed block of pixels (both text and graphics modes)!
}

OPTINLINE void VGA_Sequencer_updateRow(VGA_Type *VGA, SEQ_DATA *Sequencer)
{
	register word row;
	register uint_32 charystart;
	row = Sequencer->Scanline; //Default: our normal scanline!
	if (row>VGA->precalcs.topwindowstart) //Splitscreen operations?
	{
		row -= VGA->precalcs.topwindowstart; //This starts after the row specified, at row #0!
		--row; //We start at row #0, not row #1(1 after topwindowstart).
	}
	row >>= VGA->precalcs.scandoubling; //Apply Scan Doubling on the row scan counter: we take effect on content (double scanning)!
	Sequencer->rowscancounter = row; //Set the current row scan counter!

	row >>= VGA->precalcs.CRTCModeControlRegister_SLDIV; //Apply scanline division to the current row timing!
	row <<= 1; //We're always a multiple of 2 by index into charrowstatus!

	//Row now is an index into charrowstatus
	word *currowstatus = &VGA->CRTC.charrowstatus[row]; //Current row status!
	Sequencer->chary = row = *currowstatus++; //First is chary (effective character/graphics row)!
	Sequencer->charinner_y = *currowstatus; //Second is charinner_y!

	charystart = getVRAMScanlineStart(VGA, row); //Calculate row start!
	charystart += Sequencer->startmap; //Calculate the start of the map while we're at it: it's faster this way!
	charystart += Sequencer->bytepanning; //Apply byte panning!
	Sequencer->charystart = charystart; //What row to start with our pixels!

	//Some attribute controller special 8-bit mode support!
	Sequencer->active_pixelrate = 0; //Reset pixel load rate status for odd sized screens.
	Sequencer->active_nibblerate = 0; //Reset nibble load rate status for odd sized screens.

	VGA_loadcharacterplanes(VGA, Sequencer, 0); //Initialise the character planes for usage!
}

byte Sequencer_run; //Sequencer breaked (loop exit)?

//Special states!
byte blanking = 0; //Are we blanking!
byte retracing = 0; //Allow rendering by retrace!
byte totalling = 0; //Allow rendering by total!
byte totalretracing = 0; //Combined flags of retracing/totalling!

byte hblank = 0, hretrace = 0; //Horizontal blanking/retrace?
byte vblank = 0, vretrace = 0; //Vertical blanking/retrace?
word blankretraceendpending = 0; //Ending blank/retrace pending? bits set for any of them!

typedef void (*DisplaySignalHandler)(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal);

byte VGA_LOGPRECALCS = 0; //Log precalcs?

//displayrenderhandler[total][retrace][signal]
DisplayRenderHandler displayrenderhandler[4][0x10000]; //Our handlers for all pixels!
DisplaySignalHandler displaysignalhandler[0x10000]; //Our rendering handlers! Executed before all states!

void VGA_NOP(SEQ_DATA *Sequencer, VGA_Type *VGA) //NOP for pixels!
{}

//Total handlers!
OPTINLINE void VGA_VTotal(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//First, render ourselves to the screen!
	GPU.xres = Sequencer->xres; //Apply x resolution!
	GPU.yres = Sequencer->yres; //Apply y resolution!
	unlockGPU(); //Unlock the GPU!
	VGA_VBlankHandler(VGA); //Handle all VBlank stuff!
	lockGPU(); //Lock the GPU again! We're using it again!
	
	Sequencer->Scanline = 0; //Reset for the next frame!
	Sequencer->yres = 0; //Reset Y resolution next frame if not specified (like a real screen)!
	Sequencer->xres = 0; //Reset X resolution next frame if not specified (like a real screen)!
	
	VGA_Sequencer_updateRow(VGA, Sequencer); //Scanline has been changed!
}

OPTINLINE void VGA_HTotal(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//Process HBlank: reload display data for the next scanline!
	//Sequencer itself
	Sequencer->x = 0; //Reset for the next scanline!
	++Sequencer->Scanline; //Next scanline to process!
	
	//CRT
	if (!vretrace) //Not retracing vertically?
	{
		++VGA->CRTC.y; //Next row on-screen!
	}
	
	//Sequencer rendering data
	Sequencer->tempx = 0; //Reset the rendering position from the framebuffer!
	VGA_Sequencer_calcScanlineData(VGA);
	VGA_Sequencer_updateRow(VGA, Sequencer); //Scanline has been changed!
}

//Retrace handlers!
OPTINLINE void VGA_VRetrace(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	Sequencer->yres = VGA->CRTC.y; //Update Y resolution!
	VGA->CRTC.y = 0; //Reset destination row!
}

OPTINLINE void VGA_HRetrace(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	Sequencer->xres = VGA->CRTC.x; //Update X resolution!
	VGA->CRTC.x = 0; //Reset destination column!
}

//All renderers for active display parts:

typedef void (*VGA_Sequencer_Mode)(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo); //Render an active display pixel!

//Blank handler!
void VGA_Blank(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	if (!hretrace)
	{
		drawPixel(VGA, RGB(0x00, 0x00, 0x00)); //Draw blank!
		++VGA->CRTC.x; //Next x!
	}
}

void VGA_ActiveDisplay_noblanking(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	if (!hretrace)
	{
		//Active display!
		drawPixel(VGA, VGA_DAC(VGA, attributeinfo->attribute)); //Render through the DAC!
		++VGA->CRTC.x; //Next x!
	}
}

void VGA_Overscan_noblanking(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	if (!hretrace)
	{
		//Overscan!
		drawPixel(VGA, VGA_DAC(VGA, VGA->precalcs.overscancolor)); //Draw overscan!
		++VGA->CRTC.x; //Next x!
	}
}

//Active display handler!
void VGA_ActiveDisplay(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	byte loadcharacterplanes;
	//Render our active display here! Start with text mode!		
	static VGA_Sequencer_Mode activemode[2] = {VGA_Sequencer_TextMode,VGA_Sequencer_GraphicsMode}; //Our display modes!
	static VGA_Sequencer_Mode activedisplayhandlers[2] = {VGA_ActiveDisplay_noblanking,VGA_Blank}; //For giving the correct output sub-level!
	byte nibbled=0; //Did we process two nibbles instead of one nibble?
	word tempx = Sequencer->tempx; //Load tempx!

	othernibble: //Retrieve the current DAC index!
	Sequencer->activex = tempx++; //Active X!
	activemode[VGA->precalcs.graphicsmode](VGA,Sequencer,&attributeinfo); //Get the color to render!
	if (VGA_AttributeController(&attributeinfo,VGA))
	{
		nibbled = 1; //We're processing 2 nibbles instead of 1 nibble!
		goto othernibble; //Apply the attribute through the attribute controller!
	}

	activedisplayhandlers[blanking](VGA,Sequencer,&attributeinfo); //Blank or active display!

	if (++Sequencer->active_pixelrate > VGA->precalcs.ClockingModeRegister_DCR) //To write back the pixel clock every or every other pixel?
	{
		Sequencer->active_pixelrate = 0; //Reset for the new block!
		if (nibbled) //We've nibbled?
		{
			if (++Sequencer->active_nibblerate>1) //Nibble expired?
			{
				Sequencer->active_nibblerate = 0; //Reset for the new block!
				nibbled = 0; //We're done with the pixel!
			}
		}
		if (!nibbled) //Finished with the nibble&pixel? We're ready to check for the next one!
		{
			if (VGA->precalcs.graphicsmode) //Graphics mode?
			{
				loadcharacterplanes = ((tempx & 7)==0); //Load the character planes?
			}
			else //Text mode?
			{
				loadcharacterplanes = (VGA->CRTC.charcolstatus[(Sequencer->tempx<<1)] != VGA->CRTC.charcolstatus[(tempx<<1)]); //Load the character planes?
			}
			if (loadcharacterplanes) //First of a new block? Reload our pixel buffer!
			{
				VGA_loadcharacterplanes(VGA, Sequencer, tempx); //Load data from the graphics planes!
			}
			Sequencer->tempx = tempx; //Write back tempx!
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

OPTINLINE void VGA_SIGNAL_HANDLER(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal)
{
	//Blankings
	if (signal&VGA_SIGNAL_HBLANKSTART) //HBlank start?
	{
		hblank = 1; //We're blanking!
	}
	else if (hblank)
	{
		if (signal&VGA_SIGNAL_HBLANKEND) //HBlank end?
		{
			blankretraceendpending |= VGA_SIGNAL_HBLANKEND;
		}
		else if (blankretraceendpending&VGA_SIGNAL_HBLANKEND) //End pending HBlank!
		{
			hblank = 0; //We're not blanking anymore!
			blankretraceendpending &= ~VGA_SIGNAL_HBLANKEND; //Remove from flags pending!
		}
	}

	if (signal&VGA_SIGNAL_VBLANKSTART) //VBlank start?
	{
		vblank = 1; //We're blanking!
	}
	else if (vblank)
	{
		if (signal&VGA_SIGNAL_VBLANKEND) //VBlank end?
		{
			blankretraceendpending |= VGA_SIGNAL_VBLANKEND;
		}
		else if (blankretraceendpending&VGA_SIGNAL_VBLANKEND) //End pending HBlank!
		{
			vblank = 0; //We're not blanking anymore!
			blankretraceendpending &= ~VGA_SIGNAL_VBLANKEND; //Remove from flags pending!
		}
	}
	
	//Both H&VBlank count!
	blanking = hblank;
	blanking |= vblank; //Process blank!
	
	//Retraces
	if (signal&VGA_SIGNAL_HRETRACESTART) //HRetrace start?
	{
		if (!hretrace) //Not running yet?
		{
			VGA_HRetrace(Sequencer, VGA); //Execute the handler!
		}
		hretrace = 1; //We're retracing!
	}
	else if (hretrace)
	{
		if (signal&VGA_SIGNAL_HRETRACEEND) //HRetrace end?
		{
			hretrace = 0; //We're not retraing anymore!
		}
	}

	if (signal&VGA_SIGNAL_VRETRACESTART) //VRetrace start?
	{
		if (!vretrace) //Not running yet?
		{
			VGA_VRetrace(Sequencer, VGA); //Execute the handler!
		}
		vretrace = 1; //We're retracing!
	}
	else if (vretrace)
	{
		if (signal&VGA_SIGNAL_VRETRACEEND) //VRetrace end?
		{
			vretrace = 0; //We're not retracing anymore!
		}
	}
	
	register byte isretrace;
	isretrace = hretrace;
	isretrace |= vretrace; //We're retracing?
	//Retracing disables output!

	VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.VRetrace = vretrace; //Vertical retrace?
	VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.DisplayDisabled = retracing = isretrace; //Vertical or horizontal retrace?

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

//Initialise all handlers!
void initStateHandlers()
{
	uint_32 i;
	//Default uninitialised entries!
	displayrenderhandler[0][0] = &VGA_NOP; //Default: no action!
	displayrenderhandler[1][0] = &VGA_NOP; //Default: no action!
	displayrenderhandler[2][0] = &VGA_NOP; //Default: no action!
	displayrenderhandler[3][0] = &VGA_NOP; //Default: no action!

	for (i=1;i<0x10000;i++) //Fill the normal entries!
	{
		//Total handler for total handlers!
		displayrenderhandler[1][i] = &VGA_NOP; //Do nothing when disabled: retrace does no output!
		displayrenderhandler[2][i] = &VGA_NOP; //Do nothing when disabled: total handler!
		displayrenderhandler[3][i] = &VGA_NOP; //Do nothing when disabled: total&retrace handler!
		
		//Rendering handler without retrace AND total!
		displayrenderhandler[0][i] = ((i&VGA_DISPLAYMASK)==VGA_DISPLAYACTIVE)?&VGA_ActiveDisplay:&VGA_Overscan; //Not retracing or any total handler = display/overscan!
	}
}

void VGA_Sequencer()
{
	if (HW_DISABLED) return;
	if (!lockVGA()) return; //Lock ourselves!
	static word displaystate = 0; //Last display state!
	VGA_Type *VGA = getActiveVGA(); //Our active VGA!
	if (!memprotect(VGA, sizeof(*VGA), "VGA_Struct")) //Invalid VGA? Don't do anything!
	{
		unlockVGA();
		return; //Abort: we're disabled!
	}

	SEQ_DATA *Sequencer;
	Sequencer = GETSEQUENCER(VGA); //Our sequencer!

	//All possible states!
	if (!displayrenderhandler[0][0]) //Nothing set?
	{
		initStateHandlers(); //Init our display states for usage!
	}

	if (!lockGPU()) //Lock the GPU for our access!
	{
		unlockVGA();
		return;
	}
	if (!memprotect(GPU.emu_screenbuffer, 4, "EMU_ScreenBuffer")) //Invalid framebuffer? Don't do anything!
	{
		unlockVGA();
		unlockGPU(); //Unlock the VGA&GPU for Software access!
		return; //Abort: we're disabled!
	}

	uint_32 i = PIXELBLOCKSIZE+1; //Process 1024000 pixels at a time!
	Sequencer_run = 1; //We're running!
	for (; --i && Sequencer_run;)
	{
		//Process one pixel only!
		signal_x = Sequencer->x;
		signal_scanline = Sequencer->Scanline;
		displaystate = get_display(VGA, Sequencer->Scanline, Sequencer->x++); //Current display state!
		VGA_SIGNAL_HANDLER(Sequencer, VGA, displaystate); //Handle any change in display state first!
		displayrenderhandler[totalretracing][displaystate](Sequencer, VGA); //Execute our signal!
	}

	unlockVGA(); //Unlock the VGA for Software access!
	unlockGPU(); //Unlock the GPU for Software access!
}