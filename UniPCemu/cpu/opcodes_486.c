#include "headers/cpu/cpu.h"
#include "headers/cpu/easyregs.h"
#include "headers/cpu/cpu_OP80286.h" //80286 opcodes!
#include "headers/cpu/cpu_OP80386.h" //80386 opcodes!
#include "headers/cpu/modrm.h" //ModR/M support!
#include "headers/cpu/protection.h" //Protection fault support!
#include "headers/cpu/paging.h" //Paging support for clearing TLB!
#include "headers/cpu/flags.h" //Flags support for adding!
#include "headers/emu/debugger/debugger.h" //Debugger support!

extern MODRM_PARAMS params; //For getting all params for the CPU!
extern byte cpudebugger; //The debugging is on?
extern byte blockREP; //Block the instruction from executing (REP with (E)CX=0

//When using http://www.mlsite.net/8086/: G=Modr/m mod&r/m adress, E=Reg field in modr/m

//INFO: http://www.mlsite.net/8086/
//Extra info about above: Extension opcodes (GRP1 etc) are contained in the modr/m
//Ammount of instructions in the completed core: 123

//Aftercount: 60-6F,C0-C1, C8-C9, D6, D8-DF, F1, 0F(has been implemented anyways)
//Total count: 30 opcodes undefined.

//Info: Ap = 32-bit segment:offset pointer (data: param 1:word segment, param 2:word offset)

//Simplifier!

extern uint_32 destEIP; //Destination address for CS JMP instruction!

extern byte immb; //For CPU_readOP result!
extern word immw; //For CPU_readOPw result!
extern uint_32 imm32; //For CPU_readOPdw result!
extern uint_64 imm64; //For CPU_readOPdw result!
extern byte oper1b, oper2b; //Byte variants!
extern word oper1, oper2; //Word variants!
extern uint_32 oper1d, oper2d; //DWord variants!
extern byte res8; //Result 8-bit!
extern word res16; //Result 16-bit!
extern uint_32 res32; //Result 32-bit!
extern byte thereg; //For function number!
extern byte tempCF2;

extern VAL64Splitter temp1, temp2, temp3, temp4, temp5; //All temporary values!
extern uint_32 temp32, tempaddr32; //Defined in opcodes_8086.c

extern byte debuggerINT; //Interrupt special trigger?

extern uint_32 immaddr32; //Immediate address, for instructions requiring it, either 16-bits or 32-bits of immediate data, depending on the address size!

//Modr/m support, used when reg=NULL and custommem==0
extern byte MODRM_src0; //What destination operand in our modr/m? (1/2)
extern byte MODRM_src1; //What source operand in our modr/m? (2/2)

void CPU486_CPUID()
{
	switch (REG_EAX)
	{
	case 0x00: //Highest function parameter!
		REG_EAX = 1; //One function parameters supported!
		//GenuineIntel!
		REG_EBX = 0x756e6547;
		REG_EDX = 0x49656e69;
		REG_ECX = 0x6c65746e;
		break;
	case 0x01: //Standard level 1: Processor type/family/model/stepping and Feature flags
		//Information based on http://www.hugi.scene.org/online/coding/hugi%2016%20-%20corawhd4.htm
		REG_EAX = (0<<0xC); //Type: 00b=Primary processor
		REG_EAX |= (4<<8); //Family: 80486/AMD 5x86/Cyrix 5x86
		REG_EAX |= (2<<4); //Model: i80486SX
		REG_EAX |= (0<<0); //Processor stepping: unknown with 80486SX!
		REG_EDX = 0; //No extended functionality has been implemented!
	default:
		break;
	}
}

void CPU486_OP0F01_32()
{
	uint_32 linearaddr;
	if (thereg==7) //INVLPG?
	{
		modrm_generateInstructionTEXT("INVLPG",16,0,PARAM_MODRM_1);
		if (getcpumode()!=CPU_MODE_REAL) //Protected mode?
		{
			if (getCPL())
			{
				THROWDESCGP(0,0,0);
				return;
			}
		}
		linearaddr = MMU_realaddr(params.info[MODRM_src0].segmentregister_index,*params.info[MODRM_src0].segmentregister,params.info[MODRM_src0].mem_offset,0,params.info[MODRM_src0].is16bit); //Linear address!
		Paging_Invalidate(linearaddr); //Invalidate the address that's used!
	}
	else
	{
		CPU386_OP0F01(); //Fallback to 80386 instructions!
	}
}

