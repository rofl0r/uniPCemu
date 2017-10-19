#include "headers/types.h" //Basic types!
#include "headers/cpu/cpu.h" //Basic CPU!
#include "headers/cpu/easyregs.h" //Easy registers!
#include "headers/cpu/modrm.h" //MODR/M compatibility!
#include "headers/support/signedness.h" //CPU support functions!
#include "headers/hardware/ports.h" //Ports compatibility!
#include "headers/emu/debugger/debugger.h" //Debug compatibility!
#include "headers/cpu/cpu_OP8086.h" //8086 function specific compatibility!
#include "headers/cpu/8086_grpOPs.h" //GRP Opcode support (C0&C1 Opcodes!)
#include "headers/cpu/cpu_OPNECV30.h" //NECV30 function specific compatibility!
#include "headers/support/log.h" //Logging support!
#include "headers/cpu/protection.h" //Protection support!
#include "headers/mmu/mmuhandler.h" //MMU_invaddr support!
#include "headers/cpu/flags.h" //Flags support!

extern MODRM_PARAMS params;    //For getting all params!
extern byte blockREP; //Block the instruction from executing (REP with (E)CX=0
extern byte MODRM_src0; //What source is our modr/m? (1/2)
extern byte MODRM_src1; //What source operand in our modr/m? (2/2)
MODRM_PTR info, info2; //For storing ModR/M Info(second for 186+ IMUL instructions)!

extern byte immb; //Immediate byte!
extern word immw; //Immediate word!

/*

New instructions:

ENTER
LEAVE
PUSHA
POPA
BOUND
IMUL
INS
OUTS

*/

//We're still 16-bits!

//Info: Gv,Ev=See 8086 opcode map; Ib/w=Immediate, Iz=Iw/Idw.

extern byte oper1b, oper2b; //Byte variants!
extern word oper1, oper2; //Word variants!
extern byte res8; //Result 8-bit!
extern word res16; //Result 16-bit!
extern byte thereg; //For function number!
extern uint_32 ea; //From RM offset (GRP5 Opcodes only!)

extern VAL32Splitter temp1, temp2, temp3, temp4, temp5;

//Help functions for debugging:
extern char modrm_param1[256]; //Contains param/reg1
extern char modrm_param2[256]; //Contains param/reg2
extern byte cpudebugger; //CPU debugger active?
extern byte custommem; //Custom memory address?

extern uint_32 customoffset; //Offset to use!

extern uint_32 destEIP; //For control transfers!

OPTINLINE void CPU186_internal_MOV16(word *dest, word val) //Copy of 8086 version!
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
		if (dest) //Register?
		{
			destEIP = REG_EIP; //Store (E)IP for safety!
			modrm_updatedsegment(dest, val, 0); //Check for an updated segment!
			CPUPROT1
			*dest = val; //Write directly, if not errored out!
			CPU_apply286cycles(); //Apply the 80286+ cycles!
			CPUPROT2
		}
		else //Memory?
		{
			if (custommem)
			{
				if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), customoffset,0,getCPL(),!CPU_Address_size[activeCPU],0|0x8)) return; //Abort on fault!
				if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), customoffset+1,0,getCPL(),!CPU_Address_size[activeCPU],1|0x8)) return; //Abort on fault!
				if (CPU8086_internal_stepwritedirectw(0,CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), customoffset, val,!CPU_Address_size[activeCPU])) return; //Write to memory directly!
				CPU_apply286cycles(); //Apply the 80286+ cycles!
			}
			else //ModR/M?
			{
				if (CPU8086_internal_stepwritemodrmw(0,val,MODRM_src0,0)) return; //Write the result to memory!
				CPU_apply286cycles(); //Apply the 80286+ cycles!
			}
		}
	CPUPROT2
}

