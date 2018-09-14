#define VGA_RENDERER

#include "headers/types.h" //Basic types!
#include "headers/hardware/vga/vga.h" //VGA!
#include "headers/hardware/vga/vga_renderer.h" //Ourselves!
#include "headers/hardware/vga/vga_sequencer_graphicsmode.h" //Text mode!
#include "headers/hardware/vga/vga_sequencer_textmode.h" //Text mode!
#include "headers/hardware/vga/vga_attributecontroller.h" //Attribute controller!
#include "headers/hardware/vga/vga_crtcontroller.h" //CRT Controller for finishing up!
#include "headers/hardware/vga/vga_dacrenderer.h" //DAC support!
#include "headers/hardware/vga/vga_vram.h" //VGA VRAM support!
#include "headers/hardware/vga/vga_cga_mda.h" //CGA/MDA support!
#include "headers/hardware/vga/vga_cga_ntsc.h" //CGA NTSC support!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/vga/svga/tseng.h" //ET3/4K DWord mode support!
#include "headers/support/zalloc.h" //Memory protection support for vertical refresh rate!

//Basic timings support(originally emu_VGA.c)
#include "headers/emu/gpu/gpu.h" //Basic GPU!
#include "headers/emu/gpu/gpu_renderer.h" //Renderer support!
#include "headers/emu/gpu/gpu_text.h" //Emulator support!
#include "headers/emu/gpu/gpu_framerate.h" //Framerate support!
#include "headers/hardware/pic.h" //IRQ support!
#include "headers/support/highrestimer.h" //Automatic timer support!
#include "headers/cpu/cpu.h" //Currently emulated CPU for wait states!

//Are we disabled?
#define HW_DISABLED 0

//Limit the VGA to run slower on too slow PCs? Check at least this many pixels if defined before locking on the speed!
#define LIMITVGA 1000

#define CURRENTBLINK(VGA) VGA->blink32

//Do color mode or B/W mode DAC according to our settings!
#define VGA_DAC(VGA,DACValue) (VGA->precalcs.effectiveDAC[(DACValue)])

extern GPU_type GPU; //GPU!

DOUBLE VGA_clocks[4] = {
			0.0, //25MHz: VGA standard clock
			0.0, //28MHz: VGA standard clock
			0.0, //external clock: not connected!
			0.0 //Unused
			}; //Our clocks!

uint_32 CGALineSize = 0; //How long is our line!
byte CGALineBuffer[2048]; //Full CGA scanline buffer!
uint_32 CGAOutputBuffer[2048]; //Full CGA NTSC buffer!

VGA_clockrateextensionhandler VGA_calcclockrateextensionhandler; //The clock rate extension handler!

/*

Renderer mini-optimizations.

*/

DOUBLE oldrate = 0.0f; //The old rate we're using!

DOUBLE VGA_timing = 0.0; //No timing yet!
DOUBLE VGA_debugtiming = 0.0; //Debug countdown if applyable!
byte VGA_debugtiming_enabled = 0; //Are we applying right now?
float VGA_rendertiming = 0.0f; //Time for the renderer to tick!

TicksHolder VGA_test;
float VGA_limit = 0.0f; //Our speed factor!

#ifdef LIMITVGA
uint_32 passedcounter = LIMITVGA; //Times to check for speed with LIMITVGA
#endif

byte VGA_vtotal = 0; //Are we detecting VTotal?

byte currentVGASpeed = 0; //Default: run at 100%!
byte SynchronizationMode = 0; //Synchronization mode when used: 0=Old style, 1=New style

/*

Basic renderer functionality

*/

void initVGAclocks(byte extension)
{
	if (extension!=3) //VGA clock?
	{
		VGA_clocks[0] = VGA25MHZ; //25MHZ clock!
		VGA_clocks[1] = VGA28MHZ; //28MHZ clock!
	}
	else //EGA clock?
	{
		VGA_clocks[0] = MHZ14; //14MHz clock!
		VGA_clocks[1] = 16257000.0f; //16MHz clock
	}
	VGA_clocks[2] = 0.0; //Unused!
	VGA_clocks[3] = 0.0; //Unused!
}

DOUBLE VGA_VerticalRefreshRate(VGA_Type *VGA) //Scanline speed for one line in Hz!
{
	DOUBLE result=0.0;
	byte clock;
	//Horizontal Refresh Rate=Clock Frequency (in Hz)/horizontal pixels
	//Vertical Refresh rate=Horizontal Refresh Rate/total scan lines!
	if (unlikely(memprotect(VGA,sizeof(*VGA),NULL)==0)) //No VGA?
	{
		return 0.0; //Remove VGA Scanline counter: nothing to render!
	}
	if (unlikely(VGA_calcclockrateextensionhandler))
	{
		if (unlikely(VGA_calcclockrateextensionhandler(VGA)!=0.0)) return VGA_calcclockrateextensionhandler(VGA); //Give the extended clock if needed!
		else result = VGA_clocks[(GETBITS(VGA->registers->ExternalRegisters.MISCOUTPUTREGISTER,2,3) & 3)]; //VGA clock!
	}
	else
	{
		clock = (GETBITS(VGA->registers->ExternalRegisters.MISCOUTPUTREGISTER,2,3)&3); //What clock is specified?
		result = VGA_clocks[clock]; //VGA clock, if used!
		VGA->precalcs.use14MHzclock = (((VGA->enable_SVGA==3) && (clock==0)) || (VGA->enable_SVGA==4)); //Use 14MHz motherboard clock when supplied!
	}
	return result; //Give the result!
}

//Main rendering routine: renders pixels to the emulated screen.

//Information gotten from the GPU!
extern int_32 lightpen_x, lightpen_y; //Current lightpen location, if any!
extern byte lightpen_pressed; //Lightpen pressed?

//Our internal information for determining the lightpen for the currently emulated video card!
word lightpen_currentvramlocation; //Current VRAM location for light pen detection!

void EGA_checklightpen(word currentlocation, byte is_lightpenlocation, byte is_lightpenpressed) //Check the lightpen on the current location!
{
	INLINEREGISTER word lightpenlocation;
	if (getActiveVGA()->enable_SVGA==3) //EGA is emulated?
	{
		if (((getActiveVGA()->registers->EGA_lightpenstrobeswitch&3)==1) || (is_lightpenlocation && ((getActiveVGA()->registers->EGA_lightpenstrobeswitch&2)==0))) //Light pen preset and strobing? Are we the light pen location?
		{
			getActiveVGA()->registers->EGA_lightpenstrobeswitch &= ~1; //Clear the preset: we're not set anymore!
			getActiveVGA()->registers->EGA_lightpenstrobeswitch |= 2; //The light pen register is now set!
			lightpenlocation = currentlocation; //Load the current location for converting to CGA location!
			//Now set our lightpen location!
			getActiveVGA()->registers->lightpen_high = ((lightpenlocation>>8)&0xFF); //Our high bits!
			getActiveVGA()->registers->lightpen_low = (lightpenlocation&0xFF); //Our low bits!
		}
		//Always update the CGA lightpen button(live state)!
		getActiveVGA()->registers->EGA_lightpenstrobeswitch &= ~4; //Default: clear the pressed switch indicator: we're depressed!
		getActiveVGA()->registers->EGA_lightpenstrobeswitch |= ((is_lightpenpressed&1)<<2); //Set if we're switched or not!
	}
}

OPTINLINE void drawPixel_real(uint_32 pixel, uint_32 x, uint_32 y) //Manual version for CGA conversion!
{
	INLINEREGISTER uint_32 *screenpixel = &EMU_BUFFER(x,y); //Pointer to our pixel!
	if ((screenpixel>=EMU_SCREENBUFFEREND) || (x>=EMU_MAX_X)) return; //Out of bounds?
	//Apply light pen, directly connected to us!
	if (unlikely((*screenpixel)!=pixel)) //Are we to update the changed pixel?
	{
		*screenpixel = pixel; //Update whether it's needed or not!
		GPU.emu_buffer_dirty = 1; //Update, set changed bits when changed!
	}
}

OPTINLINE void drawPixel(VGA_Type *VGA, uint_32 pixel) //Normal VGA version!
{
	drawPixel_real(pixel,VGA->CRTC.x,VGA->CRTC.y); //Draw our pixel on the display!
}

