#include "headers/cpu/cpu.h" //CPU reqs!
#include "headers/cpu/multitasking.h" //Our typedefs!

//Everything concerning TSS.

byte SwitchPrivileges = 0xFF; //Default: don't switch privileges!

int TSS_PrivilegeChanges(int whatsegment,SEGDESCRIPTOR_TYPE *LOADEDDESCRIPTOR,word segment)
{
	//Affect SwitchPrivileges!
	return 1; //Error: not build yet!
}

byte CPU_switchtask(int whatsegment, SEGDESCRIPTOR_TYPE *LOADEDDESCRIPTOR,word *segment, word destinationtask, byte isJMPorCALL) //Switching to a certain task?
{
	//Both structures to use for the TSS!
	word LDTsegment;
	word n;
	word oldtask;
	byte TSS_dirty = 0; //Is the new TSS dirty?
	union
	{
		TSS286 TSS;
		byte data[44]; //All our data!
	} TSS16;
	union
	{
		TSS386 TSS;
		byte data[104]; //All our data!
	} TSS32;
	byte TSSSize = 0; //The TSS size!
	uint_32 limit; //The limit we use!
	if (LOADEDDESCRIPTOR->desc.P==0) //Not present?
	{
		THROWDESCSeg(destinationtask,0); //Throw #NP!
		return 1; //Error out!
	}

	switch (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR].Type) //Check the type of descriptor we're executing!
	{
	case AVL_SYSTEM_BUSY_TSS16BIT:
	case AVL_SYSTEM_TSS16BIT:
		TSSSize = 0; //16-bit TSS!
		break;
	case AVL_SYSTEM_BUSY_TSS32BIT:
	case AVL_SYSTEM_TSS32BIT: //Valid descriptor?
		TSSSize = 1; //32-bit TSS!
		if (EMULATED_CPU>=CPU_80386) break; //Continue normally: we're valid!
	default: //Invalid descriptor!
		THROWDESCGP(CPU->registers->TR); //Thow #GP!
		return 1; //Error out!
	}

	if (LOADEDDESCRIPTOR->desc.D_B) //Busy?
	{
		THROWDESCGP(destinationtask); //Thow #GP!
		return 1; //Error out!
	}

	limit = LOADEDDESCRIPTOR->desc.limit_low; //Low limit (286+)!
	if (EMULATED_CPU >= CPU_80386) //Gotten high limit?
	{
		limit |= (LOADEDDESCRIPTOR->desc.limit_high << 16); //High limit too!
	}

	if (limit < (uint_32)((EMULATED_CPU==CPU_80286)?43:103)) //Limit isn't high enough(>=103 for 386+, >=43 for 80286)?
	{
		CPU_TSSFault(destinationtask); //Throw #TS!
		return 1; //Error out!
	}

	//Now going to switch to the current task, save the registers etc in the current task!
	if (TSSSize) //32-bit TSS?
	{
		for (n = 0;n < sizeof(TSS32);++n) //Load our TSS!
		{
			TSS32.data[n] = MMU_rb(CPU_SEGMENT_TR,CPU->registers->TR,n,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
		}
	}
	else //16-bit TSS?
	{
		for (n = 0;n < sizeof(TSS16);++n) //Load our TSS!
		{
			TSS16.data[n] = MMU_rb(CPU_SEGMENT_TR, CPU->registers->TR, n, 0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
		}
	}

	if (isJMPorCALL != 2) //Not a call?
	{
		SEGDESCRIPTOR_TYPE tempdesc;
		if (LOADDESCRIPTOR(CPU_SEGMENT_TR,CPU->registers->TR,&tempdesc)) //Loaded old container?
		{
			tempdesc.desc.D_B = 0; //Mark idle!
			SAVEDESCRIPTOR(CPU_SEGMENT_TR,CPU->registers->TR,&tempdesc); //Save the new status into the old descriptor!
		}
	}

	if (isJMPorCALL == 3) //IRET?
	{
		CPU->registers->SFLAGS.NT = 0; //Clear Nested Task flag!
	}

	//16 or 32-bit TSS is loaded, now save the registers!
	if (TSSSize) //We're a 32-bit TSS?
	{
		TSS32.TSS.EAX = CPU->registers->EAX;
		TSS32.TSS.ECX = CPU->registers->ECX;
		TSS32.TSS.EDX = CPU->registers->EDX;
		TSS32.TSS.EBX = CPU->registers->EBX;
		TSS32.TSS.ESP = CPU->registers->ESP;
		TSS32.TSS.EBP = CPU->registers->EBP;
		TSS32.TSS.ESI = CPU->registers->ESI;
		TSS32.TSS.EDI = CPU->registers->EDI;
		TSS32.TSS.CS = CPU->registers->CS;
		TSS32.TSS.SS = CPU->registers->SS;
		TSS32.TSS.DS = CPU->registers->DS;
		TSS32.TSS.FS = CPU->registers->FS;
		TSS32.TSS.GS = CPU->registers->GS;
		TSS32.TSS.EFLAGS = CPU->registers->EFLAGS;
	}
	else //We're a 16-bit TSS?
	{
		TSS16.TSS.AX = CPU->registers->AX;
		TSS16.TSS.CX = CPU->registers->CX;
		TSS16.TSS.DX = CPU->registers->DX;
		TSS16.TSS.BX = CPU->registers->BX;
		TSS16.TSS.SP = CPU->registers->SP;
		TSS16.TSS.BP = CPU->registers->BP;
		TSS16.TSS.SI = CPU->registers->SI;
		TSS16.TSS.DI = CPU->registers->DI;
		TSS16.TSS.CS = CPU->registers->CS;
		TSS16.TSS.SS = CPU->registers->SS;
		TSS16.TSS.DS = CPU->registers->DS;
		TSS16.TSS.FLAGS = CPU->registers->FLAGS;
	}

	if (TSSSize) //32-bit TSS?
	{
		for (n = 0;n < sizeof(TSS32);++n) //Load our TSS!
		{
			MMU_wb(CPU_SEGMENT_TR, CPU->registers->TR, n, TSS32.data[n]); //Write the TSS! Don't be afraid of errors, since we're always accessable!
		}
	}
	else //16-bit TSS?
	{
		for (n = 0;n < sizeof(TSS16);++n) //Load our TSS!
		{
			MMU_wb(CPU_SEGMENT_TR, CPU->registers->TR, n, TSS16.data[n]); //Write the TSS! Don't be afraid of errors, since we're always accessable!
		}
	}

	oldtask = CPU->registers->TR; //Save the old task, for backlink purposes!

	//Now, load all the registers required as needed!
	CPU[activeCPU].faultraised = 0; //We have no fault raised: we need this to run the segment change!
	segmentWritten(CPU_SEGMENT_TR,destinationtask,0x40); //Execute the task switch itself, loading our new descriptor!

	switch (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR].Type) //Check the type of descriptor we're executing now!
	{
	case AVL_SYSTEM_BUSY_TSS16BIT:
	case AVL_SYSTEM_TSS16BIT:
		TSSSize = 0; //16-bit TSS!
		break;
	case AVL_SYSTEM_BUSY_TSS32BIT:
	case AVL_SYSTEM_TSS32BIT: //Valid descriptor?
		TSSSize = 1; //32-bit TSS!
		if (EMULATED_CPU >= CPU_80386) break; //Continue normally: we're valid!
	default: //Invalid descriptor!
		THROWDESCGP(destinationtask); //Thow #GP!
		return 1; //Error out!
	}

	//Load the new TSS!
	if (TSSSize) //32-bit TSS?
	{
		for (n = 0;n < sizeof(TSS32);++n) //Load our TSS!
		{
			TSS32.data[n] = MMU_rb(CPU_SEGMENT_TR, CPU->registers->TR, n, 0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
		}
	}
	else //16-bit TSS?
	{
		for (n = 0;n < sizeof(TSS16);++n) //Load our TSS!
		{
			TSS16.data[n] = MMU_rb(CPU_SEGMENT_TR, CPU->registers->TR, n, 0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
		}
	}

	if (TSSSize) //32-bit TSS?
	{
		if (isJMPorCALL == 2) //CALL?
		{
			TSS32.TSS.BackLink = oldtask; //Save the old task as a backlink in the new task!
			TSS_dirty = 1; //We're dirty!
		}
	}
	else //16-bit TSS?
	{
		if (isJMPorCALL == 2) //CALL?
		{
			TSS16.TSS.BackLink = oldtask; //Save the old task as a backlink in the new task!
			TSS_dirty = 1; //We're dirty!
		}
	}

	if (TSS_dirty) //Destination TSS dirty?
	{
		if (TSSSize) //32-bit TSS?
		{
			for (n = 0;n < sizeof(TSS32);++n) //Load our TSS!
			{
				MMU_wb(CPU_SEGMENT_TR, CPU->registers->TR, n, TSS32.data[n]); //Write the TSS! Don't be afraid of errors, since we're always accessable!
			}
		}
		else //16-bit TSS?
		{
			for (n = 0;n < sizeof(TSS16);++n) //Load our TSS!
			{
				MMU_wb(CPU_SEGMENT_TR, CPU->registers->TR, n, TSS16.data[n]); //Write the TSS! Don't be afraid of errors, since we're always accessable!
			}
		}
	}

	if (isJMPorCALL != 3) //Not an IRET?
	{
		LOADEDDESCRIPTOR->desc.D_B = 1; //Mark not idle!
		SAVEDESCRIPTOR(CPU_SEGMENT_TR, CPU->registers->TR, LOADEDDESCRIPTOR); //Save the new status into the old descriptor!
	}

	//Now we're ready to load all registers!
	if (TSSSize) //We're a 32-bit TSS?
	{
		CPU->registers->EAX = TSS32.TSS.EAX;
		CPU->registers->ECX = TSS32.TSS.ECX;
		CPU->registers->EDX = TSS32.TSS.EDX;
		CPU->registers->EBX = TSS32.TSS.EBX;
		CPU->registers->ESP = TSS32.TSS.ESP;
		CPU->registers->EBP = TSS32.TSS.EBP;
		CPU->registers->ESI = TSS32.TSS.ESI;
		CPU->registers->EDI = TSS32.TSS.EDI;
		CPU->registers->CR3_full = TSS32.TSS.CR3; //Load the new CR3 register to use the new Paging table!
		CPU->registers->EFLAGS = TSS32.TSS.EFLAGS;
		LDTsegment = TSS32.TSS.LDT; //LDT used!
	}
	else //We're a 16-bit TSS?
	{
		CPU->registers->AX = TSS16.TSS.AX;
		CPU->registers->CX = TSS16.TSS.CX;
		CPU->registers->DX = TSS16.TSS.DX;
		CPU->registers->BX = TSS16.TSS.BX;
		CPU->registers->SP = TSS16.TSS.SP;
		CPU->registers->BP = TSS16.TSS.BP;
		CPU->registers->SI = TSS16.TSS.SI;
		CPU->registers->DI = TSS16.TSS.DI;
		CPU->registers->FLAGS = TSS16.TSS.FLAGS;
		LDTsegment = TSS16.TSS.LDT; //LDT used!
	}

	//Check and verify the LDT descriptor!
	SEGDESCRIPTOR_TYPE LDTsegdesc;
	uint_32 descriptor_adress = 0;
	descriptor_adress = (LDTsegment & 4) ? ((CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_low|(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_mid<<16))| CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_high<<24) : CPU[activeCPU].registers->GDTR.base; //LDT/GDT selector!
	uint_32 descriptor_index = getDescriptorIndex(LDTsegment); //The full index within the descriptor table!

	if ((word)descriptor_index>((LDTsegment & 4) ? (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_low|(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_high<<16)) : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
	{
		CPU_TSSFault(CPU->registers->TR); //Throw error!
		return 0; //Not present: limit exceeded!
	}

	if ((!descriptor_index) /*&& ((whatsegment == CPU_SEGMENT_CS) || (whatsegment == CPU_SEGMENT_SS))*/) //NULL segment loaded into CS or SS?
	{
		THROWDESCGP(CPU->registers->TR); //Throw error!
		return 0; //Not present: limit exceeded!
	}

	int i;
	for (i = 0;i<(int)sizeof(LDTsegdesc.descdata);i++) //Process the descriptor data!
	{
		LDTsegdesc.descdata[i] = memory_directrb(descriptor_adress + i); //Read a descriptor byte directly from flat memory!
	}

	//Now the LDT entry is loaded for testing!
	if (LDTsegdesc.desc.Type != AVL_SYSTEM_LDT) //Not an LDT?
	{
		THROWDESCGP(CPU->registers->TR); //Throw error!
		return 1; //Not present: not an IDT!	
	}

	if (!LDTsegdesc.desc.P) //Not present?
	{
		THROWDESCGP(CPU->registers->TR); //Throw error!
		return 1; //Not present: not an IDT!	
	}

	CPU[activeCPU].registers->CR0.TS |= 2; //Set the high bit of the TS bit(bit 3)!

	//Now, load all normal registers in order, keeping aborts possible!
	CPU[activeCPU].faultraised = 0; //Clear the fault level: the new task has no faults by default!

	//First, load CS!
	if (TSSSize) //32-bit?
	{
		segmentWritten(CPU_SEGMENT_CS,TSS32.TSS.CS,0); //Load CS!
	}
	else
	{
		segmentWritten(CPU_SEGMENT_CS, TSS16.TSS.CS, 0); //Load CS!
	}
	if (CPU[activeCPU].faultraised) return 0; //Abort on fault raised!
	if (getCPL() != CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR].DPL) //Non-matching TSS DPL vs CS CPL?
	{
		CPU_TSSFault(CPU->registers->TR); //Throw error!
		return 0; //Not present: limit exceeded!
	}

	if (TSSSize) //32-bit?
	{
		segmentWritten(CPU_SEGMENT_DS, TSS32.TSS.DS, 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 0; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_ES, TSS32.TSS.ES, 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 0; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_FS, TSS32.TSS.FS, 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 0; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_GS, TSS32.TSS.GS, 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 0; //Abort on fault raised!
	}
	else //16-bit?
	{
		segmentWritten(CPU_SEGMENT_DS, TSS16.TSS.DS, 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 0; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_ES, TSS16.TSS.ES, 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 0; //Abort on fault raised!
	}

	//All segments are valid and readable!

	return 1; //Abort any running instruction operation!
}

void CPU_TSSFault(uint_32 errorcode)
{
	CPU_resetOP(); //Point to the faulting instruction!
	
	if (CPU_faultraised()) //We're raising a fault!
	{
		call_hard_inthandler(EXCEPTION_INVALIDTSSSEGMENT); //Call IVT entry #13 decimal!
		CPU_PUSH32(&errorcode); //Error code!
	}
}