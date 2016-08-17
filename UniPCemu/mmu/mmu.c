#define IS_MMU
#include "headers/mmu/mmu.h"
#include "headers/cpu/cpu.h"
#include "headers/mmu/memory_adressing.h" //Memory assist functions!
#include "headers/mmu/paging.h" //Paging functions!
#include "headers/support/zalloc.h" //Memory allocation!
#include "headers/support/log.h" //Logging support!
#include "headers/cpu/protection.h" //Protection support!
#include "headers/mmu/paging.h" //Protection support!
#include "headers/mmu/mmuhandler.h" //MMU Handler support!
#include "headers/emu/debugger/debugger.h" //Debugger support for logging MMU accesses!
#include "headers/hardware/dram.h" //DRAM support!
#include "headers/support/fifobuffer.h" //Write buffer support!
#include "headers/hardware/xtexpansionunit.h" //XT expansion unit support!

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
	if ((address<=MMU.size) && ((address+size)<=MMU.size)) //Within our limits of flat memory and not paged?
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
	if (MMU.memory==NULL) //None?
	{
		return NULL; //NULL: no memory alligned!
	}

	if (EMULATED_CPU<=CPU_NECV30) //-NEC V20/V30 wraps offset arround?
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

byte bufferMMUwrites = 0; //To buffer MMU writes?
FIFOBUFFER *MMUBuffer = NULL; //MMU write buffer!

