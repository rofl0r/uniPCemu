#include "headers/cpu/cpu.h" //Basic CPU support!
#include "headers/cpu/cpu_execution.h" //Execution support!
#include "headers/cpu/interrupts.h" //Interrupt support!
#include "headers/cpu/multitasking.h" //Multitasking support!
#include "headers/cpu/biu.h" //BIU support for making direct memory requests!
#include "headers/support/log.h" //To log invalids!

//Define to debug disk reads using interrupt 13h
//#define DEBUGBOOT

//Memory access functionality with Paging!
byte CPU_request_MMUrb(sword segdesc, uint_32 offset, byte is_offset16)
{
	if ((segdesc>=0) || (segdesc==-4))
	{
		offset = MMU_realaddr(segdesc,(segdesc>=0)?*CPU[activeCPU].SEGMENT_REGISTERS[segdesc&0x7]:*CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_ES], offset, 0, is_offset16); //Real adress translated through the MMU! -4=ES!
		return BIU_request_Memoryrb(offset,1); //Request a read!
	}
	else //Paging/direct access?
	{
		return BIU_request_Memoryrb(offset,0); //Request a read!
	}
}

byte CPU_request_MMUrw(sword segdesc, uint_32 offset, byte is_offset16)
{
	if ((segdesc>=0) || (segdesc==-4))
	{
		offset = MMU_realaddr(segdesc,(segdesc>=0)?*CPU[activeCPU].SEGMENT_REGISTERS[segdesc&0x7]:*CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_ES], offset, 0, is_offset16); //Real adress translated through the MMU! -4=ES!
		return BIU_request_Memoryrw(offset,1); //Request a read!
	}
	else //Paging/direct access?
	{
		return BIU_request_Memoryrw(offset,0); //Request a read!
	}
}

byte CPU_request_MMUrdw(sword segdesc, uint_32 offset, byte is_offset16)
{
	if ((segdesc>=0) || (segdesc==-4))
	{
		offset = MMU_realaddr(segdesc,(segdesc>=0)?*CPU[activeCPU].SEGMENT_REGISTERS[segdesc&0x7]:*CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_ES], offset, 0, is_offset16); //Real adress translated through the MMU! -4=ES!
		return BIU_request_Memoryrdw(offset,1); //Request a read!
	}
	else //Paging/direct access?
	{
		return BIU_request_Memoryrdw(offset,0); //Request a read!
	}
}

byte CPU_request_MMUwb(sword segdesc, uint_32 offset, byte val, byte is_offset16)
{
	if ((segdesc>=0) || (segdesc==-4))
	{
		offset = MMU_realaddr(segdesc,(segdesc>=0)?*CPU[activeCPU].SEGMENT_REGISTERS[segdesc&0x7]:*CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_ES], offset, 0, is_offset16); //Real adress translated through the MMU! -4=ES!
		return BIU_request_Memorywb(offset,val,1); //Request a write!
	}
	else //Paging/direct access?
	{
		return BIU_request_Memorywb(offset,val,0); //Request a write!
	}
}

byte CPU_request_MMUww(sword segdesc, uint_32 offset, word val, byte is_offset16)
{
	if ((segdesc>=0) || (segdesc==-4))
	{
		offset = MMU_realaddr(segdesc,(segdesc>=0)?*CPU[activeCPU].SEGMENT_REGISTERS[segdesc&0x7]:*CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_ES], offset, 0, is_offset16); //Real adress translated through the MMU! -4=ES!
		return BIU_request_Memoryww(offset,val,1); //Request a write!
	}
	else //Paging/direct access?
	{
		return BIU_request_Memoryww(offset,val,0); //Request a write!
	}
}

