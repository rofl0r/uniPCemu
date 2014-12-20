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

typedef byte (*agetpixel)(VGA_Type *VGA, int x, VGA_AttributeInfo *Sequencer_Attributeinfo); //For our jumptable for getpixel!

/*

256 COLOR MODE

*/

/*uint_32 rowscanaddress; //For old graphic modes!
*/

//This should be OK, according to: http://www.nondot.org/sabre/Mirrored/GraphicsProgrammingBlackBook/gpbb31.pdf
byte getpixel256colorshiftmode(VGA_Type *VGA, int x, VGA_AttributeInfo *Sequencer_Attributeinfo) //256colorshiftmode getcolorplanes!
{
	byte plane = (x&3); //We walk through the planes!
	uint_32 sourceplanes[4] = {0x11111111,0x22222222,0x44444444,0x88888888}; //Our source plane translation table!
	Sequencer_Attributeinfo->attributesource = sourceplanes[plane]; //All our bits are from this plane!
	x >>= 2; //Get the pixel!
	return readVRAMplane(VGA,plane,GETSEQUENCER(VGA)->startmap+x,1); //The full offset of the plane all stuff is already done, so 0 at the end!
}

/*

SHIFT REGISTER INTERLEAVE MODE

*/

byte getpixelshiftregisterinterleavemode(VGA_Type *VGA, int x, VGA_AttributeInfo *Sequencer_Attributeinfo) //256colorshiftmode getcolorplanes!
{
	//Calculate the plane index!
	word planeindex = OPTMUL((x>>3),getVRAMMemAddrSize(VGA)); //The full offset, within the plane with our pixel!
	planeindex += GETSEQUENCER(VGA)->startmap; //Add the start address!
	
	//Determine low&high plane bases!
	byte planebase = (x>>2)&1; //Base plane (0/1)! OK!
	
	byte highplanebase = planebase;
	highplanebase |= 2; //plane high (2/3) OK!
	
	//Set row scan address!
	//rowscanaddress = scanlineoffset; //Copy for compatibility!
	
	//Read the low&high planes!
	byte planelow = readVRAMplane(VGA,planebase,planeindex,1); //Read low plane!
	byte planehigh = readVRAMplane(VGA,highplanebase,planeindex,1); //Read high plane!
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
	uint_32 sourceplanes[0x10] = {0x00001111,0x00001122,0x00001144,0x00001188,0x00002211,0x00002222,0x00002244,0x00002288,0x00004411,0x00004422,0x00004444,0x00004488,0x00008811,0x00008822,0x00008844,0x00008888};
	highplanebase <<= 4; //Shift left to make it compatible with the sourceplanes index!
	highplanebase |= planebase; //Get the final plane base to use!
	Sequencer_Attributeinfo->attributesource = sourceplanes[highplanebase]; //Lowest low bit bit!
	return result; //Give the result!
}

/*

SINGLE SHIFT MODE

*/

byte getpixelsingleshiftmode(VGA_Type *VGA, int x, VGA_AttributeInfo *Sequencer_Attributeinfo)
{
	//Should be OK?
	uint_32 offset; //No offset, don't process scanline start!
	offset = 0; //Init!
	//rowscanaddress = offset; //Use this as the row scan address!
	uint_32 pixeloffset = OPTMUL((x>>3),getVRAMMemAddrSize(VGA)); //Pixel offset!
	offset += pixeloffset; //The x coordinate, 8 pixels per byte!
	offset += GETSEQUENCER(VGA)->startmap; //VRAM start!
	
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
	result <<= 1; //Shift to next plane!
	
	Sequencer_Attributeinfo->attributesource = 0x00008421; //Our source planes!
	return result; //Give the result!
}

//Shiftregister: 2=ShiftRegisterInterleave, 1=Color256ShiftMode. Priority list: 1, 2, 0; So 1&3=256colorshiftmode, 2=ShiftRegisterInterleave, 0=SingleShift.
//When index0(VGA->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER.AlphaNumericModeDisable)=1, getColorPlanesAlphaNumeric
//When index1(IGNOREATTRPLANES)=1, getColorPlanesIgnoreAttrPlanes

//http://www.openwatcom.org/index.php/VGA_Fundamentals:
//Packed Pixel: Color 256 Shift Mode.
//Parallel Planes: Else case!
//Interleaved: Shift Register Interleave!

agetpixel getpixel_jmptbl[4] = {
				getpixelsingleshiftmode,
				getpixelshiftregisterinterleavemode,
				getpixel256colorshiftmode,
				getpixel256colorshiftmode
				}; //All the getpixel functionality!
/*

Core functions!

*/

/*void VGA_Sequencer_GraphicsMode(VGA_Type *VGA,VGA_AttributeInfo *Sequencer_Attributeinfo, word tempx,word tempy,word x,word Scanline,uint_32 bytepanning) //Render graphics mode screen!
{

	/VGA->CurrentScanLine[0] = 1; //Plot font always if needed!
	Sequencer_Attributeinfo->attribute_graphics = 1; //Graphics attribute!
	if ((tempx>=getxres(VGA)) || (tempy>=getyres(VGA))) //Over x/y resolution?
	{
		Sequencer_Attributeinfo->attribute = 0; //No attribute!
		Sequencer_Attributeinfo->attributesource = 0; //No source!
		return; //Nothing here!
	}

	Sequencer_Attributeinfo->attribute = getpixel_jmptbl[VGA->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.ShiftRegister](VGA,tempx,Sequencer_Attributeinfo);
	//As the rest of the rows are simply a copy of ours, don't process them!
	/
}*/

void VGA_Sequencer_GraphicsMode(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo)
{
	attributeinfo->attribute_graphics = 1; //Graphics attribute!
	attributeinfo->attribute = getpixel_jmptbl[VGA->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.ShiftRegister](VGA,Sequencer->tempx,attributeinfo);
}