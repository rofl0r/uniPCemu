#define IS_MMU
#include "headers/mmu/mmu.h"
#include "headers/cpu/cpu.h"
#include "headers/bios/bios.h"
#include "headers/mmu/memory_adressing.h" //Memory assist functions!
#include "headers/mmu/paging.h" //Paging functions!
#include "headers/emu/gpu/gpu.h" //GPU support!
#include "headers/support/zalloc.h" //Memory allocation!
#include "headers/support/log.h" //Logging support!
#include "headers/cpu/protection.h" //Protection support!
#include "headers/mmu/mmuhandler.h" //MMU Handler support!
#include "headers/emu/debugger/debugger.h" //Debugger support for logging MMU accesses!

//Are we disabled?
#define __HW_DISABLED 0

byte MMU_logging = 0; //Are we logging?

byte MMU_ignorewrites = 0; //Ignore writes to the MMU from the CPU?

MMU_type MMU; //The MMU itself!

extern byte EMU_RUNNING; //Emulator is running?

extern BIOS_Settings_TYPE BIOS_Settings; //The BIOS!

//Pointer support (real mode only)!

void *MMU_directptr(uint_32 address, uint_32 size) //Full ptr to real MMU memory!
{
	if (address<=MMU.size) //Within our limits of flat memory and not paged?
	{
		return &MMU.memory[address]; //Give the memory's start!
	}

	MMU.invaddr = 1; //Invalid address!
	return NULL; //Not found!	
}

//MMU_ptr: 80(1)86 only!
void *MMU_ptr(sword segdesc, word segment, uint_32 offset, byte forreading, uint_32 size) //Gives direct memory pointer!
{
	uint_32 realaddr;
	if (MMU.memory==NULL) //None?
	{
		return NULL; //NULL: no memory alligned!
	}

	if (EMULATED_CPU<=CPU_80186) //-80186 wraps offset arround?
	{
		offset &= 0xFFFF; //Wrap arround!
	}
	else
	{
		return NULL; //80286+ isn't supported here!
	}
	realaddr = (segment<<4)+offset; //Our real address!
	realaddr = BITOFF(realaddr,0x100000); //Wrap arround, disable A20!	
	return MMU_directptr(realaddr,size); //Direct pointer!
}

uint_32 user_memory_used = 0; //Memory used by the software!
byte force_memoryredetect = 0; //Force memory redetect?

void resetMMU()
{
	byte memory_allowresize = 1; //Do we allow resizing?
	if (__HW_DISABLED) return; //Abort!
	doneMMU(); //We're doing a full reset!
	resetmmu:
	//dolog("MMU","Initialising MMU...");
	MMU.size = BIOS_GetMMUSize(); //Take over predefined: don't try to detect!
	//dolog("zalloc","Allocating MMU memory...");
	MMU.memory = (byte *)zalloc(MMU.size,"MMU_Memory",NULL); //Allocate the memory available for the segments
	MMU.invaddr = 0; //Default: MMU address OK!
	user_memory_used = 0; //Default: no memory used yet!
	if (MMU.memory!=NULL && !force_memoryredetect) //Allocated and not forcing redetect?
	{
		MMU_wraparround(1); //Default: wrap arround 20 bits memory address!
	}
	else //Not allocated?
	{
		MMU.size = 0; //We don't have size!
		doneMMU(); //Free up memory if allocated, to make sure we're not allocated anymore on the next try!
		if (memory_allowresize) //Can we resize memory?
		{
			autoDetectMemorySize(1); //Redetect memory size!
			force_memoryredetect = 0; //Not forcing redetect anymore: we've been redetected!
			memory_allowresize = 0; //Don't allow resizing anymore!
			goto resetmmu; //Try again!
		}
	}
	memory_allowresize = 1; //Allow resizing again!
	if (!MMU.size || !MMU.memory) //No size?
	{
		raiseError("MMU","No memory available to use!");
	}
}

void doneMMU()
{
	if (__HW_DISABLED) return; //Abort!
	if (MMU.memory) //Got memory allocated?
	{
		freez((void **)&MMU.memory,MMU.size,"doneMMU_Memory"); //Release memory!
		MMU.size = 0; //Reset: none allocated!
	}
	MMU_resetHandlers(NULL); //Reset all linked handlers!
}


uint_32 MEMsize() //Total size of memory in use?
{
	if (MMU.memory!=NULL) //Have memory?
	{
		return MMU.size; //Give number of bytes!
	}
	else
	{
		return 0; //Error!
	}
}

