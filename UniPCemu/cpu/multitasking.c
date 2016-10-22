#include "headers/cpu/cpu.h" //CPU reqs!
#include "headers/cpu/multitasking.h" //Our typedefs!
#include "headers/mmu/mmuhandler.h" //Direct MMU support!
#include "headers/cpu/cpu_pmtimings.h" //286+ timing support!

//Force 16-bit TSS on 80286?
//#define FORCE_16BITTSS

//Everything concerning TSS.

extern byte hascallinterrupttaken_type; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task gate flag. Left at 0xFF when nothing is used(unknown case?)

byte CPU_switchtask(int whatsegment, SEGDESCRIPTOR_TYPE *LOADEDDESCRIPTOR,word *segment, word destinationtask, byte isJMPorCALL, byte gated, byte is_external) //Switching to a certain task?
{
	byte isStackSwitch = 0; //Stack switch?
	byte destStack = 3; //Destination stack!
	//Both structures to use for the TSS!
	word LDTsegment;
	word n;
	word oldtask;
	byte busy=0;
	byte TSS_dirty = 0; //Is the new TSS dirty?
	union
	{
		TSS286 TSS;
		byte data[44]; //All our data!
	} TSS16;
	union
	{
		TSS386 TSS;
		byte data[108]; //All our data!
	} TSS32;
	byte TSSSize = 0; //The TSS size!

	if (LOADEDDESCRIPTOR->desc.DPL != getCPL()) //Different CPL? Stack switch?
	{
		destStack = LOADEDDESCRIPTOR->desc.DPL; //Switch to this stack!
		isStackSwitch = 1; //Switching stacks!
	}

	uint_32 limit; //The limit we use!
	if (LOADEDDESCRIPTOR->desc.P==0) //Not present?
	{
		THROWDESCNP(destinationtask,is_external,(destinationtask&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #NP!
		return 1; //Error out!
	}

	switch (LOADEDDESCRIPTOR->desc.Type) //Check the type of descriptor we're executing!
	{
	case AVL_SYSTEM_BUSY_TSS16BIT:
		busy = 1;
	case AVL_SYSTEM_TSS16BIT:
		TSSSize = 0; //16-bit TSS!
		break;
	case AVL_SYSTEM_BUSY_TSS32BIT:
		busy = 1;
	case AVL_SYSTEM_TSS32BIT: //Valid descriptor?
		TSSSize = 1; //32-bit TSS!
		/*
		if (EMULATED_CPU < CPU_80386) //Continue normally: we're valid on a 80386 only?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR); //Thow #GP!
		}
		*/
		break;
	default: //Invalid descriptor!
		THROWDESCGP(CPU[activeCPU].registers->TR,is_external,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Thow #GP!
		return 1; //Error out!
	}

	#ifdef FORCE_16BITTSS
		if (EMULATED_CPU == CPU_80286) TSSSize = 0; //Force 16-bit TSS on 286!
	#endif

	if (busy) //Busy?
	{
		THROWDESCGP(destinationtask,is_external,(destinationtask&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Thow #GP!
		return 1; //Error out!
	}

	limit = LOADEDDESCRIPTOR->desc.limit_low; //Low limit (286+)!
	if (EMULATED_CPU >= CPU_80386) //Gotten high limit?
	{
		limit |= (LOADEDDESCRIPTOR->desc.limit_high << 16); //High limit too!
	}

	if (limit < (uint_32)((EMULATED_CPU==CPU_80286)?43:103)) //Limit isn't high enough(>=103 for 386+, >=43 for 80286)?
	{
		CPU_TSSFault(destinationtask,is_external,(destinationtask&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #TS!
		return 1; //Error out!
	}

	//Now going to switch to the current task, save the registers etc in the current task!
	if (CPU[activeCPU].registers->TR) //We have an active program?
	{
		if (TSSSize) //32-bit TSS?
		{
			for (n = 0;n < sizeof(TSS32);++n) //Load our TSS!
			{
				TSS32.data[n] = MMU_rb(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,n,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
			}
		}
		else //16-bit TSS?
		{
			for (n = 0;n < sizeof(TSS16);++n) //Load our TSS!
			{
				TSS16.data[n] = MMU_rb(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, 0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
			}
		}
	}
	else //Unknown size?
	{
		if (TSSSize) //32-bit TSS?
		{
			memset(&TSS32.data,0,sizeof(TSS32.data)); //Read the TSS! Don't be afraid of errors, since we're always accessable!
		}
		else //16-bit TSS?
		{
			memset(&TSS16.data, 0, sizeof(TSS16.data)); //Read the TSS! Don't be afraid of errors, since we're always accessable!
		}
	}

	if (CPU[activeCPU].registers->TR) //Valid task to switch FROM?
	{
		if (isJMPorCALL != 2) //Not a call?
		{
			SEGDESCRIPTOR_TYPE tempdesc;
			if (LOADDESCRIPTOR(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,&tempdesc)) //Loaded old container?
			{
				tempdesc.desc.Type &= ~2; //Mark idle!
				SAVEDESCRIPTOR(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,&tempdesc); //Save the new status into the old descriptor!
			}
		}

		if (isJMPorCALL == 3) //IRET?
		{
			CPU[activeCPU].registers->SFLAGS.NT = 0; //Clear Nested Task flag!
		}

		//16 or 32-bit TSS is loaded, now save the registers!
		if (TSSSize) //We're a 32-bit TSS?
		{
			TSS32.TSS.EAX = CPU[activeCPU].registers->EAX;
			TSS32.TSS.ECX = CPU[activeCPU].registers->ECX;
			TSS32.TSS.EDX = CPU[activeCPU].registers->EDX;
			TSS32.TSS.EBX = CPU[activeCPU].registers->EBX;
			TSS32.TSS.ESP = CPU[activeCPU].registers->ESP;
			TSS32.TSS.EBP = CPU[activeCPU].registers->EBP;
			TSS32.TSS.ESI = CPU[activeCPU].registers->ESI;
			TSS32.TSS.EDI = CPU[activeCPU].registers->EDI;
			TSS32.TSS.CS = CPU[activeCPU].registers->CS;
			TSS32.TSS.SS = CPU[activeCPU].registers->SS;
			TSS32.TSS.DS = CPU[activeCPU].registers->DS;
			TSS32.TSS.FS = CPU[activeCPU].registers->FS;
			TSS32.TSS.GS = CPU[activeCPU].registers->GS;
			TSS32.TSS.EFLAGS = CPU[activeCPU].registers->EFLAGS;
		}
		else //We're a 16-bit TSS?
		{
			TSS16.TSS.AX = CPU[activeCPU].registers->AX;
			TSS16.TSS.CX = CPU[activeCPU].registers->CX;
			TSS16.TSS.DX = CPU[activeCPU].registers->DX;
			TSS16.TSS.BX = CPU[activeCPU].registers->BX;
			TSS16.TSS.SP = CPU[activeCPU].registers->SP;
			TSS16.TSS.BP = CPU[activeCPU].registers->BP;
			TSS16.TSS.SI = CPU[activeCPU].registers->SI;
			TSS16.TSS.DI = CPU[activeCPU].registers->DI;
			TSS16.TSS.CS = CPU[activeCPU].registers->CS;
			TSS16.TSS.SS = CPU[activeCPU].registers->SS;
			TSS16.TSS.DS = CPU[activeCPU].registers->DS;
			TSS16.TSS.FLAGS = CPU[activeCPU].registers->FLAGS;
		}

		if (TSSSize) //32-bit TSS?
		{
			for (n = 0;n < sizeof(TSS32);++n) //Load our TSS!
			{
				MMU_wb(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, TSS32.data[n]); //Write the TSS! Don't be afraid of errors, since we're always accessable!
			}
		}
		else //16-bit TSS?
		{
			for (n = 0;n < sizeof(TSS16);++n) //Load our TSS!
			{
				MMU_wb(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, TSS16.data[n]); //Write the TSS! Don't be afraid of errors, since we're always accessable!
			}
		}
	}


	oldtask = CPU[activeCPU].registers->TR; //Save the old task, for backlink purposes!

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
		/*
		if (EMULATED_CPU < CPU_80386) //Continue normally: we're valid on a 80386 only?
		{
		THROWDESCGP(CPU[activeCPU].registers->TR); //Thow #GP!
		}
		*/
		break;
	default: //Invalid descriptor!
		THROWDESCGP(destinationtask,is_external,(destinationtask&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Thow #GP!
		return 1; //Error out!
	}

	#ifdef FORCE_16BITTSS
		if (EMULATED_CPU == CPU_80286) TSSSize = 0; //Force 16-bit TSS on 286!
	#endif

	//Load the new TSS!
	if (TSSSize) //32-bit TSS?
	{
		for (n = 0;n < sizeof(TSS32);++n) //Load our TSS!
		{
			TSS32.data[n] = MMU_rb(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, 0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
		}
	}
	else //16-bit TSS?
	{
		for (n = 0;n < sizeof(TSS16);++n) //Load our TSS!
		{
			TSS16.data[n] = MMU_rb(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, 0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
		}
	}

	if (TSSSize) //32-bit TSS?
	{
		if (isJMPorCALL == 2 && oldtask) //CALL?
		{
			TSS32.TSS.BackLink = oldtask; //Save the old task as a backlink in the new task!
			TSS_dirty = 1; //We're dirty!
		}
	}
	else //16-bit TSS?
	{
		if (isJMPorCALL == 2 && oldtask) //CALL?
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
				MMU_wb(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, TSS32.data[n]); //Write the TSS! Don't be afraid of errors, since we're always accessable!
			}
		}
		else //16-bit TSS?
		{
			for (n = 0;n < sizeof(TSS16);++n) //Load our TSS!
			{
				MMU_wb(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, TSS16.data[n]); //Write the TSS! Don't be afraid of errors, since we're always accessable!
			}
		}
	}

	if (isJMPorCALL != 3) //Not an IRET?
	{
		LOADEDDESCRIPTOR->desc.Type |= 2; //Mark not idle!
		SAVEDESCRIPTOR(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, LOADEDDESCRIPTOR); //Save the new status into the old descriptor!
	}

	//Now we're ready to load all registers!
	if (TSSSize) //We're a 32-bit TSS?
	{
		CPU[activeCPU].registers->EAX = TSS32.TSS.EAX;
		CPU[activeCPU].registers->ECX = TSS32.TSS.ECX;
		CPU[activeCPU].registers->EDX = TSS32.TSS.EDX;
		CPU[activeCPU].registers->EBX = TSS32.TSS.EBX;
		CPU[activeCPU].registers->ESP = TSS32.TSS.ESP;
		CPU[activeCPU].registers->EBP = TSS32.TSS.EBP;
		CPU[activeCPU].registers->ESI = TSS32.TSS.ESI;
		CPU[activeCPU].registers->EDI = TSS32.TSS.EDI;
		CPU[activeCPU].registers->CR3_full = TSS32.TSS.CR3; //Load the new CR3 register to use the new Paging table!
		CPU[activeCPU].registers->EFLAGS = TSS32.TSS.EFLAGS;
		LDTsegment = TSS32.TSS.LDT; //LDT used!
	}
	else //We're a 16-bit TSS?
	{
		CPU[activeCPU].registers->AX = TSS16.TSS.AX;
		CPU[activeCPU].registers->CX = TSS16.TSS.CX;
		CPU[activeCPU].registers->DX = TSS16.TSS.DX;
		CPU[activeCPU].registers->BX = TSS16.TSS.BX;
		CPU[activeCPU].registers->SP = TSS16.TSS.SP;
		CPU[activeCPU].registers->BP = TSS16.TSS.BP;
		CPU[activeCPU].registers->SI = TSS16.TSS.SI;
		CPU[activeCPU].registers->DI = TSS16.TSS.DI;
		CPU[activeCPU].registers->FLAGS = TSS16.TSS.FLAGS;
		LDTsegment = TSS16.TSS.LDT; //LDT used!
	}

	//Check and verify the LDT descriptor!
	SEGDESCRIPTOR_TYPE LDTsegdesc;
	uint_32 descriptor_address = 0;
	descriptor_address = (LDTsegment & 4) ? ((CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_low|(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_mid<<16))| CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_high<<24) : CPU[activeCPU].registers->GDTR.base; //LDT/GDT selector!
	uint_32 descriptor_index = (LDTsegment&~0x7); //The full index within the descriptor table!

	if (LDTsegment & 4) //We cannot reside in the LDT!
	{
		CPU_TSSFault(CPU[activeCPU].registers->TR,is_external,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return 1; //Not present: we cannot reside in the LDT!
	}

	if ((word)(descriptor_index|0x7)>=((LDTsegment & 4) ? (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_low|(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_high<<16)) : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
	{
		CPU_TSSFault(CPU[activeCPU].registers->TR,is_external,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return 1; //Not present: limit exceeded!
	}

	if ((!descriptor_index) /*&& ((whatsegment == CPU_SEGMENT_CS) || (whatsegment == CPU_SEGMENT_SS))*/) //NULL segment loaded into CS or SS?
	{
		THROWDESCGP(CPU[activeCPU].registers->TR,is_external,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return 1; //Not present: limit exceeded!
	}

	int i;
	for (i = 0;i<(int)sizeof(LDTsegdesc.descdata);) //Process the descriptor data!
	{
		LDTsegdesc.descdata[i++] = memory_directrb(descriptor_address++); //Read a descriptor byte directly from flat memory!
	}

	//Now the LDT entry is loaded for testing!
	if (LDTsegdesc.desc.Type != AVL_SYSTEM_LDT) //Not an LDT?
	{
		THROWDESCGP(CPU[activeCPU].registers->TR,is_external,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return 1; //Not present: not an IDT!	
	}

	if (!LDTsegdesc.desc.P) //Not present?
	{
		THROWDESCGP(CPU[activeCPU].registers->TR,is_external,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
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
		CPU_TSSFault(CPU[activeCPU].registers->TR,is_external,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return 1; //Not present: limit exceeded!
	}

	word *SSPtr=&TSS32.TSS.SS;
	uint_32 *ESPPtr=&TSS32.TSS.ESP;
	word *SPPtr=&TSS16.TSS.SP;
	if (TSSSize) //32-bit to load?
	{
		switch (destStack) //What are we switching to?
		{
		case 0: //Level 0?
			SSPtr = &TSS32.TSS.SS0;
			ESPPtr = &TSS32.TSS.ESP0;
			break;
		case 1: //Level 1?
			SSPtr = &TSS32.TSS.SS1;
			ESPPtr = &TSS32.TSS.ESP1;
			break;
		case 2: //Level 2?
			SSPtr = &TSS32.TSS.SS2;
			ESPPtr = &TSS32.TSS.ESP2;
			break;
		case 3: //Level 3?
			SSPtr = &TSS32.TSS.SS;
			ESPPtr = &TSS32.TSS.ESP;
			break;
		}
	}
	else //16-bit to load?
	{
		switch (destStack) //What are we switching to?
		{
		case 0: //Level 0?
			SSPtr = &TSS16.TSS.SS0;
			SPPtr = &TSS16.TSS.SP0;
			break;
		case 1: //Level 1?
			SSPtr = &TSS16.TSS.SS1;
			SPPtr = &TSS16.TSS.SP1;
			break;
		case 2: //Level 2?
			SSPtr = &TSS16.TSS.SS2;
			SPPtr = &TSS16.TSS.SP2;
			break;
		case 3: //Level 3?
			SSPtr = &TSS16.TSS.SS;
			SPPtr = &TSS16.TSS.SP;
			break;
		}
	}

	segmentWritten(CPU_SEGMENT_SS, *SSPtr, 0); //Update the segment!
	if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
	if (TSSSize) //32-bit?
	{
		CPU[activeCPU].registers->ESP = *ESPPtr;
	}
	else //16-bit?
	{
		CPU[activeCPU].registers->SP = *SPPtr;
	}

	if (TSSSize) //32-bit?
	{
		segmentWritten(CPU_SEGMENT_DS, TSS32.TSS.DS, 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_ES, TSS32.TSS.ES, 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_FS, TSS32.TSS.FS, 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_GS, TSS32.TSS.GS, 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
	}
	else //16-bit?
	{
		segmentWritten(CPU_SEGMENT_DS, TSS16.TSS.DS, 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_ES, TSS16.TSS.ES, 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
	}

	//All segments are valid and readable!

	if (hascallinterrupttaken_type==0xFF) //Not set yet?
	{
		if (gated) //Different CPL?
		{
			hascallinterrupttaken_type = OTHERGATE_NORMALTASKGATE; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task gate flag. Left at 0xFF when nothing is used(unknown case?)
		}
		else //Same CPL call gate?
		{
			hascallinterrupttaken_type = OTHERGATE_NORMALTSS; //Normal TSS direct call!
		}
	}

	return 0; //Abort any running instruction operation!
}

void CPU_TSSFault(word segmentval, byte is_external, byte tbl)
{
	uint_32 errorcode;
	errorcode = (segmentval&0xFFFB)|(is_external&1)|(tbl<<1);
	CPU_resetOP(); //Point to the faulting instruction!
	
	if (CPU_faultraised()) //We're raising a fault!
	{
		if (call_soft_inthandler(EXCEPTION_INVALIDTSSSEGMENT)) //Call IVT entry #13 decimal!
		{
			CPU_PUSH32(&errorcode); //Error code!
		}
	}
}
