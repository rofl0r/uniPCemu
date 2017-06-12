#include "headers/cpu/cpu.h" //CPU reqs!
#include "headers/cpu/multitasking.h" //Our typedefs!
#include "headers/mmu/mmuhandler.h" //Direct MMU support!
#include "headers/cpu/cpu_pmtimings.h" //286+ timing support!
#include "headers/cpu/easyregs.h" //Easy register support!
#include "headers/support/log.h" //Logging support!
#include "headers/emu/debugger/debugger.h" //Debugging support!
#include "headers/cpu/protecteddebugging.h" //Protected mode debugging support!
#include "headers/cpu/biu.h" //BIU support!

//Force 16-bit TSS on 80286?
//#define FORCE_16BITTSS

//Reading of the 16-bit entries within descriptors!
#define DESC_16BITS(x) SDL_SwapLE16(x)
#define DESC_32BITS(x) SDL_SwapLE32(x)

//Everything concerning TSS.

extern byte hascallinterrupttaken_type; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task gate flag. Left at 0xFF when nothing is used(unknown case?)

extern uint_32 destEIP; //Destination address for CS JMP instruction!

void loadTSS16(TSS286 *TSS)
{
	word n;
	word *data16;
	data16 = &TSS->BackLink; //Load all addresses as 16-bit values!
	for (n = 0;n < sizeof(*TSS);n+=2) //Load our TSS!
	{
		*data16++ = MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, 0,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
	}
}

void loadTSS32(TSS386 *TSS)
{
	byte ssspreg;
	word n;
	uint_32 *data32;
	word *data16;
	TSS->BackLink = MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, 0, 0,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
	//SP0/ESP0 initializing!
	n = 4; //Start of our block!
	data32 = &TSS->ESP0; //Start with 32-bit data!
	data16 = &TSS->SS0; //Start with 16-bit data!

	for (ssspreg=0;ssspreg<3;++ssspreg) //Read all required stack registers!
	{
		*data32++ = MMU_rdw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, 0,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
		n += 4; //Next item!
		*data16++ = MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, 0,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
		n += 4; //Next item!
		++data32; //Skip the 32-bit item(the SS entry) accordingly!
	}

	data32 = &TSS->CR3; //Start with CR3!
	for (n=(7*4);n<((7+11)*4);n+=4) //Write our TSS 32-bit data!
	{
		*data32++ = MMU_rdw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, 0,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
	}

	data16 = &TSS->ES; //Start with ES!
	for (n=(((7+11)*4));n<((7+11+7)*4);n+=4) //Write our TSS 16-bit data!
	{
		*data16++ = MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, 0,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
	}

	data16 = &TSS->T; //Start of the last data!
	*data16++ = MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, (25*4), 0,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
	*data16++ = MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, (25*4)+2, 0,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
}

void saveTSS16(TSS286 *TSS)
{
	word n;
	word *data16;
	data16 = &TSS->IP; //Start with IP!
	for (n=((7*2));n<(sizeof(*TSS)-2);n+=2) //Write our TSS 16-bit data! Don't store the LDT and Stacks for different privilege levels!
	{
		MMU_ww(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, *data16,0); //Write the TSS! Don't be afraid of errors, since we're always accessable!
		++data16; //Next data!		
	}
}

void saveTSS32(TSS386 *TSS)
{
	word n;
	uint_32 *data32;
	word *data16;
	data32 = &TSS->EIP; //Start with EIP!
	for (n =(8*4);n<((8+10)*4);n+=4) //Write our TSS 32-bit data! Ignore the Stack data for different privilege levels and CR3(PDBR)!
	{
		MMU_wdw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, *data32,0); //Write the TSS! Don't be afraid of errors, since we're always accessable!
		++data32; //Next data!
	}
	data16 = &TSS->ES; //Start with ES!
	for (n=(((8+10)*4));n<((8+10+6)*4);n+=4) //Write our TSS 16-bit data! Ignore the LDT and I/O map/T-bit, as it's read-only!
	{
		MMU_ww(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, *data16,0); //Write the TSS! Don't be afraid of errors, since we're always accessable!
		++data16; //Next data!		
	}
}

byte enableMMUbuffer; //To buffer the MMU writes?

