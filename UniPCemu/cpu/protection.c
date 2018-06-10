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
#include "headers/cpu/CPU_OP8086.h" //8086+ push/pop support!
#include "headers/cpu/CPU_OP80386.h" //80386+ push/pop support!

//Log Virtual 8086 mode calls basic information?
#define LOG_VIRTUALMODECALLS

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

void CPU_triplefault()
{
	CPU[activeCPU].faultraised_lasttype = 0xFF; //Full on reset has been raised!
	CPU[activeCPU].resetPending = 1; //Start pending a reset!
	CPU[activeCPU].faultraised = 1; //We're continuing being a fault!
	CPU[activeCPU].executed = 1; //We're finishing to execute!
}

void CPU_doublefault()
{
	CPU[activeCPU].faultraised_lasttype = EXCEPTION_DOUBLEFAULT;
	CPU[activeCPU].faultraised = 1; //Raising a fault!
	uint_64 zerovalue=0; //Zero value pushed!
	++CPU[activeCPU].faultlevel; //Raise the fault level to cause triple faults!
	CPU_executionphase_startinterrupt(EXCEPTION_DOUBLEFAULT,0,zerovalue); //Execute the double fault handler!
}

byte CPU_faultraised(byte type)
{
	if (EMULATED_CPU<CPU_80286) return 1; //Always allow on older processors without protection!
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
				case EXCEPTION_DIVIDEERROR:
				case EXCEPTION_INVALIDTSSSEGMENT:
				case EXCEPTION_SEGMENTNOTPRESENT:
				case EXCEPTION_STACKFAULT:
				case EXCEPTION_GENERALPROTECTIONFAULT: //First cases?
					switch (type) //What second cause?
					{
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
				case EXCEPTION_PAGEFAULT: //Page fault? Second case!
					switch (type) //What second cause?
					{
						case EXCEPTION_PAGEFAULT:
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
	if (CPU[activeCPU].have_oldSS) //Returning the SS to it's old value?
	{
		REG_SS = CPU[activeCPU].oldSS; //Restore SS to it's original value!
	}
	if (CPU[activeCPU].have_oldESP) //Returning the (E)SP to it's old value?
	{
		REG_ESP = CPU[activeCPU].oldESP; //Restore ESP to it's original value!
	}
	if (CPU[activeCPU].have_oldEFLAGS) //Returning the (E)SP to it's old value?
	{
		REG_EFLAGS = CPU[activeCPU].oldEFLAGS; //Restore EFLAGS to it's original value!
		updateCPUmode(); //Restore the CPU mode!
	}
	if (CPU[activeCPU].have_oldSegments) //Returning the (E)SP to it's old value?
	{
		REG_DS = CPU[activeCPU].oldSegmentDS; //Restore ESP to it's original value!
		REG_ES = CPU[activeCPU].oldSegmentES; //Restore ESP to it's original value!
		REG_FS = CPU[activeCPU].oldSegmentFS; //Restore ESP to it's original value!
		REG_GS = CPU[activeCPU].oldSegmentGS; //Restore ESP to it's original value!
	}
	if (CPU[activeCPU].have_oldTR) //Returning the TR to it's old value?
	{
		CPU[activeCPU].registers->TR = CPU[activeCPU].oldTR;
		memcpy(&CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR],&CPU[activeCPU].oldTRdesc,sizeof(CPU[activeCPU].SEG_DESCRIPTOR[0])); //Restore segment descriptor!
		CPU[activeCPU].SEG_base[CPU_SEGMENT_TR] = CPU[activeCPU].oldTRbase;
		CPU[activeCPU].have_oldTR = 0; //We've been reversed manually!
	}
}

void CPU_saveFaultData() //Prepare for a fault by saving all required data!
{
		CPU[activeCPU].oldSS = REG_SS; //Restore SS to it's original value!
		CPU[activeCPU].have_oldSS = 1; //Restorable!
		CPU[activeCPU].oldESP = REG_ESP; //Restore ESP to it's original value!
		CPU[activeCPU].have_oldESP = 1; //Restorable!
		CPU[activeCPU].oldEFLAGS = REG_EFLAGS; //Restore EFLAGS to it's original value!
		CPU[activeCPU].have_oldEFLAGS = 1; //Restorable!
		updateCPUmode(); //Restore the CPU mode!
		CPU[activeCPU].oldSegmentDS = REG_DS; //Restore ESP to it's original value!
		CPU[activeCPU].oldSegmentES = REG_ES; //Restore ESP to it's original value!
		CPU[activeCPU].oldSegmentFS = REG_FS; //Restore ESP to it's original value!
		CPU[activeCPU].oldSegmentGS = REG_GS; //Restore ESP to it's original value!
		CPU[activeCPU].have_oldSegments = 1; //Restorable!
}

//More info: http://wiki.osdev.org/Paging
//General Protection fault.
void CPU_GP(int_64 errorcode)
{
	if (debugger_logging()) //Are we logging?
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
		CPU_executionphase_startinterrupt(EXCEPTION_GENERALPROTECTIONFAULT,0,errorcode); //Call IVT entry #13 decimal!
		//Execute the interrupt!
	}
}

void CPU_AC(int_64 errorcode)
{
	if (debugger_logging()) //Are we logging?
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
		CPU_executionphase_startinterrupt(EXCEPTION_ALIGNMENTCHECK,0,errorcode); //Call IVT entry #13 decimal!
		//Execute the interrupt!
	}
}

void CPU_SegNotPresent(int_64 errorcode)
{
	if (debugger_logging()) //Are we logging?
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
		CPU_executionphase_startinterrupt(EXCEPTION_SEGMENTNOTPRESENT,0,errorcode); //Call IVT entry #11 decimal!
		//Execute the interrupt!
	}
}

