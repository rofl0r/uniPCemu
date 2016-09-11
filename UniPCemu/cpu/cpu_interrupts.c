#include "headers/types.h" //Basic types!
#include "headers/cpu/cpu.h" //CPU!
#include "headers/bios/bios.h" //BIOS!
#include "headers/cpu/easyregs.h" //Easy registers!
#include "headers/cpu/cb_manager.h" //Callback support!
#include "headers/cpu/protection.h" //Protection support!
#include "headers/emu/debugger/debugger.h" //For logging registers!
#include "headers/cpu/multitasking.h" //Multitasking support!

void CPU_setint(byte intnr, word segment, word offset) //Set real mode IVT entry!
{
	MMU_ww(-1,0x0000,((intnr<<2)|2),segment); //Copy segment!
	MMU_ww(-1,0x0000,(intnr<<2),offset); //Copy offset!
}

void CPU_getint(byte intnr, word *segment, word *offset) //Set real mode IVT entry!
{
	*segment = MMU_rw(-1,0x0000,((intnr<<2)|2),0); //Copy segment!
	*offset = MMU_rw(-1,0x0000,(intnr<<2),0); //Copy offset!
}

extern uint_32 destEIP;

extern byte startreached; //When to start logging?

OPTINLINE void CPU_customint(byte intnr, word retsegment, uint_32 retoffset) //Used by soft (below) and exceptions/hardware!
{
	if (getcpumode()==CPU_MODE_REAL) //Use IVT?
	{
		CPU_PUSH16(&REG_FLAGS); //Push flags!
		CPU_PUSH16(&retsegment); //Push segment!
		word retoffset16 = (retoffset&0xFFFF);
		CPU_PUSH16(&retoffset16);
		FLAG_IF = 0; //We're calling the interrupt!
		FLAG_TF = 0; //We're calling an interrupt, resetting debuggers!
//Now, jump to it!
		destEIP = MMU_rw(-1, 0x0000, (intnr << 2), 0); //JUMP to position CS:EIP/CS:IP in table.
		segmentWritten(CPU_SEGMENT_CS,MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,0x0000,(intnr<<2)|2,0),0); //Interrupt to position CS:EIP/CS:IP in table.
	}
	else //Use Protected mode IVT?
	{
		CPU_ProtectedModeInterrupt(intnr,0,retsegment,retoffset,0); //Execute the protected mode interrupt!
	}
}


void CPU_INT(byte intnr) //Call an software interrupt; WARNING: DON'T HANDLE ANYTHING BUT THE REGISTERS ITSELF!
{
	//Now, jump to it!
	CPU_customint(intnr,REG_CS,REG_EIP); //Execute real interrupt, returning to current address!
}

byte NMIMasked = 0; //Are NMI masked?

void CPU_IRET()
{
	if (getcpumode()==CPU_MODE_REAL) //Use IVT?
	{
		destEIP = CPU_POP16(); //POP IP!
		segmentWritten(CPU_SEGMENT_CS,CPU_POP16(),3); //We're loading because of an IRET!
		CPU_flushPIQ(); //We're jumping to another address!
		if (CPU[activeCPU].faultraised==0) //No fault raised?
		{
			REG_FLAGS = CPU_POP16(); //Pop flags!
		}
	}
	else //Use protected mode IRET?
	{
		if (FLAG_NT && (getcpumode() != CPU_MODE_REAL)) //Protected mode Nested Task IRET?
		{
			SEGDESCRIPTOR_TYPE newdescriptor; //Temporary storage!
			word desttask;
			desttask = MMU_rw(CPU_SEGMENT_TR, CPU->registers->TR, 0, 0); //Read the destination task!
			if (!LOADDESCRIPTOR(CPU_SEGMENT_TR, desttask, &newdescriptor)) //Error loading new descriptor? The backlink is always at the start of the TSS!
			{
				return; //Error, by specified reason!
			}
			CPU_switchtask(CPU_SEGMENT_TR,&newdescriptor,&CPU[activeCPU].registers->TR,desttask,3); //Execute an IRET to the interrupted task!
		}
		else //Normal IRET?
		{
			destEIP = CPU_POP32(); //POP EIP!
			segmentWritten(CPU_SEGMENT_CS,CPU_POP16(),3); //We're loading because of an IRET!
			CPU_flushPIQ(); //We're jumping to another address!
			if (CPU[activeCPU].faultraised == 0) //No fault raised?
			{
				REG_EFLAGS = CPU_POP32(); //Pop flags!
			}
		}
	}
	//Special effect: re-enable NMI!
	NMIMasked = 0; //We're allowing NMI again!
}

extern byte SystemControlPortA; //System control port A data!
extern byte SystemControlPortB; //System control port B data!
byte NMI = 1; //NMI Disabled?

extern word CPU_exec_CS;
extern uint_32 CPU_exec_EIP;

byte execNMI(byte causeisMemory) //Execute an NMI!
{
	if (!NMI && !NMIMasked) //NMI interrupt enabled and not masked off?
	{
		NMIMasked = 1; //Mask future NMI!
		if (causeisMemory) //I/O error on memory?
		{
			if (SystemControlPortB & 4) //Enabled?
			{
				SystemControlPortB |= 0x80; //Signal a Memory error!
				CPU_customint(EXCEPTION_NMI, CPU_exec_CS, CPU_exec_EIP); //Return to opcode!
				CPU[activeCPU].cycles_HWOP = 50; /* Normal interrupt as hardware interrupt */
				return 0; //We're handled!
			}
		}
		else if (!causeisMemory) //Bus error?
		{
			if (SystemControlPortB & 8) //Enabled?
			{
				SystemControlPortB |= 0x40; //Signal a Bus error!
				CPU_customint(EXCEPTION_NMI, CPU_exec_CS, CPU_exec_EIP); //Return to opcode!
				CPU[activeCPU].cycles_HWOP = 50; /* Normal interrupt as hardware interrupt */
				return 0; //We're handled!
			}
		}
	}
	return 1; //Unhandled NMI!
}