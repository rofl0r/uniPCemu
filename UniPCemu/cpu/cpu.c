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
#include "headers/cpu/memory_adressing.h" //CPU_MMU_start support!
#include "headers/cpu/protecteddebugging.h" //Protected debugging support!
#include "headers/cpu/biu.h" //BIU support!

//ALL INTERRUPTS

#include "headers/interrupts/interrupt05.h"
#include "headers/interrupts/interrupt10.h"
#include "headers/interrupts/interrupt11.h"
#include "headers/interrupts/interrupt13.h"
#include "headers/interrupts/interrupt16.h"
#include "headers/interrupts/interrupt18.h"
#include "headers/interrupts/interrupt19.h"

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

byte inboard386_WaitStates = 0; //How many clocks to idle each instruction!

uint_32 getstackaddrsizelimiter()
{
	return STACK_SEGMENT_DESCRIPTOR_B_BIT()? 0xFFFFFFFF : 0xFFFF; //Stack address size!
}

byte checkStackAccess(uint_32 poptimes, byte isPUSH, byte isdword) //How much do we need to POP from the stack?
{
	uint_32 poptimesleft = poptimes; //Load the amount to check!
	uint_32 ESP = CPU[activeCPU].registers->ESP; //Load the stack pointer to verify!
	for (;poptimesleft;) //Anything left?
	{
		//We're at least a word access!
		if (checkMMUaccess(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, ESP&getstackaddrsizelimiter(),isPUSH?0:1,getCPL(),!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (checkMMUaccess(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (ESP+1)&getstackaddrsizelimiter(),isPUSH?0:1,getCPL(),!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (isdword) //DWord?
		{
			if (checkMMUaccess(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (ESP+2)&getstackaddrsizelimiter(),isPUSH?0:1,getCPL(),!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Error accessing memory?
			{
				return 1; //Abort on fault!
			}

			if (checkMMUaccess(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (ESP+3)&getstackaddrsizelimiter(),isPUSH?0:1,getCPL(),!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Error accessing memory?
			{
				return 1; //Abort on fault!
			}
		}
		ESP += isPUSH?stack_pushchange(isdword):stack_popchange(isdword); //Apply the change in virtual (E)SP to check the next value!
		--poptimesleft; //One POP processed!
	}
	return 0; //OK!
}

byte checkENTERStackAccess(uint_32 poptimes, byte isdword) //How much do we need to POP from the stack?
{
	uint_32 poptimesleft = poptimes; //Load the amount to check!
	uint_32 EBP = CPU[activeCPU].registers->EBP; //Load the stack pointer to verify!
	for (;poptimesleft;) //Anything left?
	{
		EBP += stack_popchange(isdword); //Apply the change in virtual (E)BP to check the next value!
		
		//We're at least a word access!
		if (checkMMUaccess(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, EBP&getstackaddrsizelimiter(),1,getCPL(),!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (checkMMUaccess(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (EBP+1)&getstackaddrsizelimiter(),1,getCPL(),!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (isdword) //DWord?
		{
			if (checkMMUaccess(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (EBP+2)&getstackaddrsizelimiter(),1,getCPL(),!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Error accessing memory?
			{
				return 1; //Abort on fault!
			}

			if (checkMMUaccess(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (EBP+3)&getstackaddrsizelimiter(),1,getCPL(),!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Error accessing memory?
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
	if (CPU_Operand_size[activeCPU]==0) //16-bit movement?
	{
		REG_EIP &= 0xFFFF; //Only 16-bits!
	}
	if (REG_EIP>=(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].limit_low|SEGDESC_NONCALLGATE_LIMIT_HIGH(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS]))) //Limit broken?
	{
		THROWDESCGP(0,0,0); //#GP(0) when out of limit range!
	} 
}

void CPU_JMPabs(uint_32 addr)
{
	REG_EIP = addr; //Apply to EIP!
	if (CPU_Operand_size[activeCPU]==0) //16-bit movement?
	{
		REG_EIP &= 0xFFFF; //Only 16-bits!
	}
	if (REG_EIP>=(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].limit_low|SEGDESC_NONCALLGATE_LIMIT_HIGH(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS]))) //Limit broken?
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
	return (CPU_EIPmask()==0xFFFF)?PARAM_IMM16:PARAM_IMM32; //Full mask!
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

void modrm_generateInstructionTEXT(char *instruction, byte debuggersize, uint_32 paramdata, byte type)
{
	if (cpudebugger) //Gotten no debugger to process?
	{
		//Process debugger!
		char result[256];
		cleardata(&result[0],sizeof(result));
		strcpy(result,instruction); //Set the instruction!
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
			case PARAM_MODRM12_IMM8: //param1,param2,imm8
				strcat(result," %s,%s,%02X"); //2 params!
				debugger_setcommand(result,modrm_param1,modrm_param2,paramdata);
				break;
			case PARAM_MODRM12_CL: //param1,param2,CL
				strcat(result," %s,%s,CL"); //2 params!
				debugger_setcommand(result,modrm_param1,modrm_param2);
				break;
			case PARAM_MODRM21: //param2,param1
				strcat(result," %s,%s"); //2 params!
				debugger_setcommand(result,modrm_param2,modrm_param1);
				break;
			case PARAM_MODRM21_IMM8: //param2,param1,imm8
				strcat(result," %s,%s,%02X"); //2 params!
				debugger_setcommand(result,modrm_param2,modrm_param1,paramdata);
				break;
			case PARAM_MODRM21_CL: //param2,param1,CL
				strcat(result," %s,%s,CL"); //2 params!
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

//PORT IN/OUT instructions!
byte CPU_PORT_OUT_B(word port, byte data)
{
	//Check rights!
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if (checkPortRights(port)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR,0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //#GP!
			return 1; //Abort!
		}
	}
	//Execute it!
	byte dummy;
	if (CPU[activeCPU].internalinstructionstep==0) //First step? Request!
	{
		if (BIU_request_BUSwb(port,data)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==1)
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

byte CPU_PORT_OUT_W(word port, word data)
{
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if (checkPortRights(port)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR,0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //#GP!
			return 1; //Abort!
		}
		if (checkPortRights(port+1)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR,0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //#GP!
			return 1; //Abort!
		}
	}
	//Execute it!
	word dummy;
	if (CPU[activeCPU].internalinstructionstep==0) //First step? Request!
	{
		if (BIU_request_BUSww(port,data)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==1)
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

byte CPU_PORT_OUT_D(word port, uint_32 data)
{
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if (checkPortRights(port)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR,0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //#GP!
			return 1; //Abort!
		}
		if (checkPortRights(port + 1)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR,0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //#GP!
			return 1; //Abort!
		}
		if (checkPortRights(port + 2)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR,0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //#GP!
			return 1; //Abort!
		}
		if (checkPortRights(port + 3)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR,0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //#GP!
			return 1; //Abort!
		}
	}
	//Execute it!
	uint_32 dummy;
	if (CPU[activeCPU].internalinstructionstep==0) //First step? Request!
	{
		if (BIU_request_BUSwdw(port,data)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==1)
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

byte CPU_PORT_IN_B(word port, byte *result)
{
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if (checkPortRights(port)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR,0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //#GP!
			return 1; //Abort!
		}
	}
	//Execute it!
	if (CPU[activeCPU].internalinstructionstep==0) //First step? Request!
	{
		if (BIU_request_BUSrb(port)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==1)
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

byte CPU_PORT_IN_W(word port, word *result)
{
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if (checkPortRights(port)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR,0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //#GP!
			return 1; //Abort!
		}
		if (checkPortRights(port + 1)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR,0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //#GP!
			return 1; //Abort!
		}
	}
	//Execute it!
	if (CPU[activeCPU].internalinstructionstep==0) //First step? Request!
	{
		if (BIU_request_BUSrw(port)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==1)
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

byte CPU_PORT_IN_D(word port, uint_32 *result)
{
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if (checkPortRights(port)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR,0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //#GP!
			return 1; //Abort!
		}
		if (checkPortRights(port + 1)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR,0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //#GP!
			return 1; //Abort!
		}
		if (checkPortRights(port + 2)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR,0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //#GP!
			return 1; //Abort!
		}
		if (checkPortRights(port + 3)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR,0,(CPU[activeCPU].registers->TR&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //#GP!
			return 1; //Abort!
		}
	}
	//Execute it!
	if (CPU[activeCPU].internalinstructionstep==0) //First step? Request!
	{
		if (BIU_request_BUSrdw(port)==0) //Not ready?
		{
			CPU[activeCPU].cycles_OP += 1; //Take 1 cycle only!
			CPU[activeCPU].executed = 0; //Not executed!
			return 1; //Keep running!
		}
		++CPU[activeCPU].internalinstructionstep; //Next step!
	}
	if (CPU[activeCPU].internalinstructionstep==1)
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

byte call_soft_inthandler(byte intnr, int_64 errorcode)
{
	//Now call handler!
	CPU[activeCPU].cycles_HWOP += 61; /* Normal interrupt as hardware interrupt */
	calledinterruptnumber = intnr; //Save called interrupt number!
	return CPU_INT(intnr,errorcode); //Call interrupt!
}

void call_hard_inthandler(byte intnr) //Hardware interrupt handler (FROM hardware only, or int>=0x20 for software call)!
{
//Now call handler!
	CPU[activeCPU].cycles_HWOP += 61; /* Normal interrupt as hardware interrupt */
	calledinterruptnumber = intnr; //Save called interrupt number!
	CPU_INT(intnr,-1); //Call interrupt!
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

OPTINLINE void free_CPUregisters()
{
	if (CPU[activeCPU].registers) //Still allocated?
	{
		freez((void **)&CPU[activeCPU].registers, sizeof(*CPU[activeCPU].registers), "CPU_REGISTERS"); //Release the registers if needed!
	}
}

OPTINLINE void CPU_initRegisters() //Init the registers!
{
	uint_32 CSBase; //Base of CS!
	byte CSAccessRights; //Default CS access rights, overwritten during first software reset!
	if (CPU[activeCPU].registers) //Already allocated?
	{
		CSAccessRights = CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].AccessRights; //Save old CS acccess rights to use now (after first reset)!
		free_CPUregisters(); //Free the CPU registers!
	}
	else
	{
		CSAccessRights = 0x93; //Initialise the CS access rights!
	}

	alloc_CPUregisters(); //Allocate the CPU registers!

	if (!CPU[activeCPU].registers) return; //We can't work!
										   //Calculation registers
	CPU[activeCPU].registers->EAX = 0;
	CPU[activeCPU].registers->EBX = 0;
	CPU[activeCPU].registers->ECX = 0;
	CPU[activeCPU].registers->EDX = 0;

	if (EMULATED_CPU>=CPU_80386) //Need revision info in DX?
	{
		CPU[activeCPU].registers->DH = 0x03; //We're a 80386!
		CPU[activeCPU].registers->DL = 0x08; //D1/D2 stepping(latest revision)
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
	CPU_flushPIQ(-1); //We're jumping to another address!
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

	memset(CPU[activeCPU].SEG_DESCRIPTOR, 0, sizeof(CPU[activeCPU].SEG_DESCRIPTOR)); //Clear the descriptor cache!
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
		CPU[activeCPU].registers->CR0 &= 0x7FFF0000; //Clear bit 32 and 4-0!
		CPU[activeCPU].registers->CR0 |= 0xFFF0; //The MSW is initialized to FFF0!
	}
	else //Default or 80386?
	{
		CPU[activeCPU].registers->CR0 &= 0x7FFFFFE0; //Clear bit 32 and 4-0!
		if (EMULATED_CPU >= CPU_80386) //Diffent initialization?
		{
			CPU[activeCPU].registers->CR0 &= ~0xFFFF; //The MSW is initialized to 0000!
		}
	}

	byte reg = 0;
	for (reg = 0; reg<NUMITEMS(CPU[activeCPU].SEG_DESCRIPTOR); reg++) //Process all segment registers!
	{
		//Load Real mode compatible values for all registers!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].base_high = 0;
		CPU[activeCPU].SEG_DESCRIPTOR[reg].base_mid = 0;
		CPU[activeCPU].SEG_DESCRIPTOR[reg].base_low = 0;
		CPU[activeCPU].SEG_DESCRIPTOR[reg].limit_low = 0xFFFF; //64k limit!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].noncallgate_info = 0; //No high limit etc.!
		//According to http://www.sandpile.org/x86/initial.htm the following access rights are used:
		if ((reg == CPU_SEGMENT_LDTR) || (reg == CPU_SEGMENT_TR)) //LDTR&TR=Special case! Apply special access rights!
		{
			CPU[activeCPU].SEG_DESCRIPTOR[reg].AccessRights = 0x82; //Invalid segment!
		}
		else //Normal Code/Data segment?
		{
			CPU[activeCPU].SEG_DESCRIPTOR[reg].AccessRights = 0x93; //Code/data segment, writable!
		}
	}

	//CS specific!
	CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].AccessRights = CSAccessRights; //Load CS default access rights!
	if (EMULATED_CPU>CPU_NECV30) //286+?
	{
		//Pulled low on first load, pulled high on reset:
		if (EMULATED_CPU>CPU_80286) //32-bit CPU?
		{
			CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].base_high = 0xFF; //More than 24 bits are pulled high as well!
		}
		CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].base_mid = 0xFF; //We're starting at the end of our address space, final block! (segment F000=>high 8 bits set)
	}
	else //186-?
	{
		CSBase = CPU[activeCPU].registers->CS<<4; //CS base itself!
		CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].base_mid = (CSBase>>16); //Mid range!
		CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].base_low = (CSBase&0xFFFF); //Low range!
	}
}

void CPU_initLookupTables(); //Initialize the CPU timing lookup tables! Prototype!

void resetCPU() //Initialises the currently selected CPU!
{
	byte i;
	for (i = 0;i < NUMITEMS(CPU);++i) //Process all CPUs!
	{
		CPU[i].allowInterrupts = 1; //Default to allowing all interrupts to run!
	}
	CPU_initRegisters(); //Initialise the registers!
	CPU_initPrefixes(); //Initialise all prefixes!
	CPU_resetMode(); //Reset the mode to the default mode!
	//Default: not waiting for interrupt to occur on startup!
	//Not waiting for TEST pin to occur!
	//Default: not blocked!
	//Continue interrupt call (hardware)?
	CPU[activeCPU].running = 1; //We're running!
	
	CPU[activeCPU].lastopcode = CPU[activeCPU].previousopcode = 0; //Last opcode, default to 0 and unknown?
	generate_opcode_jmptbl(); //Generate the opcode jmptbl for the current CPU!
	generate_opcode0F_jmptbl(); //Generate the opcode 0F jmptbl for the current CPU!
	generate_timings_tbl(); //Generate the timings tables for all CPU's!
	CPU_initLookupTables(); //Initialize our timing lookup tables!
	CPU[activeCPU].CallGateStack = allocfifobuffer(32<<2,0); //Non-lockable 32-bit 32 arguments FIFO buffer for call gate stack parameter copy!
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
}

void initCPU() //Initialize CPU for full system reset into known state!
{
	memset(&CPU, 0, sizeof(CPU)); //Reset the CPU fully!
	resetCPU(); //Reset normally!
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
	CPU_prefixes[activeCPU][(prefix>>3)] |= (128>>(prefix&7)); //Have prefix!
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
	return (CPU_prefixes[activeCPU][prefix>>3]&(128>>(prefix&7)))>0; //Get prefix set or reset!
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
			if (CPU_readOP(OP)) return 1; //Read opcode or prefix?
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
				if (CPU_readOP(OP)) return 1; //Read the actual opcode to use!
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

	//Now, check for the ModR/M byte, if present, and read the parameters if needed!
	timing = &CPUTimings[CPU_Operand_size[activeCPU]][(*OP<<1)|CPU[activeCPU].is0Fopcode]; //Only 2 modes implemented so far, 32-bit or 16-bit mode, with 0F opcode every odd entry!

	if (timing->used==0) goto skiptimings; //Are we not used?
	if (timing->has_modrm && CPU[activeCPU].instructionfetch.CPU_fetchingRM) //Do we have ModR/M data?
	{
		if (modrm_readparams(&params,timing->modrm_size,timing->modrm_specialflags)) return 1; //Read the params!
		CPU[activeCPU].instructionfetch.CPU_fetchparameterPos = 0; //Reset the parameter position again for new parameters!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
		if (MODRM_ERROR(params)) //An error occurred in the read params?
		{
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
						if (CPU_readOP(&immb)) return 1; //Read 8-bit immediate!
						if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
					}
				}
				else //Normal imm8?
				{
					if (CPU_readOP(&immb)) return 1; //Read 8-bit immediate!
					if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
				}
				break;
			case 2: //imm16?
				if (timing->parameters&4) //Only when ModR/M REG<2?
				{
					if (MODRM_REG(params.modrm)<2) //16-bit immediate?
					{
						if (CPU_readOPw(&immw)) return 1; //Read 16-bit immediate!
						if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
					}
				}
				else //Normal imm16?
				{
					if (CPU_readOPw(&immw)) return 1; //Read 16-bit immediate!
					if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
				}
				break;
			case 3: //imm32?
				if (timing->parameters&4) //Only when ModR/M REG<2?
				{
					if (MODRM_REG(params.modrm)<2) //32-bit immediate?
					{
						if (CPU_readOPdw(&imm32)) return 1; //Read 32-bit immediate!
						if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
					}
				}
				else //Normal imm32?
				{
					if (CPU_readOPdw(&imm32)) return 1; //Read 32-bit immediate!
					if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
				}
				break;
			case 8: //imm16 + imm8
				if (CPU[activeCPU].instructionfetch.CPU_fetchparameters==0) //First parameter?
				{
					if (CPU_readOPw(&immw)) return 1; //Read 16-bit immediate!
					if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
					CPU[activeCPU].instructionfetch.CPU_fetchparameters = 1; //Start fetching the second parameter!
					CPU[activeCPU].instructionfetch.CPU_fetchparameterPos = 0; //Init parameter position!
				}
				if (CPU[activeCPU].instructionfetch.CPU_fetchparameters==1) //Second parameter?
				{
					if (CPU_readOP(&immb)) return 1; //Read 8-bit immediate!
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
							if (CPU_readOPdw(&imm32)) return 1; //Read 32-bit immediate offset!
							if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
							imm64 = (uint_64)imm32; //Convert to 64-bit!
							CPU[activeCPU].instructionfetch.CPU_fetchparameters = 1; //Second parameter!
							CPU[activeCPU].instructionfetch.CPU_fetchparameterPos = 0; //Init parameter position!
						}
						if (CPU[activeCPU].instructionfetch.CPU_fetchparameters==1) //Second parameter?
						{
							if (CPU_readOPw(&immw)) return 1; //Read another 16-bit immediate!
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
						if (CPU_readOPdw(&imm32)) return 1; //Read 32-bit immediate offset!
						if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
						imm64 = (uint_64)imm32; //Convert to 64-bit!
						CPU[activeCPU].instructionfetch.CPU_fetchparameters = 1; //Second parameter!
						CPU[activeCPU].instructionfetch.CPU_fetchparameterPos = 0; //Init parameter position!
					}
					if (CPU[activeCPU].instructionfetch.CPU_fetchparameters==1) //Second parameter?
					{
						if (CPU_readOPw(&immw)) return 1; //Read another 16-bit immediate!
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
					if (CPU_readOPdw(&immaddr32)) return 1; //Read 32-bit immediate offset!
					if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
				}
				else //16-bit address?
				{
					if (CPU_readOPw(&immw)) return 1; //Read 32-bit immediate offset!
					immaddr32 = (uint_32)immw; //Convert to 32-bit immediate!
					if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
				}
			default: //Unknown?
				//Ignore the parameters!
				break;
		}
	}

skiptimings: //Skip all timings and parameters(invalid instruction)!
	CPU[activeCPU].instructionstep = CPU[activeCPU].internalinstructionstep = CPU[activeCPU].internalmodrmstep = CPU[activeCPU].stackchecked = 0; //Start the instruction-specific stage!
	return 0; //We're done fetching the instruction!
}

void doneCPU() //Finish the CPU!
{
	free_CPUregisters(); //Finish the allocated registers!
	CPU_doneBIU(); //Finish the BIU!
	free_fifobuffer(&CPU[activeCPU].CallGateStack); //Release our Call Gate Stack space!
}

CPU_registers dummyregisters; //Dummy registers!

//Specs for 80386 says we start in REAL mode!
//STDMODE: 0=protected; 1=real; 2=Virtual 8086.

void CPU_resetMode() //Resets the mode!
{
	if (!CPU[activeCPU].registers) CPU_initRegisters(); //Make sure we have registers!
	//Always start in REAL mode!
	if (!CPU[activeCPU].registers) return; //We can't work now!
	FLAGW_V8(0); //Disable Virtual 8086 mode!
	CPU[activeCPU].registers->CR0 &= ~CR0_PE; //Real mode!
	updateCPUmode(); //Update the CPU mode!
}

byte CPUmode = CPU_MODE_REAL; //The current CPU mode!

void updateCPUmode() //Update the CPU mode!
{
	static const byte modes[4] = { CPU_MODE_REAL, CPU_MODE_PROTECTED, CPU_MODE_REAL, CPU_MODE_8086 }; //All possible modes (VM86 mode can't exist without Protected Mode!)
	byte mode = 0;
	if (!CPU[activeCPU].registers)
	{
		CPU_initRegisters(); //Make sure we have registers!
		if (!CPU[activeCPU].registers) CPU[activeCPU].registers = &dummyregisters; //Dummy registers!
	}
	mode = FLAG_V8; //VM86 mode?
	mode <<= 1;
	mode |= (CPU[activeCPU].registers->CR0&CR0_PE); //Protected mode?
	if ((CPUmode==CPU_MODE_REAL) && (modes[mode]==CPU_MODE_PROTECTED)) //Switching from real mode to protected mode?
	{
		CPU[activeCPU].CPL = 0; //Start at CPL 0!
	}
	else if ((CPUmode!=CPU_MODE_REAL) && (modes[mode]==CPU_MODE_REAL)) //Switching from protected mode, back to real mode?
	{
		CPU[activeCPU].CPL = 0; //Make sure we're CPL 0 in Real mode!
	}
	else if ((CPUmode==CPU_MODE_PROTECTED) && (modes[mode]==CPU_MODE_8086)) //Switching from protected mode to Virtual 8086 mode?
	{
		CPU[activeCPU].CPL = 3; //Make sure we're CPL 3 in Virtual 8086 mode!
	}
	CPUmode = modes[mode]; //Mode levels: Real mode > Protected Mode > VM86 Mode!
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

byte topdown_stack() //Top-down stack?
{
	//We're a 286+, so detect it! Older processors are always in real mode!
	return !(GENERALSEGMENT_TYPE(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_SS]) & 4); //Real mode=8086; Other=SS segment, bit 4 (off=Topdown stack!)
}

//Memory is the same as PSP: 1234h is 34h 12h, in stack terms reversed, because of top-down stack!

//Use below functions for the STACK!

void CPU_PUSH8(byte val) //Push Byte!
{
	word v=val; //Convert!
	CPU_PUSH16(&v); //Push 16!
}

byte CPU_PUSH8_BIU(byte val) //Push Byte!
{
	word v=val; //Convert!
	return CPU_PUSH16_BIU(&v); //Push 16!
}

byte CPU_POP8()
{
	return (CPU_POP16()&0xFF); //Give the result!
}

byte CPU_POP8_BIU() //Request an 8-bit POP from the BIU!
{
	return (CPU_POP16_BIU()); //Give the result: we're requesting from the BIU to POP one entry!
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

void CPU_PUSH16(word *val) //Push Word!
{
	if (EMULATED_CPU<=CPU_NECV30) //186- we push the decremented value of SP to the stack instead of the original value?
	{
		stack_push(0); //We're pushing a 16-bit value!
		MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), *val,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Put value!
	}
	else //286+?
	{
		word oldval = *val; //Original value, saved before decrementing (E)SP!
		stack_push(CODE_SEGMENT_DESCRIPTOR_D_BIT()); //We're pushing a 16-bit or 32-bit value!
		if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
		{
			MMU_wdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), (uint_32)oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Put value!
		}
		else
		{
			MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Put value!
		}
	}
}

byte CPU_PUSH16_BIU(word *val) //Push Word!
{
	if (EMULATED_CPU<=CPU_NECV30) //186- we push the decremented value of SP to the stack instead of the original value?
	{
		if (CPU[activeCPU].pushbusy==0)
		{
			stack_push(0); //We're pushing a 16-bit value!
			CPU[activeCPU].pushbusy = 1; //We're pending!
		}
		if (BIU_request_MMUww(CPU_SEGMENT_SS,(CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()),*val,!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Request Put value!
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
			stack_push(CODE_SEGMENT_DESCRIPTOR_D_BIT()); //We're pushing a 16-bit or 32-bit value!
			CPU[activeCPU].pushbusy = 1; //We're pending!
		}
		if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
		{
			if (BIU_request_MMUwdw(CPU_SEGMENT_SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), (uint_32)oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Request Put value!
			{
				CPU[activeCPU].pushbusy = 0; //We're not pending anymore!
				return 1;
			}
		}
		else
		{
			if (BIU_request_MMUww(CPU_SEGMENT_SS,(CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()),oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Request Put value!
			{
				CPU[activeCPU].pushbusy = 0; //We're not pending anymore!
				return 1;
			}
			//MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Put value!
		}
	}
	return 0; //Not ready yet!
}

word CPU_POP16() //Pop Word!
{
	word result;
	if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
	{
		result = (word)MMU_rdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), 0,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	}
	else //16-bit?
	{
		result = MMU_rw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), 0,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	}
	stack_pop(CODE_SEGMENT_DESCRIPTOR_D_BIT()); //We're popping a 16-bit value!
	return result; //Give the result!
}

byte CPU_POP16_BIU() //Pop Word!
{
	byte result;
	if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
	{
		result = BIU_request_MMUrdw(CPU_SEGMENT_SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()),!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	}
	else //16-bit?
	{
		 result = BIU_request_MMUrw(CPU_SEGMENT_SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), !STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	}
	if (result) //Requested?
	{
		stack_pop(CODE_SEGMENT_DESCRIPTOR_D_BIT()); //We're popping a 16-bit value!
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
		if (BIU_request_MMUww(CPU_SEGMENT_SS,(CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()),*val,!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Request Put value!
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
			stack_push(CODE_SEGMENT_DESCRIPTOR_D_BIT()); //We're pushing a 16-bit or 32-bit value!
			CPU[activeCPU].pushbusy = 1; //We're pending!
		}
		if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
		{
			if (BIU_request_MMUwdw(CPU_SEGMENT_SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Request Put value!
			{
				CPU[activeCPU].pushbusy = 0; //We're not pending anymore!
				return 1;
			}
		}
		else
		{
			if (BIU_request_MMUww(CPU_SEGMENT_SS,(CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()),(word)oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT())) //Request Put value!
			{
				CPU[activeCPU].pushbusy = 0; //We're not pending anymore!
				return 1;
			}
			//MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Put value!
		}
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
		stack_push(CODE_SEGMENT_DESCRIPTOR_D_BIT()); //We're pushing a 32-bit value!
		if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
		{
			MMU_wdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Put value!
		}
		else //16-bit?
		{
			MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), (word)oldval,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Put value!
		}
	}
}

uint_32 CPU_POP32() //Full stack used!
{
	uint_32 result;
	if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
	{
		result = MMU_rdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, CPU[activeCPU].registers->ESP&getstackaddrsizelimiter(), 0,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	}
	else //16-bit?
	{
		result = (uint_32)MMU_rw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, CPU[activeCPU].registers->ESP&getstackaddrsizelimiter(), 0,!STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	}
	stack_pop(CODE_SEGMENT_DESCRIPTOR_D_BIT()); //We're popping a 32-bit value!
	return result; //Give the result!
}

byte CPU_POP32_BIU() //Full stack used!
{
	byte result;
	if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
	{
		result = BIU_request_MMUrdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->ESP&getstackaddrsizelimiter(), !STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	}
	else //16-bit?
	{
		result = BIU_request_MMUrw(CPU_SEGMENT_SS, CPU[activeCPU].registers->ESP&getstackaddrsizelimiter(), !STACK_SEGMENT_DESCRIPTOR_B_BIT()); //Get value!
	}
	if (result) //Requested?
	{
		stack_pop(CODE_SEGMENT_DESCRIPTOR_D_BIT()); //We're popping a 32-bit value!
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

extern Handler CurrentCPU_opcode_jmptbl[1024]; //Our standard internal standard opcode jmptbl!

void CPU_OP(byte OP) //Normal CPU opcode execution!
{
	protection_nextOP(); //Tell the protection exception handlers that we can give faults again!
	CPU[activeCPU].lastopcode = OP; //Last OPcode for reference!
	CurrentCPU_opcode_jmptbl[((word)OP << 2) | (CPU[activeCPU].is0Fopcode<<1) | CPU_Operand_size[activeCPU]](); //Now go execute the OPcode once in the runtime!
	//Don't handle unknown opcodes here: handled by native CPU parser, defined in the opcode jmptbl.
}

void CPU_beforeexec()
{
	//This applies to all processors:
	CPU[activeCPU].trapped = FLAG_TF; //Are we to be trapped this instruction?
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
			tempflags &= 0xFFFF0FFF; //Always set the high flags in real mode only!
		}
		else //Protected mode?
		{
			tempflags &= 0xFFFF7FFF; //Bit 15 is always cleared!
		}
		break;
	case CPU_80386:
		tempflags &= 0x37FFF; //Bit 15 is always cleared! AC is stuck to 0! All bits above AC are always cleared!
		break;
	case CPU_80486:
		tempflags &= 0x277FFF; //Bit 15 is always cleared! Don't allow setting of the CPUID and larger flags! Allow toggling the CPUID flag too(it's supported)!
		break;
	case CPU_PENTIUM:
		//Allow all bits to be set, except the one needed from the 80386+ identification(bit 15=0)!
		tempflags &= 0x3F7FFF;
		break;
	default: //Unknown CPU?
		break;
	}
	tempflags |= 2; //Clear bit values 8&32(unmapped bits 3&5) and set bit value 2!
	CPU[activeCPU].registers->EFLAGS = tempflags; //Update the flags!
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

extern CPUPM_Timings CPUPMTimings[216]; //The PM timings full table!

void CPU_resetTimings()
{
	CPU[activeCPU].cycles_HWOP = 0; //No hardware interrupt to use anymore!
	CPU[activeCPU].cycles_Prefetch_BIU = 0; //Reset cycles spent on BIU!
	CPU[activeCPU].cycles_Prefix = 0; //No cycles prefix to use anymore!
	CPU[activeCPU].cycles_Exception = 0; //No cycles Exception to use anymore!
	CPU[activeCPU].cycles_MMUR = CPU[activeCPU].cycles_MMUW = 0; //No cycles MMU to use anymore!
	CPU[activeCPU].cycles_IO = 0; //Reset cycles spent on the BIU I/O execution!
	CPU[activeCPU].cycles_Prefetch = 0; //No cycles prefetch to use anymore!
	CPU[activeCPU].cycles_OP = 0; //Reset cycles (used by CPU to check for presets (see below))!
	CPU[activeCPU].cycles_Prefetch_DMA = 0; //Reset cycles spent on DMA by the BIU!
	CPU[activeCPU].cycles_EA = 0; //Reset EA cycles!
}

byte REPZ = 0; //Default to REP!
byte didNewREP = 0, didRepeating=0; //Did we do a REP?

byte CPU_apply286cycles() //Apply the 80286+ cycles method. Result: 0 when to apply normal cycles. 1 when 80286+ cycles are applied!
{
	if (EMULATED_CPU<CPU_80286) return 0; //Not applied on unsupported processors!
	word *currentinstructiontiming; //Current timing we're processing!
	byte instructiontiming, ismemory, modrm_threevariablesused; //Timing loop used on 286+ CPUs!
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
		currentinstructiontiming = &CPU[activeCPU].timing286lookup[isPM()][0][0][0xCD][0x00][0]; //Start by pointing to our records to process! Enforce interrupt!
	}
	else
	{
		currentinstructiontiming = &CPU[activeCPU].timing286lookup[isPM()][ismemory][CPU[activeCPU].is0Fopcode][CPU[activeCPU].lastopcode][MODRM_REG(params.modrm)][0]; //Start by pointing to our records to process!
	}
	//Try to use the lookup table!
	for (instructiontiming=0;((instructiontiming<8)&&*currentinstructiontiming);++instructiontiming, ++currentinstructiontiming) //Process all timing candidates!
	{
		if (*currentinstructiontiming) //Valid timing?
		{
			if (CPUPMTimings[*currentinstructiontiming].CPUmode[isPM()|(CPU_Operand_size[activeCPU])].ismemory[ismemory].basetiming) //Do we have valid timing to use?
			{
				currenttimingcheck = &CPUPMTimings[*currentinstructiontiming].CPUmode[isPM()|(CPU_Operand_size[activeCPU]<<1)].ismemory[ismemory]; //Our current info to check!
				if (currenttimingcheck->addclock&0x20) //L of instruction doesn't fit in 1 bit?
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

void CPU_exec() //Processes the opcode at CS:EIP (386) or CS:IP (8086).
{
	REPZ = 0; //Default to REP!
	didNewREP = 0; didRepeating=0; //Did we do a REP?
	static uint_32 previousCSstart;
	static char debugtext[256]; //Debug text!
	//byte cycles_counted = 0; //Cycles have been counted?
	if (CPU[activeCPU].instructionfetch.CPU_isFetching && (CPU[activeCPU].instructionfetch.CPU_fetchphase==1)) //Starting a new instruction?
	{
		CPU[activeCPU].allowInterrupts = 1; //Allow interrupts again after this instruction!
		CPU[activeCPU].allowTF = 1; //Default: allow TF to be triggered after the instruction!
		CPU[activeCPU].debuggerFaultRaised = 0; //Default: no debugger fault raised!
		bufferMMU(); //Buffer the MMU writes for us!
		MMU_clearOP(); //Clear the OPcode buffer in the MMU (equal to our instruction cache)!
		debugger_beforeCPU(); //Everything that needs to be done before the CPU executes!
		MMU_resetaddr(); //Reset invalid address for our usage!
		CPU_8086REPPending(); //Process pending REP!

		previousCSstart = CPU_MMU_start(CPU_SEGMENT_CS,CPU[activeCPU].registers->CS); //Save the used CS start address!

		if (CPU[activeCPU].permanentreset) //We've entered a permanent reset?
		{
			CPU[activeCPU].cycles = 4; //Small cycle dummy! Must be greater than zero!
			return; //Don't run the CPU: we're in a permanent reset state!
		}

		CPU[activeCPU].have_oldESP = 0; //Default: no ESP to return to during exceptions!
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
		CPU[activeCPU].segment_register = CPU_SEGMENT_DEFAULT; //Default data segment register (default: auto)!
		if (!CPU[activeCPU].repeating) //Not repeating instructions?
		{
			#ifdef CPU_SAVELAST
			//Save the last coordinates!
			CPU_exec_lastCS = CPU_exec_CS;
			CPU_exec_lastEIP = CPU_exec_lastEIP;
			#endif
			CPU_exec_CS = CPU[activeCPU].registers->CS; //CS of command!
			CPU_exec_EIP = CPU[activeCPU].registers->EIP; //EIP of command!
		}
	
		//Save the starting point when debugging!
		CPU_debugger_CS = CPU_exec_CS;
		CPU_debugger_EIP = CPU_exec_EIP;

		if (getcpumode()!=CPU_MODE_REAL) //Protected mode?
		{
			if (checkProtectedModeDebugger(previousCSstart+CPU_exec_EIP,PROTECTEDMODEDEBUGGER_TYPE_EXECUTION)) //Breakpoint at the current address(linear address space)?
			{
				return; //Protected mode debugger activated! Don't fetch or execute!
			}
			if (GENERALSEGMENT_P(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_TR]) && CPU[activeCPU].registers->TR) //Active task?
			{
				if (MMU_rw(CPU_SEGMENT_TR,CPU[activeCPU].registers->TR,0,1,0)&1) //Trace bit set? Cause a debug exception when this context is run?
				{
					SETBITS(CPU[activeCPU].registers->DR6,15,1,1); //Set bit 15, the new task's T-bit: we're trapping this instruction when this context is to be run!
					CPU_INT(1,-1); //Call the interrupt, no error code!
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
	}
	else //Not a repeating instruction?
	{
		if (CPU[activeCPU].instructionfetch.CPU_isFetching) //Are we fetching?
		{
			CPU[activeCPU].executed = 0; //Not executed yet!
			if (CPU_readOP_prefix(&OP)) //Finished 
			{
				if (!CPU[activeCPU].cycles_OP) CPU[activeCPU].cycles_OP = 1; //Take 1 cycle by default!
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
	if (cpudebugger) debugger_setprefix(""); //Reset prefix for the debugger!
	gotREP = 0; //Default: no REP-prefix used!
	REPZ = 0; //Init REP to NZ!
	if (CPU_getprefix(0xF2)) //REPNE Opcode set?
	{
		gotREP = REPZ = 1; //Allow and we're REPZ!
		switch (OP) //Which special adjustment cycles Opcode?
		{
		//80186+ REP opcodes!
		//REPNZ INSB/INSW and REPNZ OUTSB/OUTSW doesn't exist!
		//8086 REPable opcodes!	
		//New:
		case 0xA4: //A4: REPNZ MOVSB
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
 			REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
			break;
		case 0xA5: //A5: REPNZ MOVSW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
			break;

		//Old:
		case 0xA6: //A6: REPNZ CMPSB
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			break;
		case 0xA7: //A7: REPNZ CMPSW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			break;

		//New:
		case 0xAA: //AA: REPNZ STOSB
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
			break;
		case 0xAB: //AB: REPNZ STOSW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
			break;
		case 0xAC: //AC: REPNZ LODSB
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
			break;
		case 0xAD: //AD: REPNZ LODSW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
			break;

		//Old:
		case 0xAE: //AE: REPNZ SCASB
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			break;
		case 0xAF: //AF: REPNZ SCASW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
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
		switch (OP) //Which special adjustment cycles Opcode?
		{
		//80186+ REP opcodes!
		case 0x6C: //A4: REP INSB
		case 0x6D: //A4: REP INSW
		case 0x6E: //A4: REP OUTSB
		case 0x6F: //A4: REP OUTSW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			if (EMULATED_CPU < CPU_NECV30) goto noREPNE0Fand8086; //Not existant on 8086!
			REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
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
			REPZ = 1; //REPE/REPZ!
			break;
		case 0xA7: //A7: REPE CMPSW
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			REPZ = 1; //REPE/REPZ!
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
			REPZ = 1; //REPE/REPZ!
			break;
		case 0xAF: //AF: REPE SCASW
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			REPZ = 1; //REPE/REPZ!
			break;
		default: //Unknown yet?
			noREPE0Fand8086: //0F exception!
			gotREP = 0; //Don't allow after all!
			break; //Not supported yet!
		}
	}

	if (gotREP) //Gotten REP?
	{
		if (cpudebugger) //Need to set any debugger info?
		{
			if (CPU_getprefix(0xF0)) //LOCK?
			{
				debugger_setprefix("LOCK"); //LOCK!
			}
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
		if (!CPU[activeCPU].registers->CX) //REP and finished?
		{
			blockREP = 1; //Block the CPU instruction from executing!
		}
	}
	didRepeating = CPU[activeCPU].repeating; //Were we doing REP?
	didNewREP = newREP; //Were we doing a REP for the first time?
	CPU[activeCPU].executed = 1; //Executed by default!
	CPU_OP(OP); //Now go execute the OPcode once!
	skipexecutionOPfault: //Instruction fetch fault?
	if (CPU[activeCPU].executed) //Are we finished executing?
	{
		CPU[activeCPU].instructionfetch.CPU_isFetching = CPU[activeCPU].instructionfetch.CPU_fetchphase = 1; //Start fetching the next instruction when available(not repeating etc.)!
		if (gotREP && !CPU[activeCPU].faultraised && !blockREP) //Gotten REP, no fault has been raised and we're executing?
		{
			if (CPU_getprefix(0xF2)) //REPNZ?
			{
				if (REPZ) //Check for zero flag?
				{
					gotREP &= (FLAG_ZF ^ 1); //To reset the opcode (ZF needs to be cleared to loop)?
				}
			}
			else if (CPU_getprefix(0xF3) && REPZ) //REPZ?
			{
				gotREP &= FLAG_ZF; //To reset the opcode (ZF needs to be set to loop)?
			}
			if (CPU[activeCPU].registers->CX-- && gotREP) //Still looping and allowed? Decrease CX after checking for the final item!
			{
				REPPending = CPU[activeCPU].repeating = 1; //Run the current instruction again and flag repeat!
			}
			else
			{
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
	if ((CPU[activeCPU].cycles_OP|CPU[activeCPU].cycles_EA|CPU[activeCPU].cycles_HWOP|CPU[activeCPU].cycles_Exception) && CPU_useCycles) //cycles entered by the instruction?
	{
		CPU[activeCPU].cycles = CPU[activeCPU].cycles_OP+CPU[activeCPU].cycles_EA+CPU[activeCPU].cycles_HWOP+CPU[activeCPU].cycles_Prefix + CPU[activeCPU].cycles_Exception + CPU[activeCPU].cycles_Prefetch + CPU[activeCPU].cycles_MMUR + CPU[activeCPU].cycles_MMUW + CPU[activeCPU].cycles_IO; //Use the cycles as specified by the instruction!
	}
	else //Automatic cycles placeholder?
	{
	#endif
		CPU[activeCPU].cycles = 1; //Default to only 1 cycle at least(no cycles aren't allowed).
	#ifdef CPU_USECYCLES
	}
	//cycles_counted = 1; //Cycles have been counted!
	CPU[activeCPU].cycles += inboard386_WaitStates; //Add 80386 WaitStates to slow us down!
	#endif

	if (CPU[activeCPU].executed) //Are we finished executing?
	{
		CPU_afterexec(); //After executing OPCode stuff!
		CPU[activeCPU].previousopcode = CPU[activeCPU].lastopcode; //Last executed OPcode for reference purposes!
		CPU[activeCPU].previousopcode0F = CPU[activeCPU].is0Fopcode; //Last executed OPcode for reference purposes!
		CPU[activeCPU].previousCSstart = previousCSstart; //Save the start address of CS for the last instruction!
	}
	CPU_tickBIU(); //Tick the prefetch as required!
	flushMMU(); //Flush MMU writes!
}

byte haslower286timingpriority(byte CPUmode,byte ismemory,word lowerindex, word higherindex)
{
	if ((CPUPMTimings[higherindex].CPUmode[CPUmode].ismemory[ismemory].addclock&2)>(CPUPMTimings[lowerindex].CPUmode[CPUmode].ismemory[ismemory].addclock&2)) return 1; //REP over non-REP
	if ((CPUPMTimings[higherindex].CPUmode[CPUmode].ismemory[ismemory].addclock&4)>(CPUPMTimings[lowerindex].CPUmode[CPUmode].ismemory[ismemory].addclock&4)) return 1; //Gate over non-Gate!
	if ((CPUPMTimings[higherindex].CPUmode[CPUmode].ismemory[ismemory].addclock&8)>(CPUPMTimings[lowerindex].CPUmode[CPUmode].ismemory[ismemory].addclock&8)) return 1; //JMP taken over JMP not taken!
	if ((CPUPMTimings[higherindex].CPUmode[CPUmode].ismemory[ismemory].addclock&16)>(CPUPMTimings[lowerindex].CPUmode[CPUmode].ismemory[ismemory].addclock&16)) return 1; //L value of BOUND fits having higher priority!
	if ((CPUPMTimings[higherindex].CPUmode[CPUmode].ismemory[ismemory].addclock&32)>(CPUPMTimings[lowerindex].CPUmode[CPUmode].ismemory[ismemory].addclock&32)) return 1; //L value of BOUND counts having higher priority!
	return 0; //We're equal priority or having higher priority! Don't swap!
}

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
	currentCPU = EMULATED_CPU-CPU_80286; //The CPU to find in the table!

	memset(&CPU[activeCPU].timing286lookup,0,sizeof(CPU[activeCPU].timing286lookup)); //Clear the entire list!

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
							memset(&CPU[activeCPU].timing286lookup[CPUmode][ismemory][is0Fopcode][instruction][modrm_register],0,sizeof(CPU[activeCPU].timing286lookup[CPUmode][ismemory][is0Fopcode][instruction][modrm_register])); //Unused timings!
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
							for (index=0;index<sublistsize;++index) //Process all items to sort!
							{
								for (sublistindex=index+1;sublistindex<sublistsize;++sublistindex) //The items to compare!
								{
									if (haslower286timingpriority(CPUmode,ismemory,sublist[index],sublist[sublistindex])) //Do we have lower timing priority (item must be after the item specified)?
									{
										tempsublist = sublist[index]; //Lower priority index saved!
										sublist[index] = sublist[sublistindex]; //Higher priority index to higher priority position!
										sublist[sublistindex] = tempsublist; //Lower priority index to lower priority position!
									}
								}
							}

							//Now, the sublist is filled with items needed for the entry!
							memcpy(&CPU[activeCPU].timing286lookup[CPUmode][ismemory][is0Fopcode][instruction][modrm_register],&sublist,MIN(sizeof(sublist),sizeof(CPU[activeCPU].timing286lookup[CPUmode][ismemory][is0Fopcode][instruction][modrm_register]))); //Copy the sublist to the active items!
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
	CPU[activeCPU].faultraised = 0; //We don't have a fault anymore! Continue on!

	if (FLAG_TF) //Trapped and to be trapped this instruction?
	{
		if (CPU[activeCPU].trapped && CPU[activeCPU].allowInterrupts && (CPU[activeCPU].allowTF)) //Are we trapped and allowed to trap?
		{
			CPU_exSingleStep(); //Type-1 interrupt: Single step interrupt!
			CPU_afterexec(); //All after execution fixing!
			return; //Abort: we're finished!
		}
	}

	if (getcpumode()!=CPU_MODE_REAL) //Debugging pending to fire to be handled?
	{
		checkProtectedModeDebuggerAfter(); //Check after executing the current instruction!
	}
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
	tempcycles = CPU[activeCPU].cycles_OP; //Save old cycles!
	if (CPU_faultraised(EXCEPTION_DIVIDEERROR))
	{
		if (EMULATED_CPU == CPU_8086) //We point to the instruction following the division?
		{
			//Points to next opcode!
			call_soft_inthandler(EXCEPTION_DIVIDEERROR,-1); //Execute INT0 normally using current CS:(E)IP!
		}
		else
		{
			//Points to next opcode!
			CPU_customint(EXCEPTION_DIVIDEERROR,CPU_exec_CS,CPU_exec_EIP,-1); //Return to opcode!
		}
	}
	CPU[activeCPU].cycles_Exception += CPU[activeCPU].cycles_OP; //Our cycles are counted as a hardware interrupt's cycles instead!
	CPU[activeCPU].cycles_OP = tempcycles; //Restore cycles!
}

extern byte HWINT_nr, HWINT_saved; //HW interrupt saved?

void CPU_exSingleStep() //Single step (after the opcode only)
{
	tempcycles = CPU[activeCPU].cycles_OP; //Save old cycles!
	HWINT_nr = 1; //Trapped INT NR!
	HWINT_saved = 1; //We're trapped!
	//Points to next opcode!
	if (CPU_faultraised(EXCEPTION_DEBUG))
	{
		call_soft_inthandler(EXCEPTION_DEBUG,-1); //Execute INT1 normally using current CS:(E)IP!
	}
	CPU[activeCPU].cycles_Exception += 50; //Our cycles!
	CPU[activeCPU].cycles_OP = tempcycles; //Restore cycles!
}

void CPU_BoundException() //Bound exception!
{
	tempcycles = CPU[activeCPU].cycles_OP; //Save old cycles!
	//Point to opcode origins!
	if (CPU_faultraised(EXCEPTION_BOUNDSCHECK))
	{
		CPU_customint(EXCEPTION_BOUNDSCHECK,CPU_exec_CS,CPU_exec_EIP,0); //Return to opcode!
	}
	CPU[activeCPU].cycles_Exception += CPU[activeCPU].cycles_OP; //Our cycles are counted as a hardware interrupt's cycles instead!
	CPU[activeCPU].cycles_OP = tempcycles; //Restore cycles!
}

void CPU_COOP_notavailable() //COProcessor not available!
{
	tempcycles = CPU[activeCPU].cycles_OP; //Save old cycles!
	//Point to opcode origins!
	if (CPU_faultraised(EXCEPTION_COPROCESSORNOTAVAILABLE))
	{
		CPU_customint(EXCEPTION_COPROCESSORNOTAVAILABLE,CPU_exec_CS,CPU_exec_EIP,0); //Return to opcode!
	}
	CPU[activeCPU].cycles_Exception += CPU[activeCPU].cycles_OP; //Our cycles are counted as a hardware interrupt's cycles instead!
	CPU[activeCPU].cycles_OP = tempcycles; //Restore cycles!
}

void CPU_unkOP() //General unknown OPcode handler!
{
	if (EMULATED_CPU>=CPU_NECV30) //Invalid opcode exception? 8086 just ignores the instruction and continues running!
	{
		unkOP_186(); //Execute the unknown opcode exception handler for the 186+!
	}
}