//Direct memory access (for the entire emulator)
byte MMU_directrb(uint_32 realaddress) //Direct read from real memory (with real data direct)!
{
	if (realaddress>MMU.size) //Overflow?
	{
		MMU.invaddr = 1; //Signal invalid address!
		execNMI(1); //Execute an NMI from memory!
		return 0xFF; //Nothing there!
	}
	return MMU.memory[realaddress]; //Get data, wrap arround!
}

byte LOG_MMU_WRITES = 0; //Log MMU writes?

void MMU_directwb(uint_32 realaddress, byte value) //Direct write to real memory (with real data direct)!
{
	if (LOG_MMU_WRITES) //Data debugging?
	{
		dolog("debugger","MMU: Writing to real %08X=%02X (%c)",realaddress,value,value?value:0x20);
	}
	if (realaddress>MMU.size) //Overflow?
	{
		MMU.invaddr = 1; //Signal invalid address!
		execNMI(1); //Execute an NMI from memory!
		return; //Abort: can't write here!
	}
	MMU.memory[realaddress] = value; //Set data, full memory protection!
	if (realaddress>user_memory_used) //More written than present in memory (first write to addr)?
	{
		user_memory_used = realaddress; //Update max memory used!
	}
}

//Used by the DMA controller only (or the paging system for faster reads):
word MMU_directrw(uint_32 realaddress) //Direct read from real memory (with real data direct)!
{
	return (MMU_directrb(realaddress+1)<<8)|MMU_directrb(realaddress); //Get data, wrap arround!
}

void MMU_directww(uint_32 realaddress, word value) //Direct write to real memory (with real data direct)!
{
	MMU_directwb(realaddress,value&0xFF);; //Low!
	MMU_directwb(realaddress+1,(value>>8)&0xFF); //High!
}

//Used by paging only!
uint_32 MMU_directrdw(uint_32 realaddress)
{
	return (MMU_directrw(realaddress+2)<<16)|MMU_directrw(realaddress); //Get data, wrap arround!	
}
void MMU_directwdw(uint_32 realaddress, uint_32 value)
{
	MMU_directww(realaddress,value&0xFF); //Low!
	MMU_directww(realaddress+2,(value>>8)&0xFF); //High!
}

//Direct memory access with Memory mapped I/O (for the CPU).
OPTINLINE byte MMU_directrb_realaddr(uint_32 realaddress, byte opcode) //Read without segment/offset translation&protection (from system/interrupt)!
{
	byte data;
	if (MMU_IO_readhandler(realaddress,&data)) //Normal memory address?
	{
		data = MMU_directrb(realaddress); //Read the data from memory (and port I/O)!		
	}
	if (MMU_logging && (!opcode)) //To log?
	{
		dolog("debugger", "Read from memory: %08X=%02X (%c)", realaddress, data, data ? data : 0x20); //Log it!
	}
	return data;
}

OPTINLINE void MMU_directwb_realaddr(uint_32 realaddress, byte val) //Write without segment/offset translation&protection (from system/interrupt)!
{
	if (MMU_logging) //To log?
	{
		dolog("debugger", "Writing to memory: %08X=%02X (%c)", realaddress, val,val?val:0x20); //Log it!
	}
	if (MMU_ignorewrites) return; //Ignore all written data: protect memory integrity!
	if (MMU_IO_writehandler(realaddress, val)) //Normal memory access?
	{
		MMU_directwb(realaddress,val); //Set data in real memory!
	}
}

byte writeword = 0; //Hi-end word written?

//Address translation routine.
OPTINLINE uint_32 MMU_realaddr(sword segdesc, word segment, uint_32 offset, byte wordop) //Real adress?
{
	register uint_32 realaddress;
	//word originalsegment = segment;
	//uint_32 originaloffset = offset; //Save!
	if ((EMULATED_CPU==CPU_8086) || (EMULATED_CPU==CPU_80186 && !((offset==0x10000) && wordop))) //-80186 wraps offset arround 64kB? 80186 allows 1 byte more in word operations!
	{
		offset &= 0xFFFF; //Wrap arround!
	}
	writeword = 0; //Reset word-write flag for checking next bytes!

	realaddress = CPU_MMU_start(segdesc, segment);
	realaddress += offset; //Real adress!

	realaddress &= MMU.wraparround; //Wrap arround, disable A20!
	//We work!
	//dolog("MMU","\nAddress translation: %04X:%08X=%08X",originalsegment,originaloffset,realaddress); //Log the converted address!
	return realaddress; //Give real adress!
}

