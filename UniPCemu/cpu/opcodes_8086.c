#include "headers/types.h" //Basic types
#include "headers/cpu/cpu.h" //CPU needed!
#include "headers/cpu/mmu.h" //MMU needed!
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
#include "headers/mmu/mmuhandler.h" //MMU_invaddr support!
#include "headers/cpu/biu.h" //BIU support!

MODRM_PARAMS params; //For getting all params for the CPU!
extern byte cpudebugger; //The debugging is on?
extern byte blockREP; //Block the instruction from executing (REP with (E)CX=0

//How many cycles to substract from the documented instruction timings for the raw EU cycles for each BIU access?
#define EU_CYCLES_SUBSTRACT_ACCESSREAD 4
#define EU_CYCLES_SUBSTRACT_ACCESSWRITE 4
#define EU_CYCLES_SUBSTRACT_ACCESSRW 8

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
byte oper1b, oper2b; //Byte variants!
word oper1, oper2; //Word variants!
byte res8; //Result 8-bit!
word res16; //Result 16-bit!
byte thereg; //For function number!
uint_32 ea; //From RM OFfset (GRP5 Opcodes only!)
byte tempCF2;

VAL32Splitter temp1, temp2, temp3, temp4, temp5; //All temporary values!
uint_32 temp32, tempaddr32; //Defined in opcodes_8086.c

extern byte debuggerINT; //Interrupt special trigger?

extern uint_32 immaddr32; //Immediate address, for instructions requiring it, either 16-bits or 32-bits of immediate data, depending on the address size!

OPTINLINE void INTdebugger8086() //Special INTerrupt debugger!
{
	if (DEBUGGER_LOG==DEBUGGERLOG_INT) //Interrupts only?
	{
		debuggerINT = 1; //Debug this instruction always!
	}
}

/*

Start of help for debugging

*/

extern char modrm_param1[256]; //Contains param/reg1
extern char modrm_param2[256]; //Contains param/reg2

char LEAtext[256];
OPTINLINE char *getLEAtext(MODRM_PARAMS *theparams)
{
	modrm_lea16_text(theparams,1,&LEAtext[0]);    //Help function for LEA instruction!
	return &LEAtext[0];
}

/*

Start of help for opcode processing

*/

extern byte CPU_databussize; //0=16/32-bit bus! 1=8-bit bus when possible (8088/80188)!
uint_32 wordaddress; //Word address used during memory access!

OPTINLINE byte CPU8086_software_int(byte interrupt, int_64 errorcode) //See int, but for hardware interrupts (IRQs)!
{
	return call_soft_inthandler(interrupt,errorcode); //Save adress to stack (We're going soft int!)!
}

OPTINLINE byte CPU8086_int(byte interrupt, byte type3) //Software interrupt from us(internal call)!
{
	byte result = 1; //Result!
	CPUPROT1
		if (EMULATED_CPU<=CPU_NECV30) //16-bit CPU?
		{
			result = CPU8086_software_int(interrupt,-1);
			if (result) //Final stage?
			{
				CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/
			}
		}
		else //Unsupported CPU? Use plain general interrupt handling instead!
		{
			CPU8086_software_int(interrupt,-1);
			if (CPU_apply286cycles()) return 1; //80286+ cycles instead?
			result = 1; //Always 1!
		}
		return result; //Finished!
	CPUPROT2
	return result; //Finished!
}

byte CPU086_int(byte interrupt) //Software interrupt (external call)!
{
	return CPU8086_int(interrupt,0); //Direct call!
}

OPTINLINE void CPU8086_IRET()
{
	CPUPROT1
	CPU_IRET(); //IRET!
	CPUPROT2
	if (CPU_apply286cycles()) return; //80286+ cycles instead?
	CPU[activeCPU].cycles_OP += 24; /*Timings!*/
}

/*

List of hardware interrupts:
0: Division by 0: Attempting to execute AAM/DIV/IDIV with divisor==0: IMPLEMENTED
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

//Stack operation support through the BIU!
byte CPU8086_PUSHw(byte base, word *data)
{
	word temp;
	if (CPU[activeCPU].instructionstep==base) //First step? Request!
	{
		if (CPU_PUSH16_BIU(data)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	if (CPU[activeCPU].instructionstep==(base+1))
	{
		if (BIU_readResultw(&temp)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_internal_PUSHw(byte base, word *data)
{
	word temp;
	if (CPU[activeCPU].internalinstructionstep==base) //First step? Request!
	{
		if (CPU_PUSH16_BIU(data)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==(base+1))
	{
		if (BIU_readResultw(&temp)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_internal_interruptPUSHw(byte base, word *data)
{
	word temp;
	if (CPU[activeCPU].internalinterruptstep==base) //First step? Request!
	{
		if (CPU_PUSH16_BIU(data)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinterruptstep; //Next step!
	}
	if (CPU[activeCPU].internalinterruptstep==(base+1))
	{
		if (BIU_readResultw(&temp)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinterruptstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_PUSHb(byte base, byte *data)
{
	byte temp;
	if (CPU[activeCPU].instructionstep==base) //First step? Request!
	{
		if (CPU_PUSH8_BIU(*data)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	if (CPU[activeCPU].instructionstep==(base+1))
	{
		if (BIU_readResultb(&temp)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_internal_PUSHb(byte base, byte *data)
{
	byte temp;
	if (CPU[activeCPU].internalinstructionstep==base) //First step? Request!
	{
		if (CPU_PUSH8_BIU(*data)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==(base+1))
	{
		if (BIU_readResultb(&temp)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_POPw(byte base, word *result)
{
	if (CPU[activeCPU].instructionstep==base) //First step? Request!
	{
		if (CPU_POP16_BIU()==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	if (CPU[activeCPU].instructionstep==(base+1))
	{
		if (BIU_readResultw(result)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_internal_POPw(byte base, word *result)
{
	if (CPU[activeCPU].internalinstructionstep==base) //First step? Request!
	{
		if (CPU_POP16_BIU()==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==(base+1))
	{
		if (BIU_readResultw(result)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_POPSP(byte base)
{
	if (CPU[activeCPU].instructionstep==base) //First step? Request!
	{
		if (BIU_request_MMUrw(CPU_SEGMENT_SS,REG_SP,1)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	if (CPU[activeCPU].instructionstep==(base+1))
	{
		if (BIU_readResultw(&REG_SP)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_POPb(byte base, byte *result)
{
	if (CPU[activeCPU].instructionstep==base) //First step? Request!
	{
		if (CPU_POP8_BIU()==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	if (CPU[activeCPU].instructionstep==(base+1))
	{
		if (BIU_readResultb(result)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

//BIU delay(keeping BIU active)
byte CPU8086_instructionstepdelayBIU(byte base, byte cycles)
{
	if (CPU[activeCPU].instructionstep==base) //First step? Request!
	{
		CPU[activeCPU].cycles_OP += cycles; //Take X cycles only!
		CPU[activeCPU].executed = 0; //Not executed!
		CPU[activeCPU].instructionstep += 2; //Next step, by 2 for compatibility!
		return 1; //Keep running!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_internal_delayBIU(byte base, byte cycles)
{
	if (CPU[activeCPU].internalinstructionstep==base) //First step? Request!
	{
		CPU[activeCPU].cycles_OP += cycles; //Take X cycles only!
		CPU[activeCPU].executed = 0; //Not executed!
		CPU[activeCPU].internalinstructionstep += 2; //Next step, by 2 for compatibility!
		return 1; //Keep running!
	}
	return 0; //Ready to process further! We're loaded!
}

//BUS --- state delay!
byte CPU8086_instructionstepdelayBIUidle(byte base, byte cycles)
{
	if (CPU[activeCPU].instructionstep==base) //First step? Request!
	{
		CPU[activeCPU].cycles_stallBUS += cycles; //Take X cycles only!
		CPU[activeCPU].executed = 0; //Not executed!
		CPU[activeCPU].instructionstep += 2; //Next step, by 2 for compatibility!
		return 1; //Keep running!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_internal_delayBIUidle(byte base, byte cycles)
{
	if (CPU[activeCPU].internalinstructionstep==base) //First step? Request!
	{
		CPU[activeCPU].cycles_stallBUS += cycles; //Take X cycles only!
		CPU[activeCPU].executed = 0; //Not executed!
		CPU[activeCPU].internalinstructionstep += 2; //Next step, by 2 for compatibility!
		return 1; //Keep running!
	}
	return 0; //Ready to process further! We're loaded!
}



//Instruction variants of ModR/M!

byte CPU8086_instructionstepreadmodrmb(byte base, byte *result, byte paramnr) //Base=Start instruction step, result=Pointer to the result container!
{
	byte BIUtype;
	if (CPU[activeCPU].instructionstep==base) //First step? Request!
	{
		if ((BIUtype = modrm_read8_BIU(&params,paramnr,result))==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
		if (BIUtype==2) //Register?
		{
			++CPU[activeCPU].instructionstep; //Skip next step!
		}
	}
	if (CPU[activeCPU].instructionstep==(base+1))
	{
		if (BIU_readResultb(result)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_instructionstepreadmodrmw(byte base, word *result, byte paramnr)
{
	byte BIUtype;
	if (CPU[activeCPU].instructionstep==base) //First step? Request!
	{
		if ((BIUtype = modrm_read16_BIU(&params,paramnr,result))==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
		if (BIUtype==2) //Register?
		{
			++CPU[activeCPU].instructionstep; //Skip next step!
		}
	}
	if (CPU[activeCPU].instructionstep==(base+1))
	{
		if (BIU_readResultw(result)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_instructionstepwritemodrmb(byte base, byte value, byte paramnr) //Base=Start instruction step, result=Pointer to the result container!
{
	byte dummy;
	byte BIUtype;
	if (CPU[activeCPU].instructionstep==base) //First step? Request!
	{
		if ((BIUtype = modrm_write8_BIU(&params,paramnr,value))==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
		if (BIUtype==2) //Register?
		{
			++CPU[activeCPU].instructionstep; //Skip next step!
		}
	}
	if (CPU[activeCPU].instructionstep==(base+1))
	{
		if (BIU_readResultb(&dummy)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_instructionstepwritemodrmw(byte base, word value, byte paramnr, byte isJMPorCALL)
{
	word dummy;
	byte BIUtype;
	if (CPU[activeCPU].instructionstep==base) //First step? Request!
	{
		if ((BIUtype = modrm_write16_BIU(&params,paramnr,value,isJMPorCALL))==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
		if (BIUtype==2) //Register?
		{
			++CPU[activeCPU].instructionstep; //Skip next step!
		}
	}
	if (CPU[activeCPU].instructionstep==(base+1))
	{
		if (BIU_readResultw(&dummy)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

//Now, the internal variants of the functions above!
byte CPU8086_internal_stepreadmodrmb(byte base, byte *result, byte paramnr) //Base=Start instruction step, result=Pointer to the result container!
{
	byte BIUtype;
	if (CPU[activeCPU].internalmodrmstep==base) //First step? Request!
	{
		if ((BIUtype = modrm_read8_BIU(&params,paramnr,result))==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
		if (BIUtype==2) //Register?
		{
			++CPU[activeCPU].internalmodrmstep; //Skip next step!
		}
	}
	if (CPU[activeCPU].internalmodrmstep==(base+1))
	{
		if (BIU_readResultb(result)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_internal_stepreadmodrmw(byte base, word *result, byte paramnr)
{
	byte BIUtype;
	if (CPU[activeCPU].internalmodrmstep==base) //First step? Request!
	{
		if ((BIUtype = modrm_read16_BIU(&params,paramnr,result))==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
		if (BIUtype==2) //Register?
		{
			++CPU[activeCPU].internalmodrmstep; //Skip next step!
		}
	}
	if (CPU[activeCPU].internalmodrmstep==(base+1))
	{
		if (BIU_readResultw(result)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_internal_stepwritemodrmb(byte base, byte value, byte paramnr) //Base=Start instruction step, result=Pointer to the result container!
{
	byte dummy;
	byte BIUtype;
	if (CPU[activeCPU].internalmodrmstep==base) //First step? Request!
	{
		if ((BIUtype = modrm_write8_BIU(&params,paramnr,value))==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
		if (BIUtype==2) //Register?
		{
			++CPU[activeCPU].internalmodrmstep; //Skip next step!
		}
	}
	if (CPU[activeCPU].internalmodrmstep==(base+1))
	{
		if (BIU_readResultb(&dummy)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_internal_stepwritedirectb(byte base, sword segment, word segval, uint_32 offset, byte val, byte is_offset16)
{
	byte dummy;
	if (CPU[activeCPU].internalmodrmstep==base) //First step? Request!
	{
		if (BIU_request_MMUwb(segment,offset,val,is_offset16)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
	}
	if (CPU[activeCPU].internalmodrmstep==(base+1))
	{
		if (BIU_readResultb(&dummy)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_internal_stepwritedirectw(byte base, sword segment, word segval, uint_32 offset, word val, byte is_offset16)
{
	word dummy;
	if (CPU[activeCPU].internalmodrmstep==base) //First step? Request!
	{
		if (BIU_request_MMUww(segment,offset,val,is_offset16)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
	}
	if (CPU[activeCPU].internalmodrmstep==(base+1))
	{
		if (BIU_readResultw(&dummy)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_internal_stepreaddirectb(byte base, sword segment, word segval, uint_32 offset, byte *result, byte is_offset16)
{
	if (CPU[activeCPU].internalmodrmstep==base) //First step? Request!
	{
		if (BIU_request_MMUrb(segment,offset,is_offset16)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
	}
	if (CPU[activeCPU].internalmodrmstep==(base+1))
	{
		if (BIU_readResultb(result)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_internal_stepreaddirectw(byte base, sword segment, word segval, uint_32 offset, word *result, byte is_offset16)
{
	if (CPU[activeCPU].internalmodrmstep==base) //First step? Request!
	{
		if (BIU_request_MMUrw(segment,offset,is_offset16)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
	}
	if (CPU[activeCPU].internalmodrmstep==(base+1))
	{
		if (BIU_readResultw(result)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_internal_stepreadinterruptw(byte base, sword segment, word segval, uint_32 offset, word *result, byte is_offset16)
{
	if (CPU[activeCPU].internalinterruptstep==base) //First step? Request!
	{
		if (BIU_request_MMUrw(segment,offset,is_offset16)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinterruptstep; //Next step!
	}
	if (CPU[activeCPU].internalinterruptstep==(base+1))
	{
		if (BIU_readResultw(result)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinterruptstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU8086_internal_stepwritemodrmw(byte base, word value, byte paramnr, byte isJMPorCALL)
{
	word dummy;
	byte BIUtype;
	if (CPU[activeCPU].internalmodrmstep==base) //First step? Request!
	{
		if ((BIUtype = modrm_write16_BIU(&params,paramnr,value,isJMPorCALL))==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
		if (BIUtype==2) //Register?
		{
			++CPU[activeCPU].internalmodrmstep; //Skip next step!
		}
	}
	if (CPU[activeCPU].internalmodrmstep==(base+1))
	{
		if (BIU_readResultw(&dummy)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalmodrmstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

//Normal support for basic operations
OPTINLINE void CMP_w(word a, word b, byte flags) //Compare instruction!
{
	CPUPROT1
	flag_sub16(a,b); //Flags only!
	if (CPU_apply286cycles()) return; //80286+ cycles instead?
	switch (flags & 7)
	{
	case 0: //Default?
		break; //Unused!
	case 1: //Accumulator?
		CPU[activeCPU].cycles_OP += 4; //Imm-Reg
		break;
	case 2: //Determined by ModR/M?
		if (params.EA_cycles) //Memory is used?
		{
			CPU[activeCPU].cycles_OP += 9-EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem->Reg!
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP += 3; //Reg->Reg!
		}
		break;
	case 3: //ModR/M+imm?
		if (params.EA_cycles) //Memory is used?
		{
			CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem->Reg!
		}
		else //Imm->Reg?
		{
			CPU[activeCPU].cycles_OP += 4; //Reg->Reg!
		}
		break;
	case 4: //Mem-Mem instruction?
		CPU[activeCPU].cycles_OP += 18-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2); //Assume two times Reg->Mem
		break;
	}
	CPUPROT2
}

OPTINLINE void CMP_b(byte a, byte b, byte flags)
{
	CPUPROT1
	flag_sub8(a,b); //Flags only!
	if (CPU_apply286cycles()) return; //80286+ cycles instead?
	switch (flags&7)
	{
	case 0: //Default?
		break; //Unused!
	case 1: //Accumulator?
		CPU[activeCPU].cycles_OP += 4; //Imm-Reg
		break;
	case 2: //Determined by ModR/M?
		if (params.EA_cycles) //Memory is used?
		{
			CPU[activeCPU].cycles_OP += 9-EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem->Reg!
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP += 3; //Reg->Reg!
		}
		break;
	case 3: //ModR/M+imm?
		if (params.EA_cycles) //Memory is used?
		{
			CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem->Reg!
		}
		else //Imm->Reg?
		{
			CPU[activeCPU].cycles_OP += 4; //Reg->Reg!
		}
		break;
	case 4: //Mem-Mem instruction?
		CPU[activeCPU].cycles_OP += 18-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2); //Assume two times Reg->Mem
		break;
	}
	CPUPROT2
}

//Modr/m support, used when reg=NULL and custommem==0
extern byte MODRM_src0; //What source is our modr/m? (1/2)
extern byte MODRM_src1; //What source is our modr/m? (1/2)

//Custom memory support!
byte custommem = 0; //Used in some instructions!
uint_32 customoffset; //Offset to use!

//Help functions:
OPTINLINE byte CPU8086_internal_INC16(word *reg)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	//Check for exceptions first!
	if (!reg) if (modrm_check16(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!reg) if (modrm_check16(&params,MODRM_src0,0)) return 1; //Abort on fault!
	CPUPROT1
	INLINEREGISTER byte tempCF = FLAG_CF; //CF isn't changed!
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (reg==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmw(0,&oper1,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1 = reg?*reg:oper1;
		oper2 = 1;
		op_add16();
		FLAGW_CF(tempCF);
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		if (reg==NULL) //Destination to write?
		{
			if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
			{
				CPU[activeCPU].cycles_OP += 15-(EU_CYCLES_SUBSTRACT_ACCESSRW); //Mem
			}
			CPU[activeCPU].executed = 0;
			return 1; //Wait for execution phase to finish!
		}
	}
	if (reg) //Register?
	{
		*reg = res16;
		if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
		{
			CPU[activeCPU].cycles_OP += 2; //16-bit reg!
		}
	}
	else //Memory?
	{
		if (reg==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmw(2,res16,MODRM_src0,0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}
OPTINLINE byte CPU8086_internal_DEC16(word *reg)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	if (!reg) if (modrm_check16(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!reg) if (modrm_check16(&params,MODRM_src0,0)) return 1; //Abort on fault!
	CPUPROT1
	INLINEREGISTER byte tempCF = FLAG_CF; //CF isn't changed!
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (reg==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmw(0,&oper1,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1 = reg?*reg:oper1;
		oper2 = 1;
		op_sub16();
		FLAGW_CF(tempCF);
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		if (reg==NULL) //Destination to write?
		{
			if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
			{
				CPU[activeCPU].cycles_OP += 15-(EU_CYCLES_SUBSTRACT_ACCESSRW); //Mem
			}
			CPU[activeCPU].executed = 0;
			return 1; //Wait for execution phase to finish!
		}
	}
	if (reg) //Register?
	{
		*reg = res16;
		if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
		{
			CPU[activeCPU].cycles_OP += 2; //16-bit reg!
		}
	}
	else //Memory?
	{
		if (reg==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmw(2,res16,MODRM_src0,0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}

OPTINLINE byte CPU8086_internal_INC8(byte *reg)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	if (!reg) if (modrm_check8(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!reg) if (modrm_check8(&params,MODRM_src0,0)) return 1; //Abort on fault!
	CPUPROT1
	INLINEREGISTER byte tempCF = FLAG_CF; //CF isn't changed!
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (reg==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmb(0,&oper1b,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1b = reg?*reg:oper1b;
		oper2b = 1;
		op_add8();
		FLAGW_CF(tempCF);
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		if (reg==NULL) //Destination to write?
		{
			if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
			{
				CPU[activeCPU].cycles_OP += 15-(EU_CYCLES_SUBSTRACT_ACCESSRW); //Mem
			}
			CPU[activeCPU].executed = 0;
			return 1; //Wait for execution phase to finish!
		}
	}
	if (reg) //Register?
	{
		*reg = res8;
		if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
		{
			CPU[activeCPU].cycles_OP += 3; //8-bit reg!
		}
	}
	else //Memory?
	{
		if (reg==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmb(2,res8,MODRM_src0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}
OPTINLINE byte CPU8086_internal_DEC8(byte *reg)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	CPUPROT1
	if (!reg) if (modrm_check8(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!reg) if (modrm_check8(&params,MODRM_src0,0)) return 1; //Abort on fault!
	INLINEREGISTER byte tempCF = FLAG_CF;
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (reg==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmb(0,&oper1b,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1b = reg?*reg:oper1b;
		oper2b = 1;
		op_sub8();
		FLAGW_CF(tempCF);
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		if (reg==NULL) //Destination to write?
		{
			if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
			{
				CPU[activeCPU].cycles_OP += 15-(EU_CYCLES_SUBSTRACT_ACCESSRW); //Mem
			}
			CPU[activeCPU].executed = 0;
			return 1; //Wait for execution phase to finish!
		}
	}
	if (reg) //Register?
	{
		*reg = res8;
		if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
		{
			CPU[activeCPU].cycles_OP += 3; //8-bit reg!
		}
	}
	else //Memory?
	{
		if (reg==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmb(2,res8,MODRM_src0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}

OPTINLINE void timing_AND_OR_XOR_ADD_SUB8(byte *dest, byte flags)
{
	if (CPU_apply286cycles()) return; //No 80286+ cycles instead?
	switch (flags) //What type of operation?
	{
	case 0: //Reg+Reg?
		CPU[activeCPU].cycles_OP += 3; //Reg->Reg!
		break;
	case 1: //Reg+imm?
		CPU[activeCPU].cycles_OP += 4; //Accumulator!
		break;
	case 2: //Determined by ModR/M?
		if (params.EA_cycles) //Memory is used?
		{
			if (dest) //Mem->Reg?
			{
				CPU[activeCPU].cycles_OP += 9-EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem->Reg!
			}
			else //Reg->Mem?
			{
				CPU[activeCPU].cycles_OP += 16-(EU_CYCLES_SUBSTRACT_ACCESSRW); //Mem->Reg!
			}
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP += 3; //Reg->Reg!
		}
		break;
	case 3: //ModR/M+imm?
		if (params.EA_cycles) //Memory is used?
		{
			if (dest) //Imm->Reg?
			{
				CPU[activeCPU].cycles_OP += 4; //Imm->Reg!
			}
			else //Imm->Mem?
			{
				CPU[activeCPU].cycles_OP += 17-(EU_CYCLES_SUBSTRACT_ACCESSRW); //Mem->Reg!
			}
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP += 3; //Reg->Reg!
		}
		break;
	}
}

OPTINLINE void timing_AND_OR_XOR_ADD_SUB16(word *dest, byte flags)
{
	if (CPU_apply286cycles()) return; //No 80286+ cycles instead?
	switch (flags) //What type of operation?
	{
	case 0: //Reg+Reg?
		CPU[activeCPU].cycles_OP += 3; //Reg->Reg!
		break;
	case 1: //Reg+imm?
		CPU[activeCPU].cycles_OP += 4; //Accumulator!
		break;
	case 2: //Determined by ModR/M?
		if (params.EA_cycles) //Memory is used?
		{
			if (dest) //Mem->Reg?
			{
				CPU[activeCPU].cycles_OP += 9-EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem->Reg!
			}
			else //Reg->Mem?
			{
				CPU[activeCPU].cycles_OP += 16-(EU_CYCLES_SUBSTRACT_ACCESSRW); //Mem->Reg!
			}
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP += 3; //Reg->Reg!
		}
		break;
	case 3: //ModR/M+imm?
		if (params.EA_cycles) //Memory is used?
		{
			if (dest) //Imm->Reg?
			{
				CPU[activeCPU].cycles_OP += 4; //Imm->Reg!
			}
			else //Imm->Mem?
			{
				CPU[activeCPU].cycles_OP += 17-(EU_CYCLES_SUBSTRACT_ACCESSRW); //Mem->Reg!
			}
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP += 3; //Reg->Reg!
		}
		break;
	}
}

//For ADD
OPTINLINE byte CPU8086_internal_ADD8(byte *dest, byte addition, byte flags)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	if (!dest) if (modrm_check8(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!dest) if (modrm_check8(&params,MODRM_src0,0)) return 1; //Abort on fault!
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmb(0,&oper1b,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1b = dest?*dest:oper1b;
		oper2b = addition;
		op_add8();
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		timing_AND_OR_XOR_ADD_SUB8(dest, flags);
		if (dest==NULL) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmb(2,res8,MODRM_src0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}
OPTINLINE byte CPU8086_internal_ADD16(word *dest, word addition, byte flags)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	if (!dest) if (modrm_check16(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!dest) if (modrm_check16(&params,MODRM_src0,0)) return 1; //Abort on fault!
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmw(0,&oper1,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1 = dest?*dest:oper1;
		oper2 = addition;
		op_add16();
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		timing_AND_OR_XOR_ADD_SUB16(dest, flags);
		if (dest==NULL) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmw(2,res16,MODRM_src0,0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}

//For ADC
OPTINLINE byte CPU8086_internal_ADC8(byte *dest, byte addition, byte flags)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	if (!dest) if (modrm_check8(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!dest) if (modrm_check8(&params,MODRM_src0,0)) return 1; //Abort on fault!
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmb(0,&oper1b,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1b = dest?*dest:oper1b;
		oper2b = addition;
		op_adc8();
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		timing_AND_OR_XOR_ADD_SUB8(dest, flags);
		if (dest==NULL) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmb(2,res8,MODRM_src0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}
OPTINLINE byte CPU8086_internal_ADC16(word *dest, word addition, byte flags)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	if (!dest) if (modrm_check16(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!dest) if (modrm_check16(&params,MODRM_src0,0)) return 1; //Abort on fault!
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmw(0,&oper1,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1 = dest?*dest:oper1;
		oper2 = addition;
		op_adc16();
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		timing_AND_OR_XOR_ADD_SUB16(dest, flags);
		if (dest==NULL) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmw(2,res16,MODRM_src0,0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}


//For OR
OPTINLINE byte CPU8086_internal_OR8(byte *dest, byte src, byte flags)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	if (!dest) if (modrm_check8(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!dest) if (modrm_check8(&params,MODRM_src0,0)) return 1; //Abort on fault!
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmb(0,&oper1b,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1b = dest?*dest:oper1b;
		oper2b = src;
		op_or8();
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		timing_AND_OR_XOR_ADD_SUB8(dest, flags);
		if (dest==NULL) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmb(2,res8,MODRM_src0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}
OPTINLINE byte CPU8086_internal_OR16(word *dest, word src, byte flags)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	if (!dest) if (modrm_check16(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!dest) if (modrm_check16(&params,MODRM_src0,0)) return 1; //Abort on fault!
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmw(0,&oper1,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1 = dest?*dest:oper1;
		oper2 = src;
		op_or16();
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		timing_AND_OR_XOR_ADD_SUB16(dest, flags);
		if (dest==NULL) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmw(2,res16,MODRM_src0,0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}

//For AND
OPTINLINE byte CPU8086_internal_AND8(byte *dest, byte src, byte flags)
{
	if (!dest) if (modrm_check8(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!dest) if (modrm_check8(&params,MODRM_src0,0)) return 1; //Abort on fault!
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmb(0,&oper1b,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1b = dest?*dest:oper1b;
		oper2b = src;
		op_and8();
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		timing_AND_OR_XOR_ADD_SUB8(dest, flags);
		if (dest==NULL) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmb(2,res8,MODRM_src0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}
OPTINLINE byte CPU8086_internal_AND16(word *dest, word src, byte flags)
{
	if (modrm_check16(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!dest) if (modrm_check16(&params,MODRM_src0,0)) return 1; //Abort on fault on write only!
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmw(0,&oper1,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1 = dest?*dest:oper1;
		oper2 = src;
		op_and16();
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		timing_AND_OR_XOR_ADD_SUB16(dest, flags);
		if (dest==NULL) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmw(2,res16,MODRM_src0,0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}


//For SUB
OPTINLINE byte CPU8086_internal_SUB8(byte *dest, byte addition, byte flags)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	if (modrm_check8(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!dest) if (modrm_check8(&params,MODRM_src0,0)) return 1; //Abort on fault on write only!
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmb(0,&oper1b,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1b = dest?*dest:oper1b;
		oper2b = addition;
		op_sub8();
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		timing_AND_OR_XOR_ADD_SUB8(dest, flags);
		if (dest==NULL) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmb(2,res8,MODRM_src0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}
OPTINLINE byte CPU8086_internal_SUB16(word *dest, word addition, byte flags)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	if (modrm_check16(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!dest) if (modrm_check16(&params,MODRM_src0,0)) return 1; //Abort on fault on write only!
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmw(0,&oper1,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1 = dest?*dest:oper1;
		oper2 = addition;
		op_sub16();
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		timing_AND_OR_XOR_ADD_SUB16(dest, flags);
		if (dest==NULL) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmw(2,res16,MODRM_src0,0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}

//For SBB
OPTINLINE byte CPU8086_internal_SBB8(byte *dest, byte addition, byte flags)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	if (!dest) if (modrm_check8(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!dest) if (modrm_check8(&params,MODRM_src0,0)) return 1; //Abort on fault!
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmb(0,&oper1b,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1b = dest?*dest:oper1b;
		oper2b = addition;
		op_sbb8();
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		timing_AND_OR_XOR_ADD_SUB8(dest, flags);
		if (dest==NULL) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmb(2,res8,MODRM_src0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}
OPTINLINE byte CPU8086_internal_SBB16(word *dest, word addition, byte flags)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	if (!dest) if (modrm_check16(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!dest) if (modrm_check16(&params,MODRM_src0,0)) return 1; //Abort on fault!
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmw(0,&oper1,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1 = dest?*dest:oper1;
		oper2 = addition;
		op_sbb16();
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		timing_AND_OR_XOR_ADD_SUB16(dest, flags);
		if (dest==NULL) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmw(2,res16,MODRM_src0,0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}

//For XOR
//See AND, but XOR
OPTINLINE byte CPU8086_internal_XOR8(byte *dest, byte src, byte flags)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	if (!dest) if (modrm_check8(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!dest) if (modrm_check8(&params,MODRM_src0,0)) return 1; //Abort on fault!
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmb(0,&oper1b,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1b = dest?*dest:oper1b;
		oper2b = src;
		op_xor8();
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		timing_AND_OR_XOR_ADD_SUB8(dest, flags);
		if (dest==NULL) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmb(2,res8,MODRM_src0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}
OPTINLINE byte CPU8086_internal_XOR16(word *dest, word src, byte flags)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	if (!dest) if (modrm_check16(&params,MODRM_src0,1)) return 1; //Abort on fault!
	if (!dest) if (modrm_check16(&params,MODRM_src0,0)) return 1; //Abort on fault!
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepreadmodrmw(0,&oper1,MODRM_src0)) return 1;
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		oper1 = dest?*dest:oper1;
		oper2 = src;
		op_xor16();
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		timing_AND_OR_XOR_ADD_SUB16(dest, flags);
		if (dest==NULL) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		if (dest==NULL) //Needs a read from memory?
		{
			if (CPU8086_internal_stepwritemodrmw(2,res16,MODRM_src0,0)) return 1;
		}
	}
	CPUPROT2
	return 0;
}

//TEST : same as AND, but discarding the result!
OPTINLINE byte CPU8086_internal_TEST8(byte dest, byte src, byte flags)
{
	CPUPROT1
	oper1b = dest;
	oper2b = src;
	op_and8();
	//We don't write anything back for TEST, so only execution step is used!
	//Adjust timing for TEST!
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		switch (flags) //What type of operation?
		{
		case 0: //Reg+Reg?
			CPU[activeCPU].cycles_OP += 3; //Reg->Reg!
			break;
		case 1: //Reg+imm?
			CPU[activeCPU].cycles_OP += 4; //Accumulator!
			break;
		case 2: //Determined by ModR/M?
			if (params.EA_cycles) //Memory is used?
			{
				//Mem->Reg/Reg->Mem?
				CPU[activeCPU].cycles_OP += 9-EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem->Reg!
			}
			else //Reg->Reg?
			{
				CPU[activeCPU].cycles_OP += 3; //Reg->Reg!
			}
			break;
		case 3: //ModR/M+imm?
			if (params.EA_cycles) //Memory is used?
			{
				if (dest) //Imm->Reg?
				{
					CPU[activeCPU].cycles_OP += 5; //Imm->Reg!
				}
				else //Imm->Mem?
				{
					CPU[activeCPU].cycles_OP += 11-EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem->Reg!
				}
			}
			else //Reg->Reg?
			{
				CPU[activeCPU].cycles_OP += 3; //Reg->Reg!
			}
			break;
		}
	}
	CPUPROT2
	return 0;
}

OPTINLINE byte CPU8086_internal_TEST16(word dest, word src, byte flags)
{
	CPUPROT1
	oper1 = dest;
	oper2 = src;
	op_and16();
	//We don't write anything back for TEST, so only execution step is used!
	//Adjust timing for TEST!
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		switch (flags) //What type of operation?
		{
		case 0: //Reg+Reg?
			CPU[activeCPU].cycles_OP += 3; //Reg->Reg!
			break;
		case 1: //Reg+imm?
			CPU[activeCPU].cycles_OP += 4; //Accumulator!
			break;
		case 2: //Determined by ModR/M?
			if (params.EA_cycles) //Memory is used?
			{
				//Mem->Reg/Reg->Mem?
				CPU[activeCPU].cycles_OP += 9-EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem->Reg!
			}
			else //Reg->Reg?
			{
				CPU[activeCPU].cycles_OP += 3; //Reg->Reg!
			}
			break;
		case 3: //ModR/M+imm?
			if (params.EA_cycles) //Memory is used?
			{
				if (dest) //Imm->Reg?
				{
					CPU[activeCPU].cycles_OP += 5; //Imm->Reg!
				}
				else //Imm->Mem?
				{
					CPU[activeCPU].cycles_OP += 11-EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem->Reg!
				}
			}
			else //Reg->Reg?
			{
				CPU[activeCPU].cycles_OP += 3; //Reg->Reg!
			}
			break;
		}
	}
	CPUPROT2
	return 0;
}

//Universal DIV instruction for x86 DIV instructions!
/*

Parameters:
	val: The value to divide
	divisor: The value to divide by
	quotient: Quotient result container
	remainder: Remainder result container
	error: 1 on error(DIV0), 0 when valid.
	resultbits: The amount of bits the result contains(16 or 8 on 8086) of quotient and remainder.
	SHLcycle: The amount of cycles for each SHL.
	ADDSUBcycle: The amount of cycles for ADD&SUB instruction to execute.

*/
void CPU8086_internal_DIV(uint_32 val, word divisor, word *quotient, word *remainder, byte *error, byte resultbits, byte SHLcycle, byte ADDSUBcycle, byte *applycycles)
{
	uint_32 temp, temp2, currentquotient; //Remaining value and current divisor!
	byte shift; //The shift to apply! No match on 0 shift is done!
	temp = val; //Load the value to divide!
	*applycycles = 1; //Default: apply the cycles normally!
	if (divisor==0) //Not able to divide?
	{
		*quotient = 0;
		*remainder = temp; //Unable to comply!
		*error = 1; //Divide by 0 error!
		return; //Abort: division by 0!
	}

	if (CPU_apply286cycles()) /* No 80286+ cycles instead? */
	{
		SHLcycle = ADDSUBcycle = 0; //Don't apply the cycle counts for this instruction!
		*applycycles = 0; //Don't apply the cycles anymore!
	}

	temp = val; //Load the remainder to use!
	*quotient = 0; //Default: we have nothing after division! 
	nextstep:
	//First step: calculate shift so that (divisor<<shift)<=remainder and ((divisor<<(shift+1))>remainder)
	temp2 = divisor; //Load the default divisor for x1!
	if (temp2>temp) //Not enough to divide? We're done!
	{
		goto gotresult; //We've gotten a result!
	}
	currentquotient = 1; //We're starting with x1 factor!
	for (shift=0;shift<(resultbits+1);++shift) //Check for the biggest factor to apply(we're going from bit 0 to maxbit)!
	{
		if ((temp2<=temp) && ((temp2<<1)>temp)) //Found our value to divide?
		{
			CPU[activeCPU].cycles_OP += SHLcycle; //We're taking 1 more SHL cycle for this!
			break; //We've found our shift!
		}
		temp2 <<= 1; //Shift to the next position!
		currentquotient <<= 1; //Shift to the next result!
		CPU[activeCPU].cycles_OP += SHLcycle; //We're taking 1 SHL cycle for this! Assuming parallel shifting!
	}
	if (shift==(resultbits+1)) //We've overflown? We're too large to divide!
	{
		*error = 1; //Raise divide by 0 error due to overflow!
		return; //Abort!
	}
	//Second step: substract divisor<<n from remainder and increase result with 1<<n.
	temp -= temp2; //Substract divisor<<n from remainder!
	*quotient += currentquotient; //Increase result(divided value) with the found power of 2 (1<<n).
	CPU[activeCPU].cycles_OP += ADDSUBcycle; //We're taking 1 substract and 1 addition cycle for this(ADD/SUB register take 3 cycles)!
	goto nextstep; //Start the next step!
	//Finished when remainder<divisor or remainder==0.
	gotresult: //We've gotten a result!
	if (temp>((1<<resultbits)-1)) //Modulo overflow?
	{
		*error = 1; //Raise divide by 0 error due to overflow!
		return; //Abort!		
	}
	if (*quotient>((1<<resultbits)-1)) //Quotient overflow?
	{
		*error = 1; //Raise divide by 0 error due to overflow!
		return; //Abort!		
	}
	*remainder = temp; //Give the modulo! The result is already calculated!
	*error = 0; //We're having a valid result!
}

