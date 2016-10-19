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

//Shutdown the application when an unknown instruction is executed?
//#define UNKOP_SHUTDOWN

extern MODRM_PARAMS params;    //For getting all params!
extern byte blockREP; //Block the instruction from executing (REP with (E)CX=0
extern byte MODRM_src0; //What source is our modr/m? (1/2)
MODRM_PTR info; //For storing ModR/M Info!

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

OPTINLINE void modrm186_generateInstructionTEXT(char *instruction, byte debuggersize, uint_32 paramdata, byte type) //Copy of 8086 version!
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
			CPUPROT2
		}
		else //Memory?
		{
			if (custommem)
			{
				if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), customoffset,0,getCPL())) return; //Abort on fault!
				if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), customoffset+1,0,getCPL())) return; //Abort on fault!
				MMU_ww(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), customoffset, val); //Write to memory directly!
			}
			else //ModR/M?
			{
				modrm_write16(&params, MODRM_src0, val, 0); //Write the result to memory!
			}
		}
	CPUPROT2
}

void CPU186_OP60()
{
	if (checkStackAccess(8,1,0)) return; //Abort on fault!
	debugger_setcommand("PUSHA");
	word oldSP = REG_SP;    //PUSHA
	CPU_PUSH16(&REG_AX);
	CPUPROT1
	CPU_PUSH16(&REG_CX);
	CPUPROT1
	CPU_PUSH16(&REG_DX);
	CPUPROT1
	CPU_PUSH16(&REG_BX);
	CPUPROT1
	CPU_PUSH16(&oldSP);
	CPUPROT1
	CPU_PUSH16(&REG_BP);
	CPUPROT1
	CPU_PUSH16(&REG_SI);
	CPUPROT1
	CPU_PUSH16(&REG_DI);
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
}

void CPU186_OP61()
{
	if (checkStackAccess(8,0,0)) return; //Abort on fault!
	debugger_setcommand("POPA");
	REG_DI = CPU_POP16();
	CPUPROT1
	REG_SI = CPU_POP16();
	CPUPROT1
	REG_BP = CPU_POP16();
	CPUPROT1
	CPU_POP16();
	CPUPROT1
	REG_BX = CPU_POP16();
	CPUPROT1
	REG_DX = CPU_POP16();
	CPUPROT1
	REG_CX = CPU_POP16();
	CPUPROT1
	REG_AX = CPU_POP16();
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
}

extern byte modrm_addoffset; //Add this offset to ModR/M reads!

//62 not implemented in fake86? Does this not exist?
void CPU186_OP62()
{
	modrm_debugger16(&params,0,1); //Debug the location!
	debugger_setcommand("BOUND %s,%s",modrm_param1,modrm_param2); //Opcode!

	word bound_min, bound_max;
	word theval;
	modrm_addoffset = 0; //No offset!
	if (modrm_check16(&params,0,1)) return; //Abort on fault!
	if (modrm_check16(&params,1,1)) return; //Abort on fault!
	modrm_addoffset = 2; //Max offset!
	if (modrm_check16(&params,1,1)) return; //Abort on fault!

	modrm_addoffset = 0; //No offset!
	theval = modrm_read16(&params,0); //Read index!
	bound_min=modrm_read16(&params,1); //Read min!
	modrm_addoffset = 2; //Max offset!
	bound_max=modrm_read16(&params,1); //Read max!
	modrm_addoffset = 0; //Reset offset!
	if ((theval<bound_min) || (theval>bound_max))
	{
		//BOUND Gv,Ma
		CPU_BoundException(); //Execute bound exception!
	}
}

void CPU186_OP68()
{
	word val = immw;    //PUSH Iz
	debugger_setcommand("PUSH %04X",val);
	if (checkStackAccess(1,1,0)) return; //Abort on fault!
	CPU_PUSH16(&val);
}

void CPU186_OP69()
{
	if (modrm_check16(&params,1,1)) return; //Abort on fault!
	temp1.val32 = modrm_read16(&params,1);
	temp2.val32 = immw;
	modrm_decode16(&params,&info,0);
	debugger_setcommand("IMUL %s,%04X",info.text,temp2);
	if ((temp1.val32 &0x8000)==0x8000) temp1.val32 |= 0xFFFF0000;
	if ((temp2.val32 &0x8000)==0x8000) temp2.val32 |= 0xFFFF0000;
	temp3.val32 = ((temp1.val32*temp2.val32)&0xFFFFFFFF);
	REG_AX = temp3.val16;
	REG_DX = temp3.val16high;
	FLAG_CF = FLAG_OF = (unsigned2signed32(temp3.val32)!=unsigned2signed16(REG_AX)); //Overflow occurred?
}

void CPU186_OP6A()
{
	if (checkStackAccess(1,1,0)) return; //Abort on fault!
	byte val = immb; //Read the value!
	debugger_setcommand("PUSH %02X",val); //PUSH this!
	CPU_PUSH8(val);    //PUSH Ib
}

