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

void initVGAclocks(); //Init all clocks used!

double VGA_VerticalRefreshRate(VGA_Type *VGA); //Scanline speed for one line in Hz!

void VGA_Sequencer_calcScanlineData(VGA_Type *VGA);

void updateVGASequencer_Mode(VGA_Type *VGA);

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

#endif
