/*

Copyright (C) 2019 - 2020  Superfury

This file is part of UniPCemu.

UniPCemu is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

UniPCemu is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with UniPCemu.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "headers/hardware/vga/vga.h" //Basic defs!
#include "headers/support/log.h" //Loggin support!
#include "headers/hardware/vga/vga_vram.h" //VRAM support!
#include "headers/mmu/mmuhandler.h" //Handling support!
#include "headers/hardware/vga/vga_cga_mda.h" //CGA/MDA support!
#include "headers/cpu/cpu.h" //Emulator cpu support for waitstates!

//#define ENABLE_SPECIALDEBUGGER

uint_32 VGA_VRAM_START = 0xA0000; //VRAM start address default!
uint_32 VGA_VRAM_END = 0xC0000; //VRAM end address default!

byte VGA_RAMEnable = 1; //Is our RAM enabled?
byte VGA_MemoryMapSelect = 0; //What memory map is active?

uint_32 VGA_MemoryMapBankRead = 0, VGA_MemoryMapBankWrite = 0; //The memory map bank to use!

OPTINLINE void VGA_updateLatches()
{
	//Update the latch the software can read.
	getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.GraphicsControllerDataLatches = getActiveVGA()->registers->ExternalRegisters.DATALATCH.latchplane[GETBITS(getActiveVGA()->registers->GraphicsRegisters.REGISTERS.READMAPSELECTREGISTER,0,3)]; //Update the latch the software reads (R/O)
}

void VGA_updateVRAMmaps(VGA_Type *VGA)
{
	VGA_RAMEnable = GETBITS(VGA->registers->ExternalRegisters.MISCOUTPUTREGISTER,1,1); //RAM enabled?
	VGA_MemoryMapSelect = GETBITS(VGA->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER,2,3); //Update the selected memory map!
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
	default:
		break;
	}
}

/*

VRAM base offset!

*/

OPTINLINE byte is_A000VRAM(uint_32 linearoffset) //In VRAM (for CPU), offset=real memory address (linear memory)?
{
	INLINEREGISTER uint_32 addr=linearoffset; //The offset to check!
	return VGA_RAMEnable && (addr>=VGA_VRAM_START) && (addr<VGA_VRAM_END); //Used when VRAM is enabled and VRAM is addressed!
}

//And now the input/output functions for segment 0xA000 (starting at offset 0)

/*

Special operations for write!

*/

