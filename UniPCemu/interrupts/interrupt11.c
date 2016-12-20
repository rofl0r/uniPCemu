#include "headers/types.h" //Basic types
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/cpu/easyregs.h" //Easy registers!
#include "headers/cpu/mmu.h" //Basic MMU support!
#include "headers/cpu/cb_manager.h" //Callback support!
#include "headers/cpu/protection.h" //Protection support!

void BIOS_int11() //Get BIOS equipment list
{
	REG_AX = MMU_rw(-1,0x0040,0x0010,0); //Give BIOS equipment word!
}