byte CPU_request_MMUwdw(sword segdesc, uint_32 offset, uint_32 val, byte is_offset16)
{
	if ((segdesc>=0) || (segdesc==-4))
	{
		offset = MMU_realaddr(segdesc,(segdesc>=0)?*CPU[activeCPU].SEGMENT_REGISTERS[segdesc&0x7]:*CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_ES], offset, 0, is_offset16); //Real adress translated through the MMU! -4=ES!
		return BIU_request_Memorywdw(offset,val,1); //Request a write!
	}
	else //Paging/direct access?
	{
		return BIU_request_Memorywdw(offset,val,0); //Request a write!
	}
}

//Execution phases itself!

extern Handler currentOP_handler; //Current opcode handler!
void CPU_executionphase_normal() //Executing an opcode?
{
	currentOP_handler(); //Now go execute the OPcode once in the runtime!
	//Don't handle unknown opcodes here: handled by native CPU parser, defined in the opcode jmptbl.
}

struct
{
int whatsegment;
SEGMENT_DESCRIPTOR LOADEDDESCRIPTOR;
word *segment;
word destinationtask;
byte isJMPorCALL;
byte gated;
int_64 errorcode;
} TASKSWITCH_INFO;
byte taskswitch_result;

void CPU_executionphase_taskswitch() //Are we to switch tasks?
{
	taskswitch_result = CPU_switchtask(TASKSWITCH_INFO.whatsegment, &TASKSWITCH_INFO.LOADEDDESCRIPTOR,TASKSWITCH_INFO.segment, TASKSWITCH_INFO.destinationtask, TASKSWITCH_INFO.isJMPorCALL, TASKSWITCH_INFO.gated, TASKSWITCH_INFO.errorcode); //Execute a task switch?
	if (taskswitch_result) //Unfinished task switch?
	{
		CPU[activeCPU].executed = 0; //Finished and ready for execution!
	}
	CPU[activeCPU].allowTF = 0; //Don't allow traps to trigger!
}

byte CPU_executionphaseinterrupt_nr = 0x00; //What interrupt to execute?
byte CPU_executionphaseinterrupt_type = 0; //Are we a type3 interrupt(bit0) or external interrupt(bit1)?
int_64 CPU_executionphaseinterrupt_errorcode = -1; //What code to push afterwards?
byte CPU_executionphaseinterrupt_is_interrupt = 0; //int instruction?
byte interrupt_result;
extern byte singlestep; //Enable EMU-driven single step!

void CPU_executionphase_interrupt() //Executing an interrupt?
{
	if (EMULATED_CPU<=CPU_NECV30) //16-bit CPU?
	{
		interrupt_result = call_soft_inthandler(CPU_executionphaseinterrupt_nr,CPU_executionphaseinterrupt_errorcode,CPU_executionphaseinterrupt_is_interrupt);
		if (interrupt_result) //Final stage?
		{
			CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/
		}
		if (interrupt_result==0) return; //Execute the interupt!
		CPU[activeCPU].faultraised = 2; //Special condition: non-fault interrupt! This is to prevent stuff like REP post-processing from executing, as this is already handled by the interrupt handler itself!
		CPU[activeCPU].allowTF = 0; //Don't allow traps to trigger!
	}
	else //Unsupported CPU? Use plain general interrupt handling instead!
	{
		interrupt_result = call_soft_inthandler(CPU_executionphaseinterrupt_nr,CPU_executionphaseinterrupt_errorcode,CPU_executionphaseinterrupt_is_interrupt);
		if (interrupt_result==0) return; //Execute the interupt!
		CPU[activeCPU].faultraised = 2; //Special condition: non-fault interrupt! This is to prevent stuff like REP post-processing from executing, as this is already handled by the interrupt handler itself!
		CPU[activeCPU].allowTF = 0; //Don't allow traps to trigger!
		if (CPU_apply286cycles()) return; //80286+ cycles instead?
	}
}

Handler currentEUphasehandler = NULL; //Current execution phase handler, start of with none loaded yet!

