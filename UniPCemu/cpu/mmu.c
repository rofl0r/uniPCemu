#include "headers/cpu/mmu.h" //Ourselves!
#include "headers/cpu/cpu.h"
#include "headers/cpu/memory_adressing.h" //Memory assist functions!
#include "headers/cpu/paging.h" //Paging functions!
#include "headers/support/log.h" //Logging support!
#include "headers/cpu/protection.h" //Protection support!
#include "headers/cpu/paging.h" //Protection support!
#include "headers/emu/debugger/debugger.h" //Debugger support for logging MMU accesses!
#include "headers/hardware/xtexpansionunit.h" //XT expansion unit support!
#include "headers/mmu/mmu_internals.h" //Internal MMU call support!
#include "headers/mmu/mmuhandler.h" //MMU direct handler support!

extern MMU_type MMU; //MMU itself!

//Are we disabled?
#define __HW_DISABLED 0

#define CPU286_WAITSTATE_DELAY 1

byte writeword = 0; //Hi-end word written?

//Pointer support (real mode only)!
void *MMU_directptr(uint_32 address, uint_32 size) //Full ptr to real MMU memory!
{
	if ((address <= MMU.size) && ((address + size) <= MMU.size)) //Within our limits of flat memory and not paged?
	{
		return &MMU.memory[address]; //Give the memory's start!
	}

	//Don't signal invalid memory address: we're internal emulator call!
	return NULL; //Not found!	
}

//MMU_ptr: 80(1)86 only!
void *MMU_ptr(sword segdesc, word segment, uint_32 offset, byte forreading, uint_32 size) //Gives direct memory pointer!
{
	uint_32 realaddr;
	if (MMU.memory == NULL) //None?
	{
		return NULL; //NULL: no memory alligned!
	}

	if (EMULATED_CPU <= CPU_NECV30) //-NEC V20/V30 wraps offset arround?
	{
		offset &= 0xFFFF; //Wrap arround!
	}
	else
	{
		return NULL; //80286+ isn't supported here!
	}
	realaddr = (segment << 4) + offset; //Our real address!
	realaddr = BITOFF(realaddr, 0x100000); //Wrap arround, disable A20!	
	return MMU_directptr(realaddr, size); //Direct pointer!
}

extern byte is_XT; //Are we an XT?

//Address translation routine.
OPTINLINE uint_32 MMU_realaddr(sword segdesc, word segment, uint_32 offset, byte wordop) //Real adress?
{
	INLINEREGISTER uint_32 realaddress;
	//word originalsegment = segment;
	//uint_32 originaloffset = offset; //Save!
	realaddress = offset; //Load the address!
	if ((EMULATED_CPU==CPU_8086) || (EMULATED_CPU==CPU_NECV30 && !((realaddress==0x10000) && wordop))) //-NEC V20/V30 wraps offset arround 64kB? NEC V20/V30 allows 1 byte more in word operations!
	{
		realaddress &= 0xFFFF; //Wrap arround!
	}
	writeword = 0; //Reset word-write flag for checking next bytes!

	realaddress += CPU_MMU_start(segdesc, segment);

	realaddress &= MMU.wraparround; //Apply A20!

	if (is_XT) realaddress &= 0xFFFFF; //Only 20-bits address is available on a XT!
	else if (EMULATED_CPU==CPU_80286) realaddress &= 0xFFFFFF; //Only 24-bits is available on a AT!

	//We work!
	//dolog("MMU","\nAddress translation: %04X:%08X=%08X",originalsegment,originaloffset,realaddress); //Log the converted address!
	latchBUS(realaddress); //This address is to be latched!
	return realaddress; //Give real adress!
}

//OPcodes for the debugger!
byte OPbuffer[256]; //A large opcode buffer!
word OPlength = 0; //The length of the opcode buffer!

extern byte cpudebugger; //To debug the CPU?

void MMU_addOP(byte data)
{
	if (OPlength < sizeof(OPbuffer)) //Not finished yet?
	{
		OPbuffer[OPlength++] = data; //Save a part of the opcode!
	}
}

void MMU_clearOP()
{
	OPlength = 0; //Clear the buffer!
}

//CPU/EMU simple memory access routine.
extern uint_32 wordaddress; //Word address used during memory access!

//isread = 1|(opcode<<1) for reads! 0 for writes!
byte checkMMUaccess(sword segdesc, word segment, uint_32 offset, byte readflags, byte CPL) //Check if a byte address is invalid to read/write for a purpose! Used in all CPU modes!
{
	INLINEREGISTER uint_32 realaddress;
	if (EMULATED_CPU<=CPU_NECV30) return 0; //No checks are done in the old processors!

	if (CPU_MMU_checklimit(segdesc,segment,offset,readflags)) //Disallowed?
	{
		MMU.invaddr = 2; //Invalid address signaling!
		return 1; //Not found.
	}

	//Check for paging next!
	realaddress = MMU_realaddr(segdesc, segment, offset, 0); //Real adress!

	//We need to block on Page Faults as well! This is still unimplemented!
	if (is_paging()) //Are we paging?
	{
		if (CPU_Paging_checkPage(realaddress,readflags,CPL)) //Map it using the paging mechanism! Errored out?
		{
			return 1; //Error out!
		}
	}

	//We're valid?
	return 0; //We're a valid access for both MMU and Paging! Allow this instruction to execute!
}

