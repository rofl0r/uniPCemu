#define VGA_SEQUENCER

#include "headers/types.h" //Basic types!
#include "headers/emu/gpu/gpu.h" //GPU!
#include "headers/emu/gpu/gpu_text.h" //Text support!
#include "headers/hardware/vga/vga.h" //VGA!
#include "headers/hardware/vga/vga_sequencer.h" //Ourselves!
#include "headers/hardware/vga/vga_sequencer_graphicsmode.h" //Text mode!
#include "headers/hardware/vga/vga_sequencer_textmode.h" //Text mode!
#include "headers/hardware/vga/vga_attributecontroller.h" //Attribute controller!
#include "headers/hardware/vga/vga_crtcontroller.h" //CRT Controller for finishing up!
#include "headers/hardware/vga/vga_dacrenderer.h" //DAC support!
#include "headers/hardware/vga/vga_vram.h" //VGA VRAM support!
#include "headers/hardware/vga/vga_cga_mda.h" //CGA/MDA support!
#include "headers/hardware/vga/vga_cga_ntsc.h" //CGA NTSC support!
#include "headers/cpu/interrupts.h" //For get/putpixel variant!
#include "headers/support/highrestimer.h" //High resolution clock!
#include "headers/support/zalloc.h" //Memory protection support!
#include "headers/support/log.h" //Logging support!

//Are we disabled?
#define HW_DISABLED 0

#define CURRENTBLINK(VGA) VGA->blink32

//Do color mode or B/W mode DAC according to our settings!
#define VGA_DAC(VGA,DACValue) (VGA->precalcs.effectiveDAC[(DACValue)])

extern GPU_type GPU; //GPU!

float VGA_clocks[4] = {
			(25.2/1.001)*1000000.0f, //25MHz: VGA standard clock
			(28.35/1.001)*1000000.0f, //28MHz: VGA standard clock
			0.0f, //external clock: not connected!
			0.0f //Unused
			}; //Our clocks!

uint_32 CGALineSize = 0; //How long is our line!
byte CGALineBuffer[2048]; //Full CGA scanline buffer!
uint_32 CGAOutputBuffer[2048]; //Full CGA NTSC buffer!

VGA_clockrateextensionhandler VGA_calcclockrateextensionhandler; //The clock rate extension handler!

float VGA_VerticalRefreshRate(VGA_Type *VGA) //Scanline speed for one line in Hz!
{
	//Horizontal Refresh Rate=Clock Frequency (in Hz)/horizontal pixels
	//Vertical Refresh rate=Horizontal Refresh Rate/total scan lines!
	if (!memprotect(VGA,sizeof(*VGA),NULL)) //No VGA?
	{
		return 0.0f; //Remove VGA Scanline counter: nothing to render!
	}
	if (VGA_calcclockrateextensionhandler) if (VGA_calcclockrateextensionhandler(VGA)!=0.0f) return VGA_calcclockrateextensionhandler(VGA); //Give the extended clock if needed!
	return VGA_clocks[(VGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.ClockSelect&3)];
}

//Main rendering routine: renders pixels to the emulated screen.

