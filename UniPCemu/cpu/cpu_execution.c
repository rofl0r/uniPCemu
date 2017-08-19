#include "headers/cpu/cpu.h" //Basic CPU support!
#include "headers/cpu/cpu_execution.h" //Execution support!
#include "headers/cpu/interrupts.h" //Interrupt support!
#include "headers/cpu/multitasking.h" //Multitasking support!

extern Handler currentOP_handler; //Current opcode handler!
void CPU_executionphase_normal() //Executing an opcode?
{
	currentOP_handler(); //Now go execute the OPcode once in the runtime!
	//Don't handle unknown opcodes here: handled by native CPU parser, defined in the opcode jmptbl.
}

struct
{
int whatsegment;
SEGDESCRIPTOR_TYPE LOADEDDESCRIPTOR;
word *segment;
word destinationtask;
byte isJMPorCALL;
byte gated;
int_64 errorcode;
} TASKSWITCH_INFO;

void CPU_executionphase_taskswitch() //Are we to switch tasks?
{
	//TODO: Execute task switch!
	if (CPU_switchtask(TASKSWITCH_INFO.whatsegment, &TASKSWITCH_INFO.LOADEDDESCRIPTOR,TASKSWITCH_INFO.segment, TASKSWITCH_INFO.destinationtask, TASKSWITCH_INFO.isJMPorCALL, TASKSWITCH_INFO.gated, TASKSWITCH_INFO.errorcode)!=0) //Unfinished task switch?
	{
		CPU[activeCPU].executed = 0; //Finished and ready for execution!
	}
}

byte CPU_executionphaseinterrupt_nr = 0x00; //What interrupt to execute?
byte CPU_executionphaseinterrupt_type3 = 0; //Are we a type3 interrupt?
int_64 CPU_executionphaseinterrupt_errorcode = -1; //What code to push afterwards?

void CPU_executionphase_interrupt() //Executing an interrupt?
{
	byte result;
	if (EMULATED_CPU<=CPU_NECV30) //16-bit CPU?
	{
		result = call_soft_inthandler(CPU_executionphaseinterrupt_nr,CPU_executionphaseinterrupt_errorcode);
		if (result) //Final stage?
		{
			CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/
		}
	}
	else //Unsupported CPU? Use plain general interrupt handling instead!
	{
		if (call_soft_inthandler(CPU_executionphaseinterrupt_nr,CPU_executionphaseinterrupt_errorcode)==0) return; //Execute the interupt!
		if (CPU_apply286cycles()) return; //80286+ cycles instead?
	}
}

Handler currentEUphasehandler = NULL; //Current execution phase handler, start of with none loaded yet!

void CPU_executionphase_newopcode() //Starting a new opcode to handle?
{
	currentEUphasehandler = &CPU_executionphase_normal; //Starting a opcode phase handler!
}

void CPU_executionphase_startinterrupt(byte vectornr, byte type3, int_64 errorcode) //Starting a new interrupt to handle?
{
	currentEUphasehandler = &CPU_executionphase_interrupt; //Starting a interrupt phase handler!
	//Copy all parameters used!
	CPU_executionphaseinterrupt_errorcode = errorcode; //Save the error code!
	CPU_executionphaseinterrupt_nr = vectornr; //Vector number!
	CPU_executionphaseinterrupt_type3 = type3; //Are we a type-3 interrupt?
	CPU[activeCPU].executed = 0; //Not executed yet!
}

void CPU_executionphase_starttaskswitch(int whatsegment, SEGDESCRIPTOR_TYPE *LOADEDDESCRIPTOR,word *segment, word destinationtask, byte isJMPorCALL, byte gated, int_64 errorcode) //Switching to a certain task?
{
	currentEUphasehandler = &CPU_executionphase_interrupt; //Starting a interrupt phase handler!
	//Copy all parameters used!
	memcpy(&TASKSWITCH_INFO.LOADEDDESCRIPTOR,LOADEDDESCRIPTOR,sizeof(TASKSWITCH_INFO.LOADEDDESCRIPTOR)); //Copy the descriptor over!
	TASKSWITCH_INFO.whatsegment = whatsegment;
	TASKSWITCH_INFO.segment = segment;
	TASKSWITCH_INFO.destinationtask = destinationtask;
	TASKSWITCH_INFO.isJMPorCALL = isJMPorCALL;
	TASKSWITCH_INFO.gated = gated;
	TASKSWITCH_INFO.errorcode = errorcode;
	CPU[activeCPU].executed = 0; //Not executed yet!
}

byte CPU_executionphase_busy() //Are we busy?
{
	return (currentEUphasehandler!=NULL); //Are we ready to operate on something?
}

//Actual phase handler that transfers to the current phase!
void CPU_OP() //Normal CPU opcode execution!
{
	if (unlikely(currentEUphasehandler==NULL)) return; //Abort when invalid!
	currentEUphasehandler(); //Start execution of the current phase in the EU!
	if (unlikely(CPU[activeCPU].executed))
	{
		currentEUphasehandler = NULL; //Finished instruction!
	}
}