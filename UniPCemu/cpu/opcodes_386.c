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
extern byte immb;
extern word immw;
extern uint_32 imm32;

#include "headers/types.h" //Basic types
#include "headers/cpu/cpu.h" //CPU needed!
#include "headers/mmu/mmu.h" //MMU needed!
#include "headers/cpu/easyregs.h" //Easy register compatibility!
#include "headers/cpu/modrm.h" //MODR/M compatibility!
#include "headers/support/signedness.h" //CPU support functions!
#include "headers/hardware/ports.h" //Ports compatibility!
#include "headers/cpu/cpu_OP8086.h" //Our own opcode presets!
#include "headers/cpu/fpu_OP8087.h" //Our own opcode presets!
#include "headers/cpu/flags.h" //Flag support!
#include "headers/cpu/8086_grpOPs.h" //GRP Opcode extensions!
#include "headers/cpu/interrupts.h" //Basic interrupt support!
#include "headers/emu/debugger/debugger.h" //CPU debugger support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/cpu/protection.h"

void op_grp3_32();
uint_32 op_grp2_32(byte cnt, byte varshift);
void op_grp5_32();
MODRM_PARAMS params; //For getting all params for the CPU!
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
extern uint_64 imm64; //For 32-bit big pointers!
uint_32 oper1d, oper2d; //DWord variants!
uint_32 res32;
extern byte thereg; //For function number!
extern uint_32 ea; //From RM OFfset (GRP5 Opcodes only!)
extern byte tempCF2;

VAL64Splitter temp1d, temp2d, temp3d, temp4d, temp5d; //All temporary values!
extern uint_32 temp32, tempaddr32; //Defined in opcodes_8086.c

extern byte debuggerINT; //Interrupt special trigger?

extern byte CPU_databussize; //0=16/32-bit bus! 1=8-bit bus when possible (8088/80188)!
uint_32 wordaddress; //Word address used during memory access!

OPTINLINE void CPU386_addDWordMemoryTiming()
{
	if (CPU_databussize) //8088?
	{
		CPU[activeCPU].cycles_OP += 4; //Add 4 clocks with all 16-bit cycles on 8086!
	}
	else //8086?
	{
		if (wordaddress & 1) //Odd address?
		{
			CPU[activeCPU].cycles_OP += 4; //Add 4 clocks with odd cycles on 8086!
		}
	}
}

OPTINLINE void INTdebugger80386() //Special INTerrupt debugger!
{
	if (DEBUGGER_LOG == DEBUGGERLOG_INT) //Interrupts only?
	{
		debuggerINT = 1; //Debug this instruction always!
	}
}

/*

Start of help for debugging

*/

extern char modrm_param1[256]; //Contains param/reg1
extern char modrm_param2[256]; //Contains param/reg2

void modrm_debugger32(MODRM_PARAMS *theparams, byte whichregister1, byte whichregister2) //32-bit handler!
{
	if (cpudebugger)
	{
		bzero(modrm_param1, sizeof(modrm_param1));
		bzero(modrm_param2, sizeof(modrm_param2));
		modrm_text32(theparams, whichregister1, &modrm_param1[0]);
		modrm_text32(theparams, whichregister2, &modrm_param2[0]);
	}
}

OPTINLINE byte NumberOfSetBits(uint_32 i)
{
	// Java: use >>> instead of >>
	// C or C++: use uint32_t
	i = i - ((i >> 1) & 0x55555555);
	i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
	return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

/*

modrm_generateInstructionTEXT: Generates text for an instruction into the debugger.
parameters:
instruction: The instruction ("ADD","INT",etc.)
debuggersize: Size of the debugger, if any (8/16/32/0 for none).
paramdata: The params to use when debuggersize set and using modr/m with correct type.
type: See above.

*/

OPTINLINE void modrm_generateInstructionTEXT386(char *instruction, byte debuggersize, uint_32 paramdata, byte type)
{
	if (cpudebugger) //Gotten no debugger to process?
	{
		//Process debugger!
		char result[256];
		bzero(result, sizeof(result));
		strcpy(result, instruction); //Set the instruction!
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
				modrm_debugger8(&params, 0, 1);
				break;
			case 16:
				modrm_debugger16(&params, 0, 1);
				break;
			case 32:
				modrm_debugger32(&params, 0, 1);
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
			strcat(result, " %s"); //1 param!
			debugger_setcommand(result, modrm_param1);
			break;
		case PARAM_MODRM2: //Param2 only?
			strcat(result, " %s"); //1 param!
			debugger_setcommand(result, modrm_param2);
			break;
		case PARAM_MODRM12: //param1,param2
			strcat(result, " %s,%s"); //2 params!
			debugger_setcommand(result, modrm_param1, modrm_param2);
			break;
		case PARAM_MODRM21: //param2,param1
			strcat(result, " %s,%s"); //2 params!
			debugger_setcommand(result, modrm_param2, modrm_param1);
			break;
		case PARAM_IMM8: //imm8
			strcat(result, " %02X"); //1 param!
			debugger_setcommand(result, paramdata);
			break;
		case PARAM_IMM16: //imm16
			strcat(result, " %04X"); //1 param!
			debugger_setcommand(result, paramdata);
			break;
		case PARAM_IMM32: //imm32
			strcat(result, " %08X"); //1 param!
			debugger_setcommand(result, paramdata);
		default: //Unknown?
			break;
		}
	}
}

char LEAtext[256];
OPTINLINE char *getLEAtext386(MODRM_PARAMS *theparams)
{
	modrm_lea16_text(theparams, 1, &LEAtext[0]);    //Help function for LEA instruction!
	return &LEAtext[0];
}

/*

Start of help for opcode processing

*/

extern byte CPU_databussize; //0=16/32-bit bus! 1=8-bit bus when possible (8088/80188)!
uint_32 wordaddress; //Word address used during memory access!
OPTINLINE void CPU_addWordMemoryTiming386()
{
	if (CPU_databussize) //8088?
	{
		CPU[activeCPU].cycles_OP += 4; //Add 4 clocks with all 16-bit cycles on 8086!
	}
	else //8086?
	{
		if (wordaddress & 1) //Odd address?
		{
			CPU[activeCPU].cycles_OP += 4; //Add 4 clocks with odd cycles on 8086!
		}
	}
}

OPTINLINE void CPU80386_hardware_int(byte interrupt, byte has_errorcode, uint_32 errorcode) //See int, but for hardware interrupts (IRQs)!
{
	CPU_INT(interrupt); //Save adress to stack (We're going soft int!)!
	if (has_errorcode) //Have error code too?
	{
		CPU_PUSH32(&errorcode); //Push error code on stack!
	}
}

OPTINLINE void CPU80386_int(byte interrupt, byte type3) //Software interrupt from us(internal call)!
{
	CPUPROT1
		CPU80386_hardware_int(interrupt, 0, 0);
	CPUPROT2
		if (type3) //Type-3 interrupt?
			CPU[activeCPU].cycles_OP = 52; /* Type-3 interrupt */
		else //Normal interrupt?
			CPU[activeCPU].cycles_OP = 51; /* Normal interrupt */
	CPU_addWordMemoryTiming386(); /*To memory?*/
	CPU_addWordMemoryTiming386(); /*To memory?*/
	CPU_addWordMemoryTiming386(); /*To memory?*/
	CPU_addWordMemoryTiming386(); /*To memory?*/
	CPU_addWordMemoryTiming386(); /*To memory?*/
}

void CPU386_int(byte interrupt) //Software interrupt (external call)!
{
	CPU80386_int(interrupt, 0); //Direct call!
}

OPTINLINE void CPU80386_IRET()
{
	CPUPROT1
		CPU_IRET(); //IRET!
	CPUPROT2
		CPU[activeCPU].cycles_OP = 24; /*Timings!*/
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


OPTINLINE void op_adc32() {
	res32 = oper1d + oper2d + FLAG_CF;
	flag_adc32(oper1d, oper2d, FLAG_CF);
}

OPTINLINE void op_add32() {
	res32 = oper1d + oper2d;
	flag_add32(oper1d, oper2d);
}

OPTINLINE void op_and32() {
	res32 = oper1d & oper2d;
	flag_log32(res32);
}

OPTINLINE void op_or32() {
	res32 = oper1d | oper2d;
	flag_log32(res32);
}

OPTINLINE void op_xor32() {
	res32 = oper1d ^ oper2d;
	flag_log32(res32);
}

OPTINLINE void op_sub32() {
	res32 = oper1d - oper2d;
	flag_sub32(oper1d, oper2d);
}

OPTINLINE void op_sbb32() {
	res32 = oper1d - (oper2d + FLAG_CF);
	flag_sbb32(oper1d, oper2d, FLAG_CF);
}

OPTINLINE void CMP_d(uint_32 a, uint_32 b, byte flags) //Compare instruction!
{
	CPUPROT1
		flag_sub32(a, b); //Flags only!
	switch (flags & 7)
	{
	case 0: //Default?
		break; //Unused!
	case 1: //Accumulator?
		CPU[activeCPU].cycles_OP = 4; //Imm-Reg
		break;
	case 2: //Determined by ModR/M?
		if (params.EA_cycles) //Memory is used?
		{
			CPU[activeCPU].cycles_OP = 9 + MODRM_EA(params); //Mem->Reg!
			CPU_addWordMemoryTiming386();
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		}
		break;
	case 3: //ModR/M+imm?
		if (params.EA_cycles) //Memory is used?
		{
			CPU[activeCPU].cycles_OP = 10 + MODRM_EA(params); //Mem->Reg!
			CPU_addWordMemoryTiming386();
		}
		else //Imm->Reg?
		{
			CPU[activeCPU].cycles_OP = 4; //Reg->Reg!
		}
		break;
	case 4: //Mem-Mem instruction?
		CPU[activeCPU].cycles_OP = 18; //Assume two times Reg->Mem
		break;
	}
	CPUPROT2
}

//Modr/m support, used when reg=NULL and custommem==0
extern byte MODRM_src0; //What source is our modr/m? (1/2)
extern byte MODRM_src1; //What source is our modr/m? (1/2)

						//Custom memory support!
extern byte custommem; //Used in some instructions!
extern uint_32 customoffset; //Offset to use!

					  //Help functions:
OPTINLINE void CPU80386_internal_INC32(uint_32 *reg)
{
	if (MMU_invaddr() || (reg == NULL))
	{
		return;
	}
	CPUPROT1
		INLINEREGISTER byte tempcf = FLAG_CF;
	oper1d = reg ? *reg : modrm_read32(&params, MODRM_src0);
	oper2d = 1;
	op_add32();
	FLAG_CF = tempcf;
	if (reg) //Register?
	{
		*reg = res32;
		CPU[activeCPU].cycles_OP = 2; //16-bit reg!
	}
	else //Memory?
	{
		modrm_write32(&params, MODRM_src0, res32); //Write the result to memory!
		CPU[activeCPU].cycles_OP = 15 + MODRM_EA(params); //Mem
		CPU_addWordMemoryTiming386();
		CPU_addWordMemoryTiming386();
	}
	CPUPROT2
}
OPTINLINE void CPU80386_internal_DEC32(uint_32 *reg)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
		INLINEREGISTER byte tempcf = FLAG_CF;
	oper1d = reg ? *reg : modrm_read32(&params, MODRM_src0);
	oper2d = 1;
	op_sub32();
	FLAG_CF = tempcf;
	if (reg) //Register?
	{
		*reg = res32;
		CPU[activeCPU].cycles_OP = 2; //16-bit reg!
	}
	else //Memory?
	{
		modrm_write32(&params, MODRM_src0, res32); //Write the result to memory!
		CPU[activeCPU].cycles_OP = 15 + MODRM_EA(params); //Mem
		CPU_addWordMemoryTiming386();
		CPU_addWordMemoryTiming386();
	}
	CPUPROT2
}