OPTINLINE void drawPixel_real(uint_32 pixel, uint_32 x, uint_32 y) //Manual version for CGA conversion!
{
	INLINEREGISTER uint_32 *screenpixel = &EMU_BUFFER(x,y); //Pointer to our pixel!
	if (screenpixel>=EMU_SCREENBUFFEREND) return; //Out of bounds?
	if ((*screenpixel)!=pixel) //Are we to update the changed pixel?
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
		if (!CGALineSize) return; //Abort if nothing to render!
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
			if (bufferpos == finalpos) break; //Stop processing when finished!
			++drawx; //Next line index!
		}
	}
	else //CGA mode?
	{
		INLINEREGISTER uint_32 *bufferpos, *finalpos;
		if (!CGALineSize) return; //Abort if nothing to render!
		finalpos = &CGAOutputBuffer[CGALineSize]; //End of the output buffer to process!
		bufferpos = &CGAOutputBuffer[0]; //First pixel to render!
		RENDER_convertCGAOutput(&CGALineBuffer[0], &CGAOutputBuffer[0], CGALineSize); //Convert the CGA line to RGB output!
		drawx = 0; //Start index to draw at!
		for (;;) //Render all pixels!
		{
			drawPixel_real(*bufferpos,drawx,VGA->CRTC.y); //Render the converted CGA output signal!
			++bufferpos;
			if (bufferpos==finalpos) break; //Stop processing when finished!
			++drawx; //Next line index!
		}
	}
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
	allow_pixelshiftcount = 1; //Allow by default!
	if (Sequencer->Scanline>=VGA->precalcs.topwindowstart) //Top window reached?
	{
		if (Sequencer->Scanline==VGA->precalcs.topwindowstart) //Start of the top window?
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

byte planesbuffer[4]; //All read planes for the current processing!

typedef void (*VGA_Sequencer_planedecoder)(VGA_Type *VGA, word loadedlocation);

OPTINLINE uint_32 patch_map1314(VGA_Type *VGA, uint_32 addresscounter) //Patch full VRAM address!
{ //Check this!
	INLINEREGISTER uint_32 bit; //Load row scan counter!
	if (VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.MAP13==0) //a13=Bit 0 of the row scan counter!
	{
		//Row scan counter bit 1 is placed on the memory bus bit 14 during active display time.
		//Bit 1, placed on memory address bit 14 has the effect of quartering the memory.
		bit = ((SEQ_DATA *)VGA->Sequencer)->rowscancounter; //Current row scan counter!
		bit &= 1; //Bit0 only!
		bit <<= 13; //Shift to our position (bit 13)!
		addresscounter &= ~0x2000; //Clear bit13!
		addresscounter |= bit; //Set bit13 if needed!
	}

	if (VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.MAP14==0) //a14<=Bit 1 of the row scan counter!
	{
		bit = ((SEQ_DATA *)VGA->Sequencer)->rowscancounter; //Current row scan counter!
		bit &= 2; //Bit1 only!
		bit <<= 13; //Shift to our position (bit 14)!
		addresscounter &= ~0x4000; //Clear bit14;
		addresscounter |= bit; //Set bit14 if needed!
	}

	return addresscounter; //Give the linear address!
}

OPTINLINE uint_32 addresswrap(VGA_Type *VGA, uint_32 memoryaddress) //Wraps memory arround 64k!
{
	INLINEREGISTER uint_32 result, address2;
	switch (VGA->precalcs.BWDModeShift) //What mode?
	{
		case 1: //Word mode?
			result = 0xD; //Load default location (13)
			result |= (VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.AW << 1); //MA15 instead of MA13 when set!
			address2 = memoryaddress; //Load the address for calculating!
			address2 >>= result; //Apply MA15/MA13 to bit 0!
			address2 &= 1; //Only load bit 0!
			result = memoryaddress; //Default: don't change!
			result <<= 1; //Shift up to create the word mode!
			result |= address2; //Add bit MA15/MA13 at bit 0!
			return result; //Give the result!
		case 2: //DWord mode?
			return ((memoryaddress<<2)&0xFFFF)|((memoryaddress>>14)&3); //Doubleword mode with SVGA support!
			break;
		default:
		case 0: //Byte mode?
			break; //Unchanged!

	}
	return memoryaddress; //Original address in byte mode!
}

VGA_AttributeInfo currentattributeinfo; //Our current collected attribute info!

OPTINLINE void VGA_loadcharacterplanes(VGA_Type *VGA, SEQ_DATA *Sequencer, word x) //Load the planes!
{
	INLINEREGISTER uint_32 loadedlocation, vramlocation; //The location we load at!
	static uint_32 lastlocation; //Last latched location!
	//Horizontal logic
	VGA_Sequencer_planedecoder planesdecoder[2] = { VGA_TextDecoder, VGA_GraphicsDecoder }; //Use the correct decoder!

	//Column logic
	if (x) //Past the first position of the line(not the initial loading)?
	{
		++Sequencer->memoryaddressclockdivider; //Increase the address counter point by one character clock!
		++Sequencer->memoryaddress; //Increase the memory address to be loaded!
	}
	else //First position on the line?
	{
		Sequencer->memoryaddressclockdivider = Sequencer->memoryaddressclock = Sequencer->memoryaddress = lastlocation = 0; //Address counters are reset!
	}

	/*if (Sequencer->memoryaddressclockdivider >= (1<<VGA->precalcs.characterclockshift)) //Divide memory address clock by 1/2/4!
	{
		Sequencer->memoryaddressclockdivider = 0; //Reset!
		*/
		lastlocation = Sequencer->memoryaddress; //Latch the new location!
	//}

	loadedlocation = lastlocation; //Load the address to be loaded!
	loadedlocation += Sequencer->charystart; //Apply the line and start map to retrieve!
	vramlocation = patch_map1314(VGA, addresswrap(VGA, loadedlocation)); //Apply address wrap and MAP13/14?

	//Row logic
	CGA_checklightpen(loadedlocation); //Check for anything requiring the lightpen on the CGA!

	//Now calculate and give the planes to be used!
	if (VGA->VRAM==0) goto skipVRAM; //VRAM must exist!
	loadedlocation = VGA_VRAMDIRECTPLANAR(VGA,vramlocation); //Load the 4 planes from VRAM, one dword at a time!
	skipVRAM: //No VRAM present to display?
	planesbuffer[0] = (loadedlocation&0xFF); //Read plane 0!
	loadedlocation >>= 8;
	planesbuffer[1] = (loadedlocation & 0xFF); //Read plane 1!
	loadedlocation >>= 8;
	planesbuffer[2] = (loadedlocation & 0xFF); //Read plane 2!
	loadedlocation >>= 8;
	planesbuffer[3] = (loadedlocation & 0xFF); //Read plane 3!
	//Now the buffer is ready to be processed into pixels!

	planesdecoder[VGA->precalcs.graphicsmode](VGA,vramlocation); //Use the decoder to get the pixels or characters!

	INLINEREGISTER byte lookupprecalcs;
	lookupprecalcs = (byte)((SEQ_DATA *)Sequencer)->charinner_y;
	lookupprecalcs <<= 1; //Make room!
	lookupprecalcs |= CURRENTBLINK(VGA); //Blink!
	lookupprecalcs <<= 1; //Make room for the pixelon!
	currentattributeinfo.lookupprecalcs = lookupprecalcs; //Save the looked up precalcs, this never changes during a processed block of pixels (both text and graphics modes)!
}

OPTINLINE void VGA_Sequencer_updateRow(VGA_Type *VGA, SEQ_DATA *Sequencer)
{
	INLINEREGISTER word row;
	INLINEREGISTER uint_32 charystart;
	row = Sequencer->Scanline; //Default: our normal scanline!
	if (row>VGA->precalcs.topwindowstart) //Splitscreen operations?
	{
		row -= VGA->precalcs.topwindowstart; //This starts after the row specified, at row #0!
		--row; //We start at row #0, not row #1(1 after topwindowstart).
	}

	//row is the vertical timing counter
	row >>= VGA->precalcs.CRTCModeControlRegister_SLDIV; //Apply Scan Doubling on the row scan counter: we take effect on content (double scanning)!
	//Apply scanline division to the current row timing!

	row <<= 1; //We're always a multiple of 2 by index into charrowstatus!

	//Row now is an index into charrowstatus
	word *currowstatus = &VGA->CRTC.charrowstatus[row]; //Current row status!
	Sequencer->chary = row = *currowstatus++; //First is chary (effective character/graphics row)!
	Sequencer->rowscancounter = Sequencer->charinner_y = *currowstatus; //Second is charinner_y, which is also the row scan counter!

	charystart = getVRAMScanlineStart(VGA, row); //Calculate row start!
	charystart += Sequencer->startmap; //Calculate the start of the map while we're at it: it's faster this way!
	charystart += Sequencer->bytepanning; //Apply byte panning!
	Sequencer->charystart = charystart; //What row to start with our pixels!

	//Some attribute controller special 8-bit mode support!
	Sequencer->active_nibblerate = 0; //Reset pixel load rate status & nibble load rate status for odd sized screens.
	Sequencer->extrastatus = &VGA->CRTC.extrahorizontalstatus[0]; //Start our extra status at the beginning of the row!

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
	Sequencer->tempx = 0; //Reset the rendering position from the framebuffer!
	VGA_Sequencer_calcScanlineData(VGA);
	VGA_Sequencer_updateRow(VGA, Sequencer); //Scanline has been changed!
	Sequencer->DACcounter = 0; //Reset the DAC counter!
	Sequencer->lastDACcolor = 0; //Reset the last DAC color!
}

//Retrace handlers!
void VGA_VRetrace(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	if (VGA->CRTC.y>Sequencer->yres) Sequencer->yres = VGA->CRTC.y; //Current y resolution!
	VGA->CRTC.y = 0; //Reset destination row!
	VGA_RenderOutput(Sequencer,VGA); //Render the output to the screen!
}

byte CGAMDARenderer = 0; //Render CGA style?

void VGA_HRetrace(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	CGALineSize = VGA->CRTC.x; //Update X resolution!
	if (VGA->CRTC.x>Sequencer->xres) Sequencer->xres = VGA->CRTC.x; //Current x resolution!
	VGA->CRTC.x = 0; //Reset destination column!
	if (!vretrace) //Not retracing vertically?
	{
		if (CGAMDARenderer) //CGA/MDA rendering mode?
		{
			drawCGALine(VGA); //Draw the current CGA line using NTSC colours!	
		}
		++VGA->CRTC.y; //Not retracing vertically? Next row on-screen!
	}
	++Sequencer->Scanline; //Next scanline to process!
}

//All renderers for active display parts:

typedef void (*VGA_Sequencer_Mode)(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo); //Render an active display pixel!

uint_32 CLUT16bit[0x10000]; //16-bit color lookup table!
uint_32 CLUT15bit[0x10000]; //15-bit color lookup table!

//Blank handler!
OPTINLINE void VGA_Blank_VGA(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	if (hretrace) return; //Don't handle during horizontal retraces!
	drawPixel(VGA, RGB(0x00, 0x00, 0x00)); //Draw blank!
	++VGA->CRTC.x; //Next x!
}

OPTINLINE void VGA_Blank_CGA(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	if (hretrace) return; //Don't handle during horizontal retraces!
	//Normally, we convert the pixel given using the VGA attribute, but in this case we need to apply NTSC conversion from reenigne.
	if (VGA->CRTC.x<NUMITEMS(CGALineBuffer)) //Valid pixel horizontally?
	{
		CGALineBuffer[VGA->CRTC.x] = 0; //Take the literal pixel color of the CGA for later NTSC conversion!
	}
	++VGA->CRTC.x; //Next x!
}


OPTINLINE void VGA_ActiveDisplay_noblanking_VGA(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	uint_32 DACcolor; //The latched color!
	if (hretrace) return; //Don't handle during horizontal retraces!
	//Active display!
	if (VGA->precalcs.DACmode&4) //Latch every 2 pixels(Hicolor mode 2 according to the chip documentation)?
	{
		if (((++Sequencer->DACcounter)&~1)==0) //To latch and not process yet? This is the least significant byte/bits!
		{
			Sequencer->lastDACcolor >>= (VGA->precalcs.DACmode&2)?8:4; //Latching 4 or 8 bits!
			if (VGA->precalcs.DACmode&2) //8-bits width?
			{
				Sequencer->lastDACcolor |= ((attributeinfo->attribute&0xFF)<<8); //Latching this attribute!
			}
			else //4-bits width?
			{
				Sequencer->lastDACcolor |= ((attributeinfo->attribute & 0xF) << 4); //Latching this attribute!
			}
			return; //Skip this data: we only latch every two pixels!
		}
		//Final latch?
		Sequencer->lastDACcolor >>= (VGA->precalcs.DACmode & 2) ? 8 : 4; //Latching 4 or 8 bits!
		if (VGA->precalcs.DACmode & 2) //8-bits width?
		{
			Sequencer->lastDACcolor |= ((attributeinfo->attribute & 0xFF) << 8); //Latching this attribute!
		}
		else //4-bits width?
		{
			Sequencer->lastDACcolor |= ((attributeinfo->attribute & 0xF) << 4); //Latching this attribute!
		}
	}
	else //Latch every pixel!
	{
		Sequencer->lastDACcolor = attributeinfo->attribute; //Latching this attribute!
	}

	//Draw the pixel that is latched!
	if (VGA->precalcs.DACmode&2) //16-bit color?
	{
		DACcolor = (Sequencer->lastDACcolor&0xFFFF); //Only use 16-bits!
		if (VGA->precalcs.DACmode&1) //15-bit color?
		{
			drawPixel(VGA, CLUT15bit[DACcolor]); //Draw the 15-bit color pixel!
		}
		else //16-bit color?
		{
			drawPixel(VGA, CLUT16bit[DACcolor]); //Draw the 16-bit color pixel!
		}
	}
	else //VGA compatibility mode?
	{
		DACcolor = (Sequencer->lastDACcolor&0xFF); //Only use 8-bits!
		drawPixel(VGA, VGA_DAC(VGA, DACcolor)); //Render through the 8-bit DAC!
	}
	++VGA->CRTC.x; //Next x!
}

OPTINLINE void VGA_ActiveDisplay_noblanking_CGA(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	if (hretrace) return; //Don't handle during horizontal retraces!
	//Active display!
	//Normally, we convert the pixel given using the VGA attribute, but in this case we need to apply NTSC conversion from reenigne.
	if (VGA->CRTC.x<NUMITEMS(CGALineBuffer)) //Valid pixel horizontally?
	{
		CGALineBuffer[VGA->CRTC.x] = attributeinfo->attribute; //Take the literal pixel color of the CGA for later NTSC conversion!
	}
	++VGA->CRTC.x; //Next x!
}

OPTINLINE void VGA_Overscan_noblanking_VGA(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	if (hretrace) return; //Don't handle during horizontal retraces!
	//Overscan!
	if (VGA->precalcs.AttributeController_16bitDAC==3) //16-bit color mode?
	{
		drawPixel(VGA,CLUT16bit[attributeinfo->attribute]); //Draw the 16-bit color pixel!
	}
	else //VGA compatibility mode?
	{
		drawPixel(VGA, VGA_DAC(VGA, VGA->precalcs.overscancolor)); //Draw overscan!
	}
	++VGA->CRTC.x; //Next x!
}

OPTINLINE void VGA_Overscan_noblanking_CGA(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	if (hretrace) return; //Don't handle during horizontal retraces!
	//Overscan!
	//Normally, we convert the pixel given using the VGA attribute, but in this case we need to apply NTSC conversion from reenigne.
	if (VGA->CRTC.x<NUMITEMS(CGALineBuffer)) //Valid pixel horizontally?
	{
		CGALineBuffer[VGA->CRTC.x] = VGA->precalcs.overscancolor; //Take the literal pixel color of the CGA for later NTSC conversion!
	}
	++VGA->CRTC.x; //Next x!
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

void updateVGASequencer_Mode(VGA_Type *VGA)
{
	VGA->precalcs.extrasignal = VGA->precalcs.graphicsmode?VGA_DISPLAYGRAPHICSMODE:0x0000; //Apply the current mode (graphics vs text mode)!
}

//Active display handler!
void VGA_ActiveDisplay_Text(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//Render our active display here! Start with text mode!		
	INLINEREGISTER byte nibbled=0; //Did we process two nibbles instead of one nibble?
	INLINEREGISTER word tempx = Sequencer->tempx, tempx2, tempx3; //Load tempx!

	othernibble: //Retrieve the current DAC index!
	Sequencer->activex = tempx++; //Active X!
	VGA_Sequencer_TextMode(VGA,Sequencer,&currentattributeinfo); //Get the color to render!
	if (VGA_AttributeController(&currentattributeinfo,VGA))
	{
		nibbled = 1; //We're processing 2 nibbles instead of 1 nibble!
		goto othernibble; //Apply the attribute through the attribute controller!
	}

	if (CGAMDARenderer) //CGA rendering mode?
	{
		VGA_ActiveDisplay_noblanking_CGA(VGA, Sequencer, &currentattributeinfo); //Blank or active display!
	}
	else
	{
		VGA_ActiveDisplay_noblanking_VGA(VGA, Sequencer, &currentattributeinfo); //Blank or active display!
	}

	if (*Sequencer->extrastatus++) //To write back the pixel clock every or every other pixel?
	{
		if (nibbled) //We've nibbled?
		{
			nibbled &= !(Sequencer->active_nibblerate ^= 1); //Nibble expired?
		}
		if (nibbled)
		{
			return; //Are we not finished with the nibble? Abort!
		}
		//Finished with the nibble&pixel? We're ready to check for the next one!
		tempx2 = tempx; //Load the current coordinate!
		tempx2 <<= 1; //Index!
		tempx3 = Sequencer->tempx; //Load the old coordinate!
		tempx3 <<= 1; //Index!
		if ((VGA->CRTC.charcolstatus[tempx3] != VGA->CRTC.charcolstatus[tempx2]) && ((VGA->CRTC.charcolstatus[tempx2]&VGA->precalcs.VideoLoadRateMask)==0)) //First of a new block? Reload our pixel buffer!
		{
			VGA_loadcharacterplanes(VGA, Sequencer, tempx); //Load data from the graphics planes!
		}
		Sequencer->tempx = tempx; //Write back tempx!
	}
}

void VGA_ActiveDisplay_Text_blanking(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//Render our active display here! Start with text mode!		
	INLINEREGISTER byte nibbled = 0; //Did we process two nibbles instead of one nibble?
	INLINEREGISTER word tempx = Sequencer->tempx, tempx2, tempx3; //Load tempx!

othernibble: //Retrieve the current DAC index!
	Sequencer->activex = tempx++; //Active X!
	VGA_Sequencer_TextMode(VGA, Sequencer, &currentattributeinfo); //Get the color to render!
	if (VGA_AttributeController(&currentattributeinfo, VGA))
	{
		nibbled = 1; //We're processing 2 nibbles instead of 1 nibble!
		goto othernibble; //Apply the attribute through the attribute controller!
	}

	if (CGAMDARenderer) //CGA rendering mode?
	{
		VGA_Blank_CGA(VGA, Sequencer, &currentattributeinfo); //Blank or active display!
	}
	else //VGA renderer?
	{
		VGA_Blank_VGA(VGA, Sequencer, &currentattributeinfo); //Blank or active display!
	}

	if (*Sequencer->extrastatus++) //To write back the pixel clock every or every other pixel?
	{
		if (nibbled) //We've nibbled?
		{
			nibbled &= !(Sequencer->active_nibblerate ^= 1); //Nibble expired?
		}
		if (nibbled)
		{
			return; //Are we not finished with the nibble? Abort!
		}
		//Finished with the nibble&pixel? We're ready to check for the next one!
		tempx2 = tempx; //Load the current coordinate!
		tempx2 <<= 1; //Index!
		tempx3 = Sequencer->tempx; //Load the old coordinate!
		tempx3 <<= 1; //Index!
		if ((VGA->CRTC.charcolstatus[tempx3] != VGA->CRTC.charcolstatus[tempx2]) && ((VGA->CRTC.charcolstatus[tempx2]&VGA->precalcs.VideoLoadRateMask)==0)) //First of a new block? Reload our pixel buffer!
		{
			VGA_loadcharacterplanes(VGA, Sequencer, tempx); //Load data from the graphics planes!
		}
		Sequencer->tempx = tempx; //Write back tempx!
	}
}

void VGA_ActiveDisplay_Graphics(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//Render our active display here! Start with text mode!		
	INLINEREGISTER byte nibbled = 0; //Did we process two nibbles instead of one nibble?
	INLINEREGISTER word tempx = Sequencer->tempx; //Load tempx!

othernibble: //Retrieve the current DAC index!
	Sequencer->activex = tempx++; //Active X!
	VGA_Sequencer_GraphicsMode(VGA, Sequencer, &currentattributeinfo); //Get the color to render!
	if (VGA_AttributeController(&currentattributeinfo, VGA))
	{
		nibbled = 1; //We're processing 2 nibbles instead of 1 nibble!
		goto othernibble; //Apply the attribute through the attribute controller!
	}

	if (CGAMDARenderer) //CGA rendering mode?
	{
		VGA_ActiveDisplay_noblanking_CGA(VGA, Sequencer, &currentattributeinfo); //Blank or active display!
	}
	else
	{
		VGA_ActiveDisplay_noblanking_VGA(VGA, Sequencer, &currentattributeinfo); //Blank or active display!
	}

	if (*Sequencer->extrastatus++) //To write back the pixel clock every or every other pixel?
	{
		if (nibbled) //We've nibbled?
		{
			nibbled &= !(Sequencer->active_nibblerate ^= 1); //Nibble expired?
		}
		if (nibbled)
		{
			Sequencer->graphicsx -= 2; //Decrease twice, undoing the increase in position!
			return; //Are we not finished with the nibble? Abort!
		}
		//Finished with the nibble&pixel? We're ready to check for the next one!
		if (((tempx & VGA->precalcs.graphicsReloadMask) == 0) && (((tempx>>3)&VGA->precalcs.VideoLoadRateMask)==0)) //First of a new block? Reload our pixel buffer!
		{
			VGA_loadcharacterplanes(VGA, Sequencer, tempx); //Load data from the graphics planes!
		}
		Sequencer->tempx = tempx; //Write back tempx!
	}
	else //Are we a graphics mode (nibbled or not) to undo increasing?
	{
		Sequencer->graphicsx -= (1 << nibbled); //Decrease twice when nibbled, else once, undoing the increase in position!
	}
}

void VGA_ActiveDisplay_Graphics_blanking(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	//Render our active display here! Start with text mode!		
	INLINEREGISTER byte nibbled = 0; //Did we process two nibbles instead of one nibble?
	INLINEREGISTER word tempx = Sequencer->tempx; //Load tempx!

othernibble: //Retrieve the current DAC index!
	Sequencer->activex = tempx++; //Active X!
	VGA_Sequencer_GraphicsMode(VGA, Sequencer, &currentattributeinfo); //Get the color to render!
	if (VGA_AttributeController(&currentattributeinfo, VGA))
	{
		nibbled = 1; //We're processing 2 nibbles instead of 1 nibble!
		goto othernibble; //Apply the attribute through the attribute controller!
	}

	if (CGAMDARenderer) //CGA rendering mode?
	{
		VGA_Blank_CGA(VGA, Sequencer, &currentattributeinfo); //Blank or active display!
	}
	else //VGA renderer?
	{
		VGA_Blank_VGA(VGA, Sequencer, &currentattributeinfo); //Blank or active display!
	}

	if (*Sequencer->extrastatus++) //To write back the pixel clock every or every other pixel?
	{
		if (nibbled) //We've nibbled?
		{
			nibbled &= !(Sequencer->active_nibblerate ^= 1); //Nibble expired?
		}
		if (nibbled)
		{
			Sequencer->graphicsx -= 2; //Decrease twice, undoing the increase in position!
			return; //Are we not finished with the nibble? Abort!
		}
		//Finished with the nibble&pixel? We're ready to check for the next one!
		if (((tempx &  VGA->precalcs.graphicsReloadMask) == 0) && (((tempx>>3)&VGA->precalcs.VideoLoadRateMask)==0)) //First of a new block? Reload our pixel buffer!
		{
			VGA_loadcharacterplanes(VGA, Sequencer, tempx); //Load data from the graphics planes!
		}
		Sequencer->tempx = tempx; //Write back tempx!
	}
	else //Are we a graphics mode (nibbled or not) to undo increasing?
	{
		Sequencer->graphicsx -= (1 << nibbled); //Decrease twice when nibbled, else once, undoing the increase in position!
	}
}

//Overscan handler!
void VGA_Overscan(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	if (CGAMDARenderer) //CGA rendering mode?
	{
		VGA_Overscan_noblanking_CGA(VGA,Sequencer,NULL); //Attribute info isn't used!
	}
	else //VGA renderer?
	{
		VGA_Overscan_noblanking_VGA(VGA, Sequencer, NULL); //Attribute info isn't used!
	}
}

void VGA_Overscan_blanking(SEQ_DATA *Sequencer, VGA_Type *VGA)
{
	if (CGAMDARenderer) //CGA rendering mode?
	{
		VGA_Blank_CGA(VGA, Sequencer, NULL); //Attribute info isn't used!
	}
	else //VGA renderer?
	{
		VGA_Blank_VGA(VGA, Sequencer, NULL); //Attribute info isn't used!
	}
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

	for (i=1;i<VGA_DISPLAYRENDERSIZE;i++) //Fill the normal entries!
	{
		//Total handler for total handlers!
		displayrenderhandler[1][i] = &VGA_NOP; //Do nothing when disabled: retrace does no output!
		displayrenderhandler[2][i] = &VGA_NOP; //Do nothing when disabled: total handler!
		displayrenderhandler[3][i] = &VGA_NOP; //Do nothing when disabled: total&retrace handler!
		
		//Rendering handler without retrace AND total!
		displayrenderhandler[0][i] = ((i&VGA_DISPLAYMASK)==VGA_DISPLAYACTIVE)?((i&VGA_DISPLAYGRAPHICSMODE)?((i&VGA_SIGNAL_BLANKING)?&VGA_ActiveDisplay_Graphics_blanking: &VGA_ActiveDisplay_Graphics): ((i&VGA_SIGNAL_BLANKING) ? &VGA_ActiveDisplay_Text_blanking : &VGA_ActiveDisplay_Text)):((i&VGA_SIGNAL_BLANKING) ? &VGA_Overscan : &VGA_Overscan_blanking); //Not retracing or any total handler = display/overscan!
	}

	for (i = 0;i < 0x10000;++i) //Create the 16&15-bit CLUT!
	{
		CLUT16bit[i] = RGB((byte)((((float)((i >> 11) & 0x1F) / (float)0x1F)*256.0f)), (byte)((((i >> 5) & 0x3F) / (float)0x3F)*256.0f), (byte)(((float)(i & 0x1F) / (float)0x1F)*256.0f)); //15-bit color lookup table (5:6:5 format)!
		CLUT15bit[i] = RGB((byte)((((float)((i>>10)&0x1F) / (float)0x1F)*256.0f)),(byte)((((i>>5)&0x1F)/(float)0x1F)*256.0f),(byte)(((float)(i & 0x1F) / (float)0x1F)*256.0f)); //15-bit color lookup table (5:5:5 format)!
	}
}