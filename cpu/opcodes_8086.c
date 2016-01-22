#include "headers/types.h" //Basic types
#include "headers/cpu/cpu.h" //CPU needed!
#include "headers/mmu/mmu.h" //MMU needed!
#include "headers/cpu/easyregs.h" //Easy register compatibility!
#include "headers/cpu/modrm.h" //MODR/M compatibility!
#include "headers/support/signedness.h" //CPU support functions!
#include "headers/hardware/ports.h" //Ports compatibility!
#include "headers/cpu/cpu_OP8086.h" //Our own opcode presets!
#include "headers/cpu/fpu_OP8087.h" //Our own opcode presets!
#include "headers/cpu/cb_manager.h" //For OPFE!
#include "headers/cpu/flags.h" //Flag support!
#include "headers/cpu/8086_grpOPs.h" //GRP Opcode extensions!
#include "headers/cpu/interrupts.h" //Basic interrupt support!
#include "headers/emu/debugger/debugger.h" //CPU debugger support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/cpu/protection.h"

MODRM_PARAMS params; //For getting all params for the CPU!
extern byte cpudebugger; //The debugging is on?
extern byte blockREP; //Block the instruction from executing (REP with (E)CX=0

//When using http://www.mlsite.net/8086/: G=Modr/m mod&r/m adress, E=Reg field in modr/m

//INFO: http://www.mlsite.net/8086/
//Extra info about above: Extension opcodes (GRP1 etc) are contained in the modr/m
//Ammount of instructions in the completed core: 123

//Aftercount: 60-6F,C0-C1, C8-C9, D6, D8-FLAG_DF, F1, 0F(has been implemented anyways)
//Total count: 30 opcodes undefined.

//Info: Ap = 32-bit segment:offset pointer (data: param 1:word segment, param 2:word offset)

//Simplifier!

extern uint_32 destEIP; //Destination address for CS JMP instruction!

byte immb; //For CPU_readOP result!
word immw; //For CPU_readOPw result!
byte oper1b, oper2b; //Byte variants!
word oper1, oper2; //Word variants!
byte res8; //Result 8-bit!
word res16; //Result 16-bit!
byte reg; //For function number!
uint_32 ea; //From RM OFfset (GRP5 Opcodes only!)
byte tempCF;

VAL32Splitter temp1, temp2, temp3, temp4, temp5; //All temporary values!
uint_32 temp32, tempaddr32; //Defined in opcodes_8086.c


/*

Start of help for debugging

*/

char modrm_param1[256]; //Contains param/reg1
char modrm_param2[256]; //Contains param/reg2

void modrm_debugger8(MODRM_PARAMS *params, byte whichregister1, byte whichregister2) //8-bit handler!
{
	if (cpudebugger)
	{
		bzero(modrm_param1,sizeof(modrm_param1));
		bzero(modrm_param2,sizeof(modrm_param2));
		modrm_text8(params,whichregister1,&modrm_param1[0]);
		modrm_text8(params,whichregister2,&modrm_param2[0]);
	}
}

void modrm_debugger16(MODRM_PARAMS *params, byte whichregister1, byte whichregister2) //16-bit handler!
{
	if (cpudebugger)
	{
		bzero(modrm_param1,sizeof(modrm_param1));
		bzero(modrm_param2,sizeof(modrm_param2));
		modrm_text16(params,whichregister1,&modrm_param1[0]);
		modrm_text16(params,whichregister2,&modrm_param2[0]);
	}
}

/*

modrm_generateInstructionTEXT: Generates text for an instruction into the debugger.
parameters:
	instruction: The instruction ("ADD","INT",etc.)
	debuggersize: Size of the debugger, if any (8/16/32/0 for none).
	paramdata: The params to use when debuggersize set and using modr/m with correct type.
	type: See above.

*/

OPTINLINE void modrm_generateInstructionTEXT(char *instruction, byte debuggersize, uint_32 paramdata, byte type)
{
	if (cpudebugger) //Gotten no debugger to process?
	{
		//Process debugger!
		char result[256];
		bzero(result,sizeof(result));
		strcpy(result,instruction); //Set the instruction!
		switch (type)
		{
			case PARAM_MODRM1: //Param1 only?
			case PARAM_MODRM2: //Param2 only?
			case PARAM_MODRM12: //param1,param2
			case PARAM_MODRM21: //param2,param1
				//We use modr/m decoding!
				switch (debuggersize)
				{
					case 8:
						modrm_debugger8(&params,0,1);
						break;
					case 16:
						modrm_debugger16(&params,0,1);
						break;
					default: //None?
						//Don't use modr/m!
						break;
				}
				break;
		}
		switch (type)
		{
			case PARAM_NONE: //No params?
				debugger_setcommand(result); //Nothing!
				break;
			case PARAM_MODRM1: //Param1 only?
				strcat(result," %s"); //1 param!
				debugger_setcommand(result,modrm_param1);
				break;
			case PARAM_MODRM2: //Param2 only?
				strcat(result," %s"); //1 param!
				debugger_setcommand(result,modrm_param2);
				break;
			case PARAM_MODRM12: //param1,param2
				strcat(result," %s,%s"); //2 params!
				debugger_setcommand(result,modrm_param1,modrm_param2);
				break;
			case PARAM_MODRM21: //param2,param1
				strcat(result," %s,%s"); //2 params!
				debugger_setcommand(result,modrm_param2,modrm_param1);
				break;
			case PARAM_IMM8: //imm8
				strcat(result," %02X"); //1 param!
				debugger_setcommand(result,paramdata);
				break;
			case PARAM_IMM16: //imm16
				strcat(result," %04X"); //1 param!
				debugger_setcommand(result,paramdata);
				break;
			case PARAM_IMM32: //imm32
				strcat(result," %08X"); //1 param!
				debugger_setcommand(result,paramdata);
			default: //Unknown?
				break;
		}
	}
}

char LEAtext[256];
OPTINLINE char *getLEAtext(MODRM_PARAMS *params)
{
	modrm_lea16_text(params,1,&LEAtext[0]);    //Help function for LEA instruction!
	return &LEAtext[0];
}

/*

Start of help for opcode processing

*/

OPTINLINE void CPU8086_hardware_int(byte interrupt, byte has_errorcode, uint_32 errorcode) //See int, but for hardware interrupts (IRQs)!
{
	CPU_INT(interrupt); //Save adress to stack (We're going soft int!)!
	if (has_errorcode) //Have error code too?
	{
		CPU_PUSH32(&errorcode); //Push error code on stack!
	}
}

OPTINLINE void CPU8086_int(byte interrupt) //Software interrupt from us(internal call)!
{
	CPUPROT1
		CPU8086_hardware_int(interrupt, 0, 0);
	CPUPROT2
}

void CPU086_int(byte interrupt) //Software interrupt (external call)!
{
	CPU8086_int(interrupt); //Direct call!
}

OPTINLINE void CPU8086_IRET()
{
	CPUPROT1
	CPU_IRET(); //IRET!
	CPUPROT2
}

/*

List of hardware interrupts:
0: Division by 0: Attempting to execute DIV/IDIV with divisor==0: IMPLEMENTED
1: Debug/Single step: Breakpoint hit, also after instruction when TRAP flag is set.
3: Breakpoint: INT 3 call: IMPLEMENTED
4: Overflow: When performing arithmetic instructions with signed operands. Called with INTO.
5: Bounds Check: BOUND instruction exceeds limit.
6: Invalid OPCode: Invalid LOCK prefix or invalid OPCode: IMPLEMENTED
7: Device not available: Attempt to use floating point instruction (8087) with no COProcessor.
8: Double fault: Interrupt occurs with no entry in IVT or exception within exception handler.
12: Stack exception: Stack operation exceeds offset FFFFh or a selector pointing to a non-present segment is loaded into SS.
13: CS,DS,ES,FS,GS Segment Overrun: Word memory access at offset FFFFh or an attempt to execute past the end of the code segment.
16: Floating point error: An error with the numeric coprocessor (Divide-by-Zero, Underflow, Overflow...)

*/


//5 Override prefixes! (LOCK, CS, SS, DS, ES)

//Prefix opcodes:
/*
void CPU8086_OPF0() {} //LOCK
void CPU8086_OP2E() {} //CS:
void CPU8086_OP36() {} //SS:
void CPU8086_OP3E() {} //DS:
void CPU8086_OP26() {} //ES:
void CPU8086_OPF2() {} //REPNZ
void CPU8086_OPF3() {} //REPZ
*/

/*

WE START WITH ALL HELP FUNCTIONS

*/

//First CMP instruction (for debugging) and directly related.

//CMP: Substract and set flags according (Z,S,O,C); Help functions

OPTINLINE void op_adc8() {
	res8 = oper1b + oper2b + FLAG_CF;
	flag_adc8 (oper1b, oper2b, FLAG_CF);
}

OPTINLINE void op_adc16() {
	res16 = oper1 + oper2 + FLAG_CF;
	flag_adc16 (oper1, oper2, FLAG_CF);
}

OPTINLINE void op_add8() {
	res8 = oper1b + oper2b;
	flag_add8 (oper1b, oper2b);
}

OPTINLINE void op_add16() {
	res16 = oper1 + oper2;
	flag_add16 (oper1, oper2);
}

OPTINLINE void op_and8() {
	res8 = oper1b & oper2b;
	flag_log8 (res8);
}

OPTINLINE void op_and16() {
	res16 = oper1 & oper2;
	flag_log16 (res16);
}

OPTINLINE void op_or8() {
	res8 = oper1b | oper2b;
	flag_log8 (res8);
}

OPTINLINE void op_or16() {
	res16 = oper1 | oper2;
	flag_log16 (res16);
}

OPTINLINE void op_xor8() {
	res8 = oper1b ^ oper2b;
	flag_log8 (res8);
}

OPTINLINE void op_xor16() {
	res16 = oper1 ^ oper2;
	flag_log16 (res16);
}

OPTINLINE void op_sub8() {
	res8 = oper1b - oper2b;
	flag_sub8 (oper1b, oper2b);
}

OPTINLINE void op_sub16() {
	res16 = oper1 - oper2;
	flag_sub16 (oper1, oper2);
}

OPTINLINE void op_sbb8() {
	res8 = oper1b - (oper2b + FLAG_CF);
	flag_sbb8 (oper1b, oper2b, FLAG_CF);
}

OPTINLINE void op_sbb16() {
	res16 = oper1 - (oper2 + FLAG_CF);
	flag_sbb16 (oper1, oper2, FLAG_CF);
}

OPTINLINE void CMP_w(word a, word b) //Compare instruction!
{
	CPUPROT1
	flag_sub16(a,b); //Flags only!
	CPUPROT2
}

OPTINLINE void CMP_b(byte a, byte b)
{
	CPUPROT1
	flag_sub8(a,b); //Flags only!
	CPUPROT2
}

//Modr/m support, used when reg=NULL and custommem==0
byte MODRM_src0 = 0; //What source is our modr/m? (1/2)
byte MODRM_src1 = 0; //What source is our modr/m? (1/2)

//Custom memory support!
byte custommem = 0; //Used in some instructions!
uint_32 offset; //Offset to use!

//Help functions:
OPTINLINE void CPU8086_internal_INC16(word *reg)
{
	if (MMU_invaddr() || (reg==NULL))
	{
		return;
	}
	CPUPROT1
	byte tempcf = FLAG_CF;
	oper1 = reg?*reg:modrm_read16(&params,MODRM_src0);
	oper2 = 1;
	op_add16();
	FLAG_CF = tempcf;
	if (reg) //Register?
	{
		*reg = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_DEC16(word *reg)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	byte tempcf = FLAG_CF;
	oper1 = reg?*reg:modrm_read16(&params,MODRM_src0);
	oper2 = 1;
	op_sub16();
	FLAG_CF = tempcf;
	if (reg) //Register?
	{
		*reg = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	CPUPROT2
}

OPTINLINE void CPU8086_internal_INC8(byte *reg)
{
	if (MMU_invaddr() || (reg==NULL))
	{
		return;
	}
	CPUPROT1
	oper1b = reg?*reg:modrm_read16(&params,MODRM_src0);
	oper2b = 1;
	op_add8();
	if (reg) //Register?
	{
		*reg = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_DEC8(byte *reg)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	oper1b = reg?*reg:modrm_read16(&params,MODRM_src0);
	oper2b = 1;
	op_sub8();
	if (reg) //Register?
	{
		*reg = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	CPUPROT2
}

//For ADD
OPTINLINE void CPU8086_internal_ADD8(byte *dest, byte addition)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	oper1b = dest?*dest:modrm_read8(&params,MODRM_src0);
	oper2b = addition;
	op_add8();
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_ADD16(word *dest, word addition)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	oper1 = dest?*dest:modrm_read16(&params,MODRM_src0);
	oper2 = addition;
	op_add16();
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	CPUPROT2
}

//For ADC
OPTINLINE void CPU8086_internal_ADC8(byte *dest, byte addition)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	oper1b = dest?*dest:modrm_read8(&params,MODRM_src0);
	oper2b = addition;
	op_adc8();
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_ADC16(word *dest, word addition)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	oper1 = dest?*dest:modrm_read16(&params,MODRM_src0);
	oper2 = addition;
	op_adc16();
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	CPUPROT2
}


//For OR
OPTINLINE void CPU8086_internal_OR8(byte *dest, byte src)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	oper1b = dest?*dest:modrm_read8(&params,MODRM_src0);
	oper2b = src;
	op_or8();
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_OR16(word *dest, word src)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	oper1 = dest?*dest:modrm_read16(&params,MODRM_src0);
	oper2 = src;
	op_or16();
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	CPUPROT2
}

//For AND
OPTINLINE void CPU8086_internal_AND8(byte *dest, byte src)
{
	CPUPROT1
	oper1b = dest?*dest:modrm_read8(&params,MODRM_src0);
	oper2b = src;
	op_and8();
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_AND16(word *dest, word src)
{
	CPUPROT1
	oper1 = dest?*dest:modrm_read16(&params,MODRM_src0);
	oper2 = src;
	op_and16();
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	CPUPROT2
}


//For SUB
OPTINLINE void CPU8086_internal_SUB8(byte *dest, byte addition)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	oper1b = dest?*dest:modrm_read8(&params,MODRM_src0);
	oper2b = addition;
	op_sub8();
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_SUB16(word *dest, word addition)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	oper1 = dest?*dest:modrm_read16(&params,MODRM_src0);
	oper2 = addition;
	op_sub16();
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	CPUPROT2
}

//For SBB
OPTINLINE void CPU8086_internal_SBB8(byte *dest, byte addition)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	oper1b = dest?*dest:modrm_read8(&params,MODRM_src0);
	oper2b = addition;
	op_sbb8();
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_SBB16(word *dest, word addition)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	oper1 = dest?*dest:modrm_read16(&params,MODRM_src0);
	oper2 = addition;
	op_sbb16();
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	CPUPROT2
}