void CPU_executionphase_newopcode() //Starting a new opcode to handle?
{
	CPU_executionphaseinterrupt_is_interrupt = 0; //Not an interrupt!
	currentEUphasehandler = &CPU_executionphase_normal; //Starting a opcode phase handler!
}



extern word INTreturn_CS; //Return CS
extern uint_32 INTreturn_EIP; //Return EIP
void CPU_executionphase_startinterrupt(byte vectornr, byte type, int_64 errorcode) //Starting a new interrupt to handle?
{
	currentEUphasehandler = &CPU_executionphase_interrupt; //Starting a interrupt phase handler!
	CPU[activeCPU].internalinterruptstep = 0; //Reset the interrupt step!
	//Copy all parameters used!
	CPU_executionphaseinterrupt_errorcode = errorcode; //Save the error code!
	CPU_executionphaseinterrupt_nr = vectornr; //Vector number!
	CPU_executionphaseinterrupt_type = type; //Are we a what kind of type are we?
	CPU_executionphaseinterrupt_is_interrupt = (((errorcode==-2)?1:0)|(type<<1)); //Interrupt?
	CPU[activeCPU].executed = 0; //Not executed yet!
	INTreturn_CS = CPU[activeCPU].registers->CS; //Return segment!
	INTreturn_EIP = CPU[activeCPU].registers->EIP; //Save the return offset!
	#ifdef DEBUGBOOT
	if (CPU_executionphaseinterrupt_nr==0x13) //To debug?
	{
		if ((CPU[activeCPU].registers->AH==2) && (getcpumode()!=CPU_MODE_PROTECTED)) //Read sectors from drive?
		{
			singlestep = 1; //Start single stepping!
		}
	}
	#endif
	if (errorcode==-3) //Special value for T-bit in TSS being triggered?
	{
		CPU_executionphaseinterrupt_errorcode = -1; //No error code, fault!
		return; //Don't execute right away to prevent looping because of T-bit in debugger TSS.
	}
	CPU_OP(); //Execute right away for simple timing compatility!
}

byte CPU_executionphase_starttaskswitch(int whatsegment, SEGMENT_DESCRIPTOR *LOADEDDESCRIPTOR,word *segment, word destinationtask, byte isJMPorCALL, byte gated, int_64 errorcode) //Switching to a certain task?
{
	currentEUphasehandler = &CPU_executionphase_taskswitch; //Starting a task switch phase handler!
	//Copy all parameters used!
	memcpy(&TASKSWITCH_INFO.LOADEDDESCRIPTOR,LOADEDDESCRIPTOR,sizeof(TASKSWITCH_INFO.LOADEDDESCRIPTOR)); //Copy the descriptor over!
	TASKSWITCH_INFO.whatsegment = whatsegment;
	TASKSWITCH_INFO.segment = segment;
	TASKSWITCH_INFO.destinationtask = destinationtask;
	TASKSWITCH_INFO.isJMPorCALL = isJMPorCALL;
	TASKSWITCH_INFO.gated = gated;
	TASKSWITCH_INFO.errorcode = errorcode;
	CPU[activeCPU].executed = 0; //Not executed yet!
	CPU_OP(); //Execute right away for simple timing compatility!
	return taskswitch_result; //Default to an abort of the current instruction!
}

byte CPU_executionphase_busy() //Are we busy?
{
	return ((currentEUphasehandler!=&CPU_executionphase_normal)&&currentEUphasehandler); //Are we operating on something other than a (new) instruction?
}

//Actual phase handler that transfers to the current phase!
void CPU_OP() //Normal CPU opcode execution!
{
	if (unlikely(currentEUphasehandler==NULL)) { dolog("cpu","Warning: nothing to do?"); return; } //Abort when invalid!
	currentEUphasehandler(); //Start execution of the current phase in the EU!
	if (unlikely(CPU[activeCPU].executed))
	{
		currentEUphasehandler = NULL; //Finished instruction!
	}
}
