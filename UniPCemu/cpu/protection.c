#include "headers/cpu/cpu.h" //Basic CPU info!
#include "headers/cpu/protection.h" //Protection support!
#include "headers/cpu/multitasking.h" //Multitasking support!
#include "headers/support/zalloc.h" //Memory/register protection support!
#include "headers/mmu/mmuhandler.h" //Direct memory access support!
#include "headers/emu/debugger/debugger.h" //For logging check!
#include "headers/support/locks.h" //We need to unlock ourselves during triple faults, to reset ourselves!
#include "headers/cpu/cpu_pmtimings.h" //286+ timing support!
#include "headers/cpu/easyregs.h" //Easy register support!

/*

Basic CPU active segment value retrieval.

*/

//Exceptions, 286+ only!

//Reading of the 16-bit entries within descriptors!
#define DESC_16BITS(x) SDL_SwapLE16(x)

extern byte hascallinterrupttaken_type; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task gate flag. Left at 0xFF when nothing is used(unknown case?)

uint_32 CALLGATE_NUMARGUMENTS = 0; //The amount of arguments of the call gate!

void CPU_triplefault()
{
	unlock(LOCK_CPU);
	resetCPU(); //Simply fully reset the CPU on triple fault(e.g. reset pin result)!
	lock(LOCK_CPU);
	CPU[activeCPU].faultraised = 1; //We're continuing being a fault!
}

void CPU_doublefault()
{
	if (getcpumode()!=CPU_MODE_REAL) //Protected mode only?
	{
		uint_64 zerovalue=0; //Zero value pushed!
		CPU[activeCPU].faultraised = 0; //Reset the fault level for the double fault(allow memory accesses again)!
		call_soft_inthandler(EXCEPTION_DOUBLEFAULT,zerovalue); //Execute the double fault handler!
	}
	CPU[activeCPU].faultraised = 1; //We're ignoring any more errors occurring!
}

byte CPU_faultraised()
{
	if (CPU[activeCPU].faultlevel) //Double/triple fault raised?
	{
		if (CPU[activeCPU].faultlevel == 2) //Triple fault?
		{
			CPU_triplefault(); //Triple faulting!
			return 0; //Shutdown!
		}
		else
		{
			++CPU[activeCPU].faultlevel; //Raise the fault level!
			CPU_doublefault(); //Double faulting!
			return 0; //Don't do anything anymore(partial shutdown)!
		}
	}
	else
	{
		CPU[activeCPU].faultlevel = 1; //We have a fault raised, so don't raise any more!
	}
	CPU[activeCPU].faultraised = 0; //We've raised a fault! Ignore more errors for now!
	return 1; //Handle the fault normally!
}

//More info: http://wiki.osdev.org/Paging
//General Protection fault.
void CPU_GP(int toinstruction,int_64 errorcode)
{
	if (toinstruction) //Point to the faulting instruction?
	{
		CPU_resetOP(); //Point to the faulting instruction!
	}
	
	if (CPU_faultraised()) //Fault raising exception!
	{
		call_soft_inthandler(EXCEPTION_GENERALPROTECTIONFAULT,errorcode); //Call IVT entry #13 decimal!
		//Execute the interrupt!
		CPU[activeCPU].faultraised = 1; //Ignore more instructions!
	}
}

void CPU_SegNotPresent(int_64 errorcode)
{
	CPU_resetOP(); //Point to the faulting instruction!

	if (CPU_faultraised()) //Fault raising exception!
	{
		call_soft_inthandler(EXCEPTION_SEGMENTNOTPRESENT,errorcode); //Call IVT entry #11 decimal!
		//Execute the interrupt!
		CPU[activeCPU].faultraised = 1; //Ignore more instructions!
	}
}

void CPU_StackFault(int_64 errorcode)
{
	CPU_resetOP(); //Point to the faulting instruction!

	if (CPU_faultraised()) //Fault raising exception!
	{
		call_soft_inthandler(EXCEPTION_STACKFAULT,errorcode); //Call IVT entry #12 decimal!
		//Execute the interrupt!
		CPU[activeCPU].faultraised = 1; //Ignore more instructions!
	}
}