//For XOR
//See AND, but XOR
OPTINLINE void CPU8086_internal_XOR8(byte *dest, byte src)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	oper1b = dest?*dest:modrm_read8(&params,MODRM_src0);
	oper2b = src;
	op_xor8();
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_XOR16(word *dest, word src)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	oper1 = dest?*dest:modrm_read16(&params,MODRM_src0);
	oper2 = src;
	op_xor16();
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	CPUPROT2
}

//TEST : same as AND, but discarding the result!
OPTINLINE void CPU8086_internal_TEST8(byte dest, byte src)
{
	byte tmpdest = dest;
	CPU8086_internal_AND8(&tmpdest,src);
}
OPTINLINE void CPU8086_internal_TEST16(word dest, word src)
{
	word tmpdest = dest;
	CPU8086_internal_AND16(&tmpdest,src);
}

//MOV
OPTINLINE void CPU8086_internal_MOV8(byte *dest, byte val)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	if (dest) //Register?
	{
		*dest = val;
	}
	else //Memory?
	{
		if (custommem)
		{
			MMU_wb(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),offset,val); //Write to memory directly!
		}
		else //ModR/M?
		{
			modrm_write8(&params,MODRM_src0,val); //Write the result to memory!
		}
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_MOV16(word *dest, word val)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	if (dest) //Register?
	{
		*dest = val;
	}
	else //Memory?
	{
		if (custommem)
		{
			MMU_ww(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),offset,val); //Write to memory directly!
		}
		else //ModR/M?
		{
			modrm_write16(&params,MODRM_src0,val,0); //Write the result to memory!
		}
	}
	CPUPROT2
}

//LEA for LDS, LES
OPTINLINE word getLEA(MODRM_PARAMS *params)
{
	return modrm_lea16(params,1);
}


/*

Non-logarithmic opcodes!

*/


OPTINLINE void CPU8086_internal_DAA()
{
	CPUPROT1
	if (((REG_AL&0xF)>9) || FLAG_AF)
	{
		oper1 = REG_AL+6;
		REG_AL = (oper1&0xFF);
		FLAG_CF = ((oper1&0xFF00)>0);
		FLAG_AF = 1;
	}
	else FLAG_AF = 0;
	if (((REG_AL&0xF0)>0x90) || FLAG_CF)
	{
		REG_AL += 0x60;
		FLAG_CF = 1;
	}
	else
	{
		FLAG_CF = 0;
	}
	flag_szp8(REG_AL);
	CPUPROT2
}
OPTINLINE void CPU8086_internal_DAS()
{
	byte tempCF, tempAL;
	tempAL = REG_AL;
	tempCF = FLAG_CF; //Save old values!
	CPUPROT1
	if (((REG_AL&0xF)>9) || FLAG_AF)
	{
		oper1 = REG_AL-6;
		REG_AL = oper1&255;
		FLAG_CF = tempCF|((oper1&0xFF00)>0);
		FLAG_AF = 1;
	}
	else FLAG_AF = 0;

	if ((tempAL>0x99) || tempCF)
	{
		REG_AL -= 0x60;
		FLAG_CF = 1;
	}
	else
	{
		FLAG_CF = 0;
	}
	flag_szp8(REG_AL);
	CPUPROT2
}
OPTINLINE void CPU8086_internal_AAA()
{
	CPUPROT1
	if (((REG_AL&0xF)>9) || FLAG_AF)
	{
		REG_AL += 6;
		++REG_AH;
		FLAG_AF = 1;
		FLAG_CF = 1;
	}
	else
	{
		FLAG_AF = 0;
		FLAG_CF = 0;
	}
	REG_AL &= 0xF;
	flag_szp8(REG_AL); //Basic flags!
	CPUPROT2
}
OPTINLINE void CPU8086_internal_AAS()
{
	CPUPROT1
	if (((REG_AL&0xF)>9) || FLAG_AF)
	{
		REG_AL -= 6;
		--REG_AH;
		FLAG_AF = 1;
		FLAG_CF = 1;
	}
	else
	{
		FLAG_AF = 0;
		FLAG_CF = 0;
	}
	REG_AL &= 0xF;
	flag_szp8(REG_AL); //Basic flags!
	CPUPROT2
}