OPTINLINE uint_32 LogicalOperation(uint_32 input)
{
	switch (GETBITS(getActiveVGA()->registers->GraphicsRegisters.REGISTERS.DATAROTATEREGISTER,3,3))
	{
	case 0x00:	/* None */
		return input; //Unmodified
	case 0x01:	/* AND */
		return input & getActiveVGA()->registers->ExternalRegisters.DATALATCH.latch;
	case 0x02:	/* OR */
		return input | getActiveVGA()->registers->ExternalRegisters.DATALATCH.latch;
	case 0x03:	/* XOR */
		return input ^ getActiveVGA()->registers->ExternalRegisters.DATALATCH.latch;
	default:
		break;
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
	data = (byte)ror((byte)data, GETBITS(getActiveVGA()->registers->GraphicsRegisters.REGISTERS.DATAROTATEREGISTER,0,7)); //Rotate it! Keep 8-bit data!
	data = getActiveVGA()->ExpandTable[data]; //Make sure the data is on the all planes!

	curplane = 1; //Process all 4 plane bits!
	do
	{
		if (GETBITS(getActiveVGA()->registers->GraphicsRegisters.REGISTERS.ENABLESETRESETREGISTER,0,0xF)&curplane) //Enable set/reset? (Mode 3 ignores this flag)
		{
			data = (data&(~getActiveVGA()->FillTable[curplane])) | getActiveVGA()->FillTable[GETBITS(getActiveVGA()->registers->GraphicsRegisters.REGISTERS.SETRESETREGISTER,0,0xF)&curplane]; //Turn all those bits off, and the set/reset plane ON=0xFF for the plane and OFF=0x00!
		}
		curplane <<= 1; //Next plane!
	} while (curplane!=0x10); //Only the 4 planes are used!
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
	data = getActiveVGA()->FillTable[data&0xF]; //Replicate across all 4 planes to 8 bits set or cleared of their respective planes. The upper 4 bits of the CPU input are unused.
	data = LogicalOperation(data); //Execute the logical operation!
	data = BitmaskOperation(data, getActiveVGA()->registers->GraphicsRegisters.REGISTERS.BITMASKREGISTER); //Execute the bitmask operation fully!
	return data;
}

uint_32 VGA_WriteMode3(uint_32 data) //Ignore enable set reset register!
{
	data = ror(data, GETBITS(getActiveVGA()->registers->GraphicsRegisters.REGISTERS.DATAROTATEREGISTER,0,7)); //Rotate it! Keep 8-bit data!
	data &= getActiveVGA()->registers->GraphicsRegisters.REGISTERS.BITMASKREGISTER; //AND with the Bit Mask field.
	data = BitmaskOperation(getActiveVGA()->FillTable[GETBITS(getActiveVGA()->registers->GraphicsRegisters.REGISTERS.SETRESETREGISTER,0,0xF)], data); //Use the generated data on the Set/Reset register
	return data;
}

uint_32 readbank = 0, writebank = 0; //Banked VRAM support!

OPTINLINE void VGA_WriteModeOperation(byte planes, uint_32 offset, byte val)
{
	static const VGA_WriteMode VGA_WRITE[4] = {VGA_WriteMode0,VGA_WriteMode1,VGA_WriteMode2,VGA_WriteMode3}; //All write modes!
	INLINEREGISTER byte curplane; //For plane loops!
	INLINEREGISTER uint_32 data; //Default to the value given!
	data = VGA_WRITE[GETBITS(getActiveVGA()->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER,0,3)]((uint_32)val); //What write mode?

	byte planeenable = GETBITS(getActiveVGA()->registers->SequencerRegisters.REGISTERS.MAPMASKREGISTER,0,0xF); //What planes to try to write to!
	if (((getActiveVGA()->precalcs.linearmode & 5) == 5) || (getActiveVGA()->precalcs.linearmode&8)) planeenable = 0xF; //Linear memory ignores this? Or are we to ignore the Write Plane Mask(linear byte mode)?
	planeenable &= planes; //The actual planes to write to!
	byte curplanemask=1;
	curplane = 0;
	do //Process all planes!
	{
		if (planeenable&curplanemask) //Modification of the plane?
		{
			writeVRAMplane(getActiveVGA(),curplane,offset,writebank,data&0xFF); //Write the plane from the data!
		}
		data >>= 8; //Shift to the next plane!
		curplanemask <<= 1; //Next plane!
	} while (++curplane!=4);
}

OPTINLINE void loadlatch(uint_32 offset)
{
	getActiveVGA()->registers->ExternalRegisters.DATALATCH.latch = VGA_VRAMDIRECTPLANAR(getActiveVGA(),offset,readbank);
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
			return readVRAMplane(getActiveVGA(), curplane, offset,readbank); //Read directly from vram using the selected plane!
		}
		planes >>= 1; //Next plane!
	} while (++curplane!=4);
	return 0; //Unknown plane! Give 0!
}

byte VGA_ReadMode1(byte planes, uint_32 offset) //Read mode 1: Compare display memory with color defined by the Color Compare field. Colors Don't care field are not considered.
{
	byte dontcare;
	uint_32 result;
	dontcare = GETBITS(getActiveVGA()->registers->GraphicsRegisters.REGISTERS.COLORDONTCAREREGISTER,0,0xF); //Don't care bits!
	result = (getActiveVGA()->registers->ExternalRegisters.DATALATCH.latch&getActiveVGA()->FillTable[dontcare])^(getActiveVGA()->FillTable[GETBITS(getActiveVGA()->registers->GraphicsRegisters.REGISTERS.COLORCOMPAREREGISTER,0,0xF)^dontcare]);
	return (byte)(~(result|(result>>8)|(result>>16)|(result>>24))); //Give the value!
}

