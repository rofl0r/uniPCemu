#define VGA_SEQUENCER_GRAPHICSMODE

#include "headers/types.h" //Basic types!
#include "headers/emu/gpu/gpu.h" //GPU!
#include "headers/hardware/vga.h" //VGA!
#include "headers/hardware/vga_screen/vga_attributecontroller.h" //Attribute controller!
#include "headers/hardware/vga_screen/vga_sequencer_graphicsmode.h" //Graphics mode!
#include "headers/hardware/vga_screen/vga_vram.h" //Our VRAM support!
#include "headers/hardware/vga_screen/vga_crtcontroller.h" //CRT Controller for finishing up!
#include "headers/cpu/interrupts.h" //For get/putpixel variant!
#include "headers/support/log.h" //Logging support!
#include "headers/mmu/mmu.h" //For BIOS data!
#include "headers/header_dosbox.h" //For comp.

byte planesbuffer[4]; //All read planes for the current processing!
byte pixelbuffer[8]; //All 8 pixels decoded from the planesbuffer!

//256 color mode still doesn't work for some reason!

void VGA_loadgraphicsplanes(VGA_Type *VGA, SEQ_DATA *Sequencer, word x) //Load the planes!
{
	//Horizontal logic
	word location; //The location loaded into the planesbuffer!
	location = x; //X!
	location >>= 3; //We take portions of 8 pixels, so increase our location every 8 pixels!
	
	location <<= getVGAShift(VGA); //Apply VGA shift: the shift is the ammount to move at a time!

	//Row logic
	location += Sequencer->charystart; //Apply the line and start map to retrieve!

	//Now calculate and give the planes to be used!
	planesbuffer[0] = readVRAMplane(VGA, 0, location, 1); //Read plane 0!
	planesbuffer[1] = readVRAMplane(VGA, 1, location, 1); //Read plane 1!
	planesbuffer[2] = readVRAMplane(VGA, 2, location, 1); //Read plane 2!
	planesbuffer[3] = readVRAMplane(VGA, 3, location, 1); //Read plane 3!
	//Now the buffer is ready to be processed into pixels!
}

/*

256 COLOR MODE

*/

//This should be OK, according to: http://www.nondot.org/sabre/Mirrored/GraphicsProgrammingBlackBook/gpbb31.pdf
void load256colorshiftmode() //256-color shift mode!
{
	register byte part, plane, result, x;
	for (x = 0; x < 8;) //Buffer 8 nibbles!
	{
		//Determine the nibble first: this is used for the attribute controller to combine if needed!
		plane = part = x; //Load x into part&plane for processing!
		part &= 1; //Take the lowest bit only for the part!
		part ^= 1; //Reverse: High nibble=bit 0 set, Low nibble=bit 0 cleared
		part <<= 2; //High nibble=4, Low nibble=0

		//Determine plane and offset within the plane!
		plane >>= 1; //We change planes after every 2 parts!

		//Now read the correct nibble!
		result = planesbuffer[plane]; //Read the plane buffered!
		result >>= part; //Shift to the required part (low/high nibble)!
		result &= 0xF; //Only the low resulting nibble is used!
		pixelbuffer[x++] = result; //Save the result in the buffer!
	} //Give the result!
}

/*

SHIFT REGISTER INTERLEAVE MODE

*/

void loadpackedshiftmode() //Packed shift mode!
{
	register byte temp, temp2, tempbuffer; //A buffer for our current pixel!
	pixelbuffer[0] = pixelbuffer[1] = pixelbuffer[2] = pixelbuffer[3] = planesbuffer[2]; //Load high plane!
	pixelbuffer[4] = pixelbuffer[5] = pixelbuffer[6] = pixelbuffer[7] = planesbuffer[3]; //Load high plane!
	pixelbuffer[0] >>= 4;
	pixelbuffer[1] >>= 2;
	pixelbuffer[3] <<= 2; //Shift to the high part!
	pixelbuffer[4] >>= 4;
	pixelbuffer[5] >>= 2;
	pixelbuffer[7] <<= 2; //Shift to the high part!

	pixelbuffer[0] &= 0xFC;
	pixelbuffer[1] &= 0xFC;
	pixelbuffer[2] &= 0xFC;
	pixelbuffer[3] &= 0xFC;
	pixelbuffer[4] &= 0xFC;
	pixelbuffer[5] &= 0xFC;
	pixelbuffer[6] &= 0xFC;
	pixelbuffer[7] &= 0xFC; //Clear bits 0-1 and 4+!

	//First byte!
	tempbuffer = temp = planesbuffer[0]; //Load low plane!
	tempbuffer &= 3;
	pixelbuffer[3] |= tempbuffer;
	temp >>= 2; //Shift to the next data!
	tempbuffer = temp;
	tempbuffer &= 3;
	pixelbuffer[2] |= tempbuffer;
	temp >>= 2; //Shift to the next data!
	tempbuffer = temp;
	tempbuffer &= 3;
	pixelbuffer[1] |= tempbuffer;
	temp >>= 2; //Shift to the next data!
	temp &= 3;
	pixelbuffer[0] |= temp;

	//Second byte!
	tempbuffer = temp = planesbuffer[1]; //Load low plane!
	tempbuffer &= 3;
	pixelbuffer[7] |= tempbuffer;
	temp >>= 2; //Shift to the next data!
	tempbuffer = temp;
	tempbuffer &= 3;
	pixelbuffer[5] |= tempbuffer;
	temp >>= 2; //Shift to the next data!
	tempbuffer = temp;
	tempbuffer &= 3;
	pixelbuffer[5] |= tempbuffer;
	temp >>= 2; //Shift to the next data!
	temp &= 3;
	pixelbuffer[4] |= temp;
	//Now all 8 pixels are loaded!
}

/*

SINGLE SHIFT MODE

*/

void loadplanarshiftmode() //Planar shift mode!
{
	//16-color mode!
	byte pixel, counter=8, result;
	for (pixel = 7; counter--;)
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

		pixelbuffer[pixel--] = result; //Load the result for usage!
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

void VGA_Sequencer_GraphicsMode(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	static Handler loadpixel_jmptbl[4] = {
				loadplanarshiftmode,
				loadpackedshiftmode,
				load256colorshiftmode,
				load256colorshiftmode
				}; //All the getpixel functionality!
	byte currentbuffer;
	attributeinfo->fontpixel = 1; //Graphics attribute is always font enabled!
	currentbuffer = Sequencer->activex; //Current x coordinate!
	currentbuffer &= 7; //We're buffering every 8 pixels!
	if (!currentbuffer) //First of a block? Reload our pixel buffer!
	{
		VGA_loadgraphicsplanes(VGA, Sequencer, Sequencer->activex); //Load data from the graphics planes!
		loadpixel_jmptbl[VGA->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.ShiftRegister](); //Load the pixels from the buffer!
	}
	attributeinfo->attribute = pixelbuffer[currentbuffer]; //Give the current pixel, loaded with our block!
}