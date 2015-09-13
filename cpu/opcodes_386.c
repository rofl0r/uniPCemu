#include "headers/types.h" //Basic types
#include "headers/cpu/cpu.h" //CPU needed!
#include "headers/mmu/mmu.h" //MMU needed!
#include "headers/cpu/easyregs.h" //Easy register compatibility!
#include "headers/cpu/modrm.h" //MODR/M compatibility!
#include "headers/support/signedness.h" //CPU support functions!

//Opcodes based on: http://www.logix.cz/michal/doc/i386/chp17-a3.htm#17-03-A
//Special modrm/opcode differences different opcodes based on modrm: http://www.sandpile.org/x86/opc_grp.htm

//Simple opcodes:

extern MODRM_PARAMS params;    //For getting all params!

//AAA
void CPU_OP37()
{
	if (((REG_AL & 0x0F)>9)||(FLAG_AF))
	{
		REG_AL = ((REG_AL+6)&0x0F);    //AAA
		FLAG_AF = 1;
		FLAG_CF = 1;
	}
	else
	{
		FLAG_CF = 0;
		FLAG_AF = 0;
	}
	CPU[activeCPU].cycles_OP = 4;
}
//AAD
void CPU_OPD50A()
{
	REG_AL = (REG_AH*10)+REG_AL;    //AAD
	CHECK_SF(REG_AL);
	REG_AH = 0;
	CHECK_SF(REG_AL);
	CHECK_ZF(REG_AL);
	CHECK_PF(REG_AL);
	CPU[activeCPU].cycles_OP = 19;
}
//AAM
void CPU_OPD40A()
{
	REG_AH = SAFEDIV(REG_AL,10);
	REG_AL = SAFEMOD(REG_AL,10);
	CHECK_SF(REG_AL);
	CHECK_ZF(REG_AL);
	CHECK_PF(REG_AL);
	CPU[activeCPU].cycles_OP = 17;
}
//AAS
void CPU_OP3F()
{
	if (((REG_AL&0x0F)>9)||(FLAG_AF))
	{
		REG_AL = REG_AL - 6;    //AAS
		REG_AL = (REG_AL&0x0F);
		REG_AH = REG_AH - 1;
		FLAG_AF = 1;
		FLAG_CF = 1;
	}
	else
	{
		FLAG_CF = 0;
		FLAG_AF = 0;
	}
	CPU[activeCPU].cycles_OP = 4;
}
















//ADC
void CPU_OP14()
{
	byte addition = ((imm8() + FLAG_CF)&0xFF);    //ADC AL,imm8
	CHECK_AF(REG_AL,addition); /*CHECK_OF(REG_AL,addition);*/
	REG_AL = ((REG_AL + addition)&0xFF);
	CHECK_SF(REG_AL);
	CHECK_ZF(REG_AL);
	CPU[activeCPU].cycles_OP = 2;
}
//OP15 multihandler
void CPU_OP15_IW()
{
	word addition = ((imm16() + FLAG_CF)&0xFFFF);    //ADC AX,imm16
	CHECK_AF(REG_AX,addition); /*CHECK_OF(REG_AL,addition);*/
	REG_AL = ((REG_AL + addition)&0xFF);
	CHECK_SF(REG_AL);
	CHECK_ZF(REG_AL);
	CPU[activeCPU].cycles_OP = 2;
}
void CPU_OP15_ID()
{
	uint_32 addition = ((imm32() + FLAG_CF)&0xFFFFFFFF);    //ADC EAX,imm32
	CHECK_AF(REG_EAX,addition); /*CHECK_OF(REG_AX,addition);*/
	REG_EAX = ((REG_EAX + addition)&0xFF);
	CHECK_SF(REG_AL);
	CHECK_ZF(REG_AL);
	CPU[activeCPU].cycles_OP = 2;
}
void CPU_OP15()
{
	if (CPU_Operand_size[activeCPU])
	{
		/* 32-bits? */ CPU_OP15_ID();
	}
	else
	{
		/* 16-bits? */ CPU_OP15_IW();
	}
}
//ADC r/m8,16,32

