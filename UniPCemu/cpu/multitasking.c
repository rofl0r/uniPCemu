#include "headers/cpu/cpu.h" //CPU reqs!
#include "headers/cpu/multitasking.h" //Our typedefs!
#include "headers/mmu/mmuhandler.h" //Direct MMU support!
#include "headers/cpu/cpu_pmtimings.h" //286+ timing support!
#include "headers/cpu/easyregs.h" //Easy register support!
#include "headers/support/log.h" //Logging support!
#include "headers/emu/debugger/debugger.h" //Debugging support!
#include "headers/cpu/protecteddebugging.h" //Protected mode debugging support!
#include "headers/cpu/biu.h" //BIU support!
#include "headers/cpu/cpu_execution.h" //Execution flow support!

//Force 16-bit TSS on 80286?
//#define FORCE_16BITTSS

//Reading of the 16-bit entries within descriptors!
#define DESC_16BITS(x) SDL_SwapLE16(x)
#define DESC_32BITS(x) SDL_SwapLE32(x)

//Everything concerning TSS.

extern byte hascallinterrupttaken_type; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task gate flag. Left at 0xFF when nothing is used(unknown case?)

extern uint_32 destEIP; //Destination address for CS JMP instruction!

extern byte debugger_forceimmediatelogging; //Force immediate logging?

void loadTSS16(TSS286 *TSS)
{
	word n;
	byte i;
	i = 0;
	n = 0;
	for (i = 0;i < NUMITEMS(TSS->dataw);) //Load our TSS!
	{
		debugger_forceimmediatelogging = 1; //Log!
		TSS->dataw[i++] = MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, 0,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
		n += 2; //Next word!
	}
	debugger_forceimmediatelogging = 0; //Don't log!
}

byte checkloadTSS16()
{
	word n;
	for (n = 0;n < 0x2C;n+=2) //Load our TSS!
	{
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, 1|0x40, 0, 0, 0)) {debugger_forceimmediatelogging = 0; return 1;} //Error out!
	}
	for (n = 0; n < 0x2C; n += 2) //Load our TSS!
	{
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, 1|0xA0, 0, 0, 0)) { debugger_forceimmediatelogging = 0; return 1; } //Error out!
	}

	debugger_forceimmediatelogging = 0; //Don't log!
	return 0; //OK!
}

void loadTSS32(TSS386 *TSS)
{
	byte ssspreg;
	word n;
	byte i;
	debugger_forceimmediatelogging = 1; //Log!
	TSS->BackLink = MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, 0, 0,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
	//SP0/ESP0 initializing!
	n = 4; //Start of our block!
	for (ssspreg=0;ssspreg<3;++ssspreg) //Read all required stack registers!
	{
		debugger_forceimmediatelogging = 1; //Log!
		TSS->ESPs[ssspreg] = MMU_rdw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, 0,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
		n += 4; //Next item!
		debugger_forceimmediatelogging = 1; //Log!
		TSS->SSs[ssspreg++] = MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, 0,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!

		n += 4; //Next item!
	}

	i = 0;
	for (n=(7*4);n<((7+11)*4);n+=4) //Write our TSS 32-bit data!
	{
		debugger_forceimmediatelogging = 1; //Log!
		TSS->generalpurposeregisters[i++] = MMU_rdw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, 0,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
	}

	i = 0;
	for (n=(((7+11)*4));n<((7+11+7)*4);n+=4) //Write our TSS 16-bit data!
	{
		debugger_forceimmediatelogging = 1; //Log!
		TSS->segmentregisters[i++] = MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, 0,0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
	}

	debugger_forceimmediatelogging = 1; //Log!
	TSS->T = MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, (25 * 4), 0, 0); //Read the TSS! Don't be afraid of errors, since we're always accessable!
	debugger_forceimmediatelogging = 1; //Log!
	TSS->IOMapBase = MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, (25 * 4) + 2, 0, 0); //Read the TSS! Don't be afraid of errors, since we're always accessable!

	debugger_forceimmediatelogging = 0; //Don't log!
}

