#include "headers/types.h" //Basic types!
#include "headers/emu/gpu/gpu.h" //GPU!
#include "headers/hardware/vga.h" //VGA!
#include "headers/hardware/vga_screen/vga_attributecontroller.h" //Attribute controller!
#include "headers/hardware/vga_screen/vga_sequencer_graphicsmode.h" //Graphics mode!
#include "headers/hardware/vga_screen/vga_vram.h" //Our VRAM support!
#include "headers/cpu/interrupts.h" //For get/putpixel variant!
#include "headers/support/log.h" //Logging support!
#include "headers/mmu/mmu.h" //For BIOS data!
#include "headers/header_dosbox.h" //For comp.

byte LOG_RENDER_BYTES = 0; //Log all rendering of scanline #0!

typedef byte (*agetpixel)(VGA_Type *VGA, SEQ_DATA *Sequencer, word x, VGA_AttributeInfo *Sequencer_Attributeinfo); //For our jumptable for getpixel!

/*

256 COLOR MODE

*/

//This should be OK, according to: http://www.nondot.org/sabre/Mirrored/GraphicsProgrammingBlackBook/gpbb31.pdf
byte getpixel256colorshiftmode(VGA_Type *VGA, SEQ_DATA *Sequencer, word x, VGA_AttributeInfo *Sequencer_Attributeinfo) //256colorshiftmode getcolorplanes!
{
	register word plane; //Plane
	register word activex; //X!
	register byte part;
	register byte result;

	//First: calculate the nibble to shift into our result!
	part = x;
	part &= 1; //Take the lowest bit only!
	part ^= 1; //Reverse: High nibble=bit 0 set, Low nibble=bit 0 cleared
	part <<= 2; //High nibble=4, Low nibble=0

	activex = x; //Load x!
	activex >>= 1; //Ignore the part number to get our pixel: Every part is half a pixel, so increase every 2 pixels!
	//const static uint_32 sourceplanes[4] = {0x11111111,0x22222222,0x44444444,0x88888888}; //Our source plane translation table!
	plane = activex;
	plane &= 3; //We walk through the planes!
	activex >>= 2; //Get the pixel (every 4 increment)!
	result = readVRAMplane(VGA,plane,Sequencer->charystart+activex,1); //The full offset of the plane all stuff is already done, so 0 at the end!
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
	word planeindex = OPTMUL((x>>3),getVRAMMemAddrSize(VGA)); //The full offset, within the plane with our pixel!
	planeindex += Sequencer->charystart; //Add the start address!
	
	//Determine low&high plane bases!
	byte planebase = (x>>2)&1; //Base plane (0/1)! OK!
	
	//Read the low&high planes!
	byte planelow = readVRAMplane(VGA,planebase,planeindex,1); //Read low plane!
	planebase |= 2; //High plane!
	byte planehigh = readVRAMplane(VGA,planebase,planeindex,1); //Read high plane!
	//byte shift = 6-((x&3)<<1); //OK!
	
	//Determine the shift for our pixels!
	byte shift = x; //Start x!
	shift &= 3;
	shift <<= 1;
	shift = 6-shift; //Get the shift!
	
	//Get the pixel
	planelow >>= shift;
	planelow &= 3;
	planehigh >>= shift;
	planehigh &= 3;
	planehigh <<= 2;
	byte result = planelow;
	result |= planehigh;
	
	//Source plane bits for all possibilities!
	return result; //Give the result!
}

/*

SINGLE SHIFT MODE

*/

byte getpixelsingleshiftmode(VGA_Type *VGA, SEQ_DATA *Sequencer, word x, VGA_AttributeInfo *Sequencer_Attributeinfo)
{
	//Should be OK?
	uint_32 offset; //No offset, don't process scanline start!
	//rowscanaddress = offset; //Use this as the row scan address!
	offset = OPTMUL((x>>3),getVRAMMemAddrSize(VGA)); //The x coordinate, 8 pixels per byte!
	offset += Sequencer->charystart; //VRAM start!
	
	byte bit = (x&7); //The bit in the byte (from the start of VRAM byte)!
	
	//16-color mode!
	byte result; //Init result!
	
	//Standard VGA processing!
	result = getBitPlaneBit(VGA,3,offset,bit,1); //Add plane to the result!
	result <<= 1; //Shift to next plane!
	result |= getBitPlaneBit(VGA,2,offset,bit,1); //Add plane to the result!
	result <<= 1; //Shift to next plane!
	result |= getBitPlaneBit(VGA,1,offset,bit,1); //Add plane to the result!
	result <<= 1; //Shift to next plane!
	result |= getBitPlaneBit(VGA,0,offset,bit,1); //Add plane to the result!
	
	//Sequencer_Attributeinfo->attributesource = 0x00008421; //Our source planes!
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