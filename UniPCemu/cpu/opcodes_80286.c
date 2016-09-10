#include "headers/types.h" //Basic type support etc.
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/emu/gpu/gpu.h" //Need GPU comp!
#include "headers/cpu/cpu_OP8086.h" //8086 interrupt instruction support!
#include "headers/bios/bios.h" //BIOS Support!
#include "headers/emu/debugger/debugger.h" //Debugger support!
#include "headers/cpu/easyregs.h" //Easy register addressing!

#include "headers/emu/gpu/gpu_emu.h" //GPU EMU support!
#include "headers/interrupts/interrupt10.h" //Interrupt 10h support!

#include "headers/cpu/protection.h" //Protection support!

extern BIOS_Settings_TYPE BIOS_Settings; //BIOS Settings!
extern MODRM_PARAMS params;    //For getting all params!
extern MODRM_PTR info; //For storing ModR/M Info!
extern byte immb;
extern word immw;
extern uint_32 imm32;
extern byte thereg; //For function number!

/*

Interrupts:
0x00: //Divide error (0x00)
0x01: //Debug Exceptions (0x01)
0x02: //NMI Interrupt
0x03: //Breakpoint: software only!
0x04: //INTO Detected Overflow
0x05: //BOUND Range Exceeded
0x06: //Invalid Opcode
0x07: //Coprocessor Not Available
0x08: //Double Fault
0x09: //Coprocessor Segment Overrun
0x0A: //Invalid Task State Segment
0x0B: //Segment not present
0x0C: //Stack fault
0x0D: //General Protection Fault
0x0E: //Page Fault
0x0F: //Reserved
//0x10:
0x10: //Coprocessor Error
0x11: //Allignment Check

*/

extern Handler CurrentCPU_opcode0F_jmptbl[512]; //Our standard internal standard opcode jmptbl!

void unkOP_286() //Unknown opcode on 186+?
{
	debugger_setcommand("<80286+ #UD>"); //Command is unknown opcode!
	//dolog("unkop","Unknown opcode on NECV30+: %02X",CPU[activeCPU].lastopcode); //Last read opcode!
	CPU_resetOP(); //Go back to the opcode itself!
	CPU086_int(0x06); //Call interrupt with return addres of the OPcode!
}

extern byte OPbuffer[256]; //A large opcode buffer!
extern word OPlength; //The length of the opcode buffer!
char command[50]; //A command buffer for storing our command (up to 10 bytes)!
void unkOP0F_286() //Unknown opcode on 186+?
{
	bzero(command, sizeof(command)); //Clear the command!
	debugger_setcommand("<80286+ 0F #UD>"); //Command is unknown opcode!
	//dolog("unkop","Unknown opcode on 80286+: %02X",CPU[activeCPU].lastopcode); //Last read opcode!
	CPU_resetOP(); //Go back to the opcode itself!
	CPU086_int(0x06); //Call interrupt with return addres of the OPcode!
}

void CPU_OP0F_286() //Special 2-byte opcode (286+)?
{
	byte OP = immb; //Read second OPcode!
	CPU[activeCPU].lastopcode = OP; //Last opcode is the 0F opcode specifier, the byte after 0F!
	CurrentCPU_opcode0F_jmptbl[(OP << 1) | CPU_Operand_size[activeCPU]](); //Execute the 0F opcode handler, if any, or fault it!
}

void CPU286_OP63() //ARPL r/m16,r16
{
	if (getcpumode() == CPU_MODE_REAL) //Real mode? #UD!
	{
		unkOP_286(); //Execute our unk opcode handler!
		return; //Abort!
	}
	word destRPL, srcRPL;
	destRPL = modrm_read16(&params,0); //Read destination RPL!
	CPUPROT1
	srcRPL = modrm_read16(&params,1); //Read source RPL!
	CPUPROT1
		if (getRPL(destRPL) < getRPL(srcRPL))
		{
			FLAG_ZF = 1; //Set ZF!
			setRPL(destRPL,getRPL(srcRPL)); //Set the new RPL!
			modrm_write16(&params,0,destRPL,0); //Set the result!
		}
		else
		{
			FLAG_ZF = 0; //Clear ZF!
		}
	CPUPROT2
	CPUPROT2
}

void CPU286_OPD6() //286+ SALC
{
	REG_AL = FLAG_CF?0xFF:0x00; //Set AL if Carry flag!
}

//0F opcodes for 286+ processors!

//Based on http://ref.x86asm.net/coder32.html

void CPU286_OP0F00() //Various extended 286+ instructions GRP opcode.
{
	modrm_readparams(&params,1,0); //Read our params!
	thereg = MODRM_REG(params.modrm);

	modrm_decode16(&params, &info, 1); //Store the address for debugging!
	switch (thereg) //What function?
	{
	case 0: //SLDT
		debugger_setcommand("SLDT %s", info.text);
		break;
	case 1: //STR
		debugger_setcommand("STR %s", info.text);
		break;
	case 2: //LLDT
		debugger_setcommand("LLDT %s", info.text);
		break;
	case 3: //LTR
		debugger_setcommand("LTR %s", info.text);
		break;
	case 4: //VERR
		debugger_setcommand("VERR %s", info.text);
		break;
	case 5: //VERW
		debugger_setcommand("VERW %s", info.text);
		break;
	case 6: //--- Unknown Opcode! ---
	case 7: //--- Unknown Opcode! ---
		unkOP0F_286(); //Unknown opcode!
		break;
	default:
		break;
	}
}

void CPU286_OP0F01() //Various extended 286+ instruction GRP opcode.
{
	modrm_readparams(&params, 1, 0); //Read our params!
	thereg = MODRM_REG(params.modrm);

	modrm_decode16(&params, &info, 1); //Store the address for debugging!
	switch (thereg) //What function?
	{
	case 1: //SIDT
		debugger_setcommand("SIDT %s", info.text);
		break;
	case 2: //LGDT
		debugger_setcommand("LGDT %s", info.text);
		break;
	case 3: //LIDT
		debugger_setcommand("LIDT %s", info.text);
		break;
	case 4: //SMSW
		debugger_setcommand("SMSW %s", info.text);
		break;
	case 6: //LMSW
		debugger_setcommand("LMSW %s", info.text);
		break;
	case 0: //--- Unknown Opcode!
	case 5: //--- Unknown Opcode!
	case 7: //--- Unknown Opcode!
		unkOP0F_286(); //Unknown opcode!
		break;
	default:
		break;
	}
}

void CPU286_OP0F02() //LAR /r
{
	unkOP0F_286(); //TODO!
}

void CPU286_OP0F03() //LSL /r
{
	unkOP0F_286(); //TODO!
}

void CPU286_OP0F06() //CLTS
{
	unkOP0F_286(); //TODO!
}

void CPU286_OP0F0B() //#UD instruction
{
	unkOP0F_286(); //Delibarately #UD!
}

void CPU286_OP0FB9() //#UD instruction
{
	unkOP0F_286(); //Delibarately #UD!
}