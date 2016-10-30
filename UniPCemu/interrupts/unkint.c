#include "headers/cpu/cpu.h"
#include "headers/cpu/easyregs.h" //Easy!

void BIOS_unkint()
{
	REG_AX = 0; //Unknown interrupt!
	FLAGW_CF(1); //Set carry flag!
}