//slash 2 (ADC)
void CPU_OP80_ADC()
{
	modrm_readparams(&params,8,0);
	sbyte addition = (imm8()+FLAG_CF);
	CHECK_AF(modrm_read8(&params,0),addition); /*CHECK_OF(modrm_read8(&params,0),addition);*/
	modrm_write8(&params,((modrm_read8(&params,0)+addition)&0xFF),0);
	CHECK_SF(modrm_read8(&params,0));
	CHECK_ZF(modrm_read8(&params,0));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
//OP81 multihandler
void CPU_OP81_ADC_IW()
{
	modrm_readparams(&params,16,0);
	sword addition = (imm16()+FLAG_CF);
	CHECK_AF(modrm_read16(&params,0),addition); /*CHECK_OF(modrm_read16(&params,0),addition);*/
	modrm_write16(&params,2,((modrm_read16(&params,0)+addition)&0xFFFF),0);
	CHECK_SF(modrm_read16(&params,0));
	CHECK_ZF(modrm_read16(&params,0));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP81_ADC_ID()
{
	modrm_readparams(&params,32,0);
	int_32 addition = (imm32()+FLAG_CF);
	CHECK_AF(modrm_read32(&params,0),addition); /*CHECK_OF(modrm_read32(&params,0),addition);*/
	modrm_write32(&params,2,((modrm_read32(&params,0)+addition)&0xFFFF));
	CHECK_SF(modrm_read32(&params,0));
	CHECK_ZF(modrm_read32(&params,0));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP81_ADC()
{
	if (CPU_Operand_size[activeCPU])
	{
		CPU_OP81_ADC_ID();
	}
	else
	{
		CPU_OP81_ADC_IW();
	}
}
//Same, but with imm8
void CPU_OP83_ADC_IW()
{
	modrm_readparams(&params,16,0);
	sword addition = (imm8()+FLAG_CF);
	CHECK_AF(modrm_read16(&params,0),addition); /*CHECK_OF(modrm_read16(&params,0),addition);*/
	modrm_write16(&params,2,((modrm_read16(&params,0)+addition)&0xFFFF),0);
	CHECK_SF(modrm_read16(&params,0));
	CHECK_ZF(modrm_read16(&params,0));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP83_ADC_ID()
{
	modrm_readparams(&params,32,0);
	int_32 addition = (imm8()+FLAG_CF);
	CHECK_AF(modrm_read32(&params,0),addition); /*CHECK_OF(modrm_read32(&params,0),addition);*/
	modrm_write32(&params,2,((modrm_read32(&params,0)+addition)&0xFFFF));
	CHECK_SF(modrm_read32(&params,0));
	CHECK_ZF(modrm_read32(&params,0));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP83_ADC()
{
	if (CPU_Operand_size[activeCPU])
	{
		CPU_OP83_ADC_ID();
	}
	else
	{
		CPU_OP83_ADC_IW();
	}
}

//slash r
void CPU_OP10()
{
	modrm_readparams(&params,8,2);
	byte addition = (modrm_read8(&params,1)+FLAG_CF);
	CHECK_AF(modrm_read8(&params,2),addition); /*CHECK_OF(modrm_read8(&params,2),addition);*/
	modrm_write8(&params,2,((modrm_read8(&params,2)+addition)&0xFF));
	CHECK_SF(modrm_read8(&params,2));
	CHECK_ZF(modrm_read8(&params,2));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP11_IW()
{
	modrm_readparams(&params,16,2);
	word addition = (modrm_read16(&params,1)+FLAG_CF);
	CHECK_AF(modrm_read16(&params,2),addition); /*CHECK_OF(modrm_read16(&params,2),addition);*/
	modrm_write16(&params,2,((modrm_read16(&params,2)+addition)&0xFFFF),1);
	CHECK_SF(modrm_read16(&params,2));
	CHECK_ZF(modrm_read16(&params,2));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP11_ID()
{
	modrm_readparams(&params,32,2);
	int_32 addition = (modrm_read32(&params,1)+FLAG_CF);
	CHECK_AF(modrm_read32(&params,2),addition); /*CHECK_OF(modrm_read32(&params,2),addition);*/
	modrm_write32(&params,2,((modrm_read32(&params,2)+addition)&0xFFFFFFFF));
	CHECK_SF(modrm_read32(&params,2));
	CHECK_ZF(modrm_read32(&params,2));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP11()
{
	if (CPU_Operand_size[activeCPU])
	{
		CPU_OP11_ID();
	}
	else
	{
		CPU_OP11_IW();
	}
}

//Same as 10-11, but reversed.
void CPU_OP12()
{
	modrm_readparams(&params,8,1);
	byte addition = (modrm_read8(&params,1)+FLAG_CF);
	CHECK_AF(modrm_read8(&params,1),addition); /*CHECK_OF(modrm_read8(&params,1),addition);*/
	modrm_write8(&params,2,((modrm_read8(&params,1)+addition)&0xFF));
	CHECK_SF(modrm_read8(&params,1));
	CHECK_ZF(modrm_read8(&params,1));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP13_IW()
{
	modrm_readparams(&params,16,1);
	word addition = (modrm_read16(&params,1)+FLAG_CF);
	CHECK_AF(modrm_read16(&params,1),addition); /*CHECK_OF(modrm_read16(&params,1),addition);*/
	modrm_write16(&params,2,((modrm_read16(&params,1)+addition)&0xFFFF),2);
	CHECK_SF(modrm_read16(&params,1));
	CHECK_ZF(modrm_read16(&params,1));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP13_ID()
{
	modrm_readparams(&params,32,1);
	int_32 addition = (modrm_read32(&params,1)+FLAG_CF);
	CHECK_AF(modrm_read32(&params,1),addition); /*CHECK_OF(modrm_read32(&params,1),addition);*/
	modrm_write32(&params,2,((modrm_read32(&params,1)+addition)&0xFFFFFFFF));
	CHECK_SF(modrm_read32(&params,1));
	CHECK_ZF(modrm_read32(&params,1));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP13()
{
	if (CPU_Operand_size[activeCPU])
	{
		CPU_OP11_ID();
	}
	else
	{
		CPU_OP11_IW();
	}
}



















//ADD
void CPU_OP04()
{
	byte addition = ((imm8())&0xFF);    //ADC AL,imm8
	CHECK_AF(REG_AL,addition); /*CHECK_OF(REG_AL,addition);*/
	REG_AL = ((REG_AL + addition)&0xFF);
	CHECK_SF(REG_AL);
	CHECK_ZF(REG_AL);
	CPU[activeCPU].cycles_OP = 2;
}
//OP15 multihandler
void CPU_OP05_IW()
{
	word addition = ((imm16())&0xFFFF);    //ADC AX,imm16
	CHECK_AF(REG_AX,addition); /*CHECK_OF(REG_AL,addition);*/
	REG_AL = ((REG_AL + addition)&0xFF);
	CHECK_SF(REG_AL);
	CHECK_ZF(REG_AL);
	CPU[activeCPU].cycles_OP = 2;
}
void CPU_OP05_ID()
{
	uint_32 addition = ((imm32())&0xFFFFFFFF);    //ADC EAX,imm32
	CHECK_AF(REG_EAX,addition); /*CHECK_OF(REG_AX,addition);*/
	REG_EAX = ((REG_EAX + addition)&0xFF);
	CHECK_SF(REG_AL);
	CHECK_ZF(REG_AL);
	CPU[activeCPU].cycles_OP = 2;
}
void CPU_OP05()
{
	if (!CPU_Operand_size[activeCPU])
	{
		/* 16-bits? */ CPU_OP15_IW();
	}
	else
	{
		/* 32-bits? */ CPU_OP15_ID();
	}
}
//ADD r/m8,16,32


//slash 0 (ADD)
void CPU_OP80_ADD()
{
	modrm_readparams(&params,8,0);
	sbyte addition = (imm8()+FLAG_CF);
	CHECK_AF(modrm_read8(&params,0),addition); /*CHECK_OF(modrm_read8(&params,0),addition);*/
	modrm_write8(&params,((modrm_read8(&params,0)+addition)&0xFF),0);
	CHECK_SF(modrm_read8(&params,0));
	CHECK_ZF(modrm_read8(&params,0));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
//OP81 multihandler
void CPU_OP81_ADD_IW()
{
	modrm_readparams(&params,16,0);
	sword addition = (imm16()+FLAG_CF);
	CHECK_AF(modrm_read16(&params,0),addition); /*CHECK_OF(modrm_read16(&params,0),addition);*/
	modrm_write16(&params,2,((modrm_read16(&params,0)+addition)&0xFFFF),0);
	CHECK_SF(modrm_read16(&params,0));
	CHECK_ZF(modrm_read16(&params,0));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP81_ADD_ID()
{
	modrm_readparams(&params,32,0);
	int_32 addition = (imm32()+FLAG_CF);
	CHECK_AF(modrm_read32(&params,0),addition); /*CHECK_OF(modrm_read32(&params,0),addition);*/
	modrm_write32(&params,((modrm_read32(&params,0)+addition)&0xFFFF),0);
	CHECK_SF(modrm_read32(&params,0));
	CHECK_ZF(modrm_read32(&params,0));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP81_ADD()
{
	if (!CPU_Operand_size[activeCPU])
	{
		CPU_OP81_ADD_IW();
	}
	else
	{
		CPU_OP81_ADD_ID();
	}
}
//Same, but with imm8
void CPU_OP83_ADD_IW()
{
	modrm_readparams(&params,16,0);
	sword addition = (imm8()+FLAG_CF);
	CHECK_AF(modrm_read16(&params,0),addition); /*CHECK_OF(modrm_read16(&params,0),addition);*/
	modrm_write16(&params,2,((modrm_read16(&params,0)+addition)&0xFFFF),0);
	CHECK_SF(modrm_read16(&params,0));
	CHECK_ZF(modrm_read16(&params,0));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP83_ADD_ID()
{
	modrm_readparams(&params,32,0);
	int_32 addition = (imm8()+FLAG_CF);
	CHECK_AF(modrm_read32(&params,0),addition); /*CHECK_OF(modrm_read32(&params,0),addition);*/
	modrm_write32(&params,((modrm_read32(&params,0)+addition)&0xFFFF),0);
	CHECK_SF(modrm_read32(&params,0));
	CHECK_ZF(modrm_read32(&params,0));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP83_ADD()
{
	if (!CPU_Operand_size[activeCPU])
	{
		CPU_OP83_ADD_IW();
	}
	else
	{
		CPU_OP83_ADD_ID();
	}
}

//Final 4-set R/M normal:
void CPU_OP00()
{
	modrm_readparams(&params,8,2);
	byte addition = (modrm_read8(&params,1));
	CHECK_AF(modrm_read8(&params,2),addition); /*CHECK_OF(modrm_read8(&params,2),addition);*/
	modrm_write8(&params,((modrm_read8(&params,2)+addition)&0xFF),1);
	CHECK_SF(modrm_read8(&params,2));
	CHECK_ZF(modrm_read8(&params,2));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP01_IW()
{
	modrm_readparams(&params,16,2);
	word addition = (modrm_read16(&params,1));
	CHECK_AF(modrm_read16(&params,2),addition); /*CHECK_OF(modrm_read16(&params,2),addition);*/
	modrm_write16(&params,2,((modrm_read16(&params,2)+addition)&0xFFFF),0);
	CHECK_SF(modrm_read16(&params,2));
	CHECK_ZF(modrm_read16(&params,2));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP01_ID()
{
	modrm_readparams(&params,32,2);
	int_32 addition = (modrm_read32(&params,1));
	CHECK_AF(modrm_read32(&params,2),addition); /*CHECK_OF(modrm_read32(&params,2),addition);*/
	modrm_write32(&params,((modrm_read32(&params,2)+addition)&0xFFFFFFFF),1);
	CHECK_SF(modrm_read32(&params,2));
	CHECK_ZF(modrm_read32(&params,2));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP01()
{
	if (!CPU_Operand_size[activeCPU])
	{
		CPU_OP11_IW();
	}
	else
	{
		CPU_OP11_ID();
	}
}

//Same as 00-01, but reversed.
void CPU_OP02()
{
	modrm_readparams(&params,8,1);
	byte addition = (modrm_read8(&params,1));
	CHECK_AF(modrm_read8(&params,1),addition); /*CHECK_OF(modrm_read8(&params,1),addition);*/
	modrm_write8(&params,((modrm_read8(&params,1)+addition)&0xFF),2);
	CHECK_SF(modrm_read8(&params,1));
	CHECK_ZF(modrm_read8(&params,1));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP03_IW()
{
	modrm_readparams(&params,16,1);
	word addition = (modrm_read16(&params,1));
	CHECK_AF(modrm_read16(&params,1),addition); /*CHECK_OF(modrm_read16(&params,1),addition);*/
	modrm_write16(&params,2,((modrm_read16(&params,1)+addition)&0xFFFF),0);
	CHECK_SF(modrm_read16(&params,1));
	CHECK_ZF(modrm_read16(&params,1));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP03_ID()
{
	modrm_readparams(&params,32,1);
	int_32 addition = (modrm_read32(&params,1));
	CHECK_AF(modrm_read32(&params,1),addition); /*CHECK_OF(modrm_read32(&params,1),addition);*/
	modrm_write32(&params,((modrm_read32(&params,1)+addition)&0xFFFFFFFF),2);
	CHECK_SF(modrm_read32(&params,1));
	CHECK_ZF(modrm_read32(&params,1));
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP03()
{
	if (!CPU_Operand_size[activeCPU])
	{
		CPU_OP11_IW();
	}
	else
	{
		CPU_OP11_ID();
	}
}












//AND
void CPU_OP24()
{
	byte addition = ((imm8())&0xFF);    //ADC AL,imm8
	CHECK_AF(REG_AL,addition); /*CHECK_OF(REG_AL,addition);*/
	REG_AL = ((REG_AL + addition)&0xFF);
	CHECK_SF(REG_AL);
	CHECK_ZF(REG_AL);
	FLAG_CF = 0;
	FLAG_OF = 0;
	CPU[activeCPU].cycles_OP = 2;
}
//OP15 multihandler
void CPU_OP25_IW()
{
	word addition = ((imm16())&0xFFFF);    //ADC AX,imm16
	CHECK_AF(REG_AX,addition); /*CHECK_OF(REG_AL,addition);*/
	REG_AL = ((REG_AL + addition)&0xFF);
	CHECK_SF(REG_AL);
	CHECK_ZF(REG_AL);
	FLAG_CF = 0;
	FLAG_OF = 0;
	CPU[activeCPU].cycles_OP = 2;
}
void CPU_OP25_ID()
{
	uint_32 addition = ((imm32())&0xFFFFFFFF);    //ADC EAX,imm32
	CHECK_AF(REG_EAX,addition); /*CHECK_OF(REG_AX,addition);*/
	REG_EAX = ((REG_EAX + addition)&0xFF);
	CHECK_SF(REG_AL);
	CHECK_ZF(REG_AL);
	FLAG_CF = 0;
	FLAG_OF = 0;
	CPU[activeCPU].cycles_OP = 2;
}
void CPU_OP25()
{
	if (!CPU_Operand_size[activeCPU])
	{
		/* 16-bits? */ CPU_OP15_IW();
	}
	else
	{
		/* 32-bits? */ CPU_OP15_ID();
	}
}
//ADD r/m8,16,32


//slash 4 (AND)
void CPU_OP80_AND()
{
	modrm_readparams(&params,8,0);
	sbyte addition = (imm8()+FLAG_CF);
	CHECK_AF(modrm_read8(&params,0),addition); /*CHECK_OF(modrm_read8(&params,0),addition);*/
	modrm_write8(&params,((modrm_read8(&params,0)+addition)&0xFF),0);
	CHECK_SF(modrm_read8(&params,0));
	CHECK_ZF(modrm_read8(&params,0));
	FLAG_CF = 0;
	FLAG_OF = 0;
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
//OP81 multihandler
void CPU_OP81_AND_IW()
{
	modrm_readparams(&params,16,0);
	sword addition = (imm16()+FLAG_CF);
	CHECK_AF(modrm_read16(&params,0),addition); /*CHECK_OF(modrm_read16(&params,0),addition);*/
	modrm_write16(&params,2,((modrm_read16(&params,0)+addition)&0xFFFF),0);
	CHECK_SF(modrm_read16(&params,0));
	CHECK_ZF(modrm_read16(&params,0));
	FLAG_CF = 0;
	FLAG_OF = 0;
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP81_AND_ID()
{
	modrm_readparams(&params,32,0);
	int_32 addition = (imm32()+FLAG_CF);
	CHECK_AF(modrm_read32(&params,0),addition); /*CHECK_OF(modrm_read32(&params,0),addition);*/
	modrm_write32(&params,((modrm_read32(&params,0)+addition)&0xFFFF),0);
	CHECK_SF(modrm_read32(&params,0));
	CHECK_ZF(modrm_read32(&params,0));
	FLAG_CF = 0;
	FLAG_OF = 0;
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP81_AND()
{
	if (!CPU_Operand_size[activeCPU])
	{
		CPU_OP81_AND_IW();
	}
	else
	{
		CPU_OP81_AND_ID();
	}
}
//Same, but with imm8
void CPU_OP83_AND_IW()
{
	modrm_readparams(&params,16,0);
	sword addition = (imm8()+FLAG_CF);
	CHECK_AF(modrm_read16(&params,0),addition); /*CHECK_OF(modrm_read16(&params,0),addition);*/
	modrm_write16(&params,2,((modrm_read16(&params,0)+addition)&0xFFFF),0);
	CHECK_SF(modrm_read16(&params,0));
	CHECK_ZF(modrm_read16(&params,0));
	FLAG_CF = 0;
	FLAG_OF = 0;
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP83_AND_ID()
{
	modrm_readparams(&params,32,0);
	int_32 addition = (imm8()+FLAG_CF);
	CHECK_AF(modrm_read32(&params,0),addition); /*CHECK_OF(modrm_read32(&params,0),addition);*/
	modrm_write32(&params,((modrm_read32(&params,0)+addition)&0xFFFF),0);
	CHECK_SF(modrm_read32(&params,0));
	CHECK_ZF(modrm_read32(&params,0));
	FLAG_CF = 0;
	FLAG_OF = 0;
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP83_AND()
{
	if (!CPU_Operand_size[activeCPU])
	{
		CPU_OP83_AND_IW();
	}
	else
	{
		CPU_OP83_AND_ID();
	}
}

//Final 4-set R/M normal:
void CPU_OP20()
{
	modrm_readparams(&params,8,2);
	byte addition = (modrm_read8(&params,1));
	CHECK_AF(modrm_read8(&params,2),addition); /*CHECK_OF(modrm_read8(&params,2),addition);*/
	modrm_write8(&params,((modrm_read8(&params,2)+addition)&0xFF),1);
	CHECK_SF(modrm_read8(&params,2));
	CHECK_ZF(modrm_read8(&params,2));
	FLAG_CF = 0;
	FLAG_OF = 0;
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP21_IW()
{
	modrm_readparams(&params,16,2);
	word addition = (modrm_read16(&params,1));
	CHECK_AF(modrm_read16(&params,2),addition); /*CHECK_OF(modrm_read16(&params,2),addition);*/
	modrm_write16(&params,2,((modrm_read16(&params,2)+addition)&0xFFFF),0);
	CHECK_SF(modrm_read16(&params,2));
	CHECK_ZF(modrm_read16(&params,2));
	FLAG_CF = 0;
	FLAG_OF = 0;
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP21_ID()
{
	modrm_readparams(&params,32,2);
	int_32 addition = (modrm_read32(&params,1));
	CHECK_AF(modrm_read32(&params,2),addition); /*CHECK_OF(modrm_read32(&params,2),addition);*/
	modrm_write32(&params,((modrm_read32(&params,2)+addition)&0xFFFFFFFF),1);
	CHECK_SF(modrm_read32(&params,2));
	CHECK_ZF(modrm_read32(&params,2));
	FLAG_CF = 0;
	FLAG_OF = 0;
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP21()
{
	if (!CPU_Operand_size[activeCPU])
	{
		CPU_OP21_IW();
	}
	else
	{
		CPU_OP21_ID();
	}
}

//Same as 20-21, but reversed.
void CPU_OP22()
{
	modrm_readparams(&params,8,1);
	byte addition = (modrm_read8(&params,1));
	CHECK_AF(modrm_read8(&params,1),addition); /*CHECK_OF(modrm_read8(&params,1),addition);*/
	modrm_write8(&params,((modrm_read8(&params,1)+addition)&0xFF),2);
	CHECK_SF(modrm_read8(&params,1));
	CHECK_ZF(modrm_read8(&params,1));
	FLAG_CF = 0;
	FLAG_OF = 0;
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP23_IW()
{
	modrm_readparams(&params,16,1);
	word addition = (modrm_read16(&params,1));
	CHECK_AF(modrm_read16(&params,1),addition); /*CHECK_OF(modrm_read16(&params,1),addition);*/
	modrm_write16(&params,2,((modrm_read16(&params,1)+addition)&0xFFFF),0);
	CHECK_SF(modrm_read16(&params,1));
	CHECK_ZF(modrm_read16(&params,1));
	FLAG_CF = 0;
	FLAG_OF = 0;
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP23_ID()
{
	modrm_readparams(&params,32,1);
	int_32 addition = (modrm_read32(&params,1));
	CHECK_AF(modrm_read32(&params,1),addition); /*CHECK_OF(modrm_read32(&params,1),addition);*/
	modrm_write32(&params,((modrm_read32(&params,1)+addition)&0xFFFFFFFF),2);
	CHECK_SF(modrm_read32(&params,1));
	CHECK_ZF(modrm_read32(&params,1));
	FLAG_CF = 0;
	FLAG_OF = 0;
	CPU[activeCPU].cycles_OP = modrm_isregister(params)?2:7; /* 2 for register, 7 for memory */
}
void CPU_OP23()
{
	if (!CPU_Operand_size[activeCPU])
	{
		CPU_OP21_IW();
	}
	else
	{
		CPU_OP21_ID();
	}
}



//25 OPCodes implemented here (counting multi-op Opcodes as only one opcode in total (80, 81, 83))



//ARPL (ADjust RPL Field of Selector)

//void CPU_OP63() { /* STILL TO IMPLEMENT */ }





















//SuperOPcodes, (see Special modr/m opcode differences page):

void CPU_OP80()
{
	byte commandinfo = CPU_readOP();
	REG_EIP--; //Read modr/m base info and return for function!
	switch (((commandinfo&0x38)>>3)) //What function?
	{
	case 0:
		CPU_OP80_ADD();
		break; //ADD
	case 1:
		break; //OR
	case 2:
		CPU_OP80_ADC();
		break; //ADC
	case 3:
		break; //SBB
	case 4:
		CPU_OP80_AND();
		break; //AND
	case 5:
		break; //SUB
	case 6:
		break; //XOR
	case 7:
		break;//CMP
	}
}
void CPU_OP81()
{
	byte commandinfo = CPU_readOP();
	REG_EIP--; //Read modr/m base info and return for function!
	switch (((commandinfo&0x38)>>3)) //What function?
	{
	case 0:
		CPU_OP81_ADD();
		break; //ADD
	case 1:
		break; //OR
	case 2:
		CPU_OP81_ADC();
		break; //ADC
	case 3:
		break; //SBB
	case 4:
		CPU_OP81_AND();
		break; //AND
	case 5:
		break; //SUB
	case 6:
		break; //XOR
	case 7:
		break;//CMP
	}
}
void CPU_OP82()
{
	byte commandinfo = CPU_readOP();
	REG_EIP--; //Read modr/m base info and return for function!
	switch (((commandinfo&0x38)>>3)) //What function?
	{
	case 0:
		break; //ADD
	case 1:
		break; //OR
	case 2:
		break; //ADC
	case 3:
		break; //SBB
	case 4:
		break; //AND
	case 5:
		break; //SUB
	case 6:
		break; //XOR
	case 7:
		break;//CMP
	}
}
void CPU_OP83()
{
	byte commandinfo = CPU_readOP();
	REG_EIP--; //Read modr/m base info and return for function!
	switch (((commandinfo&0x38)>>3)) //What function?
	{
	case 0:
		CPU_OP83_ADD();
		break; //ADD
	case 1:
		break; //OR
	case 2:
		CPU_OP83_ADC();
		break; //ADC
	case 3:
		break; //SBB
	case 4:
		CPU_OP83_AND();
		break; //AND
	case 5:
		break; //SUB
	case 6:
		break; //XOR
	case 7:
		break;//CMP
	}
}
void CPU_OP8F()
{
}
void CPU_OPC0()
{
}
void CPU_OPC1()
{
}
void CPU_OPD0()
{
}
void CPU_OPD1()
{
}
void CPU_OPD2()
{
}
void CPU_OPD3()
{
}
void CPU_OPF6()
{
}
void CPU_OPF7()
{
}
void CPU_OPFE()
{
}
void CPU_OPFF()
{
}
