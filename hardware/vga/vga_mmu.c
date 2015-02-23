#include "headers/hardware/vga.h" //Basic defs!
#include "headers/support/log.h" //Loggin support!
#include "headers/hardware/vga_screen/vga_vram.h" //VRAM support!
#include "headers/mmu/mmuhandler.h" //Handling support!

extern VGA_Type *ActiveVGA; //Active VGA!
extern byte is_loadchartable; //Loading character table?
uint_32 VGA_VRAM_START = 0xA0000; //VRAM start address default!
extern byte LOG_VRAM_WRITES; //Log VRAM writes?

OPTINLINE void VGA_updateLatches()
{
	//Update the latch the software can read.
	ActiveVGA->registers->CRTControllerRegisters.REGISTERS.GraphicsControllerDataLatches.LatchN = ActiveVGA->registers->ExternalRegisters.DATALATCH.latchplane[ActiveVGA->registers->GraphicsRegisters.REGISTERS.READMAPSELECTREGISTER.ReadMapSelect]; //Update the latch the software reads (R/O)
}

/*

VRAM base offset!

*/

OPTINLINE byte is_A000VRAM(uint_32 linearoffset) //In VRAM (for CPU), offset=real memory address (linear memory)?
{
	if (ActiveVGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.RAM_Enable) //VRAM Access by CPU Enabled?
	{
		switch (ActiveVGA->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER.MemoryMapSelect) //What memory map?
		{
		case 0: //A0000-BFFFF (128K region)?
			VGA_VRAM_START = 0; //Start!
			return ((linearoffset>=0) && (linearoffset<0x20000)); //In range?
			break;
		case 1: //A0000-AFFFF (64K region)?
			VGA_VRAM_START = 0; //Start!
			return ((linearoffset>=0) && (linearoffset<0x10000)); //In range?
			break;
		case 2: //B0000-B7FFF (32K region)?
			VGA_VRAM_START = 0x10000; //Start!
			return ((linearoffset>=0x10000) && (linearoffset<0x18000)); //In range?
			break;
		case 3: //B8000-BFFFF (32K region)?
			VGA_VRAM_START = 0x18000; //Start!
			return ((linearoffset>=0x18000) && (linearoffset<0x20000)); //In range?
			break;
		}
	}
	return 0; //Don't read/write from VRAM!
}

//And now the input/output functions for segment 0xA000 (starting at offset 0)

/*

Special operations for write!

*/

OPTINLINE uint_32 LogicalOperation(uint_32 input)
{
	switch (ActiveVGA->registers->GraphicsRegisters.REGISTERS.DATAROTATEREGISTER.LogicalOperation)
	{
	case 0x00:	/* None */
		return input; //Unmodified
	case 0x01:	/* AND */
		return input & ActiveVGA->registers->ExternalRegisters.DATALATCH.latch;
	case 0x02:	/* OR */
		return input | ActiveVGA->registers->ExternalRegisters.DATALATCH.latch;
	case 0x03:	/* XOR */
		return input ^ ActiveVGA->registers->ExternalRegisters.DATALATCH.latch;
	};
	return input; //Unknown, just give the input!
}

OPTINLINE uint_32 BitmaskOperation(uint_32 input, byte bitmaskregister)
{
	uint_32 result = 0; //Default: the result!4
	byte bit;
	for (bit=0;bit<32;bit++) //Process all available bits!
	{
		if (bitmaskregister&(1<<(bit&7))) //Use bit from input?
		{
			result |= input&(1<<bit); //Use the bit from input!
		}
		else //Use bit from latch?
		{
			result |= ActiveVGA->registers->ExternalRegisters.DATALATCH.latch&(1<<bit); //Use the bit from the latch!
		}
	}
	return result; //Give the result!
}

/*

Core read/write operations!

*/

