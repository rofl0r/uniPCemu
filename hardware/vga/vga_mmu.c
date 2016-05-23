#include "headers/hardware/vga/vga.h" //Basic defs!
#include "headers/support/log.h" //Loggin support!
#include "headers/hardware/vga/vga_vram.h" //VRAM support!
#include "headers/mmu/mmuhandler.h" //Handling support!
#include "headers/hardware/pci.h" //PCI support!
#include "headers/hardware/vga/vga_cga_mda.h" //CGA/MDA support!

uint_32 VGA_VRAM_START = 0xA0000; //VRAM start address default!
uint_32 VGA_VRAM_END = 0xC0000; //VRAM end address default!

byte VGA_RAMEnable = 1; //Is our RAM enabled?
byte VGA_MemoryMapSelect = 0; //What memory map is active?

OPTINLINE void VGA_updateLatches()
{
	//Update the latch the software can read.
	getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.GraphicsControllerDataLatches.LatchN = getActiveVGA()->registers->ExternalRegisters.DATALATCH.latchplane[getActiveVGA()->registers->GraphicsRegisters.REGISTERS.READMAPSELECTREGISTER.ReadMapSelect]; //Update the latch the software reads (R/O)
}

void VGA_updateVRAMmaps(VGA_Type *VGA)
{
	VGA_RAMEnable = VGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.RAM_Enable; //RAM enabled?
	VGA_MemoryMapSelect = VGA->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER.MemoryMapSelect; //Update the selected memory map!
	switch (VGA_MemoryMapSelect) //What memory map?
	{
	case 0: //A0000-BFFFF (128K region)?
		VGA_VRAM_START = 0xA0000; //Start!
		VGA_VRAM_END = 0xC0000; //End!
		break;
	case 1: //A0000-AFFFF (64K region)?
		VGA_VRAM_START = 0xA0000; //Start!
		VGA_VRAM_END = 0xB0000; //End!
		break;
	case 2: //B0000-B7FFF (32K region)?
		VGA_VRAM_START = 0xB0000; //Start!
		VGA_VRAM_END = 0xB8000; //End!
		break;
	case 3: //B8000-BFFFF (32K region)?
		VGA_VRAM_START = 0xB8000; //Start!
		VGA_VRAM_END = 0xC0000; //End!
		break;
	}
}

/*

VRAM base offset!

*/

OPTINLINE byte is_A000VRAM(uint_32 linearoffset) //In VRAM (for CPU), offset=real memory address (linear memory)?
{
	INLINEREGISTER uint_32 addr;
	if (VGA_RAMEnable) //VRAM Access by CPU Enabled?
	{
		addr = linearoffset;
		if (addr>=VGA_VRAM_START) //After range?
			if (addr<VGA_VRAM_END) //Before range?
				return 1;  //In range?
	}
	return 0; //Don't read/write from VRAM!
}

//And now the input/output functions for segment 0xA000 (starting at offset 0)

/*

Special operations for write!

*/

OPTINLINE uint_32 LogicalOperation(uint_32 input)
{
	switch (getActiveVGA()->registers->GraphicsRegisters.REGISTERS.DATAROTATEREGISTER.LogicalOperation)
	{
	case 0x00:	/* None */
		return input; //Unmodified
	case 0x01:	/* AND */
		return input & getActiveVGA()->registers->ExternalRegisters.DATALATCH.latch;
	case 0x02:	/* OR */
		return input | getActiveVGA()->registers->ExternalRegisters.DATALATCH.latch;
	case 0x03:	/* XOR */
		return input ^ getActiveVGA()->registers->ExternalRegisters.DATALATCH.latch;
	};
	return input; //Unknown, just give the input!
}

OPTINLINE uint_32 BitmaskOperation(uint_32 input, byte bitmaskregister)
{
	INLINEREGISTER uint_32 result = 0; //The result built!
	INLINEREGISTER uint_32 mask,inputdata; //Latch and extended mask!
	//Load the mask to use, extend to all four planes!
	mask = getActiveVGA()->ExpandTable[bitmaskregister]; //Load the current mask(one plane) expanded!
	//Convert the value&latch to result using the mask!
	inputdata = input; //Load the input to process!
	inputdata &= mask; //Apply the mask to get the bits to turn on!
	result |= inputdata; //Apply the bits to turn on to the result!
	mask ^= 0xFFFFFFFF; //Flip the mask bits to get the bits to retrieve from the latch!
	inputdata = getActiveVGA()->registers->ExternalRegisters.DATALATCH.latch; //Load the latch!
	inputdata &= mask; //Apply the mask to get the bits to turn on!
	result |= inputdata; //Apply the bits to turn on to the result!
	return result; //Give the resulting value!
}

