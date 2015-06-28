#include "headers/cpu/cpu.h"
#include "headers/cpu/easyregs.h"

void CPU486_CPUID()
{
	switch (REG_EAX)
	{
	case 0x00: //Highest function parameter!
		REG_EAX = 0; //No function parameters supported!
		//GenuineIntel!
		REG_EBX = 0x756e6547;
		REG_EDX = 0x49656e69;
		REG_ECX = 0x6c65746e;
		break;
	default:
		break;
	}
}