void CPU8086_internal_IDIV(uint_32 val, word divisor, word *quotient, word *remainder, byte *error, byte resultbits, byte SHLcycle, byte ADDSUBcycle, byte *applycycles)
{
	byte quotientnegative, remaindernegative; //To toggle the result and apply sign after and before?
	quotientnegative = remaindernegative = 0; //Default: don't toggle the result not remainder!
	if (((val>>31)!=(divisor>>15))) //Are we to change signs on the result? The result is negative instead! (We're a +/- or -/+ division)
	{
		quotientnegative = 1; //We're to toggle the result sign if not zero!
	}
	if (val&0x80000000) //Negative value to divide?
	{
		val = ((~val)+1); //Convert the negative value to be positive!
		remaindernegative = 1; //We're to toggle the remainder is any, because the value to divide is negative!
	}
	if (divisor&0x8000) //Negative divisor? Convert to a positive divisor!
	{
		divisor = ((~divisor)+1); //Convert the divisor to be positive!
	}
	CPU8086_internal_DIV(val,divisor,quotient,remainder,error,resultbits-1,SHLcycle,ADDSUBcycle,applycycles); //Execute the division as an unsigned division!
	if (*error==0) //No error has occurred? Do post-processing of the results!
	{
		if (quotientnegative) //The result is negative?
		{
			*quotient = (~*quotient)+1; //Apply the new sign to the result!
		}
		if (remaindernegative) //The remainder is negative?
		{
			*remainder = (~*remainder)+1; //Apply the new sign to the remainder!
		}
	}
}

//MOV
OPTINLINE byte CPU8086_internal_MOV8(byte *dest, byte val, byte flags)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step? Execution only!
	{
		if (dest) //Register?
		{
			*dest = val;
			if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
			{
				switch (flags) //What type are we?
				{
				case 0: //Reg+Reg?
					break; //Unused!
				case 1: //Accumulator from immediate memory address?
					CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSWRITE; //[imm16]->Accumulator!
					break;
				case 2: //ModR/M Memory->Reg?
					if (MODRM_EA(params)) //Memory?
					{
						CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSWRITE; //Mem->Reg!
					}
					else //Reg->Reg?
					{
						CPU[activeCPU].cycles_OP += 2; //Reg->Reg!
					}
					break;
				case 3: //ModR/M Memory immediate->Reg?
					if (MODRM_EA(params)) //Memory?
					{
						CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSWRITE; //Mem->Reg!
					}
					else //Reg->Reg?
					{
						CPU[activeCPU].cycles_OP += 2; //Reg->Reg!
					}
					break;
				case 4: //Register immediate->Reg?
					CPU[activeCPU].cycles_OP += 4; //Reg->Reg!
					break;
				case 8: //SegReg->Reg?
					if (MODRM_src0 || (MODRM_EA(params)==0)) //From register?
					{
						CPU[activeCPU].cycles_OP += 2; //Reg->SegReg!
					}
					else //From memory?
					{
						CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSWRITE; //Mem->SegReg!
					}
					break;
				}
			}
			++CPU[activeCPU].internalinstructionstep; //Skip the writeback step!
		}
		else //Memory destination?
		{
			if (custommem)
			{
				if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),customoffset,0,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
				{
					return 1; //Abort on fault!
				}
				if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
				{
					CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSWRITE; //Accumulator->[imm16]!
				}
			}
			else //ModR/M?
			{
				if (modrm_check8(&params,MODRM_src0,0)) return 1; //Abort on fault!
				if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
				{
					switch (flags) //What type are we?
					{
					case 0: //Reg+Reg?
						break; //Unused!
					case 1: //Accumulator from immediate memory address?
						CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSREAD; //Accumulator->[imm16]!
						break;
					case 2: //ModR/M Memory->Reg?
						if (MODRM_EA(params)) //Memory?
						{
							CPU[activeCPU].cycles_OP += 9-EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem->Reg!
						}
						else //Reg->Reg?
						{
							CPU[activeCPU].cycles_OP += 2; //Reg->Reg!
						}
						break;
					case 3: //ModR/M Memory immediate->Reg?
						if (MODRM_EA(params)) //Memory?
						{
							CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem->Reg!
						}
						else //Reg->Reg?
						{
							CPU[activeCPU].cycles_OP += 4; //Reg->Reg!
						}
						break;
					case 4: //Register immediate->Reg (Non-existant!!!)?
						CPU[activeCPU].cycles_OP += 4; //Reg->Reg!
						break;
					case 8: //Reg->SegReg?
						if (MODRM_src0 || (MODRM_EA(params) == 0)) //From register?
						{
							CPU[activeCPU].cycles_OP += 2; //SegReg->Reg!
						}
						else //From memory?
						{
							CPU[activeCPU].cycles_OP += 9-EU_CYCLES_SUBSTRACT_ACCESSREAD; //SegReg->Mem!
						}
						break;
					}
				}
			}
			++CPU[activeCPU].internalinstructionstep; //Next internal instruction step: memory access!
			CPU[activeCPU].executed = 0; return 1; //Wait for execution phase to finish!
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step: memory access!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		if (custommem)
		{
			if (CPU8086_internal_stepwritedirectb(0,CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),customoffset,val,!CPU_Address_size[activeCPU])) return 1; //Write to memory directly!
		}
		else //ModR/M?
		{
			if (CPU8086_internal_stepwritemodrmb(0,val,MODRM_src0)) return 1; //Write the result to memory!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	CPUPROT2
	return 0;
}

