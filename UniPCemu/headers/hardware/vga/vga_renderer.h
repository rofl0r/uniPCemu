/*

Copyright (C) 2019  Superfury

This file is part of UniPCemu.

UniPCemu is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

UniPCemu is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with UniPCemu.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef VGA_RENDERER_H
#define VGA_RENDERER_H

#include "headers/types.h" //Basic types!

#include "headers/emu/gpu/gpu.h" //GPU!
#include "headers/hardware/vga/vga.h" //VGA!
#include "headers/hardware/vga/vga_attributecontroller.h" //Attribute controller!

typedef struct
{
	//Global info
	//SEQ_PRECALCS precalcs[1024]; //Precalcs for all scanlines!
	//SEQ_PRECALCS *currentPrecalcs; //Current precalcs!
	
	word xres;
	word yres; //The full resolution currently counted!
	
	word Scanline; //Current scanline to process! Also the VGA's row scan counter!
	
	
	//Pixel specific
	VGA_AttributeInfo Attributeinfo; //Attribute info, kept between pixels!
	word x; //Current pixel on the scanline!
	//Newline recalculation
	uint_32 startmap; //Where our map starts!
	uint_32 bytepanning; //How much byte panning is used?
	
	//Text mode information about the current scanline!
	word chary; //Character y!
	byte charinner_y; //Inner y base of character!
	uint_32 charystart; //Start of the row in VRAM!

	byte pixelshiftcount; //Our calculated pixel shift count!
	byte presetrowscan; //Our calculated preset row scan!
	byte activepresetrowscan; //Our calculated preset row scan!

	uint_32 rowscancounter;


	/*

	Pixel timing

	*/

	word *extrastatus; //Our current extra status!
	byte *graphicsx; //Current graphics pixel in the buffer!
	byte *textx; //Current text pixel location!
	byte DACcounter; //DAC latch counter!
	uint_32 lastDACcolor; //Last latched DAC color!

	//Actual VGA counters according to the documentation!
	uint_32 memoryaddress; //The current memory address to apply!
	byte memoryaddressclock; //The memory address clock itself!
	byte linearcounterdivider; //The linear counter clock divider itself!
	byte is_topwindow; //Are we the top window?
	word topwindowCRTbase; //What top-window scanline are we at?
	byte pixelclockdivider; //Pixel clock divider to divide the pixel clock by!
	byte currentpixelclock; //What are we dividing by the pixel clock divider!

	byte frame_pixelshiftcount; //Our pre-calculated pixel shift count!
	byte frame_presetrowscan; //Our pre-calculated preset row scan!
	byte frame_activepresetrowscan; //Our pre-calculated preset row scan!
	uint_32 frame_bytepanning; //Our pre-calculated byte panning!
	byte frame_AttributeModeControlRegister_PixelPanningMode; //Pixel panning mode enabled?
	byte frame_characterheight; //Character height!
	word frame_topwindowstart;
} SEQ_DATA; //Sequencer used data!

#include "headers/packed.h"
typedef union PACKED
{
	uint_32 loadedplanes;
	byte splitplanes[4]; //All read planes for the current processing!
	struct
	{
		byte plane0;
		byte plane1;
		byte plane2;
		byte plane3;
	};
} LOADEDPLANESCONTAINER; //All four loaded planes!
#include "headers/endpacked.h"

void initVGAclocks(byte extension); //Init all clocks used!

DOUBLE VGA_VerticalRefreshRate(VGA_Type *VGA); //Scanline speed for one line in Hz!

void VGA_Sequencer_calcScanlineData(VGA_Type *VGA);

void updateVGASequencer_Mode(VGA_Type *VGA);
void updateVGADAC_Mode(VGA_Type* VGA);

//Retrieve the Sequencer from a VGA!
#define GETSEQUENCER(VGA) ((SEQ_DATA *)(VGA->Sequencer))

//Our different rendering handlers!
void VGA_NOP(SEQ_DATA *Sequencer, VGA_Type *VGA); //NOP for pixels!

void initStateHandlers(); //Initialise the state handlers for the VGA to run!

typedef void (*DisplaySignalHandler)(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal); //Our signal handler for all signals!
typedef void (*DisplayRenderHandler)(SEQ_DATA *Sequencer, VGA_Type *VGA); //Our rendering handler for all signals!

//Total&Retrace handlers!
void VGA_VTotal(SEQ_DATA *Sequencer, VGA_Type *VGA);
void VGA_VTotalEnd(SEQ_DATA *Sequencer, VGA_Type *VGA);
void VGA_HTotal(SEQ_DATA *Sequencer, VGA_Type *VGA);
void VGA_VRetrace(SEQ_DATA *Sequencer, VGA_Type *VGA);
void VGA_HRetrace(SEQ_DATA *Sequencer, VGA_Type *VGA);
void updateLightPenMode(VGA_Type *VGA);
void updateCGAMDARenderer(); //Update the renderer to use!
void updateSequencerPixelDivider(VGA_Type* VGA, SEQ_DATA* Sequencer); //Update the sequencer pixel divider!

#endif
