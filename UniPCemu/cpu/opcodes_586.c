#include "headers/cpu/cpu.h"
#include "headers/cpu/easyregs.h"

void CPU586_CPUID()
{
	switch (EMULATED_CPU)
	{
	case CPU_PENTIUM: //PENTIUM (586)?
		switch (REG_EAX)
		{
		case 0x00: //Highest function parameter!
			REG_EAX = 1; //Maximum 1 parameter supported!
			//GenuineIntel!
			REG_EBX = 0x756e6547;
			REG_EDX = 0x49656e69;
			REG_ECX = 0x6c65746e;
			break;
		case 0x01: //Processor info and feature bits!
			REG_EAX = 0x00000000; //Defaults!
			REG_EDX = 0x00000000; //No features!
			REG_ECX = 0x00000000; //No features!
			break;
		default: //Unknown parameter?
			break;
		}
		break;
	default:
		break;
	}
}