OPTINLINE void CPU8086_internal_CBW()
{
	CPUPROT1
	if ((REG_AL&0x80)==0x80)
	{
		REG_AH = 0xFF;
	}
	else
	{
		REG_AH = 0;
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_CWD()
{
	CPUPROT1
	if ((REG_AH&0x80)==0x80)
	{
		REG_DX = 0xFFFF;
	}
	else
	{
		REG_DX = 0;
	}
	CPUPROT2
}

//OK so far!
OPTINLINE void CPU8086_internal_MOVSB()
{
	byte data;
	if (blockREP) return; //Disabled REP!
	data = MMU_rb(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI, 0); //Try to read the data!
	CPUPROT1
	MMU_wb(CPU_SEGMENT_ES,REG_ES,REG_DI,data);
	CPUPROT1
	if (FLAG_DF)
	{
		--REG_SI;
		--REG_DI;
	}
	else
	{
		++REG_SI;
		++REG_DI;
	}
	CPUPROT2
	CPUPROT2
}
OPTINLINE void CPU8086_internal_MOVSW()
{
	word data;
	if (blockREP) return; //Disabled REP!
	data = MMU_rw(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI, 0); //Try to read the data!
	CPUPROT1
	MMU_ww(CPU_SEGMENT_ES,REG_ES,REG_DI,data); //Try to write the data!
	CPUPROT1
	if (FLAG_DF)
	{
		REG_SI -= 2;
		REG_DI -= 2;
	}
	else
	{
		REG_SI += 2;
		REG_DI += 2;
	}
	CPUPROT2
	CPUPROT2
}
OPTINLINE void CPU8086_internal_CMPSB()
{
	byte data1, data2;
	if (blockREP) return; //Disabled REP!
	data1 = MMU_rb(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI, 0); //Try to read the first data!
	CPUPROT1
		data2 = MMU_rb(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_DI, 0); //Try to read the second data!
	CPUPROT1
	CMP_b(data1,data2);
	if (FLAG_DF)
	{
		--REG_SI;
		--REG_DI;
	}
	else
	{
		++REG_SI;
		++REG_DI;
	}
	CPUPROT2
	CPUPROT2
}
OPTINLINE void CPU8086_internal_CMPSW()
{
	word data1, data2;
	if (blockREP) return; //Disabled REP!
	data1 = MMU_rw(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI, 0); //Try to read the first data!
	CPUPROT1
	data2 = MMU_rw(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_DI, 0); //Try to read the second data!
	CPUPROT1
	CMP_w(data1,data2);
	if (FLAG_DF)
	{
		REG_SI -= 2;
		REG_DI -= 2;
	}
	else
	{
		REG_SI += 2;
		REG_DI += 2;
	}
	CPUPROT2
	CPUPROT2
}
OPTINLINE void CPU8086_internal_STOSB()
{
	if (blockREP) return; //Disabled REP!
	MMU_wb(CPU_segment_index(CPU_SEGMENT_ES),REG_ES,REG_DI,REG_AL);
	CPUPROT1
	if (FLAG_DF)
	{
		--REG_DI;
	}
	else
	{
		++REG_DI;
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_STOSW()
{
	if (blockREP) return; //Disabled REP!
	MMU_ww(CPU_segment_index(CPU_SEGMENT_ES),REG_ES,REG_DI,REG_AX);
	CPUPROT1
	if (FLAG_DF)
	{
		REG_DI -= 2;
	}
	else
	{
		REG_DI += 2;
	}
	CPUPROT2
}
//OK so far!
OPTINLINE void CPU8086_internal_LODSB()
{
	byte value;
	if (blockREP) return; //Disabled REP!
	value = MMU_rb(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI, 0); //Try to read the result!
	CPUPROT1
	REG_AL = value;
	if (FLAG_DF)
	{
		--REG_SI;
	}
	else
	{
		++REG_SI;
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_LODSW()
{
	word value;
	if (blockREP) return; //Disabled REP!
	value = MMU_rw(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI, 0); //Try to read the result!
	CPUPROT1
	REG_AX = value;
	if (FLAG_DF)
	{
		REG_SI -= 2;
	}
	else
	{
		REG_SI += 2;
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_SCASB()
{
	byte cmp1;
	if (blockREP) return; //Disabled REP!
	cmp1 = MMU_rb(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_DI, 0); //Try to read the data to compare!
	CPUPROT1
	CMP_b(REG_AL,cmp1);
	if (FLAG_DF)
	{
		--REG_DI;
	}
	else
	{
		++REG_DI;
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_SCASW()
{
	word cmp1;
	if (blockREP) return; //Disabled REP!
	cmp1 = MMU_rw(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_DI, 0); //Try to read the data to compare!
	CPUPROT1
	CMP_w(REG_AX,cmp1);
	if (FLAG_DF)
	{
		REG_DI -= 2;
	}
	else
	{
		REG_DI += 2;
	}
	CPUPROT2
}

OPTINLINE void CPU8086_internal_RET(word popbytes)
{
	word val = CPU_POP16();    //Near return
	CPUPROT1
	REG_IP = val;
	CPU_flushPIQ(); //We're jumping to another address!
	REG_SP += popbytes;
	CPUPROT2
}
OPTINLINE void CPU8086_internal_RETF(word popbytes)
{
	word val = CPU_POP16();    //Far return
	CPUPROT1
	destEIP = val; //Load IP!
	segmentWritten(CPU_SEGMENT_CS,CPU_POP16(),2); //CS changed!
	CPUPROT1
		REG_SP += popbytes; //Process SP!
	CPUPROT2
	CPUPROT2
}

OPTINLINE void CPU8086_internal_INTO()
{
	CPUPROT1
	if (FLAG_OF)
	{
		CPU8086_int(4);
	}
	CPUPROT2
}

OPTINLINE void CPU8086_internal_AAM(byte data)
{
	CPUPROT1
	if (!data)
	{
		CPU_exDIV0();    //AAM
		return;
	}
	REG_AH = (((byte)SAFEDIV(REG_AL,data))&0xFF);
	REG_AL = (SAFEMOD(REG_AL,data)&0xFF);
	flag_szp16(REG_AX);
	FLAG_OF = FLAG_CF = FLAG_AF = 0; //Clear these!
	CPUPROT2
}
OPTINLINE void CPU8086_internal_AAD(byte data)
{
	CPUPROT1
	REG_AX = ((REG_AH*data)+REG_AL);    //AAD
	REG_AH = 0;
	flag_szp8(REG_AL); //Update the flags!
	FLAG_OF = FLAG_CF = FLAG_AF = 0; //Clear these!
	CPUPROT2
}

OPTINLINE void CPU8086_internal_XLAT()
{
	if (cpudebugger) //Debugger on?
	{
		debugger_setcommand("XLAT");    //XLAT
	}
	CPUPROT1
	byte value = MMU_rb(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),REG_BX+REG_AL,0);    //XLAT
	CPUPROT1
	REG_AL = value;
	CPUPROT2
	CPUPROT2
}

/*

TODO: data1&2 protection, CPU opcodes themselves modrm1/2 specs.

*/

OPTINLINE void CPU8086_internal_XCHG8(byte *data1, byte *data2)
{
	CPUPROT1
	oper1b = data1?*data1:modrm_read8(&params,MODRM_src0);
	CPUPROT1
	oper2b = data2?*data2:modrm_read8(&params,MODRM_src1);
	CPUPROT1
	byte temp = oper1b; //Copy!
	oper1b = oper2b; //We're ...
	oper2b = temp; //Swapping this!
	if (data1)
	{
		*data1 = oper1b;
	}
	else
	{
		modrm_write8(&params,MODRM_src0,oper1b);
	}
	CPUPROT1
	if (data2)
	{
		*data2 = oper2b;
	}
	else
	{
		modrm_write8(&params,MODRM_src1,oper2b);
	}
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
}

OPTINLINE void CPU8086_internal_XCHG16(word *data1, word *data2)
{
	CPUPROT1
	oper1 = data1?*data1:modrm_read16(&params,MODRM_src0);
	CPUPROT1
	oper2 = data2?*data2:modrm_read16(&params,MODRM_src1);
	CPUPROT1
	//Do a simple swap!
	word temp = oper1; //Copy!
	oper1 = oper2; //We're ...
	oper2 = temp; //Swapping this!
	if (data1)
	{
		*data1 = oper1;
	}
	else
	{
		modrm_write16(&params,MODRM_src0,oper1,0);
	}
	CPUPROT1
	if (data2)
	{
		*data2 = oper2;
	}
	else
	{
		modrm_write16(&params,MODRM_src1,oper2,0);
	}
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
}

extern byte modrm_addoffset; //Add this offset to ModR/M reads!

OPTINLINE void CPU8086_internal_LXS(int segmentregister) //LDS, LES etc.
{
	CPUPROT1
	word offset = modrm_read16(&params,1);
	CPUPROT1
	modrm_addoffset = 2; //Add this to the offset to use!
	word segment = modrm_read16(&params,1);
	modrm_addoffset = 0; //Reset again!
	CPUPROT1
		segmentWritten(segmentregister, segment,0); //Load the new segment!
	CPUPROT1
		modrm_write16(&params, 0, offset, 0); //Try to load the new register with the offset!
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
}

//ALL ABOVE SHOULD BE OK ACCORDING TO FAKE86 CPU.C

/*

NOW THE REAL OPCODES!

*/

void CPU8086_OP00() {modrm_readparams(&params,0,0); modrm_generateInstructionTEXT("ADDB",8,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_ADD8(modrm_addr8(&params,1,0),modrm_read8(&params,0)); }
void CPU8086_OP01() {modrm_readparams(&params,1,0); modrm_generateInstructionTEXT("ADDW",16,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_ADD16(modrm_addr16(&params,1,0),modrm_read16(&params,0)); }
void CPU8086_OP02() {modrm_readparams(&params,0,0); modrm_generateInstructionTEXT("ADDB",8,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_ADD8(modrm_addr8(&params,0,0),modrm_read8(&params,1)); }
void CPU8086_OP03() {modrm_readparams(&params,1,0); modrm_generateInstructionTEXT("ADDW",16,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_ADD16(modrm_addr16(&params,0,0),modrm_read16(&params,1)); }
void CPU8086_OP04() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("ADDB AL,",0,theimm,PARAM_IMM8); CPU8086_internal_ADD8(&REG_AL,theimm); }
void CPU8086_OP05() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("ADDW AX,",0,theimm,PARAM_IMM16); CPU8086_internal_ADD16(&REG_AX,theimm); }
void CPU8086_OP06() {modrm_generateInstructionTEXT("PUSH ES",0,0,PARAM_NONE); CPU_PUSH16(&REG_ES);/*PUSH ES*/ }
void CPU8086_OP07() {modrm_generateInstructionTEXT("POP ES",0,0,PARAM_NONE); segmentWritten(CPU_SEGMENT_ES,CPU_POP16(),0); /*CS changed!*/ }
void CPU8086_OP08() {modrm_readparams(&params,0,0); modrm_generateInstructionTEXT("ORB",8,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_OR8(modrm_addr8(&params,1,0),modrm_read8(&params,0)); }
void CPU8086_OP09() {modrm_readparams(&params,1,0); modrm_generateInstructionTEXT("ORW",16,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_OR16(modrm_addr16(&params,1,0),modrm_read16(&params,0)); }
void CPU8086_OP0A() {modrm_readparams(&params,0,0); modrm_generateInstructionTEXT("ORB",8,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_OR8(modrm_addr8(&params,0,0),modrm_read8(&params,1)); }
void CPU8086_OP0B() {modrm_readparams(&params,1,0); modrm_generateInstructionTEXT("ORW",16,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_OR16(modrm_addr16(&params,0,0),modrm_read16(&params,1)); }
void CPU8086_OP0C() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("ORB AL,",0,theimm,PARAM_IMM8); CPU8086_internal_OR8(&REG_AL,theimm); }
void CPU8086_OP0D() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("ORW AX,",0,theimm,PARAM_IMM16); CPU8086_internal_OR16(&REG_AX,theimm); }
void CPU8086_OP0E() {modrm_generateInstructionTEXT("PUSH CS",0,0,PARAM_NONE); CPU_PUSH16(&REG_CS);/*PUSH CS*/ }
void CPU8086_OP0F() /*FLAG_OF: POP CS; shouldn't be used?*/ { modrm_generateInstructionTEXT("POP CS", 0, 0, PARAM_NONE); /*Don't handle: 8086 ignores this opcode, and you won't find it there!*/ destEIP = REG_EIP; segmentWritten(CPU_SEGMENT_CS, CPU_POP16(), 0); /*POP CS!*/ }
void CPU8086_OP10() {modrm_readparams(&params,0,0); modrm_generateInstructionTEXT("ADCB",8,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_ADC8(modrm_addr8(&params,1,0),modrm_read8(&params,0)); }
void CPU8086_OP11() {modrm_readparams(&params,1,0); modrm_generateInstructionTEXT("ADCW",16,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_ADC16(modrm_addr16(&params,1,0),modrm_read16(&params,0)); }
void CPU8086_OP12() {modrm_readparams(&params,0,0); modrm_generateInstructionTEXT("ADCB",8,0,PARAM_MODRM12);  MODRM_src0 = 0; CPU8086_internal_ADC8(modrm_addr8(&params,0,0),modrm_read8(&params,1)); }
void CPU8086_OP13() {modrm_readparams(&params,1,0); modrm_generateInstructionTEXT("ADCW",16,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_ADC16(modrm_addr16(&params,0,0),modrm_read16(&params,1)); }
void CPU8086_OP14() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("ADC AL,",0,theimm,PARAM_IMM8); CPU8086_internal_ADC8(&REG_AL,theimm); }
void CPU8086_OP15() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("ADC AX,",0,theimm,PARAM_IMM16); CPU8086_internal_ADC16(&REG_AX,theimm); }
void CPU8086_OP16() {modrm_generateInstructionTEXT("PUSH SS",0,0,PARAM_NONE);/*PUSH SS*/ CPU_PUSH16(&REG_SS);/*PUSH SS*/ }
void CPU8086_OP17() {modrm_generateInstructionTEXT("POP SS",0,0,PARAM_NONE);/*POP SS*/ segmentWritten(CPU_SEGMENT_SS,CPU_POP16(),0); /*CS changed!*/ }
void CPU8086_OP18() {modrm_readparams(&params,0,0); modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("SBBB",8,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_SBB8(modrm_addr8(&params,1,0),(modrm_read8(&params,0))); }
void CPU8086_OP19() {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("SBBW",16,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_SBB16(modrm_addr16(&params,1,0),(modrm_read16(&params,0))); }
void CPU8086_OP1A() {modrm_readparams(&params,0,0); modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("SBBB",8,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_SBB8(modrm_addr8(&params,0,0),(modrm_read8(&params,1))); }
void CPU8086_OP1B() {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("SBBW",16,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_SBB16(modrm_addr16(&params,0,0),(modrm_read16(&params,1))); }
void CPU8086_OP1C() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("SBB AL,",0,theimm,PARAM_IMM8); CPU8086_internal_SBB8(&REG_AL,theimm); }
void CPU8086_OP1D() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("SBB AX,",0,theimm,PARAM_IMM16); CPU8086_internal_SBB16(&REG_AX,theimm); }
void CPU8086_OP1E() {modrm_generateInstructionTEXT("PUSH DS",0,0,PARAM_NONE);/*PUSH DS*/ CPU_PUSH16(&REG_DS);/*PUSH DS*/ }
void CPU8086_OP1F() {modrm_generateInstructionTEXT("POP DS",0,0,PARAM_NONE);/*POP DS*/ segmentWritten(CPU_SEGMENT_DS,CPU_POP16(),0); /*CS changed!*/ }
void CPU8086_OP20() {modrm_readparams(&params,0,0); modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("ANDB",8,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_AND8(modrm_addr8(&params,1,0),modrm_read8(&params,0)); }
void CPU8086_OP21() {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("ANDW",16,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_AND16(modrm_addr16(&params,1,0),modrm_read16(&params,0)); }
void CPU8086_OP22() {modrm_readparams(&params,0,0); modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("ANDB",8,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_AND8(modrm_addr8(&params,0,0),modrm_read8(&params,1)); }
void CPU8086_OP23() {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("ANDW",16,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_AND16(modrm_addr16(&params,0,0),modrm_read16(&params,1)); }
void CPU8086_OP24() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("AND AL,",0,theimm,PARAM_IMM8); CPU8086_internal_AND8(&REG_AL,theimm); }
void CPU8086_OP25() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("AND AX,",0,theimm,PARAM_IMM16); CPU8086_internal_AND16(&REG_AX,theimm); }
void CPU8086_OP27() {modrm_generateInstructionTEXT("DAA",0,0,PARAM_NONE);/*DAA?*/ CPU8086_internal_DAA();/*DAA?*/ }
void CPU8086_OP28() {modrm_readparams(&params,0,0); modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("SUBB",8,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_SUB8(modrm_addr8(&params,1,0),(modrm_read8(&params,0))); }
void CPU8086_OP29() {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("SUBW",16,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_SUB16(modrm_addr16(&params,1,0),(modrm_read16(&params,0))); }
void CPU8086_OP2A() {modrm_readparams(&params,0,0); modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("SUBB",8,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_SUB8(modrm_addr8(&params,0,0),(modrm_read8(&params,1))); }
void CPU8086_OP2B() {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("SUBW",16,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_SUB16(modrm_addr16(&params,0,0),(modrm_read16(&params,1))); }
void CPU8086_OP2C() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("SUB AL,",0,theimm,PARAM_IMM8);/*4=AL,imm8*/ CPU8086_internal_SUB8(&REG_AL,theimm);/*4=AL,imm8*/ }
void CPU8086_OP2D() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("SUB AX,",0,theimm,PARAM_IMM16);/*5=AX,imm16*/ CPU8086_internal_SUB16(&REG_AX,theimm);/*5=AX,imm16*/ }
void CPU8086_OP2F() {modrm_generateInstructionTEXT("DAS",0,0,PARAM_NONE);/*DAS?*/ CPU8086_internal_DAS();/*DAS?*/ }
void CPU8086_OP30() {modrm_readparams(&params,0,0); modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("XORB",8,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_XOR8(modrm_addr8(&params,1,0),modrm_read8(&params,0)); }
void CPU8086_OP31() {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("XORW",16,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_XOR16(modrm_addr16(&params,1,0),modrm_read16(&params,0)); }
void CPU8086_OP32() {modrm_readparams(&params,0,0); modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("XORB",8,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_XOR8(modrm_addr8(&params,0,0),modrm_read8(&params,1)); }
void CPU8086_OP33() {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("XORW",16,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_XOR16(modrm_addr16(&params,0,0),modrm_read16(&params,1)); }
void CPU8086_OP34() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("XOR AL,",0,theimm,PARAM_IMM8); CPU8086_internal_XOR8(&REG_AL,theimm); }
void CPU8086_OP35() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("XOR AX,",0,theimm,PARAM_IMM16); CPU8086_internal_XOR16(&REG_AX,theimm); }
void CPU8086_OP37() {modrm_generateInstructionTEXT("AAA",0,0,PARAM_NONE);/*AAA?*/ CPU8086_internal_AAA();/*AAA?*/ }
void CPU8086_OP38() {modrm_readparams(&params,0,0); modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("CMPB",8,0,PARAM_MODRM21); CMP_b(modrm_read8(&params,1),modrm_read8(&params,0)); }
void CPU8086_OP39() {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("CMPW",16,0,PARAM_MODRM21); CMP_w(modrm_read16(&params,1),modrm_read16(&params,0)); }
void CPU8086_OP3A() {modrm_readparams(&params,0,0); modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("CMPB",8,0,PARAM_MODRM12); CMP_b(modrm_read8(&params,0),modrm_read8(&params,1)); }
void CPU8086_OP3B() {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("CMPW",16,0,PARAM_MODRM12); CMP_w(modrm_read16(&params,0),modrm_read16(&params,1)); }
void CPU8086_OP3C() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("CMP AL,",0,theimm,PARAM_IMM8);/*CMP AL, imm8*/ CMP_b(REG_AL,theimm);/*CMP AL, imm8*/ }
void CPU8086_OP3D() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("CMP AX,",0,theimm,PARAM_IMM16);/*CMP AX, imm16*/ CMP_w(REG_AX,theimm);/*CMP AX, imm16*/ }
void CPU8086_OP3F() {modrm_generateInstructionTEXT("AAS",0,0,PARAM_NONE);/*AAS?*/ CPU8086_internal_AAS();/*AAS?*/ }
void CPU8086_OP40() {modrm_generateInstructionTEXT("INC AX",0,0,PARAM_NONE);/*INC AX*/ CPU8086_internal_INC16(&REG_AX);/*INC AX*/ }
void CPU8086_OP41() {modrm_generateInstructionTEXT("INC CX",0,0,PARAM_NONE);/*INC CX*/ CPU8086_internal_INC16(&REG_CX);/*INC CX*/ }
void CPU8086_OP42() {modrm_generateInstructionTEXT("INC DX",0,0,PARAM_NONE);/*INC DX*/ CPU8086_internal_INC16(&REG_DX);/*INC DX*/ }
void CPU8086_OP43() {modrm_generateInstructionTEXT("INC BX",0,0,PARAM_NONE);/*INC BX*/ CPU8086_internal_INC16(&REG_BX);/*INC BX*/ }
void CPU8086_OP44() {modrm_generateInstructionTEXT("INC SP",0,0,PARAM_NONE);/*INC SP*/ CPU8086_internal_INC16(&REG_SP);/*INC SP*/ }
void CPU8086_OP45() {modrm_generateInstructionTEXT("INC BP",0,0,PARAM_NONE);/*INC BP*/ CPU8086_internal_INC16(&REG_BP);/*INC BP*/ }
void CPU8086_OP46() {modrm_generateInstructionTEXT("INC SI",0,0,PARAM_NONE);/*INC SI*/ CPU8086_internal_INC16(&REG_SI);/*INC SI*/ }
void CPU8086_OP47() {modrm_generateInstructionTEXT("INC DI",0,0,PARAM_NONE);/*INC DI*/ CPU8086_internal_INC16(&REG_DI);/*INC DI*/ }
void CPU8086_OP48() {modrm_generateInstructionTEXT("DEC AX",0,0,PARAM_NONE);/*DEC AX*/ CPU8086_internal_DEC16(&REG_AX);/*DEC AX*/ }
void CPU8086_OP49() {modrm_generateInstructionTEXT("DEC CX",0,0,PARAM_NONE);/*DEC CX*/ CPU8086_internal_DEC16(&REG_CX);/*DEC CX*/ }
void CPU8086_OP4A() {modrm_generateInstructionTEXT("DEC DX",0,0,PARAM_NONE);/*DEC DX*/ CPU8086_internal_DEC16(&REG_DX);/*DEC DX*/ }
void CPU8086_OP4B() {modrm_generateInstructionTEXT("DEC BX",0,0,PARAM_NONE);/*DEC BX*/ CPU8086_internal_DEC16(&REG_BX);/*DEC BX*/ }
void CPU8086_OP4C() {modrm_generateInstructionTEXT("DEC SP",0,0,PARAM_NONE);/*DEC SP*/ CPU8086_internal_DEC16(&REG_SP);/*DEC SP*/ }
void CPU8086_OP4D() {modrm_generateInstructionTEXT("DEC BP",0,0,PARAM_NONE);/*DEC BP*/ CPU8086_internal_DEC16(&REG_BP);/*DEC BP*/ }
void CPU8086_OP4E() {modrm_generateInstructionTEXT("DEC SI",0,0,PARAM_NONE);/*DEC SI*/ CPU8086_internal_DEC16(&REG_SI);/*DEC SI*/ }
void CPU8086_OP4F() {modrm_generateInstructionTEXT("DEC DI",0,0,PARAM_NONE);/*DEC DI*/ CPU8086_internal_DEC16(&REG_DI);/*DEC DI*/ }
void CPU8086_OP50() {modrm_generateInstructionTEXT("PUSH AX",0,0,PARAM_NONE);/*PUSH AX*/ CPU_PUSH16(&REG_AX);/*PUSH AX*/ }
void CPU8086_OP51() {modrm_generateInstructionTEXT("PUSH CX",0,0,PARAM_NONE);/*PUSH CX*/ CPU_PUSH16(&REG_CX);/*PUSH CX*/ }
void CPU8086_OP52() {modrm_generateInstructionTEXT("PUSH DX",0,0,PARAM_NONE);/*PUSH DX*/ CPU_PUSH16(&REG_DX);/*PUSH DX*/ }
void CPU8086_OP53() {modrm_generateInstructionTEXT("PUSH BX",0,0,PARAM_NONE);/*PUSH BX*/ CPU_PUSH16(&REG_BX);/*PUSH BX*/ }
void CPU8086_OP54() {modrm_generateInstructionTEXT("PUSH SP",0,0,PARAM_NONE);/*PUSH SP*/ CPU_PUSH16(&REG_SP);/*PUSH SP*/ }
void CPU8086_OP55() {modrm_generateInstructionTEXT("PUSH BP",0,0,PARAM_NONE);/*PUSH BP*/ CPU_PUSH16(&REG_BP);/*PUSH BP*/ }
void CPU8086_OP56() {modrm_generateInstructionTEXT("PUSH SI",0,0,PARAM_NONE);/*PUSH SI*/ CPU_PUSH16(&REG_SI);/*PUSH SI*/ }
void CPU8086_OP57() {modrm_generateInstructionTEXT("PUSH DI",0,0,PARAM_NONE);/*PUSH DI*/ CPU_PUSH16(&REG_DI);/*PUSH DI*/ }
void CPU8086_OP58() {modrm_generateInstructionTEXT("POP AX",0,0,PARAM_NONE);/*POP AX*/ REG_AX = CPU_POP16();/*POP AX*/ }
void CPU8086_OP59() {modrm_generateInstructionTEXT("POP CX",0,0,PARAM_NONE);/*POP CX*/ REG_CX = CPU_POP16();/*POP CX*/ }
void CPU8086_OP5A() {modrm_generateInstructionTEXT("POP DX",0,0,PARAM_NONE);/*POP DX*/ REG_DX = CPU_POP16();/*POP DX*/ }
void CPU8086_OP5B() {modrm_generateInstructionTEXT("POP BX",0,0,PARAM_NONE);/*POP BX*/ REG_BX = CPU_POP16();/*POP BX*/ }
void CPU8086_OP5C() {modrm_generateInstructionTEXT("POP SP",0,0,PARAM_NONE);/*POP SP*/ REG_SP = MMU_rw(CPU_SEGMENT_SS,REG_SS,REG_SP,0);/*POP SP*/ }
void CPU8086_OP5D() {modrm_generateInstructionTEXT("POP BP",0,0,PARAM_NONE);/*POP BP*/ REG_BP = CPU_POP16();/*POP BP*/ }
void CPU8086_OP5E() {modrm_generateInstructionTEXT("POP SI",0,0,PARAM_NONE);/*POP SI*/ REG_SI = CPU_POP16();/*POP SI*/ }
void CPU8086_OP5F() {modrm_generateInstructionTEXT("POP DI",0,0,PARAM_NONE);/*POP DI*/ REG_DI = CPU_POP16();/*POP DI*/ }
void CPU8086_OP70() {signed char rel8;/*JO rel8: (FLAG_OF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JO",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_OF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ } }
void CPU8086_OP71() {signed char rel8;/*JNO rel8 : (FLAG_OF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNO",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_OF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/} }
void CPU8086_OP72() {signed char rel8;/*JC rel8: (FLAG_CF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JC",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_CF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/} }
void CPU8086_OP73() {signed char rel8;/*JNC rel8 : (FLAG_CF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNC",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_CF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/} }
void CPU8086_OP74() {signed char rel8;/*JZ rel8: (FLAG_ZF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JZ",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_ZF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/} }
void CPU8086_OP75() {signed char rel8;/*JNZ rel8 : (FLAG_ZF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNZ",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_ZF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/} }
void CPU8086_OP76() {signed char rel8;/*JBE rel8 : (FLAG_CF=1|FLAG_ZF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JBE",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_CF||FLAG_ZF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/} }
void CPU8086_OP77() {signed char rel8;/*JA rel8: (FLAG_CF=0&FLAG_ZF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JA",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_CF && !FLAG_ZF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/} }
void CPU8086_OP78() {signed char rel8;/*JS rel8: (FLAG_SF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JS",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_SF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/} }
void CPU8086_OP79() {signed char rel8;/*JNS rel8 : (FLAG_SF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNS",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_SF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/} }
void CPU8086_OP7A() {signed char rel8;/*JP rel8 : (FLAG_PF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JP",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_PF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/} }
void CPU8086_OP7B() {signed char rel8;/*JNP rel8 : (FLAG_PF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNP",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_PF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/} }
void CPU8086_OP7C() {signed char rel8;/*JL rel8: (FLAG_SF!=FLAG_OF)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JL",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_SF!=FLAG_OF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/} }
void CPU8086_OP7D() {signed char rel8;/*JGE rel8 : (FLAG_SF=FLAG_OF)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JGE",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_SF==FLAG_OF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/} }
void CPU8086_OP7E() {signed char rel8;/*JLE rel8 : (FLAG_ZF|(FLAG_SF!=FLAG_OF))*/ rel8 = imm8(); modrm_generateInstructionTEXT("JLE",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if ((FLAG_SF!=FLAG_OF) || FLAG_ZF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/} }
void CPU8086_OP7F() {signed char rel8;/*JG rel8: ((FLAG_ZF=0)&&(FLAG_SF=FLAG_OF))*/ rel8 = imm8(); modrm_generateInstructionTEXT("JG",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_ZF && (FLAG_SF==FLAG_OF)) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/} }
void CPU8086_OP84() {modrm_readparams(&params,0,0); modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("TESTB",8,0,PARAM_MODRM12); CPU8086_internal_TEST8(modrm_read8(&params,0),modrm_read8(&params,1)); }
void CPU8086_OP85() {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("TESTW",16,0,PARAM_MODRM12); CPU8086_internal_TEST16(modrm_read16(&params,0),modrm_read16(&params,1)); }
void CPU8086_OP86() {modrm_readparams(&params,0,0); modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("XCHGB",8,0,PARAM_MODRM12); MODRM_src0 = 0; MODRM_src1 = 1; CPU8086_internal_XCHG8(modrm_addr8(&params,0,0),modrm_addr8(&params,1,1)); /*XCHG reg8,r/m8*/ }
void CPU8086_OP87() {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("XCHGW",16,0,PARAM_MODRM12); MODRM_src0 = 0; MODRM_src1 = 1; CPU8086_internal_XCHG16(modrm_addr16(&params,0,0),modrm_addr16(&params,1,1)); /*XCHG reg16,r/m16*/ }
void CPU8086_OP88() {modrm_readparams(&params,0,0); modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("MOVB",8,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_MOV8(modrm_addr8(&params,1,0),modrm_read8(&params,0)); }
void CPU8086_OP89() {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("MOVW",16,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_MOV16(modrm_addr16(&params,1,0),modrm_read16(&params,0)); }
void CPU8086_OP8A() {modrm_readparams(&params,0,0); modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("MOVB",8,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_MOV8(modrm_addr8(&params,0,0),modrm_read8(&params,1)); }
void CPU8086_OP8B() {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("MOVW",16,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_MOV16(modrm_addr16(&params,0,0),modrm_read16(&params,1)); }
void CPU8086_OP8C() {modrm_readparams(&params,1,2); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("MOVW",16,0,PARAM_MODRM21); MODRM_src0 = 1; CPU8086_internal_MOV16(modrm_addr16(&params,1,0),modrm_read16(&params,0)); }
void CPU8086_OP8D() {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); debugger_setcommand("LEA %s,%s",modrm_param1,getLEAtext(&params)); MODRM_src0 = 0; CPU8086_internal_MOV16(modrm_addr16(&params,0,0),getLEA(&params)); }
void CPU8086_OP8E() {modrm_readparams(&params,1,2); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("MOVW",16,0,PARAM_MODRM12); MODRM_src0 = 0; CPU8086_internal_MOV16(modrm_addr16(&params,0,0),modrm_read16(&params,1)); }
void CPU8086_OP90() /*NOP*/ {modrm_generateInstructionTEXT("NOP",0,0,PARAM_NONE);/*NOP (XCHG AX,AX)*/ CPU8086_internal_XCHG16(&REG_AX,&REG_AX); }
void CPU8086_OP91() {modrm_generateInstructionTEXT("XCHG CX,AX",0,0,PARAM_NONE);/*XCHG AX,CX*/ CPU8086_internal_XCHG16(&REG_CX,&REG_AX); /*XCHG CX,AX*/ }
void CPU8086_OP92() {modrm_generateInstructionTEXT("XCHG DX,AX",0,0,PARAM_NONE);/*XCHG AX,DX*/ CPU8086_internal_XCHG16(&REG_DX,&REG_AX); /*XCHG DX,AX*/ }
void CPU8086_OP93() {modrm_generateInstructionTEXT("XCHG BX,AX",0,0,PARAM_NONE);/*XCHG AX,BX*/ CPU8086_internal_XCHG16(&REG_BX,&REG_AX); /*XCHG BX,AX*/ }
void CPU8086_OP94() {modrm_generateInstructionTEXT("XCHG SP,AX",0,0,PARAM_NONE);/*XCHG AX,SP*/ CPU8086_internal_XCHG16(&REG_SP,&REG_AX); /*XCHG SP,AX*/ }
void CPU8086_OP95() {modrm_generateInstructionTEXT("XCHG BP,AX",0,0,PARAM_NONE);/*XCHG AX,BP*/ CPU8086_internal_XCHG16(&REG_BP,&REG_AX); /*XCHG BP,AX*/ }
void CPU8086_OP96() {modrm_generateInstructionTEXT("XCHG SI,AX",0,0,PARAM_NONE);/*XCHG AX,SI*/ CPU8086_internal_XCHG16(&REG_SI,&REG_AX); /*XCHG SI,AX*/ }
void CPU8086_OP97() {modrm_generateInstructionTEXT("XCHG DI,AX",0,0,PARAM_NONE);/*XCHG AX,DI*/ CPU8086_internal_XCHG16(&REG_DI,&REG_AX); /*XCHG DI,AX*/ }
void CPU8086_OP98() {modrm_generateInstructionTEXT("CBW",0,0,PARAM_NONE);/*CBW : sign extend AL to AX*/ CPU8086_internal_CBW();/*CBW : sign extend AL to AX (8088+)*/ }
void CPU8086_OP99() {modrm_generateInstructionTEXT("CWD",0,0,PARAM_NONE);/*CWD : sign extend AX to DX::AX*/ CPU8086_internal_CWD();/*CWD : sign extend AX to DX::AX (8088+)*/ }
void CPU8086_OP9A() {/*CALL Ap*/ word offset = CPU_readOPw(); word segment = CPU_readOPw(); debugger_setcommand("CALL %04x:%04x", segment, offset); CPU_PUSH16(&REG_CS); CPU_PUSH16(&REG_IP); destEIP = offset;  segmentWritten(CPU_SEGMENT_CS, segment, 2); /*CS changed!*/ }
void CPU8086_OP9B() {modrm_generateInstructionTEXT("WAIT",0,0,PARAM_NONE);/*WAIT : wait for TEST pin activity. (UNIMPLEMENTED)*/ CPU[activeCPU].wait = 1;/*9B: WAIT : wait for TEST pin activity. (Edit: continue on interrupts or 8087+!!!)*/ }
void CPU8086_OP9C() {modrm_generateInstructionTEXT("PUSHF",0,0,PARAM_NONE);/*PUSHF*/ CPU_PUSH16(&REG_FLAGS); }
void CPU8086_OP9D() { modrm_generateInstructionTEXT("POPF", 0, 0, PARAM_NONE);/*POPF*/ REG_FLAGS = CPU_POP16(); updateCPUmode(); /*POPF*/ }
void CPU8086_OP9E() { modrm_generateInstructionTEXT("SAHF", 0, 0, PARAM_NONE);/*SAHF : Save AH to lower half of FLAGS.*/ REG_FLAGS = ((REG_FLAGS & 0xFF00) | REG_AH); updateCPUmode(); /*SAHF : Save AH to lower half of FLAGS.*/ }
void CPU8086_OP9F() {modrm_generateInstructionTEXT("LAHF",0,0,PARAM_NONE);/*LAHF : Load lower half of FLAGS into AH.*/ REG_AH = (REG_FLAGS&0xFF);/*LAHF : Load lower half of FLAGS into AH.*/ }
void CPU8086_OPA0() {word theimm = CPU_readOPw(); debugger_setcommand("MOVB AL,[%s:%04X]",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV AL,[imm16]*/ CPU8086_internal_MOV8(&REG_AL,MMU_rb(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,0));/*MOV AL,[imm16]*/ }
void CPU8086_OPA1() {word theimm = CPU_readOPw(); debugger_setcommand("MOVW AX,[%s:%04X]",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV AX,[imm16]*/  CPU8086_internal_MOV16(&REG_AX,MMU_rw(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,0));/*MOV AX,[imm16]*/ }
void CPU8086_OPA2() {word theimm = CPU_readOPw(); debugger_setcommand("MOVB [%s:%04X],AL",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV [imm16],AL*/ custommem = 1; offset = theimm; CPU8086_internal_MOV8(NULL,REG_AL);/*MOV [imm16],AL*/ custommem = 0; }
void CPU8086_OPA3() {word theimm = CPU_readOPw(); debugger_setcommand("MOVW [%s:%04X],AX",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV [imm16], AX*/ custommem = 1; offset = theimm; CPU8086_internal_MOV16(NULL,REG_AX);/*MOV [imm16], AX*/ custommem = 0; }
//GEBLEVEN met aanpassen.
void CPU8086_OPA4() {modrm_generateInstructionTEXT("MOVSB",0,0,PARAM_NONE);/*MOVSB*/ CPU8086_internal_MOVSB();/*MOVSB*/ }
void CPU8086_OPA5() {modrm_generateInstructionTEXT("MOVSW",0,0,PARAM_NONE);/*MOVSW*/ CPU8086_internal_MOVSW();/*MOVSW*/ }
void CPU8086_OPA6() {debugger_setcommand("CMPSB [%s:ESI],[ES:EDI]",CPU_textsegment(CPU_SEGMENT_DS));/*CMPSB*/ CPU8086_internal_CMPSB();/*CMPSB*/ }
void CPU8086_OPA7() {debugger_setcommand("CMPSW [%s:ESI],[ES:EDI]",CPU_textsegment(CPU_SEGMENT_DS));/*CMPSW*/ CPU8086_internal_CMPSW();/*CMPSW*/ }
void CPU8086_OPA8() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("TESTB AL,",0,theimm,PARAM_IMM8);/*TEST AL,imm8*/ CPU8086_internal_TEST8(REG_AL,theimm);/*TEST AL,imm8*/ }
void CPU8086_OPA9() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("TESTW AX,",0,theimm,PARAM_IMM16);/*TEST AX,imm16*/ CPU8086_internal_TEST16(REG_AX,theimm);/*TEST AX,imm16*/ }
void CPU8086_OPAA() {modrm_generateInstructionTEXT("STOSB",0,0,PARAM_NONE);/*STOSB*/ CPU8086_internal_STOSB();/*STOSB*/ }
void CPU8086_OPAB() {modrm_generateInstructionTEXT("STOSW",0,0,PARAM_NONE);/*STOSW*/ CPU8086_internal_STOSW();/*STOSW*/ }
void CPU8086_OPAC() {modrm_generateInstructionTEXT("LODSB",0,0,PARAM_NONE);/*LODSB*/ CPU8086_internal_LODSB();/*LODSB*/ }
void CPU8086_OPAD() {modrm_generateInstructionTEXT("LODSW",0,0,PARAM_NONE);/*LODSW*/ CPU8086_internal_LODSW();/*LODSW*/ }
void CPU8086_OPAE() {modrm_generateInstructionTEXT("SCASB",0,0,PARAM_NONE);/*SCASB*/ CPU8086_internal_SCASB();/*SCASB*/ }
void CPU8086_OPAF() {modrm_generateInstructionTEXT("SCASW",0,0,PARAM_NONE);/*SCASW*/ CPU8086_internal_SCASW();/*SCASW*/ }
void CPU8086_OPB0() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("MOVB AL,",0,theimm,PARAM_IMM8);/*MOV AL,imm8*/ CPU8086_internal_MOV8(&REG_AL,theimm);/*MOV AL,imm8*/ }
void CPU8086_OPB1() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("MOVB CL,",0,theimm,PARAM_IMM8);/*MOV CL,imm8*/ CPU8086_internal_MOV8(&REG_CL,theimm);/*MOV CL,imm8*/ }
void CPU8086_OPB2() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("MOVB DL,",0,theimm,PARAM_IMM8);/*MOV DL,imm8*/ CPU8086_internal_MOV8(&REG_DL,theimm);/*MOV DL,imm8*/ }
void CPU8086_OPB3() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("MOVB BL,",0,theimm,PARAM_IMM8);/*MOV BL,imm8*/ CPU8086_internal_MOV8(&REG_BL,theimm);/*MOV BL,imm8*/ }
void CPU8086_OPB4() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("MOVB AH,",0,theimm,PARAM_IMM8);/*MOV AH,imm8*/ CPU8086_internal_MOV8(&REG_AH,theimm);/*MOV AH,imm8*/ }
void CPU8086_OPB5() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("MOVB CH,",0,theimm,PARAM_IMM8);/*MOV CH,imm8*/ CPU8086_internal_MOV8(&REG_CH,theimm);/*MOV CH,imm8*/ }
void CPU8086_OPB6() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("MOVB DH,",0,theimm,PARAM_IMM8);/*MOV DH,imm8*/ CPU8086_internal_MOV8(&REG_DH,theimm);/*MOV DH,imm8*/ }
void CPU8086_OPB7() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("MOVB BH,",0,theimm,PARAM_IMM8);/*MOV BH,imm8*/ CPU8086_internal_MOV8(&REG_BH,theimm);/*MOV BH,imm8*/ }
void CPU8086_OPB8() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("MOVW AX,",0,theimm,PARAM_IMM16);/*MOV AX,imm16*/ CPU8086_internal_MOV16(&REG_AX,theimm);/*MOV AX,imm16*/ }
void CPU8086_OPB9() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("MOVW CX,",0,theimm,PARAM_IMM16);/*MOV CX,imm16*/ CPU8086_internal_MOV16(&REG_CX,theimm);/*MOV CX,imm16*/ }
void CPU8086_OPBA() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("MOVW DX,",0,theimm,PARAM_IMM16);/*MOV DX,imm16*/ CPU8086_internal_MOV16(&REG_DX,theimm);/*MOV DX,imm16*/ }
void CPU8086_OPBB() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("MOVW BX,",0,theimm,PARAM_IMM16);/*MOV BX,imm16*/ CPU8086_internal_MOV16(&REG_BX,theimm);/*MOV BX,imm16*/ }
void CPU8086_OPBC() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("MOVW SP,",0,theimm,PARAM_IMM16);/*MOV SP,imm16*/ CPU8086_internal_MOV16(&REG_SP,theimm);/*MOV SP,imm16*/ }
void CPU8086_OPBD() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("MOVW BP,",0,theimm,PARAM_IMM16);/*MOV BP,imm16*/ CPU8086_internal_MOV16(&REG_BP,theimm);/*MOV BP,imm16*/ }
void CPU8086_OPBE() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("MOVW SI,",0,theimm,PARAM_IMM16);/*MOV SI,imm16*/ CPU8086_internal_MOV16(&REG_SI,theimm);/*MOV SI,imm16*/ }
void CPU8086_OPBF() {word theimm = CPU_readOPw(); modrm_generateInstructionTEXT("MOVW DI,",0,theimm,PARAM_IMM16);/*MOV DI,imm16*/ CPU8086_internal_MOV16(&REG_DI,theimm);/*MOV DI,imm16*/ }
/*So far, OK! Second test OK (2013-10-21 11:19)*/ /* Done up to here! 20140505_1258 */
void CPU8086_OPC2() {sword popbytes = imm16();/*RET imm16 (Near return to calling proc and POP imm16 bytes)*/ modrm_generateInstructionTEXT("RET",0,popbytes,PARAM_IMM8); /*RET imm16 (Near return to calling proc and POP imm16 bytes)*/ CPU8086_internal_RET(popbytes); }
void CPU8086_OPC3() {modrm_generateInstructionTEXT("RET",0,0,PARAM_NONE);/*RET (Near return to calling proc)*/ /*RET (Near return to calling proc)*/ CPU8086_internal_RET(0); }
void CPU8086_OPC4() /*LES modr/m*/ {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("LES",0,0,PARAM_MODRM12); CPU8086_internal_LXS(CPU_SEGMENT_ES); /*Load new ES!*/ }
void CPU8086_OPC5() /*LDS modr/m*/ {modrm_readparams(&params,1,0); modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("LDS",0,0,PARAM_MODRM12); CPU8086_internal_LXS(CPU_SEGMENT_DS); /*Load new DS!*/ }
void CPU8086_OPC6() {modrm_readparams(&params,0,0); byte val = CPU_readOP(); modrm_debugger8(&params,0,1); debugger_setcommand("MOVB %s,%02x",modrm_param2,val); modrm_write8(&params,1,val); }
void CPU8086_OPC7() {modrm_readparams(&params,1,0); word val = CPU_readOPw(); modrm_debugger16(&params,0,1); debugger_setcommand("MOVW %s,%04x",modrm_param2,val); modrm_write16(&params,1,val,0); }
void CPU8086_OPCA() {sword popbytes = imm16();/*RETF imm16 (Far return to calling proc and pop imm16 bytes)*/ modrm_generateInstructionTEXT("RETF",0,popbytes,PARAM_IMM16); /*RETF imm16 (Far return to calling proc and pop imm16 bytes)*/ CPU8086_internal_RETF(popbytes); }
void CPU8086_OPCB() {modrm_generateInstructionTEXT("RETF",0,0,PARAM_NONE); /*RETF (Far return to calling proc)*/ CPU8086_internal_RETF(0); }
void CPU8086_OPCC() {modrm_generateInstructionTEXT("INT 3",0,0,PARAM_NONE); /*INT 3*/ CPU8086_int(3);/*INT 3*/ }
void CPU8086_OPCD() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("INT",0,theimm,PARAM_IMM8);/*INT imm8*/ CPU8086_int(theimm);/*INT imm8*/ }
void CPU8086_OPCE() {modrm_generateInstructionTEXT("INTO",0,0,PARAM_NONE);/*INTO*/ CPU8086_internal_INTO();/*INTO*/ }
void CPU8086_OPCF() {modrm_generateInstructionTEXT("IRET",0,0,PARAM_NONE);/*IRET*/ CPU8086_IRET();/*IRET : also restore interrupt flag!*/ }
void CPU8086_OPD4() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("AAM",0,theimm,PARAM_IMM8);/*AAM*/ CPU8086_internal_AAM(theimm);/*AAM*/ }
void CPU8086_OPD5() {byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("AAD",0,theimm,PARAM_IMM8);/*AAD*/ CPU8086_internal_AAD(theimm);/*AAD*/ }
void CPU8086_OPD6(){REG_AL=FLAG_CF?0xFF:0x00;} //Special case on the 8086: SALC!
void CPU8086_OPD7(){CPU8086_internal_XLAT();}
void CPU8086_OPE0(){signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT("LOOPNZ",0, ((REG_IP+rel8)&0xFFFF),PARAM_IMM16); if ((--REG_CX) && (!FLAG_ZF)){REG_IP += rel8; CPU_flushPIQ(); /*We're jumping to another address*/}}
//Hier gebleven met modrm_generateInstructionTEXT!
void CPU8086_OPE1(){signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT("LOOPZ",0, ((REG_IP+rel8)&0xFFFF),PARAM_IMM16);if ((--REG_CX) && (FLAG_ZF)){REG_IP += rel8;CPU_flushPIQ(); /*We're jumping to another address*/}}
void CPU8086_OPE2(){signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT("LOOP", 0,((REG_IP+rel8)&0xFFFF),PARAM_IMM16);if (--REG_CX){REG_IP += rel8;CPU_flushPIQ(); /*We're jumping to another address*/}}
void CPU8086_OPE3(){signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT("JCXZ",0,((REG_IP+rel8)&0xFFFF),PARAM_IMM16); if (!REG_CX){REG_IP += rel8;CPU_flushPIQ(); /*We're jumping to another address*/}}
void CPU8086_OPE4(){byte theimm = CPU_readOP(); modrm_generateInstructionTEXT("IN AL,",0,theimm,PARAM_IMM8);REG_AL = PORT_IN_B(theimm);}
void CPU8086_OPE5(){byte theimm = CPU_readOP();modrm_generateInstructionTEXT("IN AX,",0,theimm,PARAM_IMM8);REG_AX = PORT_IN_W(theimm);}
void CPU8086_OPE6(){byte theimm = CPU_readOP();debugger_setcommand("OUT %02X,AL",theimm);PORT_OUT_B(theimm,REG_AL);}
void CPU8086_OPE7(){byte theimm = CPU_readOP(); debugger_setcommand("OUT %02X,AX",theimm); PORT_OUT_W(theimm,REG_AX);}
void CPU8086_OPE8(){sword reloffset = imm16(); modrm_generateInstructionTEXT("CALL",0,((REG_IP + reloffset)&0xFFFF),PARAM_IMM16); CPU_PUSH16(&REG_IP); REG_IP += reloffset;CPU_flushPIQ(); /*We're jumping to another address*/}
void CPU8086_OPE9(){sword reloffset = imm16(); modrm_generateInstructionTEXT("JMP",0,((REG_IP + reloffset)&0xFFFF),PARAM_IMM16); REG_IP += reloffset;CPU_flushPIQ(); /*We're jumping to another address*/}
void CPU8086_OPEA(){ word offset = CPU_readOPw(); word segment = CPU_readOPw(); debugger_setcommand("JMP %04X:%04X", segment, offset); destEIP = offset; segmentWritten(CPU_SEGMENT_CS, segment, 1); }
void CPU8086_OPEB(){signed char reloffset = imm8(); modrm_generateInstructionTEXT("JMP",0,((REG_IP + reloffset)&0xFFFF),PARAM_IMM16); REG_IP += reloffset;CPU_flushPIQ(); /*We're jumping to another address*/}
void CPU8086_OPEC(){modrm_generateInstructionTEXT("IN AL,DX",0,0,PARAM_NONE);REG_AL = PORT_IN_B(REG_DX);}
void CPU8086_OPED(){modrm_generateInstructionTEXT("IN AX,DX",0,0,PARAM_NONE); REG_AX = PORT_IN_W(REG_DX);}
void CPU8086_OPEE(){modrm_generateInstructionTEXT("OUT DX,AL",0,0,PARAM_NONE); PORT_OUT_B(REG_DX,REG_AL);}
void CPU8086_OPEF(){modrm_generateInstructionTEXT("OUT DX,AX",0,0,PARAM_NONE); PORT_OUT_W(REG_DX,REG_AX);}
void CPU8086_OPF1(){modrm_generateInstructionTEXT("<Undefined and reserved opcode, no error>",0,0,PARAM_NONE);}
//Finally simply:
void CPU8086_OPF4(){modrm_generateInstructionTEXT("HLT",0,0,PARAM_NONE); CPU[activeCPU].halt = 1;}
void CPU8086_OPF5(){modrm_generateInstructionTEXT("CMC",0,0,PARAM_NONE); FLAG_CF = !FLAG_CF;}
void CPU8086_OPF8(){modrm_generateInstructionTEXT("CLC",0,0,PARAM_NONE); FLAG_CF = 0;}
void CPU8086_OPF9(){modrm_generateInstructionTEXT("STC",0,0,PARAM_NONE); FLAG_CF = 1;}
void CPU8086_OPFA(){modrm_generateInstructionTEXT("CLI",0,0,PARAM_NONE); FLAG_IF = 0;}
void CPU8086_OPFB(){modrm_generateInstructionTEXT("STI",0,0,PARAM_NONE); FLAG_IF = 1;}
void CPU8086_OPFC(){modrm_generateInstructionTEXT("CLD",0,0,PARAM_NONE); FLAG_DF = 0;}
void CPU8086_OPFD(){modrm_generateInstructionTEXT("STD",0,0,PARAM_NONE); FLAG_DF = 1;}

/*

NOW COME THE GRP1-5 OPCODES:

*/

//GRP1

/*

DEBUG: REALLY SUPPOSED TO HANDLE OP80-83 HERE?

*/

void CPU8086_OP80() //GRP1 Eb,Ib
{
	modrm_readparams(&params,0,0);
	MODRM_src0 = 1;
	byte imm = CPU_readOP();
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger8(&params,0,1);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //ADD
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADDB %s,%02X",&modrm_param2,imm); //CMP Eb, Ib
		}
		CPU8086_internal_ADD8(modrm_addr8(&params,1,0),imm); //ADD Eb, Ib
		break;
	case 1: //OR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ORB %s,%02X",&modrm_param2,imm); //CMP Eb, Ib
		}
		CPU8086_internal_OR8(modrm_addr8(&params,1,0),imm); //OR Eb, Ib
		break;
	case 2: //ADC
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADCB %s,%02X",&modrm_param2,imm); //CMP Eb, Ib
		}
		CPU8086_internal_ADC8(modrm_addr8(&params,1,0),imm); //ADC Eb, Ib
		break;
	case 3: //SBB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SBBB %s,%02X",&modrm_param2,imm); //CMP Eb, Ib
		}
		CPU8086_internal_SBB8(modrm_addr8(&params,1,0),imm); //SBB Eb, Ib
		break;
	case 4: //AND
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ANDB %s,%02X",&modrm_param2,imm); //CMP Eb, Ib
		}
		CPU8086_internal_AND8(modrm_addr8(&params,1,0),imm); //AND Eb, Ib
		break;
	case 5: //SUB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SUBB %s,%02X",&modrm_param2,imm); //CMP Eb, Ib
		}
		CPU8086_internal_SUB8(modrm_addr8(&params,1,0),imm); //SUB Eb, Ib
		break;
	case 6: //XOR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("XORB %s,%02X",&modrm_param2,imm); //CMP Eb, Ib
		}
		CPU8086_internal_XOR8(modrm_addr8(&params,1,0),imm); //XOR Eb, Ib
		break;
	case 7: //CMP
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("CMPB %s,%02X",&modrm_param2,imm); //CMP Eb, Ib
		}
		CMP_b(modrm_read8(&params,1),imm); //CMP Eb, Ib
		break;
	default:
		break;
	}
}