OPTINLINE byte MMU_INTERNAL_rb(sword segdesc, word segment, uint_32 offset, byte opcode, byte index) //Get adress, opcode=1 when opcode reading, else 0!
{
	INLINEREGISTER byte result; //The result!
	INLINEREGISTER uint_32 realaddress;
	byte writewordbackup = writeword; //Save the old value first!
	if (MMU.memory==NULL) //No mem?
	{
		//dolog("MMU","R:No memory present!");
		MMU.invaddr = 1; //Invalid adress!
		return 0xFF; //Out of bounds!
	}

	realaddress = MMU_realaddr(segdesc, segment, offset, writeword); //Real adress!

	if (is_paging()) //Are we paging?
	{
		realaddress = mappage(realaddress); //Map it using the paging mechanism!
	}

	if (segdesc!=-1) //Normal memory access by the CPU itself?
	{
		if (writewordbackup==0) //First data of the word access?
		{
			wordaddress = realaddress; //Word address used during memory access!
			if (EMULATED_CPU==CPU_80286) //Process normal memory cycles!
			{
				CPU[activeCPU].cycles_MMUR += 2; //Add memory cycles used!
				CPU[activeCPU].cycles_MMUR += CPU286_WAITSTATE_DELAY; //One waitstate RAM!
			}
		}
		else //Second data of a word access?
		{
			if ((realaddress&~1)!=(wordaddress&~1)) //Unaligned word access? We're the second byte on a different word boundary!
			{
				if (EMULATED_CPU==CPU_80286) //Process additional cycles!
				{
					CPU[activeCPU].cycles_MMUR += 2; //Add memory cycles used!				
					CPU[activeCPU].cycles_MMUR += CPU286_WAITSTATE_DELAY; //One waitstate RAM!
				}
			}
		}
	}

	result = MMU_INTERNAL_directrb_realaddr(realaddress,opcode,index); //Read from MMU/hardware!

	return result; //Give the result!
}

OPTINLINE word MMU_INTERNAL_rw(sword segdesc, word segment, uint_32 offset, byte opcode, byte index) //Get adress!
{
	INLINEREGISTER word result;
	result = MMU_INTERNAL_rb(segdesc, segment, offset, opcode,index);
	result |= (MMU_INTERNAL_rb(segdesc, segment, offset + 1, opcode,index|1) << 8); //Get adress word!
	return result; //Give the result!
}

OPTINLINE uint_32 MMU_INTERNAL_rdw(sword segdesc, word segment, uint_32 offset, byte opcode, byte index) //Get adress!
{
	INLINEREGISTER uint_32 result;
	result = MMU_INTERNAL_rw(segdesc, segment, offset, opcode,index);
	result |= (MMU_INTERNAL_rw(segdesc, segment, offset + 2, opcode,index|2) << 16); //Get adress dword!
	return result; //Give the result!
}

extern byte EMU_RUNNING; //Emulator is running?

OPTINLINE void MMU_INTERNAL_wb(sword segdesc, word segment, uint_32 offset, byte val, byte index) //Set adress!
{
	INLINEREGISTER uint_32 realaddress;
	byte writewordbackup = writeword; //Save the old value first!
	if (MMU.invaddr) return; //Abort!
	if (CPU[activeCPU].faultraised && EMU_RUNNING) //Fault has been raised while emulator is running?
	{
		return; //Disable writes to memory when a fault has been raised!
	}
	if ((MMU.memory==NULL) || !MMU.size) //No mem?
	{
		//dolog("MMU","W:No memory present!");
		MMU.invaddr = 1; //Invalid address signaling!
		return; //Out of bounds!
	}
	
	/*if (LOG_MMU_WRITES) //Log MMU writes?
	{
		dolog("debugger","MMU: Write to %04X:%08X=%02X",segment,offset,val); //Log our written value!
	}*/

	realaddress = MMU_realaddr(segdesc, segment, offset, writeword); //Real adress!

	if (is_paging()) //Are we paging?
	{
		realaddress = mappage(realaddress); //Map it using the paging mechanism!
	}

	if (segdesc!=-1) //Normal memory access?
	{
		if (writewordbackup==0) //First data of the word access?
		{
			wordaddress = realaddress; //Word address used during memory access!
			if (EMULATED_CPU==CPU_80286) //Process normal memory cycles!
			{
				CPU[activeCPU].cycles_MMUW += 2; //Add memory cycles used!
				CPU[activeCPU].cycles_MMUW += CPU286_WAITSTATE_DELAY; //One waitstate RAM!
			}
		}
		else //Second data of a word access?
		{
			if ((realaddress&~1)!=(wordaddress&~1)) //Unaligned word access? We're the second byte on a different word boundary!
			{
				if (EMULATED_CPU==CPU_80286) //Process additional cycles!
				{
					CPU[activeCPU].cycles_MMUW += 2; //Add memory cycles used!				
					CPU[activeCPU].cycles_MMUW += CPU286_WAITSTATE_DELAY; //One waitstate RAM!
				}
			}
		}
	}

	MMU_INTERNAL_directwb_realaddr(realaddress,val,index); //Set data!
}