OPTINLINE byte CPU8086_internal_MOV16(word *dest, word val, byte flags)
{
	if (MMU_invaddr())
	{
		return 1;
	}
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==0) //First step? Execution only!
	{
		if (dest) //Register?
		{
			destEIP = REG_EIP; //Store (E)IP for safety!
			modrm_updatedsegment(dest,val,0); //Check for an updated segment!
			CPUPROT1
			*dest = val;
			if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
			{
				switch (flags) //What type are we?
				{
				case 0: //Reg+Reg?
					break; //Unused!
				case 1: //Accumulator from immediate memory address?
					CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSREAD; //[imm16]->Accumulator!
					break;
				case 2: //ModR/M Memory->Reg?
					if (MODRM_EA(params)) //Memory?
					{
						CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSWRITE; //Mem->Reg!
					}
					else //Reg->Reg?
					{
						CPU[activeCPU].cycles_OP += 2; //Reg->Reg!
					}
					break;
				case 3: //ModR/M Memory immediate->Reg?
					if (MODRM_EA(params)) //Memory?
					{
						CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem->Reg!
					}
					else //Reg->Reg?
					{
						CPU[activeCPU].cycles_OP += 2; //Reg->Reg!
					}
					break;
				case 4: //Register immediate->Reg?
					CPU[activeCPU].cycles_OP += 4; //Reg->Reg!
					break;
				case 8: //SegReg->Reg?
					if (MODRM_src0 || (MODRM_EA(params) == 0)) //From register?
					{
						CPU[activeCPU].cycles_OP += 2; //Reg->SegReg!
					}
					else //From memory?
					{
						CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem->SegReg!
					}
					break;
				}
			}
			CPUPROT2
			++CPU[activeCPU].internalinstructionstep; //Skip the memory step!
		}
		else //Memory?
		{
			if (custommem)
			{
				if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),customoffset,0,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
				{
					return 1; //Abort on fault!
				}
				if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),customoffset+1,0,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
				{
					return 1; //Abort on fault!
				}
				if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
				{
					CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSWRITE; //Accumulator->[imm16]!
				}
			}
			else //ModR/M?
			{
				if (modrm_check16(&params,MODRM_src0,0)) return 1; //Abort on fault!
				if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
				{
					switch (flags) //What type are we?
					{
					case 0: //Reg+Reg?
						break; //Unused!
					case 1: //Accumulator from immediate memory address?
						CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSWRITE; //Accumulator->[imm16]!
						break;
					case 2: //ModR/M Memory->Reg?
						if (MODRM_EA(params)) //Memory?
						{
							CPU[activeCPU].cycles_OP += 9-EU_CYCLES_SUBSTRACT_ACCESSWRITE; //Mem->Reg!
						}
						else //Reg->Reg?
						{
							CPU[activeCPU].cycles_OP += 2; //Reg->Reg!
						}
						break;
					case 3: //ModR/M Memory immediate->Reg?
						if (MODRM_EA(params)) //Memory?
						{
							CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSWRITE; //Mem->Reg!
						}
						else //Reg->Reg?
						{
							CPU[activeCPU].cycles_OP += 4; //Reg->Reg!
						}
						break;
					case 4: //Register immediate->Reg (Non-existant!!!)?
						CPU[activeCPU].cycles_OP += 4; //Reg->Reg!
						break;
					case 8: //Reg->SegReg?
						if (MODRM_src0 || (MODRM_EA(params) == 0)) //From register?
						{
							CPU[activeCPU].cycles_OP += 2; //SegReg->Reg!
						}
						else //From memory?
						{
							CPU[activeCPU].cycles_OP += 9-EU_CYCLES_SUBSTRACT_ACCESSWRITE; //SegReg->Mem!
						}
						break;
					}
				}
			}
			++CPU[activeCPU].internalinstructionstep; //Next internal instruction step: memory access!
			CPU[activeCPU].executed = 0; return 1; //Wait for execution phase to finish!
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step: memory access!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //Execution step?
	{
		if (custommem)
		{
			if (CPU8086_internal_stepwritedirectw(0,CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),customoffset,val,!CPU_Address_size[activeCPU])) return 1; //Write to memory directly!
		}
		else //ModR/M?
		{
			if (CPU8086_internal_stepwritemodrmw(0,val,MODRM_src0,0)) return 1; //Write the result to memory!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	CPUPROT2
	return 0;
}

//LEA for LDS, LES
OPTINLINE word getLEA(MODRM_PARAMS *theparams)
{
	return modrm_lea16(theparams,1);
}


/*

Non-logarithmic opcodes!

*/


byte CPU8086_internal_DAA()
{
	word ALVAL, oldCF;
	CPUPROT1
	oldCF = FLAG_CF; //Save old Carry!
	ALVAL = (word)REG_AL;
	if (((ALVAL&0xF)>9) || FLAG_AF)
	{
		oper1 = ALVAL+6;
		ALVAL = (oper1&0xFF);
		FLAGW_CF((((oper1&0xFF00)>0)?1:0)|FLAG_CF);
		FLAGW_AF(1);
	}
	else FLAGW_AF(0);
	if (((REG_AL)>0x99) || oldCF)
	{
		ALVAL += 0x60;
		FLAGW_CF(1);
	}
	else
	{
		FLAGW_CF(0);
	}
	REG_AL = (byte)(ALVAL&0xFF); //Write the value back to AL!
	flag_szp8(REG_AL);
	//if (ALVAL&0xFF00) FLAGW_OF(1); else FLAGW_OF(0); //Undocumented: Overflow flag!
	CPUPROT2
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		CPU[activeCPU].cycles_OP += 4; //Timings!
	}
	return 0;
}
byte CPU8086_internal_DAS()
{
	INLINEREGISTER byte tempCF, tempAL;
	INLINEREGISTER word bigAL;
	bigAL = (word)(tempAL = REG_AL);
	tempCF = FLAG_CF; //Save old values!
	CPUPROT1
	if (((bigAL&0xF)>9) || FLAG_AF)
	{
		oper1 = bigAL = REG_AL-6;
		REG_AL = oper1&255;
		FLAGW_CF(tempCF|((oper1&0xFF00)>0));
		FLAGW_AF(1);
	}
	else FLAGW_AF(0);

	if ((tempAL>0x99) || tempCF)
	{
		bigAL -= 0x60;
		REG_AL = (byte)(bigAL&0xFF);
		FLAGW_CF(1);
	}
	else
	{
		FLAGW_CF(0);
	}
	flag_szp8(REG_AL);
	//if (bigAL&0xFF00) FLAGW_OF(1); else FLAGW_OF(0); //Undocumented: Overflow flag!
	CPUPROT2
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		CPU[activeCPU].cycles_OP += 4; //Timings!
	}
	return 0;
}
byte CPU8086_internal_AAA()
{
	CPUPROT1
	if (EMULATED_CPU<CPU_80286) //Before new CPU?
	{
		if (((REG_AL&0xF)>9) || FLAG_AF)
		{
			REG_AL += 6;
			++REG_AH;
			FLAGW_AF(1);
			FLAGW_CF(1);
		}
		else
		{
			FLAGW_AF(0);
			FLAGW_CF(0);
		}
		REG_AL &= 0xF;
	}
	else //Newer CPUs?
	{
		if (((REG_AL&0xF)>9) || FLAG_AF)
		{
			REG_AX += 0x0106;
			FLAGW_AF(1);
			FLAGW_CF(1);
		}
		else
		{
			FLAGW_AF(0);
			FLAGW_CF(0);
		}
		REG_AL &= 0xF;
	}
	//flag_szp8(REG_AL); //Basic flags!
	flag_p8(REG_AL); //Parity is affected!
	if (EMULATED_CPU<CPU_80286) //Before new CPU?
	{
		FLAGW_ZF((REG_AL==0)?1:0); //Zero is affected!
	}
	else
	{
		FLAGW_ZF((REG_AL==0)?1:0); //Zero is affected!
		FLAGW_SF(0); //Clear Sign!
	}
	//z=s=p=o=?
	CPUPROT2
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		CPU[activeCPU].cycles_OP += 4; //Timings!
	}
	return 0;
}
byte CPU8086_internal_AAS()
{
	CPUPROT1
	if (EMULATED_CPU<CPU_80286) //Before new CPU?
	{
		if (((REG_AL&0xF)>9) || FLAG_AF)
		{
			REG_AL -= 6;
			--REG_AH;
			FLAGW_AF(1);
			FLAGW_CF(1);
		}
		else
		{
			FLAGW_AF(0);
			FLAGW_CF(0);
		}
		REG_AL &= 0xF;
	}
	else //Newer CPUs?
	{
		if (((REG_AL&0xF)>9) || FLAG_AF)
		{
			REG_AX -= 0x0106;
			FLAGW_AF(1);
			FLAGW_CF(1);
		}
		else
		{
			FLAGW_AF(0);
			FLAGW_CF(0);
		}
		REG_AL &= 0xF;
	}
	//flag_szp8(REG_AL); //Basic flags!
	flag_p8(REG_AL); //Parity is affected!
	if (EMULATED_CPU<CPU_80286) //Before new CPU?
	{
		FLAGW_ZF((REG_AL==0)?1:0); //Zero is affected!
	}
	else
	{
		FLAGW_ZF((REG_AL==0)?1:0); //Zero is affected!
		FLAGW_SF(0); //Sign is cleared!
	}
	//z=s=o=p=?
	CPUPROT2
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		CPU[activeCPU].cycles_OP += 4; //Timings!
	}
	return 0;
}

OPTINLINE byte CPU8086_internal_CBW()
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
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		CPU[activeCPU].cycles_OP += 2; //Clock cycles!
	}
	CPUPROT2
	return 0;
}
OPTINLINE byte CPU8086_internal_CWD()
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
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		CPU[activeCPU].cycles_OP += 5; //Clock cycles!
	}
	CPUPROT2
	return 0;
}

//Now the repeatable instructions!

extern byte newREP; //Are we a new repeating instruction (REP issued for a new instruction, not repeating?)

byte MOVSB_data;
OPTINLINE byte CPU8086_internal_MOVSB()
{
	if (blockREP) return 1; //Disabled REP!
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),(CPU_Address_size[activeCPU]?REG_ESI:REG_SI),1,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (checkMMUaccess(CPU_SEGMENT_ES,REG_ES,(CPU_Address_size[activeCPU]?REG_EDI:REG_DI),0,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //First Execution step?
	{
		//Needs a read from memory?
		if (CPU8086_internal_stepreaddirectb(0,CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), (CPU_Address_size[activeCPU]?REG_ESI:REG_SI), &MOVSB_data,!CPU_Address_size[activeCPU])) return 1; //Try to read the data!
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==2) //Execution step?
	{
		if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
		{
			if (CPU[activeCPU].repeating) //Are we a repeating instruction?
			{
				if (newREP) //Include the REP?
				{
					CPU[activeCPU].cycles_OP += 9+17-(EU_CYCLES_SUBSTRACT_ACCESSRW); //Clock cycles including REP!
				}
				else //Repeating instruction itself?
				{
					CPU[activeCPU].cycles_OP += 17-(EU_CYCLES_SUBSTRACT_ACCESSRW); //Clock cycles excluding REP!
				}
			}
			else //Plain non-repeating instruction?
			{
				CPU[activeCPU].cycles_OP += 18-(EU_CYCLES_SUBSTRACT_ACCESSRW); //Clock cycles!
			}
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		CPU[activeCPU].executed = 0; return 1; //Wait for execution phase to finish!
	}
	//Writeback phase!
	if (CPU8086_internal_stepwritedirectb(2,CPU_SEGMENT_ES,REG_ES,(CPU_Address_size[activeCPU]?REG_EDI:REG_DI),MOVSB_data,!CPU_Address_size[activeCPU])) return 1;
	CPUPROT1
	if (FLAG_DF)
	{
		if (CPU_Address_size[activeCPU])
		{
			--REG_ESI;
			--REG_EDI;
		}
		else
		{
			--REG_SI;
			--REG_DI;
		}
	}
	else
	{
		if (CPU_Address_size[activeCPU])
		{
			++REG_ESI;
			++REG_EDI;
		}
		else
		{
			++REG_SI;
			++REG_DI;
		}
	}
	CPUPROT2
	return 0;
}

word MOVSW_data;
OPTINLINE byte CPU8086_internal_MOVSW()
{
	if (blockREP) return 1; //Disabled REP!
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),(CPU_Address_size[activeCPU]?REG_ESI:REG_SI),1,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),(CPU_Address_size[activeCPU]?REG_ESI:REG_SI)+1,1,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (checkMMUaccess(CPU_SEGMENT_ES,REG_ES,(CPU_Address_size[activeCPU]?REG_EDI:REG_DI),0,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (checkMMUaccess(CPU_SEGMENT_ES,REG_ES,(CPU_Address_size[activeCPU]?REG_EDI:REG_DI)+1,0,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //First Execution step?
	{
		//Needs a read from memory?
		if (CPU8086_internal_stepreaddirectw(0,CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), (CPU_Address_size[activeCPU]?REG_ESI:REG_SI), &MOVSW_data,!CPU_Address_size[activeCPU])) return 1; //Try to read the data!
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==2) //Execution step?
	{
		if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
		{
			if (CPU[activeCPU].repeating) //Are we a repeating instruction?
			{
				if (newREP) //Include the REP?
				{
					CPU[activeCPU].cycles_OP += 9 + 17 - (EU_CYCLES_SUBSTRACT_ACCESSRW); //Clock cycles including REP!
				}
				else //Repeating instruction itself?
				{
					CPU[activeCPU].cycles_OP += 17 - (EU_CYCLES_SUBSTRACT_ACCESSRW); //Clock cycles excluding REP!
				}
			}
			else //Plain non-repeating instruction?
			{
				CPU[activeCPU].cycles_OP += 18 - (EU_CYCLES_SUBSTRACT_ACCESSRW); //Clock cycles!
			}
		}
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		CPU[activeCPU].executed = 0; return 1; //Wait for execution phase to finish!
	}
	//Writeback phase!
	if (CPU8086_internal_stepwritedirectw(2,CPU_SEGMENT_ES,REG_ES,(CPU_Address_size[activeCPU]?REG_EDI:REG_DI),MOVSW_data,!CPU_Address_size[activeCPU])) return 1;
	CPUPROT1
	if (FLAG_DF)
	{
		if (CPU_Address_size[activeCPU])
		{
			REG_ESI -= 2;
			REG_EDI -= 2;
		}
		else
		{
			REG_SI -= 2;
			REG_DI -= 2;
		}
	}
	else
	{
		if (CPU_Address_size[activeCPU])
		{
			REG_ESI += 2;
			REG_EDI += 2;
		}
		else
		{
			REG_SI += 2;
			REG_DI += 2;
		}
	}
	CPUPROT2
	return 0;
}

byte CMPSB_data1,CMPSB_data2;
OPTINLINE byte CPU8086_internal_CMPSB()
{
	if (blockREP) return 1; //Disabled REP!
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS),(CPU_Address_size[activeCPU]?REG_ESI:REG_SI),1,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, (CPU_Address_size[activeCPU]?REG_EDI:REG_DI),1,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //First Execution step?
	{
		//Needs a read from memory?
		if (CPU8086_internal_stepreaddirectb(0,CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), (CPU_Address_size[activeCPU]?REG_ESI:REG_SI),&CMPSB_data1,!CPU_Address_size[activeCPU])) return 1; //Try to read the data!
		if (CPU8086_internal_stepreaddirectb(2,CPU_SEGMENT_ES, REG_ES, (CPU_Address_size[activeCPU]?REG_EDI:REG_DI), &CMPSB_data2,!CPU_Address_size[activeCPU])) return 1; //Try to read the data!
		
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	CMP_b(CMPSB_data1,CMPSB_data2,4);
	if (FLAG_DF)
	{
		if (CPU_Address_size[activeCPU])
		{
			--REG_ESI;
			--REG_EDI;
		}
		else
		{
			--REG_SI;
			--REG_DI;
		}
	}
	else
	{
		if (CPU_Address_size[activeCPU])
		{
			++REG_ESI;
			++REG_EDI;
		}
		else
		{
			++REG_SI;
			++REG_DI;
		}
	}
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		if (CPU[activeCPU].repeating) //Are we a repeating instruction?
		{
			if (newREP) //Include the REP?
			{
				CPU[activeCPU].cycles_OP += 9 + 22 - (EU_CYCLES_SUBSTRACT_ACCESSREAD*2); //Clock cycles including REP!
			}
			else //Repeating instruction itself?
			{
				CPU[activeCPU].cycles_OP += 22 - (EU_CYCLES_SUBSTRACT_ACCESSREAD*2); //Clock cycles excluding REP!
			}
		}
		else //Plain non-repeating instruction?
		{
			CPU[activeCPU].cycles_OP += 22 - (EU_CYCLES_SUBSTRACT_ACCESSREAD*2); //Clock cycles!
		}
	}
	++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	return 0;
}

word CMPSW_data1,CMPSW_data2;
OPTINLINE byte CPU8086_internal_CMPSW()
{
	if (blockREP) return 1; //Disabled REP!
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS),(CPU_Address_size[activeCPU]?REG_ESI:REG_SI),1,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS),(CPU_Address_size[activeCPU]?REG_ESI:REG_SI)+1,1,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, (CPU_Address_size[activeCPU]?REG_EDI:REG_DI),1,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, (CPU_Address_size[activeCPU]?REG_EDI:REG_DI)+1,1,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //First Execution step?
	{
		//Needs a read from memory?
		if (CPU8086_internal_stepreaddirectw(0,CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), (CPU_Address_size[activeCPU]?REG_ESI:REG_SI),&CMPSW_data1,!CPU_Address_size[activeCPU])) return 1; //Try to read the data!
		if (CPU8086_internal_stepreaddirectw(2,CPU_SEGMENT_ES, REG_ES, (CPU_Address_size[activeCPU]?REG_EDI:REG_DI), &CMPSW_data2,!CPU_Address_size[activeCPU])) return 1; //Try to read the data!
		
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	CMP_w(CMPSW_data1,CMPSW_data2,4);
	if (FLAG_DF)
	{
		if (CPU_Address_size[activeCPU])
		{
			REG_ESI -= 2;
			REG_EDI -= 2;
		}
		else
		{
			REG_SI -= 2;
			REG_DI -= 2;
		}
	}
	else
	{
		if (CPU_Address_size[activeCPU])
		{
			REG_ESI += 2;
			REG_EDI += 2;
		}
		else
		{
			REG_SI += 2;
			REG_DI += 2;
		}
	}

	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		if (CPU[activeCPU].repeating) //Are we a repeating instruction?
		{
			if (newREP) //Include the REP?
			{
				CPU[activeCPU].cycles_OP += 9 + 22 - (EU_CYCLES_SUBSTRACT_ACCESSREAD*2); //Clock cycles including REP!
			}
			else //Repeating instruction itself?
			{
				CPU[activeCPU].cycles_OP += 22 - (EU_CYCLES_SUBSTRACT_ACCESSREAD*2); //Clock cycles excluding REP!
			}
		}
		else //Plain non-repeating instruction?
		{
			CPU[activeCPU].cycles_OP += 22 - (EU_CYCLES_SUBSTRACT_ACCESSREAD*2); //Clock cycles!
		}
	}
	return 0;
}
OPTINLINE byte CPU8086_internal_STOSB()
{
	if (blockREP) return 1; //Disabled REP!
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, (CPU_Address_size[activeCPU]?REG_EDI:REG_DI),0,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //First Execution step?
	{
		//Needs a read from memory?
		if (CPU8086_internal_stepwritedirectb(0,CPU_segment_index(CPU_SEGMENT_ES),REG_ES,(CPU_Address_size[activeCPU]?REG_EDI:REG_DI),REG_AL,!CPU_Address_size[activeCPU])) return 1; //Try to read the data!
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}

	CPUPROT1
	if (FLAG_DF)
	{
		if (CPU_Address_size[activeCPU])
		{
			--REG_EDI;
		}
		else
		{
			--REG_DI;
		}
	}
	else
	{
		if (CPU_Address_size[activeCPU])
		{
			++REG_EDI;
		}
		else
		{
			++REG_DI;
		}
	}
	CPUPROT2
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		if (CPU[activeCPU].repeating) //Are we a repeating instruction?
		{
			if (newREP) //Include the REP?
			{
				CPU[activeCPU].cycles_OP += 9 + 10 - EU_CYCLES_SUBSTRACT_ACCESSWRITE; //Clock cycles including REP!
			}
			else //Repeating instruction itself?
			{
				CPU[activeCPU].cycles_OP += 10 - EU_CYCLES_SUBSTRACT_ACCESSWRITE; //Clock cycles excluding REP!
			}
		}
		else //Plain non-repeating instruction?
		{
			CPU[activeCPU].cycles_OP += 11 - EU_CYCLES_SUBSTRACT_ACCESSWRITE; //Clock cycles!
		}
	}
	return 0;
}
OPTINLINE byte CPU8086_internal_STOSW()
{
	if (blockREP) return 1; //Disabled REP!
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, (CPU_Address_size[activeCPU]?REG_EDI:REG_DI),0,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, (CPU_Address_size[activeCPU]?REG_EDI:REG_DI)+1,0,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //First Execution step?
	{
		//Needs a read from memory?
		if (CPU8086_internal_stepwritedirectw(0,CPU_segment_index(CPU_SEGMENT_ES),REG_ES,(CPU_Address_size[activeCPU]?REG_EDI:REG_DI),REG_AX,!CPU_Address_size[activeCPU])) return 1; //Try to read the data!
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	CPUPROT1
	if (FLAG_DF)
	{
		if (CPU_Address_size[activeCPU])
		{
			REG_EDI -= 2;
		}
		else
		{
			REG_DI -= 2;
		}
	}
	else
	{
		if (CPU_Address_size[activeCPU])
		{
			REG_EDI += 2;
		}
		else
		{
			REG_DI += 2;
		}
	}
	CPUPROT2
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		if (CPU[activeCPU].repeating) //Are we a repeating instruction?
		{
			if (newREP) //Include the REP?
			{
				CPU[activeCPU].cycles_OP += 9 + 10 - EU_CYCLES_SUBSTRACT_ACCESSWRITE; //Clock cycles including REP!
			}
			else //Repeating instruction itself?
			{
				CPU[activeCPU].cycles_OP += 10 - EU_CYCLES_SUBSTRACT_ACCESSWRITE; //Clock cycles excluding REP!
			}
		}
		else //Plain non-repeating instruction?
		{
			CPU[activeCPU].cycles_OP += 11 - EU_CYCLES_SUBSTRACT_ACCESSWRITE; //Clock cycles!
		}
	}
	++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	return 0;
}
//OK so far!
byte LODSB_value;
OPTINLINE byte CPU8086_internal_LODSB()
{
	if (blockREP) return 1; //Disabled REP!
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), (CPU_Address_size[activeCPU]?REG_ESI:REG_SI),1,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		++CPU[activeCPU].internalinstructionstep;
	}
	if (CPU[activeCPU].internalinstructionstep==1) //First Execution step?
	{
		//Needs a read from memory?
		if (CPU8086_internal_stepreaddirectb(0,CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), (CPU_Address_size[activeCPU]?REG_ESI:REG_SI), &LODSB_value,!CPU_Address_size[activeCPU])) return 1; //Try to read the data!
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	CPUPROT1
	REG_AL = LODSB_value;
	if (FLAG_DF)
	{
		if (CPU_Address_size[activeCPU])
		{
			--REG_ESI;
		}
		else
		{
			--REG_SI;
		}
	}
	else
	{
		if (CPU_Address_size[activeCPU])
		{
			++REG_ESI;
		}
		else
		{
			++REG_SI;
		}
	}
	CPUPROT2

	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		if (CPU[activeCPU].repeating) //Are we a repeating instruction?
		{
			if (newREP) //Include the REP?
			{
				CPU[activeCPU].cycles_OP += 9 + 13 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Clock cycles including REP!
			}
			else //Repeating instruction itself?
			{
				CPU[activeCPU].cycles_OP += 13 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Clock cycles excluding REP!
			}
		}
		else //Plain non-repeating instruction?
		{
			CPU[activeCPU].cycles_OP += 12 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Clock cycles!
		}
	}
	return 0;
}