OPTINLINE void timing_AND_OR_XOR_ADD_SUB32(uint_32 *dest, byte flags)
{
	switch (flags) //What type of operation?
	{
	case 0: //Reg+Reg?
		CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		break;
	case 1: //Reg+imm?
		CPU[activeCPU].cycles_OP = 4; //Accumulator!
		break;
	case 2: //Determined by ModR/M?
		if (params.EA_cycles) //Memory is used?
		{
			if (dest) //Mem->Reg?
			{
				CPU[activeCPU].cycles_OP = 9 + MODRM_EA(params); //Mem->Reg!
			}
			else //Reg->Mem?
			{
				CPU[activeCPU].cycles_OP = 16 + MODRM_EA(params); //Mem->Reg!
			}
			CPU_addWordMemoryTiming386();
			if (dest == NULL) CPU_addWordMemoryTiming386(); //Second access for writeback!
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		}
		break;
	case 3: //ModR/M+imm?
		if (params.EA_cycles) //Memory is used?
		{
			if (dest) //Imm->Reg?
			{
				CPU[activeCPU].cycles_OP = 4; //Imm->Reg!
			}
			else //Imm->Mem?
			{
				CPU[activeCPU].cycles_OP = 17 + MODRM_EA(params); //Mem->Reg!
			}
			CPU_addWordMemoryTiming386();
			if (dest == NULL) CPU_addWordMemoryTiming386(); //Second access for writeback!
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		}
		break;
	}
}

