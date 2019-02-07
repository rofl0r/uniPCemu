#define IS_CPU
#include "headers/cpu/cpu.h"
#include "headers/cpu/interrupts.h"
#include "headers/cpu/mmu.h"
#include "headers/support/signedness.h" //CPU support!
#include "headers/cpu/cpu_OP8086.h" //8086 comp.
#include "headers/cpu/cpu_OPNECV30.h" //unkOP comp.
#include "headers/emu/debugger/debugger.h" //Debugger support!
#include "headers/emu/gpu/gpu.h" //Start&StopVideo!
#include "headers/cpu/cb_manager.h" //CB support!
#include "headers/cpu/protection.h"
#include "headers/cpu/cpu_OP80286.h" //0F opcode support!
#include "headers/support/zalloc.h" //For allocating registers etc.
#include "headers/support/locks.h" //Locking support!
#include "headers/cpu/modrm.h" //MODR/M support!
#include "headers/emu/emucore.h" //Needed for CPU reset handler!
#include "headers/mmu/mmuhandler.h" //bufferMMU, MMU_resetaddr and flushMMU support!
#include "headers/cpu/cpu_pmtimings.h" //80286+ timings lookup table support!
#include "headers/cpu/easyregs.h" //Easy register support!
#include "headers/cpu/protecteddebugging.h" //Protected debugging support!
#include "headers/cpu/biu.h" //BIU support!
#include "headers/cpu/cpu_execution.h" //Execution support!
#include "headers/support/log.h" //Logging support!
#include "headers/cpu/flags.h" //Flag support for IMUL!

//Waitstate delay on 80286.
#define CPU286_WAITSTATE_DELAY 1

//Enable this define to use cycle-accurate emulation for supported CPUs!
#define CPU_USECYCLES

//Save the last instruction address and opcode in a backup?
#define CPU_SAVELAST

byte activeCPU = 0; //What CPU is currently active?

byte cpudebugger; //To debug the CPU?

CPU_type CPU[MAXCPUS]; //The CPU data itself!

//CPU timings information
extern CPU_Timings CPUTimings[CPU_MODES][0x200]; //All normal and 0F CPU timings, which are used, for all modes available!

//ModR/M information!
MODRM_PARAMS params; //For getting all params for the CPU exection ModR/M data!
byte MODRM_src0 = 0; //What source is our modr/m? (1/2)
byte MODRM_src1 = 0; //What source is our modr/m? (1/2)

//Immediate data read for execution!
byte immb; //For CPU_readOP result!
word immw; //For CPU_readOPw result!
uint_32 imm32; //For CPU_readOPdw result!
uint_64 imm64; //For CPU_readOPdw x2 result!
uint_32 immaddr32; //Immediate address, for instructions requiring it, either 16-bits or 32-bits of immediate data, depending on the address size!

//Opcode&Stack sizes: 0=16-bits, 1=32-bits!
byte CPU_Operand_size[2] = { 0 , 0 }; //Operand size for this opcode!
byte CPU_Address_size[2] = { 0 , 0 }; //Address size for this opcode!

//Internal prefix table for below functions!
byte CPU_prefixes[2][32]; //All prefixes, packed in a bitfield!

//More info about interrupts: http://www.bioscentral.com/misc/interrupts.htm#
//More info about interrupts: http://www.bioscentral.com/misc/interrupts.htm#

#ifdef CPU_USECYCLES
byte CPU_useCycles = 0; //Enable normal cycles for supported CPUs when uncommented?
#endif

word timing286lookup[4][2][2][0x100][8][8]; //4 modes(bit0=protected mode when set, bit1=32-bit instruction when set), 2 memory modes, 2 0F possibilities, 256 instructions, 9 modr/m variants, no more than 8 possibilities for every instruction. About 73K memory consumed(unaligned).

/*

checkSignedOverflow: Checks if a signed overflow occurs trying to store the data.
unsignedval: The unsigned, positive value
calculatedbits: The amount of bits that's stored in unsignedval.
bits: The amount of bits to store in.
convertedtopositive: The unsignedval is a positive conversion from a negative result, so needs to be converted back.

*/

//Based on http://www.ragestorm.net/blogs/?p=34

byte checkSignedOverflow(uint_64 unsignedval, byte calculatedbits, byte bits, byte convertedtopositive)
{
	uint_64 maxpositive,maxnegative;
	maxpositive = ((1ULL<<(bits-1))-1); //Maximum positive value we can have!
	maxnegative = (1ULL<<(bits-1)); //The highest value we cannot set and get past when negative!
	if (unlikely(((unsignedval>maxpositive) && (convertedtopositive==0)) || ((unsignedval>maxnegative) && (convertedtopositive)))) //Signed underflow/overflow on unsinged conversion?
	{
		return 1; //Underflow/overflow detected!
	}
	return 0; //OK!
}

extern VAL64Splitter temp1, temp2, temp3, temp4, temp5; //All temporary values!

uint_64 signextend64(uint_64 val, byte bits)
{
	INLINEREGISTER uint_64 highestbit,bitmask;
	bitmask = highestbit = (1ULL<<(bits-1)); //Sign bit to use!
	bitmask <<= 1; //Shift to bits!
	--bitmask; //Mask for the used bits!
	if (likely(val&highestbit)) //Sign to extend?
	{
		val |= (~bitmask); //Sign extend!
		return val; //Give the result!
	}
	val &= bitmask; //Mask high bits off!
	return val; //Give the result!
}

//x86 IMUL for opcodes 69h/6Bh.
uint_32 IMULresult; //Buffer to use, general purpose!
void CPU_CIMUL(uint_32 base, byte basesize, uint_32 multiplicant, byte multiplicantsize, uint_32 *result, byte resultsize)
{
	temp1.val64 = signextend64(base,basesize); //Read reg instead! Word register = Word register * imm16!
	temp2.val64 = signextend64(multiplicant,multiplicantsize); //Immediate word is second/third parameter!
	temp3.val64s = temp1.val64s; //Load and...
	temp3.val64s *= temp2.val64s; //Signed multiplication!
	temp2.val64 = signextend64(temp3.val64,resultsize); //For checking for overflow and giving the correct result!
	switch (resultsize) //What result size?
	{
	default:
	case 8: flag_log8((byte)temp2.val64); break;
	case 16: flag_log16((word)temp2.val64); break;
	case 32: flag_log32((uint_32)temp2.val64); break;
	}
	if (temp3.val64s==temp2.val64s) FLAGW_OF(0); //Overflow flag is cleared when high word is a sign extension of the low word(values are equal)!
	else FLAGW_OF(1);
	FLAGW_CF(FLAG_OF); //OF=CF!
	/*
	FLAGW_SF((temp3.val64&0x8000000000000000ULL)>>63); //Sign!
	FLAGW_PF(parity[temp3.val16&0xFF]); //Parity flag!
	FLAGW_ZF((temp3.val64==0)?1:0); //Set the zero flag!	
	*/
	*result = (uint_32)temp2.val64; //Save the result, truncated to used size as 64-bit sign extension!
}

uint_32 getstackaddrsizelimiter()
{
	return STACK_SEGMENT_DESCRIPTOR_B_BIT()? 0xFFFFFFFF : 0xFFFF; //Stack address size!
}

byte checkStackAccess(uint_32 poptimes, word isPUSH, byte isdword) //How much do we need to POP from the stack?
{
	uint_32 poptimesleft = poptimes; //Load the amount to check!
	uint_32 ESP = CPU[activeCPU].registers->ESP; //Load the stack pointer to verify!
	for (;poptimesleft;) //Anything left?
	{
		if (isPUSH)
		{
			ESP += stack_pushchange(isdword); //Apply the change in virtual (E)SP to check the next value!
		}

		//We're at least a word access!
		if (isdword)
		{
			if (checkMMUaccess32(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, ESP&getstackaddrsizelimiter(), ((isPUSH ? 0 : 1) | 0x40)|(isPUSH&0x300), getCPL(), !STACK_SEGMENT_DESCRIPTOR_B_BIT(), 0 | (8 << isdword))) //Error accessing memory?
			{
				return 1; //Abort on fault!
			}
		}
		else //Word?
		{
			if (checkMMUaccess16(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (ESP&getstackaddrsizelimiter()), ((isPUSH ? 0 : 1) | 0x40)|(isPUSH&0x300), getCPL(), !STACK_SEGMENT_DESCRIPTOR_B_BIT(), 0 | (8 << isdword))) //Error accessing memory?
			{
				return 1; //Abort on fault!
			}
		}
		if (isPUSH==0)
		{
			ESP += stack_popchange(isdword); //Apply the change in virtual (E)SP to check the next value!
		}
		--poptimesleft; //One POP processed!
	}
	poptimesleft = poptimes; //Load the amount to check!
	ESP = CPU[activeCPU].registers->ESP; //Load the stack pointer to verify!
	for (; poptimesleft;) //Anything left?
	{
		if (isPUSH)
		{
			ESP += stack_pushchange(isdword); //Apply the change in virtual (E)SP to check the next value!
		}

		//We're at least a word access!
		if (isdword) //Dword
		{
			if (checkMMUaccess32(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, ESP&getstackaddrsizelimiter(), (isPUSH ? 0 : 1) | 0xA0, getCPL(), !STACK_SEGMENT_DESCRIPTOR_B_BIT(), 0 | (8 << isdword))) //Error accessing memory?
			{
				return 1; //Abort on fault!
			}
		}
		else //Word
		{
			if (checkMMUaccess16(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, ESP&getstackaddrsizelimiter(), (isPUSH ? 0 : 1) | 0xA0, getCPL(), !STACK_SEGMENT_DESCRIPTOR_B_BIT(), 0 | (8 << isdword))) //Error accessing memory?
			{
				return 1; //Abort on fault!
			}
		}
		if (isPUSH == 0)
		{
			ESP += stack_popchange(isdword); //Apply the change in virtual (E)SP to check the next value!
		}
		--poptimesleft; //One POP processed!
	}
	return 0; //OK!
}

byte checkENTERStackAccess(uint_32 poptimes, byte isdword) //How much do we need to POP from the stack(using (E)BP)?
{
	uint_32 poptimesleft = poptimes; //Load the amount to check!
	uint_32 EBP = CPU[activeCPU].registers->EBP; //Load the stack pointer to verify!
	for (;poptimesleft;) //Anything left?
	{
		EBP -= stack_popchange(isdword); //Apply the change in virtual (E)BP to check the next value(decrease in EBP)!
		
		//We're at least a word access!
		if (isdword) //DWord?
		{
			if (checkMMUaccess32(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (EBP&getstackaddrsizelimiter()),1|0x40,getCPL(),!STACK_SEGMENT_DESCRIPTOR_B_BIT(),0|(8<<isdword))) //Error accessing memory?
			{
				return 1; //Abort on fault!
			}
		}
		else //Word?
		{
			if (checkMMUaccess16(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, EBP&getstackaddrsizelimiter(), 1 | 0x40, getCPL(), !STACK_SEGMENT_DESCRIPTOR_B_BIT(), 0 | (8 << isdword))) //Error accessing memory?
			{
				return 1; //Abort on fault!
			}
		}
		--poptimesleft; //One POP processed!
	}
	poptimesleft = poptimes; //Load the amount to check!
	EBP = CPU[activeCPU].registers->EBP; //Load the stack pointer to verify!
	for (; poptimesleft;) //Anything left?
	{
		EBP -= stack_popchange(isdword); //Apply the change in virtual (E)BP to check the next value(decrease in EBP)!

		//We're at least a word access!
		if (isdword) //DWord?
		{
			if (checkMMUaccess32(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (EBP&getstackaddrsizelimiter()), 1|0xA0, getCPL(), !STACK_SEGMENT_DESCRIPTOR_B_BIT(), 0 | (8 << isdword))) //Error accessing memory?
			{
				return 1; //Abort on fault!
			}
		}
		else //Word?
		{
			if (checkMMUaccess16(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, EBP&getstackaddrsizelimiter(), 1 | 0xA0, getCPL(), !STACK_SEGMENT_DESCRIPTOR_B_BIT(), 0 | (8 << isdword))) //Error accessing memory?
			{
				return 1; //Abort on fault!
			}
		}
		--poptimesleft; //One POP processed!
	}
	return 0; //OK!
}

//Now the code!

byte calledinterruptnumber = 0; //Called interrupt number for unkint funcs!

void CPU_JMPrel(int_32 reladdr)
{
	REG_EIP += reladdr; //Apply to EIP!
	REG_EIP &= CPU_EIPmask(); //Only 16-bits when required!
	if (CPU_MMU_checkrights(CPU_SEGMENT_CS,CPU[activeCPU].registers->CS,REG_EIP,3,&CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS],2,CPU_Operand_size[activeCPU])) //Limit broken or protection fault?
	{
		THROWDESCGP(0,0,0); //#GP(0) when out of limit range!
	}
}

void CPU_JMPabs(uint_32 addr)
{
	REG_EIP = addr; //Apply to EIP!
	REG_EIP &= CPU_EIPmask(); //Only 16-bits when required!
	if (CPU_MMU_checkrights(CPU_SEGMENT_CS,CPU[activeCPU].registers->CS,REG_EIP,3,&CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS],2,CPU_Operand_size[activeCPU])) //Limit broken or protection fault?
	{
		THROWDESCGP(0,0,0); //#GP(0) when out of limit range!
	}
}

uint_32 CPU_EIPmask()
{
	if (CPU_Operand_size[activeCPU]==0) //16-bit movement?
	{
		return 0xFFFF; //16-bit mask!
	}
	return 0xFFFFFFFF; //Full mask!
}

byte CPU_EIPSize()
{
	return ((CPU_EIPmask()==0xFFFF) && (debugger_forceEIP()==0))?PARAM_IMM16:PARAM_IMM32; //Full mask or when forcing EIP to be used!
}


char modrm_param1[256]; //Contains param/reg1
char modrm_param2[256]; //Contains param/reg2