byte checkloadTSS32()
{
	byte ssspreg;
	word n;
	debugger_forceimmediatelogging = 1; //Log!
	if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR,0,1|0x40,0,0,0)) {debugger_forceimmediatelogging = 0; return 1;} //Error out!
	//SP0/ESP0 initializing!
	n = 4; //Start of our block!

	for (ssspreg=0;ssspreg<3;++ssspreg) //Read all required stack registers!
	{
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess32(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR,n+0,1|0x40,0,0,0)) {debugger_forceimmediatelogging = 0; return 1;} //Error out!

		n += 4; //Next item!
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR,n+0,1|0x40,0,0,0)) {debugger_forceimmediatelogging = 0; return 1;} //Error out!
		n += 4; //Next item!
	}

	for (n=(7*4);n<((7+11)*4);n+=4) //Write our TSS 32-bit data!
	{
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess32(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR,n+0,1|0x40,0,0,0)) {debugger_forceimmediatelogging = 0; return 1;} //Error out!
	}

	for (n=(((7+11)*4));n<((7+11+7)*4);n+=4) //Write our TSS 16-bit data!
	{
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess32(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR,n+0,1|0x40,0,0,0)) {debugger_forceimmediatelogging = 0; return 1;} //Error out!
	}

	debugger_forceimmediatelogging = 1; //Log!
	if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR,(25*4)+0,1|0x40,0,0,0)) {debugger_forceimmediatelogging = 0; return 1;} //Error out!
	debugger_forceimmediatelogging = 1; //Log!
	if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR,(25*4)+2,1|0x40,0,0,0)) {debugger_forceimmediatelogging = 0; return 1;} //Error out!
	debugger_forceimmediatelogging = 0; //Don't log!

	debugger_forceimmediatelogging = 1; //Log!
	if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, 0, 1|0xA0, 0, 0, 0)) { debugger_forceimmediatelogging = 0; return 1; } //Error out!
	//SP0/ESP0 initializing!
	n = 4; //Start of our block!

	for (ssspreg = 0; ssspreg < 3; ++ssspreg) //Read all required stack registers!
	{
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess32(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n + 0, 1|0xA0, 0, 0, 0)) { debugger_forceimmediatelogging = 0; return 1; } //Error out!

		n += 4; //Next item!
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n + 0, 1|0xA0, 0, 0, 0)) { debugger_forceimmediatelogging = 0; return 1; } //Error out!
		n += 4; //Next item!
	}

	for (n = (7 * 4); n < ((7 + 11) * 4); n += 4) //Write our TSS 32-bit data!
	{
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess32(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n + 0, 1|0xA0, 0, 0, 0)) { debugger_forceimmediatelogging = 0; return 1; } //Error out!
	}

	for (n = (((7 + 11) * 4)); n < ((7 + 11 + 7) * 4); n += 4) //Write our TSS 16-bit data!
	{
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess32(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n + 0, 1|0xA0, 0, 0, 0)) { debugger_forceimmediatelogging = 0; return 1; } //Error out!
	}

	debugger_forceimmediatelogging = 1; //Log!
	if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, (25 * 4) + 0, 1|0xA0, 0, 0, 0)) { debugger_forceimmediatelogging = 0; return 1; } //Error out!
	debugger_forceimmediatelogging = 1; //Log!
	if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, (25 * 4) + 2, 1|0xA0, 0, 0, 0)) { debugger_forceimmediatelogging = 0; return 1; } //Error out!
	debugger_forceimmediatelogging = 0; //Don't log!

	return 0; //OK!
}

void saveTSS16(TSS286 *TSS)
{
	word n;
	byte i;
	i = 7;
	for (n=((7*2));n<(sizeof(*TSS)-2);n+=2) //Write our TSS 16-bit data! Don't store the LDT and Stacks for different privilege levels!
	{
		debugger_forceimmediatelogging = 1; //Log!
		MMU_ww(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, TSS->dataw[i++],0); //Write the TSS! Don't be afraid of errors, since we're always accessable!
	}
	debugger_forceimmediatelogging = 0; //Don't log!
}