void CPU_StackFault(int_64 errorcode)
{
	if (debugger_logging()) //Are we logging?
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
		CPU_executionphase_startinterrupt(EXCEPTION_STACKFAULT,0,errorcode); //Call IVT entry #12 decimal!
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
int getLoadedTYPE(SEGDESCRIPTOR_TYPE *loadeddescriptor)
{
	return GENERALSEGMENT_S(loadeddescriptor->desc)?EXECSEGMENT_ISEXEC(loadeddescriptor->desc):2; //Executable or data, else System?
}

int isGateDescriptor(SEGDESCRIPTOR_TYPE *loadeddescriptor) //0=Fault, 1=Gate, -1=System Segment descriptor, 2=Normal segment descriptor.
{
	if (getLoadedTYPE(loadeddescriptor)==2) //System?
	{
		switch (GENERALSEGMENT_TYPE(loadeddescriptor->desc))
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

void THROWDESCSP(word segmentval, byte external, byte tbl)
{
	CPU_StackFault((external&1)|(segmentval&(0xFFF8))|((tbl&0x3)<<1)); //#StackFault with an error in the LDT/GDT (index@bits 3-15)!
}

void THROWDESCNP(word segmentval, byte external, byte tbl)
{
	CPU_SegNotPresent((external&1)|(segmentval&(0xFFF8))|((tbl&0x3)<<1)); //#SegFault with an error in the LDT/GDT (index@bits 3-15)!
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

sbyte LOADDESCRIPTOR(int segment, word segmentval, SEGDESCRIPTOR_TYPE *container, word isJMPorCALL) //Result: 0=#GP, 1=container=descriptor.
{
	int result;
	uint_32 descriptor_address = 0;
	descriptor_address = (segmentval & 4) ? CPU[activeCPU].SEG_base[CPU_SEGMENT_LDTR] : CPU[activeCPU].registers->GDTR.base; //LDT/GDT selector!

	uint_32 descriptor_index=segmentval; //The full index within the descriptor table!
	descriptor_index &= ~0x7; //Clear bits 0-2 for our base index into the table!

	byte isNULLdescriptor = 0;

	if (((segmentval&~3)==0) && (segment==CPU_SEGMENT_LDTR)) //LDT loaded with the reserved GDT NULL descriptor?
	{
		memset(&container->descdata,0,sizeof(container->descdata)); //Load an invalid LDTR, which is marked invalid!
		isNULLdescriptor = 1; //Special reserved NULL descriptor loading valid!
	}
	else //Try to load a normal segment descriptor!
	{
		if ((segmentval&4) && (GENERALSEGMENT_P(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR])==0)) //Invalid LDT segment?
		{
			return 0; //Abort: invalid LDTR to use!
		}
		if ((word)(descriptor_index|0x7)>((segmentval & 4) ? (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_low | (SEGDESC_NONCALLGATE_LIMIT_HIGH(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR]) << 16)) : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
		{
			return 0; //Not present: limit exceeded!
		}
	
		isNULLdescriptor = 0; //Default: not a NULL descriptor!
		if ((segmentval&~3)==0) //NULL descriptor?
		{
			isNULLdescriptor = 1; //NULL descriptor!
			if ((segment==CPU_SEGMENT_CS) || (segment==CPU_SEGMENT_SS)) //NULL segment loaded into CS or SS?
			{
				return 0; //Not present: limit exceeded!	
			}
			//Otherwise, don't load the descriptor from memory, just clear valid bit!
		}
	
		descriptor_address += descriptor_index; //Add the index multiplied with the width(8 bytes) to get the descriptor!

		if (isNULLdescriptor==0) //Not special NULL descriptor handling?
		{
			int i;
			for (i=0;i<(int)sizeof(container->descdata);++i)
			{
				if (checkDirectMMUaccess(descriptor_address++,1,/*getCPL()*/ 0)) //Error in the paging unit?
				{
					return -1; //Error out!
				}
			}
			descriptor_address -= sizeof(container->descdata); //Restore start address!
			for (i=0;i<(int)sizeof(container->descdata);) //Process the descriptor data!
			{
				if (memory_readlinear(descriptor_address++,&container->descdata[i++])) //Read a descriptor byte directly from flat memory!
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
		else //NULL descriptor to DS/ES/FS/GS segment registers? Don't load the descriptor from memory!
		{
			memcpy(container,&CPU[activeCPU].SEG_DESCRIPTOR[segment],sizeof(*container)); //Copy the old value!
			container->desc.AccessRights &= 0x7F; //Clear the present flag in the descriptor itself!
		}
	}

	if (segment == CPU_SEGMENT_LDTR) //Loading a LDT with no LDT entry used?
	{
		if (segmentval & 4) //We're not loading from the GDT?
		{
			return 0; //Not present: limit exceeded!
		}
		if ((GENERALSEGMENT_TYPE(container->desc) != AVL_SYSTEM_LDT) && (!isNULLdescriptor)) //We're not an LDT while not a NULL LDTR?
		{
			return 0; //Not present: limit exceeded!
		}
	}
	
	if ((segment==CPU_SEGMENT_SS) && //SS is...
		((getLoadedTYPE(container)==1) || //An executable segment? OR
		(!getLoadedTYPE(container) && (DATASEGMENT_W(container->desc)==0)) || //Read-only DATA segment? OR
		(((getCPL()!=GENERALSEGMENT_DPL(container->desc)) && ((isJMPorCALL&0x80)==0))) //Not the same privilege(when checking for privilege) as CPL?
		)
		)
	{
		return 0; //Not present: limit exceeded!	
	}

	if(GENERALSEGMENT_P(container->desc) && (getLoadedTYPE(container) != 2) && (CODEDATASEGMENT_A(container->desc) == 0) && ((isJMPorCALL&0x100)==0)) //Non-accessed loaded and needs to be set? Our reserved bit 8 in isJMPorCALL tells us not to cause writeback for accessed!
	{
		container->desc.AccessRights |= 1; //Set the accessed bit!
		if ((result = SAVEDESCRIPTOR(segment, segmentval, container, isJMPorCALL))<=0) //Trigger writeback and errored out?
		{
			return result; //Error out!
		}
	}

	return 1; //OK!
}

//Result: 1=OK, 0=Error!
sbyte SAVEDESCRIPTOR(int segment, word segmentval, SEGDESCRIPTOR_TYPE *container, byte isJMPorCALL)
{
	uint_32 descriptor_address = 0;
	descriptor_address = (segmentval & 4) ? CPU[activeCPU].SEG_base[CPU_SEGMENT_LDTR] : CPU[activeCPU].registers->GDTR.base; //LDT/GDT selector!
	uint_32 descriptor_index = segmentval; //The full index within the descriptor table!
	descriptor_index &= ~0x7; //Clear bits 0-2 for our base index into the table!

	if ((segmentval&~3)==0) return 0; //Don't write the reserved NULL GDT entry, which isn't to be used!

	if ((segmentval&4) && (GENERALSEGMENT_P(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR])==0)) //Invalid LDT segment?
	{
		return 0; //Abort!
	}

	if ((word)(descriptor_index | 0x7) > ((segmentval & 4) ? (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_low | (SEGDESC_NONCALLGATE_LIMIT_HIGH(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR]) << 16)) : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
	{
		return 0; //Not present: limit exceeded!
	}

	if ((!getDescriptorIndex(descriptor_index)) && ((segment == CPU_SEGMENT_CS) || ((segment == CPU_SEGMENT_SS))) && ((segmentval&4)==0)) //NULL GDT segment loaded into CS or SS?
	{
		return 0; //Not present: limit exceeded!	
	}

	descriptor_address += descriptor_index; //Add the index multiplied with the width(8 bytes) to get the descriptor!

	SEGDESCRIPTOR_TYPE tempcontainer;
	if (EMULATED_CPU == CPU_80286) //80286 has less options?
	{
		if (LOADDESCRIPTOR(segment,segmentval,&tempcontainer,(isJMPorCALL&0xFF)|0x100)) //Loaded the old container?
		{
			container->desc.base_high = tempcontainer.desc.base_high; //No high byte is present, so ignore the data to write!
			container->desc.noncallgate_info = ((container->desc.noncallgate_info&~0xF)|(tempcontainer.desc.noncallgate_info&0xF)); //No high limit is present, so ignore the data to write!
		}
		//Don't handle any errors on descriptor loading!
	}

	//Patch back to memory values!
	container->desc.limit_low = DESC_16BITS(container->desc.limit_low);
	container->desc.base_low = DESC_16BITS(container->desc.base_low);

	int i;
	for (i = 0;i<(int)sizeof(container->descdata);++i) //Process the descriptor data!
	{
		if (checkDirectMMUaccess(descriptor_address++,0,/*getCPL()*/ 0)) //Error in the paging unit?
		{
			return -1; //Error out!
		}
	}
	descriptor_address -= sizeof(container->descdata);

	for (i = 0;i<(int)sizeof(container->descdata);) //Process the descriptor data!
	{
		if (memory_writelinear(descriptor_address++,container->descdata[i++])) //Read a descriptor byte directly from flat memory!
		{
			return 0; //Failed to load the descriptor!
		}
	}
	return 1; //OK!
}


uint_32 destEIP; //Destination address for CS JMP instruction!

/*

getsegment_seg: Gets a segment, if allowed.
parameters:
	whatsegment: What segment is used?
	segment: The segment to get.
	isJMPorCALL: 0 for normal segment setting. 1 for JMP, 2 for CALL, 3 for IRET.
result:
	The segment when available, NULL on error or disallow.

*/

SEGMENT_DESCRIPTOR *getsegment_seg(int segment, SEGMENT_DESCRIPTOR *dest, word *segmentval, byte isJMPorCALL, byte *isdifferentCPL) //Get this corresponding segment descriptor (or LDT. For LDT, specify LDT register as segment) for loading into the segment descriptor cache!
{
	//byte newCPL = getCPL(); //New CPL after switching! Default: no change!
	SEGDESCRIPTOR_TYPE LOADEDDESCRIPTOR, GATEDESCRIPTOR; //The descriptor holder/converter!
	word originalval=*segmentval; //Back-up of the original segment value!
	byte allowNP; //Allow #NP to be used?
	sbyte loadresult;

	if ((*segmentval&4) && (GENERALSEGMENT_P(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR])==0) && (segment!=CPU_SEGMENT_LDTR)) //Invalid LDT segment and LDT is addressed?
	{
		THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return NULL; //We're an invalid TSS to execute!
	}

	if ((loadresult = LOADDESCRIPTOR(segment,*segmentval,&LOADEDDESCRIPTOR,isJMPorCALL))<=0) //Error loading current descriptor?
	{
		if (loadresult == 0) //Not already faulted?
		{
			THROWDESCGP(*segmentval, 1, (*segmentval & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Throw #GP error!
		}
		return NULL; //Error, by specified reason!
	}
	allowNP = ((segment==CPU_SEGMENT_DS) || (segment==CPU_SEGMENT_ES) || (segment==CPU_SEGMENT_FS) || (segment==CPU_SEGMENT_GS)); //Allow segment to be marked non-present(exception: values 0-3 with data segments)?
	byte equalprivilege = 0; //Special gate stuff requirement: DPL must equal CPL? 1 for enable, 0 for normal handling.
	byte privilegedone = 0; //Privilege already calculated?
	byte is_gated = 0;
	byte is_TSS = 0; //Are we a TSS?
	byte callgatetype = 0; //Default: no call gate!

	if (((*segmentval&~3)==0)) //NULL GDT segment when not allowed?
	{
		if (segment==CPU_SEGMENT_LDTR) //in LDTR? We're valid!
		{
			memset(&LOADEDDESCRIPTOR,0,sizeof(LOADEDDESCRIPTOR)); //Allow!
			goto validLDTR; //Skip all checks, and check out as valid! We're allowed on the LDTR only!
		}
		else //Skip checks: we're invalid to check any further!
		{
			if ((segment==CPU_SEGMENT_CS) || (segment==CPU_SEGMENT_TR) || (segment==CPU_SEGMENT_SS)) //Not allowed?
			{
				THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP error!
				return NULL; //Error, by specified reason!
			}
			else if (allowNP)
			{
				memset(&LOADEDDESCRIPTOR,0,sizeof(LOADEDDESCRIPTOR)); //Allow!
				goto validLDTR; //Load NULL descriptor!
			}
		}
	}

	if (GENERALSEGMENT_P(LOADEDDESCRIPTOR.desc)==0) //Not present loaded into non-data segment register?
	{
		if (segment==CPU_SEGMENT_SS) //Stack fault?
		{
			THROWDESCSP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Stack fault!
		}
		else
		{
			THROWDESCNP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		}
		return NULL; //We're an invalid TSS to execute!
	}

	if (isGateDescriptor(&LOADEDDESCRIPTOR)==0) //Invalid descriptor?
	{
		THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP error!
		return NULL; //We're an invalid descriptor to use!
	}

	if ((isGateDescriptor(&LOADEDDESCRIPTOR)==1) && (segment == CPU_SEGMENT_CS) && isJMPorCALL) //Handling of gate descriptors?
	{
		is_gated = 1; //We're gated!
		memcpy(&GATEDESCRIPTOR, &LOADEDDESCRIPTOR, sizeof(GATEDESCRIPTOR)); //Copy the loaded descriptor to the GATE!
		//Check for invalid loads!
		switch (GENERALSEGMENT_TYPE(GATEDESCRIPTOR.desc))
		{
		default: //Unknown type?
		case AVL_SYSTEM_INTERRUPTGATE16BIT:
		case AVL_SYSTEM_TRAPGATE16BIT:
		case AVL_SYSTEM_INTERRUPTGATE32BIT:
		case AVL_SYSTEM_TRAPGATE32BIT:
			//We're an invalid gate!
			THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP error!		
			return NULL; //Not present: invalid descriptor type loaded!
			break;
		case AVL_SYSTEM_TASKGATE: //Task gate?
		case AVL_SYSTEM_CALLGATE16BIT:
		case AVL_SYSTEM_CALLGATE32BIT:
			//Valid gate! Allow!
			break;
		}
		if ((MAX(getCPL(), getRPL(*segmentval)) > GENERALSEGMENT_DPL(GATEDESCRIPTOR.desc)) && (isJMPorCALL!=3)) //Gate has too high a privilege level? Only when not an IRET(always allowed)!
		{
			THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
			return NULL; //We are a lower privilege level, so don't load!				
		}
		*segmentval = (GATEDESCRIPTOR.desc.selector & ~3) | (*segmentval & 3); //We're loading this segment now, with requesting privilege!
		if ((loadresult = LOADDESCRIPTOR(segment, *segmentval, &LOADEDDESCRIPTOR,isJMPorCALL))<=0) //Error loading current descriptor?
		{
			if (loadresult == 0) //Not faulted already?
			{
				THROWDESCGP(*segmentval, 1, (*segmentval & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Throw error!
			}
			return NULL; //Error, by specified reason!
		}
		if (isGateDescriptor(&LOADEDDESCRIPTOR)==0) //Invalid descriptor?
		{
			THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP error!
			return NULL; //We're an invalid descriptor to use!
		}
		privilegedone = 1; //Privilege has been precalculated!
		if (GENERALSEGMENT_TYPE(GATEDESCRIPTOR.desc) == AVL_SYSTEM_TASKGATE) //Task gate?
		{
			if (segment != CPU_SEGMENT_CS) //Not code? We're not a task switch! We're trying to load the task segment into a data register. This is illegal! TR doesn't support Task Gates directly(hardware only)!
			{
				THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
				return NULL; //Don't load!
			}
		}
		else //Normal descriptor?
		{
			if ((isJMPorCALL == 1) && !EXECSEGMENT_C(LOADEDDESCRIPTOR.desc)) //JMP to a nonconforming segment?
			{
				if (GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc) != getCPL()) //Different CPL?
				{
					THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
					return NULL; //We are a different privilege level, so don't load!						
				}
			}
			else if (isJMPorCALL) //Call instruction (or JMP instruction to a conforming segment)
			{
				if (GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc) > getCPL()) //We have a lower CPL?
				{
					THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
					return NULL; //We are a different privilege level, so don't load!
				}
			}
		}
	}
	else if ((isGateDescriptor(&LOADEDDESCRIPTOR)==-1) && (segment==CPU_SEGMENT_CS) && isJMPorCALL) //JMP/CALL to non-gate descriptor(and not a system segment)?
	{
		equalprivilege = 1; //Enforce equal privilege!
	}

	//Final descriptor safety check!
	if (isGateDescriptor(&LOADEDDESCRIPTOR)==0) //Invalid descriptor?
	{
		THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP error!
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
		((getLoadedTYPE(&LOADEDDESCRIPTOR)==1) && (EXECSEGMENT_R(LOADEDDESCRIPTOR.desc)==0)) //An execute-only code segment?
		)
		)
	{
		THROWDESCGP(originalval,1,(originalval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return NULL; //Not present: limit exceeded!	
	}
	
	switch (GENERALSEGMENT_TYPE(LOADEDDESCRIPTOR.desc)) //We're a TSS? We're to perform a task switch!
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

	if (is_TSS && (segment==CPU_SEGMENT_CS) && (isJMPorCALL==3)) //IRET allowed regardless of privilege?
	{
		privilegedone = 1; //Allow us always!
	}

	//Now check for CPL,DPL&RPL! (chapter 6.3.2)
	if (
		(
		(!privilegedone && !equalprivilege && (MAX(getCPL(),getRPL(*segmentval))>GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc)) && ((!(EXECSEGMENT_ISEXEC(LOADEDDESCRIPTOR.desc) && EXECSEGMENT_C(LOADEDDESCRIPTOR.desc) && getLoadedTYPE(&LOADEDDESCRIPTOR)==1)))) || //We are a lower privilege level with either non-conforming or a data/system segment descriptor?
		((!privilegedone && equalprivilege && MAX(getCPL(),getRPL(*segmentval))!=GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc)) && //We must be at the same privilege level?
			!(EXECSEGMENT_C(LOADEDDESCRIPTOR.desc)) //Not conforming checking further ahead makes sure that we don't double check things?
			)
		)
		&& (!((isJMPorCALL==3) && is_TSS)) //No privilege checking is done on IRET through TSS!
		)
	{
		THROWDESCGP(originalval,1,(originalval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return NULL; //We are a lower privilege level, so don't load!
	}

	if (is_TSS && (segment==CPU_SEGMENT_TR)) //We're a TSS loading into TR? We're to perform a task switch!
	{
		if (*segmentval & 2) //LDT lookup set?
		{
			THROWDESCGP(originalval,1,(originalval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
			return NULL; //We're an invalid TSS to call!
		}
		//Handle the task switch normally! We're allowed to use the TSS!
	}

	if ((GENERALSEGMENT_P(LOADEDDESCRIPTOR.desc)==0) && ((segment==CPU_SEGMENT_CS) || (segment==CPU_SEGMENT_SS) || (segment==CPU_SEGMENT_TR) || (*segmentval&~3))) //Not present loaded into non-data register?
	{
		if (segment==CPU_SEGMENT_SS) //Stack fault?
		{
			THROWDESCSP(originalval,1,(originalval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		}
		else
		{
			THROWDESCNP(originalval,1,(originalval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		}
		return NULL; //We're an invalid TSS to execute!
	}

	if ((segment==CPU_SEGMENT_CS) && is_TSS) //Special stuff on CS, CPL, Task switch.
	{
		//Execute a normal task switch!
		if (CPU_executionphase_starttaskswitch(segment,&LOADEDDESCRIPTOR,segmentval,*segmentval,isJMPorCALL,is_gated,-1)) //Switching to a certain task?
		{
			return NULL; //Error changing priviledges or anything else!
		}

		//We've properly switched to the destination task! Continue execution normally!
		return NULL; //Don't actually load CS with the descriptor: we've caused a task switch after all!
	}

	if ((segment == CPU_SEGMENT_CS) && (is_gated==0)) //Special stuff on normal CS register (conforming?), CPL.
	{
		if (EXECSEGMENT_C(LOADEDDESCRIPTOR.desc)) //Conforming segment?
		{
			if ((!privilegedone) && (GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc)<MAX(getCPL(),getRPL(*segmentval)))) //Target DPL must be less-or-equal to the CPL.
			{
				THROWDESCGP(originalval,1,(originalval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
				return NULL; //We are a lower privilege level, so don't load!				
			}
		}
		else //Non-conforming segment?
		{
			if ((!privilegedone) && (GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc)!=MAX(getCPL(),getRPL(*segmentval)))) //Check for equal only when using Gate Descriptors?
			{
				THROWDESCGP(originalval,1,(originalval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
				return NULL; //We are a lower privilege level, so don't load!				
			}
		}
	}

	if ((segment == CPU_SEGMENT_CS) && (isGateDescriptor(&GATEDESCRIPTOR) == 1) && (is_gated)) //Gated CS?
	{
		switch (GENERALSEGMENT_TYPE(GATEDESCRIPTOR.desc)) //What type of gate are we using?
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
			destEIP = GATEDESCRIPTOR.desc.callgate_base_low; //16-bit EIP!
			if (callgatetype == 2) //32-bit destination?
			{
				destEIP |= (GATEDESCRIPTOR.desc.callgate_base_mid<<16); //Mid EIP!
				destEIP |= (GATEDESCRIPTOR.desc.callgate_base_high<<24); //High EIP!
			}
			uint_32 argument; //Current argument to copy to the destination stack!
			word arguments;
			CPU[activeCPU].CallGateParamCount = 0; //Clear our stack to transfer!
			CPU[activeCPU].CallGateSize = (callgatetype==2)?1:0; //32-bit vs 16-bit call gate!

			if ((GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc)!=getCPL()) && (isJMPorCALL==2)) //Stack switch required (with CALL only)?
			{
				//Backup the old stack data!
				/*
				CPU[activeCPU].have_oldESP = 1;
				CPU[activeCPU].have_oldSS = 1;
				CPU[activeCPU].oldESP = REG_ESP; //Backup!
				CPU[activeCPU].oldSS = REG_SS; //Backup!
				*/ //Dont automatically at the start of the instruction!
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

				CPU[activeCPU].CPL = GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc); //Changing privilege to this!
			}
			else
			{
				*isdifferentCPL = 2; //Indicate call gate determines operand size!
			}
		}
	}

	//Handle invalid types now!
	if ((segment==CPU_SEGMENT_CS) &&
		(getLoadedTYPE(&LOADEDDESCRIPTOR)!=1) //Data or System in CS (non-exec)?
		)
	{
		THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP error!		
		return NULL; //Not present: invalid descriptor type loaded!
	}
	else if ((getLoadedTYPE(&LOADEDDESCRIPTOR)==1) && (segment!=CPU_SEGMENT_CS) && (EXECSEGMENT_R(LOADEDDESCRIPTOR.desc)==0)) //Executable non-readable in non-executable segment?
	{
		THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP error!		
		return NULL; //Not present: invalid descriptor type loaded!
	}
	else if ((getLoadedTYPE(&LOADEDDESCRIPTOR)==1) && ((segment==CPU_SEGMENT_LDTR) || (segment==CPU_SEGMENT_TR))) //Executable segment loaded invalid?
	{
		THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP error!		
		return NULL; //Not present: invalid descriptor type loaded!
	}
	else if (getLoadedTYPE(&LOADEDDESCRIPTOR)==0) //Data descriptor loaded?
	{
		if (((segment!=CPU_SEGMENT_DS) && (segment!=CPU_SEGMENT_ES) && (segment!=CPU_SEGMENT_FS) && (segment!=CPU_SEGMENT_GS) && (segment!=CPU_SEGMENT_SS))) //Data descriptor in invalid type?
		{
			THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP error!		
			return NULL; //Not present: invalid descriptor type loaded!
		}
		if ((DATASEGMENT_W(LOADEDDESCRIPTOR.desc)==0) && (segment==CPU_SEGMENT_SS)) //Non-writable SS segment?
		{
			THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP error!		
			return NULL; //Not present: invalid descriptor type loaded!
		}
	}
	else if (getLoadedTYPE(&LOADEDDESCRIPTOR)==2) //System descriptor loaded?
	{
		if ((segment==CPU_SEGMENT_CS) || (segment==CPU_SEGMENT_DS) || (segment==CPU_SEGMENT_ES) || (segment==CPU_SEGMENT_FS) || (segment==CPU_SEGMENT_GS) || (segment==CPU_SEGMENT_SS)) //System descriptor in invalid register?
		{
			THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP error!		
			return NULL; //Not present: invalid descriptor type loaded!
		}
		if ((segment==CPU_SEGMENT_LDTR) && (GENERALSEGMENT_TYPE(LOADEDDESCRIPTOR.desc)!=AVL_SYSTEM_LDT)) //Invalid LDT load?
		{
			THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP error!		
			return NULL; //Not present: invalid descriptor type loaded!
		}
		if ((segment==CPU_SEGMENT_TR) && (is_TSS==0)) //Non-TSS into task register?
		{
			THROWDESCGP(*segmentval,1,(*segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP error!		
			return NULL; //Not present: invalid descriptor type loaded!
		}
	}

	if (segment==CPU_SEGMENT_CS) //We need to reload a new CPL?
	{
		//Non-gates doesn't change RPL(CPL) of CS! (IRET?&)RETF does change CPL here(already done beforehand)!
		if ((is_gated==0) && (isJMPorCALL==4)) //RETF changes privilege level?
		{
			if (getRPL(*segmentval)>getCPL()) //Lowered?
			{
				CPU[activeCPU].CPL = getRPL(*segmentval); //New CPL, lowered!
				setRPL(*segmentval,CPU[activeCPU].CPL); //Only gated loads(CALL gates) can change RPL(active lowest CPL in CS). Otherwise, it keeps the old RPL.
				setRPL(originalval,CPU[activeCPU].CPL); //Only gated loads(CALL gates) can change RPL(active lowest CPL in CS). Otherwise, it keeps the old RPL.
			}
		}
	}

	validLDTR:
	memcpy(dest,&LOADEDDESCRIPTOR,sizeof(LOADEDDESCRIPTOR)); //Give the loaded descriptor!

	return dest; //Give the segment descriptor read from memory!
}
word segmentWritten_tempSS;
extern word RETF_popbytes; //How many to pop?

byte RETF_checkSegmentRegisters[4] = {CPU_SEGMENT_ES,CPU_SEGMENT_FS,CPU_SEGMENT_GS,CPU_SEGMENT_DS}; //All segment registers to check for when returning to a lower privilege level!

byte segmentWritten(int segment, word value, byte isJMPorCALL) //A segment register has been written to!
{
	byte RETF_segmentregister=0,RETF_whatsegment; //A segment register we're checking during a RETF instruction!
	byte oldCPL= getCPL();
	byte isDifferentCPL;
	uint_32 tempesp;
	if (getcpumode()==CPU_MODE_PROTECTED) //Protected mode, must not be real or V8086 mode, so update the segment descriptor cache!
	{
		isDifferentCPL = 0; //Default: same CPL!
		SEGMENT_DESCRIPTOR tempdescriptor;
		SEGMENT_DESCRIPTOR *descriptor = getsegment_seg(segment,&tempdescriptor,&value,isJMPorCALL,&isDifferentCPL); //Read the segment!
		uint_32 stackval;
		word stackval16; //16-bit stack value truncated!
		if (descriptor) //Loaded&valid?
		{
			if ((segment == CPU_SEGMENT_CS) && ((isJMPorCALL == 2) || (isJMPorCALL==1))) //JMP(with call gate)/CALL needs pushed data on the stack?
			{
				if ((isDifferentCPL==1) && (isJMPorCALL == 2)) //Stack switch is required with CALL only?
				{
					//TSSSize = 0; //Default to 16-bit TSS!
					switch (GENERALSEGMENT_TYPE(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR])) //What kind of TSS?
					{
					case AVL_SYSTEM_BUSY_TSS32BIT:
					case AVL_SYSTEM_TSS32BIT:
						//TSSSize = 1; //32-bit TSS!
					case AVL_SYSTEM_BUSY_TSS16BIT:
					case AVL_SYSTEM_TSS16BIT:
						if (switchStacks(GENERALSEGMENTPTR_DPL(descriptor))) return 1; //Abort failing switching stacks!
						
						if (checkStackAccess(2,1,CODE_SEGMENT_DESCRIPTOR_D_BIT())) return 1; //Abort on error!

						if (/*CPU_Operand_size[activeCPU]*/ CPU[activeCPU].CallGateSize)
						{
							CPU_PUSH32(&CPU[activeCPU].oldESP);
						}
						else
						{
							word temp=(word)(CPU[activeCPU].oldESP&0xFFFF);
							CPU_PUSH16(&temp,0);
						}
						CPU_PUSH16(&CPU[activeCPU].oldSS,CPU[activeCPU].CallGateSize); //SS to return!

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
					}
				}
				else if (isDifferentCPL==0) //Unchanging CPL? Take call size from operand size!
				{
					CPU[activeCPU].CallGateSize = CPU_Operand_size[activeCPU]; //Use the call instruction size!
				}
				//Else, call by call gate size!
				
				if (isJMPorCALL==2) //CALL pushes return address!
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

				if (hascallinterrupttaken_type==0xFF) //Not set yet?
				{
					if (isDifferentCPL==1) //Different CPL?
					{
						hascallinterrupttaken_type = CALLGATE_NUMARGUMENTS?CALLGATE_DIFFERENTLEVEL_XPARAMETERS:CALLGATE_DIFFERENTLEVEL_NOPARAMETERS; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task gate flag. Left at 0xFF when nothing is used(unknown case?)
					}
					else //Same CPL call gate?
					{
						hascallinterrupttaken_type = CALLGATE_SAMELEVEL; //Same level call gate!
					}
				}
			}
			else if ((segment == CPU_SEGMENT_CS) && (isJMPorCALL == 4)) //RETF needs popped data on the stack?
			{
				if (STACK_SEGMENT_DESCRIPTOR_B_BIT())
				{
					REG_ESP += RETF_popbytes; //Process ESP!
				}
				else
				{
					REG_SP += RETF_popbytes; //Process SP!
				}

				if (oldCPL<getRPL(value)) //CPL changed?
				{
					//Privilege change!
					//Backup the old stack data!
					/*
					CPU[activeCPU].have_oldESP = 1;
					CPU[activeCPU].have_oldSS = 1;
					CPU[activeCPU].oldESP = REG_ESP; //Backup!
					CPU[activeCPU].oldSS = REG_SS; //Backup!
					*/ //Dont automatically at the start of an instruction!

					CPU[activeCPU].CPL = getRPL(value); //New privilege level!

					//Now, return to the old prvilege level!
					hascallinterrupttaken_type = RET_DIFFERENTLEVEL; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task
					if (CPU8086_POPw(6,&segmentWritten_tempSS,CPU_Operand_size[activeCPU])) return 1; //POPped?
					if (segmentWritten(CPU_SEGMENT_SS,segmentWritten_tempSS,0)) return 1; //Back to our calling stack!
					if (/*CPU_Operand_size[activeCPU]*/ CPU_Operand_size[activeCPU])
					{
						if (CPU80386_POPdw(8,&REG_ESP)) return 1; //POP ESP!
					}
					else
					{
						if (CPU8086_POPw(8,&REG_SP,0)) return 1; //POP SP!
						REG_ESP &= 0xFFFF; //Only keep what we need!
					}

					RETF_segmentregister = 1; //We're checking the segments for privilege changes to be invalidated!
				}
				else //Same privilege? (E)SP on the destination stack is already processed, don't process again!
				{
					RETF_popbytes = 0; //Nothing to pop anymore!
				}
			}
			else if ((segment==CPU_SEGMENT_CS) && (isJMPorCALL==3)) //IRET might need extra data popped?
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
			}

			if (segment==CPU_SEGMENT_TR) //Loading the Task Register? We're to mask us as busy!
			{
				if (isJMPorCALL==0) //Not a JMP or CALL itself, or a task switch, so just a plain load using LTR?
				{
					SEGDESCRIPTOR_TYPE savedescriptor;
					switch (GENERALSEGMENT_TYPE(tempdescriptor)) //What kind of TSS?
					{
					case AVL_SYSTEM_BUSY_TSS32BIT:
					case AVL_SYSTEM_BUSY_TSS16BIT:
						THROWDESCGP(value,1,(value&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //We cannot load a busy TSS!
						return 1; //Abort on fault!
						break;
					case AVL_SYSTEM_TSS32BIT:
					case AVL_SYSTEM_TSS16BIT:
						tempdescriptor.AccessRights |= 2; //Mark not idle in the RAM descriptor!
						savedescriptor.DATA64 = tempdescriptor.DATA64; //Copy the resulting descriptor contents to our buffer for writing to RAM!
						if (SAVEDESCRIPTOR(segment,value,&savedescriptor,isJMPorCALL)<=0) //Save it back to RAM failed?
						{
							return 1; //Abort on fault!
						}
						break;
					default: //Invalid segment descriptor to load into the TR register?
						THROWDESCGP(value,1,(value&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //We cannot load a busy TSS!
						return 1; //Abort on fault!
						break; //Ignore!
					}
				}
			}
			//Now, load the new descriptor and address for CS if needed(with secondary effects)!
			memcpy(&CPU[activeCPU].SEG_DESCRIPTOR[segment],descriptor,sizeof(CPU[activeCPU].SEG_DESCRIPTOR[segment])); //Load the segment descriptor into the cache!
			CPU[activeCPU].SEG_base[segment] = ((CPU[activeCPU].SEG_DESCRIPTOR[segment].base_high<<24)|(CPU[activeCPU].SEG_DESCRIPTOR[segment].base_mid<<16)|CPU[activeCPU].SEG_DESCRIPTOR[segment].base_low); //Update the base address!
			//if (memprotect(CPU[activeCPU].SEGMENT_REGISTERS[segment],2,"CPU_REGISTERS")) //Valid segment register?
			{
				*CPU[activeCPU].SEGMENT_REGISTERS[segment] = value; //Set the segment register to the allowed value!
			}
			if (segment == CPU_SEGMENT_CS) //CS register?
			{
				CPU[activeCPU].registers->EIP = destEIP; //The current OPCode: just jump to the address specified by the descriptor OR command!
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
						if ((word)(descriptor_index | 0x7) > ((*CPU[activeCPU].SEGMENT_REGISTERS[RETF_whatsegment] & 4) ? (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_low | (SEGDESC_NONCALLGATE_LIMIT_HIGH(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR]) << 16)) : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
						{
						invalidRETFsegment:
							//Selector and Access rights are zeroed!
							*CPU[activeCPU].SEGMENT_REGISTERS[RETF_whatsegment] = 0; //Zero the register!
							if (isJMPorCALL == 3) //IRET?
							{
								CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment].AccessRights &= 0x7F; //Clear the valid flag only with IRET!
							}
							else //RETF?
							{
								CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment].AccessRights = 0; //Invalid!
							}
							continue; //Next register!
						}
					}
					if (GENERALSEGMENT_P(CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment])) //Not present?
					{
						goto invalidRETFsegment; //Next register!
					}
					if (GENERALSEGMENT_S(CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment])) //Not data/readable code segment?
					{
						goto invalidRETFsegment; //Next register!
					}
					//We're either data or code!
					if (EXECSEGMENT_ISEXEC(CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment])) //Code?
					{
						if (!EXECSEGMENT_C(CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment])) //Nonconforming? Invalid!
						{
							goto invalidRETFsegment; //Next register!
						}
						if (!EXECSEGMENT_R(CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment])) //Not readable? Invalid!
						{
							goto invalidRETFsegment; //Next register!
						}
					}
					//We're either data or readable, conforming code!
					if (GENERALSEGMENT_DPL(CPU[activeCPU].SEG_DESCRIPTOR[RETF_whatsegment])<MAX(getCPL(),getRPL(*CPU[activeCPU].SEGMENT_REGISTERS[RETF_whatsegment]))) //Not privileged enough to handle said segment descriptor?
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
		if (isJMPorCALL == 2) //CALL needs pushed data?
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
		//if (memprotect(CPU[activeCPU].SEGMENT_REGISTERS[segment],2,"CPU_REGISTERS")) //Valid segment register?
		{
			*CPU[activeCPU].SEGMENT_REGISTERS[segment] = value; //Just set the segment, don't load descriptor!
			//Load the correct base data for our loading!
			CPU[activeCPU].SEG_DESCRIPTOR[segment].base_low = (word)(((uint_32)value<<4)&0xFFFF); //Low base!
			CPU[activeCPU].SEG_DESCRIPTOR[segment].base_mid = ((((uint_32)value << 4) & 0xFF0000)>>16); //Mid base!
			CPU[activeCPU].SEG_DESCRIPTOR[segment].base_high = ((((uint_32)value << 4) & 0xFF000000)>>24); //High base!
			CPU[activeCPU].SEG_base[segment] = ((CPU[activeCPU].SEG_DESCRIPTOR[segment].base_high<<24)|(CPU[activeCPU].SEG_DESCRIPTOR[segment].base_mid<<16)|CPU[activeCPU].SEG_DESCRIPTOR[segment].base_low); //Update the base address!
			//This also maps the resulting segment in low memory (20-bit address space) in real mode, thus CS is pulled low as well!
			//Real mode affects only CS like Virtual 8086 mode(reloading all base/limit values). Other segments are unmodified.
			//Virtual 8086 mode also loads the rights etc.? This is to prevent Virtual 8086 tasks having leftover data in their descriptors, causing faults!
			if ((segment==CPU_SEGMENT_CS) || (getcpumode()==CPU_MODE_8086)) //Only done for the CS segment in real mode as well as all registers in 8086 mode?
			{
				CPU[activeCPU].SEG_DESCRIPTOR[segment].AccessRights = 0x93; //Compatible rights!
				CPU[activeCPU].SEG_DESCRIPTOR[segment].limit_low = 0xFFFF;
				CPU[activeCPU].SEG_DESCRIPTOR[segment].noncallgate_info = 0x00; //Not used!
			}
		}
		if (segment==CPU_SEGMENT_CS) //CS segment? Reload access rights in real mode on first write access!
		{
			CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].AccessRights = 0x93; //Load default access rights!
			CPU[activeCPU].registers->EIP = destEIP; //... The current OPCode: just jump to the address!
			CPU_flushPIQ(-1); //We're jumping to another address!
		}
		else if (segment == CPU_SEGMENT_SS) //SS? We're also updating the CPL!
		{
			updateCPL(); //Update the CPL according to the mode!
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

OPTINLINE byte verifyLimit(SEGMENT_DESCRIPTOR *descriptor, uint_32 offset)
{
	//Execute address test?
	INLINEREGISTER byte isvalid;
	INLINEREGISTER uint_32 limit; //The limit!
	uint_32 limits[2]; //What limit to apply?
	limits[0] = limit = ((SEGDESCPTR_NONCALLGATE_LIMIT_HIGH(descriptor) << 16) | descriptor->limit_low); //Base limit!
	limits[1] = ((limit << 12) | 0xFFF); //4KB for a limit of 4GB, fill lower 12 bits with 1!
	limit = limits[SEGDESCPTR_GRANULARITY(descriptor)]; //Use the appropriate granularity!

	isvalid = (offset<=limit); //Valid address range!
	isvalid ^= ((descriptor->AccessRights&0x1C)==0x14); //Apply expand-down data segment, if required, which reverses valid!
	isvalid &= 1; //Only 1-bit testing!
	return isvalid; //Are we valid?
}

byte CPU_MMU_checkrights_cause = 0; //What cause?
//Used by the CPU(VERR/VERW)&MMU I/O! forreading=0: Write, 1=Read normal, 3=Read opcode
byte CPU_MMU_checkrights(int segment, word segmentval, uint_32 offset, int forreading, SEGMENT_DESCRIPTOR *descriptor, byte addrtest, byte is_offset16)
{
	//First: type checking!

	if (unlikely(GENERALSEGMENTPTR_P(descriptor)==0)) //Not present(invalid in the cache)? This also applies to NULL descriptors!
	{
		CPU_MMU_checkrights_cause = 1; //What cause?
		return 1; //#GP fault: not present in descriptor cache mean invalid, thus #GP!
	}

	//Basic access rights are always checked!
	if (GENERALSEGMENTPTR_S(descriptor)) //System segment? Check for additional type information!
	{
		switch ((descriptor->AccessRights&0xE)) //What type of descriptor(ignore the accessed bit)?
		{
			case 0: //Data, read-only
			case 4: //Data(expand down), read-only
			case 10: //Code, non-conforming, execute/read
			case 14: //Code, conforming, execute/read
				if (unlikely((forreading&~0x10)==0)) //Writing?
				{
					CPU_MMU_checkrights_cause = 3; //What cause?
					return 1; //Error!
				}
				break; //Allow!
			case 2: //Data, read/write
			case 6: //Data(expand down), read/write
				break; //Allow!
			case 8: //Code, non-conforming, execute-only
			case 12: //Code, conforming, execute-only
				if (unlikely((forreading&~0x10)!=3)) //Writing or reading normally?
				{
					CPU_MMU_checkrights_cause = 3; //What cause?
					return 1; //Error!
				}
				break; //Allow!
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

//Used by the MMU! forreading: 0=Writes, 1=Read normal, 3=Read opcode fetch.
int CPU_MMU_checklimit(int segment, word segmentval, uint_32 offset, int forreading, byte is_offset16) //Determines the limit of the segment, forreading=2 when reading an opcode!
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
				if (unlikely((forreading&0x10)==0)) CPU_StackFault(((getcpumode()==CPU_MODE_PROTECTED) || (!(((CPU_MMU_checkrights_cause==6) && (getcpumode()==CPU_MODE_8086)) || (getcpumode()==CPU_MODE_REAL))))?0:-2); //Throw (pseudo) fault when not prefetching!
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
		maplocation = (port>>3); //8 bits per byte!
		mappos = (1<<(port&7)); //The bit within the byte specified!
		mapvalue = 1; //Default to have the value 1!
		if (((GENERALSEGMENT_TYPE(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR]) == AVL_SYSTEM_BUSY_TSS32BIT) || (GENERALSEGMENT_TYPE(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR]) == AVL_SYSTEM_TSS32BIT)) && CPU[activeCPU].registers->TR && GENERALSEGMENT_P(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR])) //Active 32-bit TSS?
		{
			uint_32 limit;
			limit = CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR].limit_low | (SEGDESC_NONCALLGATE_LIMIT_HIGH(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR]) << 16); //The limit of the descriptor!
			maplocation += MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR,0x66,0,!CODE_SEGMENT_DESCRIPTOR_D_BIT()); //Add the map location to the specified address!
			if (maplocation < limit) //Not over the limit? We're an valid entry!
			{
				mapvalue = (MMU_rb(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, maplocation, 0,!CODE_SEGMENT_DESCRIPTOR_D_BIT())&mappos); //We're the bit to use!
			}
		}
		if (mapvalue) //The map bit is set(or not a 32-bit task)? We're to trigger an exception!
		{
			return 1; //Trigger an exception!
		}
	}
	return 0; //Allow all for now!
}

int LOADINTDESCRIPTOR(int segment, word segmentval, SEGDESCRIPTOR_TYPE *container)
{
	uint_32 descriptor_address = 0;
	descriptor_address = (segmentval & 4) ? CPU[activeCPU].SEG_base[CPU_SEGMENT_LDTR] : CPU[activeCPU].registers->GDTR.base; //LDT/GDT selector!

	uint_32 descriptor_index = segmentval; //The full index within the descriptor table!
	descriptor_index &= ~0x7; //Clear bits 0-2 for our base index into the table!

	byte isNULLdescriptor = 0;

	if (((segmentval&~3)==0) && (segment==CPU_SEGMENT_LDTR)) //LDT loaded with the reserved GDT NULL descriptor?
	{
		memset(&container->descdata,0,sizeof(container->descdata)); //Load an invalid LDTR, which is marked invalid!
		isNULLdescriptor = 1; //Special reserved NULL descriptor loading valid!
	}
	else //Try to load a normal segment descriptor!
	{
		if ((segmentval&4) && (GENERALSEGMENT_P(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR])==0)) //Invalid LDT segment?
		{
			return 0; //Abort: invalid LDTR to use!
		}

		if ((word)(descriptor_index | 0x7) > ((segmentval & 4) ? (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_low | (SEGDESC_NONCALLGATE_LIMIT_HIGH(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR]) << 16)) : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
		{
			return 0; //Not present: limit exceeded!
		}

		if ((!getDescriptorIndex(descriptor_index)) && ((segment == CPU_SEGMENT_CS) || ((segment == CPU_SEGMENT_SS))) && ((segmentval&4)==0)) //NULL GDT segment loaded into CS or SS?
		{
			return 0; //Not present: limit exceeded!	
		}

		descriptor_address += descriptor_index; //Add the index multiplied with the width(8 bytes) to get the descriptor!

		int i;
		for (i=0;i<(int)sizeof(container->descdata);++i)
		{
			if (checkDirectMMUaccess(descriptor_address++,1,/*getCPL()*/ 0)) //Error in the paging unit?
			{
				return 1; //Error out!
			}
		}
		descriptor_address -= sizeof(container->descdata); //Restore start address!
		for (i = 0;i<(int)sizeof(container->descdata);) //Process the descriptor data!
		{
			if (memory_readlinear(descriptor_address++,&container->descdata[i++])) //Read a descriptor byte directly from flat memory!
			{
				return 0; //Failed to load the descriptor!
			}
		}

		if (EMULATED_CPU == CPU_80286) //80286 has less options?
		{
			container->desc.base_high = 0; //No high byte is present!
			container->desc.noncallgate_info &= ~0xF; //No high limit is present!
		}
	}

	if ((segment == CPU_SEGMENT_CS) &&
		(
			(getLoadedTYPE(container) != 1) //Not an executable segment?
			|| (isNULLdescriptor) //NULL descriptor loaded? Invalid too!
		)
		)
	{
		return 0; //Not present: limit exceeded!	
	}

	return 1; //OK!
}

extern byte immb; //For CPU_readOP result!

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
		TSS_StackPos += (4<<TSSSize)*newCPL; //Start of the correct TSS (E)SP! 4 for 16-bit TSS, 8 for 32-bit TSS!
		ESPn = TSSSize?MMU_rdw(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,TSS_StackPos,0,!CODE_SEGMENT_DESCRIPTOR_D_BIT()):MMU_rw(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,TSS_StackPos,0,!CODE_SEGMENT_DESCRIPTOR_D_BIT()); //Read (E)SP for the privilege level from the TSS!
		TSS_StackPos += (2<<TSSSize); //Convert the (E)SP location to SS location!
		SSn = MMU_rw(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,TSS_StackPos,0,!CODE_SEGMENT_DESCRIPTOR_D_BIT()); //SS!
		if (segmentWritten(CPU_SEGMENT_SS,SSn,0x80)) return 1; //Read SS, privilege level changes, ignore DPL vs CPL check!
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
	uint_32 errorcode32 = (uint_32)errorcode; //Get the error code itelf!
	word errorcode16 = (word)errorcode; //16-bit variant, if needed!
	SEGDESCRIPTOR_TYPE newdescriptor; //Temporary storage for task switches!
	word desttask; //Destination task for task switches!
	byte left; //The amount of bytes left to read of the IDT entry!
	uint_32 base;
	sbyte loadresult;
	base = (intnr<<3); //The base offset of the interrupt in the IDT!

	if (errorcode<0) //Invalid error code to use?
	{
		errorcode16 = 0; //Empty to log!
		errorcode32 = 0; //Empty to log!
	}

	CPU[activeCPU].executed = 0; //Default: still busy executing!
	if (CPU[activeCPU].faultraised==2) CPU[activeCPU].faultraised = 0; //Clear non-fault, if present!

	if ((base|0x7) > CPU[activeCPU].registers->IDTR.limit) //Limit exceeded?
	{
		THROWDESCGP(base,1,EXCEPTION_TABLE_IDT); //#GP!
		return 0; //Abort!
	}

	base += CPU[activeCPU].registers->IDTR.base; //Add the base for the actual offset into the IDT!
	
	IDTENTRY idtentry; //The loaded IVT entry!
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

	byte is32bit;

	if (IDTENTRY_P(idtentry)==0) //Not present?
	{
		THROWDESCGP(base,1,EXCEPTION_TABLE_IDT); //#NP isn't triggered with IDT entries! #GP is triggered instead!
		return 0;
	}

	if (is_interrupt && (IDTENTRY_DPL(idtentry) < getCPL())) //Not enough rights on software interrupt?
	{
		THROWDESCGP(base,1,EXCEPTION_TABLE_IDT); //#GP!
		return 0;
	}

	//Now, the (gate) descriptor to use is loaded!
	switch (IDTENTRY_TYPE(idtentry)) //What type are we?
	{
	case IDTENTRY_TASKGATE: //32-bit task gate?
		desttask = idtentry.selector; //Read the destination task!
		if (((loadresult = LOADDESCRIPTOR(CPU_SEGMENT_TR, desttask, &newdescriptor,2))<=0) || (desttask&4)) //Error loading new descriptor? The backlink is always at the start of the TSS! It muse also always be in the GDT!
		{
			if (loadresult >= 0) //Not faulted already?
			{
				THROWDESCGP(desttask, 1, (desttask & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Throw #GP error!
			}
			return 0; //Error, by specified reason!
		}
		CPU_executionphase_starttaskswitch(CPU_SEGMENT_TR, &newdescriptor, &CPU[activeCPU].registers->TR, desttask, 2|0x80,1,errorcode); //Execute a task switch to the new task! We're switching tasks like a CALL instruction(https://xem.github.io/minix86/manual/intel-x86-and-64-manual-vol3/o_fe12b1e2a880e0ce-250.html)! We're a call based on an interrupt!
		break;
	default: //All other cases?
		is32bit = ((IDTENTRY_TYPE(idtentry)&IDTENTRY_32BIT_GATEEXTENSIONFLAG)>>IDTENTRY_32BIT_GATEEXTENSIONFLAG_SHIFT); //Enable 32-bit gate?
		switch (IDTENTRY_TYPE(idtentry) & 0x7) //What type are we?
		{
		case IDTENTRY_16BIT_INTERRUPTGATE: //16/32-bit interrupt gate?
		case IDTENTRY_16BIT_TRAPGATE: //16/32-bit trap gate?

			//TODO: Implement V86 monitor support by adding to the stack(saving the old EFLAGS(EFLAGS backup) and clearing the V86 flag before setting the new registers(to make it run in Protected Mode until IRET, which restores EFLAGS/CS/EIP, stack and segment registers by popping them off the stack and restoring the V86 mode to be resumed. Keep EFLAGS as an backup(in CPU restoration structure, like SS:ESP) before clearing the V86 bit(to push) to load protected mode segments for the monitor. Turn off V86 to load all protected mode stack. Push the segments and IRET data on the stack(faults reset EFLAGS/SS/ESP to the backup). Finally load the remaining segments with zero(except SS/CS). Finally, transfer control to CS:EIP of the handler normally.
			//Table can be found at: http://www.read.seas.harvard.edu/~kohler/class/04f-aos/ref/i386/s15_03.htm#fig15-3

			if (!LOADINTDESCRIPTOR(CPU_SEGMENT_CS, idtentry.selector, &newdescriptor)) //Error loading new descriptor? The backlink is always at the start of the TSS!
			{
				THROWDESCGP(idtentry.selector,1,(idtentry.selector&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
				return 0; //Error, by specified reason!
			}
			if (((GENERALSEGMENT_S(newdescriptor.desc)==0) || (EXECSEGMENT_ISEXEC(newdescriptor.desc)==0)) || (EXECSEGMENT_R(newdescriptor.desc)==0)) //Not readable, execute segment or is code/executable segment?
			{
				THROWDESCGP(idtentry.selector,1,(idtentry.selector&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP!
				return 0;
			}

			//Calculate and check the limit!

			if (verifyLimit(&newdescriptor.desc,(idtentry.offsetlow | (idtentry.offsethigh << 16)))==0) //Limit exceeded?
			{
				THROWDESCGP(idtentry.selector,1,(idtentry.selector&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP!
				return 0;
			}

			byte INTTYPE=0;

			if ((EXECSEGMENT_C(newdescriptor.desc) == 0) && (GENERALSEGMENT_DPL(newdescriptor.desc)<getCPL())) //Not enough rights, but conforming?
			{
				INTTYPE = 1; //Interrupt to inner privilege!
			}
			else
			{
				if ((EXECSEGMENT_C(newdescriptor.desc)) || (GENERALSEGMENT_DPL(newdescriptor.desc)==getCPL())) //Not enough rights, but conforming?
				{
					INTTYPE = 2; //Interrupt to same privilege level!
				}
				else
				{
					THROWDESCGP(idtentry.selector,1,(idtentry.selector&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP!
					return 0;
				}
			}

			uint_32 EFLAGSbackup;
			EFLAGSbackup = REG_EFLAGS; //Back-up EFLAGS!

			byte newCPL;
			newCPL = GENERALSEGMENT_DPL(newdescriptor.desc); //New CPL to use!

			if (FLAG_V8 && (INTTYPE==1)) //Virtual 8086 mode to monitor switching to CPL 0?
			{
				#ifdef LOG_VIRTUALMODECALLS
				if (debugger_logging())
				{
					dolog("debugger", "Starting V86 interrupt/fault: INT %02X(%02X(0F:%02X)),immb:%02X,AX=%04X)", intnr, CPU[activeCPU].lastopcode, CPU[activeCPU].lastopcode0F, immb, REG_AX);
				}
				#endif
				if (newCPL!=0) //Not switching to PL0?
				{
					THROWDESCGP(idtentry.selector,1,EXCEPTION_TABLE_GDT); //Exception!
					return 0; //Abort on fault!
				}

				//Now, switch to the new EFLAGS!
				FLAGW_V8(0); //Clear the Virtual 8086 mode flag!
				updateCPUmode(); //Update the CPU mode!

				FLAGW_RF(0); //Clear Resume flag too!

				//We're back in protected mode now!
				CPU[activeCPU].CPL = newCPL; //Apply the new level for the stack load!

				//Switch Stack segment first!
				if (switchStacks(newCPL)) return 1; //Abort failing switching stacks!
				//Verify that the new stack is available!
				if (is32bit) //32-bit gate?
				{
					if (checkStackAccess(9+(((errorcode!=-1) && (errorcode!=-2))?1:0),1,1)) return 0; //Abort on fault!
				}
				else //16-bit gate?
				{
					if (checkStackAccess(9+(((errorcode!=-1) && (errorcode!=-2))?1:0),1,0)) return 0; //Abort on fault!
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
				if (segmentWritten(CPU_SEGMENT_DS,0,0)) return 0; //Clear DS! Abort on fault!
				if (segmentWritten(CPU_SEGMENT_ES,0,0)) return 0; //Clear ES! Abort on fault!
				if (segmentWritten(CPU_SEGMENT_FS,0,0)) return 0; //Clear FS! Abort on fault!
				if (segmentWritten(CPU_SEGMENT_GS,0,0)) return 0; //Clear GS! Abort on fault!
			}
			else if (FLAG_V8) 
			{
				THROWDESCGP(idtentry.selector,1,EXCEPTION_TABLE_GDT); //Exception!
				return 0; //Abort on fault!
			}
			else if ((FLAG_V8==0) && (INTTYPE==1)) //Privilege level changed in protected mode?
			{
				//Unlike the other case, we're still in protected mode!
				//We're back in protected mode now!
				CPU[activeCPU].CPL = newCPL; //Apply the new level for the stack load!

				//Switch Stack segment first!
				if (switchStacks(newCPL)) return 1; //Abort failing switching stacks!

				//Verify that the new stack is available!
				if (checkStackAccess(5+(((errorcode!=-1) && (errorcode!=-2))?1:0),1,is32bit?1:0)) return 0; //Abort on fault!

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
				if (checkStackAccess(3+(((errorcode!=-1) && (errorcode!=-2))?1:0),1,is32bit?1:0)) return 0; //Abort on fault!
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

			memcpy(&CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS], &newdescriptor, sizeof(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS])); //Load the segment descriptor into the cache!
			CPU[activeCPU].SEG_base[CPU_SEGMENT_CS] = ((CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].base_high<<24)|(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].base_mid<<16)|CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].base_low); //Update the base address!
			//if (memprotect(CPU[activeCPU].SEGMENT_REGISTERS[segment],2,"CPU_REGISTERS")) //Valid segment register?
			{
				*CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_CS] = idtentry.selector; //Set the segment register to the allowed value!
			}

			if (INTTYPE==1) CPU[activeCPU].CPL = newCPL; //Privilege level changes!

			setRPL(*CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_CS],getCPL()); //CS.RPL=CPL!

			CPU[activeCPU].registers->EIP = (idtentry.offsetlow | (idtentry.offsethigh << 16)); //The current OPCode: just jump to the address specified by the descriptor OR command!
			CPU_flushPIQ(-1); //We're jumping to another address!

			FLAGW_TF(0);
			FLAGW_NT(0);

			if ((IDTENTRY_TYPE(idtentry) & 0x7) == IDTENTRY_16BIT_INTERRUPTGATE)
			{
				FLAGW_IF(0); //No interrupts!
			}

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

			hascallinterrupttaken_type = INTERRUPTGATETIMING_SAMELEVEL; //TODO Specify same level for now, until different level is implemented!
			CPU[activeCPU].executed = 1; //We've executed, start any post-instruction stuff!
			return 1; //OK!
			break;
		default: //Unknown descriptor type?
			THROWDESCGP(base,1,EXCEPTION_TABLE_IDT); //#GP! We're always from the IDT!
			return 0; //Errored out!
			break;
		}
		break;
	}
	return 0; //Default: Errored out!
}