void CPU486_OP0F01_16()
{
	uint_32 linearaddr;
	if (thereg==7) //INVLPG?
	{
		modrm_generateInstructionTEXT("INVLPG",32,0,PARAM_MODRM2);
		if (getcpumode()!=CPU_MODE_REAL) //Protected mode?
		{
			if (getCPL())
			{
				THROWDESCGP(0,0,0);
				return;
			}
		}
		linearaddr = MMU_realaddr(params.info[MODRM_src0].segmentregister_index,*params.info[MODRM_src0].segmentregister,params.info[MODRM_src0].mem_offset,0,params.info[MODRM_src0].is16bit); //Linear address!
		Paging_Invalidate(linearaddr); //Invalidate the address that's used!
	}
	else
	{
		CPU286_OP0F01(); //Fallback to 80386 instructions!
	}
}

void CPU486_OP0F08() //INVD?
{
	modrm_generateInstructionTEXT("INVD",0,0,PARAM_NONE);
	if (getcpumode()!=CPU_MODE_REAL) //Protected mode?
	{
		if (getCPL())
		{
			THROWDESCGP(0,0,0);
			return;
		}
	}
}

void CPU486_OP0F09() //WBINVD?
{
	modrm_generateInstructionTEXT("INVD",0,0,PARAM_NONE);
	if (getcpumode()!=CPU_MODE_REAL) //Protected mode?
	{
		if (getCPL())
		{
			THROWDESCGP(0,0,0);
			return;
		}
	}
}

void CPU486_OP0FB0() {byte temp; if (modrm_check8(&params,MODRM_src0,1)) return; temp = modrm_read8(&params,MODRM_src0); if (REG_AL==temp) { if (modrm_check8(&params,MODRM_src0,0)) return; FLAGW_ZF(1); modrm_write8(&params,MODRM_src0,modrm_read8(&params,MODRM_src1)); /* r/m8=r8 */ } else { FLAGW_ZF(0); REG_AL = temp; /* AL=r/m8 */ }} //CMPXCHG r/m8,AL,r8
void CPU486_OP0FB1_16() {word temp; if (modrm_check16(&params,MODRM_src0,1|0x40)) return; if (modrm_check16(&params,MODRM_src0,1|0xA0)) return; temp = modrm_read16(&params,MODRM_src0); if (REG_AX==temp) { if (modrm_check16(&params,MODRM_src0,0|0x40)) return; if (modrm_check16(&params,MODRM_src0,0|0xA0)) return; FLAGW_ZF(1); modrm_write16(&params,MODRM_src0,modrm_read16(&params,MODRM_src1),0); /* r/m16=r16 */ } else { FLAGW_ZF(0); REG_AX = temp; /* AX=r/m16 */ }} //CMPXCHG r/m16,AX,r16
void CPU486_OP0FB1_32() {uint_32 temp; if (modrm_check32(&params,MODRM_src0,1|0x40)) return; if (modrm_check32(&params,MODRM_src0,1|0xA0)) return; temp = modrm_read32(&params,MODRM_src0); if (REG_EAX==temp) { if (modrm_check32(&params,MODRM_src0,0|0x40)) return; if (modrm_check32(&params,MODRM_src0,0|0xA0)) return; FLAGW_ZF(1); modrm_write32(&params,MODRM_src0,modrm_read32(&params,MODRM_src1)); /* r/m32=r32 */ } else { FLAGW_ZF(0); REG_EAX = temp; /* EAX=r/m32 */ }} //CMPXCHG r/m32,EAX,r32

OPTINLINE void op_add8_486() {
	res8 = oper1b + oper2b;
	flag_add8 (oper1b, oper2b);
}

OPTINLINE void op_add16_486() {
	res16 = oper1 + oper2;
	flag_add16 (oper1, oper2);
}

OPTINLINE void op_add32_486() {
	res32 = oper1d + oper2d;
	flag_add32 (oper1d, oper2d);
}