void CPU8086_OP81() //GRP1 Ev,Iv
{
	MODRM_src0 = 1;
	modrm_readparams(&params, 1, 0);
	word imm = CPU_readOPw();
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger16(&params,0,1);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //ADD
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADDW %s,%04X",&modrm_param2,imm); //ADD Eb, Ib
		}
		CPU8086_internal_ADD16(modrm_addr16(&params,1,0),imm); //ADD Eb, Ib
		break;
	case 1: //OR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ORW %s,%04X",&modrm_param2,imm); //OR Eb, Ib
		}
		CPU8086_internal_OR16(modrm_addr16(&params,1,0),imm); //OR Eb, Ib
		break;
	case 2: //ADC
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADCW %s,%04X",&modrm_param2,imm); //ADC Eb, Ib
		}
		CPU8086_internal_ADC16(modrm_addr16(&params,1,0),imm); //ADC Eb, Ib
		break;
	case 3: //SBB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SBBW %s,%04X",&modrm_param2,imm); //SBB Eb, Ib
		}
		CPU8086_internal_SBB16(modrm_addr16(&params,1,0),imm); //SBB Eb, Ib
		break;
	case 4: //AND
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ANDW %s,%04X",&modrm_param2,imm); //AND Eb, Ib
		}
		CPU8086_internal_AND16(modrm_addr16(&params,1,0),imm); //AND Eb, Ib
		break;
	case 5: //SUB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SUBW %s,%04X",&modrm_param2,imm); //SUB Eb, Ib
		}
		CPU8086_internal_SUB16(modrm_addr16(&params,1,0),imm); //SUB Eb, Ib
		break;
	case 6: //XOR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("XORW %s,%04X",&modrm_param2,imm); //XOR Eb, Ib
		}
		CPU8086_internal_XOR16(modrm_addr16(&params,1,0),imm); //XOR Eb, Ib
		break;
	case 7: //CMP
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("CMPW %s,%04X",&modrm_param2,imm); //CMP Eb, Ib
		}
		CMP_w(modrm_read16(&params,1),imm); //CMP Eb, Ib
		break;
	default:
		break;
	}
}