OPTINLINE void MMU_INTERNAL_ww(sword segdesc, word segment, uint_32 offset, word val, byte index) //Set adress (word)!
{
	INLINEREGISTER word w;
	w = val;
	MMU_INTERNAL_wb(segdesc,segment,offset,w&0xFF,index); //Low first!
	writeword = 1; //We're writing a 2nd byte word, for emulating the NEC V20/V30 0x10000 overflow bug.
	w >>= 8; //Shift low!
	MMU_INTERNAL_wb(segdesc,segment,offset+1,(byte)w,index|1); //High last!
}

OPTINLINE void MMU_INTERNAL_wdw(sword segdesc, word segment, uint_32 offset, uint_32 val, byte index) //Set adress (dword)!
{
	INLINEREGISTER word d;
	d = val;
	MMU_INTERNAL_ww(segdesc,segment,offset,d&0xFFFF,index); //Low first!
	d >>= 16; //Shift low!
	MMU_INTERNAL_ww(segdesc,segment,offset+2,d,index|2); //High last!
}

//Routines used by CPU!
byte MMU_directrb_realaddr(uint_32 realaddress, byte opcode) //Read without segment/offset translation&protection (from system/interrupt)!
{
	return MMU_INTERNAL_directrb_realaddr(realaddress,opcode,0);
}
void MMU_directwb_realaddr(uint_32 realaddress, byte val) //Read without segment/offset translation&protection (from system/interrupt)!
{
	MMU_INTERNAL_directwb_realaddr(realaddress,val,0);
}

extern byte CPU_databussize; //0=16/32-bit bus! 1=8-bit bus when possible (8088/80188)!
void MMU_wb(sword segdesc, word segment, uint_32 offset, byte val) //Set adress!
{
	if (segdesc!=-1) CPU[activeCPU].cycles_MMUW += (EMULATED_CPU==CPU_80286)?0:4; //CPU writes are counted!
	MMU_INTERNAL_wb(segdesc,segment,offset,val,0);
}
void MMU_ww(sword segdesc, word segment, uint_32 offset, word val) //Set adress!
{
	if (segdesc!=-1) CPU[activeCPU].cycles_MMUW += (EMULATED_CPU==CPU_80286)?0:(CPU_databussize?8:4); //CPU writes are counted!
	MMU_INTERNAL_ww(segdesc,segment,offset,val,0);
}
void MMU_wdw(sword segdesc, word segment, uint_32 offset, uint_32 val) //Set adress!
{
	MMU_INTERNAL_wdw(segdesc,segment,offset,val,0);
}
byte MMU_rb(sword segdesc, word segment, uint_32 offset, byte opcode) //Get adress, opcode=1 when opcode reading, else 0!
{
	if (segdesc!=-1) CPU[activeCPU].cycles_MMUR += EMULATED_CPU==CPU_80286?0:4; //CPU writes are counted!
	return MMU_INTERNAL_rb(segdesc,segment,offset,opcode,0);
}
word MMU_rw(sword segdesc, word segment, uint_32 offset, byte opcode) //Get adress, opcode=1 when opcode reading, else 0!
{
	if (segdesc!=-1) CPU[activeCPU].cycles_MMUR += (EMULATED_CPU==CPU_80286)?0:(CPU_databussize?8:4); //CPU writes are counted!
	return MMU_INTERNAL_rw(segdesc,segment,offset,opcode,0);
}
uint_32 MMU_rdw(sword segdesc, word segment, uint_32 offset, byte opcode) //Get adress, opcode=1 when opcode reading, else 0!
{
	return MMU_INTERNAL_rdw(segdesc,segment,offset,opcode,0);
}

//Extra routines for the emulator.

//A20 bit enable/disable (80286+).
void MMU_setA20(byte where, byte enabled) //To enable A20?
{
	MMU.wrapdisabled[where] = enabled; //Enabled?
	MMU.wraparround = (MMU.wrapdisabled[0]|MMU.wrapdisabled[1])?(~0):BITOFF(~0, 0x100000); //Wrap arround mask for A20 line!
}