//BCD opcodes!
OPTINLINE void CPU186_internal_AAA()
{
	CPUPROT1
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
	//flag_szp8(REG_AL); //Basic flags!
	flag_p8(REG_AL); //Parity is affected!
	FLAGW_ZF((REG_AL==0)?1:0); //Zero is affected!
	FLAGW_SF(0); //Clear Sign!
	//z=s=p=o=?
	CPUPROT2
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		CPU[activeCPU].cycles_OP += 4; //Timings!
	}
}
OPTINLINE void CPU186_internal_AAS()
{
	CPUPROT1
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
	//flag_szp8(REG_AL); //Basic flags!
	flag_p8(REG_AL); //Parity is affected!
	FLAGW_ZF((REG_AL==0)?1:0); //Zero is affected!
	FLAGW_SF(0); //Sign is cleared!
	//z=s=o=p=?
	CPUPROT2
	if (CPU_apply286cycles()==0) //No 80286+ cycles instead?
	{
		CPU[activeCPU].cycles_OP += 4; //Timings!
	}
}

//Newer versions of the BCD instructions!
void CPU186_OP37() {modrm_generateInstructionTEXT("AAA",0,0,PARAM_NONE);/*AAA?*/ CPU186_internal_AAA();/*AAA?*/ }
void CPU186_OP3F() {modrm_generateInstructionTEXT("AAS",0,0,PARAM_NONE);/*AAS?*/ CPU186_internal_AAS();/*AAS?*/ }

/*

New opcodes for 80186+!

*/

void CPU186_OP60()
{
	debugger_setcommand("PUSHA");
	if (CPU[activeCPU].instructionstep==0) if (checkStackAccess(8,1,0)) return; //Abort on fault!
	static word oldSP;
	oldSP = (word)CPU[activeCPU].oldESP;    //PUSHA
	if (CPU8086_PUSHw(0,&REG_AX,0)) return;
	CPUPROT1
	if (CPU8086_PUSHw(2,&REG_CX,0)) return;
	CPUPROT1
	if (CPU8086_PUSHw(4,&REG_DX,0)) return;
	CPUPROT1
	if (CPU8086_PUSHw(6,&REG_BX,0)) return;
	CPUPROT1
	if (CPU8086_PUSHw(8,&oldSP,0)) return;
	CPUPROT1
	if (CPU8086_PUSHw(10,&REG_BP,0)) return;
	CPUPROT1
	if (CPU8086_PUSHw(12,&REG_SI,0)) return;
	CPUPROT1
	if (CPU8086_PUSHw(14,&REG_DI,0)) return;
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPU_apply286cycles(); //Apply the 80286+ cycles!
}

void CPU186_OP61()
{
	word dummy;
	debugger_setcommand("POPA");
	if (CPU[activeCPU].instructionstep==0) if (checkStackAccess(8,0,0)) return; //Abort on fault!
	if (CPU8086_POPw(0,&REG_DI,0)) return;
	CPUPROT1
	if (CPU8086_POPw(2,&REG_SI,0)) return;
	CPUPROT1
	if (CPU8086_POPw(4,&REG_BP,0)) return;
	CPUPROT1
	if (CPU8086_POPw(6,&dummy,0)) return;
	CPUPROT1
	if (CPU8086_POPw(8,&REG_BX,0)) return;
	CPUPROT1
	if (CPU8086_POPw(10,&REG_DX,0)) return;
	CPUPROT1
	if (CPU8086_POPw(12,&REG_CX,0)) return;
	CPUPROT1
	if (CPU8086_POPw(14,&REG_AX,0)) return;
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPU_apply286cycles(); //Apply the 80286+ cycles!
}

extern byte modrm_addoffset; //Add this offset to ModR/M reads!

