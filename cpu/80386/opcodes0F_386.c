/* Same as opcodes, but the 0F extension! */

#include "headers/types.h" //Basic types
#include "headers/cpu/CPU.h" //CPU needed!
#include "headers/mmu/MMU.h" //MMU needed!
#include "headers/cpu/easyregs.h" //Easy register compatibility!
#include "headers/cpu/modrm.h" //MODR/M compatibility!
#include "headers/support/signedness.h" //CPU support functions!

//Opcodes based on: http://www.logix.cz/michal/doc/i386/chp17-a3.htm#17-03-A

void CPU_OP0F07()
{
	/*word tablesegment = REG_ES; uint_32 tableoffset = REG_EDI;*/
} /* LOADALL  */