/*

Core read/write operations!

*/

typedef uint_32 (*VGA_WriteMode)(uint_32 data);

uint_32 VGA_WriteMode0(uint_32 data) //Read-Modify-Write operation!
{
	INLINEREGISTER byte curplane;
	data = (byte)ror((byte)data, getActiveVGA()->registers->GraphicsRegisters.REGISTERS.DATAROTATEREGISTER.RotateCount); //Rotate it! Keep 8-bit data!
	data = getActiveVGA()->ExpandTable[data]; //Make sure the data is on the all planes!

	curplane = 0;
	do
	{
		if (getActiveVGA()->registers->GraphicsRegisters.REGISTERS.ENABLESETRESETREGISTER.EnableSetReset&(1 << curplane)) //Enable set/reset? (Mode 3 ignores this flag)
		{
			data = (data&(~getActiveVGA()->FillTable[(1 << curplane)])) | getActiveVGA()->FillTable[getActiveVGA()->registers->GraphicsRegisters.REGISTERS.SETRESETREGISTER.SetReset&(1 << curplane)]; //Turn all those bits off, and the set/reset plane ON=0xFF for the plane and OFF=0x00!
		}
	} while (++curplane!=4);
	data = LogicalOperation(data); //Execute the logical operation!
	data = BitmaskOperation(data, getActiveVGA()->registers->GraphicsRegisters.REGISTERS.BITMASKREGISTER); //Execute the bitmask operation!
	return data; //Give the resulting data!
}

uint_32 VGA_WriteMode1(uint_32 data) //Video-to-video transfer
{
	return getActiveVGA()->registers->ExternalRegisters.DATALATCH.latch; //Use the latch!
}

uint_32 VGA_WriteMode2(uint_32 data) //Write color to all pixels in the source address byte of VRAM. Use Bit Mask Register.
{
	data = getActiveVGA()->FillTable[data]; //Replicate across all 8 bits of their respective planes.
	data = LogicalOperation(data); //Execute the logical operation!
	data = BitmaskOperation(data, getActiveVGA()->registers->GraphicsRegisters.REGISTERS.BITMASKREGISTER); //Execute the bitmask operation fully!
	return data;
}

uint_32 VGA_WriteMode3(uint_32 data) //Ignore enable set reset register!
{
	data = ror(data, getActiveVGA()->registers->GraphicsRegisters.REGISTERS.DATAROTATEREGISTER.RotateCount); //Rotate it! Keep 8-bit data!
	data &= getActiveVGA()->registers->GraphicsRegisters.REGISTERS.BITMASKREGISTER; //AND with the Bit Mask field.
	data = BitmaskOperation(getActiveVGA()->ExpandTable[getActiveVGA()->registers->GraphicsRegisters.REGISTERS.SETRESETREGISTER.SetReset], data); //Use the generated data on the Set/Reset register
	return data;
}

OPTINLINE void VGA_WriteModeOperation(byte planes, uint_32 offset, byte val)
{
	static const VGA_WriteMode VGA_WRITE[4] = {VGA_WriteMode0,VGA_WriteMode1,VGA_WriteMode2,VGA_WriteMode3}; //All write modes!
	INLINEREGISTER byte curplane; //For plane loops!
	INLINEREGISTER uint_32 data; //Default to the value given!
	data = VGA_WRITE[getActiveVGA()->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.WriteMode]((uint_32)val); //What write mode?

	byte planeenable = getActiveVGA()->registers->SequencerRegisters.REGISTERS.MAPMASKREGISTER.MemoryPlaneWriteEnable; //What planes to try to write to!
	planeenable &= planes; //The actual planes to write to!
	byte curplanemask=1;
	curplane = 0;
	do //Process all planes!
	{
		if (planeenable&curplanemask) //Modification of the plane?
		{
			writeVRAMplane(getActiveVGA(),curplane,offset,data&0xFF); //Write the plane from the data!
		}
		data >>= 8; //Shift to the next plane!
		curplanemask <<= 1; //Next plane!
	} while (++curplane!=4);
}