word LODSW_value;
OPTINLINE byte CPU8086_internal_LODSW()
{
	if (blockREP) return 1; //Disabled REP!
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), (CPU_Address_size[activeCPU]?REG_ESI:REG_SI),1,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), (CPU_Address_size[activeCPU]?REG_ESI:REG_SI)+1,1,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		++CPU[activeCPU].internalinstructionstep;
	}
	if (CPU[activeCPU].internalinstructionstep==1) //First Execution step?
	{
		//Needs a read from memory?
		if (CPU8086_internal_stepreaddirectw(0,CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), (CPU_Address_size[activeCPU]?REG_ESI:REG_SI), &LODSW_value,!CPU_Address_size[activeCPU])) return 1; //Try to read the data!
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	CPUPROT1
	REG_AX = LODSW_value;
	if (FLAG_DF)
	{
		if (CPU_Address_size[activeCPU])
		{
			REG_ESI -= 2;
		}
		else
		{
			REG_SI -= 2;
		}
	}
	else
	{
		if (CPU_Address_size[activeCPU])
		{
			REG_ESI += 2;
		}
		else
		{
			REG_SI += 2;
		}
	}
	CPUPROT2
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		if (CPU[activeCPU].repeating) //Are we a repeating instruction?
		{
			if (newREP) //Include the REP?
			{
				CPU[activeCPU].cycles_OP += 9 + 13 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Clock cycles including REP!
			}
			else //Repeating instruction itself?
			{
				CPU[activeCPU].cycles_OP += 13 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Clock cycles excluding REP!
			}
		}
		else //Plain non-repeating instruction?
		{
			CPU[activeCPU].cycles_OP += 12 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Clock cycles!
		}
	}
	return 0;
}

byte SCASB_cmp1;
OPTINLINE byte CPU8086_internal_SCASB()
{
	if (blockREP) return 1; //Disabled REP!
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, (CPU_Address_size[activeCPU]?REG_EDI:REG_DI),1,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		++CPU[activeCPU].internalinstructionstep;
	}
	if (CPU[activeCPU].internalinstructionstep==1) //First Execution step?
	{
		//Needs a read from memory?
		if (CPU8086_internal_stepreaddirectb(0,CPU_segment_index(CPU_SEGMENT_ES), REG_ES, (CPU_Address_size[activeCPU]?REG_EDI:REG_DI), &SCASB_cmp1,!CPU_Address_size[activeCPU])) return 1; //Try to read the data!
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}

	//Old function
	CPUPROT1
	CMP_b(REG_AL,SCASB_cmp1,4);
	if (FLAG_DF)
	{
		if (CPU_Address_size[activeCPU])
		{
			--REG_EDI;
		}
		else
		{
			--REG_DI;
		}
	}
	else
	{
		if (CPU_Address_size[activeCPU])
		{
			++REG_EDI;
		}
		else
		{
			++REG_DI;
		}
	}
	CPUPROT2
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		if (CPU[activeCPU].repeating) //Are we a repeating instruction?
		{
			if (newREP) //Include the REP?
			{
				CPU[activeCPU].cycles_OP += 9 + 15 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Clock cycles including REP!
			}
			else //Repeating instruction itself?
			{
				CPU[activeCPU].cycles_OP += 15 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Clock cycles excluding REP!
			}
		}
		else //Plain non-repeating instruction?
		{
			CPU[activeCPU].cycles_OP += 15 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Clock cycles!
		}
	}
	return 0;
}

word SCASW_cmp1;
OPTINLINE byte CPU8086_internal_SCASW()
{
	if (blockREP) return 1; //Disabled REP!
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, (CPU_Address_size[activeCPU]?REG_EDI:REG_DI),1,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, (CPU_Address_size[activeCPU]?REG_EDI:REG_DI)+1,1,getCPL(),!CPU_Address_size[activeCPU])) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		++CPU[activeCPU].internalinstructionstep;
	}
	if (CPU[activeCPU].internalinstructionstep==1) //First Execution step?
	{
		//Needs a read from memory?
		if (CPU8086_internal_stepreaddirectw(0,CPU_segment_index(CPU_SEGMENT_ES), REG_ES, (CPU_Address_size[activeCPU]?REG_EDI:REG_DI), &SCASW_cmp1,!CPU_Address_size[activeCPU])) return 1; //Try to read the data!
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}

	CPUPROT1
	CMP_w(REG_AX,SCASW_cmp1,4);
	if (FLAG_DF)
	{
		if (CPU_Address_size[activeCPU])
		{
			REG_EDI -= 2;
		}
		else
		{
			REG_DI -= 2;
		}
	}
	else
	{
		if (CPU_Address_size[activeCPU])
		{
			REG_EDI += 2;
		}
		else
		{
			REG_DI += 2;
		}
	}
	CPUPROT2
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		if (CPU[activeCPU].repeating) //Are we a repeating instruction?
		{
			if (newREP) //Include the REP?
			{
				CPU[activeCPU].cycles_OP += 9 + 15 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Clock cycles including REP!
			}
			else //Repeating instruction itself?
			{
				CPU[activeCPU].cycles_OP += 15 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Clock cycles excluding REP!
			}
		}
		else //Plain non-repeating instruction?
		{
			CPU[activeCPU].cycles_OP += 15 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Clock cycles!
		}
	}
	return 0;
}

OPTINLINE byte CPU8086_instructionstepPOPtimeout(byte base)
{
	return CPU8086_instructionstepdelayBIU(base,2);//Delay 2 cycles for POPs to start!
}

OPTINLINE byte CPU8086_internal_POPtimeout(byte base)
{
	return CPU8086_internal_delayBIU(base,2);//Delay 2 cycles for POPs to start!
}

word RET_val;
OPTINLINE byte CPU8086_internal_RET(word popbytes, byte isimm)
{
	if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,0,0)) return 1; ++CPU[activeCPU].stackchecked; }
	if (CPU8086_internal_POPtimeout(0)) return 1; //POP timeout!
	if (CPU8086_internal_POPw(2,&RET_val)) return 1;
    //Near return
	CPUPROT1
	CPU_JMPabs(RET_val);
	CPU_flushPIQ(-1); //We're jumping to another address!
	REG_SP += popbytes;
	CPUPROT2
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		if (isimm)
			CPU[activeCPU].cycles_OP += 12 - EU_CYCLES_SUBSTRACT_ACCESSREAD; /* Intrasegment with constant */
		else
			CPU[activeCPU].cycles_OP += 8 - EU_CYCLES_SUBSTRACT_ACCESSREAD; /* Intrasegment */
		CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; //Stall the BIU completely now!
	}
	return 0;
}

word RETF_destCS;
word RETF_val; //Far return

OPTINLINE byte CPU8086_internal_RETF(word popbytes, byte isimm)
{
	if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(2,0,0)) return 1; ++CPU[activeCPU].stackchecked; }
	if (CPU8086_internal_POPtimeout(0)) return 1; //POP timeout!
	if (CPU8086_internal_POPw(2,&RETF_val)) return 1;
	CPUPROT1
	if (CPU8086_internal_POPw(4,&RETF_destCS)) return 1;
	CPUPROT1
	destEIP = RETF_val; //Load IP!
	segmentWritten(CPU_SEGMENT_CS,RETF_destCS,4); //CS changed, we're a RETF instruction!
	CPU_flushPIQ(-1); //We're jumping to another address!
	CPUPROT1
	REG_SP += popbytes; //Process SP!
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		if (isimm)
			CPU[activeCPU].cycles_OP += 17 - (EU_CYCLES_SUBSTRACT_ACCESSREAD*2); /* Intersegment with constant */
		else
			CPU[activeCPU].cycles_OP += 18 - (EU_CYCLES_SUBSTRACT_ACCESSREAD*2); /* Intersegment */
		CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; //Stall the BIU completely now!
	}
	CPUPROT2
	CPUPROT2
	CPUPROT2
	return 0;
}

void external8086RETF(word popbytes)
{
	CPU8086_internal_RETF(popbytes,1); //Return immediate variant!
}

OPTINLINE byte CPU8086_internal_INTO()
{
	CPUPROT1
	if (FLAG_OF)
	{
		if (CPU_faultraised(EXCEPTION_OVERFLOW))
		{
			CPU8086_int(EXCEPTION_OVERFLOW,0);
		}
	}
	else
	{
		if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
		{
			CPU[activeCPU].cycles_OP += 4; //Timings!
		}
	}
	CPUPROT2
	return 0;
}

OPTINLINE byte CPU8086_internal_AAM(byte data)
{
	CPUPROT1
	if ((!data) && (CPU[activeCPU].instructionstep==0)) //First step?
	{
		CPU[activeCPU].cycles_OP += 1; //Timings always!
		++CPU[activeCPU].instructionstep; //Next step after we're done!
		CPU[activeCPU].executed = 0; //Not executed yet!
		return 1;
	}
	word quotient, remainder;
	byte error, applycycles;
	CPU8086_internal_DIV(REG_AL,data,&quotient,&remainder,&error,8,2,6,&applycycles);
	if (error) //Error occurred?
	{
		CPU_exDIV0(); //Raise error that's requested!
		return 1;
	}
	else //Valid result?
	{
		REG_AH = (byte)(quotient&0xFF);
		REG_AL = (byte)(remainder&0xFF);
		//Flags are set on newer CPUs according to the MOD operation: Sign, Zero and Parity are set according to the mod operation(AL) and Overflow, Carry and Auxiliary carry are cleared.
		flag_szp8(REG_AL); //Result of MOD instead!
		FLAGW_OF(0); FLAGW_CF(0); FLAGW_AF(0); //Clear these!
		//C=O=A=?
	}
	CPUPROT2
	return 0;
}
OPTINLINE byte CPU8086_internal_AAD(byte data)
{
	CPUPROT1
	oper2 = (word)REG_AL; //What to add!
	REG_AX = (REG_AH*data);    //AAD
	oper1 = REG_AX; //Load for addition!
	op_add16(); //Add, 16-bit, including flags!
	REG_AX = res16; //The result to load!
	REG_AH = 0; //AH is cleared!
	//C=O=A=?
	CPUPROT2
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		CPU[activeCPU].cycles_OP += 60; //Timings!
	}
	return 0;
}

byte XLAT_value;    //XLAT

OPTINLINE byte CPU8086_internal_XLAT()
{
	if (cpudebugger) //Debugger on?
	{
		debugger_setcommand("XLAT");    //XLAT
	}
	if (CPU[activeCPU].internalinstructionstep==0) //First step?
	{
		if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),REG_BX+REG_AL,0,getCPL(),!CPU_Address_size[activeCPU])) return 1; //Abort on fault!
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==1) //First Execution step?
	{
		//Needs a read from memory?
		if (CPU8086_internal_stepreaddirectb(0,CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),REG_BX+REG_AL,&XLAT_value,!CPU_Address_size[activeCPU])) return 1; //Try to read the data!
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	CPUPROT1
	REG_AL = XLAT_value;
	CPUPROT2
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		CPU[activeCPU].cycles_OP += 11 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //XLAT timing!
	}
	return 0;
}

void CPU8086_external_XLAT() {CPU8086_internal_XLAT();} //External variant!

byte secondparambase=0, writebackbase=0;
OPTINLINE byte CPU8086_internal_XCHG8(byte *data1, byte *data2, byte flags)
{
	if (CPU[activeCPU].internalinstructionstep==0)
	{
		if (!data1) if (modrm_check8(&params,MODRM_src0,1)) return 1; //Abort on fault!
		if (!data1) if (modrm_check8(&params,MODRM_src0,0)) return 1; //Abort on fault!
		secondparambase = (data1||data2)?0:2; //Second param base
		writebackbase = ((data2==NULL) && (data1==NULL))?4:2; //Write back param base
		if (!data2) if (modrm_check8(&params,MODRM_src1,1)) return 1; //Abort on fault!
		if (!data2) if (modrm_check8(&params,MODRM_src1,0)) return 1; //Abort on fault!
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==1) //First step?
	{
		if (data1==NULL) if (CPU8086_internal_stepreadmodrmb(0,&oper1b,MODRM_src0)) return 1;
		if (data2==NULL) if (CPU8086_internal_stepreadmodrmb(secondparambase,&oper2b,MODRM_src1)) return 1;
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==2) //Execution step?
	{
		oper1b = data1?*data1:oper1b;
		oper2b = data2?*data2:oper2b;
		INLINEREGISTER byte temp = oper1b; //Copy!
		oper1b = oper2b; //We're ...
		oper2b = temp; //Swapping this!
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
		{
			switch (flags)
			{
			case 0: //Unknown?
				break;
			case 1: //Acc<->Reg?
				CPU[activeCPU].cycles_OP += 3; //Acc<->Reg!
				break;
			case 2: //Mem<->Reg?
				if (MODRM_EA(params)) //Reg<->Mem?
				{
					CPU[activeCPU].cycles_OP += 17 - (EU_CYCLES_SUBSTRACT_ACCESSRW*2); //SegReg->Mem!
				}
				else //Reg<->Reg?
				{
					CPU[activeCPU].cycles_OP += 4; //SegReg->Mem!
				}
				break;
			}
		}
		if ((data1==NULL) || (data2==NULL)) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}
	if (data1) //Register?
	{
		*data1 = oper1b;
	}
	else //Memory?
	{
		if (CPU8086_internal_stepwritemodrmb(writebackbase,oper1b,MODRM_src0)) return 1;
	}
	
	if (data2)
	{
		*data2 = oper2b;
	}
	else
	{
		if (CPU8086_internal_stepwritemodrmb(writebackbase+secondparambase,oper2b,MODRM_src1)) return 1;
	}
	CPUPROT2
	return 0;
}

OPTINLINE byte CPU8086_internal_XCHG16(word *data1, word *data2, byte flags)
{
	if (CPU[activeCPU].internalinstructionstep==0)
	{
		if (!data1) if (modrm_check16(&params,MODRM_src0,1)) return 1; //Abort on fault!
		if (!data1) if (modrm_check16(&params,MODRM_src0,0)) return 1; //Abort on fault!
		secondparambase = (data1||data2)?0:2; //Second param base
		writebackbase = ((data2==NULL) && (data1==NULL))?4:2; //Write back param base
		if (!data2) if (modrm_check16(&params,MODRM_src1,1)) return 1; //Abort on fault!
		if (!data2) if (modrm_check16(&params,MODRM_src1,0)) return 1; //Abort on fault!
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==1) //First step?
	{
		if (data1==NULL) if (CPU8086_internal_stepreadmodrmw(0,&oper1,MODRM_src0)) return 1;
		if (data2==NULL) if (CPU8086_internal_stepreadmodrmw(secondparambase,&oper2,MODRM_src1)) return 1;
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	if (CPU[activeCPU].internalinstructionstep==2) //Execution step?
	{
		oper1 = data1?*data1:oper1;
		oper2 = data2?*data2:oper2;
		INLINEREGISTER word temp = oper1; //Copy!
		oper1 = oper2; //We're ...
		oper2 = temp; //Swapping this!
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
		if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
		{
			switch (flags)
			{
			case 0: //Unknown?
				break;
			case 1: //Acc<->Reg?
				CPU[activeCPU].cycles_OP += 3; //Acc<->Reg!
				break;
			case 2: //Mem<->Reg?
				if (MODRM_EA(params)) //Reg<->Mem?
				{
					CPU[activeCPU].cycles_OP += 17 - (EU_CYCLES_SUBSTRACT_ACCESSRW*2); //SegReg->Mem!
				}
				else //Reg<->Reg?
				{
					CPU[activeCPU].cycles_OP += 4; //SegReg->Mem!
				}
				break;
			}
		}
		if ((data1==NULL) || (data2==NULL)) { CPU[activeCPU].executed = 0; return 1; } //Wait for execution phase to finish!
	}

	if (data1) //Register?
	{
		*data1 = oper1;
	}
	else //Memory?
	{
		if (CPU8086_internal_stepwritemodrmw(writebackbase,oper1,MODRM_src0,0)) return 1;
	}
	
	if (data2)
	{
		*data2 = oper2;
	}
	else
	{
		if (CPU8086_internal_stepwritemodrmw(writebackbase+secondparambase,oper2,MODRM_src1,0)) return 1;
	}
	CPUPROT2
	return 0;
}

extern byte modrm_addoffset; //Add this offset to ModR/M reads!

byte CPU8086_internal_LXS(int segmentregister) //LDS, LES etc.
{
	static word segment, offset;

	if (CPU[activeCPU].internalinstructionstep==0)
	{
		modrm_addoffset = 0; //Add this to the offset to use!
		if (modrm_check16(&params,1,1)) return 1; //Abort on fault!
		modrm_addoffset = 2; //Add this to the offset to use!
		if (modrm_check16(&params,1,1)) return 1; //Abort on fault!
		if (modrm_check16(&params,0,0)) return 1; //Abort on fault for the used segment itself!
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	CPUPROT1
	if (CPU[activeCPU].internalinstructionstep==1) //First step?
	{
		modrm_addoffset = 0; //Add this to the offset to use!
		if (CPU8086_internal_stepreadmodrmw(0,&offset,1)) return 1;
		modrm_addoffset = 2; //Add this to the offset to use!
		if (CPU8086_internal_stepreadmodrmw(2,&segment,1)) return 1;
		modrm_addoffset = 0; //Reset again!
		++CPU[activeCPU].internalinstructionstep; //Next internal instruction step!
	}
	//Execution phase!
	CPUPROT1
	destEIP = REG_EIP; //Save EIP for transfers!
	segmentWritten(segmentregister, segment,0); //Load the new segment!
	CPUPROT1
	modrm_write16(&params, 0, offset, 0); //Try to load the new register with the offset!
	CPUPROT2
	CPUPROT2
	CPUPROT2
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP += 16 - (EU_CYCLES_SUBSTRACT_ACCESSREAD*2); /* LXS based on MOV Mem->SS, DS, ES */
		}
		else //Register? Should be illegal?
		{
			CPU[activeCPU].cycles_OP += 2; /* LXS based on MOV Mem->SS, DS, ES */
		}
	}
	return 0;
}

byte CPU8086_CALLF(word segment, word offset)
{
	destEIP = offset;
	segmentWritten(CPU_SEGMENT_CS, segment, 2); /*CS changed, call version!*/
	CPU_flushPIQ(-1); //We're jumping to another address!
	return 0;
}

/*

NOW THE REAL OPCODES!

*/

extern byte didJump; //Did we jump this instruction?

byte instructionbufferb=0, instructionbufferb2=0; //For 8-bit read storage!
word instructionbufferw=0, instructionbufferw2=0; //For 16-bit read storage!