//For ADD
OPTINLINE void CPU80386_internal_ADD32(uint_32 *dest, uint_32 addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
		oper1d = dest ? *dest : modrm_read32(&params, MODRM_src0);
	oper2d = addition;
	op_add32();
	if (dest) //Register?
	{
		*dest = res32;
	}
	else //Memory?
	{
		modrm_write32(&params, MODRM_src0, res32); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB32(dest, flags);
	CPUPROT2
}

//For ADC
OPTINLINE void CPU80386_internal_ADC32(uint_32 *dest, uint_32 addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
		oper1d = dest ? *dest : modrm_read32(&params, MODRM_src0);
	oper2d = addition;
	op_adc32();
	if (dest) //Register?
	{
		*dest = res32;
	}
	else //Memory?
	{
		modrm_write32(&params, MODRM_src0, res32); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB32(dest, flags);
	CPUPROT2
}


//For OR
OPTINLINE void CPU80386_internal_OR32(uint_32 *dest, uint_32 src, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
		oper1d = dest ? *dest : modrm_read32(&params, MODRM_src0);
	oper2d = src;
	op_or32();
	if (dest) //Register?
	{
		*dest = res32;
	}
	else //Memory?
	{
		modrm_write32(&params, MODRM_src0, res32); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB32(dest, flags);
	CPUPROT2
}

//For AND
OPTINLINE void CPU80386_internal_AND32(uint_32 *dest, uint_32 src, byte flags)
{
	CPUPROT1
		oper1d = dest ? *dest : modrm_read32(&params, MODRM_src0);
	oper2d = src;
	op_and32();
	if (dest) //Register?
	{
		*dest = res32;
	}
	else //Memory?
	{
		modrm_write32(&params, MODRM_src0, res32); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB32(dest, flags);
	CPUPROT2
}


//For SUB
OPTINLINE void CPU80386_internal_SUB32(uint_32 *dest, uint_32 addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
		oper1d = dest ? *dest : modrm_read32(&params, MODRM_src0);
	oper2d = addition;
	op_sub32();
	if (dest) //Register?
	{
		*dest = res32;
	}
	else //Memory?
	{
		modrm_write32(&params, MODRM_src0, res32); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB32(dest, flags);
	CPUPROT2
}

//For SBB
OPTINLINE void CPU80386_internal_SBB32(uint_32 *dest, uint_32 addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
		oper1d = dest ? *dest : modrm_read32(&params, MODRM_src0);
	oper2d = addition;
	op_sbb32();
	if (dest) //Register?
	{
		*dest = res32;
	}
	else //Memory?
	{
		modrm_write32(&params, MODRM_src0, res32); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB32(dest, flags);
	CPUPROT2
}

//For XOR
//See AND, but XOR
OPTINLINE void CPU80386_internal_XOR32(uint_32 *dest, uint_32 src, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
		oper1d = dest ? *dest : modrm_read32(&params, MODRM_src0);
	oper2d = src;
	op_xor32();
	if (dest) //Register?
	{
		*dest = res32;
	}
	else //Memory?
	{
		modrm_write32(&params, MODRM_src0, res32); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB32(dest, flags);
	CPUPROT2
}

//TEST : same as AND, but discarding the result!
OPTINLINE void CPU80386_internal_TEST32(uint_32 dest, uint_32 src, byte flags)
{
	uint_32 tmpdest = dest;
	CPU80386_internal_AND32(&tmpdest, src, 0);
	//Adjust timing for TEST!
	switch (flags) //What type of operation?
	{
	case 0: //Reg+Reg?
		CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		break;
	case 1: //Reg+imm?
		CPU[activeCPU].cycles_OP = 4; //Accumulator!
		break;
	case 2: //Determined by ModR/M?
		if (params.EA_cycles) //Memory is used?
		{
			//Mem->Reg/Reg->Mem?
			CPU[activeCPU].cycles_OP = 9 + MODRM_EA(params); //Mem->Reg!
			CPU_addWordMemoryTiming386();
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		}
		break;
	case 3: //ModR/M+imm?
		if (params.EA_cycles) //Memory is used?
		{
			if (dest) //Imm->Reg?
			{
				CPU[activeCPU].cycles_OP = 5; //Imm->Reg!
			}
			else //Imm->Mem?
			{
				CPU[activeCPU].cycles_OP = 11 + MODRM_EA(params); //Mem->Reg!
				CPU_addWordMemoryTiming386();
			}
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		}
		break;
	}
}

//MOV
OPTINLINE void CPU80386_internal_MOV32(uint_32 *dest, uint_32 val, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
		if (dest) //Register?
		{
			*dest = val;
			switch (flags) //What type are we?
			{
			case 0: //Reg+Reg?
				break; //Unused!
			case 1: //Accumulator from immediate memory address?
				CPU[activeCPU].cycles_OP = 10; //[imm16]->Accumulator!
				CPU_addWordMemoryTiming386(); //To memory?
				break;
			case 2: //ModR/M Memory->Reg?
				if (MODRM_EA(params)) //Memory?
				{
					CPU[activeCPU].cycles_OP = 8 + MODRM_EA(params); //Mem->Reg!
					if (!dest) CPU_addWordMemoryTiming386(); //To memory?
				}
				else //Reg->Reg?
				{
					CPU[activeCPU].cycles_OP = 2; //Reg->Reg!
				}
				break;
			case 3: //ModR/M Memory immediate->Reg?
				if (MODRM_EA(params)) //Memory?
				{
					CPU[activeCPU].cycles_OP = 10 + MODRM_EA(params); //Mem->Reg!
					CPU_addWordMemoryTiming386();
					if (!dest) CPU_addWordMemoryTiming386(); //To memory?
				}
				else //Reg->Reg?
				{
					CPU[activeCPU].cycles_OP = 2; //Reg->Reg!
				}
				break;
			case 4: //Register immediate->Reg?
				CPU[activeCPU].cycles_OP = 4; //Reg->Reg!
				break;
			case 8: //SegReg->Reg?
				if (MODRM_src0 || (MODRM_EA(params) == 0)) //From register?
				{
					CPU[activeCPU].cycles_OP = 2; //Reg->SegReg!
				}
				else //From memory?
				{
					CPU[activeCPU].cycles_OP = 8 + MODRM_EA(params); //Mem->SegReg!
					if (!dest) CPU_addWordMemoryTiming386(); //To memory?
				}
				break;
			}
		}
		else //Memory?
		{
			if (custommem)
			{
				MMU_wdw(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), customoffset, val); //Write to memory directly!
				CPU[activeCPU].cycles_OP = 10; //Accumulator->[imm16]!
				CPU_addWordMemoryTiming386();
			}
			else //ModR/M?
			{
				modrm_write32(&params, MODRM_src0, val); //Write the result to memory!
				switch (flags) //What type are we?
				{
				case 0: //Reg+Reg?
					break; //Unused!
				case 1: //Accumulator from immediate memory address?
					CPU[activeCPU].cycles_OP = 10; //Accumulator->[imm16]!
					break;
				case 2: //ModR/M Memory->Reg?
					if (MODRM_EA(params)) //Memory?
					{
						CPU[activeCPU].cycles_OP = 9 + MODRM_EA(params); //Mem->Reg!
						if (!dest) CPU_addWordMemoryTiming386(); //To memory?
					}
					else //Reg->Reg?
					{
						CPU[activeCPU].cycles_OP = 2; //Reg->Reg!
					}
					break;
				case 3: //ModR/M Memory immediate->Reg?
					if (MODRM_EA(params)) //Memory?
					{
						CPU[activeCPU].cycles_OP = 10 + MODRM_EA(params); //Mem->Reg!
						if (!dest) CPU_addWordMemoryTiming386(); //To memory?
					}
					else //Reg->Reg?
					{
						CPU[activeCPU].cycles_OP = 4; //Reg->Reg!
					}
					break;
				case 4: //Register immediate->Reg (Non-existant!!!)?
					CPU[activeCPU].cycles_OP = 4; //Reg->Reg!
					break;
				case 8: //Reg->SegReg?
					if (MODRM_src0 || (MODRM_EA(params) == 0)) //From register?
					{
						CPU[activeCPU].cycles_OP = 2; //SegReg->Reg!
					}
					else //From memory?
					{
						CPU[activeCPU].cycles_OP = 9 + MODRM_EA(params); //SegReg->Mem!
						if (!dest) CPU_addWordMemoryTiming386(); //To memory?
					}
					break;
				}
			}
		}
	CPUPROT2
}

//LEA for LDS, LES
OPTINLINE uint_32 getLEA386(MODRM_PARAMS *theparams)
{
	return modrm_lea32(theparams, 1);
}

/*

Non-logarithmic opcodes!

*/

OPTINLINE void CPU80386_internal_CDQ()
{
	CPUPROT1
		if ((REG_EAX & 0x80) == 0x80)
		{
			REG_EAX |= 0xFFFF0000;
		}
		else
		{
			REG_EAX &= 0xFFFF;
		}
	CPU[activeCPU].cycles_OP = 5; //Clock cycles!
	CPUPROT2
}

//Now the repeatable instructions!

extern byte newREP; //Are we a new repeating instruction (REP issued for a new instruction, not repeating?)

OPTINLINE void CPU80386_internal_MOVSD()
{
	INLINEREGISTER uint_32 data;
	if (blockREP) return; //Disabled REP!
	data = MMU_rdw(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_ESI, 0); //Try to read the data!
	CPUPROT1
		MMU_ww(CPU_SEGMENT_ES, REG_ES, REG_EDI, data); //Try to write the data!
	CPUPROT1
		if (FLAG_DF)
		{
			REG_ESI -= 4;
			REG_EDI -= 4;
		}
		else
		{
			REG_ESI += 4;
			REG_EDI += 4;
		}
	CPUPROT2
		CPUPROT2
		if (CPU[activeCPU].repeating) //Are we a repeating instruction?
		{
			if (newREP) //Include the REP?
			{
				CPU[activeCPU].cycles_OP = 9 + 17; //Clock cycles including REP!
			}
			else //Repeating instruction itself?
			{
				CPU[activeCPU].cycles_OP = 17; //Clock cycles excluding REP!
			}
		}
		else //Plain non-repeating instruction?
		{
			CPU[activeCPU].cycles_OP = 18; //Clock cycles!
		}
	CPU_addWordMemoryTiming386(); //To memory?
	CPU_addWordMemoryTiming386(); //To memory?
}

OPTINLINE void CPU80386_internal_CMPSD()
{
	INLINEREGISTER uint_32 data1, data2;
	if (blockREP) return; //Disabled REP!
	data1 = MMU_rdw(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_ESI, 0); //Try to read the first data!
	CPUPROT1
		data2 = MMU_rdw(CPU_SEGMENT_ES, REG_ES, REG_EDI, 0); //Try to read the second data!
	CPUPROT1
		CMP_d(data1, data2, 4);
	if (FLAG_DF)
	{
		REG_ESI -= 4;
		REG_EDI -= 4;
	}
	else
	{
		REG_ESI += 4;
		REG_EDI += 4;
	}
	CPUPROT2
		CPUPROT2
		if (CPU[activeCPU].repeating) //Are we a repeating instruction?
		{
			if (newREP) //Include the REP?
			{
				CPU[activeCPU].cycles_OP = 9 + 22; //Clock cycles including REP!
			}
			else //Repeating instruction itself?
			{
				CPU[activeCPU].cycles_OP = 22; //Clock cycles excluding REP!
			}
		}
		else //Plain non-repeating instruction?
		{
			CPU[activeCPU].cycles_OP = 22; //Clock cycles!
		}
	CPU_addWordMemoryTiming386(); //To memory?
	CPU_addWordMemoryTiming386(); //To memory?
}

OPTINLINE void CPU80386_internal_STOSD()
{
	if (blockREP) return; //Disabled REP!
	MMU_wdw(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_EDI, REG_EAX);
	CPUPROT1
		if (FLAG_DF)
		{
			REG_EDI -= 4;
		}
		else
		{
			REG_EDI += 4;
		}
	CPUPROT2
		if (CPU[activeCPU].repeating) //Are we a repeating instruction?
		{
			if (newREP) //Include the REP?
			{
				CPU[activeCPU].cycles_OP = 9 + 10; //Clock cycles including REP!
			}
			else //Repeating instruction itself?
			{
				CPU[activeCPU].cycles_OP = 10; //Clock cycles excluding REP!
			}
		}
		else //Plain non-repeating instruction?
		{
			CPU[activeCPU].cycles_OP = 11; //Clock cycles!
		}
	CPU_addWordMemoryTiming386(); //To memory?
}
//OK so far!
OPTINLINE void CPU80386_internal_LODSD()
{
	INLINEREGISTER uint_32 value;
	if (blockREP) return; //Disabled REP!
	value = MMU_rdw(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_ESI, 0); //Try to read the result!
	CPUPROT1
		REG_EAX = value;
	if (FLAG_DF)
	{
		REG_ESI -= 4;
	}
	else
	{
		REG_ESI += 4;
	}
	CPUPROT2
		if (CPU[activeCPU].repeating) //Are we a repeating instruction?
		{
			if (newREP) //Include the REP?
			{
				CPU[activeCPU].cycles_OP = 9 + 13; //Clock cycles including REP!
			}
			else //Repeating instruction itself?
			{
				CPU[activeCPU].cycles_OP = 13; //Clock cycles excluding REP!
			}
		}
		else //Plain non-repeating instruction?
		{
			CPU[activeCPU].cycles_OP = 12; //Clock cycles!
		}
	CPU_addWordMemoryTiming386(); //To memory?
}
OPTINLINE void CPU80386_internal_SCASD()
{
	INLINEREGISTER uint_32 cmp1;
	if (blockREP) return; //Disabled REP!
	cmp1 = MMU_rdw(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_EDI, 0); //Try to read the data to compare!
	CPUPROT1
		CMP_d(REG_EAX, cmp1, 4);
	if (FLAG_DF)
	{
		REG_EDI -= 4;
	}
	else
	{
		REG_EDI += 4;
	}
	CPUPROT2
		if (CPU[activeCPU].repeating) //Are we a repeating instruction?
		{
			if (newREP) //Include the REP?
			{
				CPU[activeCPU].cycles_OP = 9 + 15; //Clock cycles including REP!
			}
			else //Repeating instruction itself?
			{
				CPU[activeCPU].cycles_OP = 15; //Clock cycles excluding REP!
			}
		}
		else //Plain non-repeating instruction?
		{
			CPU[activeCPU].cycles_OP = 15; //Clock cycles!
		}
	CPU_addWordMemoryTiming386(); //To memory?
}

OPTINLINE void CPU80386_internal_RET(word popbytes, byte isimm)
{
	INLINEREGISTER word val = CPU_POP32();    //Near return
	CPUPROT1
		REG_EIP = val;
	CPU_flushPIQ(); //We're jumping to another address!
	REG_ESP += popbytes;
	CPUPROT2
		if (isimm)
			CPU[activeCPU].cycles_OP = 12; /* Intrasegment with constant */
		else
			CPU[activeCPU].cycles_OP = 8; /* Intrasegment */
	CPU_addWordMemoryTiming386(); //To memory?
}
OPTINLINE void CPU80386_internal_RETF(word popbytes, byte isimm)
{
	INLINEREGISTER uint_32 val = CPU_POP32();    //Far return
	CPUPROT1
		destEIP = val; //Load IP!
	segmentWritten(CPU_SEGMENT_CS, CPU_POP16(), 2); //CS changed!
	CPUPROT1
		REG_ESP += popbytes; //Process SP!
	CPUPROT2
		CPUPROT2
		if (isimm)
			CPU[activeCPU].cycles_OP = 17; /* Intersegment with constant */
		else
			CPU[activeCPU].cycles_OP = 18; /* Intersegment */
	CPU_addWordMemoryTiming386(); //To memory?
	CPU_addWordMemoryTiming386(); //To memory?
}

void external80386RETF(word popbytes)
{
	CPU80386_internal_RETF(popbytes, 1); //Return immediate variant!
}

OPTINLINE void CPU80386_internal_INTO()
{
	CPUPROT1
		if (FLAG_OF)
		{
			CPU80386_int(EXCEPTION_OVERFLOW, 0);
			CPU[activeCPU].cycles_OP = 53; //Timings!
		}
		else
		{
			CPU[activeCPU].cycles_OP = 4; //Timings!
		}
	CPUPROT2
		CPU_addWordMemoryTiming386(); //To memory?
	CPU_addWordMemoryTiming386(); //To memory?
	CPU_addWordMemoryTiming386(); //To memory?
	CPU_addWordMemoryTiming386(); //To memory?
	CPU_addWordMemoryTiming386(); //To memory?
}

OPTINLINE void CPU80386_internal_XCHG32(uint_32 *data1, uint_32 *data2, byte flags)
{
	CPUPROT1
		oper1d = data1 ? *data1 : modrm_read32(&params, MODRM_src0);
	CPUPROT1
		oper2d = data2 ? *data2 : modrm_read32(&params, MODRM_src1);
	CPUPROT1
		//Do a simple swap!
		uint_32 temp = oper1d; //Copy!
	oper1d = oper2d; //We're ...
	oper2d = temp; //Swapping this!
	if (data1)
	{
		*data1 = oper1d;
	}
	else
	{
		modrm_write32(&params, MODRM_src0, oper1d);
	}
	CPUPROT1
		if (data2)
		{
			*data2 = oper2d;
		}
		else
		{
			modrm_write32(&params, MODRM_src1, oper2d);
		}
	CPUPROT2
		CPUPROT2
		CPUPROT2
		CPUPROT2
		switch (flags)
		{
		case 0: //Unknown?
			break;
		case 1: //Acc<->Reg?
			CPU[activeCPU].cycles_OP = 3; //Acc<->Reg!
			break;
		case 2: //Mem<->Reg?
			if (MODRM_EA(params)) //Reg<->Mem?
			{
				CPU[activeCPU].cycles_OP = 17 + MODRM_EA(params); //SegReg->Mem!
				if (data1) //One memory operand?
				{
					CPU_addWordMemoryTiming386(); //To memory?
					CPU_addWordMemoryTiming386(); //To memory?
				}
				if (data2) //One/two memory operands?
				{
					CPU_addWordMemoryTiming386(); //To memory?
					CPU_addWordMemoryTiming386(); //To memory?
				}
			}
			else //Reg<->Reg?
			{
				CPU[activeCPU].cycles_OP = 4; //SegReg->Mem!
			}
			break;
		}
}

extern byte modrm_addoffset; //Add this offset to ModR/M reads!

OPTINLINE void CPU80386_internal_LXS(int segmentregister) //LDS, LES etc.
{
	CPUPROT1
		uint_32 offset = modrm_read32(&params, 1);
	CPUPROT1
		modrm_addoffset = 4; //Add this to the offset to use!
	word segment = modrm_read16(&params, 1);
	modrm_addoffset = 0; //Reset again!
	CPUPROT1
		segmentWritten(segmentregister, segment, 0); //Load the new segment!
	CPUPROT1
		modrm_write32(&params, 0, offset); //Try to load the new register with the offset!
	CPUPROT2
		CPUPROT2
		CPUPROT2
		CPUPROT2
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 16 + MODRM_EA(params); /* LXS based on MOV Mem->SS, DS, ES */
			CPU_addWordMemoryTiming386(); //To memory?
			CPU_addWordMemoryTiming386(); //To memory?
		}
		else //Register? Should be illegal?
		{
			CPU[activeCPU].cycles_OP = 2; /* LXS based on MOV Mem->SS, DS, ES */
		}
}

/*

NOW THE REAL OPCODES!

*/

void CPU80386_OP01() { modrm_generateInstructionTEXT386("ADDD", 32, 0, PARAM_MODRM21); CPU80386_internal_ADD32(modrm_addr32(&params, 1, 0), modrm_read32(&params, 0), 2); }
void CPU80386_OP03() { modrm_generateInstructionTEXT386("ADDD", 32, 0, PARAM_MODRM12); CPU80386_internal_ADD32(modrm_addr32(&params, 0, 0), modrm_read32(&params, 1), 2); }
void CPU80386_OP05() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("ADDD EAX,", 0, theimm, PARAM_IMM32); CPU80386_internal_ADD32(&REG_EAX, theimm, 1); }
void CPU80386_OP09() { modrm_generateInstructionTEXT386("ORD", 32, 0, PARAM_MODRM21); CPU80386_internal_OR32(modrm_addr32(&params, 1, 0), modrm_read32(&params, 0), 2); }
void CPU80386_OP0B() { modrm_generateInstructionTEXT386("ORD", 32, 0, PARAM_MODRM12); CPU80386_internal_OR32(modrm_addr32(&params, 0, 0), modrm_read32(&params, 1), 2); }
void CPU80386_OP0D() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("ORD EAX,", 0, theimm, PARAM_IMM32); CPU80386_internal_OR32(&REG_EAX, theimm, 1); }
void CPU80386_OP11() { modrm_generateInstructionTEXT386("ADCD", 32, 0, PARAM_MODRM21); CPU80386_internal_ADC32(modrm_addr32(&params, 1, 0), modrm_read32(&params, 0), 2); }
void CPU80386_OP13() { modrm_generateInstructionTEXT386("ADCD", 32, 0, PARAM_MODRM12); CPU80386_internal_ADC32(modrm_addr32(&params, 0, 0), modrm_read32(&params, 1), 2); }
void CPU80386_OP15() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("ADCD EAX,", 0, theimm, PARAM_IMM32); CPU80386_internal_ADC32(&REG_EAX, theimm, 1); }
void CPU80386_OP19() { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("SBBD", 32, 0, PARAM_MODRM21); CPU80386_internal_SBB32(modrm_addr32(&params, 1, 0), (modrm_read32(&params, 0)), 2); }
void CPU80386_OP1B() { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("SBBW", 32, 0, PARAM_MODRM12); CPU80386_internal_SBB32(modrm_addr32(&params, 0, 0), (modrm_read32(&params, 1)), 2); }
void CPU80386_OP1D() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("SBB EAX,", 0, theimm, PARAM_IMM32); CPU80386_internal_SBB32(&REG_EAX, theimm, 1); }
void CPU80386_OP21() { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("ANDW", 32, 0, PARAM_MODRM21); CPU80386_internal_AND32(modrm_addr32(&params, 1, 0), modrm_read32(&params, 0), 2); }
void CPU80386_OP23() { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("ANDW", 32, 0, PARAM_MODRM12); CPU80386_internal_AND32(modrm_addr32(&params, 0, 0), modrm_read32(&params, 1), 2); }
void CPU80386_OP25() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("AND EAX,", 0, theimm, PARAM_IMM32); CPU80386_internal_AND32(&REG_EAX, theimm, 1); }
void CPU80386_OP29() { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("SUBW", 32, 0, PARAM_MODRM21); CPU80386_internal_SUB32(modrm_addr32(&params, 1, 0), (modrm_read32(&params, 0)), 2); }
void CPU80386_OP2B() { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("SUBW", 32, 0, PARAM_MODRM12); CPU80386_internal_SUB32(modrm_addr32(&params, 0, 0), (modrm_read32(&params, 1)), 2); }
void CPU80386_OP2D() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("SUB EAX,", 0, theimm, PARAM_IMM32);/*5=AX,imm32*/ CPU80386_internal_SUB32(&REG_EAX, theimm, 1);/*5=AX,imm32*/ }
void CPU80386_OP31() { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("XORW", 32, 0, PARAM_MODRM21); CPU80386_internal_XOR32(modrm_addr32(&params, 1, 0), modrm_read32(&params, 0), 2); }
void CPU80386_OP33() { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("XORW", 32, 0, PARAM_MODRM12); CPU80386_internal_XOR32(modrm_addr32(&params, 0, 0), modrm_read32(&params, 1), 2); }
void CPU80386_OP35() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("XOR EAX,", 0, theimm, PARAM_IMM32); CPU80386_internal_XOR32(&REG_EAX, theimm, 1); }
void CPU80386_OP39() { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("CMPW", 32, 0, PARAM_MODRM21); CMP_d(modrm_read32(&params, 1), modrm_read32(&params, 0), 2); }
void CPU80386_OP3B() { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("CMPW", 32, 0, PARAM_MODRM12); CMP_d(modrm_read32(&params, 0), modrm_read32(&params, 1), 2); }
void CPU80386_OP3D() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("CMP EAX,", 0, theimm, PARAM_IMM32);/*CMP AX, imm32*/ CMP_d(REG_EAX, theimm, 1);/*CMP AX, imm32*/ }
void CPU80386_OP40() { modrm_generateInstructionTEXT386("INC EAX", 0, 0, PARAM_NONE);/*INC AX*/ CPU80386_internal_INC32(&REG_EAX);/*INC AX*/ }
void CPU80386_OP41() { modrm_generateInstructionTEXT386("INC ECX", 0, 0, PARAM_NONE);/*INC CX*/ CPU80386_internal_INC32(&REG_ECX);/*INC CX*/ }
void CPU80386_OP42() { modrm_generateInstructionTEXT386("INC EDX", 0, 0, PARAM_NONE);/*INC DX*/ CPU80386_internal_INC32(&REG_EDX);/*INC DX*/ }
void CPU80386_OP43() { modrm_generateInstructionTEXT386("INC EBX", 0, 0, PARAM_NONE);/*INC BX*/ CPU80386_internal_INC32(&REG_EBX);/*INC BX*/ }
void CPU80386_OP44() { modrm_generateInstructionTEXT386("INC ESP", 0, 0, PARAM_NONE);/*INC SP*/ CPU80386_internal_INC32(&REG_ESP);/*INC SP*/ }
void CPU80386_OP45() { modrm_generateInstructionTEXT386("INC EBP", 0, 0, PARAM_NONE);/*INC BP*/ CPU80386_internal_INC32(&REG_EBP);/*INC BP*/ }
void CPU80386_OP46() { modrm_generateInstructionTEXT386("INC ESI", 0, 0, PARAM_NONE);/*INC SI*/ CPU80386_internal_INC32(&REG_ESI);/*INC SI*/ }
void CPU80386_OP47() { modrm_generateInstructionTEXT386("INC EDI", 0, 0, PARAM_NONE);/*INC DI*/ CPU80386_internal_INC32(&REG_EDI);/*INC DI*/ }
void CPU80386_OP48() { modrm_generateInstructionTEXT386("DEC EAX", 0, 0, PARAM_NONE);/*DEC AX*/ CPU80386_internal_DEC32(&REG_EAX);/*DEC AX*/ }
void CPU80386_OP49() { modrm_generateInstructionTEXT386("DEC ECX", 0, 0, PARAM_NONE);/*DEC CX*/ CPU80386_internal_DEC32(&REG_ECX);/*DEC CX*/ }
void CPU80386_OP4A() { modrm_generateInstructionTEXT386("DEC EDX", 0, 0, PARAM_NONE);/*DEC DX*/ CPU80386_internal_DEC32(&REG_EDX);/*DEC DX*/ }
void CPU80386_OP4B() { modrm_generateInstructionTEXT386("DEC EBX", 0, 0, PARAM_NONE);/*DEC BX*/ CPU80386_internal_DEC32(&REG_EBX);/*DEC BX*/ }
void CPU80386_OP4C() { modrm_generateInstructionTEXT386("DEC ESP", 0, 0, PARAM_NONE);/*DEC SP*/ CPU80386_internal_DEC32(&REG_ESP);/*DEC SP*/ }
void CPU80386_OP4D() { modrm_generateInstructionTEXT386("DEC EBP", 0, 0, PARAM_NONE);/*DEC BP*/ CPU80386_internal_DEC32(&REG_EBP);/*DEC BP*/ }
void CPU80386_OP4E() { modrm_generateInstructionTEXT386("DEC ESI", 0, 0, PARAM_NONE);/*DEC SI*/ CPU80386_internal_DEC32(&REG_ESI);/*DEC SI*/ }
void CPU80386_OP4F() { modrm_generateInstructionTEXT386("DEC EDI", 0, 0, PARAM_NONE);/*DEC DI*/ CPU80386_internal_DEC32(&REG_EDI);/*DEC DI*/ }
void CPU80386_OP50() { modrm_generateInstructionTEXT386("PUSH EAX", 0, 0, PARAM_NONE);/*PUSH AX*/ CPU_PUSH32(&REG_EAX);/*PUSH AX*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP51() { modrm_generateInstructionTEXT386("PUSH ECX", 0, 0, PARAM_NONE);/*PUSH CX*/ CPU_PUSH32(&REG_ECX);/*PUSH CX*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP52() { modrm_generateInstructionTEXT386("PUSH EDX", 0, 0, PARAM_NONE);/*PUSH DX*/ CPU_PUSH32(&REG_EDX);/*PUSH DX*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP53() { modrm_generateInstructionTEXT386("PUSH EBX", 0, 0, PARAM_NONE);/*PUSH BX*/ CPU_PUSH32(&REG_EBX);/*PUSH BX*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP54() { modrm_generateInstructionTEXT386("PUSH ESP", 0, 0, PARAM_NONE);/*PUSH SP*/ CPU_PUSH32(&REG_ESP);/*PUSH SP*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP55() { modrm_generateInstructionTEXT386("PUSH EBP", 0, 0, PARAM_NONE);/*PUSH BP*/ CPU_PUSH32(&REG_EBP);/*PUSH BP*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP56() { modrm_generateInstructionTEXT386("PUSH ESI", 0, 0, PARAM_NONE);/*PUSH SI*/ CPU_PUSH32(&REG_ESI);/*PUSH SI*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP57() { modrm_generateInstructionTEXT386("PUSH EDI", 0, 0, PARAM_NONE);/*PUSH DI*/ CPU_PUSH32(&REG_EDI);/*PUSH DI*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP58() { modrm_generateInstructionTEXT386("POP EAX", 0, 0, PARAM_NONE);/*POP AX*/ REG_EAX = CPU_POP32();/*POP AX*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP59() { modrm_generateInstructionTEXT386("POP ECX", 0, 0, PARAM_NONE);/*POP CX*/ REG_ECX = CPU_POP32();/*POP CX*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP5A() { modrm_generateInstructionTEXT386("POP EDX", 0, 0, PARAM_NONE);/*POP DX*/ REG_EDX = CPU_POP32();/*POP DX*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP5B() { modrm_generateInstructionTEXT386("POP EBX", 0, 0, PARAM_NONE);/*POP BX*/ REG_EBX = CPU_POP32();/*POP BX*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP5C() { modrm_generateInstructionTEXT386("POP ESP", 0, 0, PARAM_NONE);/*POP SP*/ REG_ESP = MMU_rw(CPU_SEGMENT_SS, REG_SS, REG_ESP, 0);/*POP SP*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP5D() { modrm_generateInstructionTEXT386("POP EBP", 0, 0, PARAM_NONE);/*POP BP*/ REG_EBP = CPU_POP32();/*POP BP*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP5E() { modrm_generateInstructionTEXT386("POP ESI", 0, 0, PARAM_NONE);/*POP SI*/ REG_ESI = CPU_POP32();/*POP SI*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP5F() { modrm_generateInstructionTEXT386("POP EDI", 0, 0, PARAM_NONE);/*POP DI*/ REG_EDI = CPU_POP32();/*POP DI*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP85() { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("TESTW", 32, 0, PARAM_MODRM12); CPU80386_internal_TEST32(modrm_read32(&params, 0), modrm_read32(&params, 1), 2); }
void CPU80386_OP87() { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("XCHGW", 32, 0, PARAM_MODRM12); CPU80386_internal_XCHG32(modrm_addr32(&params, 0, 0), modrm_addr32(&params, 1, 1), 2); /*XCHG reg32,r/m32*/ }
void CPU80386_OP89() { modrm_debugger32(&params, 1, 0); modrm_generateInstructionTEXT386("MOVW", 32, 0, PARAM_MODRM21); CPU80386_internal_MOV32(modrm_addr32(&params, 1, 0), modrm_read32(&params, 0), 2); }
void CPU80386_OP8B() { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("MOVW", 32, 0, PARAM_MODRM12); CPU80386_internal_MOV32(modrm_addr32(&params, 0, 0), modrm_read32(&params, 1), 2); }
void CPU80386_OP8C() { modrm_debugger32(&params, 1, 0); modrm_generateInstructionTEXT386("MOVW", 32, 0, PARAM_MODRM21); CPU80386_internal_MOV32(modrm_addr32(&params, 1, 0), modrm_read32(&params, 0), 8); }
void CPU80386_OP8D() { modrm_debugger32(&params, 0, 1); debugger_setcommand("LEA %s,%s", modrm_param1, getLEAtext386(&params)); CPU80386_internal_MOV32(modrm_addr32(&params, 0, 0), getLEA386(&params), 0); CPU[activeCPU].cycles_OP = 2 + MODRM_EA(params); /* Load effective address */ }
void CPU80386_OP8E() { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("MOVW", 32, 0, PARAM_MODRM12); CPU80386_internal_MOV32(modrm_addr32(&params, 0, 0), modrm_read32(&params, 1), 8); }
void CPU80386_OP90() /*NOP*/ { modrm_generateInstructionTEXT386("NOP", 0, 0, PARAM_NONE);/*NOP (XCHG AX,AX)*/ CPU80386_internal_XCHG32(&REG_EAX, &REG_EAX, 1); CPU[activeCPU].cycles_OP = 3; /* NOP */ }
void CPU80386_OP91() { modrm_generateInstructionTEXT386("XCHG ECX,EAX", 0, 0, PARAM_NONE);/*XCHG AX,CX*/ CPU80386_internal_XCHG32(&REG_ECX, &REG_EAX, 1); /*XCHG CX,AX*/ }
void CPU80386_OP92() { modrm_generateInstructionTEXT386("XCHG EDX,EAX", 0, 0, PARAM_NONE);/*XCHG AX,DX*/ CPU80386_internal_XCHG32(&REG_EDX, &REG_EAX, 1); /*XCHG DX,AX*/ }
void CPU80386_OP93() { modrm_generateInstructionTEXT386("XCHG EBX,EAX", 0, 0, PARAM_NONE);/*XCHG AX,BX*/ CPU80386_internal_XCHG32(&REG_EBX, &REG_EAX, 1); /*XCHG BX,AX*/ }
void CPU80386_OP94() { modrm_generateInstructionTEXT386("XCHG ESP,EAX", 0, 0, PARAM_NONE);/*XCHG AX,SP*/ CPU80386_internal_XCHG32(&REG_ESP, &REG_EAX, 1); /*XCHG SP,AX*/ }
void CPU80386_OP95() { modrm_generateInstructionTEXT386("XCHG EBP,EAX", 0, 0, PARAM_NONE);/*XCHG AX,BP*/ CPU80386_internal_XCHG32(&REG_EBP, &REG_EAX, 1); /*XCHG BP,AX*/ }
void CPU80386_OP96() { modrm_generateInstructionTEXT386("XCHG ESI,EAX", 0, 0, PARAM_NONE);/*XCHG AX,SI*/ CPU80386_internal_XCHG32(&REG_ESI, &REG_EAX, 1); /*XCHG SI,AX*/ }
void CPU80386_OP97() { modrm_generateInstructionTEXT386("XCHG EDI,EAX", 0, 0, PARAM_NONE);/*XCHG AX,DI*/ CPU80386_internal_XCHG32(&REG_EDI, &REG_EAX, 1); /*XCHG DI,AX*/ }
void CPU80386_OP99() { modrm_generateInstructionTEXT386("CDQ", 0, 0, PARAM_NONE);/*CDQ : sign extend EAX to EDX::EAX*/ CPU80386_internal_CDQ();/*CDQ : sign extend AX to DX::AX (8088+)*/ }
void CPU80386_OP9A() {/*CALL Ap*/ INLINEREGISTER uint_64 segmentoffset = imm64; debugger_setcommand("CALL %04x:%08x", (segmentoffset >> 32), (segmentoffset & 0xFFFFFFFF)); CPU_PUSH32(&REG_CS); CPU_PUSH32(&REG_EIP); destEIP = (segmentoffset & 0xFFFFFFFF);  segmentWritten(CPU_SEGMENT_CS, (segmentoffset >> 32), 2); /*CS changed!*/ CPU[activeCPU].cycles_OP = 28; /* Intersegment direct */ CPU_addWordMemoryTiming386(); /*To memory?*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OP9B() { modrm_generateInstructionTEXT386("WAIT", 0, 0, PARAM_NONE);/*WAIT : wait for TEST pin activity. (UNIMPLEMENTED)*/ CPU[activeCPU].wait = 1;/*9B: WAIT : wait for TEST pin activity. (Edit: continue on interrupts or 8087+!!!)*/ }
//void CPU80386_OPA0() { INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVB AL,[%s:%08X]", CPU_textsegment(CPU_SEGMENT_DS), theimm);/*MOV AL,[imm32]*/ CPU80386_internal_MOV8(&REG_AL, MMU_rb(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), theimm, 0), 1);/*MOV AL,[imm32]*/ }
void CPU80386_OPA1() { INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVW EAX,[%s:%08X]", CPU_textsegment(CPU_SEGMENT_DS), theimm);/*MOV AX,[imm32]*/  CPU80386_internal_MOV32(&REG_EAX, MMU_rw(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), theimm, 0), 1);/*MOV AX,[imm32]*/ }
//void CPU80386_OPA2() { INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVB [%s:%08X],AL", CPU_textsegment(CPU_SEGMENT_DS), theimm);/*MOV [imm32],AL*/ custommem = 1; customoffset = theimm; CPU80386_internal_MOV8(NULL, REG_AL, 1);/*MOV [imm32],AL*/ custommem = 0; }
void CPU80386_OPA3() { INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVW [%s:%08X],EAX", CPU_textsegment(CPU_SEGMENT_DS), theimm);/*MOV [imm32], AX*/ custommem = 1; customoffset = theimm; CPU80386_internal_MOV32(NULL, REG_EAX, 1);/*MOV [imm32], AX*/ custommem = 0; }
void CPU80386_OPA5() { modrm_generateInstructionTEXT386("MOVSD", 0, 0, PARAM_NONE);/*MOVSD*/ CPU80386_internal_MOVSD();/*MOVSD*/ }
void CPU80386_OPA7() { debugger_setcommand("CMPSD [%s:ESI],[ES:EDI]", CPU_textsegment(CPU_SEGMENT_DS));/*CMPSD*/ CPU80386_internal_CMPSD();/*CMPSD*/ }
void CPU80386_OPA9() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("TESTW EAX,", 0, theimm, PARAM_IMM32);/*TEST AX,imm32*/ CPU80386_internal_TEST32(REG_EAX, theimm, 1);/*TEST AX,imm32*/ }
void CPU80386_OPAB() { modrm_generateInstructionTEXT386("STOSD", 0, 0, PARAM_NONE);/*STOSD*/ CPU80386_internal_STOSD();/*STOSD*/ }
void CPU80386_OPAD() { modrm_generateInstructionTEXT386("LODSD", 0, 0, PARAM_NONE);/*LODSD*/ CPU80386_internal_LODSD();/*LODSD*/ }
void CPU80386_OPAF() { modrm_generateInstructionTEXT386("SCASD", 0, 0, PARAM_NONE);/*SCASD*/ CPU80386_internal_SCASD();/*SCASD*/ }
void CPU80386_OPB8() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("MOVW EAX,", 0, theimm, PARAM_IMM32);/*MOV AX,imm32*/ CPU80386_internal_MOV32(&REG_EAX, theimm, 4);/*MOV AX,imm32*/ }
void CPU80386_OPB9() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("MOVW ECX,", 0, theimm, PARAM_IMM32);/*MOV CX,imm32*/ CPU80386_internal_MOV32(&REG_ECX, theimm, 4);/*MOV CX,imm32*/ }
void CPU80386_OPBA() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("MOVW EDX,", 0, theimm, PARAM_IMM32);/*MOV DX,imm32*/ CPU80386_internal_MOV32(&REG_EDX, theimm, 4);/*MOV DX,imm32*/ }
void CPU80386_OPBB() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("MOVW EBX,", 0, theimm, PARAM_IMM32);/*MOV BX,imm32*/ CPU80386_internal_MOV32(&REG_EBX, theimm, 4);/*MOV BX,imm32*/ }
void CPU80386_OPBC() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("MOVW ESP,", 0, theimm, PARAM_IMM32);/*MOV SP,imm32*/ CPU80386_internal_MOV32(&REG_ESP, theimm, 4);/*MOV SP,imm32*/ }
void CPU80386_OPBD() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("MOVW EBP,", 0, theimm, PARAM_IMM32);/*MOV BP,imm32*/ CPU80386_internal_MOV32(&REG_EBP, theimm, 4);/*MOV BP,imm32*/ }
void CPU80386_OPBE() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("MOVW ESI,", 0, theimm, PARAM_IMM32);/*MOV SI,imm32*/ CPU80386_internal_MOV32(&REG_ESI, theimm, 4);/*MOV SI,imm32*/ }
void CPU80386_OPBF() { INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT386("MOVW EDI,", 0, theimm, PARAM_IMM32);/*MOV DI,imm32*/ CPU80386_internal_MOV32(&REG_EDI, theimm, 4);/*MOV DI,imm32*/ }
void CPU80386_OPC2() { INLINEREGISTER int_32 popbytes = imm32();/*RET imm32 (Near return to calling proc and POP imm32 bytes)*/ modrm_generateInstructionTEXT386("RET", 0, popbytes, PARAM_IMM8); /*RET imm32 (Near return to calling proc and POP imm32 bytes)*/ CPU80386_internal_RET(popbytes, 1); }
void CPU80386_OPC3() { modrm_generateInstructionTEXT386("RET", 0, 0, PARAM_NONE);/*RET (Near return to calling proc)*/ /*RET (Near return to calling proc)*/ CPU80386_internal_RET(0, 0); }
void CPU80386_OPC4() /*LES modr/m*/ { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("LES", 0, 0, PARAM_MODRM12); CPU80386_internal_LXS(CPU_SEGMENT_ES); /*Load new ES!*/ }
void CPU80386_OPC5() /*LDS modr/m*/ { modrm_debugger32(&params, 0, 1); modrm_generateInstructionTEXT386("LDS", 0, 0, PARAM_MODRM12); CPU80386_internal_LXS(CPU_SEGMENT_DS); /*Load new DS!*/ }
void CPU80386_OPC7() { uint_32 val = imm32; modrm_debugger32(&params, 0, 1); debugger_setcommand("MOVW %s,%08x", modrm_param2, val); modrm_write32(&params, 1, val); if (MODRM_EA(params)) { CPU[activeCPU].cycles_OP = 10 + MODRM_EA(params); /* Imm->Mem */  CPU_addWordMemoryTiming386(); /*To memory?*/ } else CPU[activeCPU].cycles_OP = 4; /* Imm->Reg */ }
void CPU80386_OPCA() { INLINEREGISTER sword popbytes = imm32();/*RETF imm32 (Far return to calling proc and pop imm32 bytes)*/ modrm_generateInstructionTEXT386("RETF", 0, popbytes, PARAM_IMM32); /*RETF imm32 (Far return to calling proc and pop imm32 bytes)*/ CPU80386_internal_RETF(popbytes, 1); }
void CPU80386_OPCB() { modrm_generateInstructionTEXT386("RETF", 0, 0, PARAM_NONE); /*RETF (Far return to calling proc)*/ CPU80386_internal_RETF(0, 0); }
void CPU80386_OPCD() { INLINEREGISTER byte theimm = immb; INTdebugger80386(); modrm_generateInstructionTEXT386("INT", 0, theimm, PARAM_IMM8);/*INT imm8*/ CPU80386_int(theimm, 0);/*INT imm8*/ }
void CPU80386_OPCE() { modrm_generateInstructionTEXT386("INTO", 0, 0, PARAM_NONE);/*INTO*/ CPU80386_internal_INTO();/*INTO*/ }
void CPU80386_OPE0() { INLINEREGISTER signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT386("LOOPNZ", 0, ((REG_EIP + rel8) & 0xFFFF), PARAM_IMM32); if ((--REG_ECX) && (!FLAG_ZF)) { REG_EIP += rel8; CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 19; /* Branch taken */ } else { CPU[activeCPU].cycles_OP = 5; /* Branch not taken */ } }
void CPU80386_OPE1() { INLINEREGISTER signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT386("LOOPZ", 0, ((REG_EIP + rel8) & 0xFFFF), PARAM_IMM32);if ((--REG_ECX) && (FLAG_ZF)) { REG_EIP += rel8;CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 18; /* Branch taken */ } else { CPU[activeCPU].cycles_OP = 6; /* Branch not taken */ } }
void CPU80386_OPE2() { INLINEREGISTER signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT386("LOOP", 0, ((REG_EIP + rel8) & 0xFFFF), PARAM_IMM32);if (--REG_ECX) { REG_EIP += rel8;CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 17; /* Branch taken */ } else { CPU[activeCPU].cycles_OP = 5; /* Branch not taken */ } }
void CPU80386_OPE3() { INLINEREGISTER signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT386("JCXZ", 0, ((REG_EIP + rel8) & 0xFFFF), PARAM_IMM32); if (!REG_ECX) { REG_EIP += rel8;CPU_flushPIQ(); /*We're jumping to another address*/CPU[activeCPU].cycles_OP = 18; /* Branch taken */ } else { CPU[activeCPU].cycles_OP = 6; /* Branch not taken */ } }
void CPU80386_OPE5() { INLINEREGISTER byte theimm = imm8();modrm_generateInstructionTEXT386("IN EAX,", 0, theimm, PARAM_IMM8); CPU_PORT_IN_D(theimm, &REG_EAX); CPU[activeCPU].cycles_OP = 10; /*Timings!*/  CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OPE7() { INLINEREGISTER byte theimm = imm8(); debugger_setcommand("OUT %02X,EAX", theimm); CPU_PORT_OUT_D(theimm, REG_EAX); CPU[activeCPU].cycles_OP = 10; /*Timings!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OPE8() { INLINEREGISTER sword reloffset = imm32(); modrm_generateInstructionTEXT386("CALL", 0, ((REG_EIP + reloffset) & 0xFFFF), PARAM_IMM32); CPU_PUSH32(&REG_EIP); REG_EIP += reloffset;CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 19; /* Intrasegment direct */ }
void CPU80386_OPE9() { INLINEREGISTER sword reloffset = imm32(); modrm_generateInstructionTEXT386("JMP", 0, ((REG_EIP + reloffset) & 0xFFFF), PARAM_IMM32); REG_EIP += reloffset;CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 15; /* Intrasegment direct */ }
void CPU80386_OPEA() { INLINEREGISTER uint_64 segmentoffset = imm64; debugger_setcommand("JMP %04X:%04X", (segmentoffset >> 32), (segmentoffset & 0xFFFFFFFF)); destEIP = (segmentoffset & 0xFFFFFFFF); segmentWritten(CPU_SEGMENT_CS, (word)(segmentoffset >> 32), 1); CPU[activeCPU].cycles_OP = 15; /* Intersegment direct */ }
void CPU80386_OPEB() { INLINEREGISTER signed char reloffset = imm8(); modrm_generateInstructionTEXT386("JMP", 0, ((REG_EIP + reloffset) & 0xFFFF), PARAM_IMM32); REG_EIP += reloffset;CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 15; /* Intrasegment direct short */ }
void CPU80386_OPED() { modrm_generateInstructionTEXT386("IN EAX,DX", 0, 0, PARAM_NONE); CPU_PORT_IN_D(REG_DX, &REG_EAX); CPU[activeCPU].cycles_OP = 8; /*Timings!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OPEF() { modrm_generateInstructionTEXT386("OUT DX,EAX", 0, 0, PARAM_NONE); CPU_PORT_OUT_D(REG_EDX, REG_EAX); CPU[activeCPU].cycles_OP = 8; /*Timings!*/ CPU_addWordMemoryTiming386(); /*To memory?*/ }
void CPU80386_OPF1() { modrm_generateInstructionTEXT386("<Undefined and reserved opcode, no error>", 0, 0, PARAM_NONE); }
void CPU80386_OPF4() { modrm_generateInstructionTEXT386("HLT", 0, 0, PARAM_NONE); CPU[activeCPU].halt = 1; CPU[activeCPU].cycles_OP = 2; /*Special timing!*/ }

/*

NOW COME THE GRP1-5 OPCODES:

*/

//GRP1

void CPU80386_OP81() //GRP1 Ev,Iv
{
	INLINEREGISTER uint_32 imm = imm32;
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger32(&params, 1, 0);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //ADD
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADDD %s,%08X", &modrm_param1, imm); //ADD Eb, Ib
		}
		CPU80386_internal_ADD32(modrm_addr32(&params, 1, 0), imm, 3); //ADD Eb, Ib
		break;
	case 1: //OR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ORD %s,%08X", &modrm_param1, imm); //OR Eb, Ib
		}
		CPU80386_internal_OR32(modrm_addr32(&params, 1, 0), imm, 3); //OR Eb, Ib
		break;
	case 2: //ADC
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADCD %s,%08X", &modrm_param1, imm); //ADC Eb, Ib
		}
		CPU80386_internal_ADC32(modrm_addr32(&params, 1, 0), imm, 3); //ADC Eb, Ib
		break;
	case 3: //SBB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SBBD %s,%08X", &modrm_param1, imm); //SBB Eb, Ib
		}
		CPU80386_internal_SBB32(modrm_addr32(&params, 1, 0), imm, 3); //SBB Eb, Ib
		break;
	case 4: //AND
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ANDD %s,%08X", &modrm_param1, imm); //AND Eb, Ib
		}
		CPU80386_internal_AND32(modrm_addr32(&params, 1, 0), imm, 3); //AND Eb, Ib
		break;
	case 5: //SUB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SUBD %s,%08X", &modrm_param1, imm); //SUB Eb, Ib
		}
		CPU80386_internal_SUB32(modrm_addr32(&params, 1, 0), imm, 3); //SUB Eb, Ib
		break;
	case 6: //XOR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("XORD %s,%08X", &modrm_param1, imm); //XOR Eb, Ib
		}
		CPU80386_internal_XOR32(modrm_addr32(&params, 1, 0), imm, 3); //XOR Eb, Ib
		break;
	case 7: //CMP
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("CMPD %s,%08X", &modrm_param1, imm); //CMP Eb, Ib
		}
		CMP_d(modrm_read32(&params, 1), imm, 3); //CMP Eb, Ib
		break;
	default:
		break;
	}
}

