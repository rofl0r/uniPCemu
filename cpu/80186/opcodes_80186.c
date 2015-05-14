#include "headers/types.h" //Basic types!
#include "headers/cpu/cpu.h" //Basic CPU!
#include "headers/cpu/easyregs.h" //Easy registers!
#include "headers/cpu/modrm.h" //MODR/M compatibility!
#include "headers/support/signedness.h" //CPU support functions!
#include "headers/hardware/ports.h" //Ports compatibility!
#include "headers/debugger/debugger.h" //Debug compatibility!
#include "headers/cpu/8086/cpu_OP8086.h" //8086 function specific compatibility!
#include "headers/cpu/8086/8086_grpOPs.h" //GRP Opcode support (C0&C1 Opcodes!)
#include "headers/cpu/80186/cpu_OP80186.h" //80186 function specific compatibility!
#include "headers/support/log.h" //Logging support!
#include "headers/cpu/80286/protection.h" //Protection support!

extern MODRM_PARAMS params;    //For getting all params!
extern byte blockREP; //Block the instruction from executing (REP with (E)CX=0
MODRM_PTR info; //For storing ModR/M Info!

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
extern byte reg; //For function number!
extern uint_32 ea; //From RM offset (GRP5 Opcodes only!)

extern uint_32 temp1, temp2, temp3, temp4, temp5, temp32, tempaddr32;

//Help functions for debugging:
extern char modrm_param1[256]; //Contains param/reg1
extern char modrm_param2[256]; //Contains param/reg2


void CPU186_OP60()
{
	debugger_setcommand("PUSHA");
	word oldSP = REG_SP;    //PUSHA
	CPU_PUSH16(&REG_AX);
	CPU_PUSH16(&REG_CX);
	CPU_PUSH16(&REG_DX);
	CPU_PUSH16(&REG_BX);
	CPU_PUSH16(&oldSP);
	CPU_PUSH16(&REG_BP);
	CPU_PUSH16(&REG_SI);
	CPU_PUSH16(&REG_DI);
}
void CPU186_OP61()
{
	debugger_setcommand("POPA");
	word dummy;    //POPA
	REG_DI = CPU_POP16();
	REG_SI = CPU_POP16();
	REG_BP = CPU_POP16();
	dummy = CPU_POP16();
	REG_BX = CPU_POP16();
	REG_DX = CPU_POP16();
	REG_CX = CPU_POP16();
	REG_AX = CPU_POP16();
}
//62 not implemented in fake86? Does this not exist?
void CPU186_OP62()
{
	word bound_min, bound_max;
	modrm_readparams(&params,2,0);
	word theval = modrm_read16(&params,1);
	modrm_decode16(&params,&info,2);
	bound_min=MMU_rw(get_segment_index(info.segmentregister),info.mem_segment,info.mem_offset,0);
	bound_max=MMU_rw(get_segment_index(info.segmentregister),info.mem_segment,info.mem_offset+2,0);
	modrm_debugger16(&params,1,2); //Debug the location!
	debugger_setcommand("BOUND %s,%s",modrm_param1,modrm_param2); //Opcode!
	if ((theval<bound_min) || (theval>bound_max))
	{
		//BOUND Gv,Ma
		CPU_BoundException(); //Execute bound exception!
	}
}
void CPU186_OP68()
{
	word val = CPU_readOPw();    //PUSH Iz
	debugger_setcommand("PUSH %04X",val);
	CPU_PUSH16(&val);
}
void CPU186_OP69()
{
	modrm_readparams(&params,2,0);
	temp1 = modrm_read16(&params,2);
	temp2 = CPU_readOPw();
	modrm_decode16(&params,&info,1);
	debugger_setcommand("IMUL %s,%04X",info.text,temp2);
	if ((temp1&0x8000)==0x8000) temp1 |= 0xFFFF0000;
	if ((temp2&0x8000)==0x8000) temp2 |= 0xFFFF0000;
	temp3 = ((temp1*temp2)&0xFFFFFFFF);
	REG_AX = (temp3&0xFFFF);
	REG_DX = ((temp3>>16)&0xFFFF);
	FLAG_CF = FLAG_OF = (unsigned2signed32(temp3)!=unsigned2signed16(REG_AX)); //Overflow occurred?
}
void CPU186_OP6A()
{
	byte val = CPU_readOP(); //Read the value!
	debugger_setcommand("PUSH %02X",val); //PUSH this!
	CPU_PUSH8(val);    //PUSH Ib
}
void CPU186_OP6B()
{
	modrm_readparams(&params,1,0);
	temp1 = modrm_read16(&params,2); //Read R/M!
	temp2 = CPU_readOP(); //Read unsigned!
	modrm_decode16(&params,&info,2); //Store the address!
	temp2 = (temp2&0x80<<8)|(temp2&0x7F); //Sign extend to 16 bits!
	debugger_setcommand("IMUL %s,%02X",info.text,temp2); //Command!
	if ( (temp1 & 0x8000L) == 0x8000L) {
			temp1 = temp1 | 0xFFFF0000L;
		}

	if ( (temp2 & 0x8000L) == 0x8000L) {
			temp2 = temp2 | 0xFFFF0000L;
		}

	temp3 = signed2unsigned32(unsigned2signed32(temp1) * unsigned2signed32(temp2));
	modrm_write16(&params,1, temp3 & 0xFFFFL,0); //Write to register!
	FLAG_CF = FLAG_OF = (unsigned2signed32(temp3)!=unsigned2signed16(temp3&0xFFFF)); //Overflow occurred?
}
void CPU186_OP6C()
{
	debugger_setcommand("INSB");
	if (blockREP) return; //Disabled REP!
	MMU_wb(get_segment_index(CPU_segment_ptr(CPU_SEGMENT_ES)),CPU_segment(CPU_SEGMENT_ES),REG_DI,PORT_IN_B(REG_DX));    //INSB
	if (FLAG_DF)
	{
		--REG_DI;
	}
	else
	{
		++REG_DI;
	}
}
void CPU186_OP6D()
{
	debugger_setcommand("INSW");
	if (blockREP) return; //Disabled REP!
	MMU_ww(get_segment_index(CPU_segment_ptr(CPU_SEGMENT_ES)),CPU_segment(CPU_SEGMENT_ES),REG_DI,PORT_IN_W(REG_DX));    //INSW
	if (FLAG_DF)
	{
		REG_DI -= 2;
	}
	else
	{
		REG_DI += 2;
	}
}
void CPU186_OP6E()
{
	debugger_setcommand("OUTSB");
	if (blockREP) return; //Disabled REP!
	PORT_OUT_B(REG_DX,MMU_rb(get_segment_index(CPU_segment_ptr(CPU_SEGMENT_ES)),CPU_segment(CPU_SEGMENT_ES),REG_SI,0)); //OUTS DX,Xb
	if (FLAG_DF)
	{
		--REG_DI;
	}
	else
	{
		++REG_DI;
	}
}
void CPU186_OP6F()
{
	debugger_setcommand("OUTSW");
	if (blockREP) return; //Disabled REP!
	PORT_OUT_W(REG_DX,MMU_rw(get_segment_index(CPU_segment_ptr(CPU_SEGMENT_ES)),CPU_segment(CPU_SEGMENT_ES),REG_SI,0));    //OUTS DX,Xz
	if (FLAG_DF)
	{
		REG_DI -= 2;
	}
	else
	{
		REG_DI += 2;
	}
}