void CPU8086_OP00() {modrm_generateInstructionTEXT("ADDB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,0)) return; CPU8086_internal_ADD8(modrm_addr8(&params,1,0),instructionbufferb,2); }
void CPU8086_OP01() {modrm_generateInstructionTEXT("ADDW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,0)) return; CPU8086_internal_ADD16(modrm_addr16(&params,1,0),instructionbufferw,2); }
void CPU8086_OP02() {modrm_generateInstructionTEXT("ADDB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,1)) return; CPU8086_internal_ADD8(modrm_addr8(&params,0,0),instructionbufferb,2); }
void CPU8086_OP03() {modrm_generateInstructionTEXT("ADDW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return; CPU8086_internal_ADD16(modrm_addr16(&params,0,0),instructionbufferw,2); }
void CPU8086_OP04() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("ADDB AL,",0,theimm,PARAM_IMM8); CPU8086_internal_ADD8(&REG_AL,theimm,1); }
void CPU8086_OP05() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("ADDW AX,",0,theimm,PARAM_IMM16); CPU8086_internal_ADD16(&REG_AX,theimm,1); }
void CPU8086_OP06() {modrm_generateInstructionTEXT("PUSH ES",0,0,PARAM_NONE); if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_PUSHw(0,&REG_ES)) return; /*PUSH ES*/	if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSWRITE; } /*Push Segreg!*/}
void CPU8086_OP07() {modrm_generateInstructionTEXT("POP ES",0,0,PARAM_NONE); if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,0,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_instructionstepPOPtimeout(0)) return; /*POP timeout*/ if (CPU8086_POPw(2,&instructionbufferw)) return; segmentWritten(CPU_SEGMENT_ES,instructionbufferw,0); /*CS changed!*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{  CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; } /*Pop Segreg!*/}
void CPU8086_OP08() {modrm_generateInstructionTEXT("ORB",8,0,PARAM_MODRM21);  if (modrm_check8(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,0)) return; CPU8086_internal_OR8(modrm_addr8(&params,1,0),instructionbufferb,2); }
void CPU8086_OP09() {modrm_generateInstructionTEXT("ORW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,0)) return; CPU8086_internal_OR16(modrm_addr16(&params,1,0),instructionbufferw,2); }
void CPU8086_OP0A() {modrm_generateInstructionTEXT("ORB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,1)) return; CPU8086_internal_OR8(modrm_addr8(&params,0,0),instructionbufferb,2); }
void CPU8086_OP0B() {modrm_generateInstructionTEXT("ORW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return; CPU8086_internal_OR16(modrm_addr16(&params,0,0),instructionbufferw,2); }
void CPU8086_OP0C() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("ORB AL,",0,theimm,PARAM_IMM8); CPU8086_internal_OR8(&REG_AL,theimm,1); }
void CPU8086_OP0D() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("ORW AX,",0,theimm,PARAM_IMM16); CPU8086_internal_OR16(&REG_AX,theimm,1); }
void CPU8086_OP0E() {modrm_generateInstructionTEXT("PUSH CS",0,0,PARAM_NONE); if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_PUSHw(0,&REG_CS)) return; /*PUSH CS*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{  CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSWRITE; /*Push Segreg!*/ } }
void CPU8086_OP0F() /*FLAG_OF: POP CS; shouldn't be used?*/ { modrm_generateInstructionTEXT("POP CS", 0, 0, PARAM_NONE); if (CPU8086_instructionstepPOPtimeout(0)) return; /*POP timeout*/ if (CPU8086_POPw(2,&instructionbufferw)) return; /*Don't handle: 8086 ignores this opcode, and you won't find it there!*/ destEIP = REG_EIP; segmentWritten(CPU_SEGMENT_CS, instructionbufferw, 0); /*POP CS!*/ CPU_flushPIQ(-1); if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{  CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; /*Pop Segreg!*/ } }
void CPU8086_OP10() {modrm_generateInstructionTEXT("ADCB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,0)) return; CPU8086_internal_ADC8(modrm_addr8(&params,1,0),instructionbufferb,2); }
void CPU8086_OP11() {modrm_generateInstructionTEXT("ADCW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,0)) return; CPU8086_internal_ADC16(modrm_addr16(&params,1,0),instructionbufferw,2); }
void CPU8086_OP12() {modrm_generateInstructionTEXT("ADCB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,1)) return; CPU8086_internal_ADC8(modrm_addr8(&params,0,0),instructionbufferb,2); }
void CPU8086_OP13() {modrm_generateInstructionTEXT("ADCW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return; CPU8086_internal_ADC16(modrm_addr16(&params,0,0),instructionbufferw,2); }
void CPU8086_OP14() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("ADC AL,",0,theimm,PARAM_IMM8); CPU8086_internal_ADC8(&REG_AL,theimm,1); }
void CPU8086_OP15() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("ADC AX,",0,theimm,PARAM_IMM16); CPU8086_internal_ADC16(&REG_AX,theimm,1); }
void CPU8086_OP16() {modrm_generateInstructionTEXT("PUSH SS",0,0,PARAM_NONE);/*PUSH SS*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_PUSHw(0,&REG_SS)) return; /*PUSH SS*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{  CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSWRITE; /*Push Segreg!*/ } }
void CPU8086_OP17() {modrm_generateInstructionTEXT("POP SS",0,0,PARAM_NONE);/*POP SS*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,0,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_instructionstepPOPtimeout(0)) return; /*POP timeout*/ if (CPU8086_POPw(2,&instructionbufferw)) return; segmentWritten(CPU_SEGMENT_SS,instructionbufferw,0); /*CS changed!*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{  CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; /*Pop Segreg!*/ }  CPU[activeCPU].allowInterrupts = 0; /* Inhabit all interrupts up to the next instruction */ }
void CPU8086_OP18() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("SBBB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,0)) return; CPU8086_internal_SBB8(modrm_addr8(&params,1,0),instructionbufferb,2); }
void CPU8086_OP19() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("SBBW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,0)) return; CPU8086_internal_SBB16(modrm_addr16(&params,1,0),instructionbufferw,2); }
void CPU8086_OP1A() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("SBBB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,1)) return; CPU8086_internal_SBB8(modrm_addr8(&params,0,0),instructionbufferb,2); }
void CPU8086_OP1B() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("SBBW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return; CPU8086_internal_SBB16(modrm_addr16(&params,0,0),instructionbufferw,2); }
void CPU8086_OP1C() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("SBB AL,",0,theimm,PARAM_IMM8); CPU8086_internal_SBB8(&REG_AL,theimm,1); }
void CPU8086_OP1D() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("SBB AX,",0,theimm,PARAM_IMM16); CPU8086_internal_SBB16(&REG_AX,theimm,1); }
void CPU8086_OP1E() {modrm_generateInstructionTEXT("PUSH DS",0,0,PARAM_NONE);/*PUSH DS*/  if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_PUSHw(0,&REG_DS)) return; /*PUSH DS*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{  CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSWRITE; } /*Push Segreg!*/}
void CPU8086_OP1F() {modrm_generateInstructionTEXT("POP DS",0,0,PARAM_NONE);/*POP DS*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,0,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_instructionstepPOPtimeout(0)) return; /*POP timeout*/ if (CPU8086_POPw(2,&instructionbufferw)) return; segmentWritten(CPU_SEGMENT_DS,instructionbufferw,0); /*CS changed!*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{  CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; /*Pop Segreg!*/ } }
void CPU8086_OP20() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("ANDB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,0)) return; CPU8086_internal_AND8(modrm_addr8(&params,1,0),instructionbufferb,2); }
void CPU8086_OP21() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("ANDW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,0)) return; CPU8086_internal_AND16(modrm_addr16(&params,1,0),instructionbufferw,2); }
void CPU8086_OP22() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("ANDB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,1)) return; CPU8086_internal_AND8(modrm_addr8(&params,0,0),instructionbufferb,2); }
void CPU8086_OP23() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("ANDW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return; CPU8086_internal_AND16(modrm_addr16(&params,0,0),instructionbufferw,2); }
void CPU8086_OP24() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("AND AL,",0,theimm,PARAM_IMM8); CPU8086_internal_AND8(&REG_AL,theimm,1); }
void CPU8086_OP25() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("AND AX,",0,theimm,PARAM_IMM16); CPU8086_internal_AND16(&REG_AX,theimm,1); }
void CPU8086_OP27() {modrm_generateInstructionTEXT("DAA",0,0,PARAM_NONE);/*DAA?*/ CPU8086_internal_DAA();/*DAA?*/ }
void CPU8086_OP28() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("SUBB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,0)) return; CPU8086_internal_SUB8(modrm_addr8(&params,1,0),instructionbufferb,2); }
void CPU8086_OP29() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("SUBW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,0)) return; CPU8086_internal_SUB16(modrm_addr16(&params,1,0),instructionbufferw,2); }
void CPU8086_OP2A() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("SUBB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,1)) return; CPU8086_internal_SUB8(modrm_addr8(&params,0,0),instructionbufferb,2); }
void CPU8086_OP2B() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("SUBW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return; CPU8086_internal_SUB16(modrm_addr16(&params,0,0),instructionbufferw,2); }
void CPU8086_OP2C() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("SUB AL,",0,theimm,PARAM_IMM8);/*4=AL,imm8*/ CPU8086_internal_SUB8(&REG_AL,theimm,1);/*4=AL,imm8*/ }
void CPU8086_OP2D() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("SUB AX,",0,theimm,PARAM_IMM16);/*5=AX,imm16*/ CPU8086_internal_SUB16(&REG_AX,theimm,1);/*5=AX,imm16*/ }
void CPU8086_OP2F() {modrm_generateInstructionTEXT("DAS",0,0,PARAM_NONE);/*DAS?*/ CPU8086_internal_DAS();/*DAS?*/ }
void CPU8086_OP30() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("XORB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,0)) return; CPU8086_internal_XOR8(modrm_addr8(&params,1,0),instructionbufferb,2); }
void CPU8086_OP31() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("XORW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,0)) return; CPU8086_internal_XOR16(modrm_addr16(&params,1,0),instructionbufferw,2); }
void CPU8086_OP32() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("XORB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,1)) return; CPU8086_internal_XOR8(modrm_addr8(&params,0,0),instructionbufferb,2); }
void CPU8086_OP33() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("XORW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return; CPU8086_internal_XOR16(modrm_addr16(&params,0,0),instructionbufferw,2); }
void CPU8086_OP34() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("XOR AL,",0,theimm,PARAM_IMM8); CPU8086_internal_XOR8(&REG_AL,theimm,1); }
void CPU8086_OP35() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("XOR AX,",0,theimm,PARAM_IMM16); CPU8086_internal_XOR16(&REG_AX,theimm,1); }
void CPU8086_OP37() {modrm_generateInstructionTEXT("AAA",0,0,PARAM_NONE);/*AAA?*/ CPU8086_internal_AAA();/*AAA?*/ }
void CPU8086_OP38() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("CMPB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return; if (modrm_check8(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,1)) return; if (CPU8086_instructionstepreadmodrmb(2,&instructionbufferb2,0)) return; CMP_b(instructionbufferb,instructionbufferb2,2); }
void CPU8086_OP39() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("CMPW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; if (modrm_check16(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return; if (CPU8086_instructionstepreadmodrmw(2,&instructionbufferw2,0)) return; CMP_w(instructionbufferw,instructionbufferw2,2); }
void CPU8086_OP3A() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("CMPB",8,0,PARAM_MODRM12); if (modrm_check8(&params,0,1)) return; if (modrm_check8(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,0)) return; if (CPU8086_instructionstepreadmodrmb(2,&instructionbufferb2,1)) return; CMP_b(instructionbufferb,instructionbufferb2,2); }
void CPU8086_OP3B() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("CMPW",16,0,PARAM_MODRM12); if (modrm_check16(&params,0,1)) return; if (modrm_check16(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,0)) return; if (CPU8086_instructionstepreadmodrmw(2,&instructionbufferw2,1)) return; CMP_w(instructionbufferw,instructionbufferw2,2); }
void CPU8086_OP3C() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("CMP AL,",0,theimm,PARAM_IMM8);/*CMP AL, imm8*/ CMP_b(REG_AL,theimm,1);/*CMP AL, imm8*/ }
void CPU8086_OP3D() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("CMP AX,",0,theimm,PARAM_IMM16);/*CMP AX, imm16*/ CMP_w(REG_AX,theimm,1);/*CMP AX, imm16*/ }
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
void CPU8086_OP50() {modrm_generateInstructionTEXT("PUSH AX",0,0,PARAM_NONE);/*PUSH AX*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_PUSHw(0,&REG_AX)) return; /*PUSH AX*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 11-EU_CYCLES_SUBSTRACT_ACCESSWRITE; /*Push Reg!*/ } }
void CPU8086_OP51() {modrm_generateInstructionTEXT("PUSH CX",0,0,PARAM_NONE);/*PUSH CX*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_PUSHw(0,&REG_CX)) return; /*PUSH CX*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 11-EU_CYCLES_SUBSTRACT_ACCESSWRITE; /*Push Reg!*/ } }
void CPU8086_OP52() {modrm_generateInstructionTEXT("PUSH DX",0,0,PARAM_NONE);/*PUSH DX*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_PUSHw(0,&REG_DX)) return; /*PUSH DX*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 11-EU_CYCLES_SUBSTRACT_ACCESSWRITE; /*Push Reg!*/ } }
void CPU8086_OP53() {modrm_generateInstructionTEXT("PUSH BX",0,0,PARAM_NONE);/*PUSH BX*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_PUSHw(0,&REG_BX)) return; /*PUSH BX*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 11-EU_CYCLES_SUBSTRACT_ACCESSWRITE; /*Push Reg!*/ } }
void CPU8086_OP54() {modrm_generateInstructionTEXT("PUSH SP",0,0,PARAM_NONE);/*PUSH SP*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_PUSHw(0,&REG_SP)) return; /*PUSH SP*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 11-EU_CYCLES_SUBSTRACT_ACCESSWRITE; /*Push Reg!*/ } }
void CPU8086_OP55() {modrm_generateInstructionTEXT("PUSH BP",0,0,PARAM_NONE);/*PUSH BP*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_PUSHw(0,&REG_BP)) return; /*PUSH BP*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 11-EU_CYCLES_SUBSTRACT_ACCESSWRITE; /*Push Reg!*/ } }
void CPU8086_OP56() {modrm_generateInstructionTEXT("PUSH SI",0,0,PARAM_NONE);/*PUSH SI*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_PUSHw(0,&REG_SI)) return; /*PUSH SI*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 11-EU_CYCLES_SUBSTRACT_ACCESSWRITE; /*Push Reg!*/ } }
void CPU8086_OP57() {modrm_generateInstructionTEXT("PUSH DI",0,0,PARAM_NONE);/*PUSH DI*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_PUSHw(0,&REG_DI)) return; /*PUSH DI*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 11-EU_CYCLES_SUBSTRACT_ACCESSWRITE; /*Push Reg!*/ } }
void CPU8086_OP58() {modrm_generateInstructionTEXT("POP AX",0,0,PARAM_NONE);/*POP AX*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,0,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_instructionstepPOPtimeout(0)) return; /*POP timeout*/ if (CPU8086_POPw(2,&REG_AX)) return; /*POP AX*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; /*Pop Reg!*/ } }
void CPU8086_OP59() {modrm_generateInstructionTEXT("POP CX",0,0,PARAM_NONE);/*POP CX*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,0,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_instructionstepPOPtimeout(0)) return; /*POP timeout*/ if (CPU8086_POPw(2,&REG_CX)) return; /*POP CX*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; /*Pop Reg!*/ } }
void CPU8086_OP5A() {modrm_generateInstructionTEXT("POP DX",0,0,PARAM_NONE);/*POP DX*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,0,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_instructionstepPOPtimeout(0)) return; /*POP timeout*/ if (CPU8086_POPw(2,&REG_DX)) return; /*POP DX*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; /*Pop Reg!*/ } }
void CPU8086_OP5B() {modrm_generateInstructionTEXT("POP BX",0,0,PARAM_NONE);/*POP BX*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,0,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_instructionstepPOPtimeout(0)) return; /*POP timeout*/ if (CPU8086_POPw(2,&REG_BX)) return; /*POP BX*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; /*Pop Reg!*/ } }
void CPU8086_OP5C() {modrm_generateInstructionTEXT("POP SP",0,0,PARAM_NONE);/*POP SP*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,0,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_instructionstepPOPtimeout(0)) return; /*POP timeout*/ if (CPU8086_POPSP(2)) return; /*POP SP*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; /*Pop Reg!*/ } }
void CPU8086_OP5D() {modrm_generateInstructionTEXT("POP BP",0,0,PARAM_NONE);/*POP BP*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,0,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_instructionstepPOPtimeout(0)) return; /*POP timeout*/ if (CPU8086_POPw(2,&REG_BP)) return; /*POP BP*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; /*Pop Reg!*/ } }
void CPU8086_OP5E() {modrm_generateInstructionTEXT("POP SI",0,0,PARAM_NONE);/*POP SI*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,0,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_instructionstepPOPtimeout(0)) return; /*POP timeout*/ if (CPU8086_POPw(2,&REG_SI)) return;/*POP SI*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; /*Pop Reg!*/ } }
void CPU8086_OP5F() {modrm_generateInstructionTEXT("POP DI",0,0,PARAM_NONE);/*POP DI*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,0,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_instructionstepPOPtimeout(0)) return; /*POP timeout*/ if (CPU8086_POPw(2,&REG_DI)) return;/*POP DI*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; /*Pop Reg!*/ } }
void CPU8086_OP70() {INLINEREGISTER sbyte rel8;/*JO rel8: (OF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JO",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_OF) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/ didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP71() {INLINEREGISTER sbyte rel8;/*JNO rel8 : (OF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNO",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_OF) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/ didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP72() {INLINEREGISTER sbyte rel8;/*JC rel8: (CF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JC",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_CF) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/ didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP73() {INLINEREGISTER sbyte rel8;/*JNC rel8 : (CF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNC",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_CF) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/ didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP74() {INLINEREGISTER sbyte rel8;/*JZ rel8: (ZF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JZ",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_ZF) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/  didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP75() {INLINEREGISTER sbyte rel8;/*JNZ rel8 : (ZF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNZ",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_ZF) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/  didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP76() {INLINEREGISTER sbyte rel8;/*JNA rel8 : (CF=1|ZF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNA",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_CF||FLAG_ZF) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/  didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP77() {INLINEREGISTER sbyte rel8;/*JA rel8: (CF=0&ZF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JA",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_CF && !FLAG_ZF) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/  didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP78() {INLINEREGISTER sbyte rel8;/*JS rel8: (SF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JS",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_SF) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/  didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP79() {INLINEREGISTER sbyte rel8;/*JNS rel8 : (SF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNS",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_SF) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/ didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP7A() {INLINEREGISTER sbyte rel8;/*JP rel8 : (PF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JP",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_PF) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/  didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP7B() {INLINEREGISTER sbyte rel8;/*JNP rel8 : (PF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNP",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_PF) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/  didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP7C() {INLINEREGISTER sbyte rel8;/*JL rel8: (SF!=OF)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JL",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_SF!=FLAG_OF) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/  didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP7D() {INLINEREGISTER sbyte rel8;/*JGE rel8 : (SF=OF)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JGE",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_SF==FLAG_OF) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/  didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP7E() {INLINEREGISTER sbyte rel8;/*JLE rel8 : (ZF|(SF!=OF))*/ rel8 = imm8(); modrm_generateInstructionTEXT("JLE",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if ((FLAG_SF!=FLAG_OF) || FLAG_ZF) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/  didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP7F() {INLINEREGISTER sbyte rel8;/*JG rel8: ((ZF=0)&&(SF=OF))*/ rel8 = imm8(); modrm_generateInstructionTEXT("JG",0,REG_EIP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_ZF && (FLAG_SF==FLAG_OF)) {CPU_JMPrel(rel8); /* JUMP to destination? */ CPU_flushPIQ(-1); /*We're jumping to another address*/  didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 16; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 4; /* Branch not taken */ } } }
void CPU8086_OP84() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("TESTB",8,0,PARAM_MODRM12); if (modrm_check8(&params,0,1)) return; if (modrm_check8(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,0)) return; if (CPU8086_instructionstepreadmodrmb(2,&instructionbufferb2,1)) return; CPU8086_internal_TEST8(instructionbufferb,instructionbufferb2,2); }
void CPU8086_OP85() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("TESTW",16,0,PARAM_MODRM12); if (modrm_check16(&params,0,1)) return; if (modrm_check16(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,0)) return; if (CPU8086_instructionstepreadmodrmw(2,&instructionbufferw2,1)) return; CPU8086_internal_TEST16(instructionbufferw,instructionbufferw2,2); }
void CPU8086_OP86() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("XCHGB",8,0,PARAM_MODRM12); CPU8086_internal_XCHG8(modrm_addr8(&params,0,0),modrm_addr8(&params,1,1),2); /*XCHG reg8,r/m8*/ }
void CPU8086_OP87() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("XCHGW",16,0,PARAM_MODRM12); CPU8086_internal_XCHG16(modrm_addr16(&params,0,0),modrm_addr16(&params,1,0),2); /*XCHG reg16,r/m16*/ }
void CPU8086_OP88() {modrm_debugger8(&params,1,0); modrm_generateInstructionTEXT("MOVB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,0)) return; CPU8086_internal_MOV8(modrm_addr8(&params,1,0),instructionbufferb,2); }
void CPU8086_OP89() {modrm_debugger16(&params,1,0); modrm_generateInstructionTEXT("MOVW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,0)) return; CPU8086_internal_MOV16(modrm_addr16(&params,1,0),instructionbufferw,2); }
void CPU8086_OP8A() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("MOVB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,1)) return; CPU8086_internal_MOV8(modrm_addr8(&params,0,0),instructionbufferb,2); }
void CPU8086_OP8B() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("MOVW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return; CPU8086_internal_MOV16(modrm_addr16(&params,0,0),instructionbufferw,2); }
void CPU8086_OP8C() {modrm_debugger16(&params,1,0); modrm_generateInstructionTEXT("MOVW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,0)) return; CPU8086_internal_MOV16(modrm_addr16(&params,1,0),instructionbufferw,8); }
void CPU8086_OP8D() {modrm_debugger16(&params,0,1); debugger_setcommand("LEA %s,%s",modrm_param1,getLEAtext(&params)); if (CPU8086_internal_MOV16(modrm_addr16(&params,0,0),getLEA(&params),0)) return; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{  CPU[activeCPU].cycles_OP += 2; /* Load effective address */ } }
void CPU8086_OP8E() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("MOVW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return; if (CPU8086_internal_MOV16(modrm_addr16(&params,0,0),instructionbufferw,8)) return; if ((params.info[0].reg16 == &CPU[activeCPU].registers->SS) && (params.info[1].isreg == 1)) { CPU[activeCPU].allowInterrupts = 0; /* Inhabit all interrupts up to the next instruction */ } }
void CPU8086_OP90() /*NOP*/ {modrm_generateInstructionTEXT("NOP",0,0,PARAM_NONE);/*NOP (XCHG AX,AX)*/ if (CPU8086_internal_XCHG16(&REG_AX,&REG_AX,1)) return; /* NOP */}
void CPU8086_OP91() {modrm_generateInstructionTEXT("XCHG CX,AX",0,0,PARAM_NONE);/*XCHG AX,CX*/ CPU8086_internal_XCHG16(&REG_CX,&REG_AX,1); /*XCHG CX,AX*/ }
void CPU8086_OP92() {modrm_generateInstructionTEXT("XCHG DX,AX",0,0,PARAM_NONE);/*XCHG AX,DX*/ CPU8086_internal_XCHG16(&REG_DX,&REG_AX,1); /*XCHG DX,AX*/ }
void CPU8086_OP93() {modrm_generateInstructionTEXT("XCHG BX,AX",0,0,PARAM_NONE);/*XCHG AX,BX*/ CPU8086_internal_XCHG16(&REG_BX,&REG_AX,1); /*XCHG BX,AX*/ }
void CPU8086_OP94() {modrm_generateInstructionTEXT("XCHG SP,AX",0,0,PARAM_NONE);/*XCHG AX,SP*/ CPU8086_internal_XCHG16(&REG_SP,&REG_AX,1); /*XCHG SP,AX*/ }
void CPU8086_OP95() {modrm_generateInstructionTEXT("XCHG BP,AX",0,0,PARAM_NONE);/*XCHG AX,BP*/ CPU8086_internal_XCHG16(&REG_BP,&REG_AX,1); /*XCHG BP,AX*/ }
void CPU8086_OP96() {modrm_generateInstructionTEXT("XCHG SI,AX",0,0,PARAM_NONE);/*XCHG AX,SI*/ CPU8086_internal_XCHG16(&REG_SI,&REG_AX,1); /*XCHG SI,AX*/ }
void CPU8086_OP97() {modrm_generateInstructionTEXT("XCHG DI,AX",0,0,PARAM_NONE);/*XCHG AX,DI*/ CPU8086_internal_XCHG16(&REG_DI,&REG_AX,1); /*XCHG DI,AX*/ }
void CPU8086_OP98() {modrm_generateInstructionTEXT("CBW",0,0,PARAM_NONE);/*CBW : sign extend AL to AX*/ CPU8086_internal_CBW();/*CBW : sign extend AL to AX (8088+)*/ }
void CPU8086_OP99() {modrm_generateInstructionTEXT("CWD",0,0,PARAM_NONE);/*CWD : sign extend AX to DX::AX*/ CPU8086_internal_CWD();/*CWD : sign extend AX to DX::AX (8088+)*/ }
void CPU8086_OP9A() {/*CALL Ap*/ INLINEREGISTER uint_32 segmentoffset = imm32; debugger_setcommand("CALL %04x:%04x", (segmentoffset>>16), (segmentoffset&CPU_EIPmask())); CPU8086_CALLF((segmentoffset>>16)&0xFFFF,segmentoffset&CPU_EIPmask()); if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 28; /* Intersegment direct */ } }
void CPU8086_OP9B() {modrm_generateInstructionTEXT("WAIT",0,0,PARAM_NONE);/*WAIT : wait for TEST pin activity. (UNIMPLEMENTED)*/ CPU[activeCPU].wait = 1;/*9B: WAIT : wait for TEST pin activity. (Edit: continue on interrupts or 8087+!!!)*/ }
void CPU8086_OP9C() {modrm_generateInstructionTEXT("PUSHF",0,0,PARAM_NONE);/*PUSHF*/ if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_PUSHw(0,&REG_FLAGS)) return; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSWRITE; /*PUSHF timing!*/ } }
void CPU8086_OP9D() {modrm_generateInstructionTEXT("POPF", 0, 0, PARAM_NONE);/*POPF*/ static word tempflags; if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,0,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_instructionstepPOPtimeout(0)) return; /*POP timeout*/ if (CPU8086_POPw(2,&tempflags)) return; if (disallowPOPFI()) { tempflags &= ~0x200; tempflags |= REG_FLAGS&0x200; /* Ignore any changes to the Interrupt flag! */ } if (getCPL()) { tempflags &= ~0x3000; tempflags |= REG_FLAGS&0x3000; /* Ignore any changes to the IOPL when not at CPL 0! */ } REG_FLAGS = tempflags; updateCPUmode(); /*POPF*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{  CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; /*POPF timing!*/ } CPU[activeCPU].allowTF = 0; /*Disallow TF to be triggered after the instruction!*/ }
void CPU8086_OP9E() {modrm_generateInstructionTEXT("SAHF", 0, 0, PARAM_NONE);/*SAHF : Save AH to lower half of FLAGS.*/ REG_FLAGS = ((REG_FLAGS & 0xFF00) | REG_AH); updateCPUmode(); /*SAHF : Save AH to lower half of FLAGS.*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{  CPU[activeCPU].cycles_OP += 4; /*SAHF timing!*/ } }
void CPU8086_OP9F() {modrm_generateInstructionTEXT("LAHF",0,0,PARAM_NONE);/*LAHF : Load lower half of FLAGS into AH.*/ REG_AH = (REG_FLAGS&0xFF);/*LAHF : Load lower half of FLAGS into AH.*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{  CPU[activeCPU].cycles_OP += 4; /*LAHF timing!*/ } }
void CPU8086_OPA0() {INLINEREGISTER word theimm = immaddr32; debugger_setcommand("MOVB AL,[%s:%04X]",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV AL,[imm16]*/ if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,1,getCPL(),1)) return; if (CPU8086_internal_stepreaddirectb(0,CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,&instructionbufferb,1)) return; CPU8086_internal_MOV8(&REG_AL,instructionbufferb,1);/*MOV AL,[imm16]*/ }
void CPU8086_OPA1() {INLINEREGISTER word theimm = immaddr32; debugger_setcommand("MOVW AX,[%s:%04X]",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV AX,[imm16]*/  if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,1,getCPL(),1)) return; if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm+1,1,getCPL(),1)) return; if (CPU8086_internal_stepreaddirectw(0,CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,&instructionbufferw,1)) return; CPU8086_internal_MOV16(&REG_AX,instructionbufferw,1);/*MOV AX,[imm16]*/ }
void CPU8086_OPA2() {INLINEREGISTER word theimm = immaddr32; debugger_setcommand("MOVB [%s:%04X],AL",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV [imm16],AL*/ custommem = 1; customoffset = theimm; if (CPU8086_internal_MOV8(NULL,REG_AL,1)) return;/*MOV [imm16],AL*/ custommem = 0; }
void CPU8086_OPA3() {INLINEREGISTER word theimm = immaddr32; debugger_setcommand("MOVW [%s:%04X],AX",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV [imm16], AX*/ custommem = 1; customoffset = theimm; if (CPU8086_internal_MOV16(NULL,REG_AX,1)) return;/*MOV [imm16], AX*/ custommem = 0; }
void CPU8086_OPA4() {modrm_generateInstructionTEXT("MOVSB",0,0,PARAM_NONE);/*MOVSB*/ CPU8086_internal_MOVSB();/*MOVSB*/ }
void CPU8086_OPA5() {modrm_generateInstructionTEXT("MOVSW",0,0,PARAM_NONE);/*MOVSW*/ CPU8086_internal_MOVSW();/*MOVSW*/ }
void CPU8086_OPA6() {debugger_setcommand("CMPSB [%s:ESI],[ES:EDI]",CPU_textsegment(CPU_SEGMENT_DS));/*CMPSB*/ CPU8086_internal_CMPSB();/*CMPSB*/ }
void CPU8086_OPA7() {debugger_setcommand("CMPSW [%s:ESI],[ES:EDI]",CPU_textsegment(CPU_SEGMENT_DS));/*CMPSW*/ CPU8086_internal_CMPSW();/*CMPSW*/ }
void CPU8086_OPA8() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("TESTB AL,",0,theimm,PARAM_IMM8);/*TEST AL,imm8*/ CPU8086_internal_TEST8(REG_AL,theimm,1);/*TEST AL,imm8*/ }
void CPU8086_OPA9() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("TESTW AX,",0,theimm,PARAM_IMM16);/*TEST AX,imm16*/ CPU8086_internal_TEST16(REG_AX,theimm,1);/*TEST AX,imm16*/ }
void CPU8086_OPAA() {modrm_generateInstructionTEXT("STOSB",0,0,PARAM_NONE);/*STOSB*/ CPU8086_internal_STOSB();/*STOSB*/ }
void CPU8086_OPAB() {modrm_generateInstructionTEXT("STOSW",0,0,PARAM_NONE);/*STOSW*/ CPU8086_internal_STOSW();/*STOSW*/ }
void CPU8086_OPAC() {modrm_generateInstructionTEXT("LODSB",0,0,PARAM_NONE);/*LODSB*/ CPU8086_internal_LODSB();/*LODSB*/ }
void CPU8086_OPAD() {modrm_generateInstructionTEXT("LODSW",0,0,PARAM_NONE);/*LODSW*/ CPU8086_internal_LODSW();/*LODSW*/ }
void CPU8086_OPAE() {modrm_generateInstructionTEXT("SCASB",0,0,PARAM_NONE);/*SCASB*/ CPU8086_internal_SCASB();/*SCASB*/ }
void CPU8086_OPAF() {modrm_generateInstructionTEXT("SCASW",0,0,PARAM_NONE);/*SCASW*/ CPU8086_internal_SCASW();/*SCASW*/ }
void CPU8086_OPB0() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB AL,",0,theimm,PARAM_IMM8);/*MOV AL,imm8*/ CPU8086_internal_MOV8(&REG_AL,theimm,4);/*MOV AL,imm8*/ }
void CPU8086_OPB1() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB CL,",0,theimm,PARAM_IMM8);/*MOV CL,imm8*/ CPU8086_internal_MOV8(&REG_CL,theimm,4);/*MOV CL,imm8*/ }
void CPU8086_OPB2() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB DL,",0,theimm,PARAM_IMM8);/*MOV DL,imm8*/ CPU8086_internal_MOV8(&REG_DL,theimm,4);/*MOV DL,imm8*/ }
void CPU8086_OPB3() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB BL,",0,theimm,PARAM_IMM8);/*MOV BL,imm8*/ CPU8086_internal_MOV8(&REG_BL,theimm,4);/*MOV BL,imm8*/ }
void CPU8086_OPB4() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB AH,",0,theimm,PARAM_IMM8);/*MOV AH,imm8*/ CPU8086_internal_MOV8(&REG_AH,theimm,4);/*MOV AH,imm8*/ }
void CPU8086_OPB5() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB CH,",0,theimm,PARAM_IMM8);/*MOV CH,imm8*/ CPU8086_internal_MOV8(&REG_CH,theimm,4);/*MOV CH,imm8*/ }
void CPU8086_OPB6() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB DH,",0,theimm,PARAM_IMM8);/*MOV DH,imm8*/ CPU8086_internal_MOV8(&REG_DH,theimm,4);/*MOV DH,imm8*/ }
void CPU8086_OPB7() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB BH,",0,theimm,PARAM_IMM8);/*MOV BH,imm8*/ CPU8086_internal_MOV8(&REG_BH,theimm,4);/*MOV BH,imm8*/ }
void CPU8086_OPB8() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW AX,",0,theimm,PARAM_IMM16);/*MOV AX,imm16*/ CPU8086_internal_MOV16(&REG_AX,theimm,4);/*MOV AX,imm16*/ }
void CPU8086_OPB9() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW CX,",0,theimm,PARAM_IMM16);/*MOV CX,imm16*/ CPU8086_internal_MOV16(&REG_CX,theimm,4);/*MOV CX,imm16*/ }
void CPU8086_OPBA() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW DX,",0,theimm,PARAM_IMM16);/*MOV DX,imm16*/ CPU8086_internal_MOV16(&REG_DX,theimm,4);/*MOV DX,imm16*/ }
void CPU8086_OPBB() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW BX,",0,theimm,PARAM_IMM16);/*MOV BX,imm16*/ CPU8086_internal_MOV16(&REG_BX,theimm,4);/*MOV BX,imm16*/ }
void CPU8086_OPBC() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW SP,",0,theimm,PARAM_IMM16);/*MOV SP,imm16*/ CPU8086_internal_MOV16(&REG_SP,theimm,4);/*MOV SP,imm16*/ }
void CPU8086_OPBD() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW BP,",0,theimm,PARAM_IMM16);/*MOV BP,imm16*/ CPU8086_internal_MOV16(&REG_BP,theimm,4);/*MOV BP,imm16*/ }
void CPU8086_OPBE() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW SI,",0,theimm,PARAM_IMM16);/*MOV SI,imm16*/ CPU8086_internal_MOV16(&REG_SI,theimm,4);/*MOV SI,imm16*/ }
void CPU8086_OPBF() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW DI,",0,theimm,PARAM_IMM16);/*MOV DI,imm16*/ CPU8086_internal_MOV16(&REG_DI,theimm,4);/*MOV DI,imm16*/ }
void CPU8086_OPC2() {INLINEREGISTER word popbytes = immw;/*RET imm16 (Near return to calling proc and POP imm16 bytes)*/ modrm_generateInstructionTEXT("RET",0,popbytes,PARAM_IMM16); /*RET imm16 (Near return to calling proc and POP imm16 bytes)*/ CPU8086_internal_RET(popbytes,1); }
void CPU8086_OPC3() {modrm_generateInstructionTEXT("RET",0,0,PARAM_NONE);/*RET (Near return to calling proc)*/ /*RET (Near return to calling proc)*/ CPU8086_internal_RET(0,0); }
void CPU8086_OPC4() /*LES modr/m*/ {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("LES",0,0,PARAM_MODRM12); CPU8086_internal_LXS(CPU_SEGMENT_ES); /*Load new ES!*/ }
void CPU8086_OPC5() /*LDS modr/m*/ {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("LDS",0,0,PARAM_MODRM12); CPU8086_internal_LXS(CPU_SEGMENT_DS); /*Load new DS!*/ }
void CPU8086_OPC6() {byte val = immb; modrm_debugger8(&params,0,1); debugger_setcommand("MOVB %s,%02x",modrm_param2,val); if (modrm_check8(&params,1,0)) return; if (CPU8086_instructionstepwritemodrmb(0,val,1)) return; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ if (MODRM_EA(params)) CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSWRITE; /* Imm->Mem */ else CPU[activeCPU].cycles_OP += 4; /* Imm->Reg */ } }
void CPU8086_OPC7() {word val = immw; modrm_debugger16(&params,0,1); debugger_setcommand("MOVW %s,%04x",modrm_param2,val); if (modrm_check16(&params,1,0)) return; if (CPU8086_instructionstepwritemodrmw(0,val,1,0)) return; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ if (MODRM_EA(params)) { CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSWRITE; /* Imm->Mem */ } else CPU[activeCPU].cycles_OP += 4; /* Imm->Reg */ } }
void CPU8086_OPCA() {INLINEREGISTER word popbytes = immw;/*RETF imm16 (Far return to calling proc and pop imm16 bytes)*/ modrm_generateInstructionTEXT("RETF",0,popbytes,PARAM_IMM16); /*RETF imm16 (Far return to calling proc and pop imm16 bytes)*/ CPU8086_internal_RETF(popbytes,1); }
void CPU8086_OPCB() {modrm_generateInstructionTEXT("RETF",0,0,PARAM_NONE); /*RETF (Far return to calling proc)*/ CPU8086_internal_RETF(0,0); }
void CPU8086_OPCC() {modrm_generateInstructionTEXT("INT 3",0,0,PARAM_NONE); /*INT 3*/ if (CPU_faultraised(EXCEPTION_CPUBREAKPOINT)) { CPU8086_int(EXCEPTION_CPUBREAKPOINT,1); } /*INT 3*/ }
void CPU8086_OPCD() {INLINEREGISTER byte theimm = immb; INTdebugger8086(); modrm_generateInstructionTEXT("INT",0,theimm,PARAM_IMM8);/*INT imm8*/ CPU8086_int(theimm,0);/*INT imm8*/ }
void CPU8086_OPCE() {modrm_generateInstructionTEXT("INTO",0,0,PARAM_NONE);/*INTO*/ CPU8086_internal_INTO();/*INTO*/ }
void CPU8086_OPCF() {modrm_generateInstructionTEXT("IRET",0,0,PARAM_NONE);/*IRET*/ CPU8086_IRET();/*IRET : also restore interrupt flag!*/ }
void CPU8086_OPD4() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("AAM",0,theimm,PARAM_IMM8);/*AAM*/ CPU8086_internal_AAM(theimm);/*AAM*/ }
void CPU8086_OPD5() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("AAD",0,theimm,PARAM_IMM8);/*AAD*/ CPU8086_internal_AAD(theimm);/*AAD*/ }
void CPU8086_OPD6(){debugger_setcommand("SALC"); REG_AL=FLAG_CF?0xFF:0x00; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 2; } } //Special case on the 8086: SALC!
void CPU8086_OPD7(){CPU8086_internal_XLAT();}
void CPU8086_OPE0(){INLINEREGISTER sbyte rel8; rel8 = imm8(); modrm_generateInstructionTEXT("LOOPNZ",0, ((REG_EIP+rel8)&CPU_EIPmask()),CPU_EIPSize()); if ((--REG_CX) && (!FLAG_ZF)){CPU_JMPrel(rel8); CPU_flushPIQ(-1); /*We're jumping to another address*/ didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 19; } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 5; } /* Branch not taken */}}
void CPU8086_OPE1(){INLINEREGISTER sbyte rel8; rel8 = imm8(); modrm_generateInstructionTEXT("LOOPZ",0, ((REG_EIP+rel8)&CPU_EIPmask()),CPU_EIPSize());if ((--REG_CX) && (FLAG_ZF)){CPU_JMPrel(rel8);CPU_flushPIQ(-1); /*We're jumping to another address*/ didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{  CPU[activeCPU].cycles_OP += 18; } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 6; } /* Branch not taken */}}
void CPU8086_OPE2(){INLINEREGISTER sbyte rel8; rel8 = imm8(); modrm_generateInstructionTEXT("LOOP", 0,((REG_EIP+rel8)&CPU_EIPmask()),CPU_EIPSize());if (--REG_CX){CPU_JMPrel(rel8);CPU_flushPIQ(-1); /*We're jumping to another address*/ didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 17; } /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 5; } /* Branch not taken */}}
void CPU8086_OPE3(){INLINEREGISTER sbyte rel8; rel8 = imm8(); modrm_generateInstructionTEXT("JCXZ",0,((REG_EIP+rel8)&CPU_EIPmask()),CPU_EIPSize()); if (!REG_CX){CPU_JMPrel(rel8);CPU_flushPIQ(-1); /*We're jumping to another address*/ didJump = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 18; }  /* Branch taken */} else { if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 6; } /* Branch not taken */}}
void CPU8086_OPE4(){INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("IN AL,",0,theimm,PARAM_IMM8); if (CPU_PORT_IN_B(0,theimm,&REG_AL)) return; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{  CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSREAD; } /*Timings!*/}
void CPU8086_OPE5(){INLINEREGISTER byte theimm = immb;modrm_generateInstructionTEXT("IN AX,",0,theimm,PARAM_IMM8); if (CPU_PORT_IN_W(0,theimm,&REG_AX)) return; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSREAD; /*Timings!*/ } }
void CPU8086_OPE6(){INLINEREGISTER byte theimm = immb;debugger_setcommand("OUT %02X,AL",theimm); if(CPU_PORT_OUT_B(0,theimm,REG_AL)) return; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSWRITE; } /*Timings!*/ }
void CPU8086_OPE7(){INLINEREGISTER byte theimm = immb; debugger_setcommand("OUT %02X,AX",theimm); if (CPU_PORT_OUT_W(0,theimm,REG_AX)) return; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 10-EU_CYCLES_SUBSTRACT_ACCESSWRITE; /*Timings!*/ }}
void CPU8086_OPE8(){INLINEREGISTER sword reloffset = imm16(); modrm_generateInstructionTEXT("CALL",0,((REG_EIP + reloffset)&CPU_EIPmask()),CPU_EIPSize()); if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; } if (CPU8086_PUSHw(0,&REG_IP)) return; CPU_JMPrel(reloffset);CPU_flushPIQ(-1); /*We're jumping to another address*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 19-EU_CYCLES_SUBSTRACT_ACCESSREAD; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Intrasegment direct */}
void CPU8086_OPE9(){INLINEREGISTER sword reloffset = imm16(); modrm_generateInstructionTEXT("JMP",0,((REG_EIP + reloffset)&CPU_EIPmask()),CPU_EIPSize()); CPU_JMPrel(reloffset);CPU_flushPIQ(-1); /*We're jumping to another address*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 15; CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/ } /* Intrasegment direct */}
void CPU8086_OPEA(){INLINEREGISTER uint_32 segmentoffset = imm32(); debugger_setcommand("JMP %04X:%04X", (segmentoffset>>16), (segmentoffset&CPU_EIPmask())); destEIP = (segmentoffset&CPU_EIPmask()); segmentWritten(CPU_SEGMENT_CS, (segmentoffset>>16), 1); CPU_flushPIQ(-1); if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 15; } /* Intersegment direct */}
void CPU8086_OPEB(){INLINEREGISTER sbyte reloffset = imm8(); modrm_generateInstructionTEXT("JMP",0,((REG_EIP + reloffset)&CPU_EIPmask()),CPU_EIPSize()); CPU_JMPrel(reloffset);CPU_flushPIQ(-1); /*We're jumping to another address*/ if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 15; CPU[activeCPU].cycles_stallBIU += 6; /*Stall the BIU partly now!*/ } /* Intrasegment direct short */}
void CPU8086_OPEC(){modrm_generateInstructionTEXT("IN AL,DX",0,0,PARAM_NONE); if (CPU_PORT_IN_B(0,REG_DX,&REG_AL)) return; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; } /*Timings!*/}
void CPU8086_OPED(){modrm_generateInstructionTEXT("IN AX,DX",0,0,PARAM_NONE); if (CPU_PORT_IN_W(0,REG_DX,&REG_AX)) return; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; /*Timings!*/ } }
void CPU8086_OPEE(){modrm_generateInstructionTEXT("OUT DX,AL",0,0,PARAM_NONE); if (CPU_PORT_OUT_B(0,REG_DX,REG_AL)) return; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSWRITE; } /*Timings!*/}
void CPU8086_OPEF(){modrm_generateInstructionTEXT("OUT DX,AX",0,0,PARAM_NONE); if (CPU_PORT_OUT_W(0,REG_DX,REG_AX)) return; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSWRITE; /*Timings!*/ } /*To memory?*/}
void CPU8086_OPF1(){modrm_generateInstructionTEXT("<Undefined and reserved opcode, no error>",0,0,PARAM_NONE);}
void CPU8086_OPF4(){modrm_generateInstructionTEXT("HLT",0,0,PARAM_NONE); CPU[activeCPU].halt = 1; if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 2; } /*Special timing!*/}
void CPU8086_OPF5(){modrm_generateInstructionTEXT("CMC",0,0,PARAM_NONE); FLAGW_CF(!FLAG_CF); if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 2; } /*Special timing!*/}
void CPU8086_OPF8(){modrm_generateInstructionTEXT("CLC",0,0,PARAM_NONE); FLAGW_CF(0); if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 2; } /*Special timing!*/}
void CPU8086_OPF9(){modrm_generateInstructionTEXT("STC",0,0,PARAM_NONE); FLAGW_CF(1); if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 2; } /*Special timing!*/}
void CPU8086_OPFA(){modrm_generateInstructionTEXT("CLI",0,0,PARAM_NONE); if (checkSTICLI()) { FLAGW_IF(0); } if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 2; } /*Special timing!*/}
void CPU8086_OPFB(){modrm_generateInstructionTEXT("STI",0,0,PARAM_NONE); if (checkSTICLI()) { FLAGW_IF(1); } if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 2; } /*Special timing!*/}
void CPU8086_OPFC(){modrm_generateInstructionTEXT("CLD",0,0,PARAM_NONE); FLAGW_DF(0); if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 2; } /*Special timing!*/}
void CPU8086_OPFD(){modrm_generateInstructionTEXT("STD",0,0,PARAM_NONE); FLAGW_DF(1); if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */{ CPU[activeCPU].cycles_OP += 2; } /*Special timing!*/}