OPTINLINE void loadlatch(uint_32 offset)
{
	getActiveVGA()->registers->ExternalRegisters.DATALATCH.latchplane[0] = readVRAMplane(getActiveVGA(),0,offset); //Plane 0
	getActiveVGA()->registers->ExternalRegisters.DATALATCH.latchplane[1] = readVRAMplane(getActiveVGA(),1,offset); //Plane 1
	getActiveVGA()->registers->ExternalRegisters.DATALATCH.latchplane[2] = readVRAMplane(getActiveVGA(),2,offset); //Plane 2
	getActiveVGA()->registers->ExternalRegisters.DATALATCH.latchplane[3] = readVRAMplane(getActiveVGA(),3,offset); //Plane 3
	VGA_updateLatches(); //Update the latch data mirroring!
}

typedef byte (*VGA_ReadMode)(byte planes, uint_32 offset);

byte VGA_ReadMode0(byte planes, uint_32 offset) //Read mode 0: Just read the normal way!
{
	INLINEREGISTER byte curplane;
	curplane = 0;
	do
	{
		if (planes&1) //Read from this plane?
		{
			return readVRAMplane(getActiveVGA(), curplane, offset); //Read directly from vram using the selected plane!
		}
		planes >>= 1; //Next plane!
	} while (++curplane!=4);
	return 0; //Unknown plane! Give 0!
}

byte VGA_ReadMode1(byte planes, uint_32 offset) //Read mode 1: Compare display memory with color defined by the Color Compare field. Colors Don't care field are not considered.
{
	INLINEREGISTER byte curplane;
	INLINEREGISTER byte result=0; //The value we return, default to 0 if undefined!
	//Each bit in the result represents one comparision between the reference color, with the bit being set if the comparision is true.
	curplane = 0;
	do//Check all planes!
	{
		if (getActiveVGA()->registers->GraphicsRegisters.REGISTERS.COLORDONTCAREREGISTER.ColorCare&(1 << curplane)) //We care about this plane?
		{
			if (readVRAMplane(getActiveVGA(), curplane, offset) == getActiveVGA()->registers->GraphicsRegisters.REGISTERS.COLORCOMPAREREGISTER.ColorCompare) //Equal?
			{
				result |= (1 << curplane); //Set the bit: the comparision is true!
			}
		}
	} while (++curplane!=4); //Process all planes!
	return result; //Give the value!
}

OPTINLINE byte VGA_ReadModeOperation(byte planes, uint_32 offset)
{
	static const VGA_ReadMode READ[2] = {VGA_ReadMode0,VGA_ReadMode1}; //Read modes!
	loadlatch(offset); //Load the latches!

	return READ[getActiveVGA()->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.ReadMode](planes,offset); //What read mode?
}

/*

The r/w operations from the CPU!

*/

