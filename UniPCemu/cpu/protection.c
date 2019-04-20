#include "headers/cpu/cpu.h" //Basic CPU info!
#include "headers/cpu/protection.h" //Protection support!
#include "headers/cpu/multitasking.h" //Multitasking support!
#include "headers/support/zalloc.h" //Memory/register protection support!
#include "headers/mmu/mmuhandler.h" //Direct memory access support!
#include "headers/emu/debugger/debugger.h" //For logging check!
#include "headers/support/locks.h" //We need to unlock ourselves during triple faults, to reset ourselves!
#include "headers/cpu/cpu_pmtimings.h" //286+ timing support!
#include "headers/cpu/easyregs.h" //Easy register support!
#include "headers/support/log.h" //Logging support!
#include "headers/cpu/biu.h" //BIU support!
#include "headers/cpu/cpu_execution.h" //Execution flow support!
#include "headers/cpu/cpu_OP8086.h" //8086+ push/pop support!
#include "headers/cpu/cpu_OP80386.h" //80386+ push/pop support!

//Log Virtual 8086 mode calls basic information?
//#define LOG_VIRTUALMODECALLS

/*

Basic CPU active segment value retrieval.

*/

//Exceptions, 286+ only!

//Reading of the 16-bit entries within descriptors!
#ifndef IS_PSP
#define DESC_16BITS(x) SDL_SwapLE16(x)
#else
#define DESC_16BITS(x) (x)
#endif

extern byte hascallinterrupttaken_type; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task gate flag. Left at 0xFF when nothing is used(unknown case?)

uint_32 CALLGATE_NUMARGUMENTS = 0; //The amount of arguments of the call gate!

extern byte advancedlog; //Advanced log setting
extern byte MMU_logging; //Are we logging from the MMU?
void CPU_triplefault()
{
	CPU[activeCPU].faultraised_lasttype = 0xFF; //Full on reset has been raised!
	CPU[activeCPU].resetPending = 1; //Start pending a reset!
	CPU[activeCPU].faultraised = 1; //We're continuing being a fault!
	CPU[activeCPU].executed = 1; //We're finishing to execute!
	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		dolog("debugger", "#Triple fault!");
	}
}

void CPU_doublefault()
{
	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		dolog("debugger", "#DF fault(%08X)!", 0);
	}

	CPU[activeCPU].faultraised_lasttype = EXCEPTION_DOUBLEFAULT;
	CPU[activeCPU].faultraised = 1; //Raising a fault!
	uint_64 zerovalue=0; //Zero value pushed!
	++CPU[activeCPU].faultlevel; //Raise the fault level to cause triple faults!
	CPU_executionphase_startinterrupt(EXCEPTION_DOUBLEFAULT,2,zerovalue); //Execute the double fault handler!
}

extern byte CPU_interruptraised; //Interrupt raised flag?

byte CPU_faultraised(byte type)
{
	if (EMULATED_CPU<CPU_80286) return 1; //Always allow on older processors without protection!
	if ((hascallinterrupttaken_type!=0xFF) || (CPU_interruptraised)) //Were we caused while raising an interrupt or other pending timing?
	{
		CPU_apply286cycles(); //Apply any cycles that need to be applied for the current interrupt to happen!
	}
	if (CPU[activeCPU].faultlevel) //Double/triple fault might have been raised?
	{
		if (CPU[activeCPU].faultlevel == 2) //Triple fault?
		{
			CPU_triplefault(); //Triple faulting!
			return 0; //Shutdown!
		}
		else
		{
			//Based on the table at http://os.phil-opp.com/double-faults.html whether or not to cause a double fault!
			CPU[activeCPU].faultlevel = 1; //We have a fault raised, so don't raise any more!
			switch (CPU[activeCPU].faultraised_lasttype) //What type was first raised?
			{
				//Contributory causing...
				case EXCEPTION_DIVIDEERROR:
				case EXCEPTION_COPROCESSOROVERRUN:
				case EXCEPTION_INVALIDTSSSEGMENT:
				case EXCEPTION_SEGMENTNOTPRESENT:
				case EXCEPTION_STACKFAULT:
				case EXCEPTION_GENERALPROTECTIONFAULT: //First cases?
					switch (type) //What second cause?
					{
						//... Contributory?
						case EXCEPTION_DIVIDEERROR:
						case EXCEPTION_COPROCESSOROVERRUN:
						case EXCEPTION_INVALIDTSSSEGMENT:
						case EXCEPTION_SEGMENTNOTPRESENT:
						case EXCEPTION_STACKFAULT:
						case EXCEPTION_GENERALPROTECTIONFAULT:
							CPU_doublefault(); //Double faulting!
							return 0; //Don't do anything anymore(partial shutdown)!
							break;
						default: //Normal handling!
							break;
					}
					break;
				//Page fault causing...
				case EXCEPTION_PAGEFAULT: //Page fault? Second case!
					switch (type) //What second cause?
					{
						//... Page fault or ...
						case EXCEPTION_PAGEFAULT:
						//... Contributory?
						case EXCEPTION_DIVIDEERROR:
						case EXCEPTION_COPROCESSOROVERRUN:
						case EXCEPTION_INVALIDTSSSEGMENT:
						case EXCEPTION_SEGMENTNOTPRESENT:
						case EXCEPTION_STACKFAULT:
						case EXCEPTION_GENERALPROTECTIONFAULT:
							CPU_doublefault(); //Double faulting!
							return 0; //Don't do anything anymore(partial shutdown)!
							break;
						default: //Normal handling!
							break;
					}
					break;
				case EXCEPTION_DOUBLEFAULT: //Special case to prevent breakdown?
					if (type==EXCEPTION_DOUBLEFAULT) //We're a double fault raising a double fault?
					{
						CPU_doublefault(); //Double fault!
						return 0; //Don't do anything anymore(partial shutdown)!
					}
				default: //No double fault!
					break;
			}
		}
	}
	else
	{
		CPU[activeCPU].faultlevel = 1; //We have a fault raised, so don't raise any more!
	}
	CPU[activeCPU].faultraised_lasttype = type; //Last type raised!
	CPU[activeCPU].faultraised = 1; //We've raised a fault! Ignore more errors for now!
	return 1; //Handle the fault normally!
}

void CPU_onResettingFault()
{
	byte segRegLeft,segRegIndex,segRegShift;
	if (CPU[activeCPU].have_oldCPL) //Returning the CPL to it's old value?
	{
		CPU[activeCPU].CPL = CPU[activeCPU].oldCPL; //Restore CPL to it's original value!
	}
	if (CPU[activeCPU].have_oldESP) //Returning the (E)SP to it's old value?
	{
		REG_ESP = CPU[activeCPU].oldESP; //Restore ESP to it's original value!
	}
	if (CPU[activeCPU].have_oldEBP) //Returning the (E)BP to it's old value?
	{
		REG_EBP = CPU[activeCPU].oldEBP; //Restore EBP to it's original value!
	}
	if (CPU[activeCPU].have_oldEFLAGS) //Returning the (E)SP to it's old value?
	{
		REG_EFLAGS = CPU[activeCPU].oldEFLAGS; //Restore EFLAGS to it's original value!
		updateCPUmode(); //Restore the CPU mode!
	}

	//Restore any segment register caches that are changed!
	segRegIndex = 0; //Init!
	segRegLeft = CPU[activeCPU].have_oldSegReg; //What segment registers are loaded to restore!
	segRegShift = 1; //Current bit to test!
	if (unlikely(segRegLeft)) //Anything to restore?
	{
		for (;(segRegLeft);) //Any segment register and cache left to restore?
		{
			if (segRegLeft&segRegShift) //Something to restore at the current segment register?
			{
				*CPU[activeCPU].SEGMENT_REGISTERS[segRegIndex] = CPU[activeCPU].oldSegReg[segRegIndex]; //Restore the segment register selector/value to it's original value!
				//Restore backing descriptor!
				memcpy(&CPU[activeCPU].SEG_DESCRIPTOR[segRegIndex], &CPU[activeCPU].SEG_DESCRIPTORbackup[segRegIndex], sizeof(CPU[activeCPU].SEG_DESCRIPTOR[0])); //Restore the descriptor!
				segRegLeft &= ~segRegShift; //Not set anymore!
			}
			segRegShift <<= 1; //Next segment register!
			++segRegIndex; //Next index to process!
		}
		CPU[activeCPU].have_oldSegReg = 0; //All segment registers and their caches have been restored!
	}
}

void CPU_commitState() //Prepare for a fault by saving all required data!
{
	//SS descriptor is linked to the CPL in some cases, so backup that as well!
	CPU[activeCPU].oldSS = REG_SS; //Save the most frequently used SS state!
	//Backup the descriptor itself!
	CPU[activeCPU].oldESP = REG_ESP; //Restore ESP to it's original value!
	CPU[activeCPU].have_oldESP = 1; //Restorable!
	CPU[activeCPU].oldEBP = REG_EBP; //Restore EBP to it's original value!
	CPU[activeCPU].have_oldEBP = 1; //Restorable!
	CPU[activeCPU].oldEFLAGS = REG_EFLAGS; //Restore EFLAGS to it's original value!
	CPU[activeCPU].have_oldEFLAGS = 1; //Restorable!
	updateCPUmode(); //Restore the CPU mode!
	CPU[activeCPU].have_oldCPL = 1; //Restorable!
	CPU[activeCPU].oldCPL = CPU[activeCPU].CPL; //Restore CPL to it's original value!
	CPU[activeCPU].oldCPUmode = getcpumode(); //Save the CPU mode!
	//Backup the descriptors themselves!
	//TR is only to be restored during a section of the task switching process, so we don't save it right here(as it's unmodified, except during task switches)!
	CPU[activeCPU].have_oldSegReg = 0; //Commit the segment registers!
}

//More info: http://wiki.osdev.org/Paging
//General Protection fault.
void CPU_GP(int_64 errorcode)
{
	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		if (errorcode>=0)
		{
			dolog("debugger","#GP fault(%08X)!",errorcode);
		}
		else
		{
			dolog("debugger","#GP fault(-1)!");
		}
	}
	if (CPU_faultraised(EXCEPTION_GENERALPROTECTIONFAULT)) //Fault raising exception!
	{
		CPU_resetOP(); //Point to the faulting instruction!
		CPU_onResettingFault(); //Apply reset to fault!
		CPU_executionphase_startinterrupt(EXCEPTION_GENERALPROTECTIONFAULT,2,errorcode); //Call IVT entry #13 decimal!
		//Execute the interrupt!
	}
}

void CPU_AC(int_64 errorcode)
{
	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		if (errorcode>=0)
		{
			dolog("debugger","#AC fault(%08X)!",errorcode);
		}
		else
		{
			dolog("debugger","#AC fault(-1)!");
		}
	}
	if (CPU_faultraised(EXCEPTION_ALIGNMENTCHECK)) //Fault raising exception!
	{
		CPU_resetOP(); //Point to the faulting instruction!
		CPU_onResettingFault(); //Apply reset to fault!
		CPU_executionphase_startinterrupt(EXCEPTION_ALIGNMENTCHECK,2,errorcode); //Call IVT entry #13 decimal!
		//Execute the interrupt!
	}
}

void CPU_SegNotPresent(int_64 errorcode)
{
	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		if (errorcode>=0)
		{
			dolog("debugger","#NP fault(%08X)!",errorcode);
		}
		else
		{
			dolog("debugger","#NP fault(-1)!");
		}
	}
	if (CPU_faultraised(EXCEPTION_SEGMENTNOTPRESENT)) //Fault raising exception!
	{
		CPU_resetOP(); //Point to the faulting instruction!
		CPU_onResettingFault(); //Apply reset to fault!
		CPU_executionphase_startinterrupt(EXCEPTION_SEGMENTNOTPRESENT,2,errorcode); //Call IVT entry #11 decimal!
		//Execute the interrupt!
	}
}

void CPU_StackFault(int_64 errorcode)
{
	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		if (errorcode>=0)
		{
			dolog("debugger","#SS fault(%08X)!",errorcode);
		}
		else
		{
			dolog("debugger","#SS fault(-1)!");
		}
	}

	if (CPU_faultraised(EXCEPTION_STACKFAULT)) //Fault raising exception!
	{
		CPU_resetOP(); //Point to the faulting instruction!
		CPU_onResettingFault(); //Apply reset to fault!
		CPU_executionphase_startinterrupt(EXCEPTION_STACKFAULT,2,errorcode); //Call IVT entry #12 decimal!
		//Execute the interrupt!
	}
}

extern byte protection_PortRightsLookedup; //Are the port rights looked up?

void protection_nextOP() //We're executing the next OPcode?
{
	CPU[activeCPU].faultraised = 0; //We don't have a fault raised anymore, so we can raise again!
	CPU[activeCPU].faultlevel = 0; //Reset the current fault level!
	protection_PortRightsLookedup = 0; //Are the port rights looked up, to be reset?
}

word CPU_segment(byte defaultsegment) //Plain segment to use!
{
	return (CPU[activeCPU].segment_register==CPU_SEGMENT_DEFAULT) ? *CPU[activeCPU].SEGMENT_REGISTERS[defaultsegment] : *CPU[activeCPU].SEGMENT_REGISTERS[CPU[activeCPU].segment_register]; //Use Data Segment (or different) for data!
}

word *CPU_segment_ptr(byte defaultsegment) //Plain segment to use, direct access!
{
	return (CPU[activeCPU].segment_register==CPU_SEGMENT_DEFAULT) ? CPU[activeCPU].SEGMENT_REGISTERS[defaultsegment] : CPU[activeCPU].SEGMENT_REGISTERS[CPU[activeCPU].segment_register]; //Use Data Segment (or different) for data!
}