void CPU80386_OP83() //GRP1 Ev,Ib
{
	INLINEREGISTER uint_32 imm;
	imm = immb;
	if (imm & 0x80) imm |= 0xFFFFFF00; //Sign extend!
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger32(&params, 1, 0);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //ADD
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADDD %s,%08X", &modrm_param1, imm); //ADD Eb, Ib
		}
		CPU80386_internal_ADD32(modrm_addr32(&params, 1, 0), imm, 3); //ADD Eb, Ib
		break;
	case 1: //OR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ORD %s,%08X", &modrm_param1, imm); //OR Eb, Ib
		}
		CPU80386_internal_OR32(modrm_addr32(&params, 1, 0), imm, 3); //OR Eb, Ib
		break;
	case 2: //ADC
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADCD %s,%08X", &modrm_param1, imm); //ADC Eb, Ib
		}
		CPU80386_internal_ADC32(modrm_addr32(&params, 1, 0), imm, 3); //ADC Eb, Ib
		break;
	case 3: //SBB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SBBD %s,%08X", &modrm_param1, imm); //SBB Eb, Ib
		}
		CPU80386_internal_SBB32(modrm_addr32(&params, 1, 0), imm, 3); //SBB Eb, Ib
		break;
	case 4: //AND
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ANDD %s,%08X", &modrm_param1, imm); //AND Eb, Ib
		}
		CPU80386_internal_AND32(modrm_addr32(&params, 1, 0), imm, 3); //AND Eb, Ib
		break;
	case 5: //SUB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SUBD %s,%08X", &modrm_param1, imm); //SUB Eb, Ib
		}
		CPU80386_internal_SUB32(modrm_addr32(&params, 1, 0), imm, 3); //SUB Eb, Ib
		break;
	case 6: //XOR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("XORD %s,%08X", &modrm_param1, imm); //XOR Eb, Ib
		}
		CPU80386_internal_XOR32(modrm_addr32(&params, 1, 0), imm, 3); //XOR Eb, Ib
		break;
	case 7: //CMP
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("CMPD %s,%08X", &modrm_param1, imm); //CMP Eb, Ib
		}
		CMP_d(modrm_read32(&params, 1), imm, 3); //CMP Eb, Ib
		break;
	default:
		break;
	}
}