void protection_nextOP() //We're executing the next OPcode?
{
	CPU[activeCPU].faultraised = 0; //We don't have a fault raised anymore, so we can raise again!
	CPU[activeCPU].faultlevel = 0; //Reset the current fault level!
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

int isGateDescriptor(SEGDESCRIPTOR_TYPE *loadeddescriptor)
{
	if (getLoadedTYPE(loadeddescriptor)==2) //System?
	{
		switch (GENERALSEGMENT_TYPE(loadeddescriptor->desc))
		{
		case AVL_SYSTEM_RESERVED_0: //NULL descriptor?
			return 0; //NULL descriptor!
		case AVL_SYSTEM_BUSY_TSS16BIT: //TSS?
		case AVL_SYSTEM_BUSY_TSS32BIT: //TSS?
		case AVL_SYSTEM_TSS16BIT: //TSS?
		case AVL_SYSTEM_TSS32BIT: //TSS?
		case AVL_SYSTEM_LDT: //LDT?
			return -1; //System segment descriptor!
		case AVL_SYSTEM_TASKGATE: //Task gate?
		case AVL_SYSTEM_CALLGATE16BIT:
		case AVL_SYSTEM_CALLGATE32BIT:
		case AVL_SYSTEM_INTERRUPTGATE16BIT:
		case AVL_SYSTEM_INTERRUPTGATE32BIT:
		case AVL_SYSTEM_TRAPGATE16BIT:
		case AVL_SYSTEM_TRAPGATE32BIT: //Any type of gate?
			return 1; //We're a gate!
		default: //Unknown type?
			break;
		}
	}
	return 0; //Not a gate descriptor!
}

void THROWDESCGP(word segmentval, byte external, byte tbl)
{
	CPU_GP(1,(external&1)|(segmentval&(0xFFFB))|((tbl&0x3)<<1)); //#GP with an error in the LDT/GDT (index@bits 3-15)!
}

void THROWDESCSP(word segmentval, byte external, byte tbl)
{
	CPU_StackFault((external&1)|(segmentval&(0xFFFB))|((tbl&0x3)<<1)); //#StackFault with an error in the LDT/GDT (index@bits 3-15)!
}

void THROWDESCNP(word segmentval, byte external, byte tbl)
{
	CPU_SegNotPresent((external&1)|(segmentval&(0xFFFB))|((tbl&0x3)<<1)); //#SegFault with an error in the LDT/GDT (index@bits 3-15)!
}

//Another source: http://en.wikipedia.org/wiki/General_protection_fault

int LOADDESCRIPTOR(int segment, word segmentval, SEGDESCRIPTOR_TYPE *container) //Result: 0=#GP, 1=container=descriptor.
{
	uint_32 descriptor_address = 0;
	descriptor_address = (segmentval & 4) ? ((CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_low | (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_mid << 16)) | CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_high << 24) : CPU[activeCPU].registers->GDTR.base; //LDT/GDT selector!

	uint_32 descriptor_index=segmentval; //The full index within the descriptor table!
	descriptor_index &= ~0x7; //Clear bits 0-2 for our base index into the table!

	if ((word)(descriptor_index|0x7)>=((segmentval & 4) ? (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_low | (SEGDESC_NONCALLGATE_LIMIT_HIGH(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR]) << 16)) : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
	{
		return 0; //Not present: limit exceeded!
	}
	
	if ((!getDescriptorIndex(descriptor_index)) && ((segment==CPU_SEGMENT_CS) || ((segment==CPU_SEGMENT_SS)))) //NULL segment loaded into CS or SS?
	{
		return 0; //Not present: limit exceeded!	
	}
	
	descriptor_address += descriptor_index; //Add the index multiplied with the width(8 bytes) to get the descriptor!

	int i;
	for (i=0;i<(int)sizeof(container->descdata);) //Process the descriptor data!
	{
		container->descdata[i++] = memory_directrb(descriptor_address++); //Read a descriptor byte directly from flat memory!
	}

	container->desc.limit_low = DESC_16BITS(container->desc.limit_low);
	container->desc.base_low = DESC_16BITS(container->desc.base_low);

	if (EMULATED_CPU == CPU_80286) //80286 has less options?
	{
		container->desc.base_high = 0; //No high byte is present!
		container->desc.noncallgate_info &= ~0xF; //No high limit is present!
	}

	if (segment == CPU_SEGMENT_LDTR) //Loading a LDT with no LDT entry used?
	{
		if (segmentval & 4) //We're not loading from the GDT?
		{
			return 0; //Not present: limit exceeded!
		}
		if (GENERALSEGMENT_TYPE(container->desc) != AVL_SYSTEM_LDT) //We're not an LDT?
		{
			return 0; //Not present: limit exceeded!
		}
	}
	
	if ((segment==CPU_SEGMENT_SS) && //SS is...
		((getLoadedTYPE(container)==1) || //An executable segment? OR
		(!getLoadedTYPE(container) && (DATASEGMENT_W(container->desc)==0)) || //Read-only DATA segment? OR
		(getCPL()!=GENERALSEGMENT_DPL(container->desc)) //Not the same privilege?
		)
		)
	{
		return 0; //Not present: limit exceeded!	
	}

	if ((segment==CPU_SEGMENT_CS) &&
		(getLoadedTYPE(container)!=1) //Data or System in CS (non-exec)?
		)
	{
		return 0; //Not present: limit exceeded!	
	}
	return 1; //OK!
}

void SAVEDESCRIPTOR(int segment, word segmentval, SEGDESCRIPTOR_TYPE *container)
{
	uint_32 descriptor_address = 0;
	descriptor_address = (segmentval & 4) ? ((CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_low | (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_mid << 16)) | CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_high << 24) : CPU[activeCPU].registers->GDTR.base; //LDT/GDT selector!
	uint_32 descriptor_index = segmentval; //The full index within the descriptor table!
	descriptor_index &= ~0x7; //Clear bits 0-2 for our base index into the table!

	if ((word)(descriptor_index | 0x7) >= ((segmentval & 4) ? (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_low | (SEGDESC_NONCALLGATE_LIMIT_HIGH(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR]) << 16)) : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
	{
		return; //Not present: limit exceeded!
	}

	if ((!getDescriptorIndex(descriptor_index)) && ((segment == CPU_SEGMENT_CS) || ((segment == CPU_SEGMENT_SS)))) //NULL segment loaded into CS or SS?
	{
		return; //Not present: limit exceeded!	
	}

	descriptor_address += descriptor_index; //Add the index multiplied with the width(8 bytes) to get the descriptor!

	SEGDESCRIPTOR_TYPE tempcontainer;
	if (EMULATED_CPU == CPU_80286) //80286 has less options?
	{
		if (LOADDESCRIPTOR(segment,segmentval,&tempcontainer)) //Loaded the old container?
		{
			container->desc.base_high = tempcontainer.desc.base_high; //No high byte is present, so ignore the data to write!
			container->desc.noncallgate_info = ((container->desc.noncallgate_info&~0xF)|(tempcontainer.desc.noncallgate_info&0xF)); //No high limit is present, so ingore the data to write!
		}
		//Don't handle any errors on descriptor loading!
	}

	//Patch back to memory values!
	container->desc.limit_low = DESC_16BITS(container->desc.limit_low);
	container->desc.base_low = DESC_16BITS(container->desc.base_low);

	int i;
	for (i = 0;i<(int)sizeof(container->descdata);) //Process the descriptor data!
	{
		memory_directwb(descriptor_address++, container->descdata[i++]); //Write a descriptor byte directly to flat memory!
	}
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

SEGMENT_DESCRIPTOR *getsegment_seg(int segment, SEGMENT_DESCRIPTOR *dest, word segmentval, byte isJMPorCALL, byte *isdifferentCPL) //Get this corresponding segment descriptor (or LDT. For LDT, specify LDT register as segment) for loading into the segment descriptor cache!
{
	SEGDESCRIPTOR_TYPE LOADEDDESCRIPTOR, GATEDESCRIPTOR; //The descriptor holder/converter!

	if (!LOADDESCRIPTOR(segment,segmentval,&LOADEDDESCRIPTOR)) //Error loading current descriptor?
	{
		THROWDESCGP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP error!
		return NULL; //Error, by specified reason!
	}
	byte equalprivilege = 0; //Special gate stuff requirement: DPL must equal CPL? 1 for enable, 0 for normal handling.
	byte privilegedone = 0; //Privilege already calculated?
	byte is_gated = 0;
	byte is_TSS = 0; //Are we a TSS?
	byte callgatetype = 0; //Default: no call gate!

	if (GENERALSEGMENT_P(LOADEDDESCRIPTOR.desc)==0) //Not present?
	{
		if (segment==CPU_SEGMENT_SS) //Stack fault?
		{
			THROWDESCSP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Stack fault!
		}
		else
		{
			THROWDESCNP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		}
		return NULL; //We're an invalid TSS to execute!
	}

	if ((isGateDescriptor(&LOADEDDESCRIPTOR)==1) && (segment == CPU_SEGMENT_CS) && isJMPorCALL) //Handling of gate descriptors?
	{
		is_gated = 1; //We're gated!
		memcpy(&GATEDESCRIPTOR, &LOADEDDESCRIPTOR, sizeof(GATEDESCRIPTOR)); //Copy the loaded descriptor to the GATE!
		if (MAX(getCPL(), getRPL(segmentval)) > GENERALSEGMENT_DPL(GATEDESCRIPTOR.desc)) //Gate has too high a privilege level?
		{
			THROWDESCGP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
			return NULL; //We are a lower privilege level, so don't load!				
		}
		segmentval = (GATEDESCRIPTOR.desc.selector & ~3) | (segmentval & 3); //We're loading this segment now, with requesting privilege!
		if (!LOADDESCRIPTOR(segment, segmentval, &LOADEDDESCRIPTOR)) //Error loading current descriptor?
		{
			THROWDESCGP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
			return NULL; //Error, by specified reason!
		}
		privilegedone = 1; //Privilege has been precalculated!
		if ((GENERALSEGMENT_TYPE(LOADEDDESCRIPTOR.desc) & 0x1D) == AVL_SYSTEM_TASKGATE) //Task gate?
		{
			if (segment != CPU_SEGMENT_CS) //Not code? We're not a task switch! We're trying to load the task segment into a data register. This is illegal!
			{
				THROWDESCGP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
				return NULL; //Don't load!
			}
		}
		else //Normal descriptor?
		{
			if (isJMPorCALL == 1 && !EXECSEGMENT_C(LOADEDDESCRIPTOR.desc)) //JMP to a nonconforming segment?
			{
				if (GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc) != getCPL()) //Different CPL?
				{
					THROWDESCGP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
					return NULL; //We are a different privilege level, so don't load!						
				}
			}
			else if (isJMPorCALL) //Call instruction (or JMP instruction to a conforming segment)
			{
				if (GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc) > getCPL()) //We have a lower CPL?
				{
					THROWDESCGP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
					return NULL; //We are a different privilege level, so don't load!
				}
			}
		}
	}
	else if ((isGateDescriptor(&LOADEDDESCRIPTOR)==-1) && (segment==CPU_SEGMENT_CS) && isJMPorCALL) //JMP/CALL to non-gate descriptor(and not a system segment)?
	{
		equalprivilege = 1; //Enforce equal privilege!
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
		THROWDESCGP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return NULL; //Not present: limit exceeded!	
	}
	
	//Now check for CPL,DPL&RPL! (chapter 6.3.2)
	if (
		(!privilegedone && !equalprivilege && (MAX(getCPL(),getRPL(segmentval))>GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc)) && !(segment==CPU_SEGMENT_CS && EXECSEGMENT_C(LOADEDDESCRIPTOR.desc))) || //We are a lower privilege level and non-conforming?
		((!privilegedone && equalprivilege && MAX(getCPL(),getRPL(segmentval))!=GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc)) && //We must be at the same privilege level?
			!(EXECSEGMENT_C(LOADEDDESCRIPTOR.desc)) //Not conforming checking further ahead makes sure that we don't double check things?
			)
		)
	{
		THROWDESCGP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return NULL; //We are a lower privilege level, so don't load!
	}

	switch (GENERALSEGMENT_TYPE(LOADEDDESCRIPTOR.desc)) //We're a TSS? We're to perform a task switch!
	{
	case AVL_SYSTEM_BUSY_TSS16BIT:
	case AVL_SYSTEM_BUSY_TSS32BIT:
	case AVL_SYSTEM_TSS16BIT:
	case AVL_SYSTEM_TSS32BIT: //TSS?
		is_TSS = (getLoadedTYPE(&LOADEDDESCRIPTOR)==2); //We're a TSS when a system segment!
		break;
	default:
		is_TSS = 0; //We're no TSS!
		break;
	}

	if (is_TSS && (segment==CPU_SEGMENT_TR)) //We're a TSS loading into TR? We're to perform a task switch!
	{
		if (segmentval & 2) //LDT lookup set?
		{
			THROWDESCGP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
			return NULL; //We're an invalid TSS to call!
		}
		//Handle the task switch normally! We're allowed to use the TSS!
	}

	if (GENERALSEGMENT_P(LOADEDDESCRIPTOR.desc)==0) //Not present?
	{
		if (segment==CPU_SEGMENT_SS) //Stack fault?
		{
			THROWDESCSP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		}
		else
		{
			THROWDESCNP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		}
		return NULL; //We're an invalid TSS to execute!
	}

	if ((segment==CPU_SEGMENT_CS) && is_TSS) //Special stuff on CS, CPL, Task switch.
	{
		//Execute a normal task switch!
		if (CPU_switchtask(segment,&LOADEDDESCRIPTOR,&segmentval,segmentval,isJMPorCALL,is_gated,0)) //Switching to a certain task?
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
			if (!privilegedone && GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc)>getCPL()) //Target DPL must be less-or-equal to the CPL.
			{
				THROWDESCGP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
				return NULL; //We are a lower privilege level, so don't load!				
			}
		}
		else //Non-conforming segment?
		{
			if (!privilegedone && GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc)!=getCPL()) //Check for equal only when using Gate Descriptors?
			{
				THROWDESCGP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
				return NULL; //We are a lower privilege level, so don't load!				
			}
			CPU[activeCPU].CPL = GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc); //New privilege level!
		}
	}

	if ((segment == CPU_SEGMENT_CS) && (getLoadedTYPE(&GATEDESCRIPTOR) == -1) && (is_gated)) //Gated CS?
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
			fifobuffer_clear(CPU[activeCPU].CallGateStack); //Clear our stack to transfer!
			if (GENERALSEGMENT_DPL(LOADEDDESCRIPTOR.desc)!=getCPL()) //Stack switch required?
			{
				*isdifferentCPL = 1; //We're a different level!
				arguments = CALLGATE_NUMARGUMENTS =  GATEDESCRIPTOR.desc.ParamCnt; //Amount of parameters!
				for (;arguments--;) //Copy as many arguments as needed!
				{
					if (DATA_SEGMENT_DESCRIPTOR_B_BIT()) //32-bit source?
					{
						argument = CPU_POP32(); //POP 32-bit argument!
					}
					else //16-bit source?
					{
						argument = (uint_32)CPU_POP16(); //POP 16-bit argument!
					}
					if (CPU[activeCPU].faultraised) //Fault was raised reading source parameters?
					{
						fifobuffer_clear(CPU[activeCPU].CallGateStack); //Clear our stack!
						return NULL; //Abort!
					}
					writefifobuffer32(CPU[activeCPU].CallGateStack,argument); //Add the argument to the call gate buffer to transfer to the new stack!
				}
			}
		}
	}

	memcpy(dest,&LOADEDDESCRIPTOR,sizeof(LOADEDDESCRIPTOR)); //Give the loaded descriptor!

	return dest; //Give the segment descriptor read from memory!
}

