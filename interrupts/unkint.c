#include "headers/cpu/cpu.h"
#include "headers/cpu/easyregs.h" //Easy!

void BIOS_unkint()
{
	AX = 0; //Unknown interrupt!
	CF = 1; //Set carry flag!
}