/*

NOW COME THE GRP1-5 OPCODES:

*/

//GRP1

/*

DEBUG: REALLY SUPPOSED TO HANDLE OP80-83 HERE?

*/

void CPU8086_OP80() //GRP1 Eb,Ib
{
	INLINEREGISTER byte imm = immb;
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger8(&params,1,0);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //ADD
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADDB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		CPU8086_internal_ADD8(modrm_addr8(&params,1,0),imm,3); //ADD Eb, Ib
		break;
	case 1: //OR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ORB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		CPU8086_internal_OR8(modrm_addr8(&params,1,0),imm,3); //OR Eb, Ib
		break;
	case 2: //ADC
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADCB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		CPU8086_internal_ADC8(modrm_addr8(&params,1,0),imm,3); //ADC Eb, Ib
		break;
	case 3: //SBB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SBBB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		CPU8086_internal_SBB8(modrm_addr8(&params,1,0),imm,3); //SBB Eb, Ib
		break;
	case 4: //AND
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ANDB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		CPU8086_internal_AND8(modrm_addr8(&params,1,0),imm,3); //AND Eb, Ib
		break;
	case 5: //SUB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SUBB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		CPU8086_internal_SUB8(modrm_addr8(&params,1,0),imm,3); //SUB Eb, Ib
		break;
	case 6: //XOR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("XORB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		CPU8086_internal_XOR8(modrm_addr8(&params,1,0),imm,3); //XOR Eb, Ib
		break;
	case 7: //CMP
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("CMPB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		if (modrm_check8(&params,1,1)) return; //Abort when needed!
		if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,1)) return;
		CMP_b(instructionbufferb,imm,3); //CMP Eb, Ib
		break;
	default:
		break;
	}
}