void CPU186_OP6B()
{
	if (modrm_check16(&params,1,1)) return; //Abort on fault!
	temp1.val32 = (uint_32)modrm_read16(&params,1); //Read R/M!
	temp2.val32 = (uint_32)immb; //Read unsigned parameter!
	modrm_decode16(&params,&info,1); //Store the address!
	if (temp1.val32&0x8000) temp1.val32 |= 0xFFFF0000;//Sign extend to 32 bits!
	if (temp2.val32&0x80) temp2.val32 |= 0xFFFFFF00; //Sign extend to 32 bits!
	debugger_setcommand("IMUL %s,%02X",info.text,temp2.val32&0xFF); //Command!

	temp3.val32s = temp1.val32s * temp2.val32s;
	modrm_write16(&params,0, temp3.val16,0); //Write to register!
	FLAG_CF = FLAG_OF = (unsigned2signed32(temp3.val32)!=unsigned2signed16(temp3.val32&0xFFFF)); //Overflow occurred?
}

void CPU186_OP6C()
{
	debugger_setcommand("INSB");
	if (blockREP) return; //Disabled REP!
	byte data;
	if (checkMMUaccess(get_segment_index(CPU_segment_ptr(CPU_SEGMENT_ES)),CPU_segment(CPU_SEGMENT_ES),REG_DI,0,getCPL())) return; //Abort on fault!
	CPU_PORT_IN_B(REG_DX,&data); //Read the port!
	CPUPROT1
	MMU_wb(get_segment_index(CPU_segment_ptr(CPU_SEGMENT_ES)),CPU_segment(CPU_SEGMENT_ES),REG_DI,data);    //INSB
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
	CPUPROT2
}

void CPU186_OP6D()
{
	debugger_setcommand("INSW");
	if (blockREP) return; //Disabled REP!
	word data;
	if (checkMMUaccess(get_segment_index(CPU_segment_ptr(CPU_SEGMENT_ES)),CPU_segment(CPU_SEGMENT_ES),REG_DI,0,getCPL())) return; //Abort on fault!
	if (checkMMUaccess(get_segment_index(CPU_segment_ptr(CPU_SEGMENT_ES)),CPU_segment(CPU_SEGMENT_ES),REG_DI+1,0,getCPL())) return; //Abort on fault!
	CPU_PORT_IN_W(REG_DX, &data); //Read the port!
	CPUPROT1
	MMU_ww(get_segment_index(CPU_segment_ptr(CPU_SEGMENT_ES)),CPU_segment(CPU_SEGMENT_ES),REG_DI,data);    //INSW
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
	CPUPROT2
}

void CPU186_OP6E()
{
	debugger_setcommand("OUTSB");
	if (blockREP) return; //Disabled REP!
	byte data;
	if (checkMMUaccess(get_segment_index(CPU_segment_ptr(CPU_SEGMENT_DS)),CPU_segment(CPU_SEGMENT_DS),REG_SI,1,getCPL())) return; //Abort on fault!
	data = MMU_rb(get_segment_index(CPU_segment_ptr(CPU_SEGMENT_DS)), CPU_segment(CPU_SEGMENT_DS), REG_SI, 0);
	CPUPROT1
	CPU_PORT_OUT_B(REG_DX,data); //OUTS DX,Xb
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
	CPUPROT2
}

void CPU186_OP6F()
{
	debugger_setcommand("OUTSW");
	if (blockREP) return; //Disabled REP!
	word data;
	if (checkMMUaccess(get_segment_index(CPU_segment_ptr(CPU_SEGMENT_DS)),CPU_segment(CPU_SEGMENT_DS),REG_SI,1,getCPL())) return; //Abort on fault!
	if (checkMMUaccess(get_segment_index(CPU_segment_ptr(CPU_SEGMENT_DS)),CPU_segment(CPU_SEGMENT_DS),REG_SI+1,1,getCPL())) return; //Abort on fault!
	data = MMU_rw(get_segment_index(CPU_segment_ptr(CPU_SEGMENT_DS)), CPU_segment(CPU_SEGMENT_DS), REG_SI, 0);
	CPUPROT1
	CPU_PORT_OUT_W(REG_DX,data);    //OUTS DX,Xz
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
	CPUPROT2
}

void CPU186_OP8E() { if (params.info[0].reg16==CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_CS]) /* CS is forbidden from this processor onwards! */ {unkOP_186(); return;} modrm_debugger16(&params, 0, 1); modrm186_generateInstructionTEXT("MOVW", 16, 0, PARAM_MODRM12); MODRM_src0 = 0; if (modrm_check16(&params,1,1)) return; CPU186_internal_MOV16(modrm_addr16(&params, 0, 0), modrm_read16(&params, 1)); }