int CPU_segment_index(byte defaultsegment) //Plain segment to use, direct access!
{
	return (CPU[activeCPU].segment_register==CPU_SEGMENT_DEFAULT) ? defaultsegment : CPU[activeCPU].segment_register; //Use Data Segment (or different in case) for data!
}

int get_segment_index(word *location)
{
	if (location==CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_CS])
	{
		return CPU_SEGMENT_CS;
	}
	else if (location==CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_DS])
	{
		return CPU_SEGMENT_DS;
	}
	else if (location==CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_ES])
	{
		return CPU_SEGMENT_ES;
	}
	else if (location==CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_SS])
	{
		return CPU_SEGMENT_SS;
	}
	else if (location==CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_FS])
	{
		return CPU_SEGMENT_FS;
	}
	else if (location==CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_GS])
	{
		return CPU_SEGMENT_GS;
	}
	else if (location == CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_TR])
	{
		return CPU_SEGMENT_TR;
	}
	return -1; //Unknown segment!
}

//getTYPE: gets the loaded descriptor type: 0=Code, 1=Exec, 2=System.
int getLoadedTYPE(SEGMENT_DESCRIPTOR *loadeddescriptor)
{
	return GENERALSEGMENTPTR_S(loadeddescriptor)?EXECSEGMENTPTR_ISEXEC(loadeddescriptor):2; //Executable or data, else System?
}

int isGateDescriptor(SEGMENT_DESCRIPTOR *loadeddescriptor) //0=Fault, 1=Gate, -1=System Segment descriptor, 2=Normal segment descriptor.
{
	if (getLoadedTYPE(loadeddescriptor)==2) //System?
	{
		switch (GENERALSEGMENTPTR_TYPE(loadeddescriptor))
		{
		case AVL_SYSTEM_RESERVED_0: //NULL descriptor?
			return 0; //NULL descriptor!
		//32-bit only stuff?
		case AVL_SYSTEM_BUSY_TSS32BIT: //TSS?
		case AVL_SYSTEM_TSS32BIT: //TSS?
			if (EMULATED_CPU<=CPU_80286) return 0; //Invalid descriptor on 286-!
		//16-bit stuff? Always supported
		case AVL_SYSTEM_BUSY_TSS16BIT: //TSS?
		case AVL_SYSTEM_TSS16BIT: //TSS?
		case AVL_SYSTEM_LDT: //LDT?
			return -1; //System segment descriptor!
		case AVL_SYSTEM_CALLGATE32BIT:
		case AVL_SYSTEM_INTERRUPTGATE32BIT:
		case AVL_SYSTEM_TRAPGATE32BIT: //Any type of gate?
			if (EMULATED_CPU<=CPU_80286) return 0; //Invalid descriptor on 286-!
		case AVL_SYSTEM_TASKGATE: //Task gate?
		case AVL_SYSTEM_CALLGATE16BIT:
		case AVL_SYSTEM_INTERRUPTGATE16BIT:
		case AVL_SYSTEM_TRAPGATE16BIT:
			return 1; //We're a gate!
		default: //Unknown type?
			break;
		}
	}
	return 2; //Not a gate descriptor, always valid!
}

void THROWDESCGP(word segmentval, byte external, byte tbl)
{
	CPU_GP((external&1)|(segmentval&(0xFFF8))|((tbl&0x3)<<1)); //#GP with an error in the LDT/GDT (index@bits 3-15)!
}

void THROWDESCSS(word segmentval, byte external, byte tbl)
{
	CPU_StackFault((external&1)|(segmentval&(0xFFF8))|((tbl&0x3)<<1)); //#StackFault with an error in the LDT/GDT (index@bits 3-15)!
}

void THROWDESCNP(word segmentval, byte external, byte tbl)
{
	CPU_SegNotPresent((external&1)|(segmentval&(0xFFF8))|((tbl&0x3)<<1)); //#SegFault with an error in the LDT/GDT (index@bits 3-15)!
}

void THROWDESCTS(word segmentval, byte external, byte tbl)
{
	CPU_TSSFault((segmentval&(0xFFF8)),(external&1),(tbl&0x3)); //#SegFault with an error in the LDT/GDT (index@bits 3-15)!
}

//Another source: http://en.wikipedia.org/wiki/General_protection_fault

extern byte debugger_forceimmediatelogging; //Force immediate logging?

//Virtual memory support wrapper for memory accesses!
byte memory_readlinear(uint_32 address, byte *result)
{
	debugger_forceimmediatelogging = 1; //Log!
	*result = Paging_directrb(-1,address,0,0,0,0); //Read the address!
	debugger_forceimmediatelogging = 0; //Don't log anymore!
	return 0; //No error!
}

byte memory_writelinear(uint_32 address, byte value)
{
	debugger_forceimmediatelogging = 1; //Log!
	Paging_directwb(-1,address,value,0,0,0,0); //Write the address!
	debugger_forceimmediatelogging = 0; //Don't log!
	return 0; //No error!
}

typedef struct
{
	byte mask;
	byte nonequals;
	byte comparision;
} checkrights_cond;

checkrights_cond checkrights_conditions[0x10] = {
	{ 0x13&~0x10,0,0 }, //0 Data, read-only
	{ 0x13&~0x10,0,0 }, //1 unused
	{ 0x13&0,1,0 }, //2 Data, read/write! Allow always!
	{ 0x13&0,1,0 }, //3 unused
	{ 0x13&~0x10,0,0 }, //4 Data(expand down), read-only
	{ 0x13&~0x10,0,0 }, //5 unused
	{ 0x13&0,1,0 }, //6 Data(expand down), read/write! Allow always!
	{ 0x13&0,1,0 }, //7 unused
	{ 0x13&~0x10,1,3 }, //8 Code, non-conforming, execute-only
	{ 0x13&~0x10,1,3 }, //9 unused
	{ 0x13&~0x10,0,0 }, //10 Code, non-conforming, execute/read
	{ 0x13&~0x10,0,0 }, //11 unused
	{ 0x13&~0x10,1,3 }, //12 Code, conforming, execute-only
	{ 0x13&~0x10,1,3 }, //13 unused
	{ 0x13&~0x10,0,0 }, //14 Code, conforming, execute/read
	{ 0x13&~0x10,0,0 } //15 unused
};

byte checkrights_conditions_rwe_errorout[0x10][0x100]; //All precalculated conditions that are possible!

void CPU_calcSegmentPrecalcsPrecalcs()
{
	byte x;
	word n;
	checkrights_cond *rights;
	for (x = 0; x < 0x10; ++x) //All possible conditions!
	{
		rights = &checkrights_conditions[x]; //What type do we check for(take it all, except the dirty bit)!
		for (n = 0; n < 0x100; ++n) //Calculate all conditions that error out or not!
		{
			checkrights_conditions_rwe_errorout[x][n] = (((((n&rights->mask) == rights->comparision) == (rights->nonequals == 0))) & 1); //Are we to error out on this condition?
		}
	}
}

void CPU_calcSegmentPrecalcs(SEGMENT_DESCRIPTOR *descriptor)
{
	//Calculate the precalculations for execution for this descriptor!
	uint_32 limits[2]; //What limit to apply?

	limits[0] = ((SEGDESCPTR_NONCALLGATE_LIMIT_HIGH(descriptor) << 16) | descriptor->desc.limit_low); //Base limit!
	limits[1] = ((limits[0] << 12) | 0xFFF); //4KB for a limit of 4GB, fill lower 12 bits with 1!
	descriptor->PRECALCS.limit = (uint_64)limits[SEGDESCPTR_GRANULARITY(descriptor)]; //Use the appropriate granularity to produce the limit!
	descriptor->PRECALCS.topdown = ((descriptor->desc.AccessRights & 0x1C) == 0x14); //Topdown segment?
	descriptor->PRECALCS.notpresent = (GENERALSEGMENTPTR_P(descriptor)==0); //Not present descriptor?
	//Roof: Expand-up: G=0: 1MB, G=1: 4GB. Expand-down: B=0:64K, B=1:4GB.
	descriptor->PRECALCS.roof = (((uint_64)0xFFFF | ((uint_64)0xFFFF << ((descriptor->PRECALCS.topdown?SEGDESCPTR_NONCALLGATE_D_B(descriptor):SEGDESCPTR_GRANULARITY(descriptor)) << 4)))&0xFFFFFFFF); //The roof of the descriptor!
	if ((descriptor->PRECALCS.topdown==0) && (SEGDESCPTR_GRANULARITY(descriptor)==0)) //Bottom-up segment that's having a 20-bit limit?
	{
		descriptor->PRECALCS.roof |= 0xF0000; //Actually a 1MB limit instead of 64K!
	}
	descriptor->PRECALCS.base = (((descriptor->desc.base_high << 24) | (descriptor->desc.base_mid << 16) | descriptor->desc.base_low)&0xFFFFFFFF); //Update the base address!
	if (descriptor->PRECALCS.topdown && (SEGDESCPTR_GRANULARITY(descriptor)==0)) //Top-down descriptor?
	{
		descriptor->PRECALCS.base += descriptor->PRECALCS.limit; //Add the limit field!
		descriptor->PRECALCS.base -= 0x10000; //Substract the modulus!
		descriptor->PRECALCS.base &= 0xFFFFFFFF; //Mask to 32-bit size!
	}
	//Apply read/write/execute permissions to the descriptor!
	memcpy(&descriptor->PRECALCS.rwe_errorout[0], &checkrights_conditions_rwe_errorout[descriptor->desc.AccessRights & 0xE][0],sizeof(descriptor->PRECALCS.rwe_errorout));
}

sbyte LOADDESCRIPTOR(int segment, word segmentval, SEGMENT_DESCRIPTOR *container, word isJMPorCALL) //Result: 0=#GP, 1=container=descriptor.
{
	uint_32 descriptor_address = 0;
	descriptor_address = (segmentval & 4) ? CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].PRECALCS.base : CPU[activeCPU].registers->GDTR.base; //LDT/GDT selector!

	uint_32 descriptor_index=segmentval; //The full index within the descriptor table!
	descriptor_index &= ~0x7; //Clear bits 0-2 for our base index into the table!

	byte isNULLdescriptor = 0;
	isNULLdescriptor = 0; //Default: not a NULL descriptor!
	if ((segmentval&~3)==0) //NULL descriptor?
	{
		isNULLdescriptor = 1; //NULL descriptor!
		//Otherwise, don't load the descriptor from memory, just clear valid bit!
	}

	if ((segmentval&4) && (GENERALSEGMENT_P(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR])==0)) //Invalid LDT segment?
	{
		return 0; //Abort: invalid LDTR to use!
	}
	if (isNULLdescriptor == 0) //Not NULL descriptor?
	{
		if ((word)(descriptor_index | 0x7) > ((segmentval & 4) ? CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].PRECALCS.limit : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
		{
			return 0; //Not present: limit exceeded!
		}
	}
	
	descriptor_address += descriptor_index; //Add the index multiplied with the width(8 bytes) to get the descriptor!

	if (isNULLdescriptor==0) //Not special NULL descriptor handling?
	{
		int i;
		for (i=0;i<(int)sizeof(container->desc.bytes);++i)
		{
			if (checkDirectMMUaccess(descriptor_address++,1,/*getCPL()*/ 0)) //Error in the paging unit?
			{
				return -1; //Error out!
			}
		}
		descriptor_address -= sizeof(container->desc.bytes); //Restore start address!
		for (i=0;i<(int)sizeof(container->desc.bytes);) //Process the descriptor data!
		{
			if (memory_readlinear(descriptor_address++,&container->desc.bytes[i++])) //Read a descriptor byte directly from flat memory!
			{
				return 0; //Failed to load the descriptor!
			}
		}

		container->desc.limit_low = DESC_16BITS(container->desc.limit_low);
		container->desc.base_low = DESC_16BITS(container->desc.base_low);

		if (EMULATED_CPU == CPU_80286) //80286 has less options?
		{
			container->desc.base_high = 0; //No high byte is present!
			container->desc.noncallgate_info &= ~0xF; //No high limit is present!
		}
	}
	else //NULL descriptor to DS/ES/FS/GS segment registers? Don't load the descriptor from memory(instead clear it's present bit)! Any other register, just clear it's descriptor for a full NULL descriptor!
	{
		if ((segment == CPU_SEGMENT_DS) || (segment == CPU_SEGMENT_ES) || (segment == CPU_SEGMENT_FS) || (segment == CPU_SEGMENT_GS) || (segment==CPU_SEGMENT_TR))
		{
			memcpy(container,&CPU[activeCPU].SEG_DESCRIPTOR[segment],sizeof(*container)); //Copy the old value!
			container->desc.AccessRights &= 0x7F; //Clear the present flag in the descriptor itself!
		}
		else
		{
			memset(container, 0, sizeof(*container)); //Load an invalid register, which is marked invalid!
		}
	}

	CPU_calcSegmentPrecalcs(container); //Precalculate anything needed!
	return 1; //OK!
}