byte checksaveTSS16()
{
	word n;
	for (n=((7*2));n<(0x2C-2);n+=2) //Write our TSS 16-bit data! Don't store the LDT and Stacks for different privilege levels!
	{
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR,n+0,0|0x40,0,0,0)) {debugger_forceimmediatelogging = 0; return 1;} //Error out!
	}
	for (n = ((7 * 2)); n < (0x2C - 2); n += 2) //Write our TSS 16-bit data! Don't store the LDT and Stacks for different privilege levels!
	{
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess16(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n + 0, 0|0xA0, 0, 0, 0)) { debugger_forceimmediatelogging = 0; return 1; } //Error out!
	}
	debugger_forceimmediatelogging = 0; //Don't log!
	return 0; //OK!
}

void saveTSS32(TSS386 *TSS)
{
	word n;
	byte i;
	i = 1;
	for (n =(8*4);n<((8+10)*4);n+=4) //Write our TSS 32-bit data! Ignore the Stack data for different privilege levels and CR3(PDBR)!
	{
		debugger_forceimmediatelogging = 1; //Log!
		MMU_wdw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, TSS->generalpurposeregisters[i++],0); //Write the TSS! Don't be afraid of errors, since we're always accessable!
	}
	i = 0;
	for (n=(((8+10)*4));n<((8+10+6)*4);n+=4) //Write our TSS 16-bit data! Ignore the LDT and I/O map/T-bit, as it's read-only!
	{
		debugger_forceimmediatelogging = 1; //Log!
		MMU_wdw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n, TSS->segmentregisters[i++],0); //Write the TSS! Don't be afraid of errors, since we're always accessable!
	}
	debugger_forceimmediatelogging = 0; //Don't log!
}

byte checksaveTSS32()
{
	word n;
	for (n =(8*4);n<((8+10)*4);n+=4) //Write our TSS 32-bit data! Ignore the Stack data for different privilege levels and CR3(PDBR)!
	{
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess32(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR,n+0,0|0x40,0,0,0)) {debugger_forceimmediatelogging = 0; return 1;} //Error out!
	}

	for (n=(((8+10)*4));n<((8+10+6)*4);n+=4) //Write our TSS 16-bit data! Ignore the LDT and I/O map/T-bit, as it's read-only!
	{
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess32(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR,n+0,0|0x40,0,0,0)) {debugger_forceimmediatelogging = 0; return 1;} //Error out!
	}
	debugger_forceimmediatelogging = 0; //Don't log!

	for (n = (8 * 4); n < ((8 + 10) * 4); n += 4) //Write our TSS 32-bit data! Ignore the Stack data for different privilege levels and CR3(PDBR)!
	{
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess32(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n + 0, 0|0xA0, 0, 0, 0)) { debugger_forceimmediatelogging = 0; return 1; } //Error out!
	}

	for (n = (((8 + 10) * 4)); n < ((8 + 10 + 6) * 4); n += 4) //Write our TSS 16-bit data! Ignore the LDT and I/O map/T-bit, as it's read-only!
	{
		debugger_forceimmediatelogging = 1; //Log!
		if (checkMMUaccess32(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, n + 0, 0|0xA0, 0, 0, 0)) { debugger_forceimmediatelogging = 0; return 1; } //Error out!
	}
	debugger_forceimmediatelogging = 0; //Don't log!
	return 0; //OK!
}


byte enableMMUbuffer; //To buffer the MMU writes?

extern word CPU_exec_CS; //Save for handling!
extern uint_32 CPU_exec_EIP; //Save for handling!

extern word CPU_exec_lastCS; //OPCode CS
extern uint_32 CPU_exec_lastEIP; //OPCode EIP
extern byte advancedlog; //Advanced log setting

extern byte MMU_logging; //Are we logging from the MMU?

