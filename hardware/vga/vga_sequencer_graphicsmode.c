#define VGA_SEQUENCER_GRAPHICSMODE

#include "headers/types.h" //Basic types!
#include "headers/emu/gpu/gpu.h" //GPU!
#include "headers/hardware/vga/vga.h" //VGA!
#include "headers/hardware/vga/vga_attributecontroller.h" //Attribute controller!
#include "headers/hardware/vga/vga_sequencer_graphicsmode.h" //Graphics mode!
#include "headers/hardware/vga/vga_vram.h" //Our VRAM support!
#include "headers/hardware/vga/vga_crtcontroller.h" //CRT Controller for finishing up!
#include "headers/cpu/interrupts.h" //For get/putpixel variant!
#include "headers/support/log.h" //Logging support!
#include "headers/mmu/mmu.h" //For BIOS data!
#include "headers/header_dosbox.h" //For comp.

extern byte planesbuffer[4]; //All read planes for the current processing!
byte pixelbuffer[8]; //All 8 pixels decoded from the planesbuffer!

//256 color mode still doesn't work for some reason!

/*

256 COLOR MODE

*/

void load256colorshiftmode() //256-color shift mode!
{
	register byte data;
	//Now all planes are loaded for our calculation!
	data = planesbuffer[0]; //First plane!
	pixelbuffer[1] = (data & 0xF); //Take second pixel!
	data >>= 4; //Shift high to low libble!
	pixelbuffer[0] = data; //Take first pixel!

	data = planesbuffer[1]; //Second plane!
	pixelbuffer[3] = (data & 0xF); //Take second pixel!
	data >>= 4; //Shift high to low libble!
	pixelbuffer[2] = data; //Take first pixel!

	data = planesbuffer[2]; //Third plane!
	pixelbuffer[5] = (data & 0xF); //Take second pixel!
	data >>= 4; //Shift high to low libble!
	pixelbuffer[4] = data; //Take first pixel!

	data = planesbuffer[3]; //Fourth plane!
	pixelbuffer[7] = (data & 0xF); //Take second pixel!
	data >>= 4; //Shift high to low libble!
	pixelbuffer[6] = data; //Take first pixel!
}

/*

SHIFT REGISTER INTERLEAVE MODE

*/

void loadpackedshiftmode() //Packed shift mode!
{
	register byte temp, tempbuffer; //A buffer for our current pixel!
	pixelbuffer[0] = pixelbuffer[1] = pixelbuffer[2] = pixelbuffer[3] = planesbuffer[2]; //Load high plane!
	pixelbuffer[4] = pixelbuffer[5] = pixelbuffer[6] = pixelbuffer[7] = planesbuffer[3]; //Load high plane!
	pixelbuffer[0] >>= 4;
	pixelbuffer[1] >>= 2;
	pixelbuffer[3] <<= 2; //Shift to the high part!
	pixelbuffer[4] >>= 4;
	pixelbuffer[5] >>= 2;
	pixelbuffer[7] <<= 2; //Shift to the high part!

	pixelbuffer[0] &= 0xC;
	pixelbuffer[1] &= 0xC;
	pixelbuffer[2] &= 0xC;
	pixelbuffer[3] &= 0xC;
	pixelbuffer[4] &= 0xC;
	pixelbuffer[5] &= 0xC;
	pixelbuffer[6] &= 0xC;
	pixelbuffer[7] &= 0xC; //Clear bits 0-1 and 4+!

	//First byte!
	tempbuffer = temp = planesbuffer[0]; //Load low plane!
	tempbuffer &= 3;
	pixelbuffer[3] |= tempbuffer;
	tempbuffer = (temp >>= 2); //Shift to the next data!
	tempbuffer &= 3;
	pixelbuffer[2] |= tempbuffer;
	tempbuffer = (temp >>= 2); //Shift to the next data!
	tempbuffer &= 3;
	pixelbuffer[1] |= tempbuffer;
	temp >>= 2; //Shift to the next data!
	tempbuffer &= 3;
	pixelbuffer[0] |= temp;

	//Second byte!
	tempbuffer = temp = planesbuffer[1]; //Load low plane!
	tempbuffer &= 3;
	pixelbuffer[7] |= tempbuffer;
	tempbuffer = (temp >>= 2); //Shift to the next data!
	tempbuffer &= 3;
	pixelbuffer[6] |= tempbuffer;
	tempbuffer = (temp >>= 2); //Shift to the next data!
	tempbuffer &= 3;
	pixelbuffer[5] |= tempbuffer;
	temp >>= 2; //Shift to the next data!
	tempbuffer &= 3;
	pixelbuffer[4] |= temp;
}

/*

SINGLE SHIFT MODE

*/

void loadplanarshiftmode() //Planar shift mode!
{
	//16-color mode!
	register byte pixel=8, result=0;
	for (;pixel;)
	{
		result = (planesbuffer[3] & 1); //Load plane 3!
		planesbuffer[3] >>= 1; //Next bit!
		result <<= 1; //Next bit!

		result |= (planesbuffer[2] & 1); //Load plane 2!
		planesbuffer[2] >>= 1; //Next bit!
		result <<= 1; //Next bit!

		result |= (planesbuffer[1] & 1); //Load plane 1!
		planesbuffer[1] >>= 1; //Next bit!
		result <<= 1; //Next bit!

		result |= (planesbuffer[0] & 1); //Load plane 0!
		planesbuffer[0] >>= 1; //Next bit!

		pixelbuffer[--pixel] = result; //Load the result for usage!
	}
}

//Shiftregister: 2=ShiftRegisterInterleave, 1=Color256ShiftMode. Priority list: 1, 2, 0; So 1&3=256colorshiftmode, 2=ShiftRegisterInterleave, 0=SingleShift.
//When index0(VGA->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER.AlphaNumericModeDisable)=1, getColorPlanesAlphaNumeric
//When index1(IGNOREATTRPLANES)=1, getColorPlanesIgnoreAttrPlanes

//http://www.openwatcom.org/index.php/VGA_Fundamentals:
//Packed Pixel: Color 256 Shift Mode.
//Parallel Planes: Else case!
//Interleaved: Shift Register Interleave!

/*

Core functions!

*/

void VGA_GraphicsDecoder(VGA_Type *VGA, word loadedlocation) //Graphics decoder!
{
	static Handler loadpixel_jmptbl[4] = {
		loadplanarshiftmode,
		loadpackedshiftmode,
		load256colorshiftmode,
		load256colorshiftmode
	}; //All the getpixel functionality!
	loadpixel_jmptbl[VGA->precalcs.GraphicsModeRegister_ShiftRegister](); //Split the pixels from the buffer!
}

void VGA_Sequencer_GraphicsMode(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	attributeinfo->attribute = pixelbuffer[Sequencer->activex&7]; //Give the current pixel, loaded with our block!
	attributeinfo->fontpixel = 1; //Graphics attribute is always foreground by default!
}