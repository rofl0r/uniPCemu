#include "headers/types.h" //Basic types!
#include "headers/cpu/cpu.h" //CPU!
#include "headers/bios/bios.h" //BIOS!
#include "headers/cpu/easyregs.h" //Easy registers!
#include "headers/cpu/cb_manager.h" //Callback support!
#include "headers/cpu/protection.h" //Protection support!
#include "headers/emu/debugger/debugger.h" //For logging registers!
#include "headers/cpu/multitasking.h" //Multitasking support!
#include "headers/mmu/mmuhandler.h" //Direct memory access support! 

//Are we to disable NMI's from All(or Memory only)?
#define DISABLE_MEMNMI
//#define DISABLE_NMI

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

OPTINLINE void CPU_customint(byte intnr, word retsegment, uint_32 retoffset) //Used by soft (below) and exceptions/hardware!
{
	if (getcpumode()==CPU_MODE_REAL) //Use IVT structure in real mode only!
	{
		CPU_PUSH16(&REG_FLAGS); //Push flags!
		CPU_PUSH16(&retsegment); //Push segment!
		word retoffset16 = (retoffset&0xFFFF);
		CPU_PUSH16(&retoffset16);
		FLAG_IF = 0; //We're calling the interrupt!
		FLAG_TF = 0; //We're calling an interrupt, resetting debuggers!
//Now, jump to it!
		destEIP = memory_directrw((intnr << 2)+CPU[activeCPU].registers->IDTR.base); //JUMP to position CS:EIP/CS:IP in table.
		segmentWritten(CPU_SEGMENT_CS,memory_directrw(((intnr<<2)|2) + CPU[activeCPU].registers->IDTR.base),0); //Interrupt to position CS:EIP/CS:IP in table.
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
			desttask = MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, 0, 0); //Read the destination task!
			if (!LOADDESCRIPTOR(CPU_SEGMENT_TR, desttask, &newdescriptor)) //Error loading new descriptor? The backlink is always at the start of the TSS!
			{
				return; //Error, by specified reason!
			}
			CPU_switchtask(CPU_SEGMENT_TR,&newdescriptor,&CPU[activeCPU].registers->TR,desttask,3); //Execute an IRET to the interrupted task!
		}
		else //Normal IRET?
		{
			if (CPU_Operand_size[activeCPU]) //32-bit mode?
			{
				destEIP = CPU_POP32(); //POP EIP!
			}
			else
			{
				destEIP = CPU_POP16(); //POP IP!
			}
			segmentWritten(CPU_SEGMENT_CS,CPU_POP16(),3); //We're loading because of an IRET!
			CPU_flushPIQ(); //We're jumping to another address!
			if (CPU[activeCPU].faultraised == 0) //No fault raised?
			{
				if (CPU_Operand_size[activeCPU]) //32-bit mode?
				{
					REG_EFLAGS = CPU_POP32(); //Pop flags!
				}
				else
				{
					REG_FLAGS = CPU_POP16(); //Pop flags!
				}
			}
		}
	}
	//Special effect: re-enable NMI!
	NMIMasked = 0; //We're allowing NMI again!
}

extern byte SystemControlPortA; //System control port A data!
extern byte SystemControlPortB; //System control port B data!
extern byte PPI62; //For XT support!
byte NMI = 1; //NMI Disabled?

extern word CPU_exec_CS;
extern uint_32 CPU_exec_EIP;

byte execNMI(byte causeisMemory) //Execute an NMI!
{
	byte doNMI = 0;
	if (causeisMemory) //I/O error on memory?
	{
		if (EMULATED_CPU >= CPU_80286) //AT?
		{
			if ((SystemControlPortB & 4)==0) //Parity check enabled(the enable bits are reversed according to the AT BIOS)?
			{
				SystemControlPortB |= 0x80; //Signal a Memory error!
				doNMI = 1; //Allow NMI, if enabled!
			}
		}
		else //XT?
		{
			if ((SystemControlPortB & 0x10)==0) //Enabled?
			{
				PPI62 |= 0x80; //Signal a Memory error on a XT!
				doNMI = 1; //Allow NMI, if enabled!
			}
		}
		#ifdef DISABLE_MEMNMI
			return 1; //We don't handle any NMI's from Bus or Memory through the NMI PIN!
		#endif
	}
	else //Cause is I/O?
	{
		//Bus error?
		if (EMULATED_CPU >= CPU_80286) //AT?
		{
			if ((SystemControlPortB & 8)==0) //Channel check enabled(the enable bits are reversed according to the AT BIOS)?
			{
				SystemControlPortB |= 0x40; //Signal a Bus error!
				doNMI = 1; //Allow NMI, if enabled!
			}
		}
		else //XT?
		{
			if (SystemControlPortB & 0x20) //Parity check enabled?
			{
				PPI62 |= 0x40; //Signal a Parity error on a XT!
				doNMI = 1; //Allow NMI, if enabled!
			}
		}
	}

#ifdef DISABLE_NMI
	return 1; //We don't handle any NMI's from Bus or Memory through the NMI PIN!
#endif
	if (!NMI && !NMIMasked) //NMI interrupt enabled and not masked off?
	{
		NMIMasked = 1; //Mask future NMI!
		if (doNMI) //I/O error on memory or bus?
		{
			CPU_customint(EXCEPTION_NMI, CPU_exec_CS, CPU_exec_EIP); //Return to opcode!
			CPU[activeCPU].cycles_HWOP = 50; /* Normal interrupt as hardware interrupt */
			return 0; //We're handled!
		}
	}
	return 1; //Unhandled NMI!
}