OPTINLINE byte VGA_ReadModeOperation(byte planes, uint_32 offset)
{
	static const VGA_ReadMode READ[2] = {VGA_ReadMode0,VGA_ReadMode1}; //Read modes!
	loadlatch(offset); //Load the latches!

	return READ[GETBITS(getActiveVGA()->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER,3,1)](planes,offset); //What read mode?
}

/*

The r/w operations from the CPU!

*/

extern byte specialdebugger; //Debugging special toggle?

char towritetext[2][256] = {"Reading","Writing"};

byte verboseVGA; //Verbose VGA dumping?

byte VGA_WriteMemoryMode=0, VGA_ReadMemoryMode=0;

void VGA_Chain4_decode(byte towrite, uint_32 offset, byte *planes, uint_32 *realoffset)
{
	INLINEREGISTER uint_32 realoffsettmp;
	INLINEREGISTER byte calcplanes;
	calcplanes = realoffsettmp = offset; //Original offset to start with!
	calcplanes &= 0x3; //Lower 2 bits determine the plane!
	*planes = (1 << calcplanes); //Give the planes to write to!
	if ((getActiveVGA()->enable_SVGA>=1) && (getActiveVGA()->enable_SVGA<=2)) //ET3000/ET4000?
	{
		realoffsettmp >>= 2; //Make sure we're linear in memory when requested! ET3000/ET4000 is different in this point! This always writes to a quarter of VRAM(since it's linear in VRAM, combined with the plane), according to the FreeVGA documentation!
	}
	else
	{
		realoffsettmp &= ~3; //Multiples of 4 won't get written on true VGA!
	}
	//We're the LG1 case of the Table 4.3.4 of the ET4000 manual!
	*realoffset = realoffsettmp; //Give the offset!
	#ifdef ENABLE_SPECIALDEBUGGER
		if (specialdebugger||verboseVGA) //Debugging special?
	#else
		if (verboseVGA) //Debugging special?
	#endif
		{
			dolog("VGA", "%s using Chain 4: Memory aperture offset %08X=Planes: %04X, Offset: %08X, VRAM offset: %08X, Bank: %08X", towritetext[towrite ? 1 : 0], offset, *planes, *realoffset, (*realoffset<<2), towrite?writebank:readbank);
		}
}

void VGA_OddEven_decode(byte towrite, uint_32 offset, byte *planes, uint_32 *realoffset)
{
	INLINEREGISTER uint_32 realoffsettmp;
	INLINEREGISTER byte calcplanes;
	calcplanes = realoffsettmp = offset; //Take the default offset!
	calcplanes &= 1; //Take 1 bit to determine the odd/even plane (odd/even)!
	if (GETBITS(getActiveVGA()->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER,1,1)) //Replace A0 with high order bit?
	{
		realoffsettmp &= ~1; //Clear bit 0 for our result!
		realoffsettmp |= (offset>>16)&1; //Replace bit 0 with the high order bit(A16), the most-significant bit!
	}
	if (GETBITS(getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER,5,1)==0) //High page on High RAM?
	{
		calcplanes |= 2; //Apply high page!
	}
	writebank <<= 1; //Shift to it's position!
	writebank &= 0xE0000; //3 bits only!
	readbank <<= 1; //Shift to it's postion!
	readbank &= 0xE0000; //3 bits only!
	*realoffset = realoffsettmp; //Give the calculated offset!
	*planes = (0x5 << calcplanes); //Convert to used plane (0&2 or 1&3)!
	#ifdef ENABLE_SPECIALDEBUGGER
		if (specialdebugger||verboseVGA) //Debugging special?
	#else
		if (verboseVGA) //Debugging special?
	#endif
		{
			dolog("VGA", "%s using Odd/Even: Memory aperture offset %08X=Planes: %04X, Offset: %08X, VRAM offset: %08X, Bank: %08X", towritetext[towrite ? 1 : 0], offset, *planes, *realoffset, (*realoffset<<2), towrite?writebank:readbank);
		}
}