void resetMMU()
{
	byte memory_allowresize = 1; //Do we allow resizing?
	if (__HW_DISABLED) return; //Abort!
	doneMMU(); //We're doing a full reset!
	resetmmu:
	//dolog("MMU","Initialising MMU...");
	MMU.size = BIOS_GetMMUSize(); //Take over predefined: don't try to detect!
	
	if ((EMULATED_CPU<=CPU_NECV30) && (MMU.size>0x100000)) MMU.size = 0x100000; //Limit unsupported sizes by the CPU!
	//dolog("zalloc","Allocating MMU memory...");
	MMU.memory = (byte *)zalloc(MMU.size,"MMU_Memory",NULL); //Allocate the memory available for the segments
	MMU.invaddr = 0; //Default: MMU address OK!
	user_memory_used = 0; //Default: no memory used yet!
	if (MMU.memory!=NULL && !force_memoryredetect) //Allocated and not forcing redetect?
	{
		MMU_setA20(0,0); //Default: Disabled A20 like 80(1)86!
		MMU_setA20(1,0); //Default: Disabled A20 like 80(1)86!
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
	MMUBuffer = allocfifobuffer(100*6,0); //Alloc the write buffer with 100 entries (100 bytes)
	
}

void doneMMU()
{
	if (__HW_DISABLED) return; //Abort!
	if (MMU.memory) //Got memory allocated?
	{
		freez((void **)&MMU.memory,MMU.size,"doneMMU_Memory"); //Release memory!
		MMU.size = 0; //Reset: none allocated!
	}
	if (MMUBuffer)
	{
		free_fifobuffer(&MMUBuffer); //Release us!
	}
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

uint_32 mem_BUSValue = 0; //Last memory read/written, BUS value stored during reads/writes!
const uint_32 BUSmask[4] = {0xFFFFFF00,0xFFFF00FF,0xFF00FFFF,0x00FFFFFF}; //Bus mask for easy toggling!

OPTINLINE void MMU_INTERNAL_INVMEM(uint_32 realddress, byte iswrite)
{
	return; //Disable this NMI!
	if (execNMI(1)) //Execute an NMI from memory!
	{
		MMU.invaddr = 1; //Signal invalid address!
	}
}

//Direct memory access (for the entire emulator)
OPTINLINE byte MMU_INTERNAL_directrb(uint_32 realaddress, byte index) //Direct read from real memory (with real data direct)!
{
	byte result;
	byte nonexistant = 0;
	if (realaddress&0x100000) //1MB+?
	{
		realaddress -= (0x100000-0xA0000); //Patch to less memory to make memory linear!
	}
	else if (realaddress>=0xA0000) //640K ISA memory hole addressed?
	{
		nonexistant = 1; //Non-existant memory!
	}
	if ((realaddress>=MMU.size) || nonexistant) //Overflow/invalid location?
	{
		MMU_INTERNAL_INVMEM(realaddress,0); //Invalid memory accessed!
		return (mem_BUSValue>>((index&3)<<3)); //Give the last data read/written by the BUS!
	}
	result = MMU.memory[realaddress]; //Get data from memory!
	DRAM_access(realaddress); //Tick the DRAM!
	if (index!=0xFF) //Don't ignore BUS?
	{
		mem_BUSValue &= BUSmask[index&3]; //Apply the bus mask!
		mem_BUSValue |= (result<<((index&3)<<3)); //Or into the last read/written value!
	}
	return result; //Give existant memory!
}

byte LOG_MMU_WRITES = 0; //Log MMU writes?

OPTINLINE void MMU_INTERNAL_directwb(uint_32 realaddress, byte value, byte index) //Direct write to real memory (with real data direct)!
{
	if (LOG_MMU_WRITES) //Data debugging?
	{
		dolog("debugger","MMU: Writing to real %08X=%02X (%c)",realaddress,value,value?value:0x20);
	}
	//Apply the 640K memory hole!
	byte nonexistant = 0;
	if (realaddress&0x100000) //1MB+?
	{
		realaddress -= (0x100000-0xA0000); //Patch to less memory to make memory linear!
	}
	else if (realaddress>=0xA0000) //640K ISA memory hole addressed?
	{
		nonexistant = 1; //Non-existant memory!
	}
	if (index!=0xFF) //Don't ignore BUS?
	{
		mem_BUSValue &= BUSmask[index&3]; //Apply the bus mask!
		mem_BUSValue |= (value<<((index&3)<<3)); //Or into the last read/written value!
	}
	if ((realaddress>=MMU.size) || nonexistant) //Overflow/invalid location?
	{
		MMU_INTERNAL_INVMEM(realaddress,1); //Invalid memory accessed!
		return; //Abort!
	}
	MMU.memory[realaddress] = value; //Set data, full memory protection!
	DRAM_access(realaddress); //Tick the DRAM!
	if (realaddress>user_memory_used) //More written than present in memory (first write to addr)?
	{
		user_memory_used = realaddress; //Update max memory used!
	}
}

//Used by the DMA controller only (or the paging system for faster reads):
OPTINLINE word MMU_INTERNAL_directrw(uint_32 realaddress, byte index) //Direct read from real memory (with real data direct)!
{
	return (MMU_INTERNAL_directrb(realaddress+1,index|1)<<8)|MMU_INTERNAL_directrb(realaddress,index); //Get data, wrap arround!
}

OPTINLINE void MMU_INTERNAL_directww(uint_32 realaddress, word value, byte index) //Direct write to real memory (with real data direct)!
{
	MMU_INTERNAL_directwb(realaddress,value&0xFF,index); //Low!
	MMU_INTERNAL_directwb(realaddress+1,(value>>8)&0xFF,index|1); //High!
}

//Used by paging only!
OPTINLINE uint_32 MMU_INTERNAL_directrdw(uint_32 realaddress, byte index)
{
	return (MMU_INTERNAL_directrw(realaddress+2,index|2)<<16)|MMU_INTERNAL_directrw(realaddress,index); //Get data, wrap arround!	
}
OPTINLINE void MMU_INTERNAL_directwdw(uint_32 realaddress, uint_32 value, byte index)
{
	MMU_INTERNAL_directww(realaddress,value&0xFF,index); //Low!
	MMU_INTERNAL_directww(realaddress+2,(value>>8)&0xFF,index|2); //High!
}

//Direct memory access with Memory mapped I/O (for the CPU).
OPTINLINE byte MMU_INTERNAL_directrb_realaddr(uint_32 realaddress, byte opcode, byte index) //Read without segment/offset translation&protection (from system/interrupt)!
{
	byte data;
	if (MMU_IO_readhandler(realaddress,&data)) //Normal memory address?
	{
		data = MMU_INTERNAL_directrb(realaddress,index); //Read the data from memory (and port I/O)!		
	}
	if (MMU_logging && (!opcode)) //To log?
	{
		dolog("debugger", "Read from memory: %08X=%02X (%c)", realaddress, data, data ? data : 0x20); //Log it!
	}
	return data;
}

byte enableMMUbuffer = 0; //To buffer the MMU writes?

OPTINLINE void MMU_INTERNAL_directwb_realaddr(uint_32 realaddress, byte val, byte index) //Write without segment/offset translation&protection (from system/interrupt)!
{
	union
	{
		uint_32 realaddress; //The address!
		byte addr[4];
	} addressconverter;
	byte status;
	if (enableMMUbuffer && MMUBuffer) //To buffer all writes?
	{
		if (fifobuffer_freesize(MMUBuffer)>=7) //Enough size left to buffer?
		{
			addressconverter.realaddress = realaddress; //The address to break up!
			status = 1; //1 byte written!
			if (!writefifobuffer(MMUBuffer,status)) return; //Invalid data!
			if (!writefifobuffer(MMUBuffer,addressconverter.addr[0])) return; //Invalid data!
			if (!writefifobuffer(MMUBuffer,addressconverter.addr[1])) return; //Invalid data!
			if (!writefifobuffer(MMUBuffer,addressconverter.addr[2])) return; //Invalid data!
			if (!writefifobuffer(MMUBuffer,addressconverter.addr[3])) return; //Invalid data!
			if (!writefifobuffer(MMUBuffer,val)) return; //Invalid data!
			if (!writefifobuffer(MMUBuffer,index)) return; //Invalid data!
			return;
		}
	}
	if (MMU_logging) //To log?
	{
		dolog("debugger", "Writing to memory: %08X=%02X (%c)", realaddress, val,val?val:0x20); //Log it!
	}
	if (MMU_ignorewrites) return; //Ignore all written data: protect memory integrity!
	if (MMU_IO_writehandler(realaddress, val)) //Normal memory access?
	{
		MMU_INTERNAL_directwb(realaddress,val,index); //Set data in real memory!
	}
}

void flushMMU() //Flush MMU writes!
{
	union
	{
		uint_32 realaddress; //The address!
		byte addr[4];
	} addressconverter;
	byte status;
	byte val,index;
	//Read the buffer
	enableMMUbuffer = 0; //Finished buffering!
	for (;readfifobuffer(MMUBuffer,&status);) //Gotten data to write(byte/word/dword data)?
	{
		//Status doesn't have any meaning yet, so ignore it(always byte data)!
		if (!readfifobuffer(MMUBuffer,&addressconverter.addr[0])) break; //Invalid data!
		if (!readfifobuffer(MMUBuffer,&addressconverter.addr[1])) break; //Invalid data!
		if (!readfifobuffer(MMUBuffer,&addressconverter.addr[2])) break; //Invalid data!
		if (!readfifobuffer(MMUBuffer,&addressconverter.addr[3])) break; //Invalid data!
		if (!readfifobuffer(MMUBuffer,&val)) break; //Invalid data!
		if (!readfifobuffer(MMUBuffer,&index)) break; //Invalid data!
		MMU_INTERNAL_directwb_realaddr(addressconverter.realaddress,val,index); //Write the value to memory!
	}
}

void bufferMMU() //Buffer MMU writes!
{
	enableMMUbuffer = 1; //Buffer MMU writes!
}

byte writeword = 0; //Hi-end word written?

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
	//We work!
	//dolog("MMU","\nAddress translation: %04X:%08X=%08X",originalsegment,originaloffset,realaddress); //Log the converted address!
	latchBUS(realaddress); //This address is to be latched!
	return realaddress; //Give real adress!
}

uint_32 MMU_translateaddr(sword segdesc, word segment, uint_32 offset, byte wordop) //Translate to actual linear adress?
{
	return MMU_realaddr(segdesc, segment, offset, wordop); //Translate it!
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

	if (CPU_MMU_checklimit(segdesc,segment,offset,1|(opcode<<1))) //Disallowed?
	{
		//dolog("MMU","R:Limit break:%04X:%08X!",segment,offset);
		MMU.invaddr = 2; //Invalid address!
		return 0xFF; //Not found.
	}

	realaddress = MMU_realaddr(segdesc, segment, offset, writeword); //Real adress!

	if (is_paging()) //Are we paging?
	{
		realaddress = mappage(realaddress); //Map it using the paging mechanism!
	}

	if (writewordbackup==0) //First data of the word access?
	{
		wordaddress = realaddress; //Word address used during memory access!
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
	
	if (CPU_MMU_checklimit(segdesc,segment,offset,0)) //Disallowed?
	{
		MMU.invaddr = 2; //Invalid address signaling!
		return; //Not found.
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

	if (writewordbackup==0) //First data of the word access?
	{
		wordaddress = realaddress; //Word address used during memory access!
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
	if (segdesc!=-1) CPU[activeCPU].cycles_MMUW += 4; //CPU writes are counted!
	MMU_INTERNAL_wb(segdesc,segment,offset,val,0);
}
void MMU_ww(sword segdesc, word segment, uint_32 offset, word val) //Set adress!
{
	if (segdesc!=-1) CPU[activeCPU].cycles_MMUW += CPU_databussize?8:4; //CPU writes are counted!
	MMU_INTERNAL_ww(segdesc,segment,offset,val,0);
}
void MMU_wdw(sword segdesc, word segment, uint_32 offset, uint_32 val) //Set adress!
{
	MMU_INTERNAL_wdw(segdesc,segment,offset,val,0);
}
byte MMU_rb(sword segdesc, word segment, uint_32 offset, byte opcode) //Get adress, opcode=1 when opcode reading, else 0!
{
	if (segdesc!=-1) CPU[activeCPU].cycles_MMUR += 4; //CPU writes are counted!
	return MMU_INTERNAL_rb(segdesc,segment,offset,opcode,0);
}
word MMU_rw(sword segdesc, word segment, uint_32 offset, byte opcode) //Get adress, opcode=1 when opcode reading, else 0!
{
	if (segdesc!=-1) CPU[activeCPU].cycles_MMUR += CPU_databussize?8:4; //CPU writes are counted!
	return MMU_INTERNAL_rw(segdesc,segment,offset,opcode,0);
}
uint_32 MMU_rdw(sword segdesc, word segment, uint_32 offset, byte opcode) //Get adress, opcode=1 when opcode reading, else 0!
{
	return MMU_INTERNAL_rdw(segdesc,segment,offset,opcode,0);
}

//Direct memory access routines (used by DMA)!
byte MMU_directrb(uint_32 realaddress) //Direct read from real memory (with real data direct)!
{
	return MMU_INTERNAL_directrb(realaddress,0);
}
word MMU_directrw(uint_32 realaddress) //Direct read from real memory (with real data direct)!
{
	return MMU_INTERNAL_directrw(realaddress,0);
}
uint_32 MMU_directrdw(uint_32 realaddress) //Direct read from real memory (with real data direct)!
{
	return MMU_INTERNAL_directrdw(realaddress,0);
}
void MMU_directwb(uint_32 realaddress, byte value) //Direct write to real memory (with real data direct)!
{
	MMU_INTERNAL_directwb(realaddress,value,0);
}
void MMU_directww(uint_32 realaddress, word value) //Direct write to real memory (with real data direct)!
{
	MMU_INTERNAL_directww(realaddress,value,0);
}
void MMU_directwdw(uint_32 realaddress, uint_32 value) //Direct write to real memory (with real data direct)!
{
	MMU_INTERNAL_directwdw(realaddress,value,0);
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
void MMU_setA20(byte where, byte enabled) //To enable A20?
{
	MMU.wrapdisabled[where] = enabled; //Enabled?
	MMU.wraparround = (MMU.wrapdisabled[0]|MMU.wrapdisabled[1])?(~0):BITOFF(~0, 0x100000); //Wrap arround mask for A20 line!
}