void CPU8086_OP82() //GRP1 Eb,Ib (same as OP80)
{
	CPU8086_OP80(); //Same!
}

void CPU8086_OP83() //GRP1 Ev,Iv
{
	MODRM_src0 = 1;
	modrm_readparams(&params, 1, 0);
	word imm;
	imm = CPU_readOP();
	if (imm&0x80) imm |= 0xFF00; //Sign extend!
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger16(&params,0,1);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //ADD
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADDW %s,%04X",&modrm_param2,imm); //ADD Eb, Ib
		}
		CPU8086_internal_ADD16(modrm_addr16(&params,1,0),imm); //ADD Eb, Ib
		break;
	case 1: //OR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ORW %s,%04X",&modrm_param2,imm); //OR Eb, Ib
		}
		CPU8086_internal_OR16(modrm_addr16(&params,1,0),imm); //OR Eb, Ib
		break;
	case 2: //ADC
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADCW %s,%04X",&modrm_param2,imm); //ADC Eb, Ib
		}
		CPU8086_internal_ADC16(modrm_addr16(&params,1,0),imm); //ADC Eb, Ib
		break;
	case 3: //SBB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SBBW %s,%04X",&modrm_param2,imm); //SBB Eb, Ib
		}
		CPU8086_internal_SBB16(modrm_addr16(&params,1,0),imm); //SBB Eb, Ib
		break;
	case 4: //AND
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ANDW %s,%04X",&modrm_param2,imm); //AND Eb, Ib
		}
		CPU8086_internal_AND16(modrm_addr16(&params,1,0),imm); //AND Eb, Ib
		break;
	case 5: //SUB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SUBW %s,%04X",&modrm_param2,imm); //SUB Eb, Ib
		}
		CPU8086_internal_SUB16(modrm_addr16(&params,1,0),imm); //SUB Eb, Ib
		break;
	case 6: //XOR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("XORW %s,%04X",&modrm_param2,imm); //XOR Eb, Ib
		}
		CPU8086_internal_XOR16(modrm_addr16(&params,1,0),imm); //XOR Eb, Ib
		break;
	case 7: //CMP
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("CMPW %s,%04X",&modrm_param2,imm); //CMP Eb, Ib
		}
		CMP_w(modrm_read16(&params,1),imm); //CMP Eb, Ib
		break;
	default:
		break;
	}
}