void VGA_Planar_decode(byte towrite, uint_32 offset, byte *planes, uint_32 *realoffset)
{
	INLINEREGISTER byte calcplanes;
	if (towrite) //Writing access?
	{
		calcplanes = 0xF; //Write to all planes possible, map mask register does the rest!
	}
	else if ((getActiveVGA()->precalcs.linearmode & 5) == 5) //Linear memory?
	{
		calcplanes = 0xF; //Read map select is ignored!
	}
	else //Normal VGA read!
	{
		calcplanes = 1; //Load plane 0!
		calcplanes <<= GETBITS(getActiveVGA()->registers->GraphicsRegisters.REGISTERS.READMAPSELECTREGISTER,0,3); //Take this plane!
	}
	//Apply new bank base for this mode!
	writebank <<= 2; //Shift to it's position!
	writebank &= 0xC0000; //2 bits only!
	readbank <<= 2; //Shift to it's postion!
	readbank &= 0xC0000; //2 bits only!
	*planes = calcplanes; //The planes to apply!
	*realoffset = offset; //Load the offset directly!
	//Use planar mode!
	#ifdef ENABLE_SPECIALDEBUGGER
		if (specialdebugger||verboseVGA) //Debugging special?
	#else
		if (verboseVGA) //Debugging special?
	#endif
		{
			dolog("VGA", "%s using Planar access: Memory aperture offset %08X=Planes: %04X, Offset: %08X, VRAM offset: %08X, Bank: %08X", towritetext[towrite ? 1 : 0], offset, *planes, *realoffset, (*realoffset<<2), towrite?writebank:readbank);
		}
}

void SVGA_LinearContinuous_decode(byte towrite, uint_32 offset, byte *planes, uint_32 *realoffset)
{
	INLINEREGISTER uint_32 realoffsettmp;
	INLINEREGISTER byte calcplanes;
	calcplanes = realoffsettmp = offset; //Original offset to start with!
	calcplanes &= 0x3; //Lower 2 bits determine the plane(ascending VRAM memory blocks of 4 bytes)!
	*planes = (1 << calcplanes); //Give the planes to write to!
	realoffsettmp >>= 2; //Rest of bits determine the direct index!
	*realoffset = realoffsettmp; //Give the offset!
}

typedef void (*decodeCPUaddressMode)(byte towrite, uint_32 offset, byte *planes, uint_32 *realoffset); //Decode addressing mode typedef!

decodeCPUaddressMode decodeCPUAddressW = VGA_OddEven_decode, decodeCPUAddressR=VGA_OddEven_decode; //Our current MMU decoder for reads and writes!

//decodeCPUaddress(Write from CPU=1; Read from CPU=0, offset (from VRAM start address), planes to read/write (4-bit mask), offset to read/write within the plane(s)).
OPTINLINE void decodeCPUaddress(byte towrite, uint_32 offset, byte *planes, uint_32 *realoffset)
{
	//Apply bank when used!
	if ((getActiveVGA()->precalcs.linearmode&4)==4) //Enable SVGA Normal segmented read/write bank mode support?
	{
		if (getActiveVGA()->precalcs.linearmode & 2) //Use high 4 bits as address!
		{
			readbank = writebank = (offset&0xF0000); //Apply read/write bank from the high 4 bits that's unused!
		}
		else //Use bank select?
		{
			readbank = VGA_MemoryMapBankRead; //Read bank
			writebank = VGA_MemoryMapBankWrite; //Write bank
		}
		//Apply the segmented VGA mode like any normal VGA!
	}
	else readbank = writebank = 0; //No memory banks are used!

	//Calculate according to the mode in our table and write/read memory mode!
	if (towrite) //Writing?
	{
		decodeCPUAddressW(towrite,offset,planes,realoffset); //Apply the write memory mode!
	}
	else //Reading?
	{
		decodeCPUAddressR(towrite,offset,planes,realoffset); //Apply the read memory mode!
	}
}

void updateVGAMMUAddressMode()
{
	static const decodeCPUaddressMode decodeCPUaddressmode[4] = {VGA_Planar_decode,VGA_Chain4_decode,VGA_OddEven_decode,SVGA_LinearContinuous_decode}; //All decode modes supported!
	decodeCPUAddressW = decodeCPUaddressmode[VGA_WriteMemoryMode&3]; //Apply the Write memory mode!
	decodeCPUAddressR = decodeCPUaddressmode[VGA_ReadMemoryMode&3]; //Apply the Read memory mode!
}

