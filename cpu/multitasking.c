#include "headers/cpu/cpu.h" //CPU reqs!
#include "headers/cpu/multitasking.h" //Our typedefs!

//Everything concerning TSS.

int TSS_PrivilegeChanges(int whatsegment,SEGDESCRIPTOR_TYPE *LOADEDDESCRIPTOR,word segment)
{
	return 1; //Error: not build yet!
}