void modrm_debugger8(MODRM_PARAMS *theparams, byte whichregister1, byte whichregister2) //8-bit handler!
{
	if (cpudebugger)
	{
		cleardata(&modrm_param1[0],sizeof(modrm_param1));
		cleardata(&modrm_param2[0],sizeof(modrm_param2));
		modrm_text8(theparams,whichregister1,&modrm_param1[0]);
		modrm_text8(theparams,whichregister2,&modrm_param2[0]);
	}
}

void modrm_debugger16(MODRM_PARAMS *theparams, byte whichregister1, byte whichregister2) //16-bit handler!
{
	if (cpudebugger)
	{
		cleardata(&modrm_param1[0],sizeof(modrm_param1));
		cleardata(&modrm_param2[0],sizeof(modrm_param2));
		modrm_text16(theparams,whichregister1,&modrm_param1[0]);
		modrm_text16(theparams,whichregister2,&modrm_param2[0]);
	}
}

void modrm_debugger32(MODRM_PARAMS *theparams, byte whichregister1, byte whichregister2) //16-bit handler!
{
	if (cpudebugger)
	{
		cleardata(&modrm_param1[0],sizeof(modrm_param1));
		cleardata(&modrm_param2[0],sizeof(modrm_param2));
		modrm_text32(theparams,whichregister1,&modrm_param1[0]);
		modrm_text32(theparams,whichregister2,&modrm_param2[0]);
	}
}

byte NumberOfSetBits(uint_32 i)
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

extern byte debugger_set; //Debugger set?

void modrm_generateInstructionTEXT(char *instruction, byte debuggersize, uint_32 paramdata, byte type)
{
	if (cpudebugger && (debugger_set==0)) //Gotten no debugger to process?
	{
		//Process debugger!
		char result[256];
		cleardata(&result[0],sizeof(result));
		safestrcpy(result,sizeof(result),instruction); //Set the instruction!
		switch (type)
		{
			case PARAM_MODRM1: //Param1 only?
			case PARAM_MODRM2: //Param2 only?
			case PARAM_MODRM12: //param1,param2
			case PARAM_MODRM12_IMM8: //param1,param2,imm8
			case PARAM_MODRM12_CL: //param1,param2,CL
			case PARAM_MODRM21: //param2,param1
			case PARAM_MODRM21_IMM8: //param2,param1,imm8
			case PARAM_MODRM21_CL: //param2,param1,CL
				//We use modr/m decoding!
				switch (debuggersize)
				{
					case 8:
						modrm_debugger8(&params,0,1);
						break;
					case 16:
						modrm_debugger16(&params,0,1);
						break;
					case 32:
						modrm_debugger32(&params,0,1);
						break;
					default: //None?
						//Don't use modr/m!
						break;
				}
				break;
			//Standards based on the modr/m in the information table.
			case PARAM_MODRM_0: //Param1 only?
			case PARAM_MODRM_1: //Param2 only?
			case PARAM_MODRM_01: //param1,param2
			case PARAM_MODRM_01_IMM8: //param1,param2,imm8
			case PARAM_MODRM_01_CL: //param1,param2,CL
			case PARAM_MODRM_10: //param2,param1
			case PARAM_MODRM_10_IMM8: //param2,param1,imm8
			case PARAM_MODRM_10_CL: //param2,param1,CL
				switch (debuggersize)
				{
					case 8:
						modrm_debugger8(&params,MODRM_src0,MODRM_src1);
						break;
					case 16:
						modrm_debugger16(&params,MODRM_src0,MODRM_src1);
						break;
					case 32:
						modrm_debugger32(&params,MODRM_src0,MODRM_src1);
						break;
					default: //None?
						//Don't use modr/m!
						break;
				}
				break;
			default:
				break;
		}
		switch (type)
		{
			case PARAM_NONE: //No params?
				debugger_setcommand(result); //Nothing!
				break;
			case PARAM_MODRM_0:
			case PARAM_MODRM1: //Param1 only?
				safestrcat(result,sizeof(result)," %s"); //1 param!
				debugger_setcommand(result,modrm_param1);
				break;
			case PARAM_MODRM_1:
			case PARAM_MODRM2: //Param2 only?
				safestrcat(result,sizeof(result)," %s"); //1 param!
				debugger_setcommand(result,modrm_param2);
				break;
			case PARAM_MODRM_01:
			case PARAM_MODRM12: //param1,param2
				safestrcat(result,sizeof(result)," %s,%s"); //2 params!
				debugger_setcommand(result,modrm_param1,modrm_param2);
				break;
			case PARAM_MODRM_01_IMM8:
			case PARAM_MODRM12_IMM8: //param1,param2,imm8
				safestrcat(result,sizeof(result)," %s,%s,%02X"); //2 params!
				debugger_setcommand(result,modrm_param1,modrm_param2,paramdata);
				break;
			case PARAM_MODRM_01_CL:
			case PARAM_MODRM12_CL: //param1,param2,CL
				safestrcat(result,sizeof(result)," %s,%s,CL"); //2 params!
				debugger_setcommand(result,modrm_param1,modrm_param2);
				break;
			case PARAM_MODRM_10:
			case PARAM_MODRM21: //param2,param1
				safestrcat(result,sizeof(result)," %s,%s"); //2 params!
				debugger_setcommand(result,modrm_param2,modrm_param1);
				break;
			case PARAM_MODRM_10_IMM8:
			case PARAM_MODRM21_IMM8: //param2,param1,imm8
				safestrcat(result,sizeof(result)," %s,%s,%02X"); //2 params!
				debugger_setcommand(result,modrm_param2,modrm_param1,paramdata);
				break;
			case PARAM_MODRM_10_CL:
			case PARAM_MODRM21_CL: //param2,param1,CL
				safestrcat(result,sizeof(result)," %s,%s,CL"); //2 params!
				debugger_setcommand(result,modrm_param2,modrm_param1);
				break;
			case PARAM_IMM8: //imm8
				safestrcat(result,sizeof(result)," %02X"); //1 param!
				debugger_setcommand(result,paramdata);
				break;
			case PARAM_IMM8_PARAM: //imm8
				safestrcat(result,sizeof(result),"%02X"); //1 param!
				debugger_setcommand(result,paramdata);
				break;
			case PARAM_IMM16: //imm16
				safestrcat(result,sizeof(result)," %04X"); //1 param!
				debugger_setcommand(result,paramdata);
				break;
			case PARAM_IMM16_PARAM: //imm16
				safestrcat(result,sizeof(result),"%04X"); //1 param!
				debugger_setcommand(result,paramdata);
				break;
			case PARAM_IMM32: //imm32
				safestrcat(result,sizeof(result)," %08X"); //1 param!
				debugger_setcommand(result,paramdata);
				break;
			case PARAM_IMM32_PARAM: //imm32
				safestrcat(result,sizeof(result),"%08X"); //1 param!
				debugger_setcommand(result,paramdata);
				break;
			default: //Unknown?
				break;
		}
	}
}