void CPU80386_OP8F() //Undocumented GRP opcode 8F r/m32
{
	if (cpudebugger)
	{
		modrm_debugger32(&params, 0, 1);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //POP
		if (cpudebugger) //Debugger on?
		{
			modrm_generateInstructionTEXT386("POPD", 32, 0, PARAM_MODRM2); //POPW Ew
		}
		modrm_write32(&params, 1, CPU_POP32()); //POP r/m32
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 17 + MODRM_EA(params); /*Pop Mem!*/
			CPU_addWordMemoryTiming386(); /*To memory?*/
			CPU_addWordMemoryTiming386(); /*To memory?*/
		}
		else //Reg?
		{
			CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/
		}
		break;
	default: //Unknown opcode or special?
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("Unknown opcode: 8F /%i", MODRM_REG(params.modrm)); //Error!
		}
		CPU_unkOP(); //Execute the unknown opcode exception handler, if any!
		break;
	}
}

void CPU80386_OPD1() //GRP2 Ev,1
{
	thereg = MODRM_REG(params.modrm);
	oper1d = modrm_read32(&params, 1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger32(&params, 0, 1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //ROL
			debugger_setcommand("ROLD %s,1", &modrm_param2);
			break;
		case 1: //ROR
			debugger_setcommand("RORD %s,1", &modrm_param2);
			break;
		case 2: //RCL
			debugger_setcommand("RCLD %s,1", &modrm_param2);
			break;
		case 3: //RCR
			debugger_setcommand("RCRD %s,1", &modrm_param2);
			break;
		case 4: //SHL
		case 6: //--- Unknown Opcode! --- Undocumented opcode!
			debugger_setcommand("SHLD %s,1", &modrm_param2);
			break;
		case 5: //SHR
			debugger_setcommand("SHRD %s,1", &modrm_param2);
			break;
		case 7: //SAR
			debugger_setcommand("SARD %s,1", &modrm_param2);
			break;
		default:
			break;
		}
	}
	modrm_write32(&params, 1, op_grp2_32(1, 0));
}
void CPU80386_OPD3() //GRP2 Ev,CL
{
	thereg = MODRM_REG(params.modrm);
	oper1d = modrm_read32(&params, 1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger32(&params, 0, 1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //ROL
			debugger_setcommand("ROLD %s,CL", &modrm_param2);
			break;
		case 1: //ROR
			debugger_setcommand("RORD %s,CL", &modrm_param2);
			break;
		case 2: //RCL
			debugger_setcommand("RCLD %s,CL", &modrm_param2);
			break;
		case 3: //RCR
			debugger_setcommand("RCRD %s,CL", &modrm_param2);
			break;
		case 4: //SHL
			debugger_setcommand("SHLD %s,CL", &modrm_param2);
			break;
		case 5: //SHR
			debugger_setcommand("SHRD %s,CL", &modrm_param2);
			break;
		case 6: //--- Unknown Opcode! ---
			debugger_setcommand("<UNKNOWN MODR/M: GRP2(w) /6, CL>");
			break;
		case 7: //SAR
			debugger_setcommand("SARD %s,CL", &modrm_param2);
			break;
		default:
			break;
		}
	}
	modrm_write32(&params, 1, op_grp2_32(REG_CL, 1));
}