void segmentWritten(int segment, word value, byte isJMPorCALL) //A segment register has been written to!
{
	byte oldCPL= getCPL();
	byte TSSSize, isDifferentCPL;
	word tempSS;
	uint_32 tempesp; //For inter-level far returns
	if (CPU[activeCPU].faultraised) return; //Abort if already an fault has been raised!
	if (getcpumode()==CPU_MODE_PROTECTED) //Protected mode, must not be real or V8086 mode, so update the segment descriptor cache!
	{
		isDifferentCPL = 0; //Default: same CPL!
		SEGMENT_DESCRIPTOR tempdescriptor;
		SEGMENT_DESCRIPTOR *descriptor = getsegment_seg(segment,&tempdescriptor,value,isJMPorCALL,&isDifferentCPL); //Read the segment!
		byte TSS_StackPos;
		uint_32 stackval;
		word stackval16; //16-bit stack value truncated!
		if (descriptor) //Loaded&valid?
		{
			if ((segment == CPU_SEGMENT_CS) && (isJMPorCALL == 2)) //CALL needs pushed data on the stack?
			{
				if (CALLGATE_NUMARGUMENTS) //Stack switch is required?
				{
					TSSSize = 0; //Default to 16-bit TSS!
					switch (GENERALSEGMENT_TYPE(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR])) //What kind of TSS?
					{
					case AVL_SYSTEM_BUSY_TSS32BIT:
					case AVL_SYSTEM_TSS32BIT:
						TSSSize = 1; //32-bit TSS!
					case AVL_SYSTEM_BUSY_TSS16BIT:
					case AVL_SYSTEM_TSS16BIT:
						TSS_StackPos = (2<<TSSSize); //Start of the stack block! 2 for 16-bit TSS, 4 for 32-bit TSS!
						TSS_StackPos += (4<<TSSSize)*GENERALSEGMENTPTR_DPL(descriptor); //Start of the correct TSS (E)SP! 4 for 16-bit TSS, 8 for 32-bit TSS!
						stackval = TSSSize?MMU_rdw(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,TSS_StackPos,0):MMU_rw(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,TSS_StackPos,0); //Read (E)SP for the privilege level from the TSS!
						if (TSSSize) //32-bit?
						{
							TSS_StackPos += 8; //Take SS position!
						}
						else
						{
							TSS_StackPos += 4; //Take SS position!
						}
						segmentWritten(CPU_SEGMENT_SS,MMU_rw(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,TSS_StackPos,0),0); //Read SS!
						if (CPU[activeCPU].faultraised) return; //Abort on fault!
						if (TSSSize) //32-bit?
						{
							CPU[activeCPU].registers->ESP = stackval; //Apply the stack position!
						}
						else
						{
							CPU[activeCPU].registers->SP = (word)stackval; //Apply the stack position!
						}
						//Now, we've switched to the destination stack! Load all parameters onto the new stack!
						for (;fifobuffer_freesize(CPU[activeCPU].CallGateStack);) //Process the CALL Gate Stack!
						{
							if (readfifobuffer32(CPU[activeCPU].CallGateStack,&stackval)) //Read the value to transfer?
							{
								if ((DATA_SEGMENT_DESCRIPTOR_B_BIT()) && (EMULATED_CPU>=CPU_80386)) //32-bit stack to push to?
								{
									CPU_PUSH32(&stackval); //Push the 32-bit stack value to the new stack!
								}
								else //16-bit?
								{
									stackval16 = (word)(stackval&0xFFFF); //Reduced data if needed!
									CPU_PUSH16(&stackval16); //Push the 16-bit stack value to the new stack!
								}
							}
						}
					}
				}
				
				if (isDifferentCPL) //CPL changed?
				{
					CPU_PUSH16(&REG_SP); //SP to return!
					if (CPU_Operand_size[activeCPU])
					{
						CPU_PUSH32(&REG_ESP);
					}
					else
					{
						CPU_PUSH16(&REG_SP);
					}
				}
				
				//Push the old address to the new stack!
				if (CPU_Operand_size[activeCPU]) //32-bit?
				{
					CPU_PUSH16(&CPU[activeCPU].registers->CS);
					CPU_PUSH32(&CPU[activeCPU].registers->EIP);
				}
				else //16-bit?
				{
					CPU_PUSH16(&CPU[activeCPU].registers->CS);
					CPU_PUSH16(&CPU[activeCPU].registers->IP);
				}

				if (hascallinterrupttaken_type==0xFF) //Not set yet?
				{
					if (CPU[activeCPU].faultraised==0) //OK?
					{
						if (isDifferentCPL) //Different CPL?
						{
							hascallinterrupttaken_type = CALLGATE_NUMARGUMENTS?CALLGATE_DIFFERENTLEVEL_XPARAMETERS:CALLGATE_DIFFERENTLEVEL_NOPARAMETERS; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task gate flag. Left at 0xFF when nothing is used(unknown case?)
						}
						else //Same CPL call gate?
						{
							hascallinterrupttaken_type = CALLGATE_SAMELEVEL; //Same level call gate!
						}
					}		
				}
			}
			else if ((segment == CPU_SEGMENT_CS) && (isJMPorCALL == 4)) //RETF needs popped data on the stack?
			{
				if (oldCPL!=getRPL(value)) //CPL changed?
				{
					//Privilege change!
					hascallinterrupttaken_type = RET_DIFFERENTLEVEL; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task
					if (CPU_Operand_size[activeCPU])
					{
						tempesp = CPU_POP32();
					}
					else
					{
						tempesp = CPU_POP16();
					}
					tempSS = CPU_POP16();
					segmentWritten(CPU_SEGMENT_SS,tempSS,0); //Back to our calling stack!
					if (CPU[activeCPU].faultraised) return;
					REG_ESP = tempesp;
				}
			}

			//Now, load the new descriptor and address for CS if needed(with secondary effects)!
			memcpy(&CPU[activeCPU].SEG_DESCRIPTOR[segment],descriptor,sizeof(CPU[activeCPU].SEG_DESCRIPTOR[segment])); //Load the segment descriptor into the cache!
			//if (memprotect(CPU[activeCPU].SEGMENT_REGISTERS[segment],2,"CPU_REGISTERS")) //Valid segment register?
			{
				*CPU[activeCPU].SEGMENT_REGISTERS[segment] = value; //Set the segment register to the allowed value!
			}
			if (segment == CPU_SEGMENT_CS) //CS register?
			{
				CPU[activeCPU].registers->EIP = destEIP; //The current OPCode: just jump to the address specified by the descriptor OR command!
				CPU_flushPIQ(); //We're jumping to another address!
			}
		}
	}
	else //Real mode has no protection?
	{
		if (isJMPorCALL == 2) //CALL needs pushed data?
		{
			if ((CPU_Operand_size[activeCPU]) && (EMULATED_CPU>=CPU_80386)) //32-bit?
			{
				CPU_PUSH16(&CPU[activeCPU].registers->CS);
				CPU_PUSH32(&CPU[activeCPU].registers->EIP);
			}
			else //16-bit?
			{
				CPU_PUSH16(&CPU[activeCPU].registers->CS);
				CPU_PUSH16(&CPU[activeCPU].registers->IP);
			}
		}
		//if (memprotect(CPU[activeCPU].SEGMENT_REGISTERS[segment],2,"CPU_REGISTERS")) //Valid segment register?
		{
			*CPU[activeCPU].SEGMENT_REGISTERS[segment] = value; //Just set the segment, don't load descriptor!
			//Load the correct base data for our loading!
			CPU[activeCPU].SEG_DESCRIPTOR[segment].base_low = (word)(((uint_32)value<<4)&0xFFFF); //Low base!
			CPU[activeCPU].SEG_DESCRIPTOR[segment].base_mid = ((((uint_32)value << 4) & 0xFF0000)>>16); //Mid base!
			CPU[activeCPU].SEG_DESCRIPTOR[segment].base_high = ((((uint_32)value << 4) & 0xFF000000)>>24); //High base!
			//This also maps the resulting segment in low memory (20-bit address space) in real mode, thus CS is pulled low as well!
		}
		if (segment==CPU_SEGMENT_CS) //CS segment? Reload access rights in real mode on first write access!
		{
			CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].AccessRights = 0x9D; //Load default access rights!
			CPU[activeCPU].registers->EIP = destEIP; //... The current OPCode: just jump to the address!
			CPU_flushPIQ(); //We're jumping to another address!
		}
	}
	//Real mode doesn't use the descriptors?
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