//Result: 1=OK, 0=Error!
sbyte SAVEDESCRIPTOR(int segment, word segmentval, SEGMENT_DESCRIPTOR *container, word isJMPorCALL)
{
	uint_32 descriptor_address = 0;
	descriptor_address = (segmentval & 4) ? CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].PRECALCS.base : CPU[activeCPU].registers->GDTR.base; //LDT/GDT selector!
	uint_32 descriptor_index = segmentval; //The full index within the descriptor table!
	descriptor_index &= ~0x7; //Clear bits 0-2 for our base index into the table!

	if ((segmentval&~3) == 0)
	{
		return 0; //Don't write the reserved NULL GDT entry, which isn't to be used!
	}

	if ((segmentval&4) && (GENERALSEGMENT_P(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR])==0)) //Invalid LDT segment?
	{
		return 0; //Abort!
	}

	if ((word)(descriptor_index | 0x7) > ((segmentval & 4) ? CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].PRECALCS.limit : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
	{
		return 0; //Not present: limit exceeded!
	}

	if ((!getDescriptorIndex(descriptor_index)) && ((segment == CPU_SEGMENT_CS) || ((segment == CPU_SEGMENT_SS))) && ((segmentval&4)==0)) //NULL GDT segment loaded into CS or SS?
	{
		return 0; //Not present: limit exceeded!	
	}

	descriptor_address += descriptor_index; //Add the index multiplied with the width(8 bytes) to get the descriptor!

	/*
	SEGMENT_DESCRIPTOR tempcontainer;
	if (EMULATED_CPU == CPU_80286) //80286 has less options?
	{
		if (LOADDESCRIPTOR(segment,segmentval,&tempcontainer,(isJMPorCALL&0x2FF)|0x100)) //Loaded the old container?
		{
			container->desc.base_high = tempcontainer.desc.base_high; //No high byte is present, so ignore the data to write!
			container->desc.noncallgate_info = ((container->desc.noncallgate_info&~0xF)|(tempcontainer.desc.noncallgate_info&0xF)); //No high limit is present, so ignore the data to write!
		}
		//Don't handle any errors on descriptor loading!
	}

	//Patch back to memory values!
	container->desc.limit_low = DESC_16BITS(container->desc.limit_low);
	container->desc.base_low = DESC_16BITS(container->desc.base_low);
	*/
	int i;
	/*
	for (i = 0;i<(int)sizeof(container->desc.bytes);++i) //Process the descriptor data!
	{
	*/
	descriptor_address += 5; //Only the access rights byte!
		if (checkDirectMMUaccess(descriptor_address++,0,/*getCPL()*/ 0)) //Error in the paging unit?
		{
			return -1; //Error out!
		}
	//}
	//descriptor_address -= sizeof(container->desc.bytes);
	descriptor_address -= 6; //Only the access rights byte!

	/*
	for (i = 0;i<(int)sizeof(container->desc.bytes);) //Process the descriptor data!
	{
	*/
	i = 5; //Only the access rights byte!
	descriptor_address += 5; //Only the access rights byte!
		if (memory_writelinear(descriptor_address++,container->desc.bytes[i++])) //Read a descriptor byte directly from flat memory!
		{
			return 0; //Failed to load the descriptor!
		}
	//}
	return 1; //OK!
}


uint_32 destEIP; //Destination address for CS JMP instruction!

byte CPU_handleInterruptGate(byte EXT, byte table, uint_32 descriptorbase, RAWSEGMENTDESCRIPTOR *theidtentry, word returnsegment, uint_32 returnoffset, int_64 errorcode, byte is_interrupt); //Execute a protected mode interrupt!

/*

getsegment_seg: Gets a segment, if allowed.
parameters:
	whatsegment: What segment is used?
	segment: The segment to get.
	isJMPorCALL: 0 for normal segment setting. 1 for JMP, 2 for CALL, 3 for IRET. bit7=Disable privilege level checking, bit8=Disable SAVEDESCRIPTOR writeback, bit9=task switch, bit10=Set EXT bit on faulting, bit 11=TSS Busy requirement(1=Busy, 0=Non-busy), bit 12=bit 13-14 are the CPL instead for privilege checks.
result:
	The segment when available, NULL on error or disallow.

*/

#define effectiveCPL() ((isJMPorCALL&0x1000)?((isJMPorCALL>>13)&3):getCPL())

sbyte touchSegment(int segment, word segmentval, SEGMENT_DESCRIPTOR *container, word isJMPorCALL)
{
	sbyte saveresult;
	if(GENERALSEGMENTPTR_P(container) && (getLoadedTYPE(container) != 2) && (CODEDATASEGMENTPTR_A(container) == 0) && ((isJMPorCALL&0x100)==0)) //Non-accessed loaded and needs to be set? Our reserved bit 8 in isJMPorCALL tells us not to cause writeback for accessed!
	{
		container->desc.AccessRights |= 1; //Set the accessed bit!
		if ((saveresult = SAVEDESCRIPTOR(segment, segmentval, container, isJMPorCALL))<=0) //Trigger writeback and errored out?
		{
			return saveresult;
		}
	}
	return 1; //Success!
}