void CPU80386_OPF7() //GRP3b Ev
{
	thereg = MODRM_REG(params.modrm);
	oper1d = modrm_read32(&params, 1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger32(&params, 0, 1); //Get src!
		switch (thereg) //What function?
		{
		case 0: //TEST modrm32, imm32
		case 1: //--- Undocumented opcode, same as above!
			debugger_setcommand("TESTD %s,%02x", &modrm_param2, imm32);
			break;
		case 2: //NOT
			modrm_generateInstructionTEXT386("NOTD", 32, 0, PARAM_MODRM2);
			break;
		case 3: //NEG
			modrm_generateInstructionTEXT386("NEGD", 32, 0, PARAM_MODRM2);
			break;
		case 4: //MUL
			modrm_generateInstructionTEXT386("MULD", 32, 0, PARAM_MODRM2);
			break;
		case 5: //IMUL
			modrm_generateInstructionTEXT386("IMULD", 32, 0, PARAM_MODRM2);
			break;
		case 6: //DIV
			modrm_generateInstructionTEXT386("DIVD", 32, 0, PARAM_MODRM2);
			break;
		case 7: //IDIV
			modrm_generateInstructionTEXT386("IDIVD", 32, 0, PARAM_MODRM2);
			break;
		default:
			break;
		}
	}
	op_grp3_32();
	if ((thereg>1) && (thereg<4)) //NOT/NEG?
	{
		modrm_write32(&params, 1, res32);
	}
}
//All OK up till here.

