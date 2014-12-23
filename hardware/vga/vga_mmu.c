#include "headers/hardware/vga.h" //Basic defs!
#include "headers/support/log.h" //Loggin support!
#include "headers/hardware/vga_screen/vga_vram.h" //VRAM support!
#include "headers/mmu/mmuhandler.h" //Handling support!

extern VGA_Type *ActiveVGA; //Active VGA!
extern byte is_loadchartable; //Loading character table?
uint_32 VGA_VRAM_START = 0xA0000; //VRAM start address default!

OPTINLINE void VGA_updateLatches()
{
	//Update the latch the software can read.
	ActiveVGA->registers->CRTControllerRegisters.REGISTERS.GraphicsControllerDataLatches.LatchN = ActiveVGA->registers->ExternalRegisters.DATALATCH.latchplane[ActiveVGA->registers->GraphicsRegisters.REGISTERS.READMAPSELECTREGISTER.ReadMapSelect]; //Update the latch the software reads (R/O)
}

/*

VRAM base offset!

*/

static OPTINLINE byte is_A000VRAM(uint_32 linearoffset) //In VRAM (for CPU), offset=real memory address (linear memory)?
{
	if (ActiveVGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.RAM_Enable) //VRAM Access by CPU Enabled?
	{
		switch (ActiveVGA->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER.MemoryMapSelect) //What memory map?
		{
		case 0: //A0000-BFFFF (128K region)?
			VGA_VRAM_START = 0xA0000; //Start!
			return ((linearoffset>=0xA0000) && (linearoffset<0xC0000)); //In range?
			break;
		case 1: //A0000-AFFFF (64K region)?
			VGA_VRAM_START = 0xA0000; //Start!
			return ((linearoffset>=0xA0000) && (linearoffset<0xB0000)); //In range?
			break;
		case 2: //B0000-B7FFF (32K region)?
			VGA_VRAM_START = 0xB0000; //Start!
			return ((linearoffset>=0xB0000) && (linearoffset<0xB8000)); //In range?
			break;
		case 3: //B8000-BFFFF (32K region)?
			VGA_VRAM_START = 0xB8000; //Start!
			return ((linearoffset>=0xB8000) && (linearoffset<0xC0000)); //In range?
			break;
		}
	}
	return 0; //Don't read/write from VRAM!
}

static OPTINLINE uint_32 getVRAMOffset(uint_32 linearoffset)
{
	uint_32 result; //Don't read/write by default!
	result = linearoffset;
	result -= VGA_VRAM_START; //Calculate start offset!
	return result; //Don't read/write from VRAM!	
}

//And now the input/output functions for segment 0xA000 (starting at offset 0)

/*

Special operations for write!

*/

OPTINLINE static uint_32 LogicalOperation(uint_32 input)
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

OPTINLINE static uint_32 BitmaskOperation(uint_32 input, byte bitmaskregister)
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
static OPTINLINE void VGA_WriteModeOperation(byte planes, uint_32 offset, byte val)
{
	uint_32 data = val; //Default to the value given!

	byte curplane; //For plane loops!
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

	byte planeenable = planes; //What planes to try to write to!
	planeenable &= ActiveVGA->registers->SequencerRegisters.REGISTERS.MAPMASKREGISTER.MemoryPlaneWriteEnable; //The actual planes to write to!
	for (curplane=0;curplane<4;curplane++) //Process all planes!
	{
		if (planeenable&(1<<curplane)) //Modification of the plane?
		{
			writeVRAMplane(ActiveVGA,curplane,offset,data&0xFF); //Write the plane from the data!
		}
		data >>= 8; //Shift to the next plane!
	}
}

static OPTINLINE void loadlatch(uint_32 offset)
{
	ActiveVGA->registers->ExternalRegisters.DATALATCH.latchplane[0] = readVRAMplane(ActiveVGA,0,offset,0); //Plane 0
	ActiveVGA->registers->ExternalRegisters.DATALATCH.latchplane[1] = readVRAMplane(ActiveVGA,1,offset,0); //Plane 1
	ActiveVGA->registers->ExternalRegisters.DATALATCH.latchplane[2] = readVRAMplane(ActiveVGA,2,offset,0); //Plane 2
	ActiveVGA->registers->ExternalRegisters.DATALATCH.latchplane[3] = readVRAMplane(ActiveVGA,3,offset,0); //Plane 3
	VGA_updateLatches(); //Update the latch data mirroring!
}