SEGMENT_DESCRIPTOR *getsegment_seg(int segment, SEGMENT_DESCRIPTOR *dest, word *segmentval, word isJMPorCALL, byte *isdifferentCPL) //Get this corresponding segment descriptor (or LDT. For LDT, specify LDT register as segment) for loading into the segment descriptor cache!
{
	//byte newCPL = getCPL(); //New CPL after switching! Default: no change!
	SEGMENT_DESCRIPTOR LOADEDDESCRIPTOR, GATEDESCRIPTOR; //The descriptor holder/converter!
	memset(&LOADEDDESCRIPTOR, 0, sizeof(LOADEDDESCRIPTOR)); //Init!
	memset(&GATEDESCRIPTOR, 0, sizeof(GATEDESCRIPTOR)); //Init!
	word originalval=*segmentval; //Back-up of the original segment value!
	byte allowNP; //Allow #NP to be used?
	sbyte loadresult;
	byte privilegedone = 0; //Privilege already calculated?
	byte is_gated = 0; //Are we gated?
	byte is_TSS = 0; //Are we a TSS?
	byte callgatetype = 0; //Default: no call gate!

	if ((*segmentval&4) && (((GENERALSEGMENT_P(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR])==0) && (segment!=CPU_SEGMENT_LDTR)) || (segment==CPU_SEGMENT_LDTR))) //Invalid LDT segment and LDT is addressed or LDTR in LDT?
	{
		throwdescsegmentval:
		if (isJMPorCALL&0x200) //TSS is the cause?
		{
			THROWDESCTS(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!			
		}
		else //Plain #GP?
		{
			THROWDESCGP(*segmentval,((isJMPorCALL&0x400)>>10),(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		}
		return NULL; //We're an invalid TSS to execute!
	}

	if ((loadresult = LOADDESCRIPTOR(segment,*segmentval,&LOADEDDESCRIPTOR,isJMPorCALL))<=0) //Error loading current descriptor?
	{
		if (loadresult == 0) //Not already faulted?
		{
			goto throwdescsegmentval;
		}
		return NULL; //Error, by specified reason!
	}
	allowNP = ((segment==CPU_SEGMENT_DS) || (segment==CPU_SEGMENT_ES) || (segment==CPU_SEGMENT_FS) || (segment==CPU_SEGMENT_GS)); //Allow segment to be marked non-present(exception: values 0-3 with data segments)?

	if (((*segmentval&~3)==0)) //NULL GDT segment when not allowed?
	{
		if (segment==CPU_SEGMENT_LDTR) //in LDTR? We're valid!
		{
			goto validLDTR; //Skip all checks, and check out as valid! We're allowed on the LDTR only!
		}
		else //Skip checks: we're invalid to check any further!
		{
			if ((segment==CPU_SEGMENT_CS) || (segment==CPU_SEGMENT_TR) || (segment==CPU_SEGMENT_SS)) //Not allowed?
			{
				goto throwdescsegmentval; //Throw #GP error!
				return NULL; //Error, by specified reason!
			}
			else if (allowNP)
			{
				goto validLDTR; //Load NULL descriptor!
			}
		}
	}

	if (isGateDescriptor(&LOADEDDESCRIPTOR)==0) //Invalid descriptor?
	{
		goto throwdescsegmentval; //Throw #GP error!
		return NULL; //We're an invalid descriptor to use!
	}

	if ((isGateDescriptor(&LOADEDDESCRIPTOR)==1) && (segment == CPU_SEGMENT_CS) && (isJMPorCALL&0x1FF) && ((isJMPorCALL&0x200)==0)) //Handling of gate descriptors? Disable on task code/data segment loading!
	{
		is_gated = 1; //We're gated!
		memcpy(&GATEDESCRIPTOR, &LOADEDDESCRIPTOR, sizeof(GATEDESCRIPTOR)); //Copy the loaded descriptor to the GATE!
		//Check for invalid loads!
		switch (GENERALSEGMENT_TYPE(GATEDESCRIPTOR))
		{
		default: //Unknown type?
		case AVL_SYSTEM_INTERRUPTGATE16BIT:
		case AVL_SYSTEM_TRAPGATE16BIT:
		case AVL_SYSTEM_INTERRUPTGATE32BIT:
		case AVL_SYSTEM_TRAPGATE32BIT:
			/*
			if ((isJMPorCALL & 0x1FF) == 2) //CALL? It's an programmed interrupt call!
			{
				CPU_handleInterruptGate(((isJMPorCALL&0x400)>>10),(*segmentval & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT, (*segmentval & 0xFFF8), &LOADEDDESCRIPTOR.desc, REG_CS, REG_EIP, -2, 1); //Raise an interrupt instead!
				return NULL; //Abort: we're handled by the interrupt handler!
			}
			*/ //80386 user manual CALL instruction reference says that interrupt and other gates being loaded end up with a General Protection fault.
			//JMP isn't valid for interrupt gates?
			//We're an invalid gate!
			goto throwdescsegmentval; //Throw #GP error!		
			return NULL; //Not present: invalid descriptor type loaded!
			break;
		case AVL_SYSTEM_TASKGATE: //Task gate?
		case AVL_SYSTEM_CALLGATE16BIT:
		case AVL_SYSTEM_CALLGATE32BIT:
			//Valid gate! Allow!
			break;
		}
		if ((MAX(getCPL(), getRPL(*segmentval)) > GENERALSEGMENT_DPL(GATEDESCRIPTOR)) && ((isJMPorCALL&0x1FF)!=3)) //Gate has too high a privilege level? Only when not an IRET(always allowed)!
		{
			goto throwdescsegmentval; //Throw error!
			return NULL; //We are a lower privilege level, so don't load!				
		}
		if (GENERALSEGMENT_P(GATEDESCRIPTOR)==0) //Not present loaded into non-data segment register?
		{
			if (segment==CPU_SEGMENT_SS) //Stack fault?
			{
				THROWDESCSS(*segmentval,(isJMPorCALL&0x200)?1:(((isJMPorCALL&0x400)>>10)),(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Stack fault!
			}
			else
			{
				THROWDESCNP(*segmentval, (isJMPorCALL&0x200)?1:(((isJMPorCALL&0x400)>>10)),(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
			}
			return NULL; //We're an invalid TSS to execute!
		}

		*segmentval = GATEDESCRIPTOR.desc.selector; //We're loading this segment now, with requesting privilege!

		if (((*segmentval&~3)==0)) //NULL GDT segment when not allowed?
		{
			goto throwdescsegmentval; //Throw #GP(0) error!
			return NULL; //Abort!
		}

		if ((loadresult = LOADDESCRIPTOR(segment, *segmentval, &LOADEDDESCRIPTOR,isJMPorCALL))<=0) //Error loading current descriptor?
		{
			if (loadresult == 0) //Not faulted already?
			{
				goto throwdescsegmentval; //Throw error!
			}
			return NULL; //Error, by specified reason!
		}
		privilegedone = 1; //Privilege has been precalculated!
		if (GENERALSEGMENT_TYPE(GATEDESCRIPTOR) == AVL_SYSTEM_TASKGATE) //Task gate?
		{
			if (segment != CPU_SEGMENT_CS) //Not code? We're not a task switch! We're trying to load the task segment into a data register. This is illegal! TR doesn't support Task Gates directly(hardware only)!
			{
				goto throwdescsegmentval; //Throw error!
				return NULL; //Don't load!
			}
		}
		else //Normal descriptor?
		{
			if (GENERALSEGMENT_S(LOADEDDESCRIPTOR)==0) goto throwdescsegmentval;
			if (((isJMPorCALL&0x1FF) == 1) && (!EXECSEGMENT_C(LOADEDDESCRIPTOR))) //JMP to a nonconforming segment?
			{
				if (GENERALSEGMENT_DPL(LOADEDDESCRIPTOR) != getCPL()) //Different CPL?
				{
					goto throwdescsegmentval; //Throw error!
					return NULL; //We are a different privilege level, so don't load!						
				}
			}
			else if (isJMPorCALL&0x1FF) //Call instruction (or JMP instruction to a conforming segment)
			{
				if (GENERALSEGMENT_DPL(LOADEDDESCRIPTOR) > getCPL()) //We have a lower CPL?
				{
					goto throwdescsegmentval; //Throw error!
					return NULL; //We are a different privilege level, so don't load!
				}
			}
		}
	}

	//Final descriptor safety check!
	if (isGateDescriptor(&LOADEDDESCRIPTOR)==0) //Invalid descriptor?
	{
		goto throwdescsegmentval; //Throw #GP error!
		return NULL; //We're an invalid descriptor to use!
	}

	if (
		(
		(segment==CPU_SEGMENT_SS) ||
		(segment==CPU_SEGMENT_DS) ||
		(segment==CPU_SEGMENT_ES) ||
		(segment==CPU_SEGMENT_FS) ||
		(segment==CPU_SEGMENT_GS) //SS,DS,ES,FS,GS are ...
		) &&
		(
		(getLoadedTYPE(&LOADEDDESCRIPTOR)==2) || //A System segment? OR ...
		((getLoadedTYPE(&LOADEDDESCRIPTOR)==1) && (EXECSEGMENT_R(LOADEDDESCRIPTOR)==0)) //An execute-only code segment?
		)
		)
	{
		throwdescoriginalval:
		if (isJMPorCALL&0x200) //TSS is the cause?
		{
			THROWDESCTS(originalval,1,(originalval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		}
		else //Plain #GP?
		{
			THROWDESCGP(originalval,((isJMPorCALL&0x400)>>10),(originalval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		}
		return NULL; //Not present: limit exceeded!	
	}
	
	switch (GENERALSEGMENT_TYPE(LOADEDDESCRIPTOR)) //We're a TSS? We're to perform a task switch!
	{
	case AVL_SYSTEM_BUSY_TSS16BIT:
	case AVL_SYSTEM_TSS16BIT: //TSS?
	case AVL_SYSTEM_BUSY_TSS32BIT:
	case AVL_SYSTEM_TSS32BIT: //TSS?
		is_TSS = (getLoadedTYPE(&LOADEDDESCRIPTOR)==2); //We're a TSS when a system segment!
		break;
	default:
		is_TSS = 0; //We're no TSS!
		break;
	}

	if (is_TSS && (segment==CPU_SEGMENT_CS) && ((isJMPorCALL&0x1FF)==3)) //IRET allowed regardless of privilege?
	{
		privilegedone = 1; //Allow us always!
	}

	//Now check for CPL,DPL&RPL! (chapter 6.3.2)
	if (
		(
		(!privilegedone && (getRPL(*segmentval)<getCPL()) && (((isJMPorCALL&0x1FF)==4)||(isJMPorCALL&0x1FF)==3)) || //IRET/RETF to higher privilege level?
		(!privilegedone && (MAX(getCPL(),getRPL(*segmentval))>GENERALSEGMENT_DPL(LOADEDDESCRIPTOR)) && ((getLoadedTYPE(&LOADEDDESCRIPTOR)!=1) && (segment!=CPU_SEGMENT_SS)) && ((isJMPorCALL&0x1FF)!=4) && ((isJMPorCALL&0x1FF)!=3)) || //We are a lower privilege level with either a data/system segment descriptor? Non-conforming code segments have different check:
		(!privilegedone && (MAX((((isJMPorCALL&0x1FF)==4)||((isJMPorCALL&0x1FF)==3))?getRPL(*segmentval):getCPL(),getRPL(*segmentval))<GENERALSEGMENT_DPL(LOADEDDESCRIPTOR)) && (EXECSEGMENT_ISEXEC(LOADEDDESCRIPTOR) && (EXECSEGMENT_C(LOADEDDESCRIPTOR)) && (getLoadedTYPE(&LOADEDDESCRIPTOR) == 1))) || //We must be at the same privilege level or higher compared to MAX(CPL,RPL) (or just RPL for RETF) for conforming code segment descriptors?
		(!privilegedone && (MAX((((isJMPorCALL&0x1FF)==4)||((isJMPorCALL&0x1FF)==3))?getRPL(*segmentval):getCPL(),getRPL(*segmentval))!=GENERALSEGMENT_DPL(LOADEDDESCRIPTOR)) && (EXECSEGMENT_ISEXEC(LOADEDDESCRIPTOR) && (!EXECSEGMENT_C(LOADEDDESCRIPTOR)) && (getLoadedTYPE(&LOADEDDESCRIPTOR) == 1))) || //We must be at the same privilege level compared to MAX(CPL,RPL) (or just RPL for RETF) for non-conforming code segment descriptors?
		(!privilegedone && ((effectiveCPL()!=getRPL(*segmentval)) || (effectiveCPL()!=GENERALSEGMENT_DPL(LOADEDDESCRIPTOR))) && (segment==CPU_SEGMENT_SS)) //SS DPL must match CPL and RPL!
		)
		&& (!(((isJMPorCALL&0x1FF)==3) && is_TSS)) //No privilege checking is done on IRET through TSS!
		&& (!((isJMPorCALL&0x80)==0x80)) //Don't ignore privilege?
		)
	{
		goto throwdescoriginalval; //Throw error!
		return NULL; //We are a lower privilege level, so don't load!
	}

	if (is_TSS && (segment==CPU_SEGMENT_TR)) //We're a TSS loading into TR? We're to perform a task switch!
	{
		if (*segmentval & 4) //LDT lookup set?
		{
			goto throwdescoriginalval; //Throw error!
			return NULL; //We're an invalid TSS to call!
		}
		//Handle the task switch normally! We're allowed to use the TSS!
	}

	if ((segment==CPU_SEGMENT_CS) && is_TSS && ((isJMPorCALL&0x200)==0)) //Special stuff on CS, CPL, Task switch.
	{
		//Present is handled by the task switch mechanism, so don't check it here!

		//Execute a normal task switch!
		if (CPU_executionphase_starttaskswitch(segment,&LOADEDDESCRIPTOR,segmentval,*segmentval,isJMPorCALL,is_gated,-1)) //Switching to a certain task?
		{
			return NULL; //Error changing priviledges or anything else!
		}

		//We've properly switched to the destination task! Continue execution normally!
		return NULL; //Don't actually load CS with the descriptor: we've caused a task switch after all!
	}

	if ((segment == CPU_SEGMENT_CS) && (is_gated == 0) && (((isJMPorCALL & 0x1FF) == 2)||((isJMPorCALL & 0x1FF) == 1))) //CALL/JMP to lower or different privilege?
	{
		if ((GENERALSEGMENT_DPL(LOADEDDESCRIPTOR) > getCPL()) && EXECSEGMENT_C(LOADEDDESCRIPTOR)) //Conforming and lower privilege?
		{
			goto throwdescoriginalval; //Throw #GP error!		
		}
		if (((getRPL(*segmentval) > getCPL()) || (GENERALSEGMENT_DPL(LOADEDDESCRIPTOR) != getCPL())) && !EXECSEGMENT_C(LOADEDDESCRIPTOR)) //Non-conforming and different privilege or lowering privilege?
		{
			goto throwdescoriginalval; //Throw #GP error!		
		}
		//Non-conforming always must match CPL, so we don't handle it here(it's in the generic check)!
	}

	//Handle invalid types to load now!
	if ((segment==CPU_SEGMENT_CS) &&
		(getLoadedTYPE(&LOADEDDESCRIPTOR)!=1) //Data or System in CS (non-exec)?
		)
	{
		goto throwdescsegmentval; //Throw #GP error!		
		return NULL; //Not present: invalid descriptor type loaded!
	}
	else if ((getLoadedTYPE(&LOADEDDESCRIPTOR)==1) && (segment!=CPU_SEGMENT_CS) && (EXECSEGMENT_R(LOADEDDESCRIPTOR)==0)) //Executable non-readable in non-executable segment?
	{
		goto throwdescsegmentval; //Throw #GP error!		
		return NULL; //Not present: invalid descriptor type loaded!
	}
	else if ((getLoadedTYPE(&LOADEDDESCRIPTOR)==1) && ((segment==CPU_SEGMENT_LDTR) || (segment==CPU_SEGMENT_TR) || (segment==CPU_SEGMENT_SS))) //Executable segment loaded invalid?
	{
		goto throwdescsegmentval; //Throw #GP error!		
		return NULL; //Not present: invalid descriptor type loaded!
	}
	else if (getLoadedTYPE(&LOADEDDESCRIPTOR)==0) //Data descriptor loaded?
	{
		if (((segment!=CPU_SEGMENT_DS) && (segment!=CPU_SEGMENT_ES) && (segment!=CPU_SEGMENT_FS) && (segment!=CPU_SEGMENT_GS) && (segment!=CPU_SEGMENT_SS))) //Data descriptor in invalid type?
		{
			goto throwdescsegmentval; //Throw #GP error!		
			return NULL; //Not present: invalid descriptor type loaded!
		}
		if ((DATASEGMENT_W(LOADEDDESCRIPTOR)==0) && (segment==CPU_SEGMENT_SS)) //Non-writable SS segment?
		{
			goto throwdescsegmentval; //Throw #GP error!		
			return NULL; //Not present: invalid descriptor type loaded!
		}
	}
	else if (getLoadedTYPE(&LOADEDDESCRIPTOR)==2) //System descriptor loaded?
	{
		if ((segment==CPU_SEGMENT_CS) || (segment==CPU_SEGMENT_DS) || (segment==CPU_SEGMENT_ES) || (segment==CPU_SEGMENT_FS) || (segment==CPU_SEGMENT_GS) || (segment==CPU_SEGMENT_SS)) //System descriptor in invalid register?
		{
			goto throwdescsegmentval; //Throw #GP error!		
			return NULL; //Not present: invalid descriptor type loaded!
		}
		if ((segment==CPU_SEGMENT_LDTR) && (GENERALSEGMENT_TYPE(LOADEDDESCRIPTOR)!=AVL_SYSTEM_LDT)) //Invalid LDT load?
		{
			goto throwdescsegmentval; //Throw #GP error!		
			return NULL; //Not present: invalid descriptor type loaded!
		}
		if ((segment==CPU_SEGMENT_TR) && (is_TSS==0)) //Non-TSS into task register?
		{
			goto throwdescsegmentval; //Throw #GP error!		
			return NULL; //Not present: invalid descriptor type loaded!
		}
	}

	//Make sure we're present last!
	if ((GENERALSEGMENT_P(LOADEDDESCRIPTOR)==0) && ((segment==CPU_SEGMENT_CS) || (segment==CPU_SEGMENT_SS) || (segment==CPU_SEGMENT_TR) || (*segmentval&~3))) //Not present loaded into non-data register?
	{
		if (segment==CPU_SEGMENT_SS) //Stack fault?
		{
			THROWDESCSS(*segmentval,(isJMPorCALL&0x200)?1:((isJMPorCALL&0x400)>>10),(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		}
		else
		{
			THROWDESCNP(*segmentval,(isJMPorCALL&0x200)?1:((isJMPorCALL&0x400)>>10),(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		}
		return NULL; //We're an invalid TSS to execute!
	}


	if ((segment == CPU_SEGMENT_CS) && (isGateDescriptor(&GATEDESCRIPTOR) == 1) && (is_gated)) //Gated CS?
	{
		switch (GENERALSEGMENT_TYPE(GATEDESCRIPTOR)) //What type of gate are we using?
		{
		case AVL_SYSTEM_CALLGATE16BIT: //16-bit call gate?
			callgatetype = 1; //16-bit call gate!
			break;
		case AVL_SYSTEM_CALLGATE32BIT: //32-bit call gate?
			callgatetype = 2; //32-bit call gate!
			break;
		default:
			callgatetype = 0; //No call gate!
			break;
		}
		if (callgatetype) //To process a call gate's parameters and offsets?
		{
			destEIP = (uint_32)GATEDESCRIPTOR.desc.callgate_base_low; //16-bit EIP!
			if (callgatetype == 2) //32-bit destination?
			{
				destEIP |= (((uint_32)GATEDESCRIPTOR.desc.callgate_base_mid)<<16); //Mid EIP!
				destEIP |= (((uint_32)GATEDESCRIPTOR.desc.callgate_base_high)<<24); //High EIP!
			}
			uint_32 argument; //Current argument to copy to the destination stack!
			word arguments;
			CPU[activeCPU].CallGateParamCount = 0; //Clear our stack to transfer!
			CPU[activeCPU].CallGateSize = (callgatetype==2)?1:0; //32-bit vs 16-bit call gate!

			if ((GENERALSEGMENT_DPL(LOADEDDESCRIPTOR)<getCPL()) && (EXECSEGMENT_C(LOADEDDESCRIPTOR)==0) && ((isJMPorCALL&0x1FF)==2)) //Stack switch required (with CALL only)?
			{
				//Now, copy the stack arguments!

				*isdifferentCPL = 1; //We're a different level!
				arguments = CALLGATE_NUMARGUMENTS =  (GATEDESCRIPTOR.desc.ParamCnt&0x1F); //Amount of parameters!
				CPU[activeCPU].CallGateParamCount = 0; //Initialize the amount of arguments that we're storing!
				if (checkStackAccess(arguments,0,(callgatetype==2)?1:0)) return NULL; //Abort on stack fault!
				for (;arguments--;) //Copy as many arguments as needed!
				{
					if (callgatetype==2) //32-bit source?
					{
						argument = MMU_rdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, CPU[activeCPU].registers->ESP&getstackaddrsizelimiter(), 0,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //POP 32-bit argument!
						if (STACK_SEGMENT_DESCRIPTOR_B_BIT()) //32-bits?
						{
							CPU[activeCPU].registers->ESP += 4; //Increase!
						}
						else //16-bits?
						{
							CPU[activeCPU].registers->SP += 4; //Increase!
						}
					}
					else //16-bit source?
					{
						argument = MMU_rw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), 0,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //POP 16-bit argument!
						if (STACK_SEGMENT_DESCRIPTOR_B_BIT()) //32-bits?
						{
							CPU[activeCPU].registers->ESP += 2; //Increase!
						}
						else //16-bits?
						{
							CPU[activeCPU].registers->SP += 2; //Increase!
						}
					}
					CPU[activeCPU].CallGateStack[CPU[activeCPU].CallGateParamCount++] = argument; //Add the argument to the call gate buffer to transfer to the new stack! Implement us as a LIFO for transfers!
				}
			}
			else
			{
				*isdifferentCPL = 2; //Indicate call gate determines operand size!
			}
		}
	}

	validLDTR:
	memcpy(dest,&LOADEDDESCRIPTOR,sizeof(LOADEDDESCRIPTOR)); //Give the loaded descriptor!

	return dest; //Give the segment descriptor read from memory!
}

word segmentWritten_tempSS;
uint_32 segmentWritten_tempESP;
word segmentWritten_tempSP;
extern word RETF_popbytes; //How many to pop?
byte is_stackswitching=0; //Are we busy stack switching?

byte RETF_checkSegmentRegisters[4] = {CPU_SEGMENT_ES,CPU_SEGMENT_FS,CPU_SEGMENT_GS,CPU_SEGMENT_DS}; //All segment registers to check for when returning to a lower privilege level!

word segmentWrittenVal, isJMPorCALLval;

byte segmentWritten(int segment, word value, word isJMPorCALL) //A segment register has been written to!
{
	byte RETF_segmentregister=0,RETF_whatsegment; //A segment register we're checking during a RETF instruction!
	byte oldCPL= getCPL();
	byte isDifferentCPL;
	byte isnonconformingcodeordata;
	sbyte loadresult;
	uint_32 tempesp;
	segmentWrittenVal = value; //What value is written!
	isJMPorCALLval = isJMPorCALL; //What type of write are we?
	if (getcpumode()==CPU_MODE_PROTECTED) //Protected mode, must not be real or V8086 mode, so update the segment descriptor cache!
	{
		isDifferentCPL = 0; //Default: same CPL!
		SEGMENT_DESCRIPTOR tempdescriptor;
		SEGMENT_DESCRIPTOR *descriptor = getsegment_seg(segment,&tempdescriptor,&value,isJMPorCALL,&isDifferentCPL); //Read the segment!
		uint_32 stackval;
		word stackval16; //16-bit stack value truncated!
		if (descriptor) //Loaded&valid?
		{
			if ((segment == CPU_SEGMENT_CS) && (((isJMPorCALL&0x1FF) == 2) || ((isJMPorCALL&0x1FF)==1))) //JMP(with call gate)/CALL needs pushed data on the stack?
			{
				if ((isDifferentCPL==1) && ((isJMPorCALL&0x1FF) == 2)) //Stack switch is required with CALL only?
				{
					//TSSSize = 0; //Default to 16-bit TSS!
					switch (GENERALSEGMENT_TYPE(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR])) //What kind of TSS?
					{
					case AVL_SYSTEM_BUSY_TSS32BIT:
					case AVL_SYSTEM_TSS32BIT:
						//TSSSize = 1; //32-bit TSS!
					case AVL_SYSTEM_BUSY_TSS16BIT:
					case AVL_SYSTEM_TSS16BIT:
						if (switchStacks(GENERALSEGMENTPTR_DPL(descriptor)|((isJMPorCALL&0x400)>>8))) return 1; //Abort failing switching stacks!
						
						if (checkStackAccess(2,1,CPU[activeCPU].CallGateSize)) return 1; //Abort on error!

						CPU_PUSH16(&CPU[activeCPU].oldSS,CPU[activeCPU].CallGateSize); //SS to return!

						if (CPU[activeCPU].CallGateSize)
						{
							CPU_PUSH32(&CPU[activeCPU].oldESP);
						}
						else
						{
							word temp=(word)(CPU[activeCPU].oldESP&0xFFFF);
							CPU_PUSH16(&temp,0);
						}
						
						//Now, we've switched to the destination stack! Load all parameters onto the new stack!
						if (checkStackAccess(CPU[activeCPU].CallGateParamCount,1,CPU[activeCPU].CallGateSize)) return 1; //Abort on error!
						for (;CPU[activeCPU].CallGateParamCount;) //Process the CALL Gate Stack!
						{
							stackval = CPU[activeCPU].CallGateStack[--CPU[activeCPU].CallGateParamCount]; //Read the next value to store!
							if (CPU[activeCPU].CallGateSize) //32-bit stack to push to?
							{
								CPU_PUSH32(&stackval); //Push the 32-bit stack value to the new stack!
							}
							else //16-bit?
							{
								stackval16 = (word)(stackval&0xFFFF); //Reduced data if needed!
								CPU_PUSH16(&stackval16,0); //Push the 16-bit stack value to the new stack!
							}
						}
						break;
					default:
						break;
					}
				}
				else if (isDifferentCPL==0) //Unchanging CPL? Take call size from operand size!
				{
					CPU[activeCPU].CallGateSize = CPU_Operand_size[activeCPU]; //Use the call instruction size!
				}
				//Else, call by call gate size!
				
				if ((isJMPorCALL&0x1FF)==2) //CALL pushes return address!
				{
					if (checkStackAccess(2,1,CPU[activeCPU].CallGateSize)) return 1; //Abort on error!

					//Push the old address to the new stack!
					if (CPU[activeCPU].CallGateSize) //32-bit?
					{
						CPU_PUSH16(&CPU[activeCPU].registers->CS,1);
						CPU_PUSH32(&CPU[activeCPU].registers->EIP);
					}
					else //16-bit?
					{
						CPU_PUSH16(&CPU[activeCPU].registers->CS,0);
						CPU_PUSH16(&CPU[activeCPU].registers->IP,0);
					}
				}

				if ((EXECSEGMENTPTR_C(descriptor)==0) && (isDifferentCPL==1)) //Non-Conforming segment, call gate and more privilege?
				{
					CPU[activeCPU].CPL = GENERALSEGMENTPTR_DPL(descriptor); //CPL = DPL!
				}
				setRPL(value,getCPL()); //RPL of CS always becomes CPL!

				if (isDifferentCPL==1) //Different CPL?
				{
					hascallinterrupttaken_type = CALLGATE_NUMARGUMENTS?CALLGATE_DIFFERENTLEVEL_XPARAMETERS:CALLGATE_DIFFERENTLEVEL_NOPARAMETERS; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task gate flag. Left at 0xFF when nothing is used(unknown case?)
				}
				else //Same CPL call gate?
				{
					hascallinterrupttaken_type = CALLGATE_SAMELEVEL; //Same level call gate!
				}
			}
			else if ((segment == CPU_SEGMENT_CS) && ((isJMPorCALL&0x1FF) == 4)) //RETF needs popped data on the stack?
			{
				if (is_stackswitching == 0) //We're ready to process?
				{
					if (STACK_SEGMENT_DESCRIPTOR_B_BIT())
					{
						REG_ESP += RETF_popbytes; //Process ESP!
					}
					else
					{
						REG_SP += RETF_popbytes; //Process SP!
					}
					if (oldCPL < getRPL(value)) //Lowering privilege?
					{
						if (checkStackAccess(2, 0, CPU_Operand_size[activeCPU])) return 1; //Stack fault?
					}
				}

				if (oldCPL<getRPL(value)) //CPL changed or still busy for this stage?
				{
					//Privilege change!
					CPU[activeCPU].CPL = getRPL(value); //New privilege level!

					//Now, return to the old prvilege level!
					hascallinterrupttaken_type = RET_DIFFERENTLEVEL; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task
					if (CPU_Operand_size[activeCPU])
					{
						if (CPU80386_internal_POPdw(6, &segmentWritten_tempESP))
						{
							CPU[activeCPU].CPL = oldCPL; //Restore CPL for continuing!
							is_stackswitching = 1; //We're stack switching!
							return 1; //POP ESP!
						}
					}
					else
					{
						if (CPU8086_internal_POPw(6, &segmentWritten_tempSP, 0))
						{
							CPU[activeCPU].CPL = oldCPL; //Restore CPL for continuing!
							is_stackswitching = 1; //We're stack switching!
							return 1; //POP SP!
						}
					}
					if (CPU8086_internal_POPw(8, &segmentWritten_tempSS, CPU_Operand_size[activeCPU]))
					{
						CPU[activeCPU].CPL = oldCPL; //Restore CPL for continuing!
						is_stackswitching = 1; //We're stack switching!
						return 1; //POPped?
					}
					is_stackswitching = 0; //We've finished stack switching!
					if (segmentWritten(CPU_SEGMENT_SS,segmentWritten_tempSS,0)) return 1; //Back to our calling stack!
					if (CPU_Operand_size[activeCPU])
					{
						REG_ESP = segmentWritten_tempESP; //POP ESP!
					}
					else
					{
						REG_ESP = (uint_32)segmentWritten_tempSP; //POP SP!
					}
					RETF_segmentregister = 1; //We're checking the segments for privilege changes to be invalidated!
				}
				else if (oldCPL > getRPL(value)) //CPL raised during RETF?
				{
					THROWDESCGP(value, (isJMPorCALL&0x200)?1:(((isJMPorCALL&0x400)>>10)), (value & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Raising CPL using RETF isn't allowed!
				}
				else //Same privilege? (E)SP on the destination stack is already processed, don't process again!
				{
					RETF_popbytes = 0; //Nothing to pop anymore!
				}
			}
			else if ((segment==CPU_SEGMENT_CS) && ((isJMPorCALL&0x1FF)==3)) //IRET might need extra data popped?
			{
				if (getRPL(value)>oldCPL) //Stack needs to be restored when returning to outer privilege level!
				{
					if (checkStackAccess(2,0,CPU_Operand_size[activeCPU])) return 1; //First level IRET data?
					if (CPU_Operand_size[activeCPU])
					{
						tempesp = CPU_POP32();
					}
					else
					{
						tempesp = CPU_POP16(CPU_Operand_size[activeCPU]);
					}

					CPU[activeCPU].CPL = getRPL(value); //New CPL!

					segmentWritten_tempSS = CPU_POP16(CPU_Operand_size[activeCPU]);
					if (segmentWritten(CPU_SEGMENT_SS,segmentWritten_tempSS,0)) return 1; //Back to our calling stack!
					REG_ESP = tempesp;

					RETF_segmentregister = 1; //We're checking the segments for privilege changes to be invalidated!
				}
				else if (oldCPL > getRPL(value)) //CPL raised during IRET?
				{
					THROWDESCGP(value, (isJMPorCALL&0x200)?1:((isJMPorCALL&0x400)>>10), (value & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Raising CPL using RETF isn't allowed!
					return 1; //Abort!
				}
			}

			if (segment==CPU_SEGMENT_TR) //Loading the Task Register? We're to mask us as busy!
			{
				if ((isJMPorCALL&0x1FF)==0) //Not a JMP or CALL itself, or a task switch, so just a plain load using LTR?
				{
					SEGMENT_DESCRIPTOR savedescriptor;
					switch (GENERALSEGMENT_TYPE(tempdescriptor)) //What kind of TSS?
					{
					case AVL_SYSTEM_BUSY_TSS32BIT:
					case AVL_SYSTEM_BUSY_TSS16BIT:
						if ((isJMPorCALL & 0x800) == 0) //Needs to be non-busy?
						{
							THROWDESCGP(value, (isJMPorCALL & 0x200) ? 1 : (((isJMPorCALL & 0x400) >> 10)), (value & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //We cannot load a busy TSS!
							return 1; //Abort on fault!
						}
						break;
					case AVL_SYSTEM_TSS32BIT:
					case AVL_SYSTEM_TSS16BIT:
						if ((isJMPorCALL & 0x800)) //Needs to be busy?
						{
							THROWDESCGP(value, (isJMPorCALL & 0x200) ? 1 : (((isJMPorCALL & 0x400) >> 10)), (value & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //We cannot load a busy TSS!
							return 1; //Abort on fault!
						}

						tempdescriptor.desc.AccessRights |= 2; //Mark not idle in the RAM descriptor!
						savedescriptor.desc.DATA64 = tempdescriptor.desc.DATA64; //Copy the resulting descriptor contents to our buffer for writing to RAM!
						if (SAVEDESCRIPTOR(segment,value,&savedescriptor,isJMPorCALL)<=0) //Save it back to RAM failed?
						{
							return 1; //Abort on fault!
						}
						break;
					default: //Invalid segment descriptor to load into the TR register?
						THROWDESCGP(value,(isJMPorCALL&0x200)?1:((isJMPorCALL&0x400)>>10),(value&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //We cannot load a busy TSS!
						return 1; //Abort on fault!
						break; //Ignore!
					}
				}
			}
			//Now, load the new descriptor and address for CS if needed(with secondary effects)!
			if ((CPU[activeCPU].have_oldSegReg&(1 << segment)) == 0) //Backup not loaded yet?
			{
				memcpy(&CPU[activeCPU].SEG_DESCRIPTORbackup[segment], &CPU[activeCPU].SEG_DESCRIPTOR[segment], sizeof(CPU[activeCPU].SEG_DESCRIPTORbackup[0])); //Restore the descriptor!
				CPU[activeCPU].oldSegReg[segment] = *CPU[activeCPU].SEGMENT_REGISTERS[segment]; //Backup the register too!
				CPU[activeCPU].have_oldSegReg |= (1 << segment); //Loaded!
			}
			memcpy(&CPU[activeCPU].SEG_DESCRIPTOR[segment],descriptor,sizeof(CPU[activeCPU].SEG_DESCRIPTOR[segment])); //Load the segment descriptor into the cache!
			//if (memprotect(CPU[activeCPU].SEGMENT_REGISTERS[segment],2,"CPU_REGISTERS")) //Valid segment register?
			{
				*CPU[activeCPU].SEGMENT_REGISTERS[segment] = value; //Set the segment register to the allowed value!
			}

			if ((loadresult = touchSegment(segment,value,descriptor,isJMPorCALL))<=0) //Errored out during touching?
			{
				if (loadresult == 0) //Not already faulted?
				{
					if (isJMPorCALL&0x200) //TSS is the cause?
					{
						THROWDESCTS(value,1,(value&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!			
					}
					else //Plain #GP?
					{
						THROWDESCGP(value,((isJMPorCALL&0x400)>>10),(value&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
					}
				}
				return 1; //Abort on fault!
			}

			if (segment == CPU_SEGMENT_CS) //CS register?
			{
				CPU[activeCPU].registers->EIP = destEIP; //The current OPCode: just jump to the address specified by the descriptor OR command!
				if (((isJMPorCALL & 0x1FF) == 4) || ((isJMPorCALL & 0x1FF) == 3)) //IRET/RETF required limit check!
				{
					if (CPU_MMU_checkrights(CPU_SEGMENT_CS, CPU[activeCPU].registers->CS, REG_EIP, 3, &CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS], 2, CPU_Operand_size[activeCPU])) //Limit broken or protection fault?
					{
						THROWDESCGP(0, 0, 0); //#GP(0) when out of limit range!
					}
				}
				CPU_flushPIQ(-1); //We're jumping to another address!
			}
			else if (segment == CPU_SEGMENT_SS) //SS? We're also updating the CPL!
			{
				updateCPL(); //Update the CPL according to the mode!
			}

			if (RETF_segmentregister) //Are we to check the segment registers for validity during a RETF?
			{
				for (RETF_segmentregister = 0; RETF_segmentregister < NUMITEMS(RETF_checkSegmentRegisters); ++RETF_segmentregister) //Process all we need to check!
				{
					RETF_whatsegment = RETF_checkSegmentRegisters[RETF_segmentregister]; //What register to check?
					word descriptor_index;
					descriptor_index = getDescriptorIndex(*CPU[activeCPU].SEGMENT_REGISTERS[RETF_whatsegment]); //What descriptor index?
					if (descriptor_index) //Valid index(Non-NULL)?
					{
						if ((word)(descriptor_index | 0x7) > ((*CPU[activeCPU].SEGMENT_REGISTERS[RETF_whatsegment] & 4) ? CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].PRECALCS.limit : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
						{
						invalidRETFsegment:
							if ((CPU[activeCPU].have_oldSegReg&(1 << RETF_whatsegment)) == 0) //Backup not loaded yet?
							{
								memcpy(&CPU[activeCPU].SEG_DESCRIPTORbackup[RETF_whatsegment], &CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment], sizeof(CPU[activeCPU].SEG_DESCRIPTORbackup[0])); //Restore the descriptor!
								CPU[activeCPU].oldSegReg[RETF_whatsegment] = *CPU[activeCPU].SEGMENT_REGISTERS[RETF_whatsegment]; //Backup the register too!
								CPU[activeCPU].have_oldSegReg |= (1 << RETF_whatsegment); //Loaded!
							}
							//Selector and Access rights are zeroed!
							*CPU[activeCPU].SEGMENT_REGISTERS[RETF_whatsegment] = 0; //Zero the register!
							if ((isJMPorCALL&0x1FF) == 3) //IRET?
							{
								CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment].desc.AccessRights &= 0x7F; //Clear the valid flag only with IRET!
							}
							else //RETF?
							{
								CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment].desc.AccessRights = 0; //Invalid!
							}
							CPU_calcSegmentPrecalcs(&CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment]); //Update the precalcs for said access rights!
							continue; //Next register!
						}
					}
					if (GENERALSEGMENT_P(CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment])==0) //Not present?
					{
						goto invalidRETFsegment; //Next register!
					}
					if (GENERALSEGMENT_S(CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment])==0) //Not data/readable code segment?
					{
						goto invalidRETFsegment; //Next register!
					}
					//We're either data or code!
					isnonconformingcodeordata = 0; //Default: neither!
					if (EXECSEGMENT_ISEXEC(CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment])) //Code?
					{
						if (!EXECSEGMENT_C(CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment])) //Nonconforming? Invalid!
						{
							isnonconformingcodeordata = 1; //Next register!
						}
						if (!EXECSEGMENT_R(CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment])) //Not readable? Invalid!
						{
							goto invalidRETFsegment; //Next register!
						}
					}
					else isnonconformingcodeordata = 1; //Data!
					//We're either data or readable code!
					if (isnonconformingcodeordata && (GENERALSEGMENT_DPL(CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment])<MAX(getCPL(),getRPL(*CPU[activeCPU].SEGMENT_REGISTERS[RETF_whatsegment])))) //Not privileged enough to handle said segment descriptor?
					{
						goto invalidRETFsegment; //Next register!
					}
				}
			}
		}
		else //A fault has been raised? Abort!
		{
			return 1; //Abort on fault!
		}
	}
	else //Real mode has no protection?
	{
		if ((isJMPorCALL&0x1FF) == 2) //CALL needs pushed data?
		{
			if ((CPU_Operand_size[activeCPU]) && (EMULATED_CPU>=CPU_80386)) //32-bit?
			{
				if (CPU8086_internal_PUSHw(0,&CPU[activeCPU].registers->CS,1)) return 1;
				if (CPU80386_internal_PUSHdw(2,&CPU[activeCPU].registers->EIP)) return 1;
			}
			else //16-bit?
			{
				if (CPU8086_internal_PUSHw(0,&CPU[activeCPU].registers->CS,0)) return 1;
				if (CPU8086_internal_PUSHw(2,&CPU[activeCPU].registers->IP,0)) return 1;
			}
		}

		if ((CPU[activeCPU].have_oldSegReg&(1 << segment)) == 0) //Backup not loaded yet?
		{
			memcpy(&CPU[activeCPU].SEG_DESCRIPTORbackup[segment], &CPU[activeCPU].SEG_DESCRIPTOR[segment], sizeof(CPU[activeCPU].SEG_DESCRIPTORbackup[0])); //Restore the descriptor!
			CPU[activeCPU].oldSegReg[segment] = *CPU[activeCPU].SEGMENT_REGISTERS[segment]; //Backup the register too!
			CPU[activeCPU].have_oldSegReg |= (1 << segment); //Loaded!
		}

		//if (memprotect(CPU[activeCPU].SEGMENT_REGISTERS[segment],2,"CPU_REGISTERS")) //Valid segment register?
		{
			*CPU[activeCPU].SEGMENT_REGISTERS[segment] = value; //Just set the segment, don't load descriptor!
			//Load the correct base data for our loading!
			CPU[activeCPU].SEG_DESCRIPTOR[segment].desc.base_low = (word)(((uint_32)value<<4)&0xFFFF); //Low base!
			CPU[activeCPU].SEG_DESCRIPTOR[segment].desc.base_mid = ((((uint_32)value << 4) & 0xFF0000)>>16); //Mid base!
			CPU[activeCPU].SEG_DESCRIPTOR[segment].desc.base_high = ((((uint_32)value << 4) & 0xFF000000)>>24); //High base!
			//This also maps the resulting segment in low memory (20-bit address space) in real mode, thus CS is pulled low as well!
			//Real mode affects only CS like Virtual 8086 mode(reloading all base/limit values). Other segments are unmodified.
			//Virtual 8086 mode also loads the rights etc.? This is to prevent Virtual 8086 tasks having leftover data in their descriptors, causing faults!
			if ((segment==CPU_SEGMENT_CS) || (getcpumode()==CPU_MODE_8086)) //Only done for the CS segment in real mode as well as all registers in 8086 mode?
			{
				CPU[activeCPU].SEG_DESCRIPTOR[segment].desc.AccessRights = 0x93; //Compatible rights!
				CPU[activeCPU].SEG_DESCRIPTOR[segment].desc.limit_low = 0xFFFF;
				CPU[activeCPU].SEG_DESCRIPTOR[segment].desc.noncallgate_info = 0x00; //Not used!
			}
		}
		if (segment==CPU_SEGMENT_CS) //CS segment? Reload access rights in real mode on first write access!
		{
			CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].desc.AccessRights = 0x93; //Load default access rights!
			CPU_calcSegmentPrecalcs(&CPU[activeCPU].SEG_DESCRIPTOR[segment]); //Calculate any precalcs for the segment descriptor(do it here since we don't load descriptors externally)!
			CPU[activeCPU].registers->EIP = destEIP; //... The current OPCode: just jump to the address!
			CPU_flushPIQ(-1); //We're jumping to another address!
		}
		else if (segment == CPU_SEGMENT_SS) //SS? We're also updating the CPL!
		{
			updateCPL(); //Update the CPL according to the mode!
			CPU_calcSegmentPrecalcs(&CPU[activeCPU].SEG_DESCRIPTOR[segment]); //Calculate any precalcs for the segment descriptor(do it here since we don't load descriptors externally)!
		}
		else
		{
			CPU_calcSegmentPrecalcs(&CPU[activeCPU].SEG_DESCRIPTOR[segment]); //Calculate any precalcs for the segment descriptor(do it here since we don't load descriptors externally)!
		}
	}
	//Real mode doesn't use the descriptors?
	return 0; //No fault raised&continue!
}

int checkPrivilegedInstruction() //Allowed to run a privileged instruction?
{
	if (getCPL()) //Not allowed when CPL isn't zero?
	{
		THROWDESCGP(0,0,0); //Throw a descriptor fault!
		return 0; //Not allowed to run!
	}
	return 1; //Allowed to run!
}

/*

MMU: Memory limit!

*/

OPTINLINE byte verifyLimit(SEGMENT_DESCRIPTOR *descriptor, uint_64 offset)
{
	//Execute address test?
	INLINEREGISTER byte isvalid;
	isvalid = (offset<=descriptor->PRECALCS.limit); //Valid address range!
	isvalid ^= descriptor->PRECALCS.topdown; //Apply expand-down data segment, if required, which reverses valid!
	isvalid &= ((offset>descriptor->PRECALCS.roof)^1); //Limit to 16-bit/32-bit address space using both top-down(required) and bottom-up(resulting in just the limit, which is lower or equal to the roof) descriptors!
	isvalid &= 1; //Only 1-bit testing!
	return isvalid; //Are we valid?
}

byte CPU_MMU_checkrights_cause = 0; //What cause?
//Used by the CPU(VERR/VERW)&MMU I/O! forreading=0: Write, 1=Read normal, 3=Read opcode

byte CPU_MMU_checkrights(int segment, word segmentval, uint_64 offset, byte forreading, SEGMENT_DESCRIPTOR *descriptor, byte addrtest, byte is_offset16)
{
	//First: type checking!

	if (unlikely(descriptor->PRECALCS.notpresent)) //Not present(invalid in the cache)? This also applies to NULL descriptors!
	{
		CPU_MMU_checkrights_cause = 1; //What cause?
		return 1; //#GP fault: not present in descriptor cache mean invalid, thus #GP!
	}

	//Basic access rights are always checked!
	if (likely(GENERALSEGMENTPTR_S(descriptor))) //System segment? Check for additional type information!
	{
		//Entries 0,4,10,14: On writing, Entries 2,6: Never match, Entries 8,12: Writing or reading normally(!=3).
		//To ignore an entry for errors, specify mask 0, non-equals nonzero, comparison 0(a.k.a. ((forreading&0)!=0)
		if (unlikely(descriptor->PRECALCS.rwe_errorout[forreading])) //Are we to error out on this read/write/execute operation?
		{
			CPU_MMU_checkrights_cause = 3; //What cause?
			return 1; //Error!
		}
	}

	//Next: limit checking!
	if (likely(addrtest)) //Address test is to be performed?
	{
		if (likely(verifyLimit(descriptor,offset))) return 0; //OK? We're finished!
		//Not valid?
		{
			CPU_MMU_checkrights_cause = 6; //What cause?
			if (segment==CPU_SEGMENT_SS) //Stack fault?
			{
				return 3; //Error!
			}
			else //Normal #GP?
			{
				return 1; //Error!
			}
		}
	}

	//Don't perform rights checks: This is done when loading the segment register only!
	return 0; //OK!
}

//Used by the MMU! forreading: 0=Writes, 1=Read normal, 3=Read opcode fetch. bit8=bit9 contains EXT bit to use!
int CPU_MMU_checklimit(int segment, word segmentval, uint_64 offset, word forreading, byte is_offset16) //Determines the limit of the segment, forreading=2 when reading an opcode!
{
	byte rights;
	//Determine the Limit!
	if (likely(EMULATED_CPU >= CPU_80286)) //Handle like a 80286+?
	{
		if (unlikely(segment==-1))
		{
			CPU_MMU_checkrights_cause = 0x80; //What cause?
			return 0; //Enable: we're an emulator call!
		}
		
		//Use segment descriptors, even when in real mode on 286+ processors!
		rights = CPU_MMU_checkrights(segment,segmentval, offset, forreading, &CPU[activeCPU].SEG_DESCRIPTOR[segment],1,is_offset16); //What rights resulting? Test the address itself too!
		if (unlikely(rights)) //Error?
		{
			switch (rights)
			{
			default: //Unknown status? Count #GP by default!
			case 1: //#GP(0) or pseudo protection fault(Real/V86 mode(V86 mode only during limit range exceptions, otherwise error code 0))?
				if (unlikely((forreading&0x10)==0)) CPU_GP(((getcpumode()==CPU_MODE_PROTECTED) || (!(((CPU_MMU_checkrights_cause==6) && (getcpumode()==CPU_MODE_8086)) || (getcpumode()==CPU_MODE_REAL))))?0:-2); //Throw (pseudo) fault when not prefetching!
				return 1; //Error out!
				break;
			case 2: //#NP?
				if (unlikely((forreading&0x10)==0)) THROWDESCNP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error: accessing non-present segment descriptor when not prefetching!
				return 1; //Error out!
				break;
			case 3: //#SS(0) or pseudo protection fault(Real/V86 mode)?
				if (unlikely((forreading&0x10)==0)) CPU_StackFault(((getcpumode()==CPU_MODE_PROTECTED) || (!(((CPU_MMU_checkrights_cause==6) && (getcpumode()==CPU_MODE_8086)) || (getcpumode()==CPU_MODE_REAL))))?(((((forreading&0x200)>>1)&(forreading&0x100))>>8)|((forreading&0x100)?(REG_SS&0xFFFC):0)):-2); //Throw (pseudo) fault when not prefetching! Set EXT bit(bit7) when requested(bit6) and give SS instead of 0!
				return 1; //Error out!
				break;
			}
		}
		return 0; //OK!
	}
	return 0; //Don't give errors: handle like a 80(1)86!
}

byte checkSpecialRights() //Check special rights, common by any rights instructions!
{
	if (getcpumode() == CPU_MODE_REAL) return 0; //Allow all for real mode!
	if (FLAG_PL < getCPL()) //We're not allowed!
	{
		return 1; //Not priviledged!
	}
	return 0; //Priviledged!
}

byte checkSpecialPortRights() //Check special rights, common by any rights instructions!
{
	if (getcpumode() == CPU_MODE_REAL) return 0; //Allow all for real mode!
	if ((getCPL()>FLAG_PL)||isV86()) //We're to check when not priviledged or Virtual 8086 mode!
	{
		return 1; //Not priviledged!
	}
	return 0; //Priviledged!
}

byte checkSTICLI() //Check STI/CLI rights!
{
	if (checkSpecialRights()) //Not priviledged?
	{
		THROWDESCGP(0,0,0); //Raise exception!
		return 0; //Ignore this command!
	}
	return 1; //We're allowed to execute!
}

byte disallowPOPFI() //Allow POPF to change interrupt flag?
{
	return checkSpecialRights(); //Simply ignore the change when not priviledged!
}

byte checkPortRights(word port) //Are we allowed to not use this port?
{
	if (checkSpecialPortRights()) //We're to check the I/O permission bitmap! 286+ only!
	{
		protection_PortRightsLookedup = 1; //The port rights are looked up!
		uint_32 maplocation;
		byte mappos;
		byte mapvalue;
		word mapbase;
		maplocation = (port>>3); //8 bits per byte!
		mappos = (1<<(port&7)); //The bit within the byte specified!
		mapvalue = 1; //Default to have the value 1!
		if (((GENERALSEGMENT_TYPE(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR]) == AVL_SYSTEM_BUSY_TSS32BIT) || (GENERALSEGMENT_TYPE(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR]) == AVL_SYSTEM_TSS32BIT)) && CPU[activeCPU].registers->TR && GENERALSEGMENT_P(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR])) //Active 32-bit TSS?
		{
			uint_32 limit;
			limit = CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR].PRECALCS.limit; //The limit of the descriptor!
			if (limit >= 0x68) //Valid to check?
			{
				if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, 0x66, 0x40 | 1, 0, 1, 0)) return 2; //Check if the address is valid according to segmentation!
				if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, 0x66, 0xA0 | 1, 0, 1, 0)) return 2; //Check if the address is valid according to the remainder of checks!
				mapbase = MMU_rw0(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, 0x66, 0, 1); //Add the map location to the specified address!
				maplocation += mapbase; //The actual location!
				//Custom, not in documentation: 
				if ((maplocation <= limit) && (mapbase < limit) && (mapbase >= 0x68)) //Not over the limit? We're an valid entry! There is no map when the base address is greater than or equal to the TSS limit().
				{
					if (checkMMUaccess(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, maplocation, 0x40 | 1, 0, 1, 0)) return 2; //Check if the address is valid according to segmentation!
					if (checkMMUaccess(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, maplocation, 0xA0 | 1, 0, 1, 0)) return 2; //Check if the address is valid according to the remainder of checks!
					mapvalue = (MMU_rb0(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, maplocation, 0, 1)&mappos); //We're the bit to use!
				}
			}
		}
		if (mapvalue) //The map bit is set(or not a 32-bit task)? We're to trigger an exception!
		{
			return 1; //Trigger an exception!
		}
	}
	return 0; //Allow all for now!
}

extern byte immb; //For CPU_readOP result!

//bit2=EXT when set.
byte switchStacks(byte newCPL)
{
	word SSn;
	uint_32 ESPn;
	byte TSSSize;
	word TSS_StackPos;
	TSSSize = 0; //Default to 16-bit TSS!
	switch (GENERALSEGMENT_TYPE(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR])) //What kind of TSS?
	{
	case AVL_SYSTEM_BUSY_TSS32BIT:
	case AVL_SYSTEM_TSS32BIT:
		TSSSize = 1; //32-bit TSS!
	case AVL_SYSTEM_BUSY_TSS16BIT:
	case AVL_SYSTEM_TSS16BIT:
		TSS_StackPos = (2<<TSSSize); //Start of the stack block! 2 for 16-bit TSS, 4 for 32-bit TSS!
		TSS_StackPos += (4<<TSSSize)*(newCPL&3); //Start of the correct TSS (E)SP! 4 for 16-bit TSS, 8 for 32-bit TSS!
		//Check against memory first!
		//First two are the SP!
		if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, TSS_StackPos, 0x40 | 1, 0, 1, 0)) return 2; //Check if the address is valid according to segmentation!
		//Next two are either high ESP or SS!
		if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, TSS_StackPos+2, 0x40 | 1, 0, 1, 0)) return 2; //Check if the address is valid according to segmentation!
		if (TSSSize) //Extra checks for 32-bit?
		{
			//The 32-bit TSS SSn value!
			if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, TSS_StackPos+4, 0x40 | 1, 0, 1, 0)) return 2; //Check if the address is valid according to segmentation!
		}
		//First two are the SP!
		if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, TSS_StackPos, 0xA0 | 1, 0, 1, 0)) return 2; //Check if the address is valid according to the remainder of checks!
		//Next two are either high ESP or SS!
		if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, TSS_StackPos+2, 0xA0 | 1, 0, 1, 0)) return 2; //Check if the address is valid according to the remainder of checks!
		if (TSSSize) //Extra checks for 32-bit?
		{
			//The 32-bit TSS SSn value!
			if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, TSS_StackPos+4, 0xA0 | 1, 0, 1, 0)) return 2; //Check if the address is valid according to the remainder of checks!
		}
		//Memory is now validated! Load the values from memory!

		ESPn = TSSSize?MMU_rdw0(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,TSS_StackPos,0,1):MMU_rw0(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,TSS_StackPos,0,1); //Read (E)SP for the privilege level from the TSS!
		TSS_StackPos += (2<<TSSSize); //Convert the (E)SP location to SS location!
		SSn = MMU_rw0(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,TSS_StackPos,0,1); //SS!
		if (segmentWritten(CPU_SEGMENT_SS,SSn,0x200|((newCPL<<8)&0x400)|0x1000|((newCPL&3)<<13))) return 1; //Read SS, privilege level changes, ignore DPL vs CPL check! Fault=#TS. EXT bit when set in bit 2 of newCPL.
		if (TSSSize) //32-bit?
		{
			CPU[activeCPU].registers->ESP = ESPn; //Apply the stack position!
		}
		else
		{
			CPU[activeCPU].registers->SP = (word)ESPn; //Apply the stack position!
		}
	default: //Unknown TSS?
		break; //No switching for now!
	}
	return 0; //OK!
}