/*

DEBUG: REALLY SUPPOSED TO HANDLE HERE?

*/

void CPU80386_OPFF() //GRP5 Ev
{
	thereg = MODRM_REG(params.modrm);
	oper1d = modrm_read32(&params, 1);
	ea = modrm_offset32(&params, 1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger32(&params, 0, 1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //INC modrm8
			modrm_generateInstructionTEXT386("INCD", 32, 0, PARAM_MODRM2); //INC!
			break;
		case 1: //DEC modrm8
			modrm_generateInstructionTEXT386("DECD", 32, 0, PARAM_MODRM2); //DEC!
			break;
		case 2: //CALL
			modrm_generateInstructionTEXT386("CALLD", 32, 0, PARAM_MODRM2); //CALL!
			break;
		case 3: //CALL Mp (Read address word and jump there)
			modrm_generateInstructionTEXT386("CALLD", 32, 0, PARAM_MODRM2); //Jump to the address pointed here!
																		//debugger_setcommand("CALL %04X:%04X",MMU_rw(CPU_SEGMENT_CS,REG_CS,ea,0),MMU_rw(CPU_SEGMENT_CS,REG_CS,ea+2,0)); //Based on CALL Ap
			break;
		case 4: //JMP
			modrm_generateInstructionTEXT386("JMPD", 32, 0, PARAM_MODRM2); //JMP to the register!
			break;
		case 5: //JMP Mp
			modrm_generateInstructionTEXT386("JMPD", 32, 0, PARAM_MODRM2); //Jump to the address pointed here!
																	   //debugger_setcommand("JMP %04X:%04X",MMU_rw(CPU_SEGMENT_CS,REG_CS,ea,0),MMU_rw(CPU_SEGMENT_CS,REG_CS,ea+2,0)); //JMP to destination!
			break;
		case 6: //PUSH
			modrm_generateInstructionTEXT386("PUSHD", 32, 0, PARAM_MODRM2); //PUSH!
			break;
		case 7: //---
			debugger_setcommand("<UNKNOWN Opcode: GRP5(w) /7>");
			break;
		default:
			break;
		}
	}
	op_grp5_32();
}

/*

Special stuff for NO COprocessor (8087) present/available (default)!

*/


/*void FPU8087_OPDBE3(){debugger_setcommand("<UNKOP8087: FNINIT>");}

void FPU8087_OPDB()
{byte subOP = immb; CPUPROT1 word oldCS = REG_CS; word oldIP = REG_EIP; if (subOP==0xE3){FPU8087_OPDBE3();} else{REG_CS = oldCS; REG_EIP = oldIP; FPU8087_noCOOP();} CPUPROT2 }
void FPU8087_OPDFE0(){debugger_setcommand("<UNKOP8087: FNINIT>");}
void FPU8087_OPDF(){CPUPROT1 byte subOP = immb; CPUPROT1 word oldCS = REG_CS; word oldIP = REG_EIP; if (subOP==0xE0){FPU8087_OPDFE0();} else {REG_CS = oldCS; REG_EIP = oldIP; FPU8087_noCOOP();} CPUPROT2 CPUPROT2 }
void FPU8087_OPDDslash7(){debugger_setcommand("<UNKOP8087: FNSTSW>");}
void FPU8087_OPDD(){word oldCS; word oldIP; oldCS = REG_CS; oldIP = REG_EIP; CPUPROT1 if (MODRM_REG(params.modrm)==7){FPU8087_OPDDslash7();}else {REG_CS = oldCS; REG_EIP = oldIP; FPU8087_noCOOP();} CPUPROT2}
void FPU8087_OPD9slash7(){debugger_setcommand("<UNKOP8087: FNSTCW>");}
void FPU8087_OPD9(){word oldCS; word oldIP; oldCS = REG_CS; oldIP = REG_EIP; CPUPROT1 if (MODRM_REG(params.modrm)==7){FPU8087_OPD9slash7();} else {REG_CS = oldCS; REG_EIP = oldIP; FPU8087_noCOOP();} CPUPROT2}
*/
void FPU80387_noCOOP() {
	debugger_setcommand("<No COprocessor OPcodes implemented!>");
	/*CPU_resetOP(); CPU_COOP_notavailable();*/ //Only on 286+!
	CPU[activeCPU].cycles_OP = MODRM_EA(params) ? 8 + MODRM_EA(params) : 2; //No hardware interrupt to use anymore!
}

void unkOP_80386() //Unknown opcode on 8086?
{
	//dolog("8086","Unknown opcode on 8086: %02X",CPU[activeCPU].lastopcode); //Last read opcode!
	CPU_unkOP(); //Execute the unknown opcode exception handler, if any!
}

//Gecontroleerd: 100% OK!

//Now, the GRP opcodes!

OPTINLINE void op_grp2_cycles386(byte cnt, byte varshift)
{
	switch (varshift) //What type of shift are we using?
	{
	case 0: //Reg/Mem with 1 shift?
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 15 + MODRM_EA(params); //Mem
			if (varshift & 4) CPU_addWordMemoryTiming386(); /*To memory?*/
		}
		else //Reg?
		{
			CPU[activeCPU].cycles_OP = 2; //Reg
		}
		break;
	case 1: //Reg/Mem with variable shift?
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 20 + MODRM_EA(params) + (cnt << 2); //Mem
			if (varshift & 4) CPU_addWordMemoryTiming386(); /*To memory?*/
		}
		else //Reg?
		{
			CPU[activeCPU].cycles_OP = 8 + (cnt << 2); //Reg
		}
		break;
	case 2: //Reg/Mem with immediate variable shift(NEC V20/V30)?
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 20 + MODRM_EA(params) + (cnt << 2); //Mem
			if (varshift & 4) CPU_addWordMemoryTiming386(); /*To memory?*/
		}
		else //Reg?
		{
			CPU[activeCPU].cycles_OP = 8 + (cnt << 2); //Reg
		}
		break;
	}
}