//62 not implemented in fake86? Does this not exist?
void CPU186_OP62()
{
	modrm_debugger16(&params,MODRM_src0,MODRM_src1); //Debug the location!
	debugger_setcommand("BOUND %s,%s",modrm_param1,modrm_param2); //Opcode!

	if (modrm_isregister(params)) //ModR/M may only be referencing memory?
	{
		unkOP_186(); //Raise #UD!
		return; //Abort!
	}

	static word bound_min, bound_max;
	static word theval;
	modrm_addoffset = 0; //No offset!
	if (modrm_check16(&params,MODRM_src0,1)) return; //Abort on fault!
	if (modrm_check16(&params,MODRM_src1,1)) return; //Abort on fault!
	modrm_addoffset = 2; //Max offset!
	if (modrm_check16(&params,MODRM_src1,1)) return; //Abort on fault!

	modrm_addoffset = 0; //No offset!
	if (CPU8086_instructionstepreadmodrmw(0,&theval,MODRM_src0)) return; //Read index!
	if (CPU8086_instructionstepreadmodrmw(2,&bound_min,MODRM_src1)) return; //Read min!
	modrm_addoffset = 2; //Max offset!
	if (CPU8086_instructionstepreadmodrmw(4,&bound_max,MODRM_src1)) return; //Read max!
	modrm_addoffset = 0; //Reset offset!
	if ((unsigned2signed16(theval)<unsigned2signed16(bound_min)) || (unsigned2signed16(theval)>unsigned2signed16(bound_max)))
	{
		//BOUND Gv,Ma
		CPU_BoundException(); //Execute bound exception!
	}
	else //No exception?
	{
		CPU_apply286cycles(); //Apply the 80286+ cycles!
	}
}

void CPU186_OP68()
{
	word val = immw;    //PUSH Iz
	debugger_setcommand("PUSH %04X",val);
	if (checkStackAccess(1,1,0)) return; //Abort on fault!
	if (CPU8086_PUSHw(0,&val,0)) return; //PUSH!
	CPU_apply286cycles(); //Apply the 80286+ cycles!
}