//PORT IN/OUT instructions!
byte portrights_error = 0;
byte CPU_PORT_OUT_B(word base, word port, byte data)
{
	//Check rights!
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if ((portrights_error = checkPortRights(port))) //Not allowed?
		{
			if (portrights_error==1) THROWDESCGP(0,0,0); //#GP!
			return 1; //Abort!
		}
	}
	//Execute it!
	byte dummy;
	if (CPU[activeCPU].internalinstructionstep==base) //First step? Request!
	{
		if (BIU_request_BUSwb(port,data)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		BIU_handleRequests(); //Handle all pending requests at once when to be processed!
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==(base+1))
	{
		if (BIU_readResultb(&dummy)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU_PORT_OUT_W(word base, word port, word data)
{
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if ((portrights_error = checkPortRights(port))) //Not allowed?
		{
			if (portrights_error==1) THROWDESCGP(0,0,0); //#GP!
			return 1; //Abort!
		}
		if ((portrights_error = checkPortRights(port+1))) //Not allowed?
		{
			if (portrights_error==1) THROWDESCGP(0,0,0); //#GP!
			return 1; //Abort!
		}
	}
	//Execute it!
	word dummy;
	if (CPU[activeCPU].internalinstructionstep==base) //First step? Request!
	{
		if (BIU_request_BUSww(port,data)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		BIU_handleRequests(); //Handle all pending requests at once when to be processed!
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==(base+1))
	{
		if (BIU_readResultw(&dummy)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU_PORT_OUT_D(word base, word port, uint_32 data)
{
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if ((portrights_error = checkPortRights(port))) //Not allowed?
		{
			if (portrights_error==1) THROWDESCGP(0,0,0); //#GP!
			return 1; //Abort!
		}
		if ((portrights_error = checkPortRights(port + 1))) //Not allowed?
		{
			if (portrights_error==1) THROWDESCGP(0,0,0); //#GP!
			return 1; //Abort!
		}
		if ((portrights_error = checkPortRights(port + 2))) //Not allowed?
		{
			if (portrights_error==1) THROWDESCGP(0,0,0); //#GP!
			return 1; //Abort!
		}
		if ((portrights_error = checkPortRights(port + 3))) //Not allowed?
		{
			if (portrights_error==1) THROWDESCGP(0,0,0); //#GP!
			return 1; //Abort!
		}
	}
	//Execute it!
	uint_32 dummy;
	if (CPU[activeCPU].internalinstructionstep==base) //First step? Request!
	{
		if (BIU_request_BUSwdw(port,data)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		BIU_handleRequests(); //Handle all pending requests at once when to be processed!
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==(base+1))
	{
		if (BIU_readResultdw(&dummy)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU_PORT_IN_B(word base, word port, byte *result)
{
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if ((portrights_error = checkPortRights(port))) //Not allowed?
		{
			if (portrights_error==1) THROWDESCGP(0,0,0); //#GP!
			return 1; //Abort!
		}
	}
	//Execute it!
	if (CPU[activeCPU].internalinstructionstep==base) //First step? Request!
	{
		if (BIU_request_BUSrb(port)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		BIU_handleRequests(); //Handle all pending requests at once when to be processed!
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==(base+1))
	{
		if (BIU_readResultb(result)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte CPU_PORT_IN_W(word base, word port, word *result)
{
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if ((portrights_error = checkPortRights(port))) //Not allowed?
		{
			if (portrights_error==1) THROWDESCGP(0,0,0); //#GP!
			return 1; //Abort!
		}
		if ((portrights_error = checkPortRights(port + 1))) //Not allowed?
		{
			if (portrights_error==1) THROWDESCGP(0,0,0); //#GP!
			return 1; //Abort!
		}
	}
	//Execute it!
	if (CPU[activeCPU].internalinstructionstep==base) //First step? Request!
	{
		if (BIU_request_BUSrw(port)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		BIU_handleRequests(); //Handle all pending requests at once when to be processed!
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

byte CPU_PORT_IN_D(word base, word port, uint_32 *result)
{
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if ((portrights_error = checkPortRights(port))) //Not allowed?
		{
			if (portrights_error==1) THROWDESCGP(0,0,0); //#GP!
			return 1; //Abort!
		}
		if ((portrights_error = checkPortRights(port + 1))) //Not allowed?
		{
			if (portrights_error==1) THROWDESCGP(0,0,0); //#GP!
			return 1; //Abort!
		}
		if ((portrights_error = checkPortRights(port + 2))) //Not allowed?
		{
			if (portrights_error==1) THROWDESCGP(0,0,0); //#GP!
			return 1; //Abort!
		}
		if ((portrights_error = checkPortRights(port + 3))) //Not allowed?
		{
			if (portrights_error==1) THROWDESCGP(0,0,0); //#GP!
			return 1; //Abort!
		}
	}
	//Execute it!
	if (CPU[activeCPU].internalinstructionstep==base) //First step? Request!
	{
		if (BIU_request_BUSrdw(port)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		BIU_handleRequests(); //Handle all pending requests at once when to be processed!
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==(base+1))
	{
		if (BIU_readResultdw(result)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	return 0; //Ready to process further! We're loaded!
}

byte call_soft_inthandler(byte intnr, int_64 errorcode, byte is_interrupt)
{
	//Now call handler!
	//CPU[activeCPU].cycles_HWOP += 61; /* Normal interrupt as hardware interrupt */
	calledinterruptnumber = intnr; //Save called interrupt number!
	return CPU_INT(intnr,errorcode,is_interrupt); //Call interrupt!
}

void call_hard_inthandler(byte intnr) //Hardware interrupt handler (FROM hardware only, or int>=0x20 for software call)!
{
//Now call handler!
	//CPU[activeCPU].cycles_HWOP += 61; /* Normal interrupt as hardware interrupt */
	calledinterruptnumber = intnr; //Save called interrupt number!
	CPU_executionphase_startinterrupt(intnr,2,-1); //Start the interrupt handler!
}

void CPU_8086_RETI() //Not from CPU!
{
	CPU_IRET(); //RETURN FROM INTERRUPT!
}

extern byte reset; //Reset?

void CPU_ErrorCallback_RESET() //Error callback with error code!
{
	debugrow("Resetting emulator: Error callback called!");
	reset = 1; //Reset the emulator!
}

void copyint(byte src, byte dest) //Copy interrupt handler pointer to different interrupt!
{
	MMU_ww(-1,0x0000,(dest<<2),MMU_rw(-1,0x0000,(src<<2),0,0),0); //Copy segment!
	MMU_ww(-1,0x0000,(dest<<2)|2,MMU_rw(-1,0x0000,((src<<2)|2),0,0),0); //Copy offset!
}

extern byte CPU_databussize; //0=16/32-bit bus! 1=8-bit bus when possible (8088/80188) or 16-bit when possible(286+)!

OPTINLINE void CPU_resetPrefixes() //Resets all prefixes we use!
{
	memset(&CPU_prefixes[activeCPU], 0, sizeof(CPU_prefixes[activeCPU])); //Reset prefixes!
}

OPTINLINE void CPU_initPrefixes()
{
	CPU_resetPrefixes(); //This is the same: just reset all prefixes to zero!
}

OPTINLINE void alloc_CPUregisters()
{
	CPU[activeCPU].registers = (CPU_registers *)zalloc(sizeof(*CPU[activeCPU].registers), "CPU_REGISTERS", getLock(LOCK_CPU)); //Allocate the registers!
	if (!CPU[activeCPU].registers)
	{
		raiseError("CPU", "Failed to allocate the required registers!");
	}
}

uint_32 oldCR0=0;
OPTINLINE void free_CPUregisters()
{
	if (CPU[activeCPU].registers) //Still allocated?
	{
		oldCR0 = CPU[0].registers->CR0; //Save the old value for INIT purposes!
		freez((void **)&CPU[activeCPU].registers, sizeof(*CPU[activeCPU].registers), "CPU_REGISTERS"); //Release the registers if needed!
	}
}

OPTINLINE void CPU_initRegisters(byte isInit) //Init the registers!
{
	uint_32 CSBase; //Base of CS!
	byte CSAccessRights; //Default CS access rights, overwritten during first software reset!
	if (CPU[activeCPU].registers) //Already allocated?
	{
		CSAccessRights = CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].desc.AccessRights; //Save old CS acccess rights to use now (after first reset)!
		free_CPUregisters(); //Free the CPU registers!
	}
	else
	{
		CSAccessRights = 0x93; //Initialise the CS access rights!
	}

	alloc_CPUregisters(); //Allocate the CPU registers!

	if (!CPU[activeCPU].registers) return; //We can't work!
	
	//General purpose registers
	CPU[activeCPU].registers->EAX = 0;
	CPU[activeCPU].registers->EBX = 0;
	CPU[activeCPU].registers->ECX = 0;
	CPU[activeCPU].registers->EDX = 0;

	if (EMULATED_CPU>=CPU_80386) //Need revision info in DX?
	{
		switch (EMULATED_CPU)
		{
		default:
		case CPU_80386:
			CPU[activeCPU].registers->DX = CPU_databussize ? 0x2303 : 0x0303;
			break;
		case CPU_80486:
			CPU[activeCPU].registers->DX = 0x0421; //80486SX! DX not supported yet!
			break;
		case CPU_PENTIUM:
			CPU[activeCPU].registers->DX = 0x0421; //80486SX! DX not supported yet!
			break;
		}
	}

	//Index registers
	CPU[activeCPU].registers->EBP = 0; //Init offset of BP?
	CPU[activeCPU].registers->ESI = 0; //Source index!
	CPU[activeCPU].registers->EDI = 0; //Destination index!

	//Stack registers
	CPU[activeCPU].registers->ESP = 0; //Init offset of stack (top-1)
	CPU[activeCPU].registers->SS = 0; //Stack segment!


	//Code location
	if (EMULATED_CPU >= CPU_NECV30) //186+?
	{
		CPU[activeCPU].registers->CS = 0xF000; //We're this selector!
		CPU[activeCPU].registers->EIP = 0xFFF0; //We're starting at this offset!
	}
	else //8086?
	{
		CPU[activeCPU].registers->CS = 0xFFFF; //Code segment: default to segment 0xFFFF to start at 0xFFFF0 (bios boot jump)!
		CPU[activeCPU].registers->EIP = 0; //Start of executable code!
	}
	
	//Data registers!
	CPU[activeCPU].registers->DS = 0; //Data segment!
	CPU[activeCPU].registers->ES = 0; //Extra segment!
	CPU[activeCPU].registers->FS = 0; //Far segment (extra segment)
	CPU[activeCPU].registers->GS = 0; //??? segment (extra segment like FS)
	CPU[activeCPU].registers->EFLAGS = 0x2; //Flags!

	//Now the handling of solid state segments (might change, use index for that!)
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_CS] = &CPU[activeCPU].registers->CS; //Link!
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_SS] = &CPU[activeCPU].registers->SS; //Link!
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_DS] = &CPU[activeCPU].registers->DS; //Link!
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_ES] = &CPU[activeCPU].registers->ES; //Link!
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_FS] = &CPU[activeCPU].registers->FS; //Link!
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_GS] = &CPU[activeCPU].registers->GS; //Link!
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_TR] = &CPU[activeCPU].registers->TR; //Link!
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_LDTR] = &CPU[activeCPU].registers->LDTR; //Link!

	memset(&CPU[activeCPU].SEG_DESCRIPTOR, 0, sizeof(CPU[activeCPU].SEG_DESCRIPTOR)); //Clear the descriptor cache!
	 //Now, load the default descriptors!

	//IDTR
	CPU[activeCPU].registers->IDTR.base = 0;
	CPU[activeCPU].registers->IDTR.limit = 0x3FF;

	//GDTR
	CPU[activeCPU].registers->GDTR.base = 0;
	CPU[activeCPU].registers->GDTR.limit = 0xFFFF; //From bochs!

	//LDTR (invalid)
	CPU[activeCPU].registers->LDTR = 0; //No LDTR (also invalid)!

	//TR (invalid)
	CPU[activeCPU].registers->TR = 0; //No TR (also invalid)!

	if (EMULATED_CPU == CPU_80286) //80286 CPU?
	{
		CPU[activeCPU].registers->CR0 = 0; //Clear bit 32 and 4-0, also the MSW!
		CPU[activeCPU].registers->CR0 |= 0xFFF0; //The MSW is initialized to FFF0!
	}
	else //Default or 80386?
	{
		if (isInit==0) //Were we not an init?
		{
			CPU[activeCPU].registers->CR0 = oldCR0; //Restore before resetting, if possible!
		}
		else
		{
			CPU[activeCPU].registers->CR0 = 0x60000010; //Restore before resetting, if possible! Apply init defaults!
		}
		CPU[activeCPU].registers->CR0 &= 0x60000000; //The MSW is initialized to 0000! High parts are reset as well!
		if (EMULATED_CPU >= CPU_80486) //80486+?
		{
			CPU[activeCPU].registers->CR0 |= 0x0010; //Only set the defined bits! Bits 30/29 remain unmodified, according to http://www.sandpile.org/x86/initial.htm
		}
		else //80386?
		{
			CPU[activeCPU].registers->CR0 = 0; //We don't have the 80486+ register bits, so reset them!
		}
	}

	byte reg = 0;
	for (reg = 0; reg<NUMITEMS(CPU[activeCPU].SEG_DESCRIPTOR); reg++) //Process all segment registers!
	{
		//Load Real mode compatible values for all registers!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].desc.base_high = 0;
		CPU[activeCPU].SEG_DESCRIPTOR[reg].desc.base_mid = 0;
		CPU[activeCPU].SEG_DESCRIPTOR[reg].desc.base_low = 0;
		CPU[activeCPU].SEG_DESCRIPTOR[reg].desc.limit_low = 0xFFFF; //64k limit!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].desc.noncallgate_info = 0; //No high limit etc.!
		//According to http://www.sandpile.org/x86/initial.htm the following access rights are used:
		if ((reg == CPU_SEGMENT_LDTR) || (reg == CPU_SEGMENT_TR)) //LDTR&TR=Special case! Apply special access rights!
		{
			CPU[activeCPU].SEG_DESCRIPTOR[reg].desc.AccessRights = 0x82; //Invalid segment!
		}
		else //Normal Code/Data segment?
		{
			CPU[activeCPU].SEG_DESCRIPTOR[reg].desc.AccessRights = 0x93; //Code/data segment, writable!
		}
	}

	//CS specific!
	CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].desc.AccessRights = CSAccessRights; //Load CS default access rights!
	if (EMULATED_CPU>CPU_NECV30) //286+?
	{
		//Pulled low on first load, pulled high on reset:
		if (EMULATED_CPU>CPU_80286) //32-bit CPU?
		{
			CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].desc.base_high = 0xFF; //More than 24 bits are pulled high as well!
		}
		CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].desc.base_mid = 0xFF; //We're starting at the end of our address space, final block! (segment F000=>high 8 bits set)
	}
	else //186-?
	{
		CSBase = CPU[activeCPU].registers->CS<<4; //CS base itself!
		CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].desc.base_mid = (CSBase>>16); //Mid range!
		CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].desc.base_low = (CSBase&0xFFFF); //Low range!
	}

	for (reg = 0; reg<NUMITEMS(CPU[activeCPU].SEG_DESCRIPTOR); reg++) //Process all segment registers!
	{
		CPU_calcSegmentPrecalcs(&CPU[activeCPU].SEG_DESCRIPTOR[reg]); //Calculate the precalcs for the segment descriptor!
	}

	CPU_flushPIQ(-1); //We're jumping to another address!
}

void CPU_initLookupTables(); //Initialize the CPU timing lookup tables! Prototype!
extern byte is_XT; //Are we an XT?

uint_32 effectivecpuaddresspins = 0xFFFFFFFF;
uint_32 cpuaddresspins[12] = { //Bit0=XT, Bit1+=CPU
							0xFFFFF, //8086 AT+
							0xFFFFF, //8086 XT
							0xFFFFF, //80186 AT+
							0xFFFFF, //80186 XT
							0xFFFFFF, //80286 AT+
							0xFFFFFF, //80286 XT
							0xFFFFFFFF, //80386 AT+
							0xFFFFFFFF, //80386 XT
							0xFFFFFFFF, //80486 AT+
							0xFFFFFFFF, //80486 XT
							0xFFFFFFFF, //80586 AT+
							0xFFFFFFFF, //80586 XT
							}; //CPU address wrapping lookup table!

void resetCPU(byte isInit) //Initialises the currently selected CPU!
{
	byte i;
	for (i = 0;i < NUMITEMS(CPU);++i) //Process all CPUs!
	{
		CPU[i].allowInterrupts = 1; //Default to allowing all interrupts to run!
	}
	CPU_initRegisters(isInit); //Initialise the registers!
	CPU_initPrefixes(); //Initialise all prefixes!
	CPU_resetMode(); //Reset the mode to the default mode!
	//Default: not waiting for interrupt to occur on startup!
	//Not waiting for TEST pin to occur!
	//Default: not blocked!
	//Continue interrupt call (hardware)?
	CPU[activeCPU].running = 1; //We're running!
	
	CPU[activeCPU].lastopcode = CPU[activeCPU].lastopcode0F = CPU[activeCPU].lastmodrm = CPU[activeCPU].previousopcode = CPU[activeCPU].previousopcode0F = CPU[activeCPU].previousmodrm = 0; //Last opcode, default to 0 and unknown?
	generate_opcode_jmptbl(); //Generate the opcode jmptbl for the current CPU!
	generate_opcode0F_jmptbl(); //Generate the opcode 0F jmptbl for the current CPU!
	generate_timings_tbl(); //Generate the timings tables for all CPU's!
	CPU_initLookupTables(); //Initialize our timing lookup tables!
	#ifdef CPU_USECYCLES
	CPU_useCycles = 1; //Are we using cycle-accurate emulation?
	#endif
	EMU_onCPUReset(); //Make sure all hardware, like CPU A20 is updated for the reset!
	CPU[activeCPU].D_B_Mask = (EMULATED_CPU>=CPU_80386)?1:0; //D_B mask when applyable!
	CPU[activeCPU].G_Mask = (EMULATED_CPU >= CPU_80386) ? 1 : 0; //G mask when applyable!
	CPU[activeCPU].is_reset = 1; //We're reset!
	CPU[activeCPU].CPL = 0; //We're real mode, so CPL=0!
	memset(&CPU[activeCPU].instructionfetch,0,sizeof(CPU[activeCPU].instructionfetch)); //Reset the instruction fetching system!
	CPU[activeCPU].instructionfetch.CPU_isFetching = CPU[activeCPU].instructionfetch.CPU_fetchphase =  1; //We're starting to fetch!
	CPU_initBIU(); //Initialize the BIU for use!
	Paging_clearTLB(); //Clear the TLB when resetting!
	effectivecpuaddresspins = cpuaddresspins[((EMULATED_CPU<<1)|is_XT)]; //What pins are supported for the current CPU/architecture?
}

void initCPU() //Initialize CPU for full system reset into known state!
{
	CPU_calcSegmentPrecalcsPrecalcs(); //Calculate the segmentation precalcs that are used!
	memset(&CPU, 0, sizeof(CPU)); //Reset the CPU fully!
	resetCPU(1); //Reset normally!
	Paging_initTLB(); //Initialize the TLB for usage!
}

void CPU_tickPendingReset()
{
	if (unlikely(CPU[activeCPU].resetPending)) //Are we pending?
	{
		if (BIU_resetRequested() && (CPU[activeCPU].instructionfetch.CPU_isFetching==1)) //Starting a new instruction or halted with pending Reset?
		{
			unlock(LOCK_CPU);
			doneCPU(); //Finish the CPU!
			resetCPU(0); //Simply fully reset the CPU on triple fault(e.g. reset pin result)!
			lock(LOCK_CPU);
			CPU[activeCPU].resetPending = 0; //Not pending reset anymore!
		}
	}
}

//data order is low-high, e.g. word 1234h is stored as 34h, 12h

/*
0xF3 Used with string REP, REPE/REPZ
0xF2 REPNE/REPNZ prefix
0xF0 LOCK prefix
0x2E CS segment override prefix
0x36 SS segment override prefix
0x3E DS segment override prefix
0x26 ES segment override prefix
0x64 FS segment override prefix
0x65 GS segment override prefix
0x66 Operand-size override
0x67 Address-size override
*/

OPTINLINE void CPU_setprefix(byte prefix) //Sets a prefix on!
{
	CPU_prefixes[activeCPU][(prefix>>3)] |= (1<<(prefix&7)); //Have prefix!
	switch (prefix) //Which prefix?
	{
	case 0x2E: //CS segment override prefix
		CPU[activeCPU].segment_register = CPU_SEGMENT_CS; //Override DS to CS!
		break;
	case 0x36: //SS segment override prefix
		CPU[activeCPU].segment_register = CPU_SEGMENT_SS; //Override DS to SS!
		break;
	case 0x3E: //DS segment override prefix
		CPU[activeCPU].segment_register = CPU_SEGMENT_DS; //Override SS to DS!
		break;
	case 0x26: //ES segment override prefix
		CPU[activeCPU].segment_register = CPU_SEGMENT_ES; //Override DS to ES!
		break;
	case 0x64: //FS segment override prefix
		CPU[activeCPU].segment_register = CPU_SEGMENT_FS; //Override DS to FS!
		break;
	case 0x65: //GS segment override prefix
		CPU[activeCPU].segment_register = CPU_SEGMENT_GS; //Override DS to GS!
		break;
	default: //Unknown special prefix action?
		break; //Do nothing!
	}
}

byte CPU_getprefix(byte prefix) //Prefix set?
{
	return ((CPU_prefixes[activeCPU][prefix>>3]>>(prefix&7))&1); //Get prefix set or reset!
}

OPTINLINE byte CPU_isPrefix(byte prefix)
{
	switch (prefix) //What prefix/opcode?
	{
	//First, normal instruction prefix codes:
		case 0xF2: //REPNE/REPNZ prefix
		case 0xF3: //REPZ
		case 0xF0: //LOCK prefix
		case 0x2E: //CS segment override prefix
		case 0x36: //SS segment override prefix
		case 0x3E: //DS segment override prefix
		case 0x26: //ES segment override prefix
			return 1; //Always a prefix!
		case 0x64: //FS segment override prefix
		case 0x65: //GS segment override prefix
		case 0x66: //Operand-size override
		case 0x67: //Address-size override
			return (EMULATED_CPU>=CPU_80386); //We're a prefix when 386+!
		default: //It's a normal OPcode?
			return 0; //No prefix!
			break; //Not use others!
	}

	return 0; //No prefix!
}

byte STACK_SEGMENT_DESCRIPTOR_B_BIT() //80286+: Gives the B-Bit of the DATA DESCRIPTOR TABLE FOR SS-register!
{
	if (EMULATED_CPU<=CPU_NECV30) //8086-NEC V20/V30?
	{
		return 0; //Always 16-bit descriptor!
	}

	return SEGDESC_NONCALLGATE_D_B(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_SS])&CPU[activeCPU].D_B_Mask; //Give the B-BIT of the SS-register!
}

byte CODE_SEGMENT_DESCRIPTOR_D_BIT() //80286+: Gives the B-Bit of the DATA DESCRIPTOR TABLE FOR SS-register!
{
	if (EMULATED_CPU<=CPU_NECV30) //8086-NEC V20/V30?
	{
		return 0; //Always 16-bit descriptor!
	}

	return SEGDESC_NONCALLGATE_D_B(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS])&CPU[activeCPU].D_B_Mask; //Give the D-BIT of the CS-register!
}

