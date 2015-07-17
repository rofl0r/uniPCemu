#include "headers/types.h" //Basic types
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/cpu/easyregs.h" //Easy registers!
#include "headers/cpu/cb_manager.h" //Callback support!

void BIOS_int15()
{
	switch (REG_AH) //What function?
	{
	case 0xC0: //Get configuration?
		REG_AH = 0x00; //Supported!
		CALLBACK_SCF(0); //Set carry flag to indicate an error!
		MMU_ww(CPU_SEGMENT_ES, REG_ES, REG_BX, 8); //8 bytes following!
		switch (EMULATED_CPU) //What CPU are we emulating?
		{
		case 0: //8086?
			MMU_wb(CPU_SEGMENT_ES, REG_ES, REG_BX + 0x02, 0xFB); //PC/XT!
			break;
		case 1: //80186?
			MMU_wb(CPU_SEGMENT_ES, REG_ES, REG_BX + 0x02, 0xFB); //PC/XT!
			break;
		case 2: //80286?
		case 3: //80386?
		case 4: //80486?
		case 5: //Pentium?
			MMU_wb(CPU_SEGMENT_ES, REG_ES, REG_BX + 0x02, 0xF8); //386 or higher CPU!
			break;
		}
		MMU_wb(CPU_SEGMENT_ES, REG_ES, REG_BX + 0x03, 0x00); //Unknown submodel!
		MMU_wb(CPU_SEGMENT_ES, REG_ES, REG_BX + 0x04, 0x01); //First BIOS revision!
		MMU_wb(CPU_SEGMENT_ES, REG_ES, REG_BX + 0x05, 0x40|0x20); //We have 2nd PIC and RTC!
		MMU_wb(CPU_SEGMENT_ES, REG_ES, REG_BX + 0x06, 0x00); //No extra hardware!
		MMU_wb(CPU_SEGMENT_ES, REG_ES, REG_BX + 0x07, 0x00); //No extra support!
		MMU_wb(CPU_SEGMENT_ES, REG_ES, REG_BX + 0x08, 0x00); //No extra support!
		MMU_wb(CPU_SEGMENT_ES, REG_ES, REG_BX + 0x09, 0x00); //No enhanced mouse mode or Flash BIOS!
		break;
	default: //Unknown function? 
		REG_AH = 0x86; //Not supported!
		CALLBACK_SCF(0); //Set carry flag to indicate an error!
		break;
	}
}