#include "headers/cpu/cpu.h" //CPU reqs!
#include "headers/cpu/multitasking.h" //Our typedefs!

//Everything concerning TSS.

int TSS_PrivilegeChanges(int whatsegment,SEGDESCRIPTOR_TYPE *LOADEDDESCRIPTOR,word segment)
{
	return 1; //Error: not build yet!
}

byte CPU_switchtask(int whatsegment, SEGDESCRIPTOR_TYPE *LOADEDDESCRIPTOR,word *segment, word destinationtask) //Switching to a certain task?
{
	return 1; //Error: not build yet!
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