uint_32 CPU_InterruptReturn = 0;

CPU_Timings *timing = NULL; //The timing used for the current instruction!
Handler currentOP_handler = &CPU_unkOP;
extern Handler CurrentCPU_opcode_jmptbl[1024]; //Our standard internal standard opcode jmptbl!

OPTINLINE void CPU_resetInstructionSteps()
{
	//Prepare for a (repeated) instruction to execute!
	CPU[activeCPU].instructionstep = CPU[activeCPU].internalinstructionstep = CPU[activeCPU].modrmstep = CPU[activeCPU].internalmodrmstep = CPU[activeCPU].internalinterruptstep = CPU[activeCPU].stackchecked = 0; //Start the instruction-specific stage!
}

uint_32 last_eip;
byte ismultiprefix = 0; //Are we multi-prefix?
OPTINLINE byte CPU_readOP_prefix(byte *OP) //Reads OPCode with prefix(es)!
{
	CPU[activeCPU].cycles_Prefix = 0; //No cycles for the prefix by default!

	if (CPU[activeCPU].instructionfetch.CPU_fetchphase) //Reading opcodes?
	{
		if (CPU[activeCPU].instructionfetch.CPU_fetchphase==1) //Reading new opcode?
		{
			CPU_resetPrefixes(); //Reset all prefixes for this opcode!
			reset_modrm(); //Reset modr/m for the current opcode, for detecting it!
			CPU_InterruptReturn = last_eip = CPU[activeCPU].registers->EIP; //Interrupt return point by default!
			CPU[activeCPU].instructionfetch.CPU_fetchphase = 2; //Reading prefixes or opcode!
			ismultiprefix = 0; //Default to not being multi prefix!
		}
		if (CPU[activeCPU].instructionfetch.CPU_fetchphase==2) //Reading prefixes or opcode?
		{
			nextprefix: //Try next prefix/opcode?
			if (CPU_readOP(OP,1)) return 1; //Read opcode or prefix?
			if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
			if (CPU_isPrefix(*OP)) //We're a prefix?
			{
				CPU[activeCPU].cycles_Prefix += 2; //Add timing for the prefix!
				if (ismultiprefix && (EMULATED_CPU <= CPU_80286)) //This CPU has the bug and multiple prefixes are added?
				{
					CPU_InterruptReturn = last_eip; //Return to the last prefix only!
				}
				CPU_setprefix(*OP); //Set the prefix ON!
				last_eip = CPU[activeCPU].registers->EIP; //Save the current EIP of the last prefix possibility!
				ismultiprefix = 1; //We're multi-prefix now when triggered again!
				goto nextprefix; //Try the next prefix!
			}
			else //No prefix? We've read the actual opcode!
			{
				CPU[activeCPU].instructionfetch.CPU_fetchphase = 3; //Advance to stage 3: Fetching 0F instruction!
			}
		}
		//Now we have the opcode and prefixes set or reset!
		if (CPU[activeCPU].instructionfetch.CPU_fetchphase==3) //Check and fetch 0F opcode?
		{
			if ((*OP == 0x0F) && (EMULATED_CPU >= CPU_80286)) //0F instruction extensions used?
			{
				if (CPU_readOP(OP,1)) return 1; //Read the actual opcode to use!
				if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
				CPU[activeCPU].is0Fopcode = 1; //We're a 0F opcode!
				CPU[activeCPU].instructionfetch.CPU_fetchphase = 0; //We're fetched completely! Ready for first decode!
				CPU[activeCPU].instructionfetch.CPU_fetchingRM = 1; //Fetching R/M, if any!
				CPU[activeCPU].instructionfetch.CPU_fetchparameterPos = 0; //Init parameter position!
				memset(&params.instructionfetch,0,sizeof(params.instructionfetch)); //Init instruction fetch status!
			}
			else //Normal instruction?
			{
				CPU[activeCPU].is0Fopcode = 0; //We're a normal opcode!
				CPU[activeCPU].instructionfetch.CPU_fetchphase = 0; //We're fetched completely! Ready for first decode!
				CPU[activeCPU].instructionfetch.CPU_fetchingRM = 1; //Fetching R/M, if any!
				CPU[activeCPU].instructionfetch.CPU_fetchparameterPos = 0; //Init parameter position!
				memset(&params.instructionfetch,0,sizeof(params.instructionfetch)); //Init instruction fetch status!
			}
		}
	}

//Determine the stack&attribute sizes(286+)!
	//Stack address size is automatically retrieved!

	if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bits operand&address defaulted? We're a 32-bit Operand&Address size to default to instead!
	{
		CPU_Operand_size[activeCPU] = 1; //Set!
		CPU_Address_size[activeCPU] = 1; //Set!
	}
	else //16-bit defaults?
	{
		CPU_Operand_size[activeCPU] = 0; //Set!
		CPU_Address_size[activeCPU] = 0; //Set!
	}

	if (CPU_getprefix(0x66)) //Invert operand size?
	{
		CPU_Operand_size[activeCPU] = !CPU_Operand_size[activeCPU]; //Invert!
	}
	if (CPU_getprefix(0x67)) //Invert address size?
	{
		CPU_Address_size[activeCPU] = !CPU_Address_size[activeCPU]; //Invert!
	}

	CPU[activeCPU].address_size = (0xFFFF | (0xFFFF<<(CPU_Address_size[activeCPU]<<4))); //Effective address size for this instruction!

	//Now, check for the ModR/M byte, if present, and read the parameters if needed!
	timing = &CPUTimings[CPU_Operand_size[activeCPU]][(*OP<<1)|CPU[activeCPU].is0Fopcode]; //Only 2 modes implemented so far, 32-bit or 16-bit mode, with 0F opcode every odd entry!

	if (((timing->readwritebackinformation&0x80)==0) && CPU_getprefix(0xF0)) //LOCK when not allowed?
	{
		goto invalidlockprefix;
	}

	if (timing->used==0) goto skiptimings; //Are we not used?
	if (timing->has_modrm && CPU[activeCPU].instructionfetch.CPU_fetchingRM) //Do we have ModR/M data?
	{
		if (modrm_readparams(&params,timing->modrm_size,timing->modrm_specialflags,*OP)) return 1; //Read the params!
		CPU[activeCPU].instructionfetch.CPU_fetchparameterPos = 0; //Reset the parameter position again for new parameters!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
		if (MODRM_ERROR(params)) //An error occurred in the read params?
		{
			invalidlockprefix: //Lock prefix when not allowed? Count as #UD!
			currentOP_handler = &CPU_unkOP; //Unknown opcode/parameter!
			CPU_unkOP(); //Execute the unknown opcode handler!
			return 1; //Abort!
		}
		MODRM_src0 = timing->modrm_src0; //First source!
		MODRM_src1 = timing->modrm_src1; //Second source!
		CPU[activeCPU].instructionfetch.CPU_fetchingRM = 0; //We're done fetching the R/M parameters!
	}

	if (timing->parameters) //Gotten parameters?
	{
		switch (timing->parameters&~4) //What parameters?
		{
			case 1: //imm8?
				if (timing->parameters&4) //Only when ModR/M REG<2?
				{
					if (MODRM_REG(params.modrm)<2) //8-bit immediate?
					{
						if (CPU_readOP(&immb,1)) return 1; //Read 8-bit immediate!
						if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
					}
				}
				else //Normal imm8?
				{
					if (CPU_readOP(&immb,1)) return 1; //Read 8-bit immediate!
					if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
				}
				break;
			case 2: //imm16?
				if (timing->parameters&4) //Only when ModR/M REG<2?
				{
					if (MODRM_REG(params.modrm)<2) //16-bit immediate?
					{
						if (CPU_readOPw(&immw,1)) return 1; //Read 16-bit immediate!
						if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
					}
				}
				else //Normal imm16?
				{
					if (CPU_readOPw(&immw,1)) return 1; //Read 16-bit immediate!
					if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
				}
				break;
			case 3: //imm32?
				if (timing->parameters&4) //Only when ModR/M REG<2?
				{
					if (MODRM_REG(params.modrm)<2) //32-bit immediate?
					{
						if (CPU_readOPdw(&imm32,1)) return 1; //Read 32-bit immediate!
						if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
					}
				}
				else //Normal imm32?
				{
					if (CPU_readOPdw(&imm32,1)) return 1; //Read 32-bit immediate!
					if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
				}
				break;
			case 8: //imm16 + imm8
				if (CPU[activeCPU].instructionfetch.CPU_fetchparameters==0) //First parameter?
				{
					if (CPU_readOPw(&immw,1)) return 1; //Read 16-bit immediate!
					if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
					CPU[activeCPU].instructionfetch.CPU_fetchparameters = 1; //Start fetching the second parameter!
					CPU[activeCPU].instructionfetch.CPU_fetchparameterPos = 0; //Init parameter position!
				}
				if (CPU[activeCPU].instructionfetch.CPU_fetchparameters==1) //Second parameter?
				{
					if (CPU_readOP(&immb,1)) return 1; //Read 8-bit immediate!
					if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
					CPU[activeCPU].instructionfetch.CPU_fetchparameters = 2; //We're fetching the second(finished) parameter! This way, we're done fetching!
				}
				break;
			case 9: //imm64(ptr16:32)?
				if (timing->parameters & 4) //Only when ModR/M REG<2?
				{
					if (MODRM_REG(params.modrm)<2) //32-bit immediate?
					{
						if (CPU[activeCPU].instructionfetch.CPU_fetchparameters==0) //First parameter?
						{
							if (CPU_readOPdw(&imm32,1)) return 1; //Read 32-bit immediate offset!
							if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
							imm64 = (uint_64)imm32; //Convert to 64-bit!
							CPU[activeCPU].instructionfetch.CPU_fetchparameters = 1; //Second parameter!
							CPU[activeCPU].instructionfetch.CPU_fetchparameterPos = 0; //Init parameter position!
						}
						if (CPU[activeCPU].instructionfetch.CPU_fetchparameters==1) //Second parameter?
						{
							if (CPU_readOPw(&immw,1)) return 1; //Read another 16-bit immediate!
							if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
							imm64 |= ((uint_64)immw << 32);
							CPU[activeCPU].instructionfetch.CPU_fetchparameters = 2; //We're finished!
						}
					}
				}
				else //Normal imm32?
				{
					if (CPU[activeCPU].instructionfetch.CPU_fetchparameters==0) //First parameter?
					{
						if (CPU_readOPdw(&imm32,1)) return 1; //Read 32-bit immediate offset!
						if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
						imm64 = (uint_64)imm32; //Convert to 64-bit!
						CPU[activeCPU].instructionfetch.CPU_fetchparameters = 1; //Second parameter!
						CPU[activeCPU].instructionfetch.CPU_fetchparameterPos = 0; //Init parameter position!
					}
					if (CPU[activeCPU].instructionfetch.CPU_fetchparameters==1) //Second parameter?
					{
						if (CPU_readOPw(&immw,1)) return 1; //Read another 16-bit immediate!
						if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
						imm64 |= ((uint_64)immw << 32);
						if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
						CPU[activeCPU].instructionfetch.CPU_fetchparameters = 2; //We're finished!
					}
				}
				break;
			case 0xA: //imm16/32, depending on the address size?
				if (CPU_Address_size[activeCPU]) //32-bit address?
				{
					if (CPU_readOPdw(&immaddr32,1)) return 1; //Read 32-bit immediate offset!
					if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
				}
				else //16-bit address?
				{
					if (CPU_readOPw(&immw,1)) return 1; //Read 32-bit immediate offset!
					immaddr32 = (uint_32)immw; //Convert to 32-bit immediate!
					if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
				}
			default: //Unknown?
				//Ignore the parameters!
				break;
		}
	}

skiptimings: //Skip all timings and parameters(invalid instruction)!
	CPU_resetInstructionSteps(); //Reset the current instruction steps!
	CPU[activeCPU].lastopcode = *OP; //Last OPcode for reference!
	CPU[activeCPU].lastopcode0F = CPU[activeCPU].is0Fopcode; //Last OPcode for reference!
	CPU[activeCPU].lastmodrm = (likely(timing)?timing->has_modrm:0)?params.modrm:0; //Modr/m if used!
	currentOP_handler = CurrentCPU_opcode_jmptbl[((word)*OP << 2) | (CPU[activeCPU].is0Fopcode<<1) | CPU_Operand_size[activeCPU]];
	CPU_executionphase_newopcode(); //We're starting a new opcode, notify the execution phase handlers!
	return 0; //We're done fetching the instruction!
}