MMU: memory start!

*/

uint_32 CPU_MMU_start(sword segment, word segmentval) //Determines the start of the segment!
{
	//Determine the Base always, even in real mode(which automatically loads the base when loading the segment registers)!
	if (segment == -1) //Forced 8086 mode by the emulators?
	{
		return (segmentval << 4); //Behave like a 8086!
	}
	
	//Protected mode addressing!
	return ((CPU[activeCPU].SEG_DESCRIPTOR[segment].base_high<<24)|(CPU[activeCPU].SEG_DESCRIPTOR[segment].base_mid<<16)|CPU[activeCPU].SEG_DESCRIPTOR[segment].base_low); //Base!
}

/*

MMU: Memory limit!

*/

//Used by the CPU(VERR/VERW)&MMU I/O!
byte CPU_MMU_checkrights(int segment, word segmentval, uint_32 offset, int forreading, SEGMENT_DESCRIPTOR *descriptor, byte addrtest)
{
	byte isconforming;

	if (getcpumode() == CPU_MODE_PROTECTED) //Not real mode? Check rights for zero descriptors!
	{
		if ((segment != CPU_SEGMENT_CS) && (segment != CPU_SEGMENT_SS) && !getDescriptorIndex(segmentval)) //Accessing memory with DS,ES,FS or GS, when they contain a NULL selector?
		{
			return 1; //Error!
		}
	}

	//First: type checking!

	if (GENERALSEGMENTPTR_P(descriptor)==0) //Not present(invalid in the cache)?
	{
		if (segment==CPU_SEGMENT_SS) //Stack fault?
		{
			return 3; //Stack fault!
		}
		else
		{
			return 2; //#NP!
		}
	}

	if (getcpumode()==CPU_MODE_PROTECTED) //Not real mode? Check rights!
	{
		if (segment == CPU_SEGMENT_CS && !(EXECSEGMENTPTR_ISEXEC(descriptor)) && (forreading == 3)) //Non-executable segment execution?
		{
			return 1; //Error!
		}
		else if (((EXECSEGMENTPTR_ISEXEC(descriptor)) || !(DATASEGMENTPTR_OTHERSTRUCT(descriptor) || DATASEGMENTPTR_W(descriptor))) && (forreading==0)) //Writing to executable segment or read-only data segment?
		{
			return 1; //Error!
		}
		else if (EXECSEGMENTPTR_ISEXEC(descriptor) && !EXECSEGMENTPTR_R(descriptor) && forreading == 1) //Reading execute-only segment?
		{
			return 1; //Error!	
		}
	}

	//Next: limit checking!

	uint_32 limit; //The limit!
	byte isvalid;

	limit = ((SEGDESCPTR_NONCALLGATE_LIMIT_HIGH(descriptor) << 16) | descriptor->limit_low); //Base limit!

	if ((SEGDESCPTR_NONCALLGATE_G(descriptor)&CPU[activeCPU].G_Mask) && (EMULATED_CPU>=CPU_80386)) //Granularity?
	{
		limit = ((limit << 12) | 0xFFF); //4KB for a limit of 4GB, fill lower 12 bits with 1!
	}

	if (addrtest) //Execute address test?
	{
		isvalid = (offset<=limit); //Valid address range!
		if ((GENERALSEGMENTPTR_S(descriptor) == 1) && (EXECSEGMENTPTR_ISEXEC(descriptor) == 0)) //Data segment?
		{
			if (DATASEGMENTPTR_E(descriptor)) //Expand-down data segment?
			{
				isvalid = !isvalid; //Reversed valid!
				if (SEGDESCPTR_NONCALLGATE_G(descriptor) == 0) //Small granularity?
				{
					isvalid = (isvalid && (offset <= 0x10000)); //Limit to 64K!
				}
			}
		}
		if (!isvalid) //Not valid?
		{
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

	//Third: privilege levels & Restrict access to data!

	switch (descriptor->AccessRights) //What type?
	{
	case AVL_CODE_EXECUTEONLY_CONFORMING:
	case AVL_CODE_EXECUTEONLY_CONFORMING_ACCESSED:
	case AVL_CODE_EXECUTE_READONLY_CONFORMING:
	case AVL_CODE_EXECUTE_READONLY_CONFORMING_ACCESSED: //Conforming?
		isconforming = 1;
		break;
	default: //Not conforming?
		isconforming = 0;
		break;
	}

	if (getcpumode()==CPU_MODE_PROTECTED) //Not in real mode? Perform rights checks!
	{
		if (segment!=CPU_SEGMENT_TR) //Not task register?
		{
			if (((MAX(getCPL(), getRPL(segmentval)) <= GENERALSEGMENTPTR_DPL(descriptor)) || isconforming)==0) //Invalid privilege?
			{
				return 1; //Not enough rights!
			}
		}
	}

	//Fifth: Accessing data in Code segments?

	return 0; //OK!
}

//Used by the MMU!
int CPU_MMU_checklimit(int segment, word segmentval, uint_32 offset, int forreading) //Determines the limit of the segment, forreading=2 when reading an opcode!
{
	//Determine the Limit!
	if (EMULATED_CPU >= CPU_80286) //Handle like a 80286+?
	{
		if (segment==-1) return 0; //Enable: we're an emulator call!
		if (CPU[activeCPU].faultraised) return 1; //Abort if already an fault has been raised!
		if (segment==-1) //Emulator access? Unknown segment to use?
		{
			return 0; //Enable all, we're direct after all!
		}
		
		//Use segment descriptors, even when in real mode on 286+ processors!
		switch (CPU_MMU_checkrights(segment,segmentval, offset, forreading, &CPU[activeCPU].SEG_DESCRIPTOR[segment],1)) //What rights resulting? Test the address itself too!
		{
		case 0: //OK?
			break; //OK!
		default: //Unknown status? Count #GP by default!
		case 1: //#GP?
			THROWDESCGP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw fault!
			return 1; //Error out!
			break;
		case 2: //#NP?
			THROWDESCNP(segmentval,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error: accessing non-present segment descriptor!
			return 1; //Error out!
			break;
		case 3: //#SS?
			THROWDESCSP(segment,0,(segmentval&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
			return 1; //Error out!
			break;
		}
		return 0; //OK!
	}
	return 0; //Don't give errors: handle like a 80(1)86!
}

byte checkSpecialRights() //Check special rights, common by any rights instructions!
{
	if (getcpumode() == CPU_MODE_REAL) return 0; //Allow all for real mode!
	if (FLAG_PL > getCPL()) //We're not allowed!
	{
		return 1; //Not priviledged!
	}
	return 0; //Priviledged!
}

byte checkSTICLI() //Check STI/CLI rights!
{
	if (checkSpecialRights()) //Not priviledged?
	{
		THROWDESCGP(CPU[activeCPU].registers->CS,0,0); //Raise exception!
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
	if (checkSpecialRights()) //We're to check the I/O permission bitmap!
	{
		uint_32 maplocation;
		byte mappos;
		maplocation = (port>>3); //8 bits per byte!
		mappos = (1<<(port&7)); //The bit within the byte specified!
		//if ((CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR].Type == AVL_SYSTEM_BUSY_TSS16BIT) || (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR].Type == AVL_SYSTEM_TSS16BIT)) //16-bit TSS?
		if ((GENERALSEGMENT_TYPE(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR]) == AVL_SYSTEM_BUSY_TSS32BIT) || (GENERALSEGMENT_TYPE(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR]) == AVL_SYSTEM_TSS32BIT)) //32-bit TSS?
		{
			uint_32 limit;
			limit = CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR].limit_low | (SEGDESC_NONCALLGATE_LIMIT_HIGH(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR]) << 16); //The limit of the descriptor!
			maplocation += MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR,0x66,0); //Add the map location to the specified address!
			if (maplocation >= limit) //Over the limit? We're an invalid entry or got no bitmap!
			{
				return 1; //We're to cause an exception!
			}
			if (MMU_rb(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, maplocation, 0)&mappos) //We're to cause an exception: we're not allowed to access this port!
			{
				return 1; //We're to cause an exception!
			}
		}
	}
	return 0; //Allow all for now!
}

int LOADINTDESCRIPTOR(int segment, word segmentval, SEGDESCRIPTOR_TYPE *container)
{
	uint_32 descriptor_address = 0;
	descriptor_address = (segmentval & 4) ? ((CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_low | (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_mid << 16)) | CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_high << 24) : CPU[activeCPU].registers->GDTR.base; //LDT/GDT selector!

	uint_32 descriptor_index = segmentval; //The full index within the descriptor table!
	descriptor_index &= ~0x7; //Clear bits 0-2 for our base index into the table!

	if ((word)(descriptor_index | 0x7) >= ((segmentval & 4) ? (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_low | (SEGDESC_NONCALLGATE_LIMIT_HIGH(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR]) << 16)) : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
	{
		return 0; //Not present: limit exceeded!
	}

	if ((!getDescriptorIndex(descriptor_index)) && ((segment == CPU_SEGMENT_CS) || ((segment == CPU_SEGMENT_SS)))) //NULL segment loaded into CS or SS?
	{
		return 0; //Not present: limit exceeded!	
	}

	descriptor_address += descriptor_index; //Add the index multiplied with the width(8 bytes) to get the descriptor!

	int i;
	for (i = 0;i<(int)sizeof(container->descdata);) //Process the descriptor data!
	{
		container->descdata[i++] = memory_directrb(descriptor_address++); //Read a descriptor byte directly from flat memory!
	}

	if (EMULATED_CPU == CPU_80286) //80286 has less options?
	{
		container->desc.base_high = 0; //No high byte is present!
		container->desc.noncallgate_info &= ~0xF; //No high limit is present!
	}

	if ((segment == CPU_SEGMENT_CS) &&
		(getLoadedTYPE(container) != 1) //Not an executable segment?
		)

	{
		return 0; //Not present: limit exceeded!	
	}

	return 1; //OK!
}

byte CPU_ProtectedModeInterrupt(byte intnr, word returnsegment, uint_32 returnoffset, int_64 errorcode) //Execute a protected mode interrupt!
{
	uint_32 errorcode32 = (uint_32)errorcode; //Get the error code itelf!
	word errorcode16 = (word)errorcode; //16-bit variant, if needed!
	byte is_EXT = 0;
	is_EXT = ((errorcode!=-1)&&(errorcode&1)?1:0); //EXT?
	SEGDESCRIPTOR_TYPE newdescriptor; //Temporary storage for task switches!
	word desttask; //Destination task for task switches!
	byte left; //The amount of bytes left to read of the IDT entry!
	uint_32 base;
	base = (intnr<<3); //The base offset of the interrupt in the IDT!
	if ((base|0x7) >= CPU[activeCPU].registers->IDTR.limit) //Limit exceeded?
	{
		THROWDESCGP(base,is_EXT,EXCEPTION_TABLE_IDT); //#GP!
		return 0; //Abort!
	}

	base += CPU[activeCPU].registers->IDTR.base; //Add the base for the actual offset into the IDT!
	
	IDTENTRY idtentry; //The loaded IVT entry!
	for (left=0;left<sizeof(idtentry.descdata);) //Data left to read?
	{
		idtentry.descdata[left++] = memory_directrb(base++); //Read a byte from the descriptor entry!
	}

	idtentry.offsethigh = DESC_16BITS(idtentry.offsethigh); //Patch when needed!
	idtentry.offsetlow = DESC_16BITS(idtentry.offsetlow); //Patch when needed!
	idtentry.selector = DESC_16BITS(idtentry.selector); //Patch when needed!

	byte is32bit;

	if (IDTENTRY_P(idtentry)==0) //Not present?
	{
		THROWDESCNP(base,is_EXT,EXCEPTION_TABLE_IDT); //#NP!
		return 0;
	}

	if (!((errorcode!=-1)&&(errorcode&1)) && (IDTENTRY_DPL(idtentry) < getCPL())) //Not enough rights?
	{
		THROWDESCGP(base,is_EXT,EXCEPTION_TABLE_IDT); //#GP!
		return 0;
	}

	//Now, the (gate) descriptor to use is loaded!
	switch (IDTENTRY_TYPE(idtentry)) //What type are we?
	{
	case IDTENTRY_32BIT_TASKGATE: //32-bit task gate?
		desttask = idtentry.selector; //Read the destination task!
		if ((!LOADDESCRIPTOR(CPU_SEGMENT_TR, desttask, &newdescriptor)) || (desttask&4)) //Error loading new descriptor? The backlink is always at the start of the TSS! It muse also always be in the GDT!
		{
			THROWDESCGP(desttask,((errorcode!=-1)&&(errorcode&1)?1:0),(desttask&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP error!
			return 0; //Error, by specified reason!
		}
		if (CPU_switchtask(CPU_SEGMENT_TR, &newdescriptor, &CPU[activeCPU].registers->TR, desttask, 0,1,errorcode)) //Execute a task switch to the new task!
		{
			if (errorcode!=-1) //Error code to be pushed on the stack?
			{
				if (SEGDESC_NONCALLGATE_D_B(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR])) //32-bit task?
				{
					CPU_PUSH32(&errorcode32); //Push the error on the stack!
				}
				else
				{
					CPU_PUSH16(&errorcode16); //Push the error on the stack!
				}
				if (CPU[activeCPU].faultraised==0) //OK?
				{
					hascallinterrupttaken_type = INTERRUPTGATETIMING_TASKGATE; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task gate flag. Left at 0xFF when nothing is used(unknown case?)
				}
			}
			return 1; //OK!
		}
		else
		{
			return 0; //Abort!
		}
		break;
	default: //All other cases?
		is32bit = IDTENTRY_TYPE(idtentry)&IDTENTRY_32BIT_GATEEXTENSIONFLAG; //Enable 32-bit gate?
		switch (IDTENTRY_TYPE(idtentry) & 0x7) //What type are we?
		{
		case IDTENTRY_16BIT_INTERRUPTGATE: //16/32-bit interrupt gate?
		case IDTENTRY_16BIT_TRAPGATE: //16/32-bit trap gate?
			if (!LOADINTDESCRIPTOR(CPU_SEGMENT_CS, idtentry.selector, &newdescriptor)) //Error loading new descriptor? The backlink is always at the start of the TSS!
			{
				THROWDESCGP(idtentry.selector,is_EXT,(idtentry.selector&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
				return 0; //Error, by specified reason!
			}
			if (((GENERALSEGMENT_S(newdescriptor.desc)==0) || (EXECSEGMENT_ISEXEC(newdescriptor.desc)==0)) || (EXECSEGMENT_R(newdescriptor.desc)==0)) //Not readable, execute segment or is code/executable segment?
			{
				THROWDESCGP(idtentry.selector,is_EXT,(idtentry.selector&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP!
				return 0;
			}
			if ((idtentry.offsetlow | (idtentry.offsethigh << 16)) > (newdescriptor.desc.limit_low | (SEGDESC_NONCALLGATE_LIMIT_HIGH(newdescriptor.desc) << 16))) //Limit exceeded?
			{
				THROWDESCGP(idtentry.selector,is_EXT,(idtentry.selector&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP!
				return 0;
			}

			if ((EXECSEGMENT_C(newdescriptor.desc) == 0) && (GENERALSEGMENT_DPL(newdescriptor.desc) < getCPL())) //Not enough rights, but conforming?
			{
				//TODO
			}
			else
			{
				//Check permission? TODO!
			}

			CPU[activeCPU].faultraised = 0; //No fault raised anymore, because we're at the new instruction to handle it!

			if (is32bit)
			{
				CPU_PUSH32(&CPU[activeCPU].registers->EFLAGS); //Push EFLAGS!
				if (CPU[activeCPU].faultraised) return 0; //Abort on fault!
			}
			else
			{
				CPU_PUSH16(&CPU[activeCPU].registers->FLAGS); //Push FLAGS!
				if (CPU[activeCPU].faultraised) return 0; //Abort on fault!
			}

			CPU_PUSH16(&CPU[activeCPU].registers->CS); //Push CS!
			if (CPU[activeCPU].faultraised) return 0; //Abort on fault!

			if (is32bit)
			{
				CPU_PUSH32(&CPU[activeCPU].registers->EIP); //Push EIP!
				if (CPU[activeCPU].faultraised) return 0; //Abort on fault!
			}
			else
			{
				CPU_PUSH16(&CPU[activeCPU].registers->IP); //Push IP!
				if (CPU[activeCPU].faultraised) return 0; //Abort on fault!
			}

			memcpy(&CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS], &newdescriptor, sizeof(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS])); //Load the segment descriptor into the cache!
			//if (memprotect(CPU[activeCPU].SEGMENT_REGISTERS[segment],2,"CPU_REGISTERS")) //Valid segment register?
			{
				*CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_CS] = idtentry.selector; //Set the segment register to the allowed value!
			}

			CPU[activeCPU].registers->EIP = (idtentry.offsetlow | (idtentry.offsethigh << 16)); //The current OPCode: just jump to the address specified by the descriptor OR command!
			CPU_flushPIQ(); //We're jumping to another address!

			FLAGW_TF(0);
			FLAGW_NT(0);

			if ((IDTENTRY_TYPE(idtentry) & 0x7) == IDTENTRY_16BIT_INTERRUPTGATE)
			{
				FLAGW_IF(0); //No interrupts!
			}

			if (errorcode!=-1) //Error code specified?
			{
				if (SEGDESC_NONCALLGATE_D_B(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR])) //32-bit task?
				{
					CPU_PUSH32(&errorcode32); //Push the error on the stack!
				}
				else
				{
					CPU_PUSH16(&errorcode16); //Push the error on the stack!
				}
			}

			if (CPU[activeCPU].faultraised==0) //OK?
			{
				hascallinterrupttaken_type = INTERRUPTGATETIMING_SAMELEVEL; //TODO Specify same level for now, until different level is implemented!
			}
			return 1; //OK!
			break;
		default: //Unknown descriptor type?
			THROWDESCGP(base,is_EXT,EXCEPTION_TABLE_GDT); //#GP! We're always from the GDT!
			return 0; //Errored out!
			break;
		}
		break;
	}
}
