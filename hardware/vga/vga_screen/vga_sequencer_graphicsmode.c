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
byte get256colorshiftmode(VGA_Type *VGA, SEQ_DATA *Sequencer, word x) //256-color shift mode!
{
	register word activex; //X!
	register byte part, plane, result;

	//Determine the nibble first: this is used for the attribute controller to combine if needed!
	part = activex = x; //Load x into part&activeX for processing!
	part &= 1; //Take the lowest bit only for the part!
	part ^= 1; //Reverse: High nibble=bit 0 set, Low nibble=bit 0 cleared
	part <<= 2; //High nibble=4, Low nibble=0

	activex >>= VGA->precalcs.characterclockshift; //Apply pixel DIVIDE when needed!

	activex >>= 1; //Ignore the part number to get our nibble: Every part is a nibble, so increase every 2 pixels!

	plane = activex; //Load plane!
	activex >>= getVGAShift(VGA); //Apply VGA shift: the shift is the ammount moved through planes in our case, 1, 2, or 4 pixels moved per 4 pixels!

	//Determine plane and offset within the plane!
	plane &= 3; //We walk through the planes!

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

byte getpackedshiftmode(VGA_Type *VGA, SEQ_DATA *Sequencer, word x) //Packed shift mode!
{
	//Calculate the plane index!
	register word shift,bitshift,tempx,planebase,planeindex;
	register byte planelow, planehigh;
	tempx = x; //Init tempx!
	tempx >>= VGA->precalcs.characterclockshift; //Apply pixel DIVIDE when needed!

	tempx <<= 1; //The index goes at half the rate of the plane: every 2 planes processed, one index is taken (8 pixels)!

	planebase = shift = tempx; //Start with the x value and load it for usage in base and shift!

	planebase >>= 2; //The base changes state every 4 pixels (1 byte processed)

	planeindex = planebase; //Take the same rate as the base for the index, but ...
	planeindex >>= getVGAShift(VGA);  //Apply VGA shift: the shift is the ammount moved through planes in our case, 1, 2, or 4 pixels moved per 8 pixels!
	planeindex += Sequencer->charystart; //Add the start address and start map!

	planebase &= 1; //Base plane (0/1)! OK!

	//Read the low&high planes!
	planelow = readVRAMplane(VGA,planebase,planeindex,1); //Read low plane!
	planebase |= 2; //Take the high plane now!
	planehigh = readVRAMplane(VGA,planebase,planeindex,1); //Read high plane!
	
	//Determine the shift for our pixels!
	shift &= 3; //The shift rotates every 4 pixels
	shift <<= 1; //Every rotate contains 2 bits
	bitshift = 6; //Shift for pixel 0!
	bitshift -= shift; //Get the shift with the actual shift!

	//Get the pixel
	planelow >>= bitshift; //Shift plane low correct.
	planelow &= 3; //We're 2 bits only

	planehigh >>= bitshift; //Shift plane high correct
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

byte getplanarshiftmode(VGA_Type *VGA, SEQ_DATA *Sequencer, word x) //Planar shift mode!
{
	//16-color mode!
	register byte result; //Init result!
	register word offset, bit;

	offset = x; //Load x!
	offset >>= VGA->precalcs.characterclockshift; //Apply pixel DIVIDE when needed!

	bit = offset;
	bit &= 7; //The bit in the byte (from the start of VRAM byte)! 8 bits = 8 pixels!

	offset >>= 3; //Shift to the byte: every 8 pixels we go up 1 byte!
	offset >>= getVGAShift(VGA); //Apply VGA shift! Every 8 pixels we add this ammount to the index!
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
				getplanarshiftmode,
				getpackedshiftmode,
				get256colorshiftmode,
				get256colorshiftmode
				}; //All the getpixel functionality!
	attributeinfo->fontpixel = 1; //Graphics attribute is always font enabled!
	attributeinfo->attribute = getpixel_jmptbl[VGA->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.ShiftRegister](VGA,Sequencer,Sequencer->activex);
}