extern byte LOG_VRAM_WRITES; //Log VRAM writes?
OPTINLINE void VGA_WriteModeOperation(byte planes, uint_32 offset, byte val)
{
	byte curplane; //For plane loops!
	uint_32 data = val; //Default to the value given!
	switch (ActiveVGA->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.WriteMode) //What write mode?
	{
	case 0: //Read-Modify-Write operation!
		data = (byte)ror((byte)val,ActiveVGA->registers->GraphicsRegisters.REGISTERS.DATAROTATEREGISTER.RotateCount); //Rotate it! Keep 8-bit data!
		data = ActiveVGA->ExpandTable[data]; //Make sure the data is on the all planes!
		
		for (curplane=0;curplane<4;curplane++)
		{
			if (ActiveVGA->registers->GraphicsRegisters.REGISTERS.ENABLESETRESETREGISTER.EnableSetReset&(1<<curplane)) //Enable set/reset? (Mode 3 ignores this flag)
			{
				data = (data&(~ActiveVGA->FillTable[(1<<curplane)])) | ActiveVGA->FillTable[ActiveVGA->registers->GraphicsRegisters.REGISTERS.SETRESETREGISTER.SetReset&(1<<curplane)]; //Turn all those bits off, and the set/reset plane ON=0xFF for the plane and OFF=0x00!
			}
		}
		data = LogicalOperation(data); //Execute the logical operation!
		data = BitmaskOperation(data,ActiveVGA->registers->GraphicsRegisters.REGISTERS.BITMASKREGISTER); //Execute the bitmask operation!
		break;
	case 1: //Video-to-video transfer
		data = ActiveVGA->registers->ExternalRegisters.DATALATCH.latch; //Use the latch!
		break;
	case 2: //Write color to all pixels in the source address byte of VRAM. Use Bit Mask Register.
		data = ActiveVGA->FillTable[data]; //Replicate across all 8 bits of their respective planes.
		data = LogicalOperation(data); //Execute the logical operation!
		data = BitmaskOperation(data,ActiveVGA->registers->GraphicsRegisters.REGISTERS.BITMASKREGISTER); //Execute the bitmask operation fully!
		break;
	case 3: //Ignore enable set reset register!
		data = ror(val,ActiveVGA->registers->GraphicsRegisters.REGISTERS.DATAROTATEREGISTER.RotateCount); //Rotate it! Keep 8-bit data!
		data &= ActiveVGA->registers->GraphicsRegisters.REGISTERS.BITMASKREGISTER; //AND with the Bit Mask field.
		data = BitmaskOperation(ActiveVGA->ExpandTable[ActiveVGA->registers->GraphicsRegisters.REGISTERS.SETRESETREGISTER.SetReset],data); //Use the generated data on the Set/Reset register
		break;
	}

	byte planeenable = ActiveVGA->registers->SequencerRegisters.REGISTERS.MAPMASKREGISTER.MemoryPlaneWriteEnable; //What planes to try to write to!
	planeenable &= planes; //The actual planes to write to!
	for (curplane=0;curplane<4;curplane++) //Process all planes!
	{
		if (planeenable&(1<<curplane)) //Modification of the plane?
		{
			writeVRAMplane(ActiveVGA,curplane,offset,data&0xFF); //Write the plane from the data!
		}
		data >>= 8; //Shift to the next plane!
	}
}

OPTINLINE void loadlatch(uint_32 offset)
{
	ActiveVGA->registers->ExternalRegisters.DATALATCH.latchplane[0] = readVRAMplane(ActiveVGA,0,offset,0); //Plane 0
	ActiveVGA->registers->ExternalRegisters.DATALATCH.latchplane[1] = readVRAMplane(ActiveVGA,1,offset,0); //Plane 1
	ActiveVGA->registers->ExternalRegisters.DATALATCH.latchplane[2] = readVRAMplane(ActiveVGA,2,offset,0); //Plane 2
	ActiveVGA->registers->ExternalRegisters.DATALATCH.latchplane[3] = readVRAMplane(ActiveVGA,3,offset,0); //Plane 3
	VGA_updateLatches(); //Update the latch data mirroring!
}

OPTINLINE byte VGA_ReadModeOperation(byte planes, uint_32 offset)
{
	byte curplane;
	byte val; //The value we return, default to 0 if undefined!
	loadlatch(offset); //Load the latches!

	switch (ActiveVGA->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.ReadMode) //What read mode?
	{
	case 0: //Read mode 0: Just read the normal way!
		for (curplane = 0; curplane < 4; curplane++)
		{
			if (planes&(1 << curplane)) //Read from this plane?
			{
				return readVRAMplane(ActiveVGA, curplane, offset, 0); //Read directly from vram using the selected plane!
			}
		}
		break;
	case 1: //Read mode 1: Compare display memory with color defined by the Color Compare field. Colors Don't care field are not considered.
		val = 0; //Reset data to not equal!
		//Each bit in the result represents one comparision between the reference color, with the bit being set if the comparision is true.
		for (curplane=0;curplane<4;curplane++) //Check all planes!
		{
			if (ActiveVGA->registers->GraphicsRegisters.REGISTERS.COLORDONTCAREREGISTER.ColorCare&(1<<curplane)) //We care about this plane?
			{
				if (readVRAMplane(ActiveVGA,curplane,offset,0)==ActiveVGA->registers->GraphicsRegisters.REGISTERS.COLORCOMPAREREGISTER.ColorCompare) //Equal?
				{
					val |= (1<<curplane); //Set the bit: the comparision is true!
				}
			}
		}
		break;
	default: //Shouldn't be here!
		break;
	}

	return val; //Give the result of the read mode operation!
}

