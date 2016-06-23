#ifndef VGA_SEQUENCER_H
#define VGA_SEQUENCER_H

#include "headers/types.h" //Basic types!

/*typedef struct
{
	//Current scanline data:
	uint_32 rowscancounter; //Row scan counter needed by the VRAM accesses!
	uint_32 bytepanning; //Byte panning (if any)
	uint_32 horizontalstart;
	int_32 pixelmove; //Ammount to substract or add to the ammount of pixels each time!
	word startmap; //Start map for this row?

	//Precalcs for different modes for us only!
	word rowstart; //Start of a graphics/text row (current row/line)
	word CurrentScanLineStart; //Start of current scan line!
} SEQ_PRECALCS; //We are needed for the VGA!
*/
#include "headers/emu/gpu/gpu.h" //GPU!
#include "headers/hardware/vga/vga.h" //VGA!
#include "headers/hardware/vga/vga_attributecontroller.h" //Attribute controller!

//Our IRQ to use when enabled (EGA compatibility)!
#define VGA_IRQ 2

typedef struct
{
	//Global info
	//SEQ_PRECALCS precalcs[1024]; //Precalcs for all scanlines!
	//SEQ_PRECALCS *currentPrecalcs; //Current precalcs!
	
	word xres;
	word yres; //The full resolution currently counted!
	
	word Scanline; //Current scanline to process! Also the VGA's row scan counter!
	word Scanline_sync; //Current scanline for sync signal!
	
	
	//Pixel specific
	VGA_AttributeInfo Attributeinfo; //Attribute info, kept between pixels!
	word x; //Current pixel on the scanline!
	word x_sync; //Current pixel on the sync!
	word tempx; //Current X (Sequencer)!
	word activex; //Real current X to process (Sequencer)!
	byte active_pixelrate; //Current pixel rate position (0-1)!
	byte active_nibblerate; //Current nibble rate position (0-1)!
	//Newline recalculation
	byte newline_ready; //We don't have a newline, so we don't need recalcs?
	uint_32 startmap; //Where our map starts!
	uint_32 bytepanning; //How much byte panning is used?
	
	//Text mode information about the current scanline!
	word chary; //Character y!
	word charinner_y; //Inner y base of character!
	uint_32 charystart; //Start of the row in VRAM!

	byte pixelshiftcount; //Our calculated pixel shift count!
	byte presetrowscan; //Our calculated preset row scan!

	uint_32 rowscancounter;


	/*

	Pixel timing

	*/

	uint_64 totalpixeltime; //Taken time of one pixel!
	uint_64 totalpixels; //Taken pixels!

	//Rendering timing:
	uint_64 totalrendertime; //Taken time of one pixel!
	uint_64 totalrenders; //Taken pixels!

	//Same as above, but split by section!
	uint_64 totalpixeltimepixel; //Taken time of one pixel!
	uint_64 totalpixelspixel; //Taken pixels!
	uint_64 lastpixeltimepixel; //Last pixel processed!
	uint_64 totalpixeltimeattribute; //Taken time of one pixel!
	uint_64 totalpixelsattribute; //Taken pixels!
	uint_64 lastpixeltimeattribute; //Last attribute processed!
	uint_64 totalpixeltimedac; //Taken time of one pixel!
	uint_64 totalpixelsdac; //Taken pixels!
	uint_64 lastpixeltimedac; //Last DAC processed!

	//Scanline and newline function speed
	uint_64 totalscanlinetime; //Taken time of one pixel!
	uint_64 totalscanlines; //Taken pixels!
	uint_64 lastscanlinetime; //Last processed!
	uint_64 totalnewlinetime; //Taken time of one pixel!
	uint_64 totalnewlines; //Taken pixels!
	uint_64 lastnewlinetime; //Last processed!


	//Pixel precision for timing with sound:
	uint_64 pixelstorender; //Ammount of pixels to render to delay!
	uint_64 pixelsrendered; //Ammount of pixels rendered, cleared on above overflow.

	word *extrastatus; //Our current extra status!
	byte *graphicsx; //Current graphics pixel in the buffer!
	byte DACcounter; //DAC latch counter!
	uint_32 lastDACcolor; //Last latched DAC color!

	//Actual VGA counters according to the documentation!
	word memoryaddressclock; //The memory address clock itself!
	byte memoryaddressclockdivider; //The memory address clock divider!
	byte memoryaddressclockdivider2; //SVGA memory address clock divider extension!
	word memoryaddress; //The current memory address to apply!
	byte linearcounterdivider; //The linear counter clock divider itself!
} SEQ_DATA; //Sequencer used data!

float VGA_VerticalRefreshRate(VGA_Type *VGA); //Scanline speed for one line in Hz!

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