byte CPU_switchtask(int whatsegment, SEGMENT_DESCRIPTOR *LOADEDDESCRIPTOR, word *segment, word destinationtask, byte isJMPorCALL, byte gated, int_64 errorcode) //Switching to a certain task?
{
	//byte isStackSwitch = 0; //Stack switch?
	//Both structures to use for the TSS!
	word LDTsegment;
	word oldtask;
	//byte busy=0;
	byte TSS_dirty = 0; //Is the new TSS dirty?
	TSS286 TSS16;
	TSS386 TSS32;
	byte TSSSize = 0; //The TSS size!
	sbyte loadresult;

	if (errorcode>=0) //Error code to be pushed on the stack(not an interrupt without error code or errorless task switch)?
	{
		hascallinterrupttaken_type = INTERRUPTGATETIMING_TASKGATE; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task gate flag. Left at 0xFF when nothing is used(unknown case?)
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

	enableMMUbuffer = 0; //Disable any MMU buffering: we need to update memory directly and properly, in order to work!

	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		dolog("debugger", "Switching task to task %04X", destinationtask);
	}

	uint_32 limit; //The limit we use!

	switch (GENERALSEGMENTPTR_TYPE(LOADEDDESCRIPTOR)) //Check the type of descriptor we're switching to!
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
			goto invaliddsttask; //Thow #GP!
		}
		break;
	default: //Invalid descriptor!
	invaliddsttask:
		THROWDESCGP(destinationtask, ((isJMPorCALL & 0x400) >> 10), (CPU[activeCPU].registers->TR & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Thow #GP!
		return 0; //Error out!
	}

	if (GENERALSEGMENTPTR_P(LOADEDDESCRIPTOR) == 0) //Not present?
	{
		THROWDESCNP(destinationtask, ((isJMPorCALL & 0x400) >> 10), (destinationtask & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Throw #NP!
		return 0; //Error out!
	}

#ifdef FORCE_16BITTSS
	if (EMULATED_CPU == CPU_80286) TSSSize = 0; //Force 16-bit TSS on 286!
#endif

	limit = LOADEDDESCRIPTOR->PRECALCS.limit; //Limit!

	if (limit < (uint_32)(TSSSize ? 43 : 103)) //Limit isn't high enough(>=103 for 386+, >=43 for 80286)?
	{
		CPU_TSSFault(destinationtask, ((isJMPorCALL & 0x400) >> 10), (destinationtask & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Throw #TS!
		return 0; //Error out!
	}

	//Now going to switch to the current task, save the registers etc in the current task!

	if (isJMPorCALL == 3) //IRET?
	{
		if ((LOADEDDESCRIPTOR->desc.AccessRights & 2) == 0) //Destination task is available?
		{
			CPU_TSSFault(destinationtask, ((isJMPorCALL & 0x400) >> 10), (destinationtask & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Throw #GP!
			return 0; //Error out!
		}
	}

	//Check and prepare source task information!
	switch (GENERALSEGMENT_TYPE(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR])) //Check the type of descriptor we're switching from!
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
		THROWDESCGP(CPU[activeCPU].registers->TR, ((isJMPorCALL & 0x400) >> 10), (CPU[activeCPU].registers->TR & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Thow #GP!
		return 0; //Error out!
	}

	if (TSSSize) //32-bit TSS?
	{
		memset(&TSS32, 0, sizeof(TSS32)); //Read the TSS! Don't be afraid of errors, since we're always accessable!
	}
	else //16-bit TSS?
	{
		memset(&TSS16, 0, sizeof(TSS16)); //Read the TSS! Don't be afraid of errors, since we're always accessable!
	}

	if (GENERALSEGMENT_P(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR])) //Valid task to switch FROM?
	{
		if ((MMU_logging == 1) && advancedlog) //Are we logging?
		{
			dolog("debugger", "Preparing outgoing task %04X for transfer", CPU[activeCPU].registers->TR);
		}

		if (TSSSize) //32-bit switchimg out?
		{
			if (checksaveTSS32()) return 0; //Abort on error!
		}
		else //16-bit switching out?
		{
			if (checksaveTSS16()) return 0; //Abort on error!
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

		if ((MMU_logging == 1) && advancedlog) //Are we logging?
		{
			dolog("debugger", "Saving outgoing task %04X to memory", CPU[activeCPU].registers->TR);
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
	else //Invalid task to switch FROM?
	{
		goto invalidsrctask; //Invalid source task!
	}

	oldtask = CPU[activeCPU].registers->TR; //Save the old task, for backlink purposes!

	//Now, load all the registers required as needed!
	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		dolog("debugger", "Switching active TSS to segment selector %04X", destinationtask);
	}
	//Backup of the TR register&descriptor isn't needed, as this is automatically done when loading it!

	if (segmentWritten(CPU_SEGMENT_TR, destinationtask, (isJMPorCALL & 0x400)|((isJMPorCALL == 3)?0x800:0))) return 0; //Execute the task switch itself, loading our new descriptor! //Abort on fault: invalid(or busy) task we're switching to!

	if ((isJMPorCALL | 0x80) != 0x82) //Not a call? Stop being busy to switch to another task(or ourselves)!
	{
		SEGMENT_DESCRIPTOR tempdesc;
		if (LOADDESCRIPTOR(CPU_SEGMENT_TR, oldtask, &tempdesc, (isJMPorCALL & 0x400)) == 1) //Loaded old container?
		{
			tempdesc.desc.AccessRights &= ~2; //Mark idle!
			if (SAVEDESCRIPTOR(CPU_SEGMENT_TR, oldtask, &tempdesc, (isJMPorCALL & 0x400)) <= 0) //Save the new status into the old descriptor!
			{
				return 0; //Abort on fault raised!
			}
		}
		else return 0; //Abort on fault raised!
	}

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
		THROWDESCGP(destinationtask, ((isJMPorCALL & 0x400) >> 10), (destinationtask & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Thow #GP!
		return 0; //Error out!
	}

#ifdef FORCE_16BITTSS
	if (EMULATED_CPU == CPU_80286) TSSSize = 0; //Force 16-bit TSS on 286!
#endif

	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		dolog("debugger", "Loading incoming TSS %04X state", CPU[activeCPU].registers->TR);
	}

	if (TSSSize) //32-bit switching in?
	{
		if (checkloadTSS32()) return 0; //Abort on error!
		if (checksaveTSS32()) return 0; //Abort on error!
	}
	else //16-bit switching in?
	{
		if (checkloadTSS16()) return 0; //Abort on error!
		if (checksaveTSS16()) return 0; //Abort on error!
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

	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		dolog("debugger", "Checking for backlink to TSS %04X", oldtask);
	}

	if ((isJMPorCALL | 0x80) == 0x82) //CALL?
	{
		if (TSSSize) //32-bit TSS?
		{
			TSS32.BackLink = oldtask; //Save the old task as a backlink in the new task!
			TSS_dirty |= 1; //We're dirty(backlink)!
		}
		else //16-bit TSS?
		{
			TSS16.BackLink = oldtask; //Save the old task as a backlink in the new task!
			TSS_dirty |= 1; //We're dirty(backlink)!
		}
	}

	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		dolog("debugger", "Marking incoming TSS %04X busy if needed", CPU[activeCPU].registers->TR);
	}

	if (isJMPorCALL != 3) //Not an IRET?
	{
		LOADEDDESCRIPTOR->desc.AccessRights |= 2; //Mark not idle!
		if (SAVEDESCRIPTOR(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, LOADEDDESCRIPTOR, (isJMPorCALL & 0x400)) <= 0) //Save the new status into the old descriptor!
		{
			return 0; //Abort on fault raised!
		}
	}

	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		dolog("debugger", "Loading incoming TSS %04X state into the registers.", CPU[activeCPU].registers->TR);
	}

	if ((CPU[activeCPU].have_oldSegReg&(1 << CPU_SEGMENT_LDTR)) == 0) //Backup not loaded yet?
	{
		memcpy(&CPU[activeCPU].SEG_DESCRIPTORbackup[CPU_SEGMENT_LDTR], &CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR], sizeof(CPU[activeCPU].SEG_DESCRIPTORbackup[0])); //Restore the descriptor!
		CPU[activeCPU].oldSegReg[CPU_SEGMENT_LDTR] = *CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_LDTR]; //Backup the register too!
		CPU[activeCPU].have_oldSegReg |= (1 << CPU_SEGMENT_LDTR); //Loaded!
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
		CPU[activeCPU].registers->LDTR = TSS32.LDT;
		LDTsegment = TSS32.LDT; //LDT used!
		Paging_clearTLB(); //Clear the TLB: CR3 has been changed!
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
		CPU[activeCPU].registers->LDTR = TSS16.LDT;
		LDTsegment = TSS16.LDT; //LDT used!
	}

	if ((isJMPorCALL | 0x80) == 0x82) //CALL?
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
		TSS_dirty |= 2; //We're dirty((E)FLAGS)!
	}

	if (TSS_dirty) //Destination TSS dirty?
	{
		if ((MMU_logging == 1) && advancedlog) //Are we logging?
		{
			dolog("debugger", "Saving incoming TSS %04X state to memory, because the state has changed(Nested Task).", CPU[activeCPU].registers->TR);
		}

		if (TSS_dirty & 1) MMU_ww(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, 0, TSSSize ? TSS32.BackLink : TSS16.BackLink, 0); //Write the TSS Backlink to use! Don't be afraid of errors, since we're always accessable!

		if (TSS_dirty & 2) //Dirty (E)FLAGS?
		{
			if (TSSSize) //32-bit TSS?
			{
				saveTSS32(&TSS32); //Save the TSS!
			}
			else //16-bit TSS?
			{
				saveTSS16(&TSS16); //Save the TSS!
			}
		}
	}

	//At this point, the basic task switch is complete. All that remains is loading all segment descriptors as required!

	CPU[activeCPU].have_oldSegReg &= ~(1<<CPU_SEGMENT_TR); //Not supporting returning to the old task anymore, we've completed the task switch, committing to the new task!
	CPU_commitState(); //Set the new fault as a return point when faulting!
	CPU_exec_CS = CPU[activeCPU].registers->CS; //Save for error handling!
	CPU_exec_EIP = (CPU[activeCPU].registers->EIP&CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].PRECALCS.roof); //Save for error handling!
	//No last: we're entering a task that has this information, so no return point is given!
	CPU_exec_lastCS = CPU_exec_CS;
	CPU_exec_lastEIP = CPU_exec_EIP;

	//Update the x86 debugger, if needed!
	protectedModeDebugger_taskswitching(); //Apply any action required for a task switch!

	updateCPUmode(); //Make sure the CPU mode is updated, according to the task!

	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		dolog("debugger", "Loading incoming TSS LDT %04X", LDTsegment);
	}

	//Check and verify the LDT descriptor!
	SEGMENT_DESCRIPTOR LDTsegdesc;
	uint_32 descriptor_index = (LDTsegment&~0x7); //The full index within the descriptor table!

	if (!(descriptor_index&~3)) //NULL segment loaded into LDTR? Special case: no LDT available!
	{
		memset(&LDTsegdesc, 0, sizeof(LDTsegdesc)); //No descriptor available to use: mark as invalid!
	}
	else //Valid LDT index?
	{
		if (LDTsegment & 4) //We cannot reside in the LDT!
		{
			CPU_TSSFault(CPU[activeCPU].registers->TR, ((isJMPorCALL & 0x400) >> 10), (CPU[activeCPU].registers->TR & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Throw error!
			return 0; //Not present: we cannot reside in the LDT!
		}

		if ((word)(descriptor_index | 0x7) > CPU[activeCPU].registers->GDTR.limit) //GDT limit exceeded?
		{
			CPU_TSSFault(CPU[activeCPU].registers->TR, ((isJMPorCALL & 0x400) >> 10), (CPU[activeCPU].registers->TR & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Throw error!
			return 0; //Not present: limit exceeded!
		}

		loadresult = LOADDESCRIPTOR(CPU_SEGMENT_LDTR, LDTsegment, &LDTsegdesc, 0x200 | (isJMPorCALL & 0x400)); //Load it, ignore errors?
		if (unlikely(loadresult <= 0)) return 0; //Invalid LDT(due to being unpaged or other fault)?

		//Now the LDT entry is loaded for testing!
		if (GENERALSEGMENT_TYPE(LDTsegdesc) != AVL_SYSTEM_LDT) //Not an LDT?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR, ((isJMPorCALL & 0x400) >> 10), (CPU[activeCPU].registers->TR & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Throw error!
			return 0; //Not present: not an IDT!	
		}

		if (!GENERALSEGMENT_P(LDTsegdesc)) //Not present?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR, ((isJMPorCALL & 0x400) >> 10), (CPU[activeCPU].registers->TR & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Throw error!
			return 0; //Not present: not an IDT!	
		}
	}

	memcpy(&CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_LDTR], &LDTsegdesc, sizeof(CPU[activeCPU].SEG_DESCRIPTOR[0])); //Make the LDTR active by loading it into the descriptor cache!

	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		dolog("debugger", "Setting Task Switched flag in CR0");
	}

	CPU[activeCPU].registers->CR0 |= CR0_TS; //Set the high bit of the TS bit(bit 3)!

	//Now, load all normal registers in order, keeping aborts possible!
	CPU[activeCPU].faultraised = 0; //Clear the fault level: the new task has no faults by default!

	//Set the default CPL!
	CPU[activeCPU].CPL = (getcpumode() == CPU_MODE_8086) ? 3 : ((getcpumode() == CPU_MODE_REAL) ? 0 : getRPL(TSSSize ? TSS32.CS : TSS16.CS)); //Load default CPL, according to the mode!

	//First, load CS!
	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		dolog("debugger", "Loading incoming TSS CS register");
	}
	destEIP = CPU[activeCPU].registers->EIP; //Save EIP for the new address, we don't want to lose it when loading!
	if (TSSSize) //32-bit?
	{
		if (segmentWritten(CPU_SEGMENT_CS, TSS32.CS, 0x200 | (isJMPorCALL & 0x400))) return 0; //Load CS!
	}
	else
	{
		if (segmentWritten(CPU_SEGMENT_CS, TSS16.CS, 0x200 | (isJMPorCALL & 0x400))) return 0; //Load CS!
	}
	CPU_flushPIQ(-1); //We're jumping to another address!
	/*
	if ((getRPL(CPU[activeCPU].registers->CS) != GENERALSEGMENT_DPL(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS])) && (getcpumode()==CPU_MODE_PROTECTED)) //Non-matching CS RPL vs CS CPL if not V86?
	{
		CPU_TSSFault(CPU[activeCPU].registers->TR, ((isJMPorCALL & 0x400) >> 10), (CPU[activeCPU].registers->TR & 4) ? EXCEPTION_TABLE_LDT : EXCEPTION_TABLE_GDT); //Throw error!
		return 0; //Not present: limit exceeded!
	}
	*/ //Doesn't make sense with conforming segments, nor with V86 segments! Also, already handled by segmentWritten.

	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		dolog("debugger", "Loading incoming TSS Stack address");
	}

	if (TSSSize) //32-bit?
	{
		if (segmentWritten(CPU_SEGMENT_SS, TSS32.SS, 0x200 | (isJMPorCALL & 0x400))) return 0; //Update the segment! Privilege must match CPL(bit 7 of isJMPorCALL==0)!
		CPU[activeCPU].registers->ESP = TSS32.ESP;
	}
	else //16-bit?
	{
		if (segmentWritten(CPU_SEGMENT_SS, TSS16.SS, 0x200 | (isJMPorCALL & 0x400))) return 0; //Update the segment! Privilege must match CPL(bit 7 of isJMPorCALL==0)!
		CPU[activeCPU].registers->SP = TSS16.SP;
	}

	//Set the default CPL!
	CPU[activeCPU].CPL = (getcpumode()==CPU_MODE_8086)?3:((getcpumode()==CPU_MODE_REAL)?0:GENERALSEGMENT_DPL(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_SS])); //Load default CPL to use from SS if needed!

	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		dolog("debugger","Loading remaining TSS segment registers");
	}
	if (TSSSize) //32-bit?
	{
		if (segmentWritten(CPU_SEGMENT_DS, TSS32.DS, 0x280|(isJMPorCALL&0x400))) return 0; //Load reg!
		if (segmentWritten(CPU_SEGMENT_ES, TSS32.ES, 0x280|(isJMPorCALL&0x400))) return 0; //Load reg!
		if (segmentWritten(CPU_SEGMENT_FS, TSS32.FS, 0x280|(isJMPorCALL&0x400))) return 0; //Load reg!
		if (segmentWritten(CPU_SEGMENT_GS, TSS32.GS, 0x280|(isJMPorCALL&0x400))) return 0; //Load reg!
	}
	else //16-bit?
	{
		if (segmentWritten(CPU_SEGMENT_DS, TSS16.DS, 0x280|(isJMPorCALL&0x400))) return 0; //Load reg!
		if (segmentWritten(CPU_SEGMENT_ES, TSS16.ES, 0x280|(isJMPorCALL&0x400))) return 0; //Load reg!
		if (segmentWritten(CPU_SEGMENT_FS, 0, 0x280|(isJMPorCALL&0x400))) return 0; //Load reg: FS is unusable!
		if (segmentWritten(CPU_SEGMENT_GS, 0, 0x280|(isJMPorCALL&0x400))) return 0; //Load reg: GS is unusable!
	}

	//All segments are valid and readable!

	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		dolog("debugger","New task ready for execution.");
	}

	uint_32 errorcode32 = (uint_32)errorcode; //Get the error code itelf!
	word errorcode16 = (word)errorcode; //16-bit variant, if needed!

	if (errorcode>=0) //Error code to be pushed on the stack(not an interrupt without error code or errorless task switch)?
	{
		if (checkStackAccess(1,1,TSSSize)) return 0; //Abort on fault!
		if (TSSSize) //32-bit task?
		{
			CPU_PUSH32(&errorcode32); //Push the error on the stack!
		}
		else
		{
			CPU_PUSH16(&errorcode16,0); //Push the error on the stack!
		}
		hascallinterrupttaken_type = INTERRUPTGATETIMING_TASKGATE; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task gate flag. Left at 0xFF when nothing is used(unknown case?)
	}

	CPU[activeCPU].faultlevel = 0; //Clear the fault level: the new task has no faults by default!

	if (TSSSize) //32-bit TSS?
	{
		if ((TSS32.T & 1) && (FLAG_RF == 0)) //Trace bit set? Cause a debug exception when this context is run?
		{
			if (protectedModeDebugger_taskswitched()) return 0; //Finished task switch with debugger interrupt handling!
		}
	}

	CPU[activeCPU].executed = 1; //Task switch completed!
	return 0; //Abort any running instruction operation, finish up!
}

void CPU_TSSFault(word segmentval, byte is_external, byte tbl)
{
	uint_32 errorcode;
	errorcode = (segmentval&0xFFF8)|(is_external&1)|((tbl&3)<<1);
	if ((MMU_logging == 1) && advancedlog) //Are we logging?
	{
		dolog("debugger","#TS fault(%08X)!",errorcode);
	}
	CPU_resetOP(); //Point to the faulting instruction!
	
	if (CPU_faultraised(EXCEPTION_INVALIDTSSSEGMENT)) //We're raising a fault!
	{
		CPU_executionphase_startinterrupt(EXCEPTION_INVALIDTSSSEGMENT,2,errorcode); //Call IVT entry #13 decimal!
	}
}