/*

The r/w operations from the CPU!

*/

//decodeCPUaddress(Write from CPU=1; Read from CPU=0, offset (from VRAM start address), planes to read/write (4-bit mask), offset to read/write within the plane(s)).
OPTINLINE void decodeCPUaddress(byte towrite, uint_32 offset, byte *planes, uint_32 *realoffset)
{
	if (ActiveVGA->registers->SequencerRegisters.REGISTERS.SEQUENCERMEMORYMODEREGISTER.Chain4Enable) //Chain 4 mode?
	{
		*planes = (1 << (offset & 0x3)); //Lower bits, create bitmask!
		*realoffset = offset;
		*realoffset >>= 2; //Rest of the bits. Multiples of 4 wont get written!
		return; //Done!
	}
	//if (!ActiveVGA->registers->SequencerRegisters.REGISTERS.SEQUENCERMEMORYMODEREGISTER.EnableOE) //Odd/even mode disabled? (According to Dosbox, this value is 0!)
		//Sequential mode?
	if (!ActiveVGA->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER.EnableOddEvenMode) //Sequential mode?
	{
		if (towrite) //Writing access?
		{
			*planes = 0xF; //Write to all planes possible, map mask register does the rest!
		}
		else
		{
			*planes = 1; //Load plane 0!
			*planes <<= ActiveVGA->registers->GraphicsRegisters.REGISTERS.READMAPSELECTREGISTER.ReadMapSelect; //Take this plane!
		}
		*realoffset = offset; //Direct offset into VRAM!
		//The offset is used directly!
		return; //Done!
	}

	//Odd/even mode used (compatiblity case)?
	//Do the same as VPC!
	register byte calcplanes;
	calcplanes = offset;
	calcplanes &= 1; //Take 1 bit to determine the plane (0/1)!
	calcplanes = (1 << calcplanes); //The plane calculated (0/1)!
	calcplanes |= (calcplanes << 2); //Add the high plane for destination!
	offset &= 0xFFFE; //Take the offset within the plane!
	*planes = calcplanes; //Load the planes to address!
	*realoffset = offset; //Load the offset to address!
}

extern byte LOG_VRAM_WRITES; //Log VRAM writes?

byte planes; //What planes to affect!
uint_32 realoffset; //What offset to affect!

byte VGAmemIO_rb(uint_32 baseoffset, uint_32 reloffset, byte *value)
{
	if (is_A000VRAM(reloffset)) //VRAM and within range?
	{
		reloffset -= VGA_VRAM_START; //Calculate start offset into VRAM!

		decodeCPUaddress(0, reloffset, &planes, &realoffset); //Our VRAM offset starting from the 32-bit offset (A0000 etc.)!

		*value = VGA_ReadModeOperation(planes, realoffset); //Apply the operation on read mode!
		return 1; //Written!
	}
	return 0; //Not written!
}

byte VGAmemIO_wb(uint_32 baseoffset, uint_32 reloffset, byte value)
{
	if (is_A000VRAM(reloffset)) //VRAM and within range?
	{
		reloffset -= VGA_VRAM_START; //Calculate start offset into VRAM!

		decodeCPUaddress(1, reloffset, &planes, &realoffset); //Our VRAM offset starting from the 32-bit offset (A0000 etc.)!

		VGA_WriteModeOperation(planes, realoffset, value); //Apply the operation on write mode!
		return 1; //Written!
	}
	return 0; //Not written!
}

void VGAmemIO_reset()
{
	//Register/reset memory mapped I/O!
	MMU_resetHandlers("VGA");
	MMU_registerWriteHandler(0xA0000,0xBFFFF,&VGAmemIO_wb,"VGA");
	MMU_registerReadHandler(0xA0000,0xBFFFF,&VGAmemIO_rb,"VGA");
}