byte CPU_ProtectedModeInterrupt(byte intnr, word returnsegment, uint_32 returnoffset, int_64 errorcode, byte is_interrupt) //Execute a protected mode interrupt!
{
	byte left; //The amount of bytes left to read of the IDT entry!
	uint_32 base;
	base = (intnr<<3); //The base offset of the interrupt in the IDT!

	hascallinterrupttaken_type = (getCPL())?INTERRUPTGATETIMING_SAMELEVEL:INTERRUPTGATETIMING_DIFFERENTLEVEL; //Assume we're jumping to CPL0 when erroring out!

	CPU[activeCPU].executed = 0; //Default: still busy executing!
	if (CPU[activeCPU].faultraised==2) CPU[activeCPU].faultraised = 0; //Clear non-fault, if present!

	byte isEXT;
	isEXT = ((is_interrupt&1)?0:((is_interrupt&4)>>2))|((errorcode>=0)?(errorcode&1):0); //The EXT bit to use for direct exceptions! 0 for interrupts, 1 for exceptions!

	if ((base|0x7) > CPU[activeCPU].registers->IDTR.limit) //Limit exceeded?
	{
		THROWDESCGP(base,isEXT,EXCEPTION_TABLE_IDT); //#GP!
		return 0; //Abort!
	}

	base += CPU[activeCPU].registers->IDTR.base; //Add the base for the actual offset into the IDT!
	
	RAWSEGMENTDESCRIPTOR idtentry; //The loaded IVT entry!
	for (left=0;left<(int)sizeof(idtentry.descdata);++left)
	{
		if (checkDirectMMUaccess(base++,1,/*getCPL()*/ 0)) //Error in the paging unit?
		{
			return 1; //Error out!
		}
	}
	base -= sizeof(idtentry.descdata); //Restore start address!
	for (left=0;left<sizeof(idtentry.descdata);) //Data left to read?
	{
		if (memory_readlinear(base++,&idtentry.descdata[left++])) //Read a descriptor byte directly from flat memory!
		{
			return 0; //Failed to load the descriptor!
		}
	}
	base -= sizeof(idtentry.descdata); //Restore start address!
	base -= CPU[activeCPU].registers->IDTR.base; //Substract the base for the actual offset into the IDT!
	//Now, base is the restored vector into the IDT!

	idtentry.offsethigh = DESC_16BITS(idtentry.offsethigh); //Patch when needed!
	idtentry.offsetlow = DESC_16BITS(idtentry.offsetlow); //Patch when needed!
	idtentry.selector = DESC_16BITS(idtentry.selector); //Patch when needed!
	return CPU_handleInterruptGate(isEXT,EXCEPTION_TABLE_IDT,base,&idtentry,returnsegment,returnoffset,errorcode,is_interrupt); //Handle the interrupt gate!
}