void doneCPU() //Finish the CPU!
{
	free_CPUregisters(); //Finish the allocated registers!
	CPU_doneBIU(); //Finish the BIU!
	memset(&CPU[activeCPU],0,sizeof(CPU[activeCPU])); //Initilialize the CPU to known state!
}

CPU_registers dummyregisters; //Dummy registers!

//Specs for 80386 says we start in REAL mode!
//STDMODE: 0=protected; 1=real; 2=Virtual 8086.

void CPU_resetMode() //Resets the mode!
{
	if (!CPU[activeCPU].registers) CPU_initRegisters(0); //Make sure we have registers!
	//Always start in REAL mode!
	if (!CPU[activeCPU].registers) return; //We can't work now!
	FLAGW_V8(0); //Disable Virtual 8086 mode!
	CPU[activeCPU].registers->CR0 &= ~CR0_PE; //Real mode!
	updateCPUmode(); //Update the CPU mode!
}

byte CPUmode = CPU_MODE_REAL; //The current CPU mode!
const byte modes[4] = { CPU_MODE_REAL, CPU_MODE_PROTECTED, CPU_MODE_REAL, CPU_MODE_8086 }; //All possible modes (VM86 mode can't exist without Protected Mode!)

void updateCPL() //Update the CPL to be the currently loaded CPL!
{
	byte mode = 0; //Buffer new mode to start using for comparison!
	mode = FLAG_V8; //VM86 mode?
	mode <<= 1;
	mode |= (CPU[activeCPU].registers->CR0&CR0_PE); //Protected mode?
	mode = modes[mode]; //What is the new set mode, if changed?
	//Determine CPL based on the mode!
	if (mode == CPU_MODE_PROTECTED) //Switching from real mode to protected mode?
	{
		CPU[activeCPU].CPL = GENERALSEGMENT_DPL(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_SS]); //DPL of SS determines CPL from now on!
	}
	else if (mode == CPU_MODE_8086) //Switching to Virtual 8086 mode?
	{
		CPU[activeCPU].CPL = 3; //Make sure we're CPL 3 in Virtual 8086 mode!
	}
	else //Switching back to real mode?
	{
		CPU[activeCPU].CPL = 0; //Make sure we're CPL 0 in Real mode!
	}
}

void updateCPUmode() //Update the CPU mode!
{
	byte mode = 0; //Buffer new mode to start using for comparison!
	if (!CPU[activeCPU].registers)
	{
		CPU_initRegisters(0); //Make sure we have registers!
		if (!CPU[activeCPU].registers) CPU[activeCPU].registers = &dummyregisters; //Dummy registers!
	}
	mode = FLAG_V8; //VM86 mode?
	mode <<= 1;
	mode |= (CPU[activeCPU].registers->CR0&CR0_PE); //Protected mode?
	mode = modes[mode]; //What is the new set mode, if changed?
	if (unlikely(mode!=CPUmode)) //Mode changed?
	{
		if ((CPUmode == CPU_MODE_REAL) && (mode == CPU_MODE_PROTECTED)) //Switching from real mode to protected mode?
		{
			CPU[activeCPU].CPL = GENERALSEGMENT_DPL(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_SS]); //DPL of SS determines CPL from now on!
		}
		else if ((CPUmode != CPU_MODE_REAL) && (mode == CPU_MODE_REAL)) //Switching back to real mode?
		{
			CPU[activeCPU].CPL = 0; //Make sure we're CPL 0 in Real mode!
		}
		else if ((CPUmode != CPU_MODE_8086) && (mode == CPU_MODE_8086)) //Switching to Virtual 8086 mode?
		{
			CPU[activeCPU].CPL = 3; //Make sure we're CPL 3 in Virtual 8086 mode!
		}
		CPUmode = mode; //Mode levels: Real mode > Protected Mode > VM86 Mode!
	}
	CPU[activeCPU].is_paging = ((CPUmode!=CPU_MODE_REAL)&((CPU[activeCPU].registers->CR0&CR0_PG)>>31)); //Are we paging in protected mode!
	CPU[activeCPU].is_aligning = ((EMULATED_CPU >= CPU_80486) && FLAGREGR_AC(CPU[activeCPU].registers) && (CPU[activeCPU].registers->CR0 & 0x40000)); //Alignment check in effect for CPL 3?
}

byte getcpumode() //Retrieves the current mode!
{
	return CPUmode; //Give the current CPU mode!
}

byte isPM()
{
	return (CPUmode!=CPU_MODE_REAL)?1:0; //Are we in protected mode?
}

byte isV86()
{
	return (CPUmode==CPU_MODE_8086)?1:0; //Are we in virtual 8086 mode?
}

//PUSH and POP values!

//Memory is the same as PSP: 1234h is 34h 12h, in stack terms reversed, because of top-down stack!

//Use below functions for the STACK!

void CPU_PUSH8(byte val, byte is32instruction) //Push Byte!
{
	word v=val; //Convert!
	CPU_PUSH16(&v,0); //Push 16!
}

byte CPU_PUSH8_BIU(byte val, byte is32instruction) //Push Byte!
{
	word v=val; //Convert!
	return CPU_PUSH16_BIU(&v,is32instruction); //Push 16!
}

byte CPU_POP8(byte is32instruction)
{
	return (CPU_POP16(is32instruction)&0xFF); //Give the result!
}

byte CPU_POP8_BIU(byte is32instruction) //Request an 8-bit POP from the BIU!
{
	return (CPU_POP16_BIU(is32instruction)); //Give the result: we're requesting from the BIU to POP one entry!
}

//Changes in stack during PUSH and POP operations!
sbyte stack_pushchange(byte dword)
{
	return -(2 << dword); //Decrease!
}

sbyte stack_popchange(byte dword)
{
	return (2 << dword); //Decrease!
}

OPTINLINE void stack_push(byte dword) //Push 16/32-bits to stack!
{
	if (STACK_SEGMENT_DESCRIPTOR_B_BIT()) //32-bits?
	{
		CPU[activeCPU].registers->ESP -= (2 << dword); //Decrease!
	}
	else //16-bits?
	{
		CPU[activeCPU].registers->SP -= (2 << dword); //Decrease!
	}
}

OPTINLINE void stack_pop(byte dword) //Push 16/32-bits to stack!
{
	if (STACK_SEGMENT_DESCRIPTOR_B_BIT()) //32-bits?
	{
		CPU[activeCPU].registers->ESP += (2 << dword); //Increase!
	}
	else //16-bits?
	{
		CPU[activeCPU].registers->SP += (2 << dword); //Increase!
	}
}

void CPU_PUSH16(word *val, byte is32instruction) //Push Word!
{
	if (EMULATED_CPU<=CPU_NECV30) //186- we push the decremented value of SP to the stack instead of the original value?
	{
		stack_push(0); //We're pushing a 16-bit value!
		MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), *val,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Put value!
	}
	else //286+?
	{
		word oldval = *val; //Original value, saved before decrementing (E)SP!
		stack_push(is32instruction); //We're pushing a 16-bit or 32-bit value!
		if (is32instruction) //32-bit?
		{
			MMU_wdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), (uint_32)oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Put value!
		}
		else
		{
			MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Put value!
		}
	}
}