//decodeCPUaddress(Write from CPU=1; Read from CPU=0, offset (from VRAM start address), planes to read/write (4-bit mask), offset to read/write within the plane(s)).
OPTINLINE void decodeCPUaddress(byte towrite, uint_32 offset, byte *planes, uint_32 *realoffset)
{
	byte oddevenmemorymode;
	INLINEREGISTER uint_32 realoffsettmp;
	INLINEREGISTER byte calcplanes;

	oddevenmemorymode = getActiveVGA()->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.OddEvenMode; //This enforces Odd/Even memory addressing during access from memory when Host_OE is enabled to provide CGA compatibility!
	if (oddevenmemorymode) goto forceoddevenmode; //Force odd/even mode when enabled!

	if (getActiveVGA()->registers->SequencerRegisters.REGISTERS.SEQUENCERMEMORYMODEREGISTER.Chain4Enable) //Chain 4 mode?
	{
		calcplanes = realoffsettmp = offset; //Original offset to start with!
		calcplanes &= 0x3; //Lower 2 bits determine the plane!
		*planes = (1 << calcplanes); //Give the planes to write to!
		realoffsettmp &= 0xFFFC; //Rest of bits, multiples of 4 won't get written! Used to be 0xFFFB, but should be multiples of 4 ignored, so clear bit 2, changed to 0xFFFC (multiple addresses of 4 with plane bits ignored) to make it correctly linear with 256 color modes (dword mode)!
		*realoffset = realoffsettmp; //Give the offset!
		return; //Done!
	}

	if (getActiveVGA()->registers->SequencerRegisters.REGISTERS.SEQUENCERMEMORYMODEREGISTER.OEDisabled) //Odd/Even mode disabled?
	{
		if (towrite) //Writing access?
		{
			calcplanes = 0xF; //Write to all planes possible, map mask register does the rest!
		}
		else
		{
			calcplanes = 1; //Load plane 0!
			calcplanes <<= getActiveVGA()->registers->GraphicsRegisters.REGISTERS.READMAPSELECTREGISTER.ReadMapSelect; //Take this plane!
		}
		*planes = calcplanes; //The planes to apply!
		*realoffset = offset; //Load the offset directly!
		return; //Done!
	}

	//Odd/even mode used (compatiblity case)?
	//Do the same as VPC!
	forceoddevenmode:
	calcplanes = realoffsettmp = offset; //Take the default offset!
	if (getActiveVGA()->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER.EnableOddEvenMode || oddevenmemorymode) //Read using odd/even addressing?
	{
		calcplanes &= 1; //Take 1 bit to determine the plane (0/1)!
		realoffsettmp &= 0xFFFE; //Clear bit 0 for our result!
	}
	else
	{
		calcplanes = 0; //Use plane 0 always!
	}
	*realoffset = realoffsettmp; //Give the calculated offset!
	if ((VGA_MemoryMapSelect == 1) && (!oddevenmemorymode)) //Memory map mode 1?
	{
		//Determine by Page Select!
		if (getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.OE_HighPage) //Lower page?
		{
			*planes = (0x1 << calcplanes); //Convert to used plane 0/1!
		}
		else //Upper page?
		{
			*planes = (0x4 << calcplanes); //Convert to used plane 2/3!
		}
	}
	else //Default behaviour?
	{
		*planes = (0x5 << calcplanes); //Convert to used plane (0&2 or 1&3)!
	}
}

byte planes; //What planes to affect!
uint_32 realoffset; //What offset to affect!

void applyCGAMDAOffset(uint_32 *offset)
{
	if (CGAEMULATION_ENABLED(getActiveVGA())) //CGA?
	{
		*offset &= 0x3FFF; //Wrap around 16KB!
	}
	else if (MDAEMULATION_ENABLED(getActiveVGA())) //MDA?
	{
		*offset &= 0xFFF; //Wrap around 4KB!
	}
}

byte VGAmemIO_rb(uint_32 offset, byte *value)
{
	if (is_A000VRAM(offset)) //VRAM and within range?
	{
		offset -= VGA_VRAM_START; //Calculate start offset into VRAM!
		applyCGAMDAOffset(&offset); //Apply CGA/MDA offset if needed!
		decodeCPUaddress(0, offset, &planes, &realoffset); //Our VRAM offset starting from the 32-bit offset (A0000 etc.)!
		*value = VGA_ReadModeOperation(planes, realoffset); //Apply the operation on read mode!
		return 1; //Read!
	}
	return 0; //Not read!
}

byte VGAmemIO_wb(uint_32 offset, byte value)
{
	if (is_A000VRAM(offset)) //VRAM and within range?
	{
		offset -= VGA_VRAM_START; //Calculate start offset into VRAM!
		applyCGAMDAOffset(&offset); //Apply CGA/MDA offset if needed!
		decodeCPUaddress(1, offset, &planes, &realoffset); //Our VRAM offset starting from the 32-bit offset (A0000 etc.)!
		VGA_WriteModeOperation(planes, realoffset, value); //Apply the operation on write mode!
		return 1; //Written!
	}
	return 0; //Not written!
}

void VGAmemIO_reset()
{
	//Register/reset memory mapped I/O!
	MMU_resetHandlers("VGA");
	MMU_registerWriteHandler(&VGAmemIO_wb,"VGA");
	MMU_registerReadHandler(&VGAmemIO_rb,"VGA");
}