static OPTINLINE byte VGA_ReadModeOperation(byte plane, uint_32 offset)
{
	loadlatch(offset); //Load the latches!

	byte curplane;
	byte val = 0; //The value we return, default to 0 if undefined!
	switch (ActiveVGA->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.ReadMode) //What read mode?
	{
	case 0: //Read mode 0: Just read the normal way!
		val = readVRAMplane(ActiveVGA,plane,offset,0); //Read directly from vram using selected plane!
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

extern byte LOG_VRAM_WRITES; //Log VRAM writes?
OPTINLINE void VRAM_writecpu(uint_32 offset, byte value)
{
	//Now convert the address to plane(s) and offsets!
	
	uint_32 originaloffset = getVRAMOffset(offset); //Our VRAM offset starting from the 32-bit offset (A0000 etc.)!
	uint_32 realoffset = originaloffset; //Default to original offset!

	byte plane = 0; //The determined plane we use, default: none to write!
	byte mode; //Safe of the mode!
	if (ActiveVGA->registers->SequencerRegisters.REGISTERS.SEQUENCERMEMORYMODEREGISTER.Chain4Enable) //Chain 4 mode?
	{
		mode = 2; //Mode 2!
		plane = (1<<(originaloffset&0x3)); //Lower bits, create bitmask!
		realoffset = (originaloffset>>2); //Rest of the bits. Multiples of 4 wont get written!
	}
	else if (ActiveVGA->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.OddEvenMode //Odd/even mode possible?
		&& ActiveVGA->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER.EnableOddEvenMode
		&& ActiveVGA->registers->SequencerRegisters.REGISTERS.SEQUENCERMEMORYMODEREGISTER.EnableOE //Odd/even mode enabled?
		) //Odd/even mode?
	{
		mode = 1; //Mode 1!
		plane = 1;
		byte plane2;
		plane2 = ActiveVGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.OE_HighPage; //Load high page!
		plane2 <<= 1;
		plane2 |= (originaloffset&1); //Sub!
		plane <<= plane2; //The plane to use: odd(1) or even(0)!
		realoffset = originaloffset;
		realoffset &= ~1;
		realoffset >>= 1; //Calculate the correct offset within the VRAM!
	}
	else //Sequential mode?
	{
		mode = 0; //Mode 0!
		plane = 0xF; //Write to all planes possible, map mask register does the rest!
		//The offset is used directly!
	}

	VGA_WriteModeOperation(plane,realoffset,value); //Apply the operation on write mode!
}

OPTINLINE byte VRAM_readcpu(uint_32 offset)
{
	uint_32 originaloffset = getVRAMOffset(offset); //Our VRAM offset starting from the 32-bit offset (A0000 etc.)!
	uint_32 realoffset = originaloffset; //Default to original offset!

	byte plane = 0; //The determined plane we use, default: none to write!
	if (ActiveVGA->registers->SequencerRegisters.REGISTERS.SEQUENCERMEMORYMODEREGISTER.Chain4Enable) //Chain 4 mode?
	{
		plane = (originaloffset&0x3); //Lower bits, create bitmask!
		realoffset = (originaloffset>>2); //Rest of the bits. Multiples of 4 wont get written!
	}
	else if (ActiveVGA->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.OddEvenMode //Odd/even mode possible?
		&& ActiveVGA->registers->SequencerRegisters.REGISTERS.SEQUENCERMEMORYMODEREGISTER.EnableOE //Odd/even mode enabled?
		) //Odd/even mode?
	{
		plane = ActiveVGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.OE_HighPage; //Load high page!
		plane <<= 1;
		plane |= (originaloffset&1); //Sub!
		realoffset = originaloffset;
		realoffset &= ~1;
		realoffset >>= 1; //Calculate the correct offset within the VRAM!
	}
	else //Sequential mode?
	{
		plane = ActiveVGA->registers->GraphicsRegisters.REGISTERS.READMAPSELECTREGISTER.ReadMapSelect; //Use the Read Map Select register!
		//The offset is used directly!
	}

	return VGA_ReadModeOperation(plane,realoffset); //Apply the operation on read mode!
}

byte VGAmemIO_rb(uint_32 baseoffset, uint_32 reloffset, byte *value)
{
	if (is_A000VRAM(baseoffset+reloffset)) //VRAM and within range?
	{
		*value = VRAM_readcpu(baseoffset+reloffset); //Memory to VRAM!
		return 1; //Written!
	}
	return 0; //Not written!
}

byte VGAmemIO_wb(uint_32 baseoffset, uint_32 reloffset, byte value)
{
	if (is_A000VRAM(baseoffset+reloffset)) //VRAM and within range?
	{
		VRAM_writecpu(baseoffset+reloffset,value); //Memory to VRAM!
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