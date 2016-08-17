#include "headers/types.h"
#include "headers/bios/biosmenu.h" //For key support!
#include "headers/mmu/mmu.h" //MMU support!
#include "headers/cpu/protection.h" //For CPU_segment_index!
#include "headers/emu/input.h" //Direct input support!
#include "headers/cpu/easyregs.h" //Easy register support!
//Boot failure.

//Execute ROM BASIC which is at address 0F600h:0000h

void BIOS_int18()
{
	char msg1[256] = "Non-System disk or disk error"; //First row!
	char msg2[256] = "replace and strike any key when ready"; //Second row!
	printmsg(0xF,"%s\r\n",msg1); //First part of the message!
	printmsg(0xF,"%s\r\n",msg2); //Second part of the message!
	//Booting is left to the next step in assembly code!
}