byte MDAcolors[4] = {0x00,0x81,0xC0,0xFF}; //All 4 MDA colours according to http://www.seasip.info/VintagePC/mda.html, as MDA greyscale indexes!

extern byte CGA_RGB; //Are we a RGB monitor(1) or Composite monitor(0)?

OPTINLINE void drawCGALine(VGA_Type *VGA) //Draw the current CGA line to display!
{
	INLINEREGISTER uint_32 drawx;
	if (CGALineSize>2048) CGALineSize = 2048; //Limit to what we have available!
	if (VGA->registers->specialMDAflags&1) //MDA rendering mode?
	{
		INLINEREGISTER byte data; //The current entry to draw!
		INLINEREGISTER uint_32 color; //The full color to draw!
		INLINEREGISTER byte *bufferpos, *finalpos; //The current and end position to draw!
		if (unlikely(CGALineSize==0)) return; //Abort if nothing to render!
		finalpos = &CGALineBuffer[CGALineSize]; //End of the output buffer to process!
		bufferpos = &CGALineBuffer[0]; //First pixel to render!
		drawx = 0; //Start index to draw at!
		for (;;) //Process all pixels!
		{
			data = *bufferpos; //Load the current pixel!
			data &= 3; //Only 2 bits are used for the MDA!
			data = MDAcolors[data]; //Translate the pixel to proper DAC indexes!
			color = VGA->precalcs.effectiveMDADAC[data]; //Look up the MDA DAC color to use(translate to RGB)!
			drawPixel_real(color,drawx,VGA->CRTC.y); //Render the pixel as MDA colors through the B/W DAC!
			++bufferpos; //Next pixel!
			if (unlikely(bufferpos == finalpos)) break; //Stop processing when finished!
			++drawx; //Next line index!
		}
	}
	else //CGA mode?
	{
		INLINEREGISTER uint_32 *bufferpos, *finalpos;
		if (unlikely(CGALineSize==0)) return; //Abort if nothing to render!
		finalpos = &CGAOutputBuffer[CGALineSize]; //End of the output buffer to process!
		bufferpos = &CGAOutputBuffer[0]; //First pixel to render!
		RENDER_convertCGAOutput(&CGALineBuffer[0], &CGAOutputBuffer[0], CGALineSize); //Convert the CGA line to RGB output!
		drawx = 0; //Start index to draw at!
		for (;;) //Render all pixels!
		{
			drawPixel_real(*bufferpos,drawx,VGA->CRTC.y); //Render the converted CGA output signal!
			if (unlikely(++bufferpos==finalpos)) break; //Stop processing when finished!
			++drawx; //Next line index!
		}
	}
}