void CPU186_OP69()
{
	memcpy(&info,&params.info[MODRM_src0],sizeof(info)); //Reg!
	memcpy(&info2,&params.info[MODRM_src1],sizeof(info2)); //Second parameter(R/M)!
	if (MODRM_MOD(params.modrm)==3) //Two-operand version?
	{
		debugger_setcommand("IMUL %s,%04X",info.text,immw); //IMUL reg,imm16
	}
	else //Three-operand version?
	{
		debugger_setcommand("IMUL %s,%s,%04X",info.text,info2.text,immw); //IMUL reg,r/m16,imm16
	}
	if (CPU[activeCPU].instructionstep==0) //First step?
	{
		if (MODRM_MOD(params.modrm)!=3) //Use R/M to calculate the result(Three-operand version)?
		{
			if (modrm_check16(&params,1,1)) return; //Abort on fault!
			if (CPU8086_instructionstepreadmodrmw(0,&temp1.val16,1)) return; //Read R/M!
			temp1.val16high = 0; //Clear high part by default!
		}
		else
		{
			if (CPU8086_instructionstepreadmodrmw(0,&temp1.val16,0)) return; //Read reg instead! Word register = Word register * imm16!
			temp1.val16high = 0; //Clear high part by default!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	if (CPU[activeCPU].instructionstep==1) //Second step?
	{
		temp2.val32 = immw; //Immediate word is second/third parameter!
		if ((temp1.val32 &0x8000)==0x8000) temp1.val32 |= 0xFFFF0000;
		if ((temp2.val32 &0x8000)==0x8000) temp2.val32 |= 0xFFFF0000;
		temp3.val32s = temp1.val32s; //Load and...
		temp3.val32s *= temp2.val32s; //Signed multiplication!
		CPU_apply286cycles(); //Apply the 80286+ cycles!
		//We're writing to the register always, so no normal writeback!
		++CPU[activeCPU].instructionstep; //Next step!
	}
	modrm_write16(&params,MODRM_src0,temp3.val16,0); //Write to the destination(register)!
	if (((temp3.val32>>15)==0) || ((temp3.val32>>15)==0x1FFFF)) FLAGW_OF(0); //Overflow flag is cleared when high word is a sign extension of the low word!
	else FLAGW_OF(1);
	FLAGW_CF(FLAG_OF); //OF=CF!
	FLAGW_SF((temp3.val16&0x8000)>>15); //Sign!
	FLAGW_PF(parity[temp3.val16&0xFF]); //Parity flag!
	FLAGW_ZF((temp3.val16==0)?1:0); //Set the zero flag!
}

void CPU186_OP6A()
{
	word val = immb; //Read the value!
	if (val&0x80) val |= 0xFF00; //Sign-extend!
	debugger_setcommand("PUSHB %02X",(val&0xFF)); //PUSH this!
	if (checkStackAccess(1,1,0)) return; //Abort on fault!
	if (CPU8086_PUSHw(0,&val,0)) return;    //PUSH Ib
	CPU_apply286cycles(); //Apply the 80286+ cycles!
}

void CPU186_OP6B()
{
	memcpy(&info,&params.info[MODRM_src0],sizeof(info)); //Reg!
	memcpy(&info2,&params.info[MODRM_src1],sizeof(info2)); //Second parameter(R/M)!
	if (MODRM_MOD(params.modrm)==3) //Two-operand version?
	{
		debugger_setcommand("IMUL %s,%02X",info.text,immb); //IMUL reg,imm8
	}
	else //Three-operand version?
	{
		debugger_setcommand("IMUL %s,%s,%02X",info.text,info2.text,immb); //IMUL reg,r/m16,imm8
	}

	if (CPU[activeCPU].instructionstep==0) //First step?
	{
		if (MODRM_MOD(params.modrm)!=3) //Use R/M to calculate the result(Three-operand version)?
		{
			if (modrm_check16(&params,1,1)) return; //Abort on fault!
			if (CPU8086_instructionstepreadmodrmw(0,&temp1.val16,MODRM_src1)) return; //Read R/M!
			temp1.val16high = 0; //Clear high part by default!
		}
		else
		{
			if (CPU8086_instructionstepreadmodrmw(0,&temp1.val16,MODRM_src0)) return; //Read reg instead! Word register = Word register * imm16!
			temp1.val16high = 0; //Clear high part by default!
		}
		++CPU[activeCPU].instructionstep; //Next step!
	}
	if (CPU[activeCPU].instructionstep==1) //Second step?
	{
		temp2.val32 = (uint_32)immb; //Read unsigned parameter!

		if (temp1.val32&0x8000) temp1.val32 |= 0xFFFF0000;//Sign extend to 32 bits!
		if (temp2.val32&0x80) temp2.val32 |= 0xFFFFFF00; //Sign extend to 32 bits!
		temp3.val32s = temp1.val32s * temp2.val32s;
		CPU_apply286cycles(); //Apply the 80286+ cycles!
		//We're writing to the register always, so no normal writeback!
		++CPU[activeCPU].instructionstep; //Next step!
	}

	modrm_write16(&params,MODRM_src0,temp3.val16,0); //Write to register!
	if (((temp3.val32>>7)==0) || ((temp3.val32>>7)==0x1FFFFFF)) FLAGW_OF(0); //Overflow is cleared when the high byte is a sign extension of the low byte?
	else FLAGW_OF(1);
	FLAGW_CF(FLAG_OF); //Same!
	FLAGW_SF((temp3.val16&0x8000)>>15); //Sign!
	FLAGW_PF(parity[temp3.val16&0xFF]); //Parity flag!
	FLAGW_ZF((temp3.val16==0)?1:0); //Set the zero flag!
}

void CPU186_OP6C()
{
	debugger_setcommand("INSB");
	if (blockREP) return; //Disabled REP!
	static byte data;
	if (CPU[activeCPU].internalinstructionstep==0) if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_ES),CPU_segment(CPU_SEGMENT_ES),(CPU_Address_size[activeCPU]?REG_EDI:REG_DI),0,getCPL(),!CPU_Address_size[activeCPU],0)) return; //Abort on fault!
	if (CPU_PORT_IN_B(0,REG_DX,&data)) return; //Read the port!
	CPUPROT1
	if (CPU8086_internal_stepwritedirectb(0,CPU_segment_index(CPU_SEGMENT_ES),CPU_segment(CPU_SEGMENT_ES),(CPU_Address_size[activeCPU]?REG_EDI:REG_DI),data,!CPU_Address_size[activeCPU])) return; //INSB
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
	CPU_apply286cycles(); //Apply the 80286+ cycles!
	CPUPROT2
	CPUPROT2
}