void CPU8086_OP8F() //Undocumented GRP opcode 8F r/m16
{
	MODRM_src0 = 1;
	modrm_readparams(&params, 1, 0);
	if (cpudebugger)
	{
		modrm_debugger16(&params,0,1);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //POP
		if (cpudebugger) //Debugger on?
		{
			modrm_generateInstructionTEXT("POPW",16,0,PARAM_MODRM2); //POPW Ew
		}
		modrm_write16(&params,1,CPU_POP16(),0); //POP r/m16
		break;
	default: //Unknown opcode or special?
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("Unknown opcode: 8F /%i",MODRM_REG(params.modrm)); //Error!
		}
		break;
	}
}

void CPU8086_OPD0() //GRP2 Eb,1
{
	MODRM_src0 = 1;
	modrm_readparams(&params, 0, 0);
	reg = MODRM_REG(params.modrm);
	oper1b = modrm_read8(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger8(&params,0,1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //ROL
			debugger_setcommand("ROLB %s,1",&modrm_param2);
			break;
		case 1: //ROR
			debugger_setcommand("RORB %s,1",&modrm_param2);
			break;
		case 2: //RCL
			debugger_setcommand("RCLB %s,1",&modrm_param2);
			break;
		case 3: //RCR
			debugger_setcommand("RCRB %s,1",&modrm_param2);
			break;
		case 4: //SHL
		case 6: //--- Unknown Opcode! --- Undocumented opcode!
			debugger_setcommand("SHLB %s,1",&modrm_param2);
			break;
		case 5: //SHR
			debugger_setcommand("SHRB %s,1",&modrm_param2);
			break;
		case 7: //SAR
			debugger_setcommand("SARB %s,1",&modrm_param2);
			break;
		default:
			break;
		}
	}
	modrm_write8(&params,1,op_grp2_8(1));
}
void CPU8086_OPD1() //GRP2 Ev,1
{
	MODRM_src0 = 1;
	modrm_readparams(&params, 1, 0);
	reg = MODRM_REG(params.modrm);
	oper1 = modrm_read16(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger16(&params,0,1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //ROL
			debugger_setcommand("ROLW %s,1",&modrm_param2);
			break;
		case 1: //ROR
			debugger_setcommand("RORW %s,1",&modrm_param2);
			break;
		case 2: //RCL
			debugger_setcommand("RCLW %s,1",&modrm_param2);
			break;
		case 3: //RCR
			debugger_setcommand("RCRW %s,1",&modrm_param2);
			break;
		case 4: //SHL
		case 6: //--- Unknown Opcode! --- Undocumented opcode!
			debugger_setcommand("SHLW %s,1",&modrm_param2);
			break;
		case 5: //SHR
			debugger_setcommand("SHRW %s,1",&modrm_param2);
			break;
		case 7: //SAR
			debugger_setcommand("SAR %s,1",&modrm_param2);
			break;
		default:
			break;
		}
	}
	modrm_write16(&params,1,op_grp2_16(1),0);
}
void CPU8086_OPD2() //GRP2 Eb,CL
{
	MODRM_src0 = 1;
	modrm_readparams(&params, 0, 0);
	reg = MODRM_REG(params.modrm);
	oper1b = modrm_read8(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger8(&params,0,1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //ROL
			debugger_setcommand("ROLB %s,CL",&modrm_param2);
			break;
		case 1: //ROR
			debugger_setcommand("RORB %s,CL",&modrm_param2);
			break;
		case 2: //RCL
			debugger_setcommand("RCLB %s,CL",&modrm_param2);
			break;
		case 3: //RCR
			debugger_setcommand("RCRB %s,CL",&modrm_param2);
			break;
		case 4: //SHL
		case 6: //--- Unknown Opcode! --- Undocumented opcode!
			debugger_setcommand("SHLB %s,CL",&modrm_param2);
			break;
		case 5: //SHR
			debugger_setcommand("SHRB %s,CL",&modrm_param2);
			break;
		case 7: //SAR
			debugger_setcommand("SARB %s,CL",&modrm_param2);
			break;
		default:
			break;
		}
	}
	modrm_write8(&params,1,op_grp2_8(REG_CL));
}
void CPU8086_OPD3() //GRP2 Ev,CL
{
	MODRM_src0 = 1;
	modrm_readparams(&params, 1, 0);
	reg = MODRM_REG(params.modrm);
	oper1 = modrm_read16(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger16(&params,0,1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //ROL
			debugger_setcommand("ROLW %s,CL",&modrm_param2);
			break;
		case 1: //ROR
			debugger_setcommand("RORW %s,CL",&modrm_param2);
			break;
		case 2: //RCL
			debugger_setcommand("RCLW %s,CL",&modrm_param2);
			break;
		case 3: //RCR
			debugger_setcommand("RCRW %s,CL",&modrm_param2);
			break;
		case 4: //SHL
			debugger_setcommand("SHLW %s,CL",&modrm_param2);
			break;
		case 5: //SHR
			debugger_setcommand("SHRW %s,CL",&modrm_param2);
			break;
		case 6: //--- Unknown Opcode! ---
			debugger_setcommand("<UNKNOWN MODR/M: GRP2(w) /6, CL>");
			break;
		case 7: //SAR
			debugger_setcommand("SARW %s,CL",&modrm_param2);
			break;
		default:
			break;
		}
	}
	modrm_write16(&params,1,op_grp2_16(REG_CL),0);
}