void CPU8086_OP81() //GRP1 Ev,Iv
{
	INLINEREGISTER word imm = immw;
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger16(&params,1,0);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //ADD
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADDW %s,%04X",&modrm_param1,imm); //ADD Eb, Ib
		}
		CPU8086_internal_ADD16(modrm_addr16(&params,1,0),imm,3); //ADD Eb, Ib
		break;
	case 1: //OR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ORW %s,%04X",&modrm_param1,imm); //OR Eb, Ib
		}
		CPU8086_internal_OR16(modrm_addr16(&params,1,0),imm,3); //OR Eb, Ib
		break;
	case 2: //ADC
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADCW %s,%04X",&modrm_param1,imm); //ADC Eb, Ib
		}
		CPU8086_internal_ADC16(modrm_addr16(&params,1,0),imm,3); //ADC Eb, Ib
		break;
	case 3: //SBB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SBBW %s,%04X",&modrm_param1,imm); //SBB Eb, Ib
		}
		CPU8086_internal_SBB16(modrm_addr16(&params,1,0),imm,3); //SBB Eb, Ib
		break;
	case 4: //AND
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ANDW %s,%04X",&modrm_param1,imm); //AND Eb, Ib
		}
		CPU8086_internal_AND16(modrm_addr16(&params,1,0),imm,3); //AND Eb, Ib
		break;
	case 5: //SUB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SUBW %s,%04X",&modrm_param1,imm); //SUB Eb, Ib
		}
		CPU8086_internal_SUB16(modrm_addr16(&params,1,0),imm,3); //SUB Eb, Ib
		break;
	case 6: //XOR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("XORW %s,%04X",&modrm_param1,imm); //XOR Eb, Ib
		}
		CPU8086_internal_XOR16(modrm_addr16(&params,1,0),imm,3); //XOR Eb, Ib
		break;
	case 7: //CMP
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("CMPW %s,%04X",&modrm_param1,imm); //CMP Eb, Ib
		}
		if (modrm_check16(&params,1,1)) return; //Abort when needed!
		if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return;
		CMP_w(instructionbufferw,imm,3); //CMP Eb, Ib
		break;
	default:
		break;
	}
}

void CPU8086_OP82() //GRP1 Eb,Ib (same as OP80)
{
	CPU8086_OP80(); //Same!
}

void CPU8086_OP83() //GRP1 Ev,Ib
{
	INLINEREGISTER word imm;
	imm = immb;
	if (imm&0x80) imm |= 0xFF00; //Sign extend!
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger16(&params,1,0);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //ADD
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADDW %s,%04X",&modrm_param1,imm); //ADD Eb, Ib
		}
		CPU8086_internal_ADD16(modrm_addr16(&params,1,0),imm,3); //ADD Eb, Ib
		break;
	case 1: //OR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ORW %s,%04X",&modrm_param1,imm); //OR Eb, Ib
		}
		CPU8086_internal_OR16(modrm_addr16(&params,1,0),imm,3); //OR Eb, Ib
		break;
	case 2: //ADC
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADCW %s,%04X",&modrm_param1,imm); //ADC Eb, Ib
		}
		CPU8086_internal_ADC16(modrm_addr16(&params,1,0),imm,3); //ADC Eb, Ib
		break;
	case 3: //SBB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SBBW %s,%04X",&modrm_param1,imm); //SBB Eb, Ib
		}
		CPU8086_internal_SBB16(modrm_addr16(&params,1,0),imm,3); //SBB Eb, Ib
		break;
	case 4: //AND
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ANDW %s,%04X",&modrm_param1,imm); //AND Eb, Ib
		}
		CPU8086_internal_AND16(modrm_addr16(&params,1,0),imm,3); //AND Eb, Ib
		break;
	case 5: //SUB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SUBW %s,%04X",&modrm_param1,imm); //SUB Eb, Ib
		}
		CPU8086_internal_SUB16(modrm_addr16(&params,1,0),imm,3); //SUB Eb, Ib
		break;
	case 6: //XOR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("XORW %s,%04X",&modrm_param1,imm); //XOR Eb, Ib
		}
		CPU8086_internal_XOR16(modrm_addr16(&params,1,0),imm,3); //XOR Eb, Ib
		break;
	case 7: //CMP
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("CMPW %s,%04X",&modrm_param1,imm); //CMP Eb, Ib
		}
		if (modrm_check16(&params,1,1)) return; //Abort when needed!
		if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return;
		CMP_w(instructionbufferw,imm,3); //CMP Eb, Ib
		break;
	default:
		break;
	}
}

void CPU8086_OP8F() //Undocumented GRP opcode 8F r/m16
{
	if (cpudebugger)
	{
		modrm_debugger16(&params,0,1);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //POP
		//Cycle-accurate emulation of the instruction!
		if (cpudebugger) //Debugger on?
		{
			modrm_generateInstructionTEXT("POPW",16,0,PARAM_MODRM2); //POPW Ew
		}
		if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,0,0)) return; ++CPU[activeCPU].stackchecked; }
		if (modrm_check16(&params,1,0)) return; //Abort when needed!
		static word value;
		//Execution step!
		if (CPU8086_instructionstepPOPtimeout(0)) return; /*POP timeout*/
		if (CPU8086_POPw(2,&value)) return; //POP first!
		if (CPU8086_instructionstepwritemodrmw(4,value,1,0)) return; //POP r/m16
		if (params.info[1].reg16 == &CPU[activeCPU].registers->SS) //Popping into SS?
		{
			CPU[activeCPU].allowInterrupts = 0; /* Inhabit all interrupts up to the next instruction */
		}
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Mem?
			{
				CPU[activeCPU].cycles_OP += 17-EU_CYCLES_SUBSTRACT_ACCESSRW; /*Pop Mem!*/
			}
			else //Reg?
			{
				CPU[activeCPU].cycles_OP += 8-EU_CYCLES_SUBSTRACT_ACCESSREAD; /*Pop Reg!*/
			}
		}
		break;
	default: //Unknown opcode or special?
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("Unknown opcode: 8F /%i",MODRM_REG(params.modrm)); //Error!
		}
		CPU_unkOP(); //Execute the unknown opcode exception handler, if any!
		break;
	}
}

void CPU8086_OPD0() //GRP2 Eb,1
{
	thereg = MODRM_REG(params.modrm);
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
	if (modrm_check8(&params,1,1)) return; //Abort when needed!
	if (modrm_check8(&params,1,0)) return; //Abort when needed!
	if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,1)) return;
	oper1b = instructionbufferb;
	if (CPU8086_instructionstepwritemodrmb(2,op_grp2_8(1,0),1)) return;
}
void CPU8086_OPD1() //GRP2 Ev,1
{
	thereg = MODRM_REG(params.modrm);
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
	if (modrm_check16(&params,1,1)) return; //Abort when needed!
	if (modrm_check16(&params,1,0)) return; //Abort when needed!
	if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return;
	oper1 = instructionbufferw;
	if (CPU8086_instructionstepwritemodrmw(2,op_grp2_16(1,0),1,0)) return;
}
void CPU8086_OPD2() //GRP2 Eb,CL
{
	thereg = MODRM_REG(params.modrm);
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
	if (modrm_check8(&params,1,1)) return; //Abort when needed!
	if (modrm_check8(&params,1,0)) return; //Abort when needed!
	if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,1)) return;
	oper1b = instructionbufferb;
	if (CPU8086_instructionstepwritemodrmb(2,op_grp2_8(REG_CL,1),1)) return;
}
void CPU8086_OPD3() //GRP2 Ev,CL
{
	thereg = MODRM_REG(params.modrm);
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
	if (modrm_check16(&params,1,1)) return; //Abort when needed!
	if (modrm_check16(&params,1,0)) return; //Abort when needed!
	if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return;
	oper1 = instructionbufferw;
	if (CPU8086_instructionstepwritemodrmw(2,op_grp2_16(REG_CL,1),1,0)) return;
}


void CPU8086_OPF6() //GRP3a Eb
{
	thereg = MODRM_REG(params.modrm);
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
	if (modrm_check8(&params,1,1)) return; //Abort when needed!
	if ((MODRM_REG(params.modrm)>1) && (MODRM_REG(params.modrm)<4))
	{
		if (modrm_check8(&params,1,0)) return; //Abort when needed!
	}
	if (CPU8086_instructionstepreadmodrmb(0,&instructionbufferb,1)) return;
	oper1b = instructionbufferb;
	op_grp3_8();
	if ((MODRM_REG(params.modrm)>1) && (MODRM_REG(params.modrm)<4))
	{
		if (CPU8086_instructionstepwritemodrmb(2,res8,1)) return;
	}
}
void CPU8086_OPF7() //GRP3b Ev
{
	thereg = MODRM_REG(params.modrm);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger16(&params,0,1); //Get src!
		switch (thereg) //What function?
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
	if (modrm_check16(&params,1,1)) return; //Abort when needed!
	if ((thereg>1) && (thereg<4)) //NOT/NEG?
	{
		if (modrm_check16(&params,1,0)) return; //Abort when needed!
	}
	if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return;
	oper1 = instructionbufferw;
	op_grp3_16();
	if ((thereg>1) && (thereg<4)) //NOT/NEG?
	{
		if (CPU8086_instructionstepwritemodrmw(2,res16,1,0)) return;
	}
}
//All OK up till here.

/*

DEBUG: REALLY SUPPOSED TO HANDLE HERE?

*/

void CPU8086_OPFE() //GRP4 Eb
{
	modrm_debugger8(&params,0,1);
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //INC
		if (cpudebugger) //Debugger on?
		{
			modrm_generateInstructionTEXT("INCB",8,0,PARAM_MODRM2); //INC!
		}
		if (modrm_check8(&params,1,1)) return; //Abort when needed!
		if (modrm_check8(&params,1,0)) return; //Abort when needed!
		MODRM_src0 = 1; //We're taking this source!
		CPU8086_internal_INC8(modrm_addr8(&params,1,0));
		break;
	case 1: //DEC
		if (cpudebugger) //Debugger on?
		{
			modrm_generateInstructionTEXT("DECB",8,0,PARAM_MODRM2); //DEC!
		}
		if (modrm_check8(&params,1,1)) return; //Abort when needed!
		if (modrm_check8(&params,1,0)) return; //Abort when needed!
		MODRM_src0 = 1; //We're taking this source!
		CPU8086_internal_DEC8(modrm_addr8(&params,1,0));
		break;
	default: //Unknown opcode or special?
		CPU_unkOP(); //Execute the unknown opcode exception handler, if any!
		break;
	}
}

void CPU8086_OPFF() //GRP5 Ev
{
	thereg = MODRM_REG(params.modrm);
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
			modrm_generateInstructionTEXT("PUSHW",16,0,PARAM_MODRM2); //PUSH!
			break;
		case 7: //---
			debugger_setcommand("<UNKNOWN Opcode: GRP5(w) /7>");
			break;
		default:
			break;
		}
	}
	if (modrm_check16(&params,1,1)) return; //Abort when needed!
	if (CPU8086_instructionstepreadmodrmw(0,&instructionbufferw,1)) return;
	oper1 = instructionbufferw;
	ea = modrm_offset16(&params,1);
	op_grp5();
}

/*

Special stuff for NO COprocessor (8087) present/available (default)!

*/


void FPU8087_noCOOP(){
	debugger_setcommand("<No COprocessor OPcodes implemented!>");
	if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
	{
		CPU[activeCPU].cycles_OP += MODRM_EA(params)?8:2; //No hardware interrupt to use anymore!
	}
}

//Gecontroleerd: 100% OK!

//Now, the GRP opcodes!

OPTINLINE void op_grp2_cycles(byte cnt, byte varshift)
{
	switch (varshift) //What type of shift are we using?
	{
	case 0: //Reg/Mem with 1 shift?
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Mem?
			{
				CPU[activeCPU].cycles_OP += 15-(EU_CYCLES_SUBSTRACT_ACCESSRW); //Mem
			}
			else //Reg?
			{
				CPU[activeCPU].cycles_OP += 2; //Reg
			}
		}
		break;
	case 1: //Reg/Mem with variable shift?
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Mem?
			{
				CPU[activeCPU].cycles_OP += 20 + (cnt << 2)- (EU_CYCLES_SUBSTRACT_ACCESSRW); //Mem
			}
			else //Reg?
			{
				CPU[activeCPU].cycles_OP += 8 + (cnt << 2); //Reg
			}
		}
		break;
	case 2: //Reg/Mem with immediate variable shift(NEC V20/V30)?
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Mem?
			{
				CPU[activeCPU].cycles_OP += 20 + (cnt << 2) - (EU_CYCLES_SUBSTRACT_ACCESSRW); //Mem
			}
			else //Reg?
			{
				CPU[activeCPU].cycles_OP += 8 + (cnt << 2); //Reg
			}
		}
		break;
	}
}

byte op_grp2_8(byte cnt, byte varshift) {
	//word d,
	INLINEREGISTER word s, shift, oldCF, msb;
	word backup;
	//if (cnt>0x8) return(oper1b); //NEC V20/V30+ limits shift count
	s = oper1b;
	oldCF = FLAG_CF;
	if (EMULATED_CPU >= CPU_NECV30) cnt &= 0x1F; //Clear the upper 3 bits to become a NEC V20/V30+!
	switch (thereg) {
	case 0: //ROL r/m8
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80) FLAGW_CF(1); else FLAGW_CF(0);
			s = s << 1;
			s = s | FLAG_CF;
		}
		if (cnt==1) FLAGW_OF(FLAG_CF ^ ((s >> 7) & 1));
		break;

	case 1: //ROR r/m8
		for (shift = 1; shift <= cnt; shift++) {
			FLAGW_CF(s & 1);
			s = (s >> 1) | (FLAG_CF << 7);
		}
		if (cnt==1) FLAGW_OF((s >> 7) ^ ((s >> 6) & 1));
		break;

	case 2: //RCL r/m8
		for (shift = 1; shift <= cnt; shift++) {
			oldCF = FLAG_CF;
			if (s & 0x80) FLAGW_CF(1); else FLAGW_CF(0);
			s = s << 1;
			s = s | oldCF;
		}
		if (cnt==1) FLAGW_OF(FLAG_CF ^ ((s >> 7) & 1));
		break;

	case 3: //RCR r/m8
		for (shift = 1; shift <= cnt; shift++) {
			oldCF = FLAG_CF;
			FLAGW_CF(s & 1);
			s = (s >> 1) | (oldCF << 7);
		}
		if (cnt==1) FLAGW_OF((s >> 7) ^ ((s >> 6) & 1));
		break;

	case 4: case 6: //SHL r/m8
		//FLAGW_AF(0);
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80) FLAGW_CF(1); else FLAGW_CF(0);
			//if (s & 0x8) FLAGW_AF(1); //Auxiliary carry?
			s = (s << 1) & 0xFF;
		}
		if (cnt==1) FLAGW_OF((FLAG_CF ^ (s >> 7)));
		flag_szp8((uint8_t)(s&0xFF)); break;

	case 5: //SHR r/m8
		if (cnt == 1) FLAGW_OF((s & 0x80) ? 1 : 0); else FLAGW_OF(0);
		//FLAGW_AF(0);
		for (shift = 1; shift <= cnt; shift++) {
			FLAGW_CF(s & 1);
			backup = s; //Save backup!
			s = s >> 1;
			//if (((backup^s)&0x10)) FLAGW_AF(1); //Auxiliary carry?
		}
		flag_szp8((uint8_t)(s & 0xFF)); break;

	case 7: //SAR r/m8
		msb = s & 0x80;
		//FLAGW_AF(0);
		for (shift = 1; shift <= cnt; shift++) {
			FLAGW_CF(s & 1);
			backup = s; //Save backup!
			s = (s >> 1) | msb;
			//if (((backup^s)&0x10)) FLAGW_AF(1); //Auxiliary carry?
		}
		byte tempSF;
		tempSF = FLAG_SF; //Save the SF!
		/*flag_szp8((uint8_t)(s & 0xFF));*/
		//http://www.electronics.dit.ie/staff/tscarff/8086_instruction_set/8086_instruction_set.html#SAR says only C and O flags!
		if (!cnt) //Nothing done?
		{
			FLAGW_SF(tempSF); //We don't update when nothing's done!
		}
		else if (cnt==1) //Overflow is cleared on all 1-bit shifts!
		{
			flag_s8(s); //Affect sign as well!
			FLAGW_OF(0); //Cleared!
		}
		else if (cnt) //Anything shifted at all?
		{
			flag_s8(s); //Affect sign as well!
		}
		if ((EMULATED_CPU>=CPU_NECV30) && cnt) //NECV20+ affected?
		{
			flag_p8(s); //Affect parity as well!
		}
		break;
	}
	op_grp2_cycles(cnt, varshift);
	return(s & 0xFF);
}

