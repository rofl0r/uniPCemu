#include "headers/types.h" //Basic types
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/cpu/easyregs.h" //Easy registers!
#include "headers/mmu/mmu.h" //Basic MMU support!
#include "headers/cpu/cb_manager.h" //Callback support!
#include "headers/cpu/protection.h" //Protection support!


void BIOS_int11() //Get BIOS equipment list
{
	REG_AX = MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,0x0040,0x0010,0); //Give BIOS equipment word!
}