uint_32 op_grp2_32(byte cnt, byte varshift) {
	//uint32_t d,
	INLINEREGISTER uint_64 s, shift, oldCF, msb;
	//if (cnt>0x10) return(oper1); //NEC V20/V30+ limits shift count
	if (EMULATED_CPU >= CPU_NECV30) cnt &= 0x1F; //Clear the upper 3 bits to become a NEC V20/V30+!
	s = oper1d;
	oldCF = FLAG_CF;
	switch (thereg) {
	case 0: //ROL r/m32
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80000000) FLAG_CF = 1; else FLAG_CF = 0;
			s = s << 1;
			s = s | FLAG_CF;
		}
		if (cnt) FLAG_OF = FLAG_CF ^ ((s >> 31) & 1);
		break;

	case 1: //ROR r/m32
		for (shift = 1; shift <= cnt; shift++) {
			FLAG_CF = s & 1;
			s = (s >> 1) | (FLAG_CF << 31);
		}
		if (cnt) FLAG_OF = (byte)((s >> 31) ^ ((s >> 30) & 1));
		break;

	case 2: //RCL r/m32
		for (shift = 1; shift <= cnt; shift++) {
			oldCF = FLAG_CF;
			if (s & 0x80000000) FLAG_CF = 1; else FLAG_CF = 0;
			s = s << 1;
			s = s | oldCF;
			//oldCF = ((s&0x8000)>>15)&1; //Save FLAG_CF!
			//s = (s<<1)+FLAG_CF;
			//FLAG_CF = oldCF;
		}
		if (cnt) FLAG_OF = FLAG_CF ^ ((s >> 31) & 1);
		break;

	case 3: //RCR r/m32
		if (cnt) FLAG_OF = ((s >> 31) & 1) ^ FLAG_CF;
		for (shift = 1; shift <= cnt; shift++) {
			oldCF = FLAG_CF;
			FLAG_CF = s & 1;
			s = (s >> 1) | (oldCF << 31);
			//oldCF = s&1;
			//s = (s<<1)+(FLAG_CF<<32);
			//FLAG_CF = oldCF;
		}
		if (cnt) FLAG_OF = (byte)((s >> 31) ^ ((s >> 30) & 1));
		break;

	case 4: case 6: //SHL r/m32
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80000000) FLAG_CF = 1; else FLAG_CF = 0;
			s = (s << 1) & 0xFFFFFFFF;
		}
		if ((cnt) && (FLAG_CF == (s >> 31))) FLAG_OF = 0; else FLAG_OF = 1;
		flag_szp32((uint_32)s); break;

	case 5: //SHR r/m32
		if (cnt) FLAG_OF = (s & 0x80000000) ? 1 : 0;
		for (shift = 1; shift <= cnt; shift++) {
			FLAG_CF = s & 1;
			s = s >> 1;
		}
		flag_szp32((uint_32)s); break;

	case 7: //SAR r/m32
		if (cnt) FLAG_OF = 0;
		msb = s & 0x80000000; //Read the MSB!
		for (shift = 1; shift <= cnt; shift++) {
			FLAG_CF = s & 1;
			s = (s >> 1) | msb;
		}
		byte tempSF;
		tempSF = FLAG_SF; //Save the SF!
		flag_szp32((uint_32)s);
		if (!cnt) //Nothing done?
		{
			FLAG_SF = tempSF; //We don't update when nothing's done!
		}
		break;
	}
	op_grp2_cycles386(cnt, varshift | 4);
	return(s & 0xFFFFFFFF);
}

byte tmps, tmpp; //Sign/parity backup!

extern byte CPU_databussize; //Current data bus size!

//byte tempAL;
word tempEAX;
uint_64 tempEDXEAX;

OPTINLINE void op_div32(uint64_t valdiv, uint_32 divisor) {
	//word v1, v2;
	if (!divisor) { CPU_exDIV0(); return; }
	if ((valdiv / (uint64_t)divisor) > 0xFFFFFFFF) { CPU_exDIV0(); return; }
	REG_EDX = (uint_32)(valdiv % (uint64_t)divisor);
	REG_EAX = (uint_32)(valdiv / (uint64_t)divisor);
}

OPTINLINE void op_idiv32(uint64_t valdiv, uint_32 divisor) {
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
	REG_EAX = d1;
	REG_EDX = d2;
	*/

	//Same, but with normal instructions!
	union
	{
		uint_64 valdivw;
		int_64 valdivs;
	} dataw1, //For loading the signed value of the registers!
		dataw2; //For performing calculations!

	union
	{
		uint_32 divisorb;
		int_32 divisors;
	} datab1, datab2; //For converting the data to signed values!

	dataw1.valdivw = valdiv; //Load word!
	datab1.divisorb = divisor; //Load divisor!

	dataw2.valdivs = dataw1.valdivs; //Set and...
	dataw2.valdivs /= datab1.divisors; //... Divide!

	datab2.divisors = (int_32)dataw2.valdivs; //Try to load the signed result!
	if ((int_64)dataw2.valdivw != (int_64)datab2.divisors) { CPU_exDIV0(); return; } //Overflow (data loss)!

	REG_EAX = datab2.divisorb; //Divided!
	dataw2.valdivs = dataw1.valdivs; //Reload and...
	dataw2.valdivs %= datab1.divisors; //... Modulo!
	datab1.divisors = (int_32)dataw2.valdivs; //Convert to 8-bit!
	REG_EDX = datab1.divisorb; //Move rest into result!

	//if (valdiv > 0x7FFFFFFF) v1 = valdiv - 0xFFFFFFFF - 1; else v1 = valdiv;
	//if (divisor > 32767) v2 = divisor - 65536; else v2 = divisor;
	//if ((v1/v2) > 65535) { CPU8086_int(0); return; }
	//temp3 = (v1/v2) & 65535;
	//regs.wordregs[regax] = temp3;
	//temp3 = (v1%v2) & 65535;
	//regs.wordregs[regdx] = temp3;
}

void op_grp3_32() {
	//uint32_t d1, d2, s1, s2, sign;
	//word d, s;
	//oper1 = signext(oper1b); oper2 = signext(oper2b);
	//sprintf(msg, "  Oper1: %04X    Oper2: %04X\n", oper1, oper2); print(msg);
	switch (thereg) {
	case 0: case 1: //TEST
		CPU80386_internal_TEST32(oper1d, imm32, 3);
		break;
	case 2: //NOT
		res32 = ~oper1d;
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 16 + MODRM_EA(params); //Mem!
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg!
		}
		break;
	case 3: //NEG
		res32 = (~oper1d) + 1;
		flag_sub32(0, oper1d);
		if (res32) FLAG_CF = 1; else FLAG_CF = 0;
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 16 + MODRM_EA(params); //Mem!
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg!
		}
		break;
	case 4: //MULW
		tempEAX = REG_EAX; //Save a backup for calculating cycles!
		temp1d.val64 = (uint64_t)oper1d * (uint32_t)REG_EAX;
		REG_EAX = temp1d.val32;
		REG_EDX = temp1d.val32high;
		if (REG_EDX) { FLAG_CF = FLAG_OF = 1; }
		else { FLAG_CF = FLAG_OF = 0; }
		//if ((EMULATED_CPU == CPU_8086) && temp1.val32) FLAG_ZF = 0; //8086/8088 clears the Zero flag when not zero only.
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 124 + MODRM_EA(params); //Mem max!
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 118; //Reg!
		}
		if (NumberOfSetBits(tempEAX)>1) //More than 1 bit set?
		{
			CPU[activeCPU].cycles_OP += NumberOfSetBits(tempEAX) - 1; //1 cycle for all bits more than 1 bit set!
		}
		break;
	case 5: //IMULW
		temp1d.val64 = REG_EAX;
		temp2d.val64 = oper1d;
		//Sign extend!
		if (temp1d.val32 & 0x80000000) temp1d.val64 |= 0xFFFFFFFF00000000;
		if (temp2d.val32 & 0x80000000) temp2d.val64 |= 0xFFFFFFFF00000000;
		temp3d.val64s = temp1d.val64s; //Load and...
		temp3d.val64s *= temp2d.val64s; //Signed multiplication!
		REG_EAX = temp3d.val32; //into register ax
		REG_EDX = temp3d.val32high; //into register dx
		FLAG_CF = FLAG_OF = ((int_64)temp3d.val32s != temp3d.val64s) ? 1 : 0; //Overflow occurred?
		FLAG_SF = (REG_EDX >> 15) & 1; //Sign flag is affected!
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 128 + MODRM_EA(params); //Mem max!
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 134; //Reg max!
		}
		break;
	case 6: //DIV
		op_div32(((uint64_t)REG_EDX << 32) | REG_EAX, oper1d);
		if (MODRM_EA(params)) CPU_addWordMemoryTiming386(); /*To memory?*/
		break;
	case 7: //IDIV
		op_idiv32(((uint64_t)REG_EDX << 32) | REG_EAX, oper1d); break;
	}
}

void op_grp5_32() {
	MODRM_PTR info; //To contain the info!
	INLINEREGISTER byte tempCF;
	switch (thereg) {
	case 0: //INC Ev
		oper2d = 1;
		tempCF = FLAG_CF;
		op_add32();
		FLAG_CF = tempCF;
		modrm_write32(&params, 1, res32);
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 15 + MODRM_EA(params); //Mem
			CPU_addWordMemoryTiming386(); /*To memory?*/
			CPU_addWordMemoryTiming386(); /*To memory?*/
		}
		else //Reg?
		{
			CPU[activeCPU].cycles_OP = 2; //Reg
		}
		break;
	case 1: //DEC Ev
		oper2d = 1;
		tempCF = FLAG_CF;
		op_sub32();
		FLAG_CF = tempCF;
		modrm_write32(&params, 1, res32);
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 15 + MODRM_EA(params); //Mem
			CPU_addWordMemoryTiming386(); /*To memory?*/
			CPU_addWordMemoryTiming386(); /*To memory?*/
		}
		else //Reg?
		{
			CPU[activeCPU].cycles_OP = 2; //Reg
		}
		break;
	case 2: //CALL Ev
		CPU_PUSH32(&REG_EIP);
		REG_EIP = oper1d;
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 21 + MODRM_EA(params); /* Intrasegment indirect through memory */
			CPU_addWordMemoryTiming386(); /*To memory?*/
			CPU_addWordMemoryTiming386(); /*To memory?*/
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 16; /* Intrasegment indirect through register */
		}
		CPU_flushPIQ(); //We're jumping to another address!
		break;
	case 3: //CALL Mp
		CPU_PUSH16(&REG_CS); CPU_PUSH32(&REG_EIP);
		modrm_decode32(&params, &info, 1); //Get data!
		destEIP = MMU_rdw(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset, 0);
		segmentWritten(CPU_SEGMENT_CS, MMU_rw(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset + 4, 0), 2);
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 37 + MODRM_EA(params); /* Intersegment indirect */
			CPU_addWordMemoryTiming386(); /*To memory?*/
			CPU_addWordMemoryTiming386(); /*To memory?*/
			CPU_addWordMemoryTiming386(); /*To memory?*/
			CPU_addWordMemoryTiming386(); /*To memory?*/
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 28; /* Intersegment direct */
		}
		break;
	case 4: //JMP Ev
		REG_EIP = oper1d;
		CPU_flushPIQ(); //We're jumping to another address!
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 18 + MODRM_EA(params); /* Intrasegment indirect through memory */
			CPU_addWordMemoryTiming386(); /*To memory?*/
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 11; /* Intrasegment indirect through register */
		}
		break;
	case 5: //JMP Mp
		modrm_decode32(&params, &info, 1); //Get data!
		destEIP = MMU_rdw(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset, 0);
		segmentWritten(CPU_SEGMENT_CS, MMU_rw(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset + 4, 0), 1);
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 24 + MODRM_EA(params); /* Intersegment indirect through memory */
			CPU_addWordMemoryTiming386(); /*To memory?*/
			CPU_addWordMemoryTiming386(); /*To memory?*/
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 11; /* Intersegment indirect through register */
		}
		break;
	case 6: //PUSH Ev
		CPU_PUSH32(&oper1d); break;
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 16 + MODRM_EA(params); /*Push Mem!*/
			CPU_addWordMemoryTiming386(); /*To memory?*/
			CPU_addWordMemoryTiming386(); /*To memory?*/
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/
		}
		break;
	default: //Unknown OPcode?
		CPU_unkOP(); //Execute the unknown opcode exception handler, if any!
		break;
	}
}