extern byte immb; //For CPU_readOP result!
extern word immw; //For CPU_readOPw result!
void CPU8086_OPF6() //GRP3a Eb
{
	MODRM_src0 = 1;
	modrm_readparams(&params, 0, 0);
	reg = MODRM_REG(params.modrm);
	oper1b = modrm_read8(&params,1);
	if (MODRM_REG(params.modrm)<2) //TEST?
	{
		immb = CPU_readOP(); //Operand!
	}
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger8(&params,0,1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //TEST modrm8, imm8
		case 1: //--- Undocumented opcode, same as above!
			debugger_setcommand("TESTB %s,%02x",&modrm_param2,immb);
			break;
		case 2: //NOT
			debugger_setcommand("NOTB %s",&modrm_param2);
			break;
		case 3: //NEG
			debugger_setcommand("NEGB %s",&modrm_param2);
			break;
		case 4: //MUL
			debugger_setcommand("MULB %s",&modrm_param2);
			break;
		case 5: //IMUL
			debugger_setcommand("IMULB %s",&modrm_param2);
			break;
		case 6: //DIV
			modrm_generateInstructionTEXT("DIVB",8,0,PARAM_MODRM2);
			break;
		case 7: //IDIV
			modrm_generateInstructionTEXT("IDIVB",8,0,PARAM_MODRM2);
			break;
		default:
			break;
		}
	}
	op_grp3_8();
	if ((MODRM_REG(params.modrm)>1) && (MODRM_REG(params.modrm)<4))
	{
		modrm_write8(&params,1,res8);
	}
}
void CPU8086_OPF7() //GRP3b Ev
{
	MODRM_src0 = 1;
	modrm_readparams(&params, 1, 0);
	reg = MODRM_REG(params.modrm);
	oper1 = modrm_read16(&params,1);
	if (MODRM_REG(params.modrm)<2) //TEST has an operand?
	{
		immw = CPU_readOPw(); //Operand!
	}
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger16(&params,0,1); //Get src!
		switch (reg) //What function?
		{
		case 0: //TEST modrm16, imm16
		case 1: //--- Undocumented opcode, same as above!
			debugger_setcommand("TESTW %s,%02x",&modrm_param2,immw);
			break;
		case 2: //NOT
			modrm_generateInstructionTEXT("NOTW",16,0,PARAM_MODRM2);
			break;
		case 3: //NEG
			modrm_generateInstructionTEXT("NEGW",16,0,PARAM_MODRM2);
			break;
		case 4: //MUL
			modrm_generateInstructionTEXT("MULW",16,0,PARAM_MODRM2);
			break;
		case 5: //IMUL
			modrm_generateInstructionTEXT("IMULW",16,0,PARAM_MODRM2);
			break;
		case 6: //DIV
			modrm_generateInstructionTEXT("DIVW",16,0,PARAM_MODRM2);
			break;
		case 7: //IDIV
			modrm_generateInstructionTEXT("IDIVW",16,0,PARAM_MODRM2);
			break;
		default:
			break;
		}
	}
	op_grp3_16();
	if ((reg>1) && (reg<4)) //NOT/NEG?
	{
		modrm_write16(&params,1,res16,0);
	}
}
//All OK up till here.

/*

DEBUG: REALLY SUPPOSED TO HANDLE HERE?

*/

void CPU8086_OPFE() //GRP4 Eb
{
	MODRM_src0 = 1;
	byte tempcf;
	modrm_readparams(&params,0,0);
	word cb16;
	modrm_debugger8(&params,0,1);
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //INC
		if (cpudebugger) //Debugger on?
		{
			modrm_generateInstructionTEXT("INCB",8,0,PARAM_MODRM2); //INC!
		}
		tempcf = FLAG_CF;
		res8 = modrm_read8(&params,1)+1;
		flag_add8(modrm_read8(&params,1),1);
		FLAG_CF = tempcf;
		modrm_write8(&params,1,res8);
		break;
	case 1: //DEC
		if (cpudebugger) //Debugger on?
		{
			modrm_generateInstructionTEXT("DECB",8,0,PARAM_MODRM2); //DEC!
		}
		tempcf = FLAG_CF;
		res8 = modrm_read8(&params,1)-1;
		flag_sub8(modrm_read8(&params,1),1);
		FLAG_CF = tempcf;
		modrm_write8(&params,1,res8);
		break;
	case 7: //---: Special: callback handler!
		cb16 = CPU_readOPw(); //Read callback!
		if (cpudebugger) //Debugger on?
		{
			modrm_generateInstructionTEXT("<INTERNAL CALLBACK>",0,cb16,PARAM_IMM16);
		}
		CB_handler(cb16); //Call special handler!
		break;
	default: //Unknown opcode or special?
		break;
	}
}

void CPU8086_OPFF() //GRP5 Ev
{
	MODRM_src0 = 1;
	modrm_readparams(&params, 1, 0);
	reg = MODRM_REG(params.modrm);
	oper1 = modrm_read16(&params,1);
	ea = modrm_offset16(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger16(&params,0,1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //INC modrm8
			modrm_generateInstructionTEXT("INCW",16,0,PARAM_MODRM2); //INC!
			break;
		case 1: //DEC modrm8
			modrm_generateInstructionTEXT("DECW",16,0,PARAM_MODRM2); //DEC!
			break;
		case 2: //CALL
			modrm_generateInstructionTEXT("CALL",16,0,PARAM_MODRM2); //CALL!
			break;
		case 3: //CALL Mp (Read address word and jump there)
			modrm_generateInstructionTEXT("CALL",16,0,PARAM_MODRM2); //Jump to the address pointed here!
			//debugger_setcommand("CALL %04X:%04X",MMU_rw(CPU_SEGMENT_CS,REG_CS,ea,0),MMU_rw(CPU_SEGMENT_CS,REG_CS,ea+2,0)); //Based on CALL Ap
			break;
		case 4: //JMP
			modrm_generateInstructionTEXT("JMP",16,0,PARAM_MODRM2); //JMP to the register!
			break;
		case 5: //JMP Mp
			modrm_generateInstructionTEXT("JMP",16,0,PARAM_MODRM2); //Jump to the address pointed here!
			//debugger_setcommand("JMP %04X:%04X",MMU_rw(CPU_SEGMENT_CS,REG_CS,ea,0),MMU_rw(CPU_SEGMENT_CS,REG_CS,ea+2,0)); //JMP to destination!
			break;
		case 6: //PUSH
			modrm_generateInstructionTEXT("PUSH",16,0,PARAM_MODRM2); //PUSH!
			break;
		case 7: //---
			debugger_setcommand("<UNKNOWN Opcode: GRP5(w) /7>");
			break;
		default:
			break;
		}
	}
	op_grp5();
}

/*

Special stuff for NO COprocessor (8087) present/available (default)!

*/


/*void FPU8087_OPDBE3(){debugger_setcommand("<UNKOP8087: FNINIT>");}

void FPU8087_OPDB()
{byte subOP = CPU_readOP(); CPUPROT1 word oldCS = REG_CS; word oldIP = REG_IP; if (subOP==0xE3){FPU8087_OPDBE3();} else{REG_CS = oldCS; REG_IP = oldIP; FPU8087_noCOOP();} CPUPROT2 }
void FPU8087_OPDFE0(){debugger_setcommand("<UNKOP8087: FNINIT>");}
void FPU8087_OPDF(){CPUPROT1 byte subOP = CPU_readOP(); CPUPROT1 word oldCS = REG_CS; word oldIP = REG_IP; if (subOP==0xE0){FPU8087_OPDFE0();} else {REG_CS = oldCS; REG_IP = oldIP; FPU8087_noCOOP();} CPUPROT2 CPUPROT2 }
void FPU8087_OPDDslash7(){debugger_setcommand("<UNKOP8087: FNSTSW>");}
void FPU8087_OPDD(){word oldCS; word oldIP; oldCS = REG_CS; oldIP = REG_IP; modrm_readparams(&params,0,0); CPUPROT1 if (MODRM_REG(params.modrm)==7){FPU8087_OPDDslash7();}else {REG_CS = oldCS; REG_IP = oldIP; FPU8087_noCOOP();} CPUPROT2}
void FPU8087_OPD9slash7(){debugger_setcommand("<UNKOP8087: FNSTCW>");}
void FPU8087_OPD9(){word oldCS; word oldIP; oldCS = REG_CS; oldIP = REG_IP; modrm_readparams(&params,0,0); CPUPROT1 if (MODRM_REG(params.modrm)==7){FPU8087_OPD9slash7();} else {REG_CS = oldCS; REG_IP = oldIP; FPU8087_noCOOP();} CPUPROT2}
*/
void FPU8087_noCOOP(){
	debugger_setcommand("<No COprocessor OPcodes implemented!>");
	/*CPU_resetOP(); CPU_COOP_notavailable();*/ //Only on 286+!
	modrm_readparams(&params, 0, 0); //Read params and discard data!
}

void unkOP_8086() //Unknown opcode on 8086?
{
	//dolog("8086","Unknown opcode on 8086: %02X",CPU[activeCPU].lastopcode); //Last read opcode!
}

//Gecontroleerd: 100% OK!

//Now, the GRP opcodes!

byte op_grp2_8(byte cnt) {
	//word d,
	word s, shift, oldCF, msb;
	//if (cnt>0x8) return(oper1b); //80186+ limits shift count
	s = oper1b;
	oldCF = FLAG_CF;
	if (EMULATED_CPU >= CPU_80186) cnt &= 0x1F; //Clear the upper 3 bits to become a 80186+!
	switch (reg) {
	case 0: //ROL r/m8
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80) FLAG_CF = 1; else FLAG_CF = 0;
			s = s << 1;
			s = s | FLAG_CF;
		}
		if (cnt) FLAG_OF = FLAG_CF ^ ((s >> 7) & 1);
		break;

	case 1: //ROR r/m8
		for (shift = 1; shift <= cnt; shift++) {
			FLAG_CF = s & 1;
			s = (s >> 1) | (FLAG_CF << 7);
		}
		if (cnt) FLAG_OF = (s >> 7) ^ ((s >> 6) & 1);
		break;

	case 2: //RCL r/m8
		for (shift = 1; shift <= cnt; shift++) {
			oldCF = FLAG_CF;
			if (s & 0x80) FLAG_CF = 1; else FLAG_CF = 0;
			s = s << 1;
			s = s | oldCF;
		}
		if (cnt) FLAG_OF = FLAG_CF ^ ((s >> 7) & 1);
		break;

	case 3: //RCR r/m8
		for (shift = 1; shift <= cnt; shift++) {
			oldCF = FLAG_CF;
			FLAG_CF = s & 1;
			s = (s >> 1) | (oldCF << 7);
		}
		if (cnt) FLAG_OF = (s >> 7) ^ ((s >> 6) & 1);
		break;

	case 4: case 6: //SHL r/m8
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80) FLAG_CF = 1; else FLAG_CF = 0;
			s = (s << 1) & 0xFF;
		}
		if (cnt) FLAG_OF = (FLAG_CF ^ (s >> 7));
		flag_szp8((uint8_t)(s&0xFF)); break;

	case 5: //SHR r/m8
		if (cnt == 1) FLAG_OF = (s & 0x80) ? 1 : 0; else FLAG_OF = 0;
		for (shift = 1; shift <= cnt; shift++) {
			FLAG_CF = s & 1;
			s = s >> 1;
		}
		flag_szp8((uint8_t)(s & 0xFF)); break;

	case 7: //SAR r/m8
		if (cnt) FLAG_OF = 0;
		msb = s & 0x80;
		for (shift = 1; shift <= cnt; shift++) {
			FLAG_CF = s & 1;
			s = (s >> 1) | msb;
		}
		byte tempSF;
		tempSF = FLAG_SF; //Save the SF!
		flag_szp8((uint8_t)(s & 0xFF)); break;
		if (!cnt) //Nothing done?
		{
			FLAG_SF = tempSF; //We don't update when nothing's done!
		}
	}
	return(s & 0xFF);
}