word op_grp2_16(byte cnt, byte varshift) {
	//uint32_t d,
	INLINEREGISTER uint_32 s, shift, oldCF, msb;
	//if (cnt>0x10) return(oper1); //NEC V20/V30+ limits shift count
	if (EMULATED_CPU >= CPU_NECV30) cnt &= 0x1F; //Clear the upper 3 bits to become a NEC V20/V30+!
	word backup;
	s = oper1;
	oldCF = FLAG_CF;
	switch (thereg) {
	case 0: //ROL r/m16
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x8000) FLAGW_CF(1); else FLAGW_CF(0);
			s = s << 1;
			s = s | FLAG_CF;
		}
		if (cnt==1) FLAGW_OF(FLAG_CF ^ ((s >> 15) & 1));
		break;

	case 1: //ROR r/m16
		for (shift = 1; shift <= cnt; shift++) {
			FLAGW_CF(s & 1);
			s = (s >> 1) | (FLAG_CF << 15);
		}
		if (cnt==1) FLAGW_OF((s >> 15) ^ ((s >> 14) & 1));
		break;

	case 2: //RCL r/m16
		for (shift = 1; shift <= cnt; shift++) {
			oldCF = FLAG_CF;
			if (s & 0x8000) FLAGW_CF(1); else FLAGW_CF(0);
			s = s << 1;
			s = s | oldCF;
			//oldCF = ((s&0x8000)>>15)&1; //Save FLAG_CF!
			//s = (s<<1)+FLAG_CF;
			//FLAG_CF = oldCF;
		}
		if (cnt==1) FLAGW_OF(FLAG_CF ^ ((s >> 15) & 1));
		break;

	case 3: //RCR r/m16
		if (cnt==1) FLAGW_OF(((s >> 15) & 1) ^ FLAG_CF);
		for (shift = 1; shift <= cnt; shift++) {
			oldCF = FLAG_CF;
			FLAGW_CF(s & 1);
			s = (s >> 1) | (oldCF << 15);
			//oldCF = s&1;
			//s = (s<<1)+(FLAG_CF<<16);
			//FLAG_CF = oldCF;
		}
		if (cnt==1) FLAGW_OF((s >> 15) ^ ((s >> 14) & 1));
		break;

	case 4: case 6: //SHL r/m16
		//FLAGW_AF(0);
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x8000) FLAGW_CF(1); else FLAGW_CF(0);
			//if (s & 0x8) FLAGW_AF(1); //Auxiliary carry?
			s = (s << 1) & 0xFFFF;
		}
		if ((cnt) && (FLAG_CF == (s >> 15))) FLAGW_OF(0); else FLAGW_OF(1);
		flag_szp16(s); break;

	case 5: //SHR r/m16
		if (cnt) FLAGW_OF((s & 0x8000) ? 1 : 0);
		//FLAGW_AF(0);
		for (shift = 1; shift <= cnt; shift++) {
			FLAGW_CF(s & 1);
			backup = s; //Save backup!
			s = s >> 1;
			//if (((backup^s)&0x10)) FLAGW_AF(1); //Auxiliary carry?
		}
		flag_szp16(s); break;

	case 7: //SAR r/m16
		msb = s & 0x8000; //Read the MSB!
		//FLAGW_AF(0);
		for (shift = 1; shift <= cnt; shift++) {
			FLAGW_CF(s & 1);
			backup = s; //Save backup!
			s = (s >> 1) | msb;
			//if (((backup^s)&0x10)) FLAGW_AF(1); //Auxiliary carry?
		}
		byte tempSF;
		tempSF = FLAG_SF; //Save the SF!
		/*flag_szp16(s);*/
		//http://www.electronics.dit.ie/staff/tscarff/8086_instruction_set/8086_instruction_set.html#SAR says only C and O flags!
		if (!cnt) //Nothing done?
		{
			FLAGW_SF(tempSF); //We don't update when nothing's done!
		}
		else if (cnt==1) //Overflow is cleared on all 1-bit shifts!
		{
			flag_s16(s); //Affect sign as well!
			FLAGW_OF(0); //Cleared!
		}
		else if (cnt) //Anything shifted at all?
		{
			flag_s16(s); //Affect sign as well!
		}
		if ((EMULATED_CPU>=CPU_NECV30) && cnt) //NECV20+ affected?
		{
			flag_p16(s); //Affect parity as well!
			flag_s16(s); //Affect sign as well!
		}
		break;
	}
	op_grp2_cycles(cnt, varshift|4);
	return(s & 0xFFFF);
}

OPTINLINE void op_div8(word valdiv, byte divisor) {
	if ((!divisor) && (CPU[activeCPU].internalinstructionstep==0)) //First step?
	{
		//Timings always!
		++CPU[activeCPU].internalinstructionstep; //Next step after we're done!
		CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
		CPU[activeCPU].executed = 0; //Not executed yet!
		return;
	}
	word quotient, remainder; //Result and modulo!
	byte error, applycycles; //Error/apply cycles!
	CPU8086_internal_DIV(valdiv,divisor,&quotient,&remainder,&error,8,2,6,&applycycles); //Execute the unsigned division! 8-bits result and modulo!
	if (error==0) //No error?
	{
		REG_AL = (byte)(quotient&0xFF); //Quotient!
		REG_AH = (byte)(remainder&0xFF); //Remainder!
	}
	else //Error?
	{
		CPU_exDIV0(); //Exception!
		return; //Exception executed!
	}
	if (applycycles) /* No 80286+ cycles instead? */
	{
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP += 6 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem max!
		}
	}
}

OPTINLINE void op_idiv8(word valdiv, byte divisor) {
	//word v1, v2,
	if ((!divisor) && (CPU[activeCPU].internalinstructionstep==0)) //First step?
	{
		//Timings always!
		++CPU[activeCPU].internalinstructionstep; //Next step after we're done!
		CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
		CPU[activeCPU].executed = 0; //Not executed yet!
		return;
	}
	word quotient, remainder; //Result and modulo!
	byte error, applycycles; //Error/apply cycles!
	uint_32 valdivd;
	word divisorw;
	valdivd = valdiv;
	divisorw = divisor;
	if (valdiv&0x8000) valdivd |= 0xFFFF0000; //Sign extend to 32-bits!
	if (divisor&0x80) divisorw |= 0xFF00; //Sign extend to 16-bits!
	CPU8086_internal_IDIV(valdivd,divisorw,&quotient,&remainder,&error,8,2,6,&applycycles); //Execute the unsigned division! 8-bits result and modulo!
	if (error==0) //No error?
	{
		REG_AL = (quotient&0xFF); //Quotient!
		REG_AH = (remainder&0xFF); //Remainder!
	}
	else //Error?
	{
		CPU_exDIV0(); //Exception!
		return; //Exception executed!
	}
	if (applycycles) /* No 80286+ cycles instead? */
	{
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP += 6 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem max!
		}
	}
	if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
	{
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP += 118 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem max!
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP += 112; //Reg!
		}
	}
}

byte tmps,tmpp; //Sign/parity backup!

extern byte CPU_databussize; //Current data bus size!

byte tempAL;
word tempAX;
uint_32 tempDXAX;

void op_grp3_8() {
	//uint32_t d1, d2, s1, s2, sign;
	//word d, s;
	oper1 = signext(oper1b); oper2 = signext(oper2b);
	switch (thereg) {
	case 0: case 1: //TEST
		CPU8086_internal_TEST8(oper1b,immb,3);
		break;

	case 2: //NOT
		res8 = ~oper1b;
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP += 16 - (EU_CYCLES_SUBSTRACT_ACCESSRW); //Mem!
			}
			else //Register?
			{
				CPU[activeCPU].cycles_OP += 3; //Reg!
			}
		}
		break;

	case 3: //NEG
		res8 = (~oper1b) + 1;
		flag_sub8(0, oper1b);
		if (res8 == 0) FLAGW_CF(0); else FLAGW_CF(1);
		FLAGW_AF((res8&0xF)?1:0); //Auxiliary flag!
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP += 16 - (EU_CYCLES_SUBSTRACT_ACCESSRW); //Mem!
			}
			else //Register?
			{
				CPU[activeCPU].cycles_OP += 3; //Reg!
			}
		}
		break;

	case 4: //MULB
		tempAL = REG_AL; //Save a backup for calculating cycles!
		temp1.val32 = (uint32_t)oper1b * (uint32_t)REG_AL;
		REG_AX = temp1.val16 & 0xFFFF;
		if ((temp1.val16&0xFF00)==0)
		{
			FLAGW_OF(0); //Both zeroed!
		}
		else FLAGW_OF(1); //Set due to overflow!

		FLAGW_CF(FLAG_OF); //Same!
		tempAL = FLAG_ZF; //Backup!
		flag_szp8(REG_AL);
		if (EMULATED_CPU==CPU_8086) //8086 only?
		{
			FLAGW_ZF(tempAL); //Restore Zero flag!
			if (REG_AX) FLAGW_ZF(0); //8086/8088 clears the Zero flag when not zero only. Undocumented bug!
		}
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP += 76 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem max!
			}
			else //Register?
			{
				CPU[activeCPU].cycles_OP += 70; //Reg!
			}
			if (NumberOfSetBits(tempAL)>1) //More than 1 bit set?
			{
				CPU[activeCPU].cycles_OP += NumberOfSetBits(tempAL) - 1; //1 cycle for all bits more than 1 bit set!
			}
		}
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
		FLAGW_SF((temp3.val16&0x80)>>7); //Sign!
		FLAGW_PF(parity[temp3.val16&0xFF]); //Parity flag!
		if (((temp3.val16&0xFF80)==0) || ((temp3.val16&0xFF80)==0xFF80))
		{
			FLAGW_OF(0); //Both zeroed!
		}
		else FLAGW_OF(1); //Set due to overflow!

		FLAGW_CF(FLAG_OF); //Same!
		FLAGW_ZF((temp3.val16==0)?1:0); //Set the zero flag!
		if (EMULATED_CPU==CPU_8086)
		{
			FLAGW_ZF(0); //Clear ZF!
		}
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP += 86 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem max!
			}
			else //Register?
			{
				CPU[activeCPU].cycles_OP += 80; //Reg!
			}
		}
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
	if ((!divisor) && (CPU[activeCPU].internalinstructionstep==0)) //First step?
	{
		//Timings always!
		++CPU[activeCPU].internalinstructionstep; //Next step after we're done!
		CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
		CPU[activeCPU].executed = 0; //Not executed yet!
		return;
	}
	word quotient, remainder; //Result and modulo!
	byte error, applycycles; //Error/apply cycles!
	CPU8086_internal_DIV(valdiv,divisor,&quotient,&remainder,&error,16,2,6,&applycycles); //Execute the unsigned division! 8-bits result and modulo!
	if (error==0) //No error?
	{
		REG_AX = quotient; //Quotient!
		REG_DX = remainder; //Remainder!
	}
	else //Error?
	{
		CPU_exDIV0(); //Exception!
		return; //Exception executed!
	}
	if (applycycles) /* No 80286+ cycles instead? */
	{
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP += 6 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem max!
		}
	}
}

OPTINLINE void op_idiv16(uint32_t valdiv, word divisor) {
	//uint32_t v1, v2,
	if ((!divisor) && (CPU[activeCPU].internalinstructionstep==0)) //First step?
	{
		//Timings always!
		++CPU[activeCPU].internalinstructionstep; //Next step after we're done!
		CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
		CPU[activeCPU].executed = 0; //Not executed yet!
		return;
	}

	word quotient, remainder; //Result and modulo!
	byte error, applycycles; //Error/apply cycles!
	CPU8086_internal_IDIV(valdiv,divisor,&quotient,&remainder,&error,16,2,6,&applycycles); //Execute the unsigned division! 8-bits result and modulo!
	if (error==0) //No error?
	{
		REG_AX = quotient; //Quotient!
		REG_DX = remainder; //Remainder!
	}
	else //Error?
	{
		CPU_exDIV0(); //Exception!
		return; //Exception executed!
	}
	if (applycycles) /* No 80286+ cycles instead? */
	{
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP += 6 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem max!
		}
	}
}

void op_grp3_16() {
	//uint32_t d1, d2, s1, s2, sign;
	//word d, s;
	//oper1 = signext(oper1b); oper2 = signext(oper2b);
	//sprintf(msg, "  Oper1: %04X    Oper2: %04X\n", oper1, oper2); print(msg);
	switch (thereg) {
	case 0: case 1: //TEST
		CPU8086_internal_TEST16(oper1, immw, 3);
		break;
	case 2: //NOT
		res16 = ~oper1;
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP += 16 - (EU_CYCLES_SUBSTRACT_ACCESSRW); //Mem!
			}
			else //Register?
			{
				CPU[activeCPU].cycles_OP += 3; //Reg!
			}
		}
		break;
	case 3: //NEG
		res16 = (~oper1) + 1;
		flag_sub16(0, oper1);
		if (res16) FLAGW_CF(1); else FLAGW_CF(0);
		FLAGW_AF((res16&0xF)?1:0); //Auxiliary flag!
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP += 16 - (EU_CYCLES_SUBSTRACT_ACCESSRW); //Mem!
			}
			else //Register?
			{
				CPU[activeCPU].cycles_OP += 3; //Reg!
			}
		}
		break;
	case 4: //MULW
		tempAX = REG_AX; //Save a backup for calculating cycles!
		temp1.val32 = (uint32_t)oper1 * (uint32_t)REG_AX;
		REG_AX = temp1.val16;
		REG_DX = temp1.val16high;
		if (temp1.val16high==0) FLAGW_OF(0);
		else FLAGW_OF(1);
		FLAGW_CF(FLAG_OF); //OF=CF!

		tempAL = FLAG_ZF; //Backup!
		flag_szp16(REG_AX);
		if (EMULATED_CPU==CPU_8086)
		{
			FLAGW_ZF(tempAL); //Restore!
			if ((EMULATED_CPU==CPU_8086) && temp1.val32) FLAGW_ZF(0); //8086/8088 clears the Zero flag when not zero only.
		}
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP += 124 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem max!
			}
			else //Register?
			{
				CPU[activeCPU].cycles_OP += 118; //Reg!
			}
			if (NumberOfSetBits(tempAX)>1) //More than 1 bit set?
			{
				CPU[activeCPU].cycles_OP += NumberOfSetBits(tempAX) - 1; //1 cycle for all bits more than 1 bit set!
			}
		}
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
		if (((temp3.val32>>15)==0) || ((temp3.val32>>15)==0x1FFFF)) FLAGW_OF(0);
		else FLAGW_OF(1);
		FLAGW_CF(FLAG_OF); //OF=CF!
		FLAGW_SF((temp3.val32&0x80000000)>>31); //Sign!
		FLAGW_PF(parity[temp3.val32&0xFF]); //Parity flag!
		FLAGW_ZF((temp3.val32==0)?1:0); //Set the zero flag!
		if (EMULATED_CPU==CPU_8086)
		{
			FLAGW_ZF(0); //Clear ZF!
		}
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP += 128 - EU_CYCLES_SUBSTRACT_ACCESSREAD; //Mem max!
			}
			else //Register?
			{
				CPU[activeCPU].cycles_OP += 134; //Reg max!
			}
		}
		break;
	case 6: //DIV
		op_div16(((uint32_t)REG_DX << 16) | REG_AX, oper1);
		break;
	case 7: //IDIV
		op_idiv16(((uint32_t)REG_DX << 16) | REG_AX, oper1); break;
	}
}

void op_grp5() {
	MODRM_PTR info; //To contain the info!
	static word destCS;
	static word destIP;
	switch (thereg) {
	case 0: //INC Ev
		if (modrm_check16(&params,1,1)) return; //Abort when needed!
		if (modrm_check16(&params,1,0)) return; //Abort when needed!
		MODRM_src0 = 1; //We're taking this source!
		CPU8086_internal_INC16(modrm_addr16(&params,1,0));
		break;
	case 1: //DEC Ev
		if (modrm_check16(&params,1,1)) return; //Abort when needed!
		if (modrm_check16(&params,1,0)) return; //Abort when needed!
		MODRM_src0 = 1; //We're taking this source!
		CPU8086_internal_DEC16(modrm_addr16(&params,1,0));
		break;
	case 2: //CALL Ev
		if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; }
		if (CPU8086_internal_PUSHw(0,&REG_IP)) return;
		CPU_JMPabs(oper1);
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Mem?
			{
				CPU[activeCPU].cycles_OP += 21 - EU_CYCLES_SUBSTRACT_ACCESSREAD; /* Intrasegment indirect through memory */
			}
			else //Register?
			{
				CPU[activeCPU].cycles_OP += 16; /* Intrasegment indirect through register */
			}
			CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/
		}
		CPU_flushPIQ(-1); //We're jumping to another address!
		break;
	case 3: //CALL Mp
		modrm_decode16(&params, &info, 1); //Get data!

		modrm_addoffset = 0; //First IP!
		if (modrm_check16(&params,1,1)) return; //Abort when needed!
		modrm_addoffset = 2; //Then destination CS!
		if (modrm_check16(&params,1,1)) return; //Abort when needed!

		modrm_addoffset = 0; //First IP!
		if (CPU8086_internal_stepreadmodrmw(0,&destIP,1)) return; //Get destination IP!
		destEIP = (word)destIP; //Convert to EIP!
		CPUPROT1
		modrm_addoffset = 2; //Then destination CS!
		if (CPU8086_internal_stepreadmodrmw(2,&destCS,1)) return; //Get destination CS!
		CPUPROT1
		modrm_addoffset = 0;
		CPU8086_CALLF(destCS,destEIP); //Call the destination address!
		CPUPROT1
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Mem?
			{
				CPU[activeCPU].cycles_OP += 37 - (EU_CYCLES_SUBSTRACT_ACCESSREAD*2); /* Intersegment indirect */
			}
			else //Register?
			{
				CPU[activeCPU].cycles_OP += 28; /* Intersegment direct */
			}
			CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/
		}
		CPUPROT2
		CPUPROT2
		CPUPROT2
		break;
	case 4: //JMP Ev
		CPU_JMPabs(oper1);
		CPU_flushPIQ(-1); //We're jumping to another address!
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP += 18 - EU_CYCLES_SUBSTRACT_ACCESSREAD; /* Intrasegment indirect through memory */
			}
			else //Register?
			{
				CPU[activeCPU].cycles_OP += 11; /* Intrasegment indirect through register */
			}
			CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/
		}
		break;
	case 5: //JMP Mp
		modrm_decode16(&params, &info, 1); //Get data!
		if (checkMMUaccess(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset,1,getCPL(),!CPU_Address_size[activeCPU])) return; //Abort on fault!
		if (checkMMUaccess(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset+1,1,getCPL(),!CPU_Address_size[activeCPU])) return; //Abort on fault!
		if (checkMMUaccess(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset+2,1,getCPL(),!CPU_Address_size[activeCPU])) return; //Abort on fault!
		if (checkMMUaccess(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset+3,1,getCPL(),!CPU_Address_size[activeCPU])) return; //Abort on fault!

		CPUPROT1
		destEIP = oper1; //Convert to EIP!
		modrm_addoffset = 2; //Then destination CS!
		if (CPU8086_internal_stepreadmodrmw(0,&destCS,1)) return; //Get destination CS!
		CPUPROT1
		segmentWritten(CPU_SEGMENT_CS, destCS, 1);
		CPU_flushPIQ(-1); //We're jumping to another address!
		CPUPROT1
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP += 24 - (EU_CYCLES_SUBSTRACT_ACCESSREAD*2); /* Intersegment indirect through memory */
			}
			else //Register?
			{
				CPU[activeCPU].cycles_OP += 11; /* Intersegment indirect through register */
			}
			CPU[activeCPU].cycles_stallBIU += CPU[activeCPU].cycles_OP; /*Stall the BIU completely now!*/
		}
		CPUPROT2
		CPUPROT2
		CPUPROT2
		break;
	case 6: //PUSH Ev
		if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(1,1,0)) return; ++CPU[activeCPU].stackchecked; }
		if (CPU8086_internal_PUSHw(0,&oper1)) return;
		CPUPROT1
		if (CPU_apply286cycles()==0) /* No 80286+ cycles instead? */
		{
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP += 16 - (EU_CYCLES_SUBSTRACT_ACCESSRW); /*Push Mem!*/
			}
			else //Register?
			{
				CPU[activeCPU].cycles_OP += 11 - EU_CYCLES_SUBSTRACT_ACCESSWRITE; /*Push Reg!*/
			}
		}
		CPUPROT2
		break;
	default: //Unknown OPcode?
		CPU_unkOP(); //Execute the unknown opcode exception handler, if any!
		break;
	}
}
