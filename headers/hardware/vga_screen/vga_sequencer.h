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
#include "headers/hardware/vga.h" //VGA!
#include "headers/hardware/vga_screen/vga_attributecontroller.h" //Attribute controller!

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
	word tempx; //Current X (Sequencer)!
	word activex; //Real current X to process (Sequencer)!
	byte doublepixels; //Current doublepixels status.
	//Newline recalculation
	byte newline_ready; //We don't have a newline, so we don't need recalcs?
	uint_32 startmap; //Where our map starts!
	uint_32 bytepanning; //How much byte panning is used?
	
	//Text mode information about the current scanline!
	word chary; //Character y!
	word charinner_y; //Inner y base of character!
	uint_32 charystart; //Start of the row in VRAM!


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
} SEQ_DATA; //Sequencer used data!

void VGA_Sequencer(VGA_Type *VGA); //Process sequencer scanline etc.!
void VGA_Sequencer_calcScanlineData(VGA_Type *VGA);

//Retrieve the Sequencer from a VGA!
#define GETSEQUENCER(VGA) ((SEQ_DATA *)(VGA->Sequencer))

#endif