void CPU186_OP6D()
{
	debugger_setcommand("INSW");
	if (blockREP) return; //Disabled REP!
	static word data;
	if (CPU[activeCPU].internalinstructionstep==0) if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_ES),CPU_segment(CPU_SEGMENT_ES),(CPU_Address_size[activeCPU]?REG_EDI:REG_DI),0,getCPL(),!CPU_Address_size[activeCPU],0|0x8)) return; //Abort on fault!
	if (CPU[activeCPU].internalinstructionstep==0) if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_ES),CPU_segment(CPU_SEGMENT_ES),(CPU_Address_size[activeCPU]?REG_EDI:REG_DI)+1,0,getCPL(),!CPU_Address_size[activeCPU],1|0x8)) return; //Abort on fault!
	if (CPU_PORT_IN_W(0,REG_DX, &data)) return; //Read the port!
	CPUPROT1
	if (CPU8086_internal_stepwritedirectw(0,CPU_segment_index(CPU_SEGMENT_ES),CPU_segment(CPU_SEGMENT_ES),(CPU_Address_size[activeCPU]?REG_EDI:REG_DI),data,!CPU_Address_size[activeCPU])) return; //INSW
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
	CPU_apply286cycles(); //Apply the 80286+ cycles!
	CPUPROT2
	CPUPROT2
}

void CPU186_OP6E()
{
	debugger_setcommand("OUTSB");
	if (blockREP) return; //Disabled REP!
	static byte data;
	if (CPU[activeCPU].internalmodrmstep==0) if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),(CPU_Address_size[activeCPU]?REG_ESI:REG_SI),1,getCPL(),!CPU_Address_size[activeCPU],0)) return; //Abort on fault!
	if (CPU8086_internal_stepreaddirectb(0,CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),(CPU_Address_size[activeCPU]?REG_ESI:REG_SI),&data,!CPU_Address_size[activeCPU])) return; //OUTSB
	CPUPROT1
	if (CPU_PORT_OUT_B(0,REG_DX,data)) return; //OUTS DX,Xb
	CPUPROT1
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
	CPU_apply286cycles(); //Apply the 80286+ cycles!
	CPUPROT2
	CPUPROT2
}

void CPU186_OP6F()
{
	debugger_setcommand("OUTSW");
	if (blockREP) return; //Disabled REP!
	static word data;
	if (CPU[activeCPU].internalmodrmstep==0) if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),(CPU_Address_size[activeCPU]?REG_ESI:REG_SI),1,getCPL(),!CPU_Address_size[activeCPU],0|0x8)) return; //Abort on fault!
	if (CPU[activeCPU].internalmodrmstep==0) if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),(CPU_Address_size[activeCPU]?REG_ESI:REG_SI)+1,1,getCPL(),!CPU_Address_size[activeCPU],1|0x8)) return; //Abort on fault!
	if (CPU8086_internal_stepreaddirectw(0,CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),(CPU_Address_size[activeCPU]?REG_ESI:REG_SI),&data,!CPU_Address_size[activeCPU])) return; //OUTSW
	CPUPROT1
	if (CPU_PORT_OUT_W(0,REG_DX,data)) return;    //OUTS DX,Xz
	CPUPROT1
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
	CPU_apply286cycles(); //Apply the 80286+ cycles!
	CPUPROT2
	CPUPROT2
}

word temp8Edata;
void CPU186_OP8E() { if (params.info[0].reg16==CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_CS]) /* CS is forbidden from this processor onwards! */ {unkOP_186(); return;} modrm_debugger16(&params, 0, 1); modrm_generateInstructionTEXT("MOV", 16, 0, PARAM_MODRM12); MODRM_src0 = 0; if (modrm_check16(&params,1,1)) return; if (CPU8086_instructionstepreadmodrmw(0,&temp8Edata,1)) return; CPU186_internal_MOV16(modrm_addr16(&params, 0, 0), temp8Edata); }