extern word CPU_exec_CS; //Save for handling!
extern uint_32 CPU_exec_EIP; //Save for handling!

byte CPU_switchtask(int whatsegment, SEGDESCRIPTOR_TYPE *LOADEDDESCRIPTOR,word *segment, word destinationtask, byte isJMPorCALL, byte gated, int_64 errorcode) //Switching to a certain task?
{
	//byte isStackSwitch = 0; //Stack switch?
	byte destStack = 3; //Destination stack!
	//Both structures to use for the TSS!
	word LDTsegment;
	word oldtask;
	//byte busy=0;
	byte TSS_dirty = 0; //Is the new TSS dirty?
	TSS286 TSS16;
	TSS386 TSS32;
	byte TSSSize = 0; //The TSS size!

	enableMMUbuffer = 0; //Disable any MMU buffering: we need to update memory directly and properly, in order to work!

	if (debugger_logging()) //Are we logging?
	{
		dolog("debugger","Switching task to task %04X",destinationtask);
	}

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
		//busy = 1;
	case AVL_SYSTEM_TSS16BIT:
		TSSSize = 0; //16-bit TSS!
		break;
	case AVL_SYSTEM_BUSY_TSS32BIT:
		//busy = 1;
	case AVL_SYSTEM_TSS32BIT: //Valid descriptor?
		TSSSize = 1; //32-bit TSS!
		if (EMULATED_CPU < CPU_80386) //Continue normally: we're valid on a 80386 only?
		{
			goto invalidsrctask; //Thow #GP!
		}
		break;
	default: //Invalid descriptor!
		invalidsrctask:
		THROWDESCGP(CPU[activeCPU].registers->TR,(errorcode!=-1)?(errorcode&1):0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Thow #GP!
		return 1; //Error out!
	}

	#ifdef FORCE_16BITTSS
		if (EMULATED_CPU == CPU_80286) TSSSize = 0; //Force 16-bit TSS on 286!
	#endif

	limit = LOADEDDESCRIPTOR->desc.limit_low; //Low limit (286+)!
	if (EMULATED_CPU >= CPU_80386) //Gotten high limit?
	{
		limit |= (SEGDESC_NONCALLGATE_LIMIT_HIGH(LOADEDDESCRIPTOR->desc) << 16); //High limit too!
	}

	if (limit < (uint_32)(TSSSize?43:103)) //Limit isn't high enough(>=103 for 386+, >=43 for 80286)?
	{
		CPU_TSSFault(destinationtask,(errorcode!=-1)?(errorcode&1):0,(destinationtask&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #TS!
		return 1; //Error out!
	}

	//Now going to switch to the current task, save the registers etc in the current task!

	if (TSSSize) //32-bit TSS?
	{
		memset(&TSS32,0,sizeof(TSS32)); //Read the TSS! Don't be afraid of errors, since we're always accessable!
	}
	else //16-bit TSS?
	{
		memset(&TSS16, 0, sizeof(TSS16)); //Read the TSS! Don't be afraid of errors, since we're always accessable!
	}

	if (isJMPorCALL==3) //IRET?
	{
		if ((LOADEDDESCRIPTOR->desc.AccessRights&2)==0) //Destination task is available?
		{
			CPU_TSSFault(destinationtask,(errorcode!=-1)?(errorcode&1):0,(destinationtask&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw #GP!
			return 1; //Error out!
		}
	}

	if (CPU[activeCPU].registers->TR) //Valid task to switch FROM?
	{
		if (debugger_logging()) //Are we logging?
		{
			dolog("debugger","Preparing outgoing task %04X for transfer",CPU[activeCPU].registers->TR);
		}
		
		if (isJMPorCALL != 2) //Not a call? Stop being busy to switch to another task(or ourselves)!
		{
			SEGDESCRIPTOR_TYPE tempdesc;
			if (LOADDESCRIPTOR(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,&tempdesc)) //Loaded old container?
			{
				tempdesc.desc.AccessRights &= ~2; //Mark idle!
				if (SAVEDESCRIPTOR(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,&tempdesc)==0) //Save the new status into the old descriptor!
				{
					return 1; //Abort on fault raised!
				}
			}
		}

		if (isJMPorCALL == 3) //IRET?
		{
			FLAGW_NT(0); //Clear Nested Task flag of the leaving task!
		}

		//16 or 32-bit TSS is loaded, now save the registers!
		if (TSSSize) //We're a 32-bit TSS?
		{
			TSS32.EAX = CPU[activeCPU].registers->EAX;
			TSS32.ECX = CPU[activeCPU].registers->ECX;
			TSS32.EDX = CPU[activeCPU].registers->EDX;
			TSS32.EBX = CPU[activeCPU].registers->EBX;
			TSS32.ESP = CPU[activeCPU].registers->ESP;
			TSS32.EBP = CPU[activeCPU].registers->EBP;
			TSS32.ESI = CPU[activeCPU].registers->ESI;
			TSS32.EDI = CPU[activeCPU].registers->EDI;
			TSS32.CS = CPU[activeCPU].registers->CS;
			TSS32.EIP = CPU[activeCPU].registers->EIP;
			TSS32.SS = CPU[activeCPU].registers->SS;
			TSS32.DS = CPU[activeCPU].registers->DS;
			TSS32.ES = CPU[activeCPU].registers->ES;
			TSS32.FS = CPU[activeCPU].registers->FS;
			TSS32.GS = CPU[activeCPU].registers->GS;
			TSS32.EFLAGS = CPU[activeCPU].registers->EFLAGS;
		}
		else //We're a 16-bit TSS?
		{
			TSS16.AX = CPU[activeCPU].registers->AX;
			TSS16.CX = CPU[activeCPU].registers->CX;
			TSS16.DX = CPU[activeCPU].registers->DX;
			TSS16.BX = CPU[activeCPU].registers->BX;
			TSS16.SP = CPU[activeCPU].registers->SP;
			TSS16.BP = CPU[activeCPU].registers->BP;
			TSS16.SI = CPU[activeCPU].registers->SI;
			TSS16.DI = CPU[activeCPU].registers->DI;
			TSS16.CS = CPU[activeCPU].registers->CS;
			TSS16.IP = CPU[activeCPU].registers->IP;
			TSS16.SS = CPU[activeCPU].registers->SS;
			TSS16.DS = CPU[activeCPU].registers->DS;
			TSS16.ES = CPU[activeCPU].registers->ES;
			TSS16.FLAGS = CPU[activeCPU].registers->FLAGS;
		}

		if (debugger_logging()) //Are we logging?
		{
			dolog("debugger","Saving outgoing task %04X to memory",CPU[activeCPU].registers->TR);
		}

		if (TSSSize) //32-bit TSS?
		{
			saveTSS32(&TSS32); //Save us!
		}
		else //16-bit TSS?
		{
			saveTSS16(&TSS16); //Save us!
		}
	}

	oldtask = CPU[activeCPU].registers->TR; //Save the old task, for backlink purposes!

	//Now, load all the registers required as needed!
	CPU[activeCPU].faultraised = 0; //We have no fault raised: we need this to run the segment change!
	if (debugger_logging()) //Are we logging?
	{
		dolog("debugger","Switching active TSS to segment selector %04X",destinationtask);
	}

	segmentWritten(CPU_SEGMENT_TR,destinationtask,0); //Execute the task switch itself, loading our new descriptor!
	if (CPU[activeCPU].faultraised) return 1; //Abort on fault: invalid(or busy) task we're switching to!

	switch (GENERALSEGMENT_TYPE(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR])) //Check the type of descriptor we're executing now!
	{
	case AVL_SYSTEM_BUSY_TSS16BIT:
	case AVL_SYSTEM_TSS16BIT:
		TSSSize = 0; //16-bit TSS!
		break;
	case AVL_SYSTEM_BUSY_TSS32BIT:
	case AVL_SYSTEM_TSS32BIT: //Valid descriptor?
		TSSSize = 1; //32-bit TSS!
		if (EMULATED_CPU < CPU_80386) //Continue normally: we're valid on a 80386 only?
		{
			goto invaliddesttask; //Thow #GP!
		}
		break;
	default: //Invalid descriptor!
		invaliddesttask:
		THROWDESCGP(destinationtask,(errorcode!=-1)?(errorcode&1):0,(destinationtask&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Thow #GP!
		return 1; //Error out!
	}

	#ifdef FORCE_16BITTSS
		if (EMULATED_CPU == CPU_80286) TSSSize = 0; //Force 16-bit TSS on 286!
	#endif

	if (debugger_logging()) //Are we logging?
	{
		dolog("debugger","Loading incoming TSS %04X state",CPU[activeCPU].registers->TR);
	}

	//Load the new TSS!
	if (TSSSize) //32-bit TSS?
	{
		loadTSS32(&TSS32); //Load the TSS!
	}
	else //16-bit TSS?
	{
		loadTSS16(&TSS16); //Load the TSS!
	}
	TSS_dirty = 0; //Not dirty!

	if (debugger_logging()) //Are we logging?
	{
		dolog("debugger","Checking for backlink to TSS %04X",oldtask);
	}

	if ((isJMPorCALL == 2) && oldtask) //CALL?
	{
		if (TSSSize) //32-bit TSS?
		{
			TSS32.BackLink = oldtask; //Save the old task as a backlink in the new task!
			TSS_dirty = 1; //We're dirty!
		}
		else //16-bit TSS?
		{
			TSS16.BackLink = oldtask; //Save the old task as a backlink in the new task!
			TSS_dirty = 1; //We're dirty!
		}
	}

	if (debugger_logging()) //Are we logging?
	{
		dolog("debugger","Marking incoming TSS %04X busy if needed",CPU[activeCPU].registers->TR);
	}

	if (isJMPorCALL != 3) //Not an IRET?
	{
		LOADEDDESCRIPTOR->desc.AccessRights |= 2; //Mark not idle!
		if (SAVEDESCRIPTOR(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, LOADEDDESCRIPTOR)==0) //Save the new status into the old descriptor!
		{
			return 1; //Abort on fault raised!
		}
	}

	if (debugger_logging()) //Are we logging?
	{
		dolog("debugger","Loading incoming TSS %04X state into the registers.",CPU[activeCPU].registers->TR);
	}

	//Now we're ready to load all registers!
	if (TSSSize) //We're a 32-bit TSS?
	{
		CPU[activeCPU].registers->EAX = TSS32.EAX;
		CPU[activeCPU].registers->ECX = TSS32.ECX;
		CPU[activeCPU].registers->EDX = TSS32.EDX;
		CPU[activeCPU].registers->EBX = TSS32.EBX;
		CPU[activeCPU].registers->ESP = TSS32.ESP;
		CPU[activeCPU].registers->EBP = TSS32.EBP;
		CPU[activeCPU].registers->ESI = TSS32.ESI;
		CPU[activeCPU].registers->EDI = TSS32.EDI;
		CPU[activeCPU].registers->CR3 = TSS32.CR3; //Load the new CR3 register to use the new Paging table!
		CPU[activeCPU].registers->EFLAGS = TSS32.EFLAGS;
		//Load all remaining registers manually for exceptions!
		CPU[activeCPU].registers->CS = TSS32.CS;
		CPU[activeCPU].registers->DS = TSS32.DS;
		CPU[activeCPU].registers->ES = TSS32.ES;
		CPU[activeCPU].registers->FS = TSS32.FS;
		CPU[activeCPU].registers->GS = TSS32.GS;
		CPU[activeCPU].registers->EIP = TSS32.EIP;
		CPU[activeCPU].registers->SS = TSS32.SS; //Default stack to use: the old stack!
		LDTsegment = TSS32.LDT; //LDT used!
	}
	else //We're a 16-bit TSS?
	{
		CPU[activeCPU].registers->EAX = TSS16.AX;
		CPU[activeCPU].registers->ECX = TSS16.CX;
		CPU[activeCPU].registers->EDX = TSS16.DX;
		CPU[activeCPU].registers->EBX = TSS16.BX;
		CPU[activeCPU].registers->ESP = TSS16.SP;
		CPU[activeCPU].registers->EBP = TSS16.BP;
		CPU[activeCPU].registers->ESI = TSS16.SI;
		CPU[activeCPU].registers->EDI = TSS16.DI;
		CPU[activeCPU].registers->EFLAGS = (uint_32)TSS16.FLAGS;
		//Load all remaining registers manually for exceptions!
		CPU[activeCPU].registers->CS = TSS16.CS; //This should also load the privilege level!
		CPU[activeCPU].registers->DS = TSS16.DS;
		CPU[activeCPU].registers->ES = TSS16.ES;
		CPU[activeCPU].registers->EIP = (uint_32)TSS16.IP;
		CPU[activeCPU].registers->SS = TSS16.SS; //Default stack to use: the old stack!
		LDTsegment = TSS16.LDT; //LDT used!
	}

	if ((isJMPorCALL == 2) && oldtask) //CALL?
	{
		FLAGW_NT(1); //Set Nested Task flag of the new task!
		if (TSSSize) //32-bit TSS?
		{
			TSS32.EFLAGS = CPU[activeCPU].registers->EFLAGS; //Save the new flag!
		}
		else //16-bit TSS?
		{
			TSS16.FLAGS = CPU[activeCPU].registers->FLAGS; //Save the new flag!
		}
		TSS_dirty = 1; //We're dirty!
	}

	if (TSS_dirty) //Destination TSS dirty?
	{
		if (debugger_logging()) //Are we logging?
		{
			dolog("debugger","Saving incoming TSS %04X state to memory, because the state has changed(Nested Task).",CPU[activeCPU].registers->TR);
		}
		MMU_ww(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, 0, TSSSize?TSS32.BackLink:TSS16.BackLink,0); //Write the TSS Backlink to use! Don't be afraid of errors, since we're always accessable!
		if (TSSSize) //32-bit TSS?
		{
			saveTSS32(&TSS32); //Save the TSS!
		}
		else //16-bit TSS?
		{
			saveTSS16(&TSS16); //Save the TSS!
		}
	}

	//Update the x86 debugger, if needed!
	protectedModeDebugger_taskswitch(); //Apply any action required for a task switch!

	updateCPUmode(); //Make sure the CPU mode is updated, according to the task!

	CPU_exec_CS = CPU[activeCPU].registers->CS; //Save for error handling!
	CPU_exec_EIP = CPU[activeCPU].registers->EIP; //Save for error handling!

	if (debugger_logging()) //Are we logging?
	{
		dolog("debugger","Loading incoming TSS LDT %04X",LDTsegment);
	}

	//Check and verify the LDT descriptor!
	SEGDESCRIPTOR_TYPE LDTsegdesc;
	uint_32 descriptor_address = 0;
	descriptor_address = (LDTsegment & 4) ? ((CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_low|(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_mid<<16))| CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR].base_high<<24) : CPU[activeCPU].registers->GDTR.base; //LDT/GDT selector!
	uint_32 descriptor_index = (LDTsegment&~0x7); //The full index within the descriptor table!

	if (!(descriptor_index&~3)) //NULL segment loaded into LDTR? Special case: no LDT available!
	{
		memset(&LDTsegdesc.descdata,0,sizeof(LDTsegdesc.descdata)); //No descriptor available to use: mark as invalid!
	}
	else //Valid LDT index?
	{
		if (LDTsegment & 4) //We cannot reside in the LDT!
		{
			CPU_TSSFault(CPU[activeCPU].registers->TR,(errorcode!=-1)?(errorcode&1):0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
			return 1; //Not present: we cannot reside in the LDT!
		}

		if ((word)(descriptor_index|0x7)>CPU[activeCPU].registers->GDTR.limit) //GDT limit exceeded?
		{
			CPU_TSSFault(CPU[activeCPU].registers->TR,(errorcode!=-1)?(errorcode&1):0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
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
	}

	if (debugger_logging()) //Are we logging?
	{
		dolog("debugger","Setting Task Switched flag in CR0");
	}

	CPU[activeCPU].registers->CR0 |= CR0_TS; //Set the high bit of the TS bit(bit 3)!

	//Now, load all normal registers in order, keeping aborts possible!
	CPU[activeCPU].faultraised = 0; //Clear the fault level: the new task has no faults by default!

	//Set the default CPL!
	CPU[activeCPU].CPL = (getcpumode()==CPU_MODE_8086)?3:((getcpumode()==CPU_MODE_REAL)?0:getRPL(TSSSize?TSS32.CS:TSS16.CS)); //Load default CPL, according to the mode!

	//First, load CS!
	if (debugger_logging()) //Are we logging?
	{
		dolog("debugger","Loading incoming TSS CS register");
	}
	destEIP = CPU[activeCPU].registers->EIP; //Save EIP for the new address, we don't want to lose it when loading!
	if (TSSSize) //32-bit?
	{
		segmentWritten(CPU_SEGMENT_CS,TSS32.CS,0); //Load CS!
	}
	else
	{
		segmentWritten(CPU_SEGMENT_CS, TSS16.CS, 0); //Load CS!
	}
	CPU_flushPIQ(-1); //We're jumping to another address!
	if (CPU[activeCPU].faultraised) return 0; //Abort on fault raised!
	if (getCPL() != GENERALSEGMENT_DPL(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR])) //Non-matching TSS DPL vs CS CPL?
	{
		CPU_TSSFault(CPU[activeCPU].registers->TR,(errorcode!=-1)?(errorcode&1):0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
		return 1; //Not present: limit exceeded!
	}

	if (debugger_logging()) //Are we logging?
	{
		dolog("debugger","Loading incoming TSS Stack address");
	}
	word *SSPtr=&TSS32.SS;
	uint_32 *ESPPtr=&TSS32.ESP;
	word *SPPtr=&TSS16.SP;
	if (TSSSize) //32-bit to load?
	{
		switch (destStack) //What are we switching to?
		{
		case 0: //Level 0?
			SSPtr = &TSS32.SS0;
			ESPPtr = &TSS32.ESP0;
			break;
		case 1: //Level 1?
			SSPtr = &TSS32.SS1;
			ESPPtr = &TSS32.ESP1;
			break;
		case 2: //Level 2?
			SSPtr = &TSS32.SS2;
			ESPPtr = &TSS32.ESP2;
			break;
		case 3: //Level 3?
			SSPtr = &TSS32.SS;
			ESPPtr = &TSS32.ESP;
			break;
		}
	}
	else //16-bit to load?
	{
		switch (destStack) //What are we switching to?
		{
		case 0: //Level 0?
			SSPtr = &TSS16.SS0;
			SPPtr = &TSS16.SP0;
			break;
		case 1: //Level 1?
			SSPtr = &TSS16.SS1;
			SPPtr = &TSS16.SP1;
			break;
		case 2: //Level 2?
			SSPtr = &TSS16.SS2;
			SPPtr = &TSS16.SP2;
			break;
		case 3: //Level 3?
			SSPtr = &TSS16.SS;
			SPPtr = &TSS16.SP;
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

	//Set the default CPL!
	CPU[activeCPU].CPL = (getcpumode()==CPU_MODE_8086)?3:((getcpumode()==CPU_MODE_REAL)?0:GENERALSEGMENT_DPL(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_SS])); //Load default CPL to use from SS if needed!

	if (debugger_logging()) //Are we logging?
	{
		dolog("debugger","Loading remaining TSS segment registers");
	}
	if (TSSSize) //32-bit?
	{
		segmentWritten(CPU_SEGMENT_DS, TSS32.DS, 0x80); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_ES, TSS32.ES, 0x80); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_FS, TSS32.FS, 0x80); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_GS, TSS32.GS, 0x80); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
	}
	else //16-bit?
	{
		segmentWritten(CPU_SEGMENT_DS, TSS16.DS, 0x80); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
		segmentWritten(CPU_SEGMENT_ES, TSS16.ES, 0x80); //Load reg!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault raised!
	}

	//All segments are valid and readable!

	if (debugger_logging()) //Are we logging?
	{
		dolog("debugger","New task ready for execution.");
	}

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
	errorcode = (segmentval&0xFFF8)|(is_external&1)|(tbl<<1);
	if (debugger_logging()) //Are we logging?
	{
		dolog("debugger","#TSS fault(%08X)!",errorcode);
	}
	CPU_resetOP(); //Point to the faulting instruction!
	
	if (CPU_faultraised(EXCEPTION_INVALIDTSSSEGMENT)) //We're raising a fault!
	{
		call_soft_inthandler(EXCEPTION_INVALIDTSSSEGMENT,errorcode); //Call IVT entry #13 decimal!
	}
}