void VGA_Sequencer_calcScanlineData(VGA_Type *VGA) //Recalcs all scanline data for the sequencer!
{
	//First, all our variables!
	uint_32 bytepanning;
	byte allow_pixelshiftcount;
	
	SEQ_DATA *Sequencer;
	Sequencer = GETSEQUENCER(VGA); //Our sequencer!

	//Determine panning
	bytepanning = VGA->precalcs.PresetRowScanRegister_BytePanning; //Byte panning for Start Address Register for characters or 0,0 pixel!

	//Determine shifts and reset the start map if needed!
	allow_pixelshiftcount = 1; //Allow by default!
	if (Sequencer->Scanline>=VGA->precalcs.topwindowstart) //Top window reached?
	{
		if (unlikely(Sequencer->Scanline==VGA->precalcs.topwindowstart)) //Start of the top window?
		{
			Sequencer->startmap = 0; //What start address to use? Start at the top of VRAM!
		}
		//Enforce start of map to beginning in VRAM for the top window!
		if (VGA->precalcs.AttributeModeControlRegister_PixelPanningMode)
		{
			bytepanning = 0; //Act like no byte panning is enabled!
			allow_pixelshiftcount = 0; //Don't allow it anymore!
		}
	}

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

LOADEDPLANESCONTAINER loadedplanes; //All four loaded planes!

typedef void (*VGA_Sequencer_planedecoder)(VGA_Type *VGA, word loadedlocation);

OPTINLINE uint_32 patch_map1314(VGA_Type *VGA, uint_32 addresscounter) //Patch full VRAM address!
{ //Check this!
	INLINEREGISTER uint_32 bit; //Load row scan counter!
	if (GETBITS(VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER,0,1)==0) //a13=Bit 0 of the row scan counter!
	{
		//Row scan counter bit 1 is placed on the memory bus bit 14 during active display time.
		//Bit 1, placed on memory address bit 14 has the effect of quartering the memory.
		bit = ((SEQ_DATA *)VGA->Sequencer)->rowscancounter; //Current row scan counter!
		bit &= 1; //Bit0 only!
		bit <<= 13; //Shift to our position (bit 13)!
		addresscounter &= ~0x2000; //Clear bit13!
		addresscounter |= bit; //Set bit13 if needed!
	}

	if (GETBITS(VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER,1,1)==0) //a14<=Bit 1 of the row scan counter!
	{
		bit = ((SEQ_DATA *)VGA->Sequencer)->rowscancounter; //Current row scan counter!
		bit &= 2; //Bit1 only!
		bit <<= 13; //Shift to our position (bit 14)!
		addresscounter &= ~0x4000; //Clear bit14;
		addresscounter |= bit; //Set bit14 if needed!
	}

	return addresscounter; //Give the linear address!
}

VGA_addresswrapextensionhandler VGA_calcaddresswrapextensionhandler = NULL; //The DWord shift extension handler!

OPTINLINE uint_32 addresswrap(VGA_Type *VGA, uint_32 memoryaddress) //Wraps memory arround 64k!
{
	INLINEREGISTER uint_32 result, address2;
	if (VGA_calcaddresswrapextensionhandler) return VGA_calcaddresswrapextensionhandler(VGA,memoryaddress); //Apply extension shift method when specified!
	switch (VGA->precalcs.BWDModeShift) //What mode?
	{
		case 1: //Word mode?
			result = 0xD; //Load default location (13)
			result |= (GETBITS(VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER,5,1) << 1); //MA15 instead of MA13 when set!
			address2 = memoryaddress; //Load the address for calculating!
			address2 >>= result; //Apply MA15/MA13 to bit 0!
			address2 &= 1; //Only load bit 0!
			result = memoryaddress; //Default: don't change!
			result <<= 1; //Shift up to create the word mode!
			result |= address2; //Add bit MA15/MA13 at bit 0!
			return result; //Give the result!
		case 2: //DWord mode?
			//Doubleword mode executed normally according to documentation!
			if (getActiveVGA()->enable_SVGA == 0) //VGA?
			{
				return (memoryaddress<<2)|((memoryaddress>>14)&3); //VGA-compatible DWORD addressing!
			}
			break;
		default:
		case 0: //Byte mode?
			//Don't do anything?
			break; //Unchanged!

	}
	return memoryaddress; //Original address in byte mode!
}

VGA_AttributeInfo currentattributeinfo; //Our current collected attribute info!

OPTINLINE void VGA_loadcharacterplanes(VGA_Type *VGA, SEQ_DATA *Sequencer) //Load the planes!
{
	INLINEREGISTER uint_32 vramlocation; //The location we load at!
	//Horizontal logic
	VGA_Sequencer_planedecoder planesdecoder[2] = { VGA_TextDecoder, VGA_GraphicsDecoder }; //Use the correct decoder!

	//Column logic
	vramlocation = Sequencer->memoryaddress; //Load the address to be loaded!
	lightpen_currentvramlocation = vramlocation; //Save the new current location for light pen detection!

	//Column/Row logic
	vramlocation = patch_map1314(VGA, addresswrap(VGA, vramlocation)); //Apply address wrap and MAP13/14?

	//Now calculate and give the planes to be used!
	loadedplanes.loadedplanes = VGA_VRAMDIRECTPLANAR(VGA,vramlocation,0); //Load the 4 planes from VRAM, as an entire DWORD!

	//Now the buffer is ready to be processed into pixels!
	planesdecoder[VGA->precalcs.graphicsmode](VGA,vramlocation); //Use the decoder to get the pixels or characters!

	INLINEREGISTER byte lookupprecalcs;
	lookupprecalcs = ((SEQ_DATA *)Sequencer)->charinner_y;
	lookupprecalcs <<= 1; //Make room!
	lookupprecalcs |= CURRENTBLINK(VGA); //Blink!
	lookupprecalcs <<= 1; //Make room for the pixelon!
	currentattributeinfo.lookupprecalcs = lookupprecalcs; //Save the looked up precalcs, this never changes during a processed block of pixels (both text and graphics modes)!
}

OPTINLINE byte VGA_ActiveDisplay_timing(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	word extrastatus = *Sequencer->extrastatus; //Next status!
	if (unlikely((GETBITS(getActiveVGA()->registers->SequencerRegisters.REGISTERS.RESETREGISTER,1,1) && GETBITS(getActiveVGA()->registers->SequencerRegisters.REGISTERS.RESETREGISTER,0,1))==0)) //Reset sequencer?
	{
		return 0; //Abort: we're disabled!
	}

	if (extrastatus & 2) //Half character clock is to be executed?
	{
		if ((++Sequencer->linearcounterdivider&VGA->precalcs.characterclockshift) == 0) //Increase memory address counter?
		{
			Sequencer->linearcounterdivider = 0; //Reset!
			++Sequencer->memoryaddress; //Increase the memory address counter!
		}

		if (unlikely((++Sequencer->memoryaddressclock&VGA->precalcs.VideoLoadRateMask) == 0)) //Reload data this clock?
		{
			Sequencer->memoryaddressclock = 0; //Reset!
			VGA_loadcharacterplanes(VGA, Sequencer); //Load data from the graphics planes!
		}
	}

	Sequencer->extrastatus += ((extrastatus>>2)&1); //Increase the extra status, when allowed!

	return extrastatus & 1; //Read next pixel?
}

static VGA_AttributeController_Mode attributecontroller_modes[4] = { VGA_AttributeController_4bit, VGA_AttributeController_8bit, VGA_AttributeController_8bit, VGA_AttributeController_16bit }; //Both modes we use!

VGA_AttributeController_Mode attrmode = VGA_AttributeController_4bit; //Default mode!

void updateVGAAttributeController_Mode(VGA_Type *VGA)
{
	if (VGA->precalcs.AttributeController_16bitDAC) //16-bit DAC override active?
	{
		attrmode = attributecontroller_modes[VGA->precalcs.AttributeController_16bitDAC]; //Apply the current mode!
	}
	else //VGA compatibility mode?
	{
		attrmode = attributecontroller_modes[VGA->precalcs.AttributeModeControlRegister_ColorEnable8Bit]; //Apply the current mode according to VGA registers!
	}
}

OPTINLINE byte VGA_AttributeController(VGA_AttributeInfo *Sequencer_attributeinfo, VGA_Type *VGA) //Process attribute to DAC index!
{
	//Originally: VGA_Type *VGA, word Scanline, word x, VGA_AttributeInfo *info

	//Our changing variables that are required!
	return attrmode(Sequencer_attributeinfo, VGA); //Passthrough!
}

OPTINLINE void VGA_Sequencer_updateRow(VGA_Type *VGA, SEQ_DATA *Sequencer)
{
	byte x; //For horizontal shifting/temp storage!
	INLINEREGISTER word row;
	INLINEREGISTER uint_32 charystart;
	row = Sequencer->Scanline; //Default: our normal scanline!
	if (VGA->precalcs.enableInterlacing) //Are we interlacing?
	{
		row >>= 1; //Generate an doubled row in the field!
	}
	if (row>VGA->precalcs.topwindowstart) //Splitscreen operations?
	{
		row -= VGA->precalcs.topwindowstart; //This starts after the row specified, at row #0!
		--row; //We start at row #0, not row #1(1 after topwindowstart).
		Sequencer->is_topwindow = 1; //We're starting the top window rendering!
	}
	else
	{
		Sequencer->is_topwindow = 0; //We're not the top window!
	}

	//row is the vertical timing counter
	row >>= VGA->precalcs.scandoubling; //Apply scan doubling to the row scan counter(inner character row and thus, by extension, the row itself)!
	row >>= VGA->precalcs.CRTCModeControlRegister_SLDIV; //Apply Scan Doubling on the row scan counter: we take effect on content (double scanning)!
	//Apply scanline division to the current row timing!

	row += VGA->precalcs.presetrowscan; //Apply the preset row scan to the scanline!

	row <<= 1; //We're always a multiple of 2 by index into charrowstatus!

	//Row now is an index into charrowstatus
	word *currowstatus = &VGA->CRTC.charrowstatus[row]; //Current row status!
	Sequencer->chary = row = *currowstatus++; //First is chary (effective character/graphics row)!
	Sequencer->rowscancounter = (word)(Sequencer->charinner_y = (byte)*currowstatus); //Second is charinner_y, which is also the row scan counter!

	charystart = VGA->precalcs.rowsize*row; //Calculate row start!
	charystart += Sequencer->startmap; //Calculate the start of the map while we're at it: it's faster this way!
	charystart += Sequencer->bytepanning; //Apply byte panning!
	Sequencer->memoryaddress = Sequencer->charystart = charystart; //What row to start with our pixels! Apply the line and start map to retrieve(start at the new start of the scanline to draw)!

	//Some attribute controller special 8-bit mode support!
	Sequencer->extrastatus = &VGA->CRTC.extrahorizontalstatus[0]; //Start our extra status at the beginning of the row!

	Sequencer->memoryaddressclock = Sequencer->linearcounterdivider = 0; //Address counters are reset!
	currentattributeinfo.latchstatus = 0; //Reset the latches used for rendering!
	VGA_loadcharacterplanes(VGA, Sequencer); //Load data from the first planes!

	//Process any horizontal pixel shift count!
	if (VGA->precalcs.textmode) //Text mode?
	{
		for (x = 0;x < VGA->precalcs.pixelshiftcount;++x) //Process pixel shift count!
		{
			if (VGA_ActiveDisplay_timing(Sequencer, VGA)) //Render the next pixel?
			{
				VGA_Sequencer_TextMode(VGA, Sequencer, &currentattributeinfo); //Get the color to render!
				VGA_AttributeController(&currentattributeinfo, VGA); //Ignore the nibbled/not nibbled result!
			}
		}
	}
	else //Graphics mode?
	{
		for (x = 0;x < VGA->precalcs.pixelshiftcount;++x) //Process pixel shift count!
		{
			if (VGA_ActiveDisplay_timing(Sequencer, VGA)) //Render the next pixel?
			{
				VGA_Sequencer_GraphicsMode(VGA, Sequencer, &currentattributeinfo); //Get the color to render!
				VGA_AttributeController(&currentattributeinfo, VGA); //Ignore the nibbled/not nibbled result!
			}
		}
	}
}

byte Sequencer_run; //Sequencer breaked (loop exit)?

//Special states!
byte blanking = 0; //Are we blanking!
byte retracing = 0; //Allow rendering by retrace!
byte totalling = 0; //Allow rendering by total!

byte hblank = 0, hretrace = 0; //Horizontal blanking/retrace?
byte vblank = 0, vretrace = 0; //Vertical blanking/retrace?

byte VGA_LOGPRECALCS = 0; //Log precalcs?

//displayrenderhandler[total_retrace][signal]
DisplayRenderHandler displayrenderhandler[4][VGA_DISPLAYRENDERSIZE]; //Our handlers for all pixels!

void VGA_NOP(SEQ_DATA *Sequencer, VGA_Type *VGA) //NOP for pixels!
{}

OPTINLINE void VGA_RenderOutput(SEQ_DATA *Sequencer, VGA_Type *VGA) //Render the current rendered frame to the display!
{
	//First, render ourselves to the screen!
	GPU.xres = Sequencer->xres; //Apply x resolution!
	GPU.yres = Sequencer->yres; //Apply y resolution!
	if (Sequencer->is_topwindow) //Top window isn't supported yet?
	{
		GPU.yres = Sequencer->topwindowCRTbase; //Take the top window only, the bottom window(splitscreen) isn't supported yet!
	}
	//unlockGPU(); //Unlock the GPU!
	VGA_VBlankHandler(VGA); //Handle all VBlank stuff!
	//lockGPU(); //Lock the GPU again! We're using it again!
	Sequencer->yres = 0; //Reset Y resolution next frame if not specified (like a real screen)!
	Sequencer->xres = 0; //Reset X resolution next frame if not specified (like a real screen)!
}

//Total handlers!
void VGA_VTotal(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	Sequencer->Scanline = 0; //Reset for the next frame!
	//VGA_RenderOutput(Sequencer,VGA); //Render the output to the screen!
	VGA_Sequencer_calcScanlineData(VGA);
	VGA_Sequencer_updateRow(VGA, Sequencer); //Scanline has been changed!
}

void VGA_VTotalEnd(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//The end of vertical total has been reached, reload start address!
	Sequencer->startmap = VGA->precalcs.startaddress[0]; //What start address to use for the next frame?
}

void VGA_HTotal(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//Process HBlank: reload display data for the next scanline!
	//Sequencer itself
	Sequencer->x = 0; //Reset for the next scanline!
	
	//Sequencer rendering data
	VGA_Sequencer_calcScanlineData(VGA);
	VGA_Sequencer_updateRow(VGA, Sequencer); //Scanline has been changed!
	Sequencer->DACcounter = 0; //Reset the DAC counter!
	Sequencer->lastDACcolor = 0; //Reset the last DAC color!
}

//Retrace handlers!
void VGA_VRetrace(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	if (VGA->CRTC.y>Sequencer->yres)
	{
		Sequencer->yres = VGA->CRTC.y; //Current y resolution!
	}
	VGA->CRTC.y = 0; //Reset destination row!
	VGA_RenderOutput(Sequencer,VGA); //Render the output to the screen!
}

byte CGAMDARenderer = 0; //Render CGA style?

void VGA_HRetrace(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	CGALineSize = VGA->CRTC.x; //Update X resolution!
	if (VGA->CRTC.x>Sequencer->xres) Sequencer->xres = VGA->CRTC.x; //Current x resolution!
	VGA->CRTC.x = 0; //Reset destination column!
	if (likely(vretrace==0)) //Not retracing vertically?
	{
		if (CGAMDARenderer) //CGA/MDA rendering mode?
		{
			drawCGALine(VGA); //Draw the current CGA line using NTSC colours!	
		}
		++VGA->CRTC.y; //Not retracing vertically? Next row on-screen!
		if (likely(Sequencer->is_topwindow==0)) //Not top window?
		{
			Sequencer->topwindowCRTbase = VGA->CRTC.y; //Save bottom resolution!
		}
	}
	++Sequencer->Scanline; //Next scanline to process!
}

//All renderers for active display parts:

typedef void (*VGA_Sequencer_Mode)(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo); //Render an active display pixel!
typedef void (*VGA_LightPen_Mode)(word currentlocation, byte is_lightpenlocation, byte is_lightpenpressed); //Light pen handler for the specified hardware!

uint_32 CLUT16bit[0x10000]; //16-bit color lookup table!
uint_32 CLUT15bit[0x10000]; //15-bit color lookup table!

void VGA_LightPenHandler(word currentlocation, byte is_lightpenlocation, byte is_lightpenpressed)
{
	//Do nothing: VGA has no light pen support!
}

VGA_LightPen_Mode lightpenhandler = &VGA_LightPenHandler; //Light Pen Handler!

void updateLightPenMode(VGA_Type *VGA)
{
	if (unlikely(VGA->enable_SVGA==3)) //EGA?
	{
		lightpenhandler = &EGA_checklightpen; //Check for anything requiring the lightpen on the EGA!
	}
	else if (unlikely(VGA->enable_SVGA==4)) //CGA/MDA?
	{
		lightpenhandler = &CGA_checklightpen; //Check for anything requiring the lightpen on the CGA!
	}
	else //VGA?
	{
		lightpenhandler = &VGA_LightPenHandler; //Use VGA light pen handler!
	}
}

//drawnto: 0=GPU, 1=CGALineBuffer
OPTINLINE void video_updateLightPen(VGA_Type *VGA, byte drawnto)
{
	byte lightpen_triggered;
	lightpen_triggered = ((lightpen_x==VGA->CRTC.x) && (lightpen_y==VGA->CRTC.y)); //Are we at the location specified by the lightpen on the CRT?
	lightpenhandler(lightpen_currentvramlocation,lightpen_triggered,lightpen_pressed); //Check for anything requiring the lightpen on the device!
}

//Blank handler!
void VGA_Blank_VGA(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	if (hretrace) return; //Don't handle during horizontal retraces or top screen rendering!
	drawPixel(VGA, RGB(0x00, 0x00, 0x00)); //Draw blank!
	video_updateLightPen(VGA,0); //Update the light pen!
	++VGA->CRTC.x; //Next x!
}

void VGA_Blank_CGA(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	if (hretrace) return; //Don't handle during horizontal retraces!
	//Normally, we convert the pixel given using the VGA attribute, but in this case we need to apply NTSC conversion from reenigne.
	if (likely(VGA->CRTC.x<NUMITEMS(CGALineBuffer))) //Valid pixel horizontally?
	{
		CGALineBuffer[VGA->CRTC.x] = 0; //Take the literal pixel color of the CGA for later NTSC conversion!
	}
	video_updateLightPen(VGA,1); //Update the light pen!
	++VGA->CRTC.x; //Next x!
}

void VGA_ActiveDisplay_noblanking_VGA(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	uint_32 DACcolor; //The latched color!
	INLINEREGISTER byte doublepixels=0;
	if (hretrace) return; //Don't handle during horizontal retraces!
	//Active display!
	if ((VGA->precalcs.DACmode&4)==4) //Not latching in 1 raising&lowering(by the attribute controller) clock(Not mode 2, but mode 1)?
	{
		//Latch a 8-bit pixel?
		Sequencer->lastDACcolor >>= (4<<attributeinfo->attributesize); //Latching 4/8/16 bits, whether used or not!
		Sequencer->lastDACcolor |= ((attributeinfo->attribute)<< ((8 << (VGA->precalcs.DACmode & 2) >> 1) - (4 << attributeinfo->attributesize))); //Latching this attribute! Low byte is latched first!

		if ((++Sequencer->DACcounter)&((4>>attributeinfo->attributesize)-1)) //To latch and not process yet? This is the least significant byte/bits of the counter!
		{
			return; //Skip this data: we only latch every two pixels!
		}
	}
	else //Pseudo-color mode or Mode 2 15/16-bit DAC?
	{
		Sequencer->lastDACcolor = attributeinfo->attribute; //Latching this attribute!
	}

	doublepixels = ((1<<(VGA->precalcs.ClockingModeRegister_DCR&1))<<attributeinfo->attributesize); //Double the pixels(half horizontal clock) and multiply for each extra pixel clock taken?
	if ((VGA->precalcs.DACmode&4)==4) //Multiple inputs are taken?
	{
		doublepixels <<= (2>>attributeinfo->attributesize); //On top of the attribute doubling the clocks used, we (qua)druple it again for nibbles, double it for bytes and do nothing for words! 
	}

	//Convert the pixel to a RGB value before drawing any blocks of pixels!
	if (VGA->precalcs.DACmode&2) //16-bit color?
	{
		//Now draw in the selected color depth!
		if (VGA->precalcs.DACmode&1) //16-bit color?
		{
			DACcolor = CLUT16bit[(Sequencer->lastDACcolor&0xFFFF)]; //Draw the 16-bit color pixel!
		}
		else //15-bit color?
		{
			DACcolor = CLUT15bit[(Sequencer->lastDACcolor&0xFFFF)]; //Draw the 15-bit color pixel!
		}
	}
	else //VGA compatibility mode? 8-bit color!
	{
		if (VGA->precalcs.EGA_DisableInternalVideoDrivers) //Special case: internal video drivers disabled?
		{
			DACcolor = VGA_DAC(VGA,VGA->CRTC.DACOutput = (VGA->registers->ExternalRegisters.FEATURECONTROLREGISTER&3)); //The FEAT0 and FEAT1 outputs become the new output!
		}
		else
		{
			VGA->CRTC.DACOutput = (Sequencer->lastDACcolor&0xFF); //DAC index!
			DACcolor = VGA_DAC(VGA,(Sequencer->lastDACcolor&0xFF)); //Render through the 8-bit DAC!
		}
	}
	//Draw the pixel(s) that is/are latched!
	do //We always render at least 1 pixel from the DAC!
	{
		drawPixel(VGA, DACcolor); //Draw the color pixel(s)!
		video_updateLightPen(VGA,0); //Update the light pen!
		++VGA->CRTC.x; //Next x!
	} while (--doublepixels); //Any pixels left to render?
}

void VGA_ActiveDisplay_noblanking_CGA(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	if (hretrace) return; //Don't handle during horizontal retraces!
	//Active display!
	//Normally, we convert the pixel given using the VGA attribute, but in this case we need to apply NTSC conversion from reenigne.
	if (likely(VGA->CRTC.x<NUMITEMS(CGALineBuffer))) //Valid pixel horizontally?
	{
		CGALineBuffer[VGA->CRTC.x] = (byte)attributeinfo->attribute; //Take the literal pixel color of the CGA for later NTSC conversion!
	}
	video_updateLightPen(VGA,1); //Update the light pen!
	++VGA->CRTC.x; //Next x!
}

void VGA_Overscan_noblanking_VGA(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	if (hretrace) return; //Don't handle during horizontal retraces!
	//Overscan!
	/*
	if (VGA->precalcs.AttributeController_16bitDAC==3) //16-bit color mode?
	{
		drawPixel(VGA,CLUT16bit[attributeinfo->attribute]); //Draw the 16-bit color pixel!
	}
	else //VGA compatibility mode?
	{
	*/
		if (VGA->precalcs.EGA_DisableInternalVideoDrivers) //Special case: internal video drivers disabled?
		{
			VGA->CRTC.DACOutput = (VGA->registers->ExternalRegisters.FEATURECONTROLREGISTER&3); //The FEAT0 and FEAT1 outputs become the new output!
			drawPixel(VGA, VGA_DAC(VGA, VGA->CRTC.DACOutput)); //Draw overscan in the specified color instead!
		}
		else //Normal VGA behaviour?
		{
			VGA->CRTC.DACOutput = VGA->precalcs.overscancolor; //Overscan index!
			drawPixel(VGA, VGA_DAC(VGA, VGA->precalcs.overscancolor)); //Draw overscan!
		}
	//}
	video_updateLightPen(VGA,0); //Update the light pen!
	++VGA->CRTC.x; //Next x!
}

void VGA_Overscan_noblanking_CGA(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	if (hretrace) return; //Don't handle during horizontal retraces!
	//Overscan!
	//Normally, we convert the pixel given using the VGA attribute, but in this case we need to apply NTSC conversion from reenigne.
	if (likely(VGA->CRTC.x<NUMITEMS(CGALineBuffer))) //Valid pixel horizontally?
	{
		CGALineBuffer[VGA->CRTC.x] = VGA->precalcs.overscancolor; //Take the literal pixel color of the CGA for later NTSC conversion!
	}
	video_updateLightPen(VGA,1); //Update the light pen!
	++VGA->CRTC.x; //Next x!
}

void updateVGASequencer_Mode(VGA_Type *VGA)
{
	VGA->precalcs.extrasignal = VGA->precalcs.graphicsmode?VGA_DISPLAYGRAPHICSMODE:0x0000; //Apply the current mode (graphics vs text mode)!
}

VGA_Sequencer_Mode activedisplay_noblanking_handler = NULL;
VGA_Sequencer_Mode activedisplay_blank_handler = NULL;
VGA_Sequencer_Mode overscan_noblanking_handler = NULL;
VGA_Sequencer_Mode overscan_blank_handler = NULL;

//Active display handler!
void VGA_ActiveDisplay_Text(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//Render our active display here!
	if (VGA_ActiveDisplay_timing(Sequencer, VGA)) //Execute our timings!
	{
		VGA_Sequencer_TextMode(VGA,Sequencer,&currentattributeinfo); //Get the color to render!
		if (VGA_AttributeController(&currentattributeinfo,VGA))
		{
			return; //Nibbled!
		}
	}

	activedisplay_noblanking_handler(VGA, Sequencer, &currentattributeinfo); //Blank or active display!
}

void VGA_ActiveDisplay_Text_blanking(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//Render our active display here!
	if (VGA_ActiveDisplay_timing(Sequencer, VGA)) //Execute our timings!
	{
		VGA_Sequencer_TextMode(VGA, Sequencer, &currentattributeinfo); //Get the color to render!
		if (VGA_AttributeController(&currentattributeinfo, VGA))
		{
			return; //Nibbled!
		}
	}

	activedisplay_blank_handler(VGA, Sequencer, &currentattributeinfo); //Blank or active display!
}

void VGA_ActiveDisplay_Graphics(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//Render our active display here!
	if (VGA_ActiveDisplay_timing(Sequencer, VGA)) //Execute our timings!
	{
		VGA_Sequencer_GraphicsMode(VGA, Sequencer, &currentattributeinfo); //Get the color to render!
		if (VGA_AttributeController(&currentattributeinfo, VGA))
		{
			return; //Nibbled!
		}
	}

	activedisplay_noblanking_handler(VGA, Sequencer, &currentattributeinfo); //Blank or active display!
}

void VGA_ActiveDisplay_Graphics_blanking(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//Render our active display here! Start with text mode!		
	if (VGA_ActiveDisplay_timing(Sequencer,VGA)) //Execute our timings!
	{
		VGA_Sequencer_GraphicsMode(VGA, Sequencer, &currentattributeinfo); //Get the color to render!
		if (VGA_AttributeController(&currentattributeinfo, VGA))
		{
			return; //Nibbled!
		}
	}

	activedisplay_blank_handler(VGA, Sequencer, &currentattributeinfo); //Blank or active display!
}

//Overscan handler!
void VGA_Overscan(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	overscan_noblanking_handler(VGA,Sequencer,NULL); //Attribute info isn't used!
}

void VGA_Overscan_blanking(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	overscan_blank_handler(VGA, Sequencer, NULL); //Attribute info isn't used!
}

void updateCGAMDARenderer() //Update the renderer to use!
{
	if (unlikely(CGAMDARenderer)) //CGA/MDA rendering mode?
	{
		activedisplay_noblanking_handler = &VGA_ActiveDisplay_noblanking_CGA; //Blank or active display!
		activedisplay_blank_handler = &VGA_Blank_CGA; //Blank or active display!
		overscan_noblanking_handler = &VGA_Overscan_noblanking_CGA; //Attribute info isn't used!
		overscan_blank_handler = &VGA_Blank_CGA; //Attribute info isn't used!
	}
	else //VGA+ rendering mode?
	{
		activedisplay_noblanking_handler = &VGA_ActiveDisplay_noblanking_VGA; //Blank or active display!
		activedisplay_blank_handler = &VGA_Blank_VGA; //Blank or active display!
		overscan_noblanking_handler = &VGA_Overscan_noblanking_VGA; //Attribute info isn't used!
		overscan_blank_handler = &VGA_Blank_VGA; //Attribute info isn't used!
	}
}

//Combination functions of the above:

//Horizontal before vertical, retrace before total.

extern byte charxbuffer[256]; //Full character inner x location!

//Initialise all handlers!
void initStateHandlers()
{
	uint_32 i;
	//Default uninitialised entries!
	displayrenderhandler[0][0] = &VGA_NOP; //Default: no action!
	displayrenderhandler[1][0] = &VGA_NOP; //Default: no action!
	displayrenderhandler[2][0] = &VGA_NOP; //Default: no action!
	displayrenderhandler[3][0] = &VGA_NOP; //Default: no action!

	updateCGAMDARenderer(); //Initialise the renderer for usage!

	for (i=1;i<VGA_DISPLAYRENDERSIZE;i++) //Fill the normal entries!
	{
		//Total handler for total handlers!
		displayrenderhandler[1][i] = &VGA_NOP; //Do nothing when disabled: retrace does no output!
		displayrenderhandler[2][i] = &VGA_NOP; //Do nothing when disabled: total handler!
		displayrenderhandler[3][i] = &VGA_NOP; //Do nothing when disabled: total&retrace handler!
		
		//Rendering handler without retrace AND total!
		displayrenderhandler[0][i] = ((i&VGA_DISPLAYMASK)==VGA_DISPLAYACTIVE)?((i&VGA_DISPLAYGRAPHICSMODE)?((i&VGA_SIGNAL_BLANKING)?&VGA_ActiveDisplay_Graphics_blanking: &VGA_ActiveDisplay_Graphics): ((i&VGA_SIGNAL_BLANKING) ? &VGA_ActiveDisplay_Text_blanking : &VGA_ActiveDisplay_Text)):((i&VGA_SIGNAL_BLANKING) ? &VGA_Overscan : &VGA_Overscan_blanking); //Not retracing or any total handler = display/overscan!
	}

	for (i = 0;i < 0x10000;++i) //Create the 16&15-bit CLUT! The format is Red on MSB, Green in the middle and Blue in the LSB.
	{
		CLUT16bit[i] = RGB((byte)(((((float)((i >> 11) & 0x1F)) / (float)0x1F)*255.0f)), (byte)(((float)((i >> 5) & 0x3F) / (float)0x3F)*255.0f), (byte)((((float)((i >> 0) & 0x1F) / (float)0x1F)*255.0f))); //16-bit color lookup table (5:6:5 format)!
		CLUT15bit[i] = RGB((byte)(((((float)((i >> 10) & 0x1F)) / (float)0x1F)*255.0f)), (byte)(((float)((i >> 5) & 0x1F) / (float)0x1F)*255.0f), (byte)((((float)((i >> 0) & 0x1F) / (float)0x1F)*255.0f))); //15-bit color lookup table (5:5:5 format)!
	}
	memset(&charxbuffer,0xFF,sizeof(charxbuffer)); //Character x buffer!
	for (i=0;i<9;++i)
	{
		charxbuffer[i] = i; //We're this inner pixel!
	}
	VGA_GraphicsDecoder(getActiveVGA(),0); //Load initial data!
	VGA_TextDecoder(getActiveVGA(),0); //Load initial data!
}

/*

Now, the renderer basic, timings etc.

*/

//0=Automatic synchronization, 1=Tightly synchronized with the CPU emulation.
void setVGASpeed(byte setting)
{
	if (setting) //New style setting?
	{
		if (setting==1) //Modern Automatic synchronization and request to tightly synchronize?
		{
			if (currentVGASpeed) //Set?
			{
				currentVGASpeed = 0; //Start tight synchronization!
			}
		}
		else if ((!currentVGASpeed) && (setting==2)) //Tightly synchronized and request to use automatic synchronization?
		{
			passedcounter = LIMITVGA; //Start speed detection with this many items!
			currentVGASpeed = 1; //Start automatic synchronization!
			SynchronizationMode = 1; //New style synchronization!
		}
		//When there's no change, do nothing!
	}
	else //Old style synchronization method?
	{
		currentVGASpeed = 1; //Start automatic synchronization!
		passedcounter = 1; //Don't apply passed counter! As long as we're >0 to apply synchronization!
		SynchronizationMode = 0; //Old style synchronization!		
	}
}

void adjustVGASpeed()
{
	#ifdef LIMITVGA
	passedcounter = LIMITVGA; //Start counting this many times before locking to the speed!
	#endif
}

void changeRowTimer(VGA_Type *VGA) //Change the VGA row processing timer the ammount of lines on display!
{
	#ifdef __HW_DISABLED
	return; //Disabled?
	#endif
	DOUBLE rate;
	rate = VGA_VerticalRefreshRate(VGA); //Get our rate first!
	if (unlikely(rate!=oldrate)) //New rate has been specified?
	{
		oldrate = rate; //We've updated to this rate!
		#ifdef IS_LONGDOUBLE
		VGA_rendertiming = (float)(1000000000.0L/rate); //Handle this rate from now on! Keep us locked though to prevent screen updates messing with this!
		#else
		VGA_rendertiming = (float)(1000000000.0/rate); //Handle this rate from now on! Keep us locked though to prevent screen updates messing with this!
		#endif
		adjustVGASpeed(); //Auto-adjust our speed!
	}
}

void VGA_initTimer()
{
	VGA_timing = 0.0f; //We're starting to run now!
	oldrate = VGA_VerticalRefreshRate(getActiveVGA()); //Initialise the default rate!
	#ifdef IS_LONGDOUBLE
	VGA_rendertiming = (float)(1000000000.0L/oldrate); //Handle this rate from now on!
	#else
	VGA_rendertiming = (float)(1000000000.0/oldrate); //Handle this rate from now on!
	#endif
	initTicksHolder(&VGA_test);
	adjustVGASpeed(); //Auto-adjust our speed!
}

extern GPU_type GPU;

extern byte allcleared; //Are all pointers cleared?

OPTINLINE byte doVGA_Sequencer() //Do we even execute?
{
	if (unlikely((getActiveVGA()==NULL) || allcleared)) //Invalid VGA? Don't do anything!
	{
		return 0; //Abort: we're disabled without a invalid VGA!
	}
	if (unlikely(GPU.emu_screenbuffer==NULL)) //Invalid screen buffer?
	{
		return 0; //Abort: we're disabled!
	}
	return 1; //We can render something!
}

byte isoutputdisabled = 0; //Output disabled?

//Special states!
extern byte blanking; //Are we blanking!
extern byte retracing; //Allow rendering by retrace!
extern byte totalling; //Allow rendering by total!
byte totalretracing; //Combined flags of retracing/totalling!

extern byte hblank, hretrace; //Horizontal blanking/retrace?
extern byte vblank, vretrace; //Vertical blanking/retrace?
byte hblankendpending = 0; //Ending blank/retrace pending? bits set for any of them!
byte vblankendpending = 0; //Ending blank/retrace pending? bits set for any of them!

byte vtotal = 0; //VTotal busy?

byte VGA_hblankstart = 0; //HBlank started?
extern byte CGAMDARenderer; //CGA/MDA renderer?

OPTINLINE uint_32 get_display(VGA_Type *VGA, word Scanline, word x) //Get/adjust the current display part for the next pixel (going from 0-total on both x and y)!
{
	INLINEREGISTER uint_32 stat; //The status of the pixel!
	//We are a maximum of 4096x1024 size!
	Scanline &= 0xFFF; //Range safety: 4095 scanlines!
	x &= 0xFFF; //Range safety: 4095 columns!
	stat = VGA->CRTC.rowstatus[Scanline]; //Get row status!
	stat |= VGA->CRTC.colstatus[x]; //Get column status!
	stat |= VGA->precalcs.extrasignal; //Graphics mode etc. display status affects the signal too!
	stat |= (blanking<<VGA_SIGNAL_BLANKINGSHIFT); //Apply the current blanking signal as well!
	VGA_hblankstart = stat; //Save directly! Ignore the overflow!
	VGA_hblankstart >>= 7; //Shift into bit 1 to get the hblank status(small hack)!
	return stat; //Give the combined (OR'ed) status!
}

word displaystate; //Last display state!

//All possible wait-states for the video adapter!
typedef void (*WaitStateHandler)(VGA_Type *VGA);

void updateVGAWaitState(); //Prototype!

void WaitState_None(VGA_Type *VGA) {} //No waitstate: NOP!
void WaitState_WaitDots(VGA_Type *VGA)
{
	//Wait 8 hdots!
	if (unlikely(--VGA->WaitStateCounter == 0)) //First wait state done?
	{
		VGA->WaitState = 2; //Enter the next phase: Wait for the next lchar(16 dots period)!
		updateVGAWaitState(); //Update the waitstate!
	}
}
void WaitState_NextlChar(VGA_Type *VGA)
{
	//Wait for the next lchar?
	if (unlikely((VGA->PixelCounter & 0xF) == 0)) //Second wait state done?
	{
		VGA->WaitState = 0; //Enter the next phase: Wait for the next ccycle(3 hdots)
		CPU[activeCPU].halt |= 8; //Start again when the next CPU clock arrives!
		CPU[activeCPU].halt &= ~4; //We're done waiting!
		updateVGAWaitState(); //Update the waitstate!
	}
}

WaitStateHandler CurrentWaitState = NULL; //Current waitstate!

WaitStateHandler WaitStates[8] = { NULL,
WaitState_WaitDots, //Wait 8 hdots!
WaitState_NextlChar, //Wait for the next lchar?
WaitState_None, //Wait for the next ccycle(3 hdots)?
WaitState_None,WaitState_None,WaitState_None,WaitState_None }; //All possible waitstates!

void updateVGAWaitState()
{
	CurrentWaitState = WaitStates[getActiveVGA()->WaitState]; //Load the new waitstate!
}

//HBlank/Retrace handling!

typedef void (*hblankretraceHandler)(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal);

void exechblankretrace(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal)
{
	if (unlikely(VGA_hblankstart)) //HBlank start?
	{
		hblank = 1; //We're blanking!
	}
	else if (unlikely(hblank))
	{
		if (unlikely(signal&VGA_SIGNAL_HBLANKEND)) //HBlank end?
		{
			if ((VGA->registers->specialCGAMDAflags) & 1)
			{
				//End pending HBlank!
				hblank = 0; //We're not blanking anymore!
				hblankendpending = 0; //Remove from flags pending!
			}
			else
			{
				hblankendpending = 1;
			}
		}
	}

	if (unlikely(signal&VGA_SIGNAL_HRETRACESTART)) //HRetrace start?
	{
		if (unlikely(hretrace==0)) //Not running yet?
		{
			VGA_HRetrace(Sequencer, VGA); //Execute the handler!
		}
		hretrace = 1; //We're retracing!
	}
	else if (unlikely(hretrace))
	{
		if (unlikely(signal&VGA_SIGNAL_HRETRACEEND)) //HRetrace end?
		{
			hretrace = 0; //We're not retraing anymore!
		}
	}
}

void nohblankretrace(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal)
{
	if (likely(hblankendpending==0)) return; //End pending HBlank!
	{
		hblank = 0; //We're not blanking anymore!
		hblankendpending = 0; //Remove from flags pending!
	}
}

extern byte is_XT; //Are we emulating an XT architecture?

OPTINLINE void VGA_SIGNAL_HANDLER(SEQ_DATA *Sequencer, VGA_Type *VGA, byte *totalretracing, byte hblankretrace)
{
	const static byte retracemasks[4] = { 0xFF,0x00,0x00,0x00 }; //Disable display when retracing!
	const static hblankretraceHandler hblankretracehandlers[2] = { nohblankretrace,exechblankretrace }; //The handlers!

	INLINEREGISTER word tempsignalbackup, tempsignal; //Our signal backup and signal itself!
recalcsignal: //Recalculate the signal to process!
	tempsignal = tempsignalbackup = displaystate; //The back-up of the signal!
	//Blankings

	hblankretracehandlers[hblankretrace](Sequencer,VGA,tempsignal); //Horizontal timing?

	tempsignal = tempsignalbackup; //Restore the original backup signal!
	tempsignal &= VGA_VBLANKRETRACEMASK; //Check for blanking/tretracing!
	if (unlikely(tempsignal)) //VBlank?
	{
		if (unlikely(tempsignal&VGA_SIGNAL_VBLANKSTART)) //VBlank start?
		{
			vblank = 1; //We're blanking!
		}
		else if (unlikely(vblank))
		{
			if (unlikely(tempsignal&VGA_SIGNAL_VBLANKEND)) //VBlank end?
			{
				if (VGA->registers->specialCGAMDAflags & 1) //CGA special?
				{
					vblank = 0; //We're not blanking anymore!
					vblankendpending = 0; //Remove from flags pending!
					SETBITS(VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER,3,1,0); //No vertical retrace?
				}
				else
				{
					vblankendpending = 1; //Start pending vblank end!
				}
			}
		}

		if (unlikely(tempsignal&VGA_SIGNAL_VRETRACESTART)) //VRetrace start?
		{
			if (unlikely(vretrace==0)) //Not running yet?
			{
				VGA_VRetrace(Sequencer, VGA); //Execute the handler!

				//VGA/EGA vertical retrace interrupt support!
				if (GETBITS(VGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER,4,1)) //Enabled vertical retrace interrupt?
				{
					if (!GETBITS(VGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER,5,1)) //Generate vertical retrace interrupts?
					{
						raiseirq(is_XT?VGA_IRQ_XT:VGA_IRQ_AT); //Execute the CRT interrupt when possible!
					}
					SETBITS(VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER,7,1,1); //We're pending an CRT interrupt!
				}
			}
			SETBITS(VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER,3,1,(vretrace = 1)); //We're retracing!
		}
		else if (unlikely(vretrace))
		{
			if (unlikely(tempsignal&VGA_SIGNAL_VRETRACEEND)) //VRetrace end?
			{
				vretrace = 0; //We're not retracing anymore!
				SETBITS(VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER,3,1,0); //Vertical retrace?
			}
			else
			{
				SETBITS(VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER,3,1,1); //Vertical retrace?
			}
		}
		else //No vretrace?
		{
			SETBITS(VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER,3,1,0); //Vertical retrace?
		}
	}
	else
	{
		if (unlikely(vblankendpending)) //End pending HBlank!
		{
			vblank = 0; //We're not blanking anymore!
			vblankendpending = 0; //Remove from flags pending!
		}
		SETBITS(VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER,3,1,0); //No vertical retrace?
	}

	//Both H&VBlank count!
	blanking = hblank;
	blanking |= vblank; //Process blank!
	//Screen disable applies blanking permanently!
	blanking |= GETBITS(VGA->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER,5,1); //Use disabled output when asked to!

	//Process resetting the HSync/VSync counters!

	INLINEREGISTER byte isretrace; //Vertical or horizontal retrace?
	*totalretracing = isretrace = hretrace;
	retracing = (isretrace |= vretrace); //We're retracing?

	//Process HTotal/VTotal
	//INLINEREGISTER byte currenttotalling; //Current totalling state!
	//currenttotalling = 0; //Default: Not totalling!
	tempsignal = tempsignalbackup; //Restore the original backup signal!
	if (unlikely(tempsignal&VGA_SIGNAL_HTOTAL)) //HTotal?
	{
		VGA_HTotal(Sequencer,VGA); //Process HTotal!
		//currenttotalling = 1; //Total reached!
		displaystate = get_display(getActiveVGA(), Sequencer->Scanline, Sequencer->x++); //Current display state!
		if (likely((displaystate&VGA_SIGNAL_HTOTAL)==0)) //Not infinitely looping?
		{
			hblankretrace = (displaystate&VGA_HBLANKRETRACEMASK)?1:0; //Check for blanking/retracing!
			goto recalcsignal; //Execute immediately!
		}
	}
	if (unlikely(tempsignal&VGA_SIGNAL_VTOTAL)) //VTotal?
	{
		VGA_VTotal(Sequencer,VGA); //Process VTotal!
		/*currenttotalling =*/ vtotal = 1; //Total reached!
		displaystate = get_display(getActiveVGA(), Sequencer->Scanline, Sequencer->x++); //Current display state!
		if (likely((displaystate&VGA_SIGNAL_VTOTAL)==0)) //Not infinitely looping(VTotal ended)?
		{
			VGA_VTotalEnd(Sequencer, VGA); //Signal end of vertical total!
			vtotal = 0; //Not vertical total anymore!
			hblankretrace = (displaystate&VGA_HBLANKRETRACEMASK)?1:0; //Check for blanking/retracing!
			goto recalcsignal; //Execute immediately!
		}
	}
	else if (unlikely(vtotal)) //VTotal ended?
	{
		VGA_VTotalEnd(Sequencer,VGA); //Signal end of vertical total!
		vtotal = 0; //Not vertical total anymore!
	}

	tempsignal &= VGA_DISPLAYMASK; //Check the display now!

	INLINEREGISTER byte currenttotalretracing;
	//isretrace |= (currenttotalretracing = currenttotalling); //Apply totalling to retrace checks, also load totalling for totalretracing information!
	//currenttotalretracing <<= 1; //1 bit needed more!
	//currenttotalretracing = isretrace; //Are we retracing?
	//*totalretracing = currenttotalretracing; //Save retrace info!

	currenttotalretracing = (tempsignal==VGA_DISPLAYACTIVE); //We're active display when not retracing/totalling and active display area!
	currenttotalretracing &= retracemasks[isretrace]; //Apply the retrace mask: we're not using the displayenabled when retracing!
	VGA->CRTC.DisplayEnabled = currenttotalretracing; //The Display Enable signal, which depends on the active video adapter how to use it!
	++VGA->PixelCounter; //Simply blindly increase the pixel counter!

	if (unlikely(CurrentWaitState)) CurrentWaitState(VGA); //Execute the current waitstate, when used!
}

extern DisplayRenderHandler displayrenderhandler[4][VGA_DISPLAYRENDERSIZE]; //Our handlers for all pixels!

OPTINLINE void VGA_Renderer(SEQ_DATA *Sequencer)
{
	static byte totalretracing = 0;
	//Process one pixel only!
	displaystate = get_display(getActiveVGA(), Sequencer->Scanline, Sequencer->x++); //Current display state!
	VGA_SIGNAL_HANDLER(Sequencer, getActiveVGA(),&totalretracing,(displaystate&VGA_HBLANKRETRACEMASK)?1:0); //Handle any change in display state first!
	displayrenderhandler[totalretracing][displaystate](Sequencer, getActiveVGA()); //Execute our signal!
}

//CPU cycle locked version of VGA rendering!
void updateVGA(DOUBLE timepassed, uint_32 MHZ14passed)
{
	#ifdef LIMITVGA
	float limitcalc=0;
	uint_32 renderingsbackup=0;
	float timeprocessed=0.0;
	#endif
	if (unlikely(VGA_debugtiming_enabled)) //Valid debug timing to apply?
	{
		VGA_debugtiming += timepassed; //Time has passed!
	}

	INLINEREGISTER uint_32 renderings; //How many clocks to render?
	renderings = MHZ14passed; //Default to 14MHz clock from the motherboard!
	if (unlikely(getActiveVGA()->precalcs.use14MHzclock==0)) //Not using 14MHz clocking?
	{
		VGA_timing += timepassed; //Time has passed!

		if (unlikely((VGA_timing >= VGA_rendertiming) && VGA_rendertiming)) //Might have passed?
		{
			renderings = (uint_32)floorf((float)(VGA_timing/VGA_rendertiming)); //Ammount of times to render!
			VGA_timing -= (renderings*VGA_rendertiming); //Rest the amount we can process!
		}
		else
		{
			renderings = 0; //Nothing to render!
		}
	}

	if (unlikely(renderings)) //Anything to render?
	{
		#ifdef LIMITVGA
		if ((renderings>VGA_limit) && VGA_limit) //Limit broken?
		{
			renderings = VGA_limit; //Limit the processing to the amount of time specified!
		}
		#endif
		if (unlikely(renderings==0)) return; //Nothing to render!
		#ifdef LIMITVGA
		if (passedcounter && currentVGASpeed) //Still counting?
		{
			timeprocessed = (renderings*VGA_rendertiming); //How much are we processing?
			renderingsbackup = renderings; //Save the backup for comparision!
			VGA_vtotal = 0; //Reset our flag to detect finish of a frame while measuring!
		}
		#endif

		if (unlikely(doVGA_Sequencer()==0)) return; //Don't execute the sequencer if requested to!

		SEQ_DATA *Sequencer;
		Sequencer = GETSEQUENCER(getActiveVGA()); //Our sequencer!

		//All possible states!
		if (unlikely(displayrenderhandler[0][0]==0)) initStateHandlers(); //Init our display states for usage when needed!
		if (unlikely(Sequencer->extrastatus==0)) Sequencer->extrastatus = &getActiveVGA()->CRTC.extrahorizontalstatus[0]; //Start our extra status at the beginning of the row!

		#ifdef LIMITVGA
		if (unlikely(passedcounter && currentVGASpeed)) getnspassed(&VGA_test); //Still counting? Then count our interval!
		#endif
		do
		{
			if (likely(renderings==1)) VGA_Renderer(Sequencer); //2+ optimization? Not usable? Execute only once!
			else //2+ optimization?
			{
				switch (renderings-1) //How many to render(1-5 clocks multiple optimizations)?
				{
				case 3: //4 rendering?
					VGA_Renderer(Sequencer); //Tick the VGA once!
				case 2: //3 rendering?
					VGA_Renderer(Sequencer); //Tick the VGA once!
				case 1: //2 rendering?
					VGA_Renderer(Sequencer); //Tick the VGA once!
					renderings -= (renderings-1); //We've processed 1-3 more!
				case 0: //1 rendering?
					VGA_Renderer(Sequencer); //Tick the VGA once!
					break;
				default: //5+ optimization?
					VGA_Renderer(Sequencer); //Tick the VGA once!
					VGA_Renderer(Sequencer); //Tick the VGA once!
					VGA_Renderer(Sequencer); //Tick the VGA once!
					VGA_Renderer(Sequencer); //Tick the VGA once!
					VGA_Renderer(Sequencer); //Tick the VGA once!
					renderings -= 4; //We've processed 4 more!
					break;
				}
			}
		} while (--renderings); //Ticks left to tick?

		if (getActiveVGA()->enable_SVGA!=4) //EGA/VGA+(Not MDA/CGA)?
		{
			SETBITS(getActiveVGA()->registers->ExternalRegisters.INPUTSTATUS1REGISTER,0,1,retracing); //Only update the display disabled when required to: it's only needed by the CPU, not the renderer!
		}
		else
		{
			SETBITS(getActiveVGA()->registers->ExternalRegisters.INPUTSTATUS1REGISTER,0,1,(getActiveVGA()->CRTC.DisplayEnabled^1)); //Only update the display disabled when required to: it's only needed by the CPU, not the renderer!
		}

		#ifdef LIMITVGA
		if (unlikely(passedcounter && currentVGASpeed)) //Still counting?
		{
			limitcalc = getnspassed(&VGA_test); //How long have we taken?

			//timeprocessed=how much time to use, limitcalc=how much time we have taken, renderingsbackup=How many pixels have we processed.
			VGA_limit = floorf(((float)renderingsbackup/(float)limitcalc)*timeprocessed); //Don't process any more than we're allowed to (timepassed).
			if (limitcalc<=timeprocessed) VGA_limit = 0; //Don't limit if we're running at full speed (we're below time we are allowed to process)!
			if (SynchronizationMode) --passedcounter; //A part has been rendered! Only with
		}
		#endif
	}
}

extern BIOS_Settings_TYPE BIOS_Settings; //Our settings!

void EMU_update_VGA_Settings() //Update the VGA settings!
{
	DAC_Use_BWMonitor((BIOS_Settings.bwmonitor>0) ? 1 : 0); //Select color/bw monitor!
	if (DAC_Use_BWMonitor(0xFF)) //Using a b/w monitor?
	{
		DAC_BWColor(BIOS_Settings.bwmonitor); //Set the color to use!
	}
	switch (BIOS_Settings.VGA_Mode) //What precursor compatibility mode?
	{
		default: //Pure VGA?
		case 6: //Tseng ET4000?
		case 0: //Pure VGA?
		case 8: //EGA?
			setVGA_NMIonPrecursors(0); //No NMI on precursors!
			setVGA_CGA(0); //No CGA!
			setVGA_MDA(0); //No MDA!
			break;
		case 1: //VGA with NMI?
			setVGA_NMIonPrecursors(BIOS_Settings.VGA_Mode); //Set NMI on precursors!
			setVGA_CGA(0); //No CGA!
			setVGA_MDA(0); //No MDA!
			break;
		case 2: //VGA with CGA?
			setVGA_NMIonPrecursors(0); //Disable NMI on precursors!
			setVGA_CGA(2); //CGA enabled with VGA!
			setVGA_MDA(0); //No MDA!
			break;
		case 3: //VGA with MDA?
			setVGA_NMIonPrecursors(0); //Disable NMI on precursors!
			setVGA_CGA(0); //No CGA!
			setVGA_MDA(2); //MDA enabled with VGA!
			break;
		case 4: //Pure CGA?
			setVGA_NMIonPrecursors(0); //Disable NMI on precursors!
			setVGA_CGA(1); //Pure CGA!
			setVGA_MDA(0); //No MDA!
			break;
		case 5: //Pure MDA?
			setVGA_NMIonPrecursors(0); //Disable NMI on precursors!
			setVGA_CGA(0); //No CGA!
			setVGA_MDA(1); //Pure MDA!
			break;
	}
	setVGASpeed(BIOS_Settings.VGASynchronization); //Apply VGA synchronization setting!
	if (getActiveVGA()) //Gotten an active VGA?
	{
		DAC_updateEntries(getActiveVGA()); //Update all DAC entries according to the current/new color settings!
		byte CGAMode;
		CGAMode = BIOS_Settings.CGAModel; //What CGA is emulated?
		if ((CGAMode&3)!=CGAMode) CGAMode = 0; //Default to RGB, old-style CGA!
		setCGA_NTSC(CGAMode&1); //RGB with modes 0&2, NTSC with modes 1&3
		setCGA_NewCGA(CGAMode&2); //New-style with modes 2&3, Old-style with modes 0&1
	}
}
