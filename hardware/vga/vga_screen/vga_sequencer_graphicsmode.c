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

typedef byte (*agetpixel)(VGA_Type *VGA, SEQ_DATA *Sequencer, word x); //For our jumptable for getpixel!

/*

256 COLOR MODE

*/

//This should be OK, according to: http://www.nondot.org/sabre/Mirrored/GraphicsProgrammingBlackBook/gpbb31.pdf
byte getpixel256colorshiftmode(VGA_Type *VGA, SEQ_DATA *Sequencer, word x) //256colorshiftmode getcolorplanes!
{
	register word activex; //X!
	register byte part, plane, result;

	//First: calculate the nibble to shift into our result!
	part = activex = x; //Load x into part&activeX for processing!
	part &= 1; //Take the lowest bit only for the part!
	part ^= 1; //Reverse: High nibble=bit 0 set, Low nibble=bit 0 cleared
	part <<= 2; //High nibble=4, Low nibble=0
	activex >>= 1; //Ignore the part number to get our nibble: Every part is a nibble, so increase every 2 pixels!

	//Now we're just a simple index to maintain and find the correct byte!
	activex >>= VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.DIV2; //Apply DIVIDE by 2 when needed!

	//Determine plane and offset within the plane!
	plane = activex; //Load plane!
	plane &= 3; //We walk through the planes!
	activex >>= 2; //Apply the pixel (every 4 increment)!
	
	//Apply startmap!
	activex += Sequencer->charystart; //Apply the line and start map to retrieve!

	result = readVRAMplane(VGA, plane, activex, 1); //The full offset of the plane all stuff is already done, so 0 at the end!
	result >>= part; //Shift to the required part (low/high nibble)!
	result &= 0xF; //Only the low resulting nibble is used!
	return result; //Give the result!
}

/*

SHIFT REGISTER INTERLEAVE MODE

*/

byte getpixelshiftregisterinterleavemode(VGA_Type *VGA, SEQ_DATA *Sequencer, word x) //256colorshiftmode getcolorplanes!
{
	//Calculate the plane index!
	register word shift,tempx,planebase,planeindex;
	register byte planelow, planehigh;
	tempx = x; //Init tempx!
	tempx >>= VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.DIV2; //Apply DIVIDE by 2 when needed!

	planebase = shift = tempx; //Start with the x value!
	planebase >>= 2; //The base changes state every 4 pixels (1 byte processed)
	planeindex = planebase; //Take the same rate as the base for the index, but ...
	planeindex >>= 1; //The index goes at half the rate of the plane: every 2 planes processed, one index is taken!

	planebase &= 1; //Base plane (0/1)! OK!

	planeindex += Sequencer->charystart; //Add the start address and start map!
	
	//Read the low&high planes!
	planelow = readVRAMplane(VGA,planebase,planeindex,1); //Read low plane!
	planebase |= 2; //Take the high plane now!
	planehigh = readVRAMplane(VGA,planebase,planeindex,1); //Read high plane!
	
	//Determine the shift for our pixels!
	shift &= 3; //The shift rotates every 4 pixels
	shift <<= 1; //Every rotate contains 2 bits
	shift = 6; //Shift for pixel 0!
	shift -= shift; //Get the shift!
	
	//Get the pixel
	planelow >>= shift; //Shift plane low correct.
	planelow &= 3; //We're 2 bits only

	planehigh >>= shift; //Shift plane high correct
	planehigh &= 3; //We're 2 bits only

	planehigh <<= 2; //Prepare high plane for combination!

	//Build the result!
	planelow |= planehigh; //Add high plane to the result!
	
	//Source plane bits for all possibilities!
	return planelow; //Give the result!
}

/*

SINGLE SHIFT MODE

*/

byte getpixelsingleshiftmode(VGA_Type *VGA, SEQ_DATA *Sequencer, word x)
{
	//16-color mode!
	register byte result; //Init result!
	register word offset, bit;

	offset = x; //Load x!
	offset >>= VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.DIV2; //Apply DIVIDE by 2 when needed!

	bit = offset;
	bit &= 7; //The bit in the byte (from the start of VRAM byte)! 8 bits = 8 pixels!

	offset >>= 3; //Shift to the byte: every 8 pixels we go up 1 byte!
	offset += Sequencer->charystart; //VRAM start of row and start map!
	
	//Standard VGA processing!
	result = getBitPlaneBit(VGA,3,offset,bit,1); //Add plane to the result!
	result <<= 1; //Shift to next plane!
	result |= getBitPlaneBit(VGA,2,offset,bit,1); //Add plane to the result!
	result <<= 1; //Shift to next plane!
	result |= getBitPlaneBit(VGA,1,offset,bit,1); //Add plane to the result!
	result <<= 1; //Shift to next plane!
	result |= getBitPlaneBit(VGA,0,offset,bit,1); //Add plane to the result!
	return result; //Give the result!
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
	static agetpixel getpixel_jmptbl[4] = {
				getpixelsingleshiftmode,
				getpixelshiftregisterinterleavemode,
				getpixel256colorshiftmode,
				getpixel256colorshiftmode
				}; //All the getpixel functionality!
	attributeinfo->fontpixel = 1; //Graphics attribute is always font enabled!
	attributeinfo->attribute = getpixel_jmptbl[VGA->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.ShiftRegister](VGA,Sequencer,Sequencer->activex);
}