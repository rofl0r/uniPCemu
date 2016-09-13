#include "headers/cpu/cpu.h" //Basic CPU info!
#include "headers/cpu/protection.h" //Protection support!
#include "headers/cpu/multitasking.h" //Multitasking support!
#include "headers/support/zalloc.h" //Memory/register protection support!
#include "headers/mmu/mmuhandler.h" //Direct memory access support!

/*

Basic CPU active segment value retrieval.

*/

//Exceptions, 286+ only!

void CPU_triplefault()
{
	resetCPU(); //Simply fully reset the CPU on triple fault(e.g. reset pin result)!
	CPU[activeCPU].faultraised = 1; //We're continuing being a fault!
}

void CPU_doublefault()
{
	uint_32 zerovalue=0; //Zero value pushed!
	CPU[activeCPU].faultraised = 0; //Reset the fault level for the double fault(allow memory accesses again)!
	call_hard_inthandler(EXCEPTION_DOUBLEFAULT); //Execute the double fault handler!
	if (CPU[activeCPU].faultraised == 0) //Success during this step?
	{
		CPU_PUSH32(&zerovalue); //Error code of 0!
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
void CPU_GP(int toinstruction,uint_32 errorcode)
{
	if (toinstruction) //Point to the faulting instruction?
	{
		CPU_resetOP(); //Point to the faulting instruction!
	}
	
	if (CPU_faultraised()) //Fault raising exception!
	{
		call_hard_inthandler(EXCEPTION_GENERALPROTECTIONFAULT); //Call IVT entry #13 decimal!
		if (CPU[activeCPU].faultraised == 0) //Success during this step?
		{
			CPU_PUSH32(&errorcode); //Error code!
		}
		//Execute the interrupt!
		CPU[activeCPU].faultraised = 1; //Ignore more instructions!
	}
}

void CPU_SegNotPresent(uint_32 errorcode)
{
	CPU_resetOP(); //Point to the faulting instruction!

	if (CPU_faultraised()) //Fault raising exception!
	{
		call_hard_inthandler(EXCEPTION_SEGMENTNOTPRESENT); //Call IVT entry #11 decimal!
		if (CPU[activeCPU].faultraised == 0) //Success during this step?
		{
			CPU_PUSH32(&errorcode); //Error code!
		}
		//Execute the interrupt!
		CPU[activeCPU].faultraised = 1; //Ignore more instructions!
	}
}

void CPU_StackFault(uint_32 errorcode)
{
	CPU_resetOP(); //Point to the faulting instruction!

	if (CPU_faultraised()) //Fault raising exception!
	{
		call_hard_inthandler(EXCEPTION_STACKFAULT); //Call IVT entry #12 decimal!
		if (CPU[activeCPU].faultraised==0) //Success during this step?
		{
			CPU_PUSH32(&errorcode); //Error code!
		}
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

SEGDESCRIPTOR_TYPE LOADEDDESCRIPTOR, GATEDESCRIPTOR; //The descriptor holder/converter!

//getTYPE: gets the loaded descriptor type: 0=Code, 1=Exec, 2=System.
int getLoadedTYPE(SEGDESCRIPTOR_TYPE *loadeddescriptor)
{
	return (loadeddescriptor->desc.nonS?loadeddescriptor->desc.EXECSEGMENT.ISEXEC:2);
}
int isGateDescriptor(SEGDESCRIPTOR_TYPE *loadeddescriptor)
{
	return (!loadeddescriptor->desc.nonS && (loadeddescriptor->desc.Type==6)); //Gate descriptor?
}

void THROWDESCGP(word segment)
{
	CPU_GP(1,(segment&(0xFFFB))|(segment&4)); //#GP with an error in the LDT/GDT (index@bits 3-15)!
}

void THROWDESCSP(word segment, byte external)
{
	CPU_StackFault((external<<0)|(segment&(0xFFFB))|(segment&4)); //#StackFault with an error in the LDT/GDT (index@bits 3-15)!
}

void THROWDESCSeg(word segment, byte external)
{
	CPU_SegNotPresent((external<<0)|(segment&(0xFFFB))|(segment&4)); //#SegFault with an error in the LDT/GDT (index@bits 3-15)!
}

//Another source: http://en.wikipedia.org/wiki/General_protection_fault

int LOADDESCRIPTOR(int whatsegment, word segment, SEGDESCRIPTOR_TYPE *container)
{
	uint_32 descriptor_address = 0;
	descriptor_address = (segment & 4) ? ((CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_low | (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_mid << 16)) | CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_high << 24) : CPU[activeCPU].registers->GDTR.base; //LDT/GDT selector!
	uint_32 descriptor_index = getDescriptorIndex(segment); //The full index within the descriptor table!

	descriptor_index <<= 3; //Multiply into range!

	if ((word)(descriptor_index>>3)>=((segment & 4) ? (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_low | (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_high << 16)) : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
	{
		THROWDESCGP(segment); //Throw error!
		return 0; //Not present: limit exceeded!
	}
	
	if ((!descriptor_index) && ((whatsegment==CPU_SEGMENT_CS) || (whatsegment==CPU_SEGMENT_SS))) //NULL segment loaded into CS or SS?
	{
		THROWDESCGP(segment); //Throw error!
		return 0; //Not present: limit exceeded!	
	}
	
	descriptor_address += descriptor_index; //Add the index multiplied with the width(8 bytes) to get the descriptor!

	int i;
	for (i=0;i<(int)sizeof(container->descdata);i++) //Process the descriptor data!
	{
		container->descdata[i] = memory_directrb(descriptor_address+i); //Read a descriptor byte directly from flat memory!
	}

	if (EMULATED_CPU == CPU_80286) //80286 has less options?
	{
		container->desc.base_high = 0; //No high byte is present!
		container->desc.limit_high = 0; //No high limit is present!
	}

	if (whatsegment == CPU_SEGMENT_LDTR) //Loading a LDT with no LDT entry used?
	{
		if (segment & 4) //We're not loading from the GDT?
		{
			THROWDESCGP(segment); //Throw error!
			return 0; //Not present: limit exceeded!
		}
		if (container->desc.Type != AVL_SYSTEM_LDT) //We're not an LDT?
		{
			THROWDESCGP(segment); //Throw error!
			return 0; //Not present: limit exceeded!
		}
	}
	
	if ((whatsegment==CPU_SEGMENT_SS) && //SS is...
		((getLoadedTYPE(container)==1) || //An executable segment? OR
		(!getLoadedTYPE(container) && (container->desc.EXECSEGMENT.R)) || //Read-only DATA segment? OR
		(getCPL()!=container->desc.DPL) //Not the same privilege?
		)
		)
	{
		THROWDESCSP(segment,0); //Throw error!
		return 0; //Not present: limit exceeded!	
	}

	if ((whatsegment==CPU_SEGMENT_CS) &&
		(
		(getLoadedTYPE(container)!=1 && !isGateDescriptor(container)) || //Data or System in CS (non-exec)?
		(!getLoadedTYPE(container) && !isGateDescriptor(container)) //Or System?
		)
		)
	
	{
		THROWDESCGP(segment); //Throw error!
		return 0; //Not present: limit exceeded!	
	}
	return 1; //OK!
}

void SAVEDESCRIPTOR(int whatsegment, word segment, SEGDESCRIPTOR_TYPE *container)
{
	uint_32 descriptor_adress = 0;
	descriptor_adress = (segment & 4) ? ((CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_low | (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_mid << 16)) | CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_high << 24) : CPU[activeCPU].registers->GDTR.base; //LDT/GDT selector!
	uint_32 descriptor_index = getDescriptorIndex(segment); //The full index within the descriptor table!

	descriptor_index <<= 3; //Multiply into range!

	if ((word)(descriptor_index>>3)>=((segment & 4) ? (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_low | (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_high << 16)) : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
	{
		return; //Not present: limit exceeded!
	}

	if (!descriptor_index && ((whatsegment == CPU_SEGMENT_CS) || (whatsegment == CPU_SEGMENT_SS))) //NULL segment loaded into CS or SS?
	{
		return; //Not present: limit exceeded!	
	}

	SEGDESCRIPTOR_TYPE tempcontainer;
	if (EMULATED_CPU == CPU_80286) //80286 has less options?
	{
		if (LOADDESCRIPTOR(whatsegment,segment,&tempcontainer)) //Loaded the old container?
		{
			container->desc.base_high = tempcontainer.desc.base_high; //No high byte is present, so ignore the data to write!
			container->desc.limit_high = tempcontainer.desc.limit_high; //No high limit is present, so ingore the data to write!
		}
	}

	int i;
	for (i = 0;i<(int)sizeof(container->descdata);i++) //Process the descriptor data!
	{
		memory_directwb(descriptor_adress + i, container->descdata[i]); //Write a descriptor byte directly to flat memory!
	}
}

/*

getsegment_seg: Gets a segment, if allowed.
parameters:
	whatsegment: What segment is used?
	segment: The segment to get.
	isJMPorCALL: 0 for normal segment setting. 1 for JMP, 2 for CALL, 3 for IRET.
result:
	The segment when available, NULL on error or disallow.

*/

SEGMENT_DESCRIPTOR *getsegment_seg(int whatsegment, word segment, byte isJMPorCALL) //Get this corresponding segment descriptor (or LDT. For LDT, specify LDT register as segment) for loading into the segment descriptor cache!
{
	if (!LOADDESCRIPTOR(whatsegment,segment,&LOADEDDESCRIPTOR)) //Error loading current descriptor?
	{
		return NULL; //Error, by specified reason!
	}
	byte equalprivilege = 0; //Special gate stuff requirement: DPL must equal CPL? 1 for enable, 0 for normal handling.
	byte privilegedone = 0; //Privilege already calculated?
	byte is_gated = 0;
	byte is_TSS = 0; //Are we a TSS?
	if (isGateDescriptor(&LOADEDDESCRIPTOR) && (whatsegment == CPU_SEGMENT_CS) && isJMPorCALL) //Handling of gate descriptors?
	{
		is_gated = 1; //We're gated!
		memcpy(&GATEDESCRIPTOR, &LOADEDDESCRIPTOR, sizeof(GATEDESCRIPTOR)); //Copy the loaded descriptor to the GATE!
		if (MAX(getCPL(), getRPL(segment)) > GATEDESCRIPTOR.desc.DPL) //Gate has too high a privilege level?
		{
			THROWDESCGP(segment); //Throw error!
			return NULL; //We are a lower privilege level, so don't load!				
		}
		segment = (GATEDESCRIPTOR.desc.base_high << 3) | (segment & 7); //We're loading this segment now!
		if (!LOADDESCRIPTOR(whatsegment, segment, &LOADEDDESCRIPTOR)) //Error loading current descriptor?
		{
			THROWDESCGP(segment); //Throw error!
			return NULL; //Error, by specified reason!
		}
		privilegedone = 1; //Privilege has been precalculated!
		if ((LOADEDDESCRIPTOR.desc.Type & 0x1D) == 9) //Task gate?
		{
			if (whatsegment != CPU_SEGMENT_CS) //Not code? We're not a task switch! We're trying to load the task segment into a data register. This is illegal!
			{
				THROWDESCGP(segment); //Throw error!
				return NULL; //Don't load!
			}
		}
		else //Normal descriptor?
		{
			if (isJMPorCALL == 1 && !LOADEDDESCRIPTOR.desc.EXECSEGMENT.C) //JMP to a nonconforming segment?
			{
				if (LOADEDDESCRIPTOR.desc.DPL != getCPL()) //Different CPL?
				{
					THROWDESCGP(segment); //Throw error!
					return NULL; //We are a different privilege level, so don't load!						
				}
			}
			else if (isJMPorCALL) //Call instruction (or JMP instruction to a conforming segment)
			{
				if (LOADEDDESCRIPTOR.desc.DPL > getCPL()) //We have a lower CPL?
				{
					THROWDESCGP(segment); //Throw error!
					return NULL; //We are a different privilege level, so don't load!
				}
			}
		}
	}
	else if (!isGateDescriptor(&LOADEDDESCRIPTOR) && whatsegment==CPU_SEGMENT_CS && isJMPorCALL) //JMP/CALL to non-gate descriptor?
	{
		equalprivilege = 1; //Enforce equal privilege!
	}

	if (
		(
		(whatsegment==CPU_SEGMENT_SS) ||
		(whatsegment==CPU_SEGMENT_DS) ||
		(whatsegment==CPU_SEGMENT_ES) ||
		(whatsegment==CPU_SEGMENT_FS) ||
		(whatsegment==CPU_SEGMENT_GS) //SS,DS,ES,FS,GS are ...
		) &&
		(
		(getLoadedTYPE(&LOADEDDESCRIPTOR)==2) || //A System segment? OR ...
		((getLoadedTYPE(&LOADEDDESCRIPTOR)==1) && (!LOADEDDESCRIPTOR.desc.EXECSEGMENT.R)) //An execute-only code segment?
		)
		)
	{
		THROWDESCGP(segment); //Throw error!
		return NULL; //Not present: limit exceeded!	
	}
	
	//Now check for CPL,DPL&RPL! (chapter 6.3.2)
	if (
		(!privilegedone && !equalprivilege && MAX(getCPL(),getRPL(segment))>LOADEDDESCRIPTOR.desc.DPL && !(whatsegment==CPU_SEGMENT_CS && LOADEDDESCRIPTOR.desc.EXECSEGMENT.C)) || //We are a lower privilege level and non-conforming?
		((!privilegedone && equalprivilege && MAX(getCPL(),getRPL(segment))!=LOADEDDESCRIPTOR.desc.DPL) && //We must be at the same privilege level?
			!(LOADEDDESCRIPTOR.desc.EXECSEGMENT.C) //Not conforming checking further ahead makes sure that we don't double check things?
			)
		)
	{
		THROWDESCGP(segment); //Throw error!
		return NULL; //We are a lower privilege level, so don't load!
	}

	switch (LOADEDDESCRIPTOR.desc.Type) //We're a TSS? We're to perform a task switch!
	{
	case AVL_SYSTEM_BUSY_TSS16BIT:
	case AVL_SYSTEM_BUSY_TSS32BIT:
	case AVL_SYSTEM_TSS16BIT:
	case AVL_SYSTEM_TSS32BIT: //TSS?
		is_TSS = (LOADEDDESCRIPTOR.desc.nonS==0); //We're a TSS when not a system segment!
		break;
	default:
		is_TSS = 0; //We're no TSS!
		break;
	}

	if (is_TSS && (segment==CPU_SEGMENT_TR)) //We're a TSS? We're to perform a task switch!
	{
		if (segment & 2) //LDT lookup set?
		{
			THROWDESCGP(segment); //Throw error!
			return NULL; //We're an invalid TSS to call!
		}
		if (
			((whatsegment == CPU_SEGMENT_CS) ||
			(whatsegment == CPU_SEGMENT_SS) ||
			(whatsegment == CPU_SEGMENT_DS) ||
			(whatsegment == CPU_SEGMENT_ES) ||
			(whatsegment == CPU_SEGMENT_FS) ||
			(whatsegment == CPU_SEGMENT_GS)) //CS,SS,DS,ES,FS,GS are not allowed to be loaded with the TSS descriptor!
			&& ((!is_gated) && (!isJMPorCALL)) //We're not gated and not jumping/calling? Don't allow the task switch!
			)
		{
			THROWDESCGP(segment); //Throw error!
			return NULL; //We're an invalid TSS to call!
		}
		//Handle the task switch normally! We're allowed to use the TSS!
	}

	if ((whatsegment==CPU_SEGMENT_CS) && is_TSS) //Special stuff on CS, CPL, Task switch.
	{
		if (!LOADEDDESCRIPTOR.desc.P) //Not present?
		{
			THROWDESCSeg(segment,1); //Throw error!
			return NULL; //We're an invalid TSS to execute!
		}

		//Execute a normal task switch!
		if (CPU_switchtask(whatsegment,&LOADEDDESCRIPTOR,&segment,segment,isJMPorCALL)) //Switching to a certain task?
		{
			return NULL; //Error changing priviledges or anything else!
		}

		//We've properly switched to the destination task! Continue execution normally!
		return NULL; //Don't actually load CS with the descriptor: we've caused a task switch after all!
	}

	if (whatsegment == CPU_SEGMENT_CS) //Special stuff on normal CS register (conforming?), CPL.
	{
		if (LOADEDDESCRIPTOR.desc.EXECSEGMENT.C) //Conforming segment?
		{
			if (!privilegedone && LOADEDDESCRIPTOR.desc.DPL>getCPL()) //Target DPL must be less-or-equal to the CPL.
			{
				THROWDESCGP(segment); //Throw error!
				return NULL; //We are a lower privilege level, so don't load!				
			}
		}
		else //Non-conforming segment?
		{
			if (!privilegedone && LOADEDDESCRIPTOR.desc.DPL!=getCPL()) //Check for equal only when using Gate Descriptors?
			{
				THROWDESCGP(segment); //Throw error!
				return NULL; //We are a lower privilege level, so don't load!				
			}
			CPU[activeCPU].CPL = LOADEDDESCRIPTOR.desc.DPL; //New privilege level!
		}
	}

	return &LOADEDDESCRIPTOR.desc; //Give the segment descriptor read from memory!
}

uint_32 destEIP; //Destination address for CS JMP instruction!

void segmentWritten(int segment, word value, byte isJMPorCALL) //A segment register has been written to!
{
	if (CPU[activeCPU].faultraised) return; //Abort if already an fault has been raised!
	if (getcpumode()==CPU_MODE_PROTECTED) //Protected mode, must not be real or V8086 mode, so update the segment descriptor cache!
	{
		SEGMENT_DESCRIPTOR *descriptor = getsegment_seg(segment,value,isJMPorCALL); //Read the segment!
		if (descriptor) //Loaded&valid?
		{
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
		THROWDESCGP(0); //Throw a descriptor fault!
		return 0; //Not allowed to run!
	}
	return 1; //Allowed to run!
}

/*

MMU: memory start!

*/

uint_32 CPU_MMU_start(sword segment, word segmentval) //Determines the start of the segment!
{
	//Determine the Base!
	if (getcpumode()==CPU_MODE_PROTECTED) //Not Real or 8086 mode?
	{
		if (segment == -1) //Forced 8086 mode by the emulators?
		{
			return (segmentval << 4); //Behave like a 8086!
		}
	
		//Protected mode addressing!
		return ((CPU[activeCPU].SEG_DESCRIPTOR[segment].base_high<<24)|(CPU[activeCPU].SEG_DESCRIPTOR[segment].base_mid<<16)|CPU[activeCPU].SEG_DESCRIPTOR[segment].base_low); //Base!
	}

	return (segmentval<<4); //Behave like a 80(1)86!
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

	if (!descriptor->P) //Not present?
	{
		return 2; //#NP!
	}

	if (getcpumode()==CPU_MODE_PROTECTED) //Not real mode? Check rights!
	{
		if (segment == CPU_SEGMENT_CS && !(descriptor->EXECSEGMENT.ISEXEC) && (forreading == 3)) //Non-executable segment execution?
		{
			return 1; //Error!
		}
		else if (((descriptor->EXECSEGMENT.ISEXEC) || !(descriptor->DATASEGMENT.OTHERSTRUCT || descriptor->DATASEGMENT.W)) && !forreading) //Writing to executable segment or read-only data segment?
		{
			return 1; //Error!
		}
		else if (descriptor->EXECSEGMENT.ISEXEC && !descriptor->EXECSEGMENT.R && forreading == 1) //Reading execute-only segment?
		{
			return 1; //Error!	
		}
	}

	//Next: limit checking!

	uint_32 limit; //The limit!
	byte isvalid;

	limit = ((descriptor->limit_high << 8) | descriptor->limit_low); //Base limit!

	if (descriptor->G && (EMULATED_CPU>=CPU_80386)) //Granularity?
	{
		limit = ((limit << 12) | 0xFFF); //4KB for a limit of 4GB, fill lower 12 bits with 1!
	}

	if (addrtest) //Execute address test?
	{
		isvalid = (offset<=limit); //Valid address range!
		if ((descriptor->nonS == 1) && ((descriptor->Type & 4) == 0)) //Data segment?
		{
			if (descriptor->DATASEGMENT.E) //Expand-down segment?
			{
				isvalid = !isvalid; //Reversed valid!
				if (descriptor->G == 0) //Small granularity?
				{
					isvalid = (isvalid && (offset <= 0x10000)); //Limit to 64K!
				}
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
			if (!((MAX(getCPL(), getRPL(segmentval)) <= descriptor->DPL) || isconforming)) //Invalid privilege?
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
			if (segment!=-1) //Normal operations (called for the CPU for sure)?
			{
				return (offset>0xFFFF); //Behave like a 8086: overflow casts error!
			}
			else //System hardware (emulator itself)?
			{
				return 0; //Enable all, we're direct after all!
			}
		}
		
		//Use segment descriptors, even when in real mode on 286+ processors!
		switch (CPU_MMU_checkrights(segment,segmentval, offset, forreading, &CPU[activeCPU].SEG_DESCRIPTOR[segment],1)) //What rights resulting? Test the address itself too!
		{
		case 0: //OK?
			break; //OK!
		default: //Unknown status? Count #GP by default!
		case 1: //#GP?
			THROWDESCGP(segmentval); //Throw fault!
			return 1; //Error out!
			break;
		case 2: //#NP?
			THROWDESCSeg(segment, 0); //Throw error: accessing non-present segment descriptor!
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
	if (CPU[activeCPU].registers->SFLAGS.IOPL > getCPL()) //We're not allowed!
	{
		return 1; //Not priviledged!
	}
	return 0; //Priviledged!
}

byte checkSTICLI() //Check STI/CLI rights!
{
	if (checkSpecialRights()) //Not priviledged?
	{
		THROWDESCGP(CPU[activeCPU].registers->CS); //Raise exception!
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
		//if ((CPU->SEG_DESCRIPTOR[CPU_SEGMENT_TR].Type == AVL_SYSTEM_BUSY_TSS16BIT) || (CPU->SEG_DESCRIPTOR[CPU_SEGMENT_TR].Type == AVL_SYSTEM_TSS16BIT)) //16-bit TSS?
		if ((CPU->SEG_DESCRIPTOR[CPU_SEGMENT_TR].Type == AVL_SYSTEM_BUSY_TSS32BIT) || (CPU->SEG_DESCRIPTOR[CPU_SEGMENT_TR].Type == AVL_SYSTEM_TSS32BIT)) //32-bit TSS?
		{
			uint_32 limit;
			limit = CPU->SEG_DESCRIPTOR[CPU_SEGMENT_TR].limit_low | (CPU->SEG_DESCRIPTOR[CPU_SEGMENT_TR].limit_high << 16); //The limit of the descriptor!
			maplocation += MMU_rw(CPU_SEGMENT_TR, CPU->registers->TR,0x66,0); //Add the map location to the specified address!
			if (maplocation >= limit) //Over the limit? We're an invalid entry or got no bitmap!
			{
				return 1; //We're to cause an exception!
			}
			if (MMU_rb(CPU_SEGMENT_TR, CPU->registers->TR, maplocation, 0)&mappos) //We're to cause an exception: we're not allowed to access this port!
			{
				return 1; //We're to cause an exception!
			}
		}
	}
	return 0; //Allow all for now!
}

int LOADINTDESCRIPTOR(int whatsegment, word segment, SEGDESCRIPTOR_TYPE *container)
{
	uint_32 descriptor_adress = 0;
	descriptor_adress = (segment & 4) ? ((CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_low | (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_mid << 16)) | CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_high << 24) : CPU[activeCPU].registers->GDTR.base; //LDT/GDT selector!
	uint_32 descriptor_index = getDescriptorIndex(segment); //The full index within the descriptor table!

	descriptor_index <<= 3; //Multiply into range!

	if ((word)(descriptor_index>>3)>=((segment & 4) ? (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_low | (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_high << 16)) : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
	{
		THROWDESCGP(segment); //Throw error!
		return 0; //Not present: limit exceeded!
	}

	if (!descriptor_index) //NULL segment loaded into CS?
	{
		THROWDESCGP(segment); //Throw error!
		return 0; //Not present: limit exceeded!	
	}

	int i;
	for (i = 0;i<(int)sizeof(container->descdata);i++) //Process the descriptor data!
	{
		container->descdata[i] = memory_directrb(descriptor_adress + i); //Read a descriptor byte directly from flat memory!
	}

	if ((whatsegment == CPU_SEGMENT_CS) &&
		(
		(getLoadedTYPE(container) != 1 && !isGateDescriptor(container)) || //Data or System in CS (non-exec)?
			(!getLoadedTYPE(container) && !isGateDescriptor(container)) //Or System?
			)
		)

	{
		THROWDESCGP(segment); //Throw error!
		return 0; //Not present: limit exceeded!	
	}

	return 1; //OK!
}

void CPU_ProtectedModeInterrupt(byte intnr, byte is_HW, word returnsegment, uint_32 returnoffset, uint_32 error) //Execute a protected mode interrupt!
{
	SEGDESCRIPTOR_TYPE newdescriptor; //Temporary storage for task switches!
	word desttask; //Destination task for task switches!
	byte left; //The amount of bytes left to read of the IDT entry!
	uint_32 base;
	base = (intnr<<3); //The base offset of the interrupt in the IDT!
	if ((base + 5) >= CPU->registers->IDTR.limit) //Limit exceeded?
	{
		THROWDESCGP(base+2+(is_HW?1:0)); //#GP!
		return; //Abort!
	}

	base += CPU->registers->IDTR.base; //Add the base for the actual offset into the IDT!
	
	IDTENTRY idtentry; //The loaded IVT entry!
	for (left=0;left<sizeof(idtentry.descdata);++left) //Data left to read?
	{
		idtentry.descdata[left] = memory_directrb(base++); //Read a byte from the descriptor entry!
	}

	byte is32bit;

	if ((!is_HW) && (idtentry.DPL < getCPL())) //Not enough rights?
	{
		THROWDESCGP(base + 2 + (is_HW ? 1 : 0)); //#GP!
		return;
	}

	if (idtentry.P==0) //Not present?
	{
		THROWDESCSeg(base + 2,is_HW); //#NP!
		return;
	}

	//Now, the (gate) descriptor to use is loaded!
	switch (idtentry.Type) //What type are we?
	{
	case IDTENTRY_32BIT_TASKGATE: //32-bit task gate?
		desttask = idtentry.selector; //Read the destination task!
		if (!LOADDESCRIPTOR(CPU_SEGMENT_TR, desttask, &newdescriptor)) //Error loading new descriptor? The backlink is always at the start of the TSS!
		{
			return; //Error, by specified reason!
		}
		CPU_switchtask(CPU_SEGMENT_TR, &newdescriptor, &CPU[activeCPU].registers->TR, desttask, 0); //Execute a task switch to the new task!
		if (is_HW && (error != -1))
		{
			CPU_PUSH32(&error); //Push the error on the stack!
		}
		break;
	default: //All other cases?
		is32bit = idtentry.Type&IDTENTRY_32BIT_GATEEXTENSIONFLAG; //Enable 32-bit gate?
		switch (idtentry.Type & 0x7) //What type are we?
		{
		case IDTENTRY_16BIT_INTERRUPTGATE: //16/32-bit interrupt gate?
		case IDTENTRY_16BIT_TRAPGATE: //16/32-bit trap gate?
			if (!LOADINTDESCRIPTOR(CPU_SEGMENT_CS, idtentry.selector, &newdescriptor)) //Error loading new descriptor? The backlink is always at the start of the TSS!
			{
				return; //Error, by specified reason!
			}
			if (((newdescriptor.desc.nonS) || (newdescriptor.desc.EXECSEGMENT.ISEXEC==0)) || (newdescriptor.desc.EXECSEGMENT.R==0)) //Not readable, execute segment or is non-System segment?
			{
				THROWDESCGP(idtentry.selector); //Throw #GP!
				return;
			}
			if ((idtentry.offsetlow | (idtentry.offsethigh << 16)) > (newdescriptor.desc.limit_low | (newdescriptor.desc.limit_high << 16))) //Limit exceeded?
			{
				THROWDESCGP(is_HW?1:0); //Throw #GP!
				return;
			}

			if ((newdescriptor.desc.EXECSEGMENT.C == 0) && (newdescriptor.desc.DPL < getCPL())) //Not enough rights, but conforming?
			{
				//TODO
			}
			else
			{
				//Check permission? TODO!
			}

			if (is32bit)
			{
				CPU_PUSH32(&CPU[activeCPU].registers->EFLAGS); //Push EFLAGS!
				if (CPU[activeCPU].faultraised) return; //Abort on fault!
			}
			else
			{
				CPU_PUSH16(&CPU[activeCPU].registers->FLAGS); //Push FLAGS!
				if (CPU[activeCPU].faultraised) return; //Abort on fault!
			}

			CPU_PUSH16(&CPU[activeCPU].registers->CS); //Push CS!
			if (CPU[activeCPU].faultraised) return; //Abort on fault!

			if (is32bit)
			{
				CPU_PUSH32(&CPU[activeCPU].registers->EIP); //Push EIP!
				if (CPU[activeCPU].faultraised) return; //Abort on fault!
			}
			else
			{
				CPU_PUSH16(&CPU[activeCPU].registers->IP); //Push IP!
				if (CPU[activeCPU].faultraised) return; //Abort on fault!
			}

			memcpy(&CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS], &newdescriptor, sizeof(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS])); //Load the segment descriptor into the cache!
			//if (memprotect(CPU[activeCPU].SEGMENT_REGISTERS[segment],2,"CPU_REGISTERS")) //Valid segment register?
			{
				*CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_CS] = idtentry.selector; //Set the segment register to the allowed value!
			}

			CPU[activeCPU].registers->EIP = (idtentry.offsetlow | (idtentry.offsethigh << 16)); //The current OPCode: just jump to the address specified by the descriptor OR command!
			CPU_flushPIQ(); //We're jumping to another address!

			CPU[activeCPU].registers->SFLAGS.TF = 0;
			CPU[activeCPU].registers->SFLAGS.NT = 0;

			if ((idtentry.Type & 0x7) == IDTENTRY_16BIT_INTERRUPTGATE)
			{
				CPU[activeCPU].registers->SFLAGS.IF = 0; //No interrupts!
			}
			break;
		default: //Unknown descriptor type?
			THROWDESCGP(base + 2 + (is_HW ? 1 : 0)); //#GP!
			break;
		}
		break;
	}
}