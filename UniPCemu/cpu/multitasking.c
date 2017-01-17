#include "headers/cpu/cpu.h" //CPU reqs!
#include "headers/cpu/multitasking.h" //Our typedefs!
#include "headers/mmu/mmuhandler.h" //Direct MMU support!
#include "headers/cpu/cpu_pmtimings.h" //286+ timing support!
#include "headers/cpu/easyregs.h" //Easy register support!

//Force 16-bit TSS on 80286?
//#define FORCE_16BITTSS

//Reading of the 16-bit entries within descriptors!
#define DESC_16BITS(x) SDL_SwapLE16(x)
#define DESC_32BITS(x) SDL_SwapLE32(x)

//Everything concerning TSS.

extern byte hascallinterrupttaken_type; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task gate flag. Left at 0xFF when nothing is used(unknown case?)

extern uint_32 destEIP; //Destination address for CS JMP instruction!

byte CPU_switchtask(int whatsegment, SEGDESCRIPTOR_TYPE *LOADEDDESCRIPTOR,word *segment, word destinationtask, byte isJMPorCALL, byte gated, int_64 errorcode) //Switching to a certain task?
{
	//byte isStackSwitch = 0; //Stack switch?
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

	if (GENERALSEGMENT_DPL(LOADEDDESCRIPTOR->desc) != getCPL()) //Different CPL? Stack switch?
	{
		destStack = GENERALSEGMENT_DPL(LOADEDDESCRIPTOR->desc); //Switch to this stack!
		//isStackSwitch = 1; //Switching stacks!
	}

	uint_32 limit; //The limit we use!
	if (GENERALSEGMENT_P(LOADEDDESCRIPTOR->desc)==0) //Not present?
	{
		THROWDESCNP(destinationtask,(errorcode!=-1)?(errorcode&1):0,(destinationtask&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #NP!
		return 1; //Error out!
	}

	switch (GENERALSEGMENT_TYPE(LOADEDDESCRIPTOR->desc)) //Check the type of descriptor we're executing!
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
		THROWDESCGP(CPU[activeCPU].registers->TR,(errorcode!=-1)?(errorcode&1):0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Thow #GP!
		return 1; //Error out!
	}

	#ifdef FORCE_16BITTSS
		if (EMULATED_CPU == CPU_80286) TSSSize = 0; //Force 16-bit TSS on 286!
	#endif

	if (busy) //Busy?
	{
		THROWDESCGP(destinationtask,(errorcode!=-1)?(errorcode&1):0,(destinationtask&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Thow #GP!
		return 1; //Error out!
	}

	limit = LOADEDDESCRIPTOR->desc.limit_low; //Low limit (286+)!
	if (EMULATED_CPU >= CPU_80386) //Gotten high limit?
	{
		limit |= (SEGDESC_NONCALLGATE_LIMIT_HIGH(LOADEDDESCRIPTOR->desc) << 16); //High limit too!
	}

	if (limit < (uint_32)((EMULATED_CPU==CPU_80286)?43:103)) //Limit isn't high enough(>=103 for 386+, >=43 for 80286)?
	{
		CPU_TSSFault(destinationtask,(errorcode!=-1)?(errorcode&1):0,(destinationtask&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #TS!
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
				tempdesc.desc.AccessRights &= ~2; //Mark idle!
				SAVEDESCRIPTOR(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,&tempdesc); //Save the new status into the old descriptor!
			}
		}

		if (isJMPorCALL == 3) //IRET?
		{
			FLAGW_NT(0); //Clear Nested Task flag of the leaving task!
		}

		//16 or 32-bit TSS is loaded, now save the registers!
		if (TSSSize) //We're a 32-bit TSS?
		{
			TSS32.TSS.EAX = DESC_32BITS(CPU[activeCPU].registers->EAX);
			TSS32.TSS.ECX = DESC_32BITS(CPU[activeCPU].registers->ECX);
			TSS32.TSS.EDX = DESC_32BITS(CPU[activeCPU].registers->EDX);
			TSS32.TSS.EBX = DESC_32BITS(CPU[activeCPU].registers->EBX);
			TSS32.TSS.ESP = DESC_32BITS(CPU[activeCPU].registers->ESP);
			TSS32.TSS.EBP = DESC_32BITS(CPU[activeCPU].registers->EBP);
			TSS32.TSS.ESI = DESC_32BITS(CPU[activeCPU].registers->ESI);
			TSS32.TSS.EDI = DESC_32BITS(CPU[activeCPU].registers->EDI);
			TSS32.TSS.CS = DESC_16BITS(CPU[activeCPU].registers->CS);
			TSS32.TSS.EIP = DESC_32BITS(CPU[activeCPU].registers->EIP);
			TSS32.TSS.SS = DESC_16BITS(CPU[activeCPU].registers->SS);
			TSS32.TSS.DS = DESC_16BITS(CPU[activeCPU].registers->DS);
			TSS32.TSS.FS = DESC_16BITS(CPU[activeCPU].registers->FS);
			TSS32.TSS.GS = DESC_16BITS(CPU[activeCPU].registers->GS);
			TSS32.TSS.EFLAGS = DESC_32BITS(CPU[activeCPU].registers->EFLAGS);
		}
		else //We're a 16-bit TSS?
		{
			TSS16.TSS.AX = DESC_16BITS(CPU[activeCPU].registers->AX);
			TSS16.TSS.CX = DESC_16BITS(CPU[activeCPU].registers->CX);
			TSS16.TSS.DX = DESC_16BITS(CPU[activeCPU].registers->DX);
			TSS16.TSS.BX = DESC_16BITS(CPU[activeCPU].registers->BX);
			TSS16.TSS.SP = DESC_16BITS(CPU[activeCPU].registers->SP);
			TSS16.TSS.BP = DESC_16BITS(CPU[activeCPU].registers->BP);
			TSS16.TSS.SI = DESC_16BITS(CPU[activeCPU].registers->SI);
			TSS16.TSS.DI = DESC_16BITS(CPU[activeCPU].registers->DI);
			TSS16.TSS.CS = DESC_16BITS(CPU[activeCPU].registers->CS);
			TSS16.TSS.IP = DESC_16BITS(CPU[activeCPU].registers->IP);
			TSS16.TSS.SS = DESC_16BITS(CPU[activeCPU].registers->SS);
			TSS16.TSS.DS = DESC_16BITS(CPU[activeCPU].registers->DS);
			TSS16.TSS.FLAGS = DESC_16BITS(CPU[activeCPU].registers->FLAGS);
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

	switch (GENERALSEGMENT_TYPE(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR])) //Check the type of descriptor we're executing now!
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
		THROWDESCGP(destinationtask,(errorcode!=-1)?(errorcode&1):0,(destinationtask&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Thow #GP!
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
			TSS32.TSS.BackLink = DESC_16BITS(oldtask); //Save the old task as a backlink in the new task!
			TSS_dirty = 1; //We're dirty!
		}
	}
	else //16-bit TSS?
	{
		if (isJMPorCALL == 2 && oldtask) //CALL?
		{
			TSS16.TSS.BackLink = DESC_16BITS(oldtask); //Save the old task as a backlink in the new task!
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
		LOADEDDESCRIPTOR->desc.AccessRights |= 2; //Mark not idle!
		SAVEDESCRIPTOR(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, LOADEDDESCRIPTOR); //Save the new status into the old descriptor!
	}

	//Now we're ready to load all registers!
	if (TSSSize) //We're a 32-bit TSS?
	{
		CPU[activeCPU].registers->EAX = DESC_32BITS(TSS32.TSS.EAX);
		CPU[activeCPU].registers->ECX = DESC_32BITS(TSS32.TSS.ECX);
		CPU[activeCPU].registers->EDX = DESC_32BITS(TSS32.TSS.EDX);
		CPU[activeCPU].registers->EBX = DESC_32BITS(TSS32.TSS.EBX);
		CPU[activeCPU].registers->ESP = DESC_32BITS(TSS32.TSS.ESP);
		CPU[activeCPU].registers->EBP = DESC_32BITS(TSS32.TSS.EBP);
		CPU[activeCPU].registers->ESI = DESC_32BITS(TSS32.TSS.ESI);
		CPU[activeCPU].registers->EDI = DESC_32BITS(TSS32.TSS.EDI);
		CPU[activeCPU].registers->CR3 = DESC_32BITS(TSS32.TSS.CR3); //Load the new CR3 register to use the new Paging table!
		CPU[activeCPU].registers->EFLAGS = DESC_32BITS(TSS32.TSS.EFLAGS);
		//Load all remaining registers manually for exceptions!
		CPU[activeCPU].registers->CS = DESC_16BITS(TSS32.TSS.CS);
		CPU[activeCPU].registers->DS = DESC_16BITS(TSS32.TSS.DS);
		CPU[activeCPU].registers->ES = DESC_16BITS(TSS32.TSS.ES);
		CPU[activeCPU].registers->FS = DESC_16BITS(TSS32.TSS.FS);
		CPU[activeCPU].registers->GS = DESC_16BITS(TSS32.TSS.GS);
		CPU[activeCPU].registers->EIP = DESC_32BITS(TSS32.TSS.EIP);
		CPU[activeCPU].registers->SS = DESC_16BITS(TSS32.TSS.SS); //Default stack to use: the old stack!
		LDTsegment = DESC_16BITS(TSS32.TSS.LDT); //LDT used!
	}
	else //We're a 16-bit TSS?
	{
		CPU[activeCPU].registers->AX = DESC_16BITS(TSS16.TSS.AX);
		CPU[activeCPU].registers->CX = DESC_16BITS(TSS16.TSS.CX);
		CPU[activeCPU].registers->DX = DESC_16BITS(TSS16.TSS.DX);
		CPU[activeCPU].registers->BX = DESC_16BITS(TSS16.TSS.BX);
		CPU[activeCPU].registers->SP = DESC_16BITS(TSS16.TSS.SP);
		CPU[activeCPU].registers->BP = DESC_16BITS(TSS16.TSS.BP);
		CPU[activeCPU].registers->SI = DESC_16BITS(TSS16.TSS.SI);
		CPU[activeCPU].registers->DI = DESC_16BITS(TSS16.TSS.DI);
		CPU[activeCPU].registers->FLAGS = DESC_16BITS(TSS16.TSS.FLAGS);
		//Load all remaining registers manually for exceptions!
		CPU[activeCPU].registers->CS = DESC_16BITS(TSS16.TSS.CS); //This should also load the privilege level!
		CPU[activeCPU].registers->DS = DESC_16BITS(TSS16.TSS.DS);
		CPU[activeCPU].registers->ES = DESC_16BITS(TSS16.TSS.ES);
		CPU[activeCPU].registers->EIP = (uint_32)DESC_16BITS(TSS16.TSS.IP);
		CPU[activeCPU].registers->SS = DESC_16BITS(TSS16.TSS.SS); //Default stack to use: the old stack!
		LDTsegment = DESC_16BITS(TSS16.TSS.LDT); //LDT used!
	}

	//Check and verify the LDT descriptor!
	SEGDESCRIPTOR_TYPE LDTsegdesc;
	uint_32 descriptor_address = 0;
	descriptor_address = (LDTsegment & 4) ? ((CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_low|(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_mid<<16))| CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_high<<24) : CPU[activeCPU].registers->GDTR.base; //LDT/GDT selector!
	uint_32 descriptor_index = (LDTsegment&~0x7); //The full index within the descriptor table!

	if (LDTsegment & 4) //We cannot reside in the LDT!
	{
		CPU_TSSFault(CPU[activeCPU].registers->TR,(errorcode!=-1)?(errorcode&1):0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return 1; //Not present: we cannot reside in the LDT!
	}

	if ((word)(descriptor_index|0x7)>=((LDTsegment & 4) ? (CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].limit_low|(SEGDESC_NONCALLGATE_LIMIT_HIGH(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR])<<16)) : CPU[activeCPU].registers->GDTR.limit)) //LDT/GDT limit exceeded?
	{
		CPU_TSSFault(CPU[activeCPU].registers->TR,(errorcode!=-1)?(errorcode&1):0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return 1; //Not present: limit exceeded!
	}

	if ((!descriptor_index) /*&& ((whatsegment == CPU_SEGMENT_CS) || (whatsegment == CPU_SEGMENT_SS))*/) //NULL segment loaded into CS or SS?
	{
		THROWDESCGP(CPU[activeCPU].registers->TR,(errorcode!=-1)?(errorcode&1):0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return 1; //Not present: limit exceeded!
	}

	descriptor_address += descriptor_index; //Make sure to index into the descriptor table!

	int i;
	for (i = 0;i<(int)sizeof(LDTsegdesc.descdata);) //Process the descriptor data!
	{
		LDTsegdesc.descdata[i++] = memory_directrb(descriptor_address++); //Read a descriptor byte directly from flat memory!
	}

	//Now the LDT entry is loaded for testing!
	if (GENERALSEGMENT_TYPE(LDTsegdesc.desc) != AVL_SYSTEM_LDT) //Not an LDT?
	{
		THROWDESCGP(CPU[activeCPU].registers->TR,(errorcode!=-1)?(errorcode&1):0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return 1; //Not present: not an IDT!	
	}

	if (!GENERALSEGMENT_P(LDTsegdesc.desc)) //Not present?
	{
		THROWDESCGP(CPU[activeCPU].registers->TR,(errorcode!=-1)?(errorcode&1):0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return 1; //Not present: not an IDT!	
	}

	CPU[activeCPU].registers->CR0 |= CR0_TS; //Set the high bit of the TS bit(bit 3)!

	//Now, load all normal registers in order, keeping aborts possible!
	CPU[activeCPU].faultraised = 0; //Clear the fault level: the new task has no faults by default!

	//First, load CS!
	destEIP = CPU[activeCPU].registers->EIP; //Save EIP for the new address, we don't want to lose it when loading!
	if (TSSSize) //32-bit?
	{
		segmentWritten(CPU_SEGMENT_CS,DESC_16BITS(TSS32.TSS.CS),0); //Load CS!
	}
	else
	{
		segmentWritten(CPU_SEGMENT_CS, DESC_16BITS(TSS16.TSS.CS), 0); //Load CS!
	}
	CPU_flushPIQ(); //We're jumping to another address!
	if (CPU[activeCPU].faultraised) return 0; //Abort on fault raised!
	if (getCPL() != GENERALSEGMENT_DPL(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR])) //Non-matching TSS DPL vs CS CPL?
	{
		CPU_TSSFault(CPU[activeCPU].registers->TR,(errorcode!=-1)?(errorcode&1):0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
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

	segmentWritten(CPU_SEGMENT_SS, DESC_16BITS(*SSPtr), 0); //Update the segment!
	if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
	if (TSSSize) //32-bit?
	{
		CPU[activeCPU].registers->ESP = DESC_32BITS(*ESPPtr);
	}
	else //16-bit?
	{
		CPU[activeCPU].registers->SP = DESC_16BITS(*SPPtr);
	}

	if (TSSSize) //32-bit?
	{
		segmentWritten(CPU_SEGMENT_DS, DESC_16BITS(TSS32.TSS.DS), 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_ES, DESC_16BITS(TSS32.TSS.ES), 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_FS, DESC_16BITS(TSS32.TSS.FS), 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_GS, DESC_16BITS(TSS32.TSS.GS), 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
	}
	else //16-bit?
	{
		segmentWritten(CPU_SEGMENT_DS, DESC_16BITS(TSS16.TSS.DS), 0); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_ES, DESC_16BITS(TSS16.TSS.ES), 0); //Load reg!
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
		call_soft_inthandler(EXCEPTION_INVALIDTSSSEGMENT,errorcode); //Call IVT entry #13 decimal!
	}
}