void CPU186_OPC0()
{
	oper2b = immb;

	memcpy(&info,&params.info[MODRM_src0],sizeof(info)); //Store the address for debugging!
	thereg = MODRM_REG(params.modrm);
	switch (thereg) //What function?
	{
		case 0: //ROL
			debugger_setcommand("ROLB %s,%02X",info.text,oper2b);
			break;
		case 1: //ROR
			debugger_setcommand("RORB %s,%02X",info.text,oper2b);
			break;
		case 2: //RCL
			debugger_setcommand("RCLB %s,%02X",info.text,oper2b);
			break;
		case 3: //RCR
			debugger_setcommand("RCRB %s,%02X",info.text,oper2b);
			break;
		case 4: //SHL
		case 6: //--- Unknown Opcode! --- Undocumented opcode!
			debugger_setcommand("SHLB %s,%02X",info.text,oper2b);
			break;
		case 5: //SHR
			debugger_setcommand("SHRB %s,%02X",info.text,oper2b);
			break;
		case 7: //SAR
			debugger_setcommand("SARB %s,%02X",info.text,oper2b);
			break;
		default:
			break;
	}

	if (CPU[activeCPU].instructionstep==0) if (modrm_check8(&params,MODRM_src0,1)) return; //Abort when needed!
	if (CPU[activeCPU].instructionstep==0) if (modrm_check8(&params,MODRM_src0,0)) return; //Abort when needed!
	if (CPU8086_instructionstepreadmodrmb(0,&oper1b,MODRM_src0)) return;
	if (CPU[activeCPU].instructionstep==2) //Execution step?
	{
		res8 = op_grp2_8(oper2b,2); //Execute!
		++CPU[activeCPU].instructionstep; //Next step: writeback!
	}
	if (CPU8086_instructionstepwritemodrmb(3,res8,MODRM_src0)) return;
} //GRP2 Eb,Ib

void CPU186_OPC1()
{
	memcpy(&info,&params.info[MODRM_src0],sizeof(info)); //Store the address for debugging!
	oper2 = (word)immb;
	thereg = MODRM_REG(params.modrm);
	switch (thereg) //What function?
	{
		case 0: //ROL
			debugger_setcommand("ROL %s,%02X",info.text,oper2);
			break;
		case 1: //ROR
			debugger_setcommand("ROR %s,%02X",info.text,oper2);
			break;
		case 2: //RCL
			debugger_setcommand("RCL %s,%02X",info.text,oper2);
			break;
		case 3: //RCR
			debugger_setcommand("RCR %s,%02X",info.text,oper2);
			break;
		case 4: //SHL
			debugger_setcommand("SHL %s,%02X",info.text,oper2);
			break;
		case 5: //SHR
			debugger_setcommand("SHR %s,%02X",info.text,oper2);
			break;
		case 6: //--- Unknown Opcode! --- Undocumented opcode!
			debugger_setcommand("SHL %s,%02X",info.text,oper2);
			break;
		case 7: //SAR
			debugger_setcommand("SAR %s,%02X",info.text,oper2);
			break;
		default:
			break;
	}
	
	if (CPU[activeCPU].instructionstep==0) if (modrm_check16(&params,MODRM_src0,1)) return; //Abort when needed!
	if (CPU[activeCPU].instructionstep==0) if (modrm_check16(&params,MODRM_src0,0)) return; //Abort when needed!
	if (CPU8086_instructionstepreadmodrmw(0,&oper1,MODRM_src0)) return;
	if (CPU[activeCPU].instructionstep==2) //Execution step?
	{
		res16 = op_grp2_16((byte)oper2,2); //Execute!
		++CPU[activeCPU].instructionstep; //Next step: writeback!
	}
	if (CPU8086_instructionstepwritemodrmw(3,res16,MODRM_src0,0)) return;
} //GRP2 Ev,Ib