byte CPU_PUSH16_BIU(word *val, byte is32instruction) //Push Word!
{
	if (EMULATED_CPU<=CPU_NECV30) //186- we push the decremented value of SP to the stack instead of the original value?
	{
		if (CPU[activeCPU].pushbusy==0)
		{
			stack_push(0); //We're pushing a 16-bit value!
			CPU[activeCPU].pushbusy = 1; //We're pending!
		}
		if (CPU_request_MMUww(CPU_SEGMENT_SS,(CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()),*val,!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Request Put value!
		{
			CPU[activeCPU].pushbusy = 0; //We're not pending anymore!
			return 1;
		}
	}
	else //286+?
	{
		static word oldval;
		if (CPU[activeCPU].pushbusy==0)
		{
			oldval = *val; //Original value, saved before decrementing (E)SP!
			stack_push(is32instruction); //We're pushing a 16-bit or 32-bit value!
			CPU[activeCPU].pushbusy = 1; //We're pending!
		}
		/*if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
		{
			if (CPU_request_MMUwdw(CPU_SEGMENT_SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), (uint_32)oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Request Put value!
			{
				CPU[activeCPU].pushbusy = 0; //We're not pending anymore!
				return 1;
			}
		}
		else
		{
		*/
			if (CPU_request_MMUww(CPU_SEGMENT_SS,(CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()),oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Request Put value!
			{
				CPU[activeCPU].pushbusy = 0; //We're not pending anymore!
				return 1;
			}
			//MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Put value!
		//}
	}
	return 0; //Not ready yet!
}

word CPU_POP16(byte is32instruction) //Pop Word!
{
	word result;
	/*if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
	{
		result = (word)MMU_rdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), 0,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	}
	else //16-bit?
	{
	*/
		result = MMU_rw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), 0,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	//}
	stack_pop(/*CODE_SEGMENT_DESCRIPTOR_D_BIT()*/ is32instruction); //We're popping a 16-bit value!
	return result; //Give the result!
}

byte CPU_POP16_BIU(byte is32instruction) //Pop Word!
{
	byte result;
	/*if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
	{
		result = CPU_request_MMUrdw(CPU_SEGMENT_SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()),!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	}
	else //16-bit?
	{
	*/
		 result = CPU_request_MMUrw(CPU_SEGMENT_SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), !STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	//}
	if (result) //Requested?
	{
		stack_pop(/*CODE_SEGMENT_DESCRIPTOR_D_BIT()*/ is32instruction); //We're popping a 16-bit value!
	}
	return result; //Give the result!
}

byte CPU_PUSH32_BIU(uint_32 *val) //Push DWord!
{
	if (EMULATED_CPU<CPU_80386) //286-?
	{
		if (CPU[activeCPU].pushbusy==0)
		{
			stack_push(0); //We're pushing a 16-bit value!
			CPU[activeCPU].pushbusy = 1; //We're pending!
		}
		if (CPU_request_MMUww(CPU_SEGMENT_SS,(CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()),*val,!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Request Put value!
		{
			CPU[activeCPU].pushbusy = 0; //We're not pending anymore!
			return 1;
		}
	}
	else //386+?
	{
		static uint_32 oldval;
		if (CPU[activeCPU].pushbusy==0)
		{
			oldval = *val; //Original value, saved before decrementing (E)SP!
			stack_push(/*CODE_SEGMENT_DESCRIPTOR_D_BIT()*/ 1); //We're pushing a 16-bit or 32-bit value!
			CPU[activeCPU].pushbusy = 1; //We're pending!
		}
		/*if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
		{
		*/
			if (CPU_request_MMUwdw(CPU_SEGMENT_SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Request Put value!
			{
				CPU[activeCPU].pushbusy = 0; //We're not pending anymore!
				return 1;
			}
		/*}
		else
		{
			if (CPU_request_MMUww(CPU_SEGMENT_SS,(CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()),(word)oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Request Put value!
			{
				CPU[activeCPU].pushbusy = 0; //We're not pending anymore!
				return 1;
			}
			//MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Put value!
		}
		*/
	}
	return 0; //Not ready!
}

void CPU_PUSH32(uint_32 *val) //Push DWord!
{
	if (EMULATED_CPU<CPU_80386) //286-?
	{
		stack_push(0); //We're pushing a 32-bit value!
		MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), *val,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Put value!
	}
	else //386+?
	{
		uint_32 oldval = *val; //Old value!
		stack_push(/*CODE_SEGMENT_DESCRIPTOR_D_BIT()*/ 1); //We're pushing a 32-bit value!
		/*if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
		{
		*/
			MMU_wdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Put value!
		/*}
		else //16-bit?
		{
			MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), (word)oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Put value!
		}*/
	}
}

uint_32 CPU_POP32() //Full stack used!
{
	uint_32 result;
	/*if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
	{
	*/
		result = MMU_rdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, CPU[activeCPU].registers->ESP&getstackaddrsizelimiter(), 0,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	/*}
	else //16-bit?
	{
		result = (uint_32)MMU_rw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, CPU[activeCPU].registers->ESP&getstackaddrsizelimiter(), 0,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	}
	*/
	stack_pop(/*CODE_SEGMENT_DESCRIPTOR_D_BIT()*/ 1); //We're popping a 32-bit value!
	return result; //Give the result!
}

byte CPU_POP32_BIU() //Full stack used!
{
	byte result;
	/*if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
	{
	*/
		result = CPU_request_MMUrdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->ESP&getstackaddrsizelimiter(), !STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	/*}
	else //16-bit?
	{
		result = CPU_request_MMUrw(CPU_SEGMENT_SS, CPU[activeCPU].registers->ESP&getstackaddrsizelimiter(), !STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	}
	*/
	if (result) //Requested?
	{
		stack_pop(/*CODE_SEGMENT_DESCRIPTOR_D_BIT()*/ 1); //We're popping a 32-bit value!
	}
	return result; //Give the result!
}

//Final stuff:

char textsegments[][5] =   //Comply to CPU_REGISTER_XX order!
{
	"CS",
	"SS",
	"DS",
	"ES",
	"FS",
	"GS",
	"TR",
	"LDTR"
};

char *CPU_textsegment(byte defaultsegment) //Plain segment to use!
{
	if (CPU[activeCPU].segment_register==CPU_SEGMENT_DEFAULT) //Default segment?
	{
		return &textsegments[defaultsegment][0]; //Default segment!
	}
	return &textsegments[CPU[activeCPU].segment_register][0]; //Use Data Segment (or different in case) for data!
}

void CPU_afterexec(); //Prototype for below!

word CPU_exec_CS, CPU_debugger_CS; //OPCode CS
uint_32 CPU_exec_EIP, CPU_debugger_EIP; //OPCode EIP

word CPU_exec_lastCS=0; //OPCode CS
uint_32 CPU_exec_lastEIP=0; //OPCode EIP

void CPU_beforeexec()
{
	//This applies to all processors:
	INLINEREGISTER uint_32 tempflags;
	tempflags = CPU[activeCPU].registers->EFLAGS; //Load the flags to set/clear!
	tempflags &= ~(8|32); //Clear bits 3&5!

	switch (EMULATED_CPU) //What CPU flags to emulate?
	{
	case CPU_8086:
	case CPU_NECV30:
		tempflags |= 0xF000; //High bits are stuck to 1!
		break;
	case CPU_80286:
		if (getcpumode() == CPU_MODE_REAL) //Real mode?
		{
			tempflags &= 0x0FFF; //Always set the high flags in real mode only!
		}
		else //Protected mode?
		{
			tempflags &= 0x7FFF; //Bit 15 is always cleared!
		}
		break;
	case CPU_80386:
		if (getcpumode() == CPU_MODE_REAL) //Real mode?
		{
			tempflags &= 0x37FFF; //Always set the high flags in real mode only!
		}
		else //Protected mode?
		{
			tempflags &= 0x37FFF; //Bit 15 is always cleared! AC is stuck to 0! All bits above AC are always cleared!
		}
		break;
	case CPU_80486:
		if (getcpumode() == CPU_MODE_REAL) //Real mode?
		{
			tempflags &= 0x277FFF; //Always set the high flags in real mode only!
		}
		else //Protected mode?
		{
			tempflags &= 0x277FFF; //Bit 15 is always cleared! Don't allow setting of the CPUID and larger flags! Allow toggling the CPUID flag too(it's supported)!
		}
		break;
	case CPU_PENTIUM:
		//Allow all bits to be set, except the one needed from the 80386+ identification(bit 15=0)!
		if (getcpumode() == CPU_MODE_REAL) //Real mode?
		{
			tempflags &= 0x3F7FFF; //Always set the high flags in real mode only!
		}
		else //Protected mode?
		{
			tempflags &= 0x3F7FFF;
		}
		break;
	default: //Unknown CPU?
		break;
	}
	tempflags |= 2; //Clear bit values 8&32(unmapped bits 3&5) and set bit value 2!
	CPU[activeCPU].registers->EFLAGS = tempflags; //Update the flags!

	if (CPU[activeCPU].instructionfetch.CPU_isFetching && (CPU[activeCPU].instructionfetch.CPU_fetchphase==1)) //Starting a new instruction?
	{
		CPU[activeCPU].trapped = FLAG_TF; //Are we to be trapped this instruction?
	}
}

byte blockREP = 0; //Block the instruction from executing (REP with (E)CX=0
byte gotREP = 0; //Default: no REP-prefix used!
byte REPPending = 0; //Pending REP reset?

void CPU_8086REPPending() //Execute this before CPU_exec!
{
	if (REPPending) //Pending REP?
	{
		REPPending = 0; //Disable pending REP!
		CPU_resetOP(); //Rerun the last instruction!
	}
}

byte CPU_segmentOverridden(byte TheActiveCPU)
{
	return (CPU[TheActiveCPU].segment_register != CPU_SEGMENT_DEFAULT); //Is the segment register overridden?
}

byte newREP = 1; //Are we a new repeating instruction (REP issued?)

//Stuff for CPU 286+ timing processing!
byte didJump = 0; //Did we jump this instruction?
byte ENTER_L = 0; //Level value of the ENTER instruction!

byte hascallinterrupttaken_type = 0xFF; //INT gate type taken. Low 4 bits are the type. High 2 bits are privilege level/task gate flag. Left at 0xFF when nothing is used(unknown case?)
byte CPU_interruptraised = 0;

extern CPUPM_Timings CPUPMTimings[CPUPMTIMINGS_SIZE]; //The PM timings full table!

void CPU_resetTimings()
{
	CPU[activeCPU].cycles_HWOP = 0; //No hardware interrupt to use anymore!
	CPU[activeCPU].cycles_Prefetch_BIU = 0; //Reset cycles spent on BIU!
	CPU[activeCPU].cycles_Prefix = 0; //No cycles prefix to use anymore!
	CPU[activeCPU].cycles_Exception = 0; //No cycles Exception to use anymore!
	CPU[activeCPU].cycles_Prefetch = 0; //No cycles prefetch to use anymore!
	CPU[activeCPU].cycles_OP = 0; //Reset cycles (used by CPU to check for presets (see below))!
	CPU[activeCPU].cycles_stallBIU = 0; //Reset cycles to stall (used by BIU to check for stalling during any jump (see below))!
	CPU[activeCPU].cycles_stallBUS = 0; //Reset cycles to stall the BUS!
	CPU[activeCPU].cycles_Prefetch_DMA = 0; //Reset cycles spent on DMA by the BIU!
	CPU[activeCPU].cycles_EA = 0; //Reset EA cycles!
}

byte REPZ = 0; //Default to REP!
byte didNewREP = 0, didRepeating=0; //Did we do a REP?
byte BST_cnt = 0; //How many of bit scan/test (forward) times are taken?
byte protection_PortRightsLookedup = 0; //Are the port rights looked up?

byte CPU_apply286cycles() //Apply the 80286+ cycles method. Result: 0 when to apply normal cycles. 1 when 80286+ cycles are applied!
{
	if (EMULATED_CPU<CPU_80286) return 0; //Not applied on unsupported processors!
	word *currentinstructiontiming; //Current timing we're processing!
	byte instructiontiming, ismemory, modrm_threevariablesused; //Timing loop used on 286+ CPUs!
	word currentinstructiontimingindex;
	MemoryTimingInfo *currenttimingcheck; //Current timing check!
	//80286 uses other timings than the other chips!
	ismemory = modrm_ismemory(params)?1:0; //Are we accessing memory?
	if (ismemory)
	{
		modrm_threevariablesused = MODRM_threevariables(params); //Three variables used?
	}
	else
	{
		modrm_threevariablesused = 0; //Only 2 or less variables used in calculating the ModR/M.
	}

	if (CPU_interruptraised) //Any fault is raised?
	{
		ismemory = modrm_threevariablesused = 0; //Not to be applied with this!
		currentinstructiontiming = &timing286lookup[isPM()|((CPU_Operand_size[activeCPU])<<1)][0][0][0xCD][0x00][0]; //Start by pointing to our records to process! Enforce interrupt!
	}
	else
	{
		currentinstructiontiming = &timing286lookup[isPM()|((CPU_Operand_size[activeCPU])<<1)][ismemory][CPU[activeCPU].is0Fopcode][CPU[activeCPU].lastopcode][MODRM_REG(params.modrm)][0]; //Start by pointing to our records to process!
	}
	//Try to use the lookup table!
	for (instructiontiming=0;((instructiontiming<8)&&*currentinstructiontiming);++instructiontiming, ++currentinstructiontiming) //Process all timing candidates!
	{
		if (*currentinstructiontiming) //Valid timing?
		{
			currentinstructiontimingindex = (*currentinstructiontiming - 1); //Actual instruction timing index to use(1-base to 0-base)!
			if (CPUPMTimings[currentinstructiontimingindex].CPUmode[isPM()].ismemory[ismemory].basetiming) //Do we have valid timing to use?
			{
				currenttimingcheck = &CPUPMTimings[currentinstructiontimingindex].CPUmode[isPM()].ismemory[ismemory]; //Our current info to check!
				if (currenttimingcheck->addclock&0x80) //Multiply BST_cnt and add to this to get the correct timing?
				{
					if ((currenttimingcheck->n&0x80)==((protection_PortRightsLookedup&1)<<7)) //Match case?
					{
						//REP support added for string instructions!
						if (didNewREP || ((currenttimingcheck->addclock&2)==0)) //Including the REP, first instruction?
						{
							CPU[activeCPU].cycles_OP += currenttimingcheck->basetiming; //Use base timing specified only!
						}
						else //Already repeating instruction continued?
						{
							CPU[activeCPU].cycles_OP += (currenttimingcheck->n&0x7F); //Simply cycle count added each REPeated instruction!
						}
						if (modrm_threevariablesused && (currenttimingcheck->addclock&1)) ++CPU[activeCPU].cycles_OP; //One cycle to add with added clock!
						return 1; //Apply the cycles!
					}
				}
				else if (currenttimingcheck->addclock&0x40) //Multiply BST_cnt and add to this to get the correct timing?
				{
					CPU[activeCPU].cycles_OP += currenttimingcheck->basetiming; //Use base timing specified only!
					CPU[activeCPU].cycles_OP += currenttimingcheck->n*BST_cnt; //This adds the n value for each level linearly!
					if (modrm_threevariablesused && (currenttimingcheck->addclock&1)) ++CPU[activeCPU].cycles_OP; //One cycle to add with added clock!
					return 1; //Apply the cycles!									
				}
				else if (currenttimingcheck->addclock&0x20) //L of instruction doesn't fit in 1 bit?
				{
					if ((ENTER_L&1)!=ENTER_L) //Doesn't fit in 1 bit?
					{
						if ((ENTER_L&1)==currenttimingcheck->n) //Matching timing?
						{
							CPU[activeCPU].cycles_OP += currenttimingcheck->basetiming; //Use base timing specified only!
							CPU[activeCPU].cycles_OP += currenttimingcheck->n*(ENTER_L-1); //This adds the n value for each level after level 1 linearly!
							if (modrm_threevariablesused && (currenttimingcheck->addclock&1)) ++CPU[activeCPU].cycles_OP; //One cycle to add with added clock!
							return 1; //Apply the cycles!									
						}
					}
				}
				else if (currenttimingcheck->addclock&0x10) //L of instruction fits in 1 bit and matches?
				{
					if ((ENTER_L&1)==ENTER_L) //Fits in 1 bit?
					{
						if ((ENTER_L&1)==currenttimingcheck->n) //Matching timing?
						{
							CPU[activeCPU].cycles_OP += currenttimingcheck->basetiming; //Use base timing specified only!
							if (modrm_threevariablesused && (currenttimingcheck->addclock&1)) ++CPU[activeCPU].cycles_OP; //One cycle to add with added clock!
							return 1; //Apply the cycles!									
						}
					}
				}
				else if (currenttimingcheck->addclock&0x08) //Only when jump taken?
				{
					if (didJump) //Did we jump?
					{
						CPU[activeCPU].cycles_OP += currenttimingcheck->basetiming; //Use base timing specified only!								
						if (modrm_threevariablesused && (currenttimingcheck->addclock&1)) ++CPU[activeCPU].cycles_OP; //One cycle to add with added clock!
						return 1; //Apply the cycles!
					}
				}
				else if (currenttimingcheck->addclock&0x04) //Gate type has to match in order to be processed?
				{
					if (currenttimingcheck->n==hascallinterrupttaken_type) //Did we execute this kind of gate?
					{
						CPU[activeCPU].cycles_OP += currenttimingcheck->basetiming; //Use base timing specified only!								
						if (modrm_threevariablesused && (currenttimingcheck->addclock&1)) ++CPU[activeCPU].cycles_OP; //One cycle to add with added clock!
						return 1; //Apply the cycles!								
					}
				}
				else if (currenttimingcheck->addclock&0x02) //REP((N)Z) instruction prefix only?
				{
					if (didRepeating) //Are we executing a repeat?
					{
						if (didNewREP) //Including the REP, first instruction?
						{
							CPU[activeCPU].cycles_OP += currenttimingcheck->basetiming; //Use base timing specified only!
						}
						else //Already repeating instruction continued?
						{
							CPU[activeCPU].cycles_OP += currenttimingcheck->n; //Simply cycle count added each REPeated instruction!
						}
						if (modrm_threevariablesused && (currenttimingcheck->addclock&1)) ++CPU[activeCPU].cycles_OP; //One cycle to add with added clock!
						return 1; //Apply the cycles!
					}
				}
				else //Normal/default behaviour? Always matches!
				{
					CPU[activeCPU].cycles_OP += currenttimingcheck->basetiming; //Use base timing specified only!
					if (modrm_threevariablesused && (currenttimingcheck->addclock&1)) ++CPU[activeCPU].cycles_OP; //One cycle to add with added clock!
					return 1; //Apply the cycles!
				}
			}
		}
	}
	return 0; //Not applied, because it's an unknown instruction!
}

extern BIU_type BIU[MAXCPUS]; //All possible BIUs!
byte BIUresponsedummy = 0;

void CPU_exec() //Processes the opcode at CS:EIP (386) or CS:IP (8086).
{
	static uint_32 previousCSstart;
	static char debugtext[256]; //Debug text!
	uint_32 REPcondition; //What kind of condition?
	//byte cycles_counted = 0; //Cycles have been counted?
	if (likely((BIU_Ready()&&(CPU[activeCPU].halt==0))==0)) //BIU not ready to continue? We're handling seperate cycles still!
	{
		CPU[activeCPU].executed = 0; //Not executing anymore!
		goto BIUWaiting; //Are we ready to step the Execution Unit?
	}
	if (CPU_executionphase_busy()) //Busy during execution?
	{
		goto executionphase_running; //Continue an running instruction!
	}
	if (CPU[activeCPU].instructionfetch.CPU_isFetching && (CPU[activeCPU].instructionfetch.CPU_fetchphase==1)) //Starting a new instruction(or repeating one)?
	{
		CPU[activeCPU].allowInterrupts = 1; //Allow interrupts again after this instruction!
		CPU[activeCPU].allowTF = 1; //Default: allow TF to be triggered after the instruction!
		CPU[activeCPU].debuggerFaultRaised = 0; //Default: no debugger fault raised!
		bufferMMU(); //Buffer the MMU writes for us!
		debugger_beforeCPU(); //Everything that needs to be done before the CPU executes!
		MMU_resetaddr(); //Reset invalid address for our usage!
		CPU_8086REPPending(); //Process pending REP!
		protection_nextOP(); //Prepare protection for the next instruction!
		if (!CPU[activeCPU].repeating)
		{
			MMU_clearOP(); //Clear the OPcode buffer in the MMU (equal to our instruction cache) when not repeating!
			BIU_instructionStart(); //Handle all when instructions are starting!
		}

		previousCSstart = CPU_MMU_start(CPU_SEGMENT_CS,CPU[activeCPU].registers->CS); //Save the used CS start address!

		if (CPU[activeCPU].permanentreset) //We've entered a permanent reset?
		{
			CPU[activeCPU].cycles = 4; //Small cycle dummy! Must be greater than zero!
			return; //Don't run the CPU: we're in a permanent reset state!
		}

		if (fifobuffer_freesize(BIU[activeCPU].responses)!=BIU[activeCPU].responses->size) //Starting an instruction with a response remaining?
		{
			dolog("CPU","Warning: starting instruction with BIU still having a result buffered! Previous instruction: %02X(0F:%i,ModRM:%02X)@%04X:%08x",CPU[activeCPU].previousopcode,CPU[activeCPU].previousopcode0F,CPU[activeCPU].previousmodrm,CPU_exec_CS,CPU_exec_EIP);
			BIU_readResultb(&BIUresponsedummy); //Discard the result: we're logging but continuing on simply!
		}

		CPU[activeCPU].have_oldESP = 0; //Default: no ESP to return to during exceptions!
		CPU[activeCPU].have_oldEBP = 0; //Default: no EBP to return to during exceptions!
		CPU[activeCPU].have_oldSS = 0; //Default: no SS to return to during exceptions!
		CPU[activeCPU].have_oldSegments = 0; //Default: no Segments to return during exceptions!
		CPU[activeCPU].have_oldEFLAGS = 0; //Default: no EFLAGS to return during exceptions!

		//Initialize stuff needed for local CPU timing!
		didJump = 0; //Default: we didn't jump!
		ENTER_L = 0; //Default to no L depth!
		hascallinterrupttaken_type = 0xFF; //Default to no call/interrupt taken type!
		CPU_interruptraised = 0; //Default: no interrupt raised!

		//Now, starting the instruction preprocessing!
		CPU[activeCPU].is_reset = 0; //We're not reset anymore from now on!
		if (!CPU[activeCPU].repeating) //Not repeating instructions?
		{
			CPU[activeCPU].segment_register = CPU_SEGMENT_DEFAULT; //Default data segment register (default: auto)!
			//Save the last coordinates!
			CPU_exec_lastCS = CPU_exec_CS;
			CPU_exec_lastEIP = CPU_exec_EIP;
			//Save the current coordinates!
			CPU_exec_CS = CPU[activeCPU].registers->CS; //CS of command!
			CPU_exec_EIP = (CPU[activeCPU].registers->EIP&CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].PRECALCS.roof); //EIP of command!
		}
	
		//Save the starting point when debugging!
		CPU_debugger_CS = CPU_exec_CS;
		CPU_debugger_EIP = CPU_exec_EIP;
		CPU_saveFaultData(); //Save any fault data!

		if (getcpumode()!=CPU_MODE_REAL) //Protected mode?
		{
			if (CPU[activeCPU].allowInterrupts) //Do we allow interrupts(and traps) to be fired?
			{
				if (checkProtectedModeDebugger(previousCSstart+CPU_exec_EIP,PROTECTEDMODEDEBUGGER_TYPE_EXECUTION)) //Breakpoint at the current address(linear address space)?
				{
					return; //Protected mode debugger activated! Don't fetch or execute!
				}
			}
		}

		CPU[activeCPU].faultraised = 0; //Default fault raised!
		CPU[activeCPU].faultlevel = 0; //Default to no fault level!

		cleardata(&debugtext[0],sizeof(debugtext)); //Init debugger!
	}

	static byte OP = 0xCC; //The opcode!
	if (CPU[activeCPU].repeating) //REPeating instruction?
	{
		OP = CPU[activeCPU].lastopcode; //Execute the last opcode again!
		newREP = 0; //Not a new repeating instruction!
		if (CPU[activeCPU].instructionfetch.CPU_isFetching && (CPU[activeCPU].instructionfetch.CPU_fetchphase==1)) //New instruction to start?
		{
			CPU_resetInstructionSteps(); //Reset all timing that's still running!
		}
		memset(&CPU[activeCPU].instructionfetch,0,sizeof(CPU[activeCPU].instructionfetch)); //Not fetching anything anymore, we're ready to use!
	}
	else //Not a repeating instruction?
	{
		if (CPU[activeCPU].instructionfetch.CPU_isFetching) //Are we fetching?
		{
			CPU[activeCPU].executed = 0; //Not executed yet!
			if (CPU_readOP_prefix(&OP)) //Finished 
			{
				if (!CPU[activeCPU].cycles_OP) CPU[activeCPU].cycles_OP = 1; //Take 1 cycle by default!
				if (CPU[activeCPU].faultraised) //Fault has been raised while fetching&decoding the instruction?
				{
					memset(&CPU[activeCPU].instructionfetch,0,sizeof(CPU[activeCPU].instructionfetch)); //Finished fetching!
					CPU[activeCPU].instructionfetch.CPU_isFetching = CPU[activeCPU].instructionfetch.CPU_fetchphase = 1; //Start fetching the next instruction when available(not repeating etc.)!
					CPU[activeCPU].executed = 1; //We're counting as an finished instruction to handle the fault!
				}
				goto fetchinginstruction; //Process prefix(es) and read OPCode!
			}
			if (CPU[activeCPU].cycles_EA==0)
			{
				memset(&CPU[activeCPU].instructionfetch,0,sizeof(CPU[activeCPU].instructionfetch)); //Finished fetching!
			}
			else //EA cycles still set? We're pending EA!
			{
				goto fetchinginstruction; //EA cycles are timing!
			}
		}
		if (CPU[activeCPU].faultraised) goto skipexecutionOPfault; //Abort on fault!
		newREP = 1; //We're a new repeating instruction!
	}

	//Handle all prefixes!
	if (cpudebugger) debugger_setprefix(""); //Reset prefix for the debugger!
	gotREP = 0; //Default: no REP-prefix used!
	REPZ = 0; //Init REP to REPNZ/Unused zero flag(during REPNE)!
	CPU[activeCPU].REPfinishtiming = 0; //Default: no finish timing!
	if (CPU_getprefix(0xF2)) //REPNE Opcode set?
	{
		gotREP = 1; //We've gotten a repeat!
		REPZ = 0; //Allow and we're not REPZ!
		switch (OP) //Which special adjustment cycles Opcode?
		{
		//80186+ REP opcodes!
		//REPNZ INSB/INSW and REPNZ OUTSB/OUTSW doesn't exist!
		//8086 REPable opcodes!	
		//New:
		case 0xA4: //A4: REPNZ MOVSB
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			break;
		case 0xA5: //A5: REPNZ MOVSW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			break;

		//Old:
		case 0xA6: //A6: REPNZ CMPSB
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			REPZ = 1; //Check the zero flag!
			if (EMULATED_CPU<=CPU_NECV30) CPU[activeCPU].REPfinishtiming += 4; //Finish timing!
			break;
		case 0xA7: //A7: REPNZ CMPSW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			REPZ = 1; //Check the zero flag!
			if (EMULATED_CPU<=CPU_NECV30) CPU[activeCPU].REPfinishtiming += 4; //Finish timing!
			break;

		//New:
		case 0xAA: //AA: REPNZ STOSB
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			break;
		case 0xAB: //AB: REPNZ STOSW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			break;
		case 0xAC: //AC: REPNZ LODSB
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			break;
		case 0xAD: //AD: REPNZ LODSW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			break;

		//Old:
		case 0xAE: //AE: REPNZ SCASB
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			REPZ = 1; //Check the zero flag!
			if (EMULATED_CPU<=CPU_NECV30) CPU[activeCPU].REPfinishtiming += 4; //Finish timing!
			break;
		case 0xAF: //AF: REPNZ SCASW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			REPZ = 1; //Check the zero flag!
			if (EMULATED_CPU<=CPU_NECV30) CPU[activeCPU].REPfinishtiming += 4; //Finish timing!
			break;
		default: //Unknown yet?
		noREPNE0Fand8086: //0F/8086 #UD exception!
			gotREP = 0; //Dont allow after all!
			CPU[activeCPU].cycles_OP = 0; //Unknown!
			break; //Not supported yet!
		}
	}
	else if (CPU_getprefix(0xF3)) //REP/REPE Opcode set?
	{
		gotREP = 1; //Allow!
		REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
		switch (OP) //Which special adjustment cycles Opcode?
		{
		//80186+ REP opcodes!
		case 0x6C: //A4: REP INSB
		case 0x6D: //A4: REP INSW
		case 0x6E: //A4: REP OUTSB
		case 0x6F: //A4: REP OUTSW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			if (EMULATED_CPU < CPU_NECV30) goto noREPNE0Fand8086; //Not existant on 8086!
			break;
			//8086 REP opcodes!
		case 0xA4: //A4: REP MOVSB
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			break;
		case 0xA5: //A5: REP MOVSW
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			break;
		case 0xA6: //A6: REPE CMPSB
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			REPZ = 1; //Check the zero flag!
			if (EMULATED_CPU<=CPU_NECV30) CPU[activeCPU].REPfinishtiming += 4; //Finish timing!
			break;
		case 0xA7: //A7: REPE CMPSW
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			REPZ = 1; //Check the zero flag!
			if (EMULATED_CPU<=CPU_NECV30) CPU[activeCPU].REPfinishtiming += 4; //Finish timing!
			break;
		case 0xAA: //AA: REP STOSB
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			break;
		case 0xAB: //AB: REP STOSW
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			break;
		case 0xAC: //AC: REP LODSB
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			break;
		case 0xAD: //AD: REP LODSW
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			break;
		case 0xAE: //AE: REPE SCASB
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			REPZ = 1; //Check the zero flag!
			if (EMULATED_CPU<=CPU_NECV30) CPU[activeCPU].REPfinishtiming += 4; //Finish timing!
			break;
		case 0xAF: //AF: REPE SCASW
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			REPZ = 1; //Check the zero flag!
			if (EMULATED_CPU<=CPU_NECV30) CPU[activeCPU].REPfinishtiming += 4; //Finish timing!
			break;
		default: //Unknown yet?
			noREPE0Fand8086: //0F exception!
			gotREP = 0; //Don't allow after all!
			break; //Not supported yet!
		}
	}

	if (gotREP) //Gotten REP?
	{
		if (!(CPU_Address_size[activeCPU]?CPU[activeCPU].registers->ECX:CPU[activeCPU].registers->CX)) //REP and finished?
		{
			blockREP = 1; //Block the CPU instruction from executing!
		}
	}

	if (gotREP==0) ++CPU[activeCPU].cycles_OP; //Non-REP adds 1 cycle!

	if (unlikely(cpudebugger)) //Need to set any debugger info?
	{
		if (CPU_getprefix(0xF0)) //LOCK?
		{
			debugger_setprefix("LOCK"); //LOCK!
		}
		if (gotREP) //REPeating something?
		{
			if (CPU_getprefix(0xF2)) //REPNZ?
			{
				debugger_setprefix("REPNZ"); //Set prefix!
			}
			else if (CPU_getprefix(0xF3)) //REP/REPZ?
			{
				if (REPZ) //REPZ?
				{
					debugger_setprefix("REPZ"); //Set prefix!
				}
				else //REP?
				{
					debugger_setprefix("REP"); //Set prefix!
				}
			}
		}
	}

	if (gotREP) CPU[activeCPU].cycles_OP += 4; //rep!
	
	didRepeating = CPU[activeCPU].repeating; //Were we doing REP?
	didNewREP = newREP; //Were we doing a REP for the first time?
	executionphase_running:
	CPU[activeCPU].executed = 1; //Executed by default!
	CPU_OP(); //Now go execute the OPcode once!
	skipexecutionOPfault: //Instruction fetch fault?
	if (CPU[activeCPU].executed) //Are we finished executing?
	{
		//Prepare for the next (fetched or repeated) instruction to start executing!
		CPU[activeCPU].instructionfetch.CPU_isFetching = CPU[activeCPU].instructionfetch.CPU_fetchphase = 1; //Start fetching the next instruction when available(not repeating etc.)!
		//Handle REP instructions post-instruction next!
		/*if (gotREP && !CPU[activeCPU].faultraised)
		{
			CPU[activeCPU].cycles_OP += 2; //finish rep! Both blocking and non-blocking!
		}*/
		if (gotREP && !CPU[activeCPU].faultraised && !blockREP) //Gotten REP, no fault/interrupt has been raised and we're executing?
		{
			if (unlikely(REPZ && (CPU_getprefix(0xF2) || CPU_getprefix(0xF3)))) //REP(N)Z used?
			{
				gotREP &= (FLAG_ZF^CPU_getprefix(0xF2)); //Reset the opcode when ZF doesn't match(needs to be 0 to keep looping).
			}
			if (CPU_Address_size[activeCPU]) //32-bit REP?
			{
				REPcondition = CPU[activeCPU].registers->ECX--; //ECX set and decremented?
			}
			else
			{
				REPcondition = CPU[activeCPU].registers->CX--; //CX set and decremented?
			}
			if (REPcondition && gotREP) //Still looping and allowed? Decrease (E)CX after checking for the final item!
			{
				REPPending = CPU[activeCPU].repeating = 1; //Run the current instruction again and flag repeat!
			}
			else //Finished looping?
			{
				CPU[activeCPU].cycles_OP += CPU[activeCPU].REPfinishtiming; //Apply finishing REP timing!
				if ((didRepeating) && (EMULATED_CPU<=CPU_NECV30)) CPU[activeCPU].cycles_OP += 2; //Additional finish timing!
				CPU[activeCPU].repeating = 0; //Not repeating anymore!
			}
		}
		else
		{
			REPPending = CPU[activeCPU].repeating = 0; //Not repeating anymore!
		}
		blockREP = 0; //Don't block REP anymore!
	}
	fetchinginstruction: //We're still fetching the instruction in some way?
	//Apply the ticks to our real-time timer and BIU!
	//Fall back to the default handler on 80(1)86 systems!
	#ifdef CPU_USECYCLES
	if ((CPU[activeCPU].cycles_OP|CPU[activeCPU].cycles_stallBIU|CPU[activeCPU].cycles_stallBUS|CPU[activeCPU].cycles_EA|CPU[activeCPU].cycles_HWOP|CPU[activeCPU].cycles_Exception) && CPU_useCycles) //cycles entered by the instruction?
	{
		CPU[activeCPU].cycles = CPU[activeCPU].cycles_OP+CPU[activeCPU].cycles_EA+CPU[activeCPU].cycles_HWOP+CPU[activeCPU].cycles_Prefix + CPU[activeCPU].cycles_Exception + CPU[activeCPU].cycles_Prefetch; //Use the cycles as specified by the instruction!
	}
	else //Automatic cycles placeholder?
	{
	#endif
		CPU[activeCPU].cycles = 1; //Default to only 1 cycle at least(no cycles aren't allowed).
	#ifdef CPU_USECYCLES
	}
	//cycles_counted = 1; //Cycles have been counted!
	#endif

	if (CPU[activeCPU].executed) //Are we finished executing?
	{
		CPU_afterexec(); //After executing OPCode stuff!
		CPU[activeCPU].previousopcode = CPU[activeCPU].lastopcode; //Last executed OPcode for reference purposes!
		CPU[activeCPU].previousopcode0F = CPU[activeCPU].is0Fopcode; //Last executed OPcode for reference purposes!
		CPU[activeCPU].previousmodrm = CPU[activeCPU].lastmodrm; //Last executed OPcode for reference purposes!
		CPU[activeCPU].previousCSstart = previousCSstart; //Save the start address of CS for the last instruction!
	}
	BIUWaiting: //The BIU is waiting!
	CPU_tickBIU(); //Tick the prefetch as required!
	flushMMU(); //Flush MMU writes!
}

byte haslower286timingpriority(byte CPUmode,byte ismemory,word lowerindex, word higherindex)
{
	--lowerindex; //We're checking base 0, not base 1!
	--higherindex; //We're checking base 0, not base 1!
	if ((CPUPMTimings[higherindex].CPUmode[CPUmode].ismemory[ismemory].addclock&2)>(CPUPMTimings[lowerindex].CPUmode[CPUmode].ismemory[ismemory].addclock&2)) return 1; //REP over non-REP
	if ((CPUPMTimings[higherindex].CPUmode[CPUmode].ismemory[ismemory].addclock&4)>(CPUPMTimings[lowerindex].CPUmode[CPUmode].ismemory[ismemory].addclock&4)) return 1; //Gate over non-Gate!
	if ((CPUPMTimings[higherindex].CPUmode[CPUmode].ismemory[ismemory].addclock&8)>(CPUPMTimings[lowerindex].CPUmode[CPUmode].ismemory[ismemory].addclock&8)) return 1; //JMP taken over JMP not taken!
	if ((CPUPMTimings[higherindex].CPUmode[CPUmode].ismemory[ismemory].addclock&16)>(CPUPMTimings[lowerindex].CPUmode[CPUmode].ismemory[ismemory].addclock&16)) return 1; //L value of BOUND fits having higher priority!
	if ((CPUPMTimings[higherindex].CPUmode[CPUmode].ismemory[ismemory].addclock&32)>(CPUPMTimings[lowerindex].CPUmode[CPUmode].ismemory[ismemory].addclock&32)) return 1; //L value of BOUND counts having higher priority!
	return 0; //We're equal priority or having higher priority! Don't swap!
}

int lookupTablesCPU = -1; //What lookup table is loaded?

void CPU_initLookupTables() //Initialize the CPU timing lookup tables!
{
	word index; //The index into the main table!
	word sublistindex; //The index in the sublist!
	byte CPUmode; //The used CPU mode!
	byte ismemory; //Memory used in the CPU mode!
	byte is0Fopcode; //0F opcode bit!
	word instruction; //Instruction itself!
	word modrm_register; //The modr/m register used, if any(modr/m specified only)!
	word sublistsize; //Sub-list size!
	word sublist[8]; //All instructions matching this!
	word tempsublist; //Temporary value for swapping items!
	byte latestCPU; //Last supported CPU for this instruction timing!
	byte currentCPU; //The CPU we're emulating, relative to the 80286!
	byte current32; //32-bit opcode?
	byte notfound = 0; //Not found CPU timings?
	if (lookupTablesCPU==(int)EMULATED_CPU) return; //Already loaded? Don't reload when already ready to use!
	lookupTablesCPU = (int)EMULATED_CPU; //We're loading the specified CPU to be active!

	memset(&timing286lookup,0,sizeof(timing286lookup)); //Clear the entire list!

	if (EMULATED_CPU<CPU_80286) //Not a capable CPU for these timings?
	{
		return; //No lookup table to apply, use an empty table!
	}

	currentCPU = EMULATED_CPU-CPU_80286; //The CPU to find in the table!
	for (CPUmode=0;CPUmode<4;++CPUmode) //All CPU modes! Real vs Protected is bit 0, 16-bit vs 32-bit is bit 1!
	{
		for (ismemory=0;ismemory<2;++ismemory) //All memory modes!
		{
			for (is0Fopcode=0;is0Fopcode<2;++is0Fopcode) //All 0F opcode possibilities!
			{
				for (instruction=0;instruction<0x100;++instruction) //All instruction opcodes!
				{
					for (modrm_register=0;modrm_register<8;++modrm_register) //All modr/m variants!
					{
						sublistsize = 0; //Initialize our size to none!
						latestCPU = currentCPU; //Start off with the current CPU that's supported!
						notfound = 1; //Default to not found!
						current32 = (CPUmode>>1); //32-bit opcode?
						try16bit:
						for (;;) //Find the top CPU supported!
						{
							//First, detect the latest supported CPU!
							for (index=0;index<NUMITEMS(CPUPMTimings);++index) //Process all timings available!
							{
								if ((CPUPMTimings[index].CPU==latestCPU) && (CPUPMTimings[index].is0F==is0Fopcode) && (CPUPMTimings[index].is32==current32) && (CPUPMTimings[index].OPcode==(instruction&CPUPMTimings[index].OPcodemask))) //Basic opcode matches?
								{
									if ((CPUPMTimings[index].modrm_reg==0) || (CPUPMTimings[index].modrm_reg==(modrm_register+1))) //MODR/M filter matches to full opcode?
									{
										notfound = 0; //We're found!
										goto topCPUTimingsdetected; //We're detected!
									}
								}
							}
							if (latestCPU==0) goto topCPUTimingsdetected; //Abort when finished!
							--latestCPU; //Check the next CPU!
						}
						topCPUTimingsdetected: //TOP CPU timings detected?
						memset(&sublist,0,sizeof(sublist)); //Clear our sublist!
						if (notfound) //No CPU found matching this instruction?
						{
							if (current32) //32-bit opcode to check?
							{
								current32 = 0; //Try 16-bit opcode instead!
								latestCPU = currentCPU; //Start off with the current CPU that's supported!
								notfound = 1; //Default to not found!
								goto try16bit; //Try the 16-bit variant instead!
							}
							memset(&timing286lookup[CPUmode][ismemory][is0Fopcode][instruction][modrm_register],0,sizeof(timing286lookup[CPUmode][ismemory][is0Fopcode][instruction][modrm_register])); //Unused timings!
						}
						else //Valid CPU found for this instruction?
						{
							//Now, find all items that apply to this instruction!
							for (index=0;index<NUMITEMS(CPUPMTimings);++index) //Process all timings available!
							{
								if ((CPUPMTimings[index].CPU==latestCPU) && (CPUPMTimings[index].is0F==is0Fopcode) && (CPUPMTimings[index].OPcode==(instruction&CPUPMTimings[index].OPcodemask))) //Basic opcode matches?
								{
									if ((CPUPMTimings[index].modrm_reg==0) || (CPUPMTimings[index].modrm_reg==(modrm_register+1))) //MODR/M filter matches to full opcode?
									{
										if (sublistsize<NUMITEMS(sublist)) //Can we even add this item?
										{
											sublist[sublistsize++] = (index+1); //Add the index to the sublist, when possible!
										}
									}
								}
							}
					
							//Now, sort the items in their apropriate order!
							for (index=0;index<(sublistsize-1);++index) //Process all items to sort!
							{
								for (sublistindex=0;sublistindex<(sublistsize-index-1);++sublistindex) //The items to compare!
								{
									if (haslower286timingpriority(CPUmode,ismemory,sublist[sublistindex],sublist[sublistindex+1])) //Do we have lower timing priority (item must be after the item specified)?
									{
										tempsublist = sublist[sublistindex]; //Lower priority index saved!
										sublist[sublistindex] = sublist[sublistindex+1]; //Higher priority index to higher priority position!
										sublist[sublistindex+1] = tempsublist; //Lower priority index to lower priority position!
									}
								}
							}

							//Now, the sublist is filled with items needed for the entry!
							memcpy(&timing286lookup[CPUmode][ismemory][is0Fopcode][instruction][modrm_register],&sublist,MIN(sizeof(sublist),sizeof(timing286lookup[CPUmode][ismemory][is0Fopcode][instruction][modrm_register]))); //Copy the sublist to the active items!
						}
					}
				}
			}
		}
	}
	//The list is now ready for use!
}

void CPU_afterexec() //Stuff to do after execution of the OPCode (cycular tasks etc.)
{
	if (FLAG_TF) //Trapped and to be trapped this instruction?
	{
		if (CPU[activeCPU].trapped && CPU[activeCPU].allowInterrupts && (CPU[activeCPU].allowTF) && ((FLAG_RF==0)||(EMULATED_CPU<CPU_80386)) && (CPU[activeCPU].faultraised==0)) //Are we trapped and allowed to trap?
		{
			CPU_exSingleStep(); //Type-1 interrupt: Single step interrupt!
			if (CPU[activeCPU].trapped) return; //Continue on while handling us!
			CPU_afterexec(); //All after execution fixing!
			return; //Abort: we're finished!
		}
	}

	checkProtectedModeDebuggerAfter(); //Check after executing the current instruction!
}

extern uint_32 destEIP;

void CPU_resetOP() //Rerun current Opcode? (From interrupt calls this recalls the interrupts, handling external calls in between)
{
	CPU[activeCPU].registers->CS = CPU_exec_CS; //CS is reset!
	CPU[activeCPU].registers->EIP = CPU_exec_EIP; //Destination address is reset!
	CPU_flushPIQ(CPU_exec_EIP); //Flush the PIQ, restoring the destination address to the start of the instruction!
}

//Exceptions!

//8086+ exceptions (real mode)

byte tempcycles;

void CPU_exDIV0() //Division by 0!
{
	if (CPU_faultraised(EXCEPTION_DIVIDEERROR)==0)
	{
		return; //Abort handling when needed!
	}
	if (EMULATED_CPU > CPU_8086) //We don't point to the instruction following the division?
	{
		CPU_resetOP(); //Return to the instruction instead!
	}
	//Else: Points to next opcode!

	CPU_executionphase_startinterrupt(EXCEPTION_DIVIDEERROR,0,-1); //Execute INT0 normally using current CS:(E)IP!
}

extern byte HWINT_nr, HWINT_saved; //HW interrupt saved?

void CPU_exSingleStep() //Single step (after the opcode only)
{
	if (CPU_faultraised(EXCEPTION_DEBUG)==0)
	{
		return; //Abort handling when needed!
	}
	HWINT_nr = 1; //Trapped INT NR!
	HWINT_saved = 1; //We're trapped!
	//Points to next opcode!
	tempcycles = CPU[activeCPU].cycles_OP; //Save old cycles!
	if (EMULATED_CPU >= CPU_80386) FLAGW_RF(1); //Automatically set the resume flag on a debugger fault!
	CPU_executionphase_startinterrupt(EXCEPTION_DEBUG,2,-1); //Execute INT1 normally using current CS:(E)IP!
	CPU[activeCPU].trapped = 0; //We're not trapped anymore: we're handling the single-step!
}

void CPU_BoundException() //Bound exception!
{
	//Point to opcode origins!
	if (CPU_faultraised(EXCEPTION_BOUNDSCHECK)==0)
	{
		return; //Abort handling when needed!
	}
	CPU_resetOP(); //Reset instruction to start of instruction!
	tempcycles = CPU[activeCPU].cycles_OP; //Save old cycles!
	CPU_executionphase_startinterrupt(EXCEPTION_BOUNDSCHECK,0,-1); //Execute INT1 normally using current CS:(E)IP!
}

void THROWDESCNM() //#NM exception handler!
{
	//Point to opcode origins!
	if (CPU_faultraised(EXCEPTION_COPROCESSORNOTAVAILABLE)==0) //Throw #NM exception!
	{
		return; //Abort handling when needed!
	}
	CPU_resetOP(); //Reset instruction to start of instruction!
	tempcycles = CPU[activeCPU].cycles_OP; //Save old cycles!
	CPU_executionphase_startinterrupt(EXCEPTION_COPROCESSORNOTAVAILABLE,2,-1); //Execute INT1 normally using current CS:(E)IP! No error code is pushed!
}

void CPU_COOP_notavailable() //COProcessor not available!
{
	THROWDESCNM(); //Same!
}

void THROWDESCMF() //#MF(Coprocessor Error) exception handler!
{
	//Point to opcode origins!
	if (CPU_faultraised(EXCEPTION_COPROCESSORERROR)==0) //Throw #NM exception!
	{
		return; //Abort handling when needed!
	}
	CPU_resetOP(); //Reset instruction to start of instruction!
	tempcycles = CPU[activeCPU].cycles_OP; //Save old cycles!
	CPU_executionphase_startinterrupt(EXCEPTION_COPROCESSORERROR,2,-1); //Execute INT1 normally using current CS:(E)IP! No error code is pushed!
}

void CPU_unkOP() //General unknown OPcode handler!
{
	if (EMULATED_CPU>=CPU_NECV30) //Invalid opcode exception? 8086 just ignores the instruction and continues running!
	{
		unkOP_186(); //Execute the unknown opcode exception handler for the 186+!
	}
}