word op_grp2_16(byte cnt) {
	//uint32_t d,
	uint_32 s, shift, oldCF, msb;
	//if (cnt>0x10) return(oper1); //80186+ limits shift count
	if (EMULATED_CPU >= CPU_80186) cnt &= 0x1F; //Clear the upper 3 bits to become a 80186+!
	s = oper1;
	oldCF = FLAG_CF;
	switch (reg) {
	case 0: //ROL r/m16
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x8000) FLAG_CF = 1; else FLAG_CF = 0;
			s = s << 1;
			s = s | FLAG_CF;
		}
		if (cnt) FLAG_OF = FLAG_CF ^ ((s >> 15) & 1);
		break;

	case 1: //ROR r/m16
		for (shift = 1; shift <= cnt; shift++) {
			FLAG_CF = s & 1;
			s = (s >> 1) | (FLAG_CF << 15);
		}
		if (cnt) FLAG_OF = (s >> 15) ^ ((s >> 14) & 1);
		break;

	case 2: //RCL r/m16
		for (shift = 1; shift <= cnt; shift++) {
			oldCF = FLAG_CF;
			if (s & 0x8000) FLAG_CF = 1; else FLAG_CF = 0;
			s = s << 1;
			s = s | oldCF;
			//oldCF = ((s&0x8000)>>15)&1; //Save FLAG_CF!
			//s = (s<<1)+FLAG_CF;
			//FLAG_CF = oldCF;
		}
		if (cnt) FLAG_OF = FLAG_CF ^ ((s >> 15) & 1);
		break;

	case 3: //RCR r/m16
		if (cnt) FLAG_OF = ((s >> 15) & 1) ^ FLAG_CF;
		for (shift = 1; shift <= cnt; shift++) {
			oldCF = FLAG_CF;
			FLAG_CF = s & 1;
			s = (s >> 1) | (oldCF << 15);
			//oldCF = s&1;
			//s = (s<<1)+(FLAG_CF<<16);
			//FLAG_CF = oldCF;
		}
		if (cnt) FLAG_OF = (s >> 15) ^ ((s >> 14) & 1);
		break;

	case 4: case 6: //SHL r/m16
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x8000) FLAG_CF = 1; else FLAG_CF = 0;
			s = (s << 1) & 0xFFFF;
		}
		if ((cnt) && (FLAG_CF == (s >> 15))) FLAG_OF = 0; else FLAG_OF = 1;
		flag_szp16(s); break;

	case 5: //SHR r/m16
		if (cnt) FLAG_OF = (s & 0x8000) ? 1 : 0;
		for (shift = 1; shift <= cnt; shift++) {
			FLAG_CF = s & 1;
			s = s >> 1;
		}
		flag_szp16(s); break;

	case 7: //SAR r/m16
		if (cnt) FLAG_OF = 0;
		msb = s & 0x8000; //Read the MSB!
		for (shift = 1; shift <= cnt; shift++) {
			FLAG_CF = s & 1;
			s = (s >> 1) | msb;
		}
		byte tempSF;
		tempSF = FLAG_SF; //Save the SF!
		flag_szp16(s);
		if (!cnt) //Nothing done?
		{
			FLAG_SF = tempSF; //We don't update when nothing's done!
		}
		break;
	}
	return(s & 0xFFFF);
}

OPTINLINE void op_div8(word valdiv, byte divisor) {
	if (!divisor) { CPU_exDIV0(); return; }
	if ((valdiv / (word)divisor) > 0xFF) { CPU_exDIV0(); return; }
	REG_AH = valdiv % (word)divisor;
	REG_AL = valdiv / (word)divisor;
}

OPTINLINE void op_idiv8(word valdiv, byte divisor) {
	//word v1, v2,
	if (divisor == 0) { CPU_exDIV0(); return; }
	/*
	word s1, s2, d1, d2;
	int sign;
	s1 = valdiv;
	s2 = divisor;
	sign = (((s1 ^ s2) & 0x8000) != 0);
	s1 = (s1 < 0x8000) ? s1 : ((~s1 + 1) & 0xffff);
	s2 = (s2 < 0x8000) ? s2 : ((~s2 + 1) & 0xffff);
	d1 = s1 / s2;
	d2 = s1 % s2;
	if (d1 & 0xFF00) { CPU_exDIV0(); return; }
	if (sign) {
	d1 = (~d1 + 1) & 0xff;
	d2 = (~d2 + 1) & 0xff;
	}
	REG_AH = d2;
	REG_AL = d1;
	*/

	//Same, but with normal instructions!
	union
	{
		word valdivw;
		sword valdivs;
	} dataw1, //For loading the signed value of the registers!
		dataw2; //For performing calculations!

	union
	{
		byte divisorb;
		sbyte divisors;
	} datab1, //For loading the data
		datab2; //For loading the result and test it against overflow!
				//For converting the data to signed values!

	dataw1.valdivw = valdiv; //Load word!
	datab1.divisorb = divisor; //Load divisor!

	dataw2.valdivs = dataw1.valdivs; //Set and...
	dataw2.valdivs /= datab1.divisors; //... Divide!

	datab2.divisors = (sbyte)dataw2.valdivs; //Try to load the signed result!
	if (datab2.divisors != dataw2.valdivs) { CPU_exDIV0(); return; } //Overflow (data loss)!

	REG_AL = datab2.divisors; //Divided!
	dataw2.valdivs = dataw1.valdivs; //Reload and...
	dataw2.valdivs %= datab1.divisors; //... Modulo!
	datab1.divisors = (sbyte)dataw2.valdivs; //Convert to 8-bit!
	REG_AH = datab1.divisorb; //Move rest into result!

							  //if (valdiv > 32767) v1 = valdiv - 65536; else v1 = valdiv;
							  //if (divisor > 127) v2 = divisor - 256; else v2 = divisor;
							  //v1 = valdiv;
							  //v2 = signext(divisor);
							  //if ((v1/v2) > 255) { CPU8086_int(0); return; }
							  //regs.byteregs[regal] = (v1/v2) & 255;
							  //regs.byteregs[regah] = (v1 % v2) & 255;
}

byte tmps,tmpp; //Sign/parity backup!

extern byte CPU_databussize; //Current data bus size!

void op_grp3_8() {
	//uint32_t d1, d2, s1, s2, sign;
	//word d, s;
	oper1 = signext(oper1b); oper2 = signext(oper2b);
	switch (reg) {
	case 0: case 1: //TEST
		flag_log8(oper1b & immb);
		break;

	case 2: //NOT
		res8 = ~oper1b; break;

	case 3: //NEG
		res8 = (~oper1b) + 1;
		flag_sub8(0, oper1b);
		if (res8 == 0) FLAG_CF = 0; else FLAG_CF = 1;
		break;

	case 4: //MULB
		temp1.val32 = (uint32_t)oper1b * (uint32_t)REG_AL;
		REG_AX = temp1.val16 & 0xFFFF;
		if ((EMULATED_CPU==CPU_8086) && REG_AX) FLAG_ZF = 0; //8086/8088 clear Zero flag when not zero only.
		FLAG_CF = FLAG_OF = ((word)REG_AL != REG_AX)?1:0;
		break;

	case 5: //IMULB
		oper1 = oper1b;
		temp1.val32 = REG_AL;
		temp2.val32 = oper1b;
		//Sign extend!
		if ((temp1.val8 & 0x80) == 0x80) temp1.val32 |= 0xFFFFFF00;
		if ((temp2.val8 & 0x80) == 0x80) temp2.val32 |= 0xFFFFFF00;
		//Multiply and convert to 16-bit!
		temp3.val32s = temp1.val32s; //Load and...
		temp3.val32s *= temp2.val32s; //Multiply!
		REG_AX = temp3.val16; //Load into AX!
		FLAG_CF = FLAG_OF = ((sword)(unsigned2signed8(REG_AL) != unsigned2signed16(REG_AX)) ? 1 : 0);
		break;

	case 6: //DIV
		op_div8(REG_AX, oper1b);
		break;

	case 7: //IDIV
		op_idiv8(REG_AX, oper1b);
		break;
	}
}

OPTINLINE void op_div16(uint32_t valdiv, word divisor) {
	//word v1, v2;
	if (!divisor) { CPU_exDIV0(); return; }
	if ((valdiv / (uint32_t)divisor) > 0xFFFF) { CPU_exDIV0(); return; }
	REG_DX = valdiv % (uint32_t)divisor;
	REG_AX = valdiv / (uint32_t)divisor;
}

OPTINLINE void op_idiv16(uint32_t valdiv, word divisor) {
	//uint32_t v1, v2,

	if (!divisor) { CPU_exDIV0(); return; }
	/*
	uint_32 d1, d2, s1, s2;
	int sign;
	s1 = valdiv;
	s2 = divisor;
	s2 = (s2 & 0x8000) ? (s2 | 0xffff0000) : s2;
	sign = (((s1 ^ s2) & 0x80000000) != 0);
	s1 = (s1 < 0x80000000) ? s1 : ((~s1 + 1) & 0xffffffff);
	s2 = (s2 < 0x80000000) ? s2 : ((~s2 + 1) & 0xffffffff);
	d1 = s1 / s2;
	d2 = s1 % s2;
	if (d1 & 0xFFFF0000) { CPU_exDIV0(); return; }
	if (sign) {
	d1 = (~d1 + 1) & 0xffff;
	d2 = (~d2 + 1) & 0xffff;
	}
	REG_AX = d1;
	REG_DX = d2;
	*/

	//Same, but with normal instructions!
	union
	{
		uint_32 valdivw;
		int_32 valdivs;
	} dataw1, //For loading the signed value of the registers!
		dataw2; //For performing calculations!

	union
	{
		word divisorb;
		sword divisors;
	} datab1, datab2; //For converting the data to signed values!

	dataw1.valdivw = valdiv; //Load word!
	datab1.divisorb = divisor; //Load divisor!

	dataw2.valdivs = dataw1.valdivs; //Set and...
	dataw2.valdivs /= datab1.divisors; //... Divide!

	datab2.divisors = (sword)dataw2.valdivs; //Try to load the signed result!
	if ((int_32)dataw2.valdivw != (int_32)datab2.divisors) { CPU_exDIV0(); return; } //Overflow (data loss)!

	REG_AX = datab2.divisorb; //Divided!
	dataw2.valdivs = dataw1.valdivs; //Reload and...
	dataw2.valdivs %= datab1.divisors; //... Modulo!
	datab1.divisors = (sword)dataw2.valdivs; //Convert to 8-bit!
	REG_DX = datab1.divisorb; //Move rest into result!

							  //if (valdiv > 0x7FFFFFFF) v1 = valdiv - 0xFFFFFFFF - 1; else v1 = valdiv;
							  //if (divisor > 32767) v2 = divisor - 65536; else v2 = divisor;
							  //if ((v1/v2) > 65535) { CPU8086_int(0); return; }
							  //temp3 = (v1/v2) & 65535;
							  //regs.wordregs[regax] = temp3;
							  //temp3 = (v1%v2) & 65535;
							  //regs.wordregs[regdx] = temp3;
}

void op_grp3_16() {
	//uint32_t d1, d2, s1, s2, sign;
	//word d, s;
	//oper1 = signext(oper1b); oper2 = signext(oper2b);
	//sprintf(msg, "  Oper1: %04X    Oper2: %04X\n", oper1, oper2); print(msg);
	switch (reg) {
	case 0: case 1: //TEST
		flag_log16(oper1 & immw); break;
	case 2: //NOT
		res16 = ~oper1; break;
	case 3: //NEG
		res16 = (~oper1) + 1;
		flag_sub16(0, oper1);
		if (res16) FLAG_CF = 1; else FLAG_CF = 0;
		break;
	case 4: //MULW
		temp1.val32 = (uint32_t)oper1 * (uint32_t)REG_AX;
		REG_AX = temp1.val16;
		REG_DX = temp1.val16high;
		if ((EMULATED_CPU==CPU_8086) && temp1.val32) FLAG_ZF = 0; //8086/8088 clear Zero flag when not zero only.
		if (REG_DX) { FLAG_CF = FLAG_OF = 1; }
		else { FLAG_CF = FLAG_OF = 0; }
		break;
	case 5: //IMULW
		temp1.val32 = REG_AX;
		temp2.val32 = oper1;
		//Sign extend!
		if (temp1.val16 & 0x8000) temp1.val32 |= 0xFFFF0000;
		if (temp2.val16 & 0x8000) temp2.val32 |= 0xFFFF0000;
		temp3.val32s = temp1.val32s; //Load and...
		temp3.val32s *= temp2.val32s; //Signed multiplication!
		REG_AX = temp3.val16; //into register ax
		REG_DX = temp3.val16high; //into register dx
		FLAG_CF = FLAG_OF = ((int_32)temp3.val16s != temp3.val32s)?1:0; //Overflow occurred?
		break;
	case 6: //DIV
		op_div16(((uint32_t)REG_DX << 16) | REG_AX, oper1); break;
	case 7: //IDIV
		op_idiv16(((uint32_t)REG_DX << 16) | REG_AX, oper1); break;
	}
}

void op_grp5() {
	MODRM_PTR info; //To contain the info!
	switch (reg) {
	case 0: //INC Ev
		oper2 = 1;
		tempCF = FLAG_CF;
		op_add16();
		FLAG_CF = tempCF;
		modrm_write16(&params, 1, res16, 0); break;
	case 1: //DEC Ev
		oper2 = 1;
		tempCF = FLAG_CF;
		op_sub16();
		FLAG_CF = tempCF;
		modrm_write16(&params, 1, res16, 0); break;
	case 2: //CALL Ev
		CPU_PUSH16(&REG_IP);
		REG_IP = oper1;
		CPU_flushPIQ(); //We're jumping to another address!
		break;
	case 3: //CALL Mp
		CPU_PUSH16(&REG_CS); CPU_PUSH16(&REG_IP);
		modrm_decode16(&params, &info, 1); //Get data!
		destEIP = MMU_rw(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset, 0);
		segmentWritten(CPU_SEGMENT_CS, MMU_rw(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset + 2, 0), 2);
		break;
	case 4: //JMP Ev
		REG_IP = oper1;
		CPU_flushPIQ(); //We're jumping to another address!
		break;
	case 5: //JMP Mp
		modrm_decode16(&params, &info, 1); //Get data!
		destEIP = MMU_rw(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset, 0);
		segmentWritten(CPU_SEGMENT_CS, MMU_rw(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset + 2, 0), 1);
		break;
	case 6: //PUSH Ev
		CPU_PUSH16(&oper1); break;
	}
}