void CPU186_OPC0()
{
	if (modrm_check8(&params,1,1)) return; //Abort on error!
	if (modrm_check8(&params,1,0)) return; //Abort on error!

	oper1b = modrm_read8(&params,1);
	oper2b = immb;
	thereg = MODRM_REG(params.modrm);

	modrm_decode16(&params,&info,1); //Store the address for debugging!
	switch (thereg) //What function?
	{
		case 0: //ROL
			debugger_setcommand("ROL %s,%02X",info.text,oper2b);
			break;
		case 1: //ROR
			debugger_setcommand("ROR %s,%02X",info.text,oper2b);
			break;
		case 2: //RCL
			debugger_setcommand("RCL %s,%02X",info.text,oper2b);
			break;
		case 3: //RCR
			debugger_setcommand("RCR %s,%02X",info.text,oper2b);
			break;
		case 4: //SHL
			debugger_setcommand("SHL %s,%02X",info.text,oper2b);
			break;
		case 5: //SHR
			debugger_setcommand("SHR %s,%02X",info.text,oper2b);
			break;
		case 6: //--- Unknown Opcode! --- Undocumented opcode!
			debugger_setcommand("SHL %s,%02X",info.text,oper2b);
			break;
		case 7: //SAR
			debugger_setcommand("SAR %s,%02X",info.text,oper2b);
			break;
		default:
			break;
	}
		
	
	modrm_write8(&params,1,op_grp2_8(oper2b,2));
} //GRP2 Eb,Ib

void CPU186_OPC1()
{
	if (modrm_check8(&params,1,1)) return; //Abort on error!
	if (modrm_check8(&params,1,0)) return; //Abort on error!
	oper1 = modrm_read16(&params,1);
	oper2 = (word)immb;
	thereg = MODRM_REG(params.modrm);

	modrm_decode16(&params,&info,1); //Store the address for debugging!
	switch (thereg) //What function?
	{
		case 0: //ROL
			debugger_setcommand("ROL %s,%02X",info.text,oper2b);
			break;
		case 1: //ROR
			debugger_setcommand("ROR %s,%02X",info.text,oper2b);
			break;
		case 2: //RCL
			debugger_setcommand("RCL %s,%02X",info.text,oper2b);
			break;
		case 3: //RCR
			debugger_setcommand("RCR %s,%02X",info.text,oper2b);
			break;
		case 4: //SHL
			debugger_setcommand("SHL %s,%02X",info.text,oper2b);
			break;
		case 5: //SHR
			debugger_setcommand("SHR %s,%02X",info.text,oper2b);
			break;
		case 6: //--- Unknown Opcode! --- Undocumented opcode!
			debugger_setcommand("SHL %s,%02X",info.text,oper2b);
			break;
		case 7: //SAR
			debugger_setcommand("SAR %s,%02X",info.text,oper2b);
			break;
		default:
			break;
	}
	
	modrm_write16(&params,1,op_grp2_16((byte)oper2,2),0);
} //GRP2 Ev,Ib
void CPU186_OPC8()
{
	word temp16;    //ENTER Iw,Ib
	word stacksize = immw;
	byte nestlev = immb;
	debugger_setcommand("ENTER %04X,%02X",stacksize,nestlev);
	if (checkStackAccess(1+(nestlev?(nestlev&0x1F):0),1,0)) return; //Abort on error!
	CPU_PUSH16(&REG_BP);
    word frametemp = REG_SP;
    if (nestlev)
    {
        nestlev &= 0x1F; //MOD 32!
    	for (temp16=1; temp16<nestlev; temp16++)
    	{
    		REG_BP -= 2; //Push BP to the next size of BP!
    		CPU_PUSH16(&REG_BP);
    	}
    	CPU_PUSH16(&REG_SP);
    }
    REG_BP = frametemp;
	
	if (CPU_StackAddress_size[activeCPU]) //32-bit size?
	{
        REG_ESP -= stacksize; //Zero extend!
	}
	else //--?
	{
        REG_SP -= stacksize;
	}
}
void CPU186_OPC9()
{
	debugger_setcommand("LEAVE");
	if (checkStackAccess(1,0,0)) return; //Abort on fault!
	REG_SP = REG_BP;    //LEAVE
	REG_BP = CPU_POP16();
}

void unkOP_186() //Unknown opcode on 186+?
{
	debugger_setcommand("<NECV20/V30+ #UD>"); //Command is unknown opcode!
	//dolog("unkop","Unknown opcode on NECV30+: %02X",CPU[activeCPU].lastopcode); //Last read opcode!
	CPU_resetOP(); //Go back to the opcode itself!
	CPU086_int(0x06); //Call interrupt with return addres of the OPcode!
#ifdef UNKOP_SHUTDOWN
	EMU_Shutdown(1); //Request to shut down!
#endif
}

//Fully checked, and the same as fake86.
