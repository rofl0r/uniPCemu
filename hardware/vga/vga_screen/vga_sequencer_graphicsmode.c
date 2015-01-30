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

typedef byte (*agetpixel)(VGA_Type *VGA, SEQ_DATA *Sequencer, word x, VGA_AttributeInfo *Sequencer_Attributeinfo); //For our jumptable for getpixel!

/*

256 COLOR MODE

*/

//This should be OK, according to: http://www.nondot.org/sabre/Mirrored/GraphicsProgrammingBlackBook/gpbb31.pdf
byte getpixel256colorshiftmode(VGA_Type *VGA, SEQ_DATA *Sequencer, word x, VGA_AttributeInfo *Sequencer_Attributeinfo) //256colorshiftmode getcolorplanes!
{
	register word plane, activex; //X!
	register byte part, result;

	//First: calculate the nibble to shift into our result!
	part = activex = x;
	part &= 1; //Take the lowest bit only!
	part ^= 1; //Reverse: High nibble=bit 0 set, Low nibble=bit 0 cleared
	part <<= 2; //High nibble=4, Low nibble=0

	activex >>= 1; //Ignore the part number to get our nibble: Every part is a nibble, so increase every 2 pixels!
	activex >>= VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.DIV2; //Apply DIVIDE by 2 when needed!
	activex += Sequencer->charystart; //Apply the line to retrieve!

	//Determine plane and offset within the plane!
	plane = activex;
	plane &= 3; //We walk through the planes!
	activex >>= 2; //Get the pixel (every 4 increment)!
	
	//Apply startmap!
	activex += Sequencer->startmap; //What start address?

	result = readVRAMplane(VGA,plane,activex,1); //The full offset of the plane all stuff is already done, so 0 at the end!
	result >>= part; //Shift to the required part (low/high nibble)!
	result &= 0xF; //Only the low resulting nibble is used!
	return result; //Give the result!
}

/*

SHIFT REGISTER INTERLEAVE MODE

*/

byte getpixelshiftregisterinterleavemode(VGA_Type *VGA, SEQ_DATA *Sequencer, word x, VGA_AttributeInfo *Sequencer_Attributeinfo) //256colorshiftmode getcolorplanes!
{
	//Calculate the plane index!
	register word shift,tempx,planebase,planeindex;

	tempx = shift = planebase = x; //Init!
	tempx >>= 3;
	tempx >>= VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.DIV2; //Apply DIVIDE by 2 when needed!
	tempx += Sequencer->charystart; //Add the start address!
	planebase >>= 2;

	planebase &= 1; //Base plane (0/1)! OK!
	planeindex = tempx;
	planeindex += Sequencer->startmap; //What start address?
	
	//Determine low&high plane bases!
	
	//Read the low&high planes!
	register byte planelow = readVRAMplane(VGA,planebase,planeindex,1); //Read low plane!
	planebase |= 2; //High plane!
	register byte planehigh = readVRAMplane(VGA,planebase,planeindex,1); //Read high plane!
	//byte shift = 6-((x&3)<<1); //OK!
	
	//Determine the shift for our pixels!
	shift &= 3;
	shift <<= 1;
	shift = 6-shift; //Get the shift!
	
	//Get the pixel
	planelow >>= shift;
	planelow &= 3;

	planehigh >>= shift;
	planehigh &= 3;

	planehigh <<= 2; //Prepare high plane for addition!

	//Build the result!
	planelow |= planehigh; //Add high plane!
	
	//Source plane bits for all possibilities!
	return planelow; //Give the result!
}

/*

SINGLE SHIFT MODE

*/

byte getpixelsingleshiftmode(VGA_Type *VGA, SEQ_DATA *Sequencer, word x, VGA_AttributeInfo *Sequencer_Attributeinfo)
{
	//16-color mode!
	register byte result; //Init result!
	register uint_32 offset, bit;

	if (x >= 100)
	{
		result = 0; //Do nothing, breakpoint here!
	}

	bit = x;
	bit &= 7; //The bit in the byte (from the start of VRAM byte)!
	x >>= 3; //Shift to the byte!
	x >>= VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.DIV2; //Apply DIVIDE by 2 when needed!
	offset = Sequencer->charystart; //VRAM start!
	offset += x; //The x coordinate, 8 pixels per byte!
	offset += Sequencer->startmap; //What start address?
	
	//Standard VGA processing!
	result = getBitPlaneBit(VGA,3,offset,bit,1); //Add plane to the result!
	result <<= 1; //Shift to next plane!
	result |= getBitPlaneBit(VGA,2,offset,bit,1); //Add plane to the result!
	result <<= 1; //Shift to next plane!
	result |= getBitPlaneBit(VGA,1,offset,bit,1); //Add plane to the result!
	result <<= 1; //Shift to next plane!
	result |= getBitPlaneBit(VGA,0,offset,bit,1); //Add plane to the result!
	return 0xE; //Yellow/brown always!
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
	attributeinfo->attribute = getpixel_jmptbl[VGA->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.ShiftRegister](VGA,Sequencer,Sequencer->activex,attributeinfo);
}