void CPU186_OPC0()
{
	modrm_readparams(&params,1,0);

	oper1b = modrm_read8(&params,2);
	oper2b = CPU_readOP();
	reg = MODRM_REG(params.modrm);

	modrm_decode16(&params,&info,2); //Store the address for debugging!
	switch (reg) //What function?
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
		
	
	modrm_write8(&params,2,op_grp2_8(oper2b));
} //GRP2 Eb,Ib

void CPU186_OPC1()
{
	modrm_readparams(&params,2,0);

	oper1 = modrm_read16(&params,2);
	oper2 = (word)CPU_readOP();
	reg = MODRM_REG(params.modrm);

	modrm_decode16(&params,&info,2); //Store the address for debugging!
	switch (reg) //What function?
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
	
	modrm_write16(&params,2,op_grp2_16(oper2),0);
} //GRP2 Ev,Ib
void CPU186_OPC8()
{
	word temp16;    //ENTER Iw,Ib
	word stacksize = CPU_readOPw();
	byte nestlev = CPU_readOP();
	debugger_setcommand("ENTER %04X,%02X",stacksize,nestlev);
	if (CPU_Operand_size)
	{
	    CPU_PUSH32(&REG_EBP);
	}
	else
	{
	    CPU_PUSH16(&REG_BP);
	}
	if (CPU_Operand_size) //32-bit size?
    {
    	uint_32 frametemp = REG_ESP;
    	if (nestlev)
    	{
            nestlev &= 0x1F; //MOD 32!
    		for (temp16=1; temp16<nestlev; temp16++)
    		{
    			REG_BP -= 4; //Push BP to the next size of BP!
    			CPU_PUSH32(&REG_EBP);
    		}
    		CPU_PUSH32(&REG_ESP);
    	}
    	REG_EBP = frametemp;
    }
    else
    {
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
	}
	
	if (CPU_StackAddress_size) //32-bit size?
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
	REG_SP = REG_BP;    //LEAVE
	REG_BP = CPU_POP16();
}

extern byte OPbuffer[256]; //A large opcode buffer!
extern byte OPlength; //The length of the opcode buffer!
char command[50]; //A command buffer for storing our command (up to 10 bytes)!
void unkOP_186() //Unknown opcode on 186+?
{
	bzero(command,sizeof(command)); //Clear the command!
	debugger_setcommand("<80186+ #UD>"); //Command is unknown opcode!
	//dolog("unkop","Unknown opcode on 80186+: %02X",CPU[activeCPU].lastopcode); //Last read opcode!
	CPU_resetOP(); //Go back to the opcode itself!
	CPU8086_int(0x06); //Call interrupt with return addres of the OPcode!
}

//Fully checked, and the same as fake86.