extern byte ENTER_L; //Level value of the ENTER instruction!
void CPU186_OPC8()
{
	word temp16;    //ENTER Iw,Ib
	word stacksize = immw;
	byte nestlev = immb;
	word bpdata;
	debugger_setcommand("ENTER %04X,%02X",stacksize,nestlev);
	nestlev &= 0x1F; //MOD 32!
	if (EMULATED_CPU>CPU_80486) //We don't check it all before, but during the execution on 486- processors!
	{
		if (CPU[activeCPU].instructionstep==0) if (checkStackAccess(1+nestlev,1,0)) return; //Abort on error!
		if (CPU[activeCPU].instructionstep==0) if (checkENTERStackAccess((nestlev>1)?(nestlev-1):0,0)) return; //Abort on error!
	}
	ENTER_L = nestlev; //Set the nesting level used!
	//according to http://www.felixcloutier.com/x86/ENTER.html
	if (EMULATED_CPU<=CPU_80486) //We don't check it all before, but during the execution on 486- processors!
	{
		if (CPU[activeCPU].instructionstep==0) if (checkStackAccess(1,1,0)) return; //Abort on error!		
	}

	/*
	if (CPU[activeCPU].instructionstep==0) //Starting the instruction?
	{
		CPU[activeCPU].have_oldESP = 1; //We have an old ESP to jump back to!
		CPU[activeCPU].oldESP = REG_ESP; //Back-up!
	}
	*/ //Done automatically at the start of an instruction!

	if (CPU8086_PUSHw(0,&REG_BP,0)) return; //Busy pushing?
	word frametemp = (word)CPU[activeCPU].oldESP; //Read the original value to start at(for stepping compatibility)!
	word framestep,instructionstep;
	instructionstep = 2; //We start at step 2 for the stack operations on instruction step!
	framestep = 0; //We start at step 0 for the stack frame operations!
	if (nestlev)
	{
		for (temp16=1; temp16<nestlev; ++temp16)
		{
			if (EMULATED_CPU<=CPU_80486) //We don't check it all before, but during the execution on 486- processors!
			{
				if (CPU[activeCPU].internalinstructionstep==framestep) if (checkENTERStackAccess(1,0)) return; //Abort on error!				
			}
			if (CPU8086_internal_stepreaddirectw(framestep,CPU_SEGMENT_SS,REG_SS,REG_BP-(temp16<<1),&bpdata,1)) return; //Read data from memory to copy the stack!
			framestep += 2; //We're adding 2 immediately!
			if (CPU[activeCPU].internalinstructionstep==framestep) //At the write back phase?
			{
				if (EMULATED_CPU<=CPU_80486) //We don't check it all before, but during the execution on 486- processors!
				{
					if (checkStackAccess(1,1,0)) return; //Abort on error!
				}
				if (CPU8086_PUSHw(instructionstep,&bpdata,0)) return; //Write back!
			}
			instructionstep += 2; //Next instruction step base to process!
		}
		if (EMULATED_CPU<=CPU_80486) //We don't check it all before, but during the execution on 486- processors!
		{
			if (checkStackAccess(1,1,0)) return; //Abort on error!		
		}
		if (CPU8086_PUSHw(instructionstep,&frametemp,0)) return; //Felixcloutier.com says frametemp, fake86 says Sp(incorrect).
	}
	
	REG_BP = frametemp;
	REG_SP -= stacksize; //Substract: the stack size is data after the buffer created, not immediately at the params.  
	CPU_apply286cycles(); //Apply the 80286+ cycles!
}
void CPU186_OPC9()
{
	word oldSP;
	debugger_setcommand("LEAVE");
	if (checkStackAccess(1,0,0)) return; //Abort on fault!
	oldSP = REG_SP; //Backup SP!
	REG_SP = REG_BP;    //LEAVE
	if (CPU8086_POPw(0,&REG_BP,0)) //Not done yet?
	{
		REG_SP = oldSP; //Restore SP to retry later!
		return; //Abort!
	}
	CPU_apply286cycles(); //Apply the 80286+ cycles!
}

//Fully checked, and the same as fake86.