byte planes; //What planes to affect!
uint_32 realoffset; //What offset to affect!

extern byte useIPSclock; //Are we using the IPS clock instead of cycle accurate clock?

void applyCGAMDAOffset(byte CPUtiming, uint_32 *offset)
{
	if (CGAEMULATION_ENABLED(getActiveVGA())) //CGA?
	{
		*offset &= 0x3FFF; //Wrap around 16KB!

		//Apply wait states(except when using the IPS clock)!
		if ((CPU[activeCPU].running==1) && (useIPSclock==0) && CPUtiming) //Are we running? Introduce wait states! Don't allow wait states when using the IPS clock: it will crash because the instruction is never finished, thus never allowing the video adapter emulation to finish the wait state!
		{
			getActiveVGA()->WaitState = 1; //Start our waitstate for CGA memory access!
			getActiveVGA()->WaitStateCounter = 8; //Reset our counter for the 8 hdots to wait!
			CPU[activeCPU].halt |= 4; //We're starting to wait for the CGA!
			updateVGAWaitState(); //Update the current waitstate!
		}
	}
	else if (MDAEMULATION_ENABLED(getActiveVGA())) //MDA?
	{
		*offset &= 0xFFF; //Wrap around 4KB!
	}
}

extern uint_32 memory_dataread;
extern byte memory_datasize; //The size of the data that has been read!
byte VGAmemIO_rb(uint_32 offset)
{
	if (unlikely(is_A000VRAM(offset))) //VRAM and within range?
	{
		offset -= VGA_VRAM_START; //Calculate start offset into VRAM!
		applyCGAMDAOffset(1,&offset); //Apply CGA/MDA offset if needed!
		decodeCPUaddress(0, offset, &planes, &realoffset); //Our VRAM offset starting from the 32-bit offset (A0000 etc.)!
		memory_dataread = VGA_ReadModeOperation(planes, realoffset); //Apply the operation on read mode!
		if (CGAEMULATION_ENABLED(getActiveVGA())||MDAEMULATION_ENABLED(getActiveVGA())) //Unchanged mapping?
		{
			memory_dataread = getActiveVGA()->CGAMDAShadowRAM[offset]; //Read from shadow RAM!
		}
		memory_datasize = 1; //Only 1 byte chunks can be read!
		return 1; //Read!
	}
	return 0; //Not read!
}

void CGAMDA_doWriteRAMrefresh(uint_32 offset)
{
	applyCGAMDAOffset(0,&offset); //Apply CGA/MDA offset if needed!
	decodeCPUaddress(1, offset, &planes, &realoffset); //Our VRAM offset starting from the 32-bit offset (A0000 etc.)!
	VGA_WriteModeOperation(planes, realoffset, getActiveVGA()->CGAMDAShadowRAM[offset]); //Apply the operation on write mode!
}

extern byte memory_datawrittensize; //How many bytes have been written to memory during a write!
byte VGAmemIO_wb(uint_32 offset, byte value)
{
	if (unlikely(is_A000VRAM(offset))) //VRAM and within range?
	{
		offset -= VGA_VRAM_START; //Calculate start offset into VRAM!
		applyCGAMDAOffset(1,&offset); //Apply CGA/MDA offset if needed!
		decodeCPUaddress(1, offset, &planes, &realoffset); //Our VRAM offset starting from the 32-bit offset (A0000 etc.)!
		VGA_WriteModeOperation(planes, realoffset, value); //Apply the operation on write mode!
		if (CGAEMULATION_ENABLED(getActiveVGA())||MDAEMULATION_ENABLED(getActiveVGA())) //Unchanged mapping?
		{
			getActiveVGA()->CGAMDAShadowRAM[offset] = value; //Write to shadow RAM!
		}
		memory_datawrittensize = 1; //Only 1 byte written!
		return 1; //Written!
	}
	return 0; //Not written!
}

void VGAmemIO_reset()
{
	//Register/reset memory mapped I/O!
	/*
	MMU_resetHandlers("VGA");
	MMU_registerWriteHandler(&VGAmemIO_wb,"VGA");
	MMU_registerReadHandler(&VGAmemIO_rb,"VGA");
	*/
	//Done directly by the MMU, since we're always present!
}