void CPU486_OP0FC0() {modrm_generateInstructionTEXT("XADD",8,0,PARAM_MODRM21); if (modrm_check8(&params,MODRM_src0,0)) return; oper1b = modrm_read8(&params,MODRM_src1); oper2b = modrm_read8(&params,MODRM_src0); op_add8_486(); modrm_write8(&params,MODRM_src1,oper2b); modrm_write8(&params,MODRM_src0,res8);} //XADD r/m8,r8
void CPU486_OP0FC1_16() {modrm_generateInstructionTEXT("XADD",16,0,PARAM_MODRM21); if (modrm_check16(&params,MODRM_src0,0|0x40)) return; if (modrm_check16(&params,MODRM_src0,0|0xA0)) return; oper1 = modrm_read16(&params,MODRM_src1); oper2 = modrm_read16(&params,MODRM_src0); op_add16_486(); modrm_write16(&params,MODRM_src1,oper2,0); modrm_write16(&params,MODRM_src0,res16,0);} //XADD r/m16,r16
void CPU486_OP0FC1_32() {modrm_generateInstructionTEXT("XADD",32,0,PARAM_MODRM21); if (modrm_check32(&params,MODRM_src0,0|0x40)) return; if (modrm_check32(&params,MODRM_src0,0|0xA0)) return; oper1d = modrm_read32(&params,MODRM_src1); oper2d = modrm_read32(&params,MODRM_src0); op_add32_486(); modrm_write32(&params,MODRM_src1,oper2d); modrm_write32(&params,MODRM_src0,res32);} //XADD r/m32,r32

//BSWAP on 16-bit registers is undefined!
void CPU486_BSWAP16(word *reg)
{
	/* Swap endianness on a register(Big->Little or Little->Big) */
	INLINEREGISTER word buf;
	buf = *reg; //Read to start!
	buf = (((buf>>8)&0xFF)|((buf<<8)&0xFF00)); //Swap bytes to finish!
	*reg = buf; //Save the result!
}

void CPU486_BSWAP32(uint_32 *reg)
{
	/* Swap endianness on a register(Big->Little or Little->Big) */
	INLINEREGISTER uint_32 buf;
	buf = *reg; //Read to start!
	buf = ((buf>>16)|(buf<<16)); //Swap words!
	buf = (((buf>>8)&0xFF00FF)|((buf<<8)&0xFF00FF00)); //Swap bytes to finish!
	*reg = buf; //Save the result!
}

void CPU486_OP0FC8_16() {debugger_setcommand("BSWAP AX"); CPU486_BSWAP16(&REG_AX);} //BSWAP AX
void CPU486_OP0FC8_32() {debugger_setcommand("BSWAP EAX"); CPU486_BSWAP32(&REG_EAX);} //BSWAP EAX
void CPU486_OP0FC9_16() {debugger_setcommand("BSWAP CX"); CPU486_BSWAP16(&REG_CX);} //BSWAP CX
void CPU486_OP0FC9_32() {debugger_setcommand("BSWAP ECX"); CPU486_BSWAP32(&REG_ECX);} //BSWAP ECX
void CPU486_OP0FCA_16() {debugger_setcommand("BSWAP DX"); CPU486_BSWAP16(&REG_DX);} //BSWAP DX
void CPU486_OP0FCA_32() {debugger_setcommand("BSWAP EDX"); CPU486_BSWAP32(&REG_EDX);} //BSWAP EDX
void CPU486_OP0FCB_16() {debugger_setcommand("BSWAP BX"); CPU486_BSWAP16(&REG_BX);} //BSWAP BX
void CPU486_OP0FCB_32() {debugger_setcommand("BSWAP EBX"); CPU486_BSWAP32(&REG_EBX);} //BSWAP EBX
void CPU486_OP0FCC_16() {debugger_setcommand("BSWAP SP"); CPU486_BSWAP16(&REG_SP);} //BSWAP SP
void CPU486_OP0FCC_32() {debugger_setcommand("BSWAP ESP"); CPU486_BSWAP32(&REG_ESP);} //BSWAP ESP
void CPU486_OP0FCD_16() {debugger_setcommand("BSWAP BP"); CPU486_BSWAP16(&REG_BP);} //BSWAP BP
void CPU486_OP0FCD_32() {debugger_setcommand("BSWAP EBP"); CPU486_BSWAP32(&REG_EBP);} //BSWAP EBP
void CPU486_OP0FCE_16() {debugger_setcommand("BSWAP SI"); CPU486_BSWAP16(&REG_SI);} //BSWAP SI
void CPU486_OP0FCE_32() {debugger_setcommand("BSWAP ESI"); CPU486_BSWAP32(&REG_ESI);} //BSWAP ESI
void CPU486_OP0FCF_16() {debugger_setcommand("BSWAP DI"); CPU486_BSWAP16(&REG_DI);} //BSWAP DI
void CPU486_OP0FCF_32() {debugger_setcommand("BSWAP EDI"); CPU486_BSWAP32(&REG_EDI);} //BSWAP EDI