byte CPU_handleInterruptGate(byte EXT, byte table,uint_32 descriptorbase, RAWSEGMENTDESCRIPTOR *theidtentry, word returnsegment, uint_32 returnoffset, int_64 errorcode, byte is_interrupt) //Execute a protected mode interrupt!
{
	uint_32 errorcode32 = (uint_32)errorcode; //Get the error code itelf!
	word errorcode16 = (word)errorcode; //16-bit variant, if needed!
	SEGMENT_DESCRIPTOR newdescriptor; //Temporary storage for task switches!
	word desttask; //Destination task for task switches!
	uint_32 base;
	sbyte loadresult;
	byte oldCPL;
	base = descriptorbase; //The base offset of the interrupt in the IDT!
	oldCPL = getCPL(); //Save the old CPL for reference!

	hascallinterrupttaken_type = (getRPL(theidtentry->selector)==oldCPL)?INTERRUPTGATETIMING_SAMELEVEL:INTERRUPTGATETIMING_DIFFERENTLEVEL;

	if (errorcode<0) //Invalid error code to use?
	{
		errorcode16 = 0; //Empty to log!
		errorcode32 = 0; //Empty to log!
	}

	EXT &= 1; //1-bit value!

	CPU[activeCPU].executed = 0; //Default: still busy executing!
	if (CPU[activeCPU].faultraised==2) CPU[activeCPU].faultraised = 0; //Clear non-fault, if present!

	byte is32bit;
	RAWSEGMENTDESCRIPTOR idtentry; //The loaded IVT entry!
	memcpy(&idtentry,theidtentry,sizeof(idtentry)); //Make a copy for our own use!

	if ((is_interrupt&1) && (IDTENTRY_DPL(idtentry) < getCPL())) //Not enough rights on software interrupt?
	{
		THROWDESCGP(base,EXT,table); //#GP!
		return 0;
	}
	//Now, the (gate) descriptor to use is loaded!
	switch (IDTENTRY_TYPE(idtentry)) //What type are we?
	{
	case IDTENTRY_TASKGATE: //32-bit task gate?
	case IDTENTRY_16BIT_INTERRUPTGATE: //16/32-bit interrupt gate?
	case IDTENTRY_16BIT_TRAPGATE: //16/32-bit trap gate?
	case IDTENTRY_16BIT_INTERRUPTGATE|IDTENTRY_32BIT_GATEEXTENSIONFLAG: //16/32-bit interrupt gate?
	case IDTENTRY_16BIT_TRAPGATE|IDTENTRY_32BIT_GATEEXTENSIONFLAG: //16/32-bit trap gate?
		break;
	default:
		THROWDESCGP(base,EXT,table); //#NP isn't triggered with IDT entries! #GP is triggered instead!
		return 0;
	}

	if (IDTENTRY_P(idtentry)==0) //Not present?
	{
		THROWDESCNP(base,EXT,table); //#NP isn't triggered with IDT entries! #GP is triggered instead?
		return 0;
	}

	//Now, the (gate) descriptor to use is loaded!
	switch (IDTENTRY_TYPE(idtentry)) //What type are we?
	{
	case IDTENTRY_TASKGATE: //32-bit task gate?
		desttask = idtentry.selector; //Read the destination task!
		if (((loadresult = LOADDESCRIPTOR(CPU_SEGMENT_TR, desttask, &newdescriptor,2|(EXT<<10)))<=0) || (desttask&4)) //Error loading new descriptor? The backlink is always at the start of the TSS! It muse also always be in the GDT!
		{
			if (loadresult >= 0) //Not faulted already?
			{
				THROWDESCGP(desttask, EXT, (desttask & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Throw #GP error!
			}
			return 0; //Error, by specified reason!
		}
		CPU_executionphase_starttaskswitch(CPU_SEGMENT_TR, &newdescriptor, &CPU[activeCPU].registers->TR, desttask, ((2|0x80)|(EXT<<10)),1,errorcode); //Execute a task switch to the new task! We're switching tasks like a CALL instruction(https://xem.github.io/minix86/manual/intel-x86-and-64-manual-vol3/o_fe12b1e2a880e0ce-250.html)! We're a call based on an interrupt!
		break;
	default: //All other cases?
		is32bit = ((IDTENTRY_TYPE(idtentry)&IDTENTRY_32BIT_GATEEXTENSIONFLAG)>>IDTENTRY_32BIT_GATEEXTENSIONFLAG_SHIFT); //Enable 32-bit gate?
		switch (IDTENTRY_TYPE(idtentry) & 0x7) //What type are we?
		{
		case IDTENTRY_16BIT_INTERRUPTGATE: //16/32-bit interrupt gate?
		case IDTENTRY_16BIT_TRAPGATE: //16/32-bit trap gate?
			hascallinterrupttaken_type = (getRPL(idtentry.selector)==oldCPL)?INTERRUPTGATETIMING_SAMELEVEL:INTERRUPTGATETIMING_DIFFERENTLEVEL;

			//Table can be found at: http://www.read.seas.harvard.edu/~kohler/class/04f-aos/ref/i386/s15_03.htm#fig15-3

			if ((loadresult = LOADDESCRIPTOR(CPU_SEGMENT_CS, idtentry.selector, &newdescriptor,2))<=0) //Error loading new descriptor? The backlink is always at the start of the TSS!
			{
				if (loadresult==0) //Not faulted already?
				{
					THROWDESCGP(idtentry.selector,EXT,(idtentry.selector&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
				}
				return 0; //Error, by specified reason!
			}

			hascallinterrupttaken_type = (GENERALSEGMENT_DPL(newdescriptor)==oldCPL)?INTERRUPTGATETIMING_SAMELEVEL:INTERRUPTGATETIMING_DIFFERENTLEVEL; //Assume destination privilege level for faults!

			if (
				(getLoadedTYPE(&newdescriptor) != 1) //Not an executable segment?
					) //NULL descriptor loaded? Invalid too(done by the above present check too)!
			{
				THROWDESCGP(idtentry.selector,EXT,(idtentry.selector&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
				return 0; //Not present: limit exceeded!	
			}

			if (!GENERALSEGMENT_P(newdescriptor)) //Not present?
			{
				THROWDESCNP(idtentry.selector,EXT,(idtentry.selector&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP!
				return 0;
			}

			byte INTTYPE=0;

			if ((EXECSEGMENT_C(newdescriptor) == 0) && (GENERALSEGMENT_DPL(newdescriptor)<getCPL())) //Not enough rights, but conforming?
			{
				INTTYPE = 1; //Interrupt to inner privilege!
			}
			else
			{
				if ((EXECSEGMENT_C(newdescriptor)) || (GENERALSEGMENT_DPL(newdescriptor)==getCPL())) //Not enough rights, but conforming?
				{
					INTTYPE = 2; //Interrupt to same privilege level!
				}
				else
				{
					THROWDESCGP(idtentry.selector,EXT,(idtentry.selector&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP!
					return 0;
				}
			}

			uint_32 EFLAGSbackup;
			EFLAGSbackup = REG_EFLAGS; //Back-up EFLAGS!

			byte newCPL;
			newCPL = GENERALSEGMENT_DPL(newdescriptor); //New CPL to use!

			if (FLAG_V8 && (INTTYPE==1)) //Virtual 8086 mode to monitor switching to CPL 0?
			{
				hascallinterrupttaken_type = (newCPL==oldCPL)?INTERRUPTGATETIMING_SAMELEVEL:INTERRUPTGATETIMING_DIFFERENTLEVEL;
				#ifdef LOG_VIRTUALMODECALLS
				if ((MMU_logging == 1) && advancedlog)
				{
					dolog("debugger", "Starting V86 interrupt/fault: INT %02X(%02X(0F:%02X)),immb:%02X,AX=%04X)", intnr, CPU[activeCPU].lastopcode, CPU[activeCPU].lastopcode0F, immb, REG_AX);
				}
				#endif
				if (newCPL!=0) //Not switching to PL0?
				{
					THROWDESCGP(idtentry.selector,EXT,EXCEPTION_TABLE_GDT); //Exception!
					return 0; //Abort on fault!
				}

				//Now, switch to the new EFLAGS!
				FLAGW_V8(0); //Clear the Virtual 8086 mode flag!
				updateCPUmode(); //Update the CPU mode!

				//We're back in protected mode now!

				//Switch Stack segment first!
				if (switchStacks(newCPL|(EXT<<2))) return 1; //Abort failing switching stacks!
				//Verify that the new stack is available!
				if (is32bit) //32-bit gate?
				{
					if (checkStackAccess(9+(((errorcode!=-1) && (errorcode!=-2))?1:0),1|0x100|((EXT&1)<<9),1)) return 0; //Abort on fault!
				}
				else //16-bit gate?
				{
					if (checkStackAccess(9+(((errorcode!=-1) && (errorcode!=-2))?1:0),1|0x100|((EXT&1)<<9),0)) return 0; //Abort on fault!
				}

				//Calculate and check the limit!

				if (verifyLimit(&newdescriptor,((idtentry.offsetlow | (idtentry.offsethigh << 16))&(0xFFFFFFFF>>((is32bit^1)<<4))))==0) //Limit exceeded?
				{
					THROWDESCGP(0,0,0); //Throw #GP(0)!
					return 0;
				}

				//Save the Segment registers on the new stack!
				CPU_PUSH16(&REG_GS,is32bit);
				CPU_PUSH16(&REG_FS,is32bit);
				CPU_PUSH16(&REG_DS,is32bit);
				CPU_PUSH16(&REG_ES,is32bit);
				CPU_PUSH16(&CPU[activeCPU].oldSS,is32bit);
				CPU_PUSH32(&CPU[activeCPU].oldESP);
				//Other registers are the normal variants!

				//Load all Segment registers with zeroes!
				if (segmentWritten(CPU_SEGMENT_DS,0,(EXT<<10))) return 0; //Clear DS! Abort on fault!
				if (segmentWritten(CPU_SEGMENT_ES,0,(EXT<<10))) return 0; //Clear ES! Abort on fault!
				if (segmentWritten(CPU_SEGMENT_FS,0,(EXT<<10))) return 0; //Clear FS! Abort on fault!
				if (segmentWritten(CPU_SEGMENT_GS,0,(EXT<<10))) return 0; //Clear GS! Abort on fault!
			}
			else if (FLAG_V8) 
			{
				THROWDESCGP(idtentry.selector,EXT,EXCEPTION_TABLE_GDT); //Exception!
				return 0; //Abort on fault!
			}
			else if ((FLAG_V8==0) && (INTTYPE==1)) //Privilege level changed in protected mode?
			{
				//Unlike the other case, we're still in protected mode!
				//We're back in protected mode now!

				//Switch Stack segment first!
				if (switchStacks(newCPL|(EXT<<2))) return 1; //Abort failing switching stacks!

				//Verify that the new stack is available!
				if (checkStackAccess(5+(((errorcode!=-1) && (errorcode!=-2))?1:0),1|0x100|((EXT&1)<<9),is32bit?1:0)) return 0; //Abort on fault!

				//Calculate and check the limit!

				if (verifyLimit(&newdescriptor,((idtentry.offsetlow | (idtentry.offsethigh << 16))&(0xFFFFFFFF>>((is32bit^1)<<4))))==0) //Limit exceeded?
				{
					THROWDESCGP(0,0,0); //Throw #GP(0)!
					return 0;
				}

				if (is32bit) //32-bit gate?
				{
					CPU_PUSH16(&CPU[activeCPU].oldSS,1);
					CPU_PUSH32(&CPU[activeCPU].oldESP);
				}
				else //16-bit gate?
				{
					word temp = (word)(CPU[activeCPU].oldESP&0xFFFF); //Backup SP!
					CPU_PUSH16(&CPU[activeCPU].oldSS,0);
					CPU_PUSH16(&temp,0); //Old SP!
				}
				//Other registers are the normal variants!
			}
			else
			{
				if (checkStackAccess(3+(((errorcode!=-1) && (errorcode!=-2))?1:0),1|0x100|((EXT&1)<<9),is32bit?1:0)) return 0; //Abort on fault!
				//Calculate and check the limit!

				if (verifyLimit(&newdescriptor,((idtentry.offsetlow | (idtentry.offsethigh << 16))&(0xFFFFFFFF>>((is32bit^1)<<4))))==0) //Limit exceeded?
				{
					THROWDESCGP(0,0,0); //Throw #GP(0)!
					return 0;
				}
			}

			if (is32bit)
			{
				CPU_PUSH32(&EFLAGSbackup); //Push original EFLAGS!
			}
			else
			{
				word temp2 = (word)(EFLAGSbackup&0xFFFF);
				CPU_PUSH16(&temp2,0); //Push FLAGS!
			}

			CPU_PUSH16(&CPU[activeCPU].registers->CS,is32bit); //Push CS!

			if (is32bit)
			{
				CPU_PUSH32(&CPU[activeCPU].registers->EIP); //Push EIP!
			}
			else
			{
				CPU_PUSH16(&CPU[activeCPU].registers->IP,0); //Push IP!
			}

			if ((CPU[activeCPU].have_oldSegReg&(1 << CPU_SEGMENT_CS)) == 0) //Backup not loaded yet?
			{
				memcpy(&CPU[activeCPU].SEG_DESCRIPTORbackup[CPU_SEGMENT_CS], &CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS], sizeof(CPU[activeCPU].SEG_DESCRIPTORbackup[0])); //Restore the descriptor!
				CPU[activeCPU].oldSegReg[CPU_SEGMENT_CS] = *CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_CS]; //Backup the register too!
				CPU[activeCPU].have_oldSegReg |= (1 << CPU_SEGMENT_CS); //Loaded!
			}
			memcpy(&CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS], &newdescriptor, sizeof(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS])); //Load the segment descriptor into the cache!
			//if (memprotect(CPU[activeCPU].SEGMENT_REGISTERS[segment],2,"CPU_REGISTERS")) //Valid segment register?
			{
				*CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_CS] = idtentry.selector; //Set the segment register to the allowed value!
			}

			if (INTTYPE==1) CPU[activeCPU].CPL = newCPL; //Privilege level changes!
			if (INTTYPE==1) CPU[activeCPU].CPL = newCPL; //Privilege level changes!

			setRPL(*CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_CS],getCPL()); //CS.RPL=CPL!

			CPU[activeCPU].registers->EIP = ((idtentry.offsetlow | (idtentry.offsethigh << 16))&(0xFFFFFFFF >> ((is32bit ^ 1) << 4))); //The current OPCode: just jump to the address specified by the descriptor OR command!
			CPU_flushPIQ(-1); //We're jumping to another address!

			FLAGW_TF(0);
			FLAGW_NT(0);
			FLAGW_RF(0); //Clear Resume flag too!

			if (EMULATED_CPU >= CPU_80486)
			{
				FLAGW_AC(0); //Clear Alignment Check flag too!
			}

			if ((IDTENTRY_TYPE(idtentry) & 0x7) == IDTENTRY_16BIT_INTERRUPTGATE)
			{
				FLAGW_IF(0); //No interrupts!
			}
			updateCPUmode(); //flags have been updated!

			if ((errorcode>=0)) //Error code specified?
			{
				if (/*SEGDESC_NONCALLGATE_D_B(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR])&CPU[activeCPU].D_B_Mask*/ is32bit) //32-bit task?
				{
					CPU_PUSH32(&errorcode32); //Push the error on the stack!
				}
				else
				{
					CPU_PUSH16(&errorcode16,is32bit); //Push the error on the stack!
				}
			}

			if ((loadresult = touchSegment(CPU_SEGMENT_CS,idtentry.selector,&newdescriptor,2))<=0) //Errored out during touching?
			{
				if (loadresult == 0) //Not already faulted?
				{
					if (2&0x200) //TSS is the cause?
					{
						THROWDESCTS(idtentry.selector,1,(idtentry.selector&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!			
					}
					else //Plain #GP?
					{
						THROWDESCGP(idtentry.selector,((idtentry.selector&0x400)>>10),(idtentry.selector&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
					}
				}
				return 1; //Abort on fault!
			}

			hascallinterrupttaken_type = (getCPL()==oldCPL)?INTERRUPTGATETIMING_SAMELEVEL:INTERRUPTGATETIMING_DIFFERENTLEVEL;
			CPU[activeCPU].executed = 1; //We've executed, start any post-instruction stuff!
			CPU_interruptcomplete(); //Prepare us for new instructions!
			return 1; //OK!
			break;
		default: //Unknown descriptor type?
			THROWDESCGP(base,EXT,table); //#GP! We're always from the IDT!
			return 0; //Errored out!
			break;
		}
		break;
	}
	return 0; //Default: Errored out!
}