//OPcodes for the debugger!
byte OPbuffer[256]; //A large opcode buffer!
byte OPlength = 0; //The length of the opcode buffer!

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
byte MMU_rb(sword segdesc, word segment, uint_32 offset, byte opcode) //Get adress, opcode=1 when opcode reading, else 0!
{
	byte result; //The result!
	uint_32 realaddress;
	if ((MMU.memory==NULL) || (MMU.size==0)) //No mem?
	{
		//dolog("MMU","R:No memory present!");
		MMU.invaddr = 1; //Invalid adress!
		return 0; //Out of bounds!
	}

	if (CPU_MMU_checklimit(segdesc,segment,offset,1|(opcode<<1))) //Disallowed?
	{
		//dolog("MMU","R:Limit break:%04X:%08X!",segment,offset);
		MMU.invaddr = 1; //Invalid address!
		return 0; //Not found.
	}

	realaddress = MMU_realaddr(segdesc,segment,offset,writeword); //Real adress!
	
	result = MMU_directrb_realaddr(realaddress,opcode); //Read from MMU/hardware!

	if (opcode == 1) //We're an OPcode retrieval?
	{
		MMU_addOP(result); //Add to the opcode cache!
	}

	return result; //Give the result!
}

word MMU_rw(sword segdesc, word segment, uint_32 offset, byte opcode) //Get adress!
{
	word result;
	result = MMU_rb(segdesc, segment, offset, opcode);
	result |= (MMU_rb(segdesc, segment, offset + 1, opcode) << 8); //Get adress word!
	return result; //Give the result!
}

uint_32 MMU_rdw(sword segdesc, word segment, uint_32 offset, byte opcode) //Get adress!
{
	uint_32 result;
	result = MMU_rw(segdesc, segment, offset, opcode);
	result |= (MMU_rw(segdesc, segment, offset + 2, opcode) << 16); //Get adress dword!
	return result; //Give the result!
}

void MMU_wb(sword segdesc, word segment, uint_32 offset, byte val) //Set adress!
{
	uint_32 realaddress;
	if (MMU.invaddr) return; //Abort!
	if ((MMU.memory==NULL) || !MMU.size) //No mem?
	{
		//dolog("MMU","W:No memory present!");
		return; //Out of bounds!
	}
	
	if (CPU[activeCPU].faultraised && EMU_RUNNING) //Fault has been raised while emulator is running?
	{
		return; //Disable writes to memory when a fault has been raised!
	}

	if (CPU_MMU_checklimit(segdesc,segment,offset,0)) //Disallowed?
	{
		MMU.invaddr = 1; //Invalid address signaling!
		return; //Not found.
	}
	
	/*if (LOG_MMU_WRITES) //Log MMU writes?
	{
		dolog("debugger","MMU: Write to %04X:%08X=%02X",segment,offset,val); //Log our written value!
	}*/

	realaddress = MMU_realaddr(segdesc,segment,offset,writeword); //Real adress!

	MMU_directwb_realaddr(realaddress,val); //Set data!
}

void MMU_ww(sword segdesc, word segment, uint_32 offset, word val) //Set adress (word)!
{
	MMU_wb(segdesc,segment,offset,val&0xFF); //Low first!
	writeword = 1; //We're writing a 2nd byte word, for emulating the 80186 0x10000 overflow bug.
	MMU_wb(segdesc,segment,offset+1,(val>>8)&0xFF); //High last!
}

void MMU_wdw(sword segdesc, word segment, uint_32 offset, uint_32 val) //Set adress (dword)!
{
	MMU_ww(segdesc,segment,offset,val&0xFFFF); //Low first!
	MMU_ww(segdesc,segment,offset+2,(val>>16)&0xFFFF); //High last!
}

//Extra routines for the emulator.

//Dump memory
void MMU_dumpmemory(char *filename) //Dump the memory to a file!
{
	FILE *f;
	f = fopen(filename,"wb"); //Open file!
	fwrite(MMU.memory,1,user_memory_used,f); //Write memory to file!
	fclose(f); //Close file!
}

//Have memory available?
byte hasmemory()
{
	if (MMU.memory==NULL) //No memory?
	{
		return 0; //No memory!
	}
	if (MMU.size==0) //No memory?
	{
		return 0; //No memory!
	}
	return 1; //Have memory!
}

//Memory has gone wrong in direct access?
byte MMU_invaddr()
{
	return MMU.invaddr; //Given an invalid adress?
}

void MMU_resetaddr()
{
	MMU.invaddr = 0; //Reset: we're valid again!
}

//A20 bit enable/disable (80286+).
void MMU_wraparround(byte dowrap) //To wrap arround 1MB limit?
{
	//dolog("MMU","Set A20 bit disabled: %i",dowrap); //To wrap arround?
	MMU.wraparround = (dowrap>0)? BITOFF(~0, 0x100000):~0; //Wrap arround mask!
}