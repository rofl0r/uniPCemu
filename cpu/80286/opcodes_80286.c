#include "headers/types.h" //Basic type support etc.
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/emu/gpu/gpu.h" //Need GPU comp!
#include "headers/cpu/8086/cpu_OP8086.h" //8086 interrupt instruction support!
#include "headers/bios/bios.h" //BIOS Support!
#include "headers/debugger/debugger.h" //Debugger support!
#include "headers/cpu/easyregs.h" //Easy register addressing!

#include "headers/emu/gpu/gpu_emu.h" //GPU EMU support!
#include "headers/interrupts/interrupt10.h" //Interrupt 10h support!

extern BIOS_Settings_TYPE BIOS_Settings; //BIOS Settings!

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

extern Handler opcode0F_jmptbl[NUM0FEXTS][0x100][2]; //0F opcode jumptable for 286+, starting at 286+.

void unkOP0F_286() //0F unknown opcode handler on 286+?
{
	CPU_resetOP(); //Go back to the opcode itself!
	CPU8086_int(0x06); //Call interrupt!
}

void CPU_OP0F_286() //Special 2-byte opcode (286+)?
{
	byte OP = CPU_readOP(); //Read second OPcode!
	CPU[activeCPU].lastopcode = OP; //Last opcode is the 0F opcode specifier, the byte after 0F!
	if ((EMULATED_CPU-2)>=0) //Valid CPU with the OPcodes?
	{
		int cpu = EMULATED_CPU-2; //Init cpu!
		byte operandsize = CPU_Operand_size[cpu]; //Operand size to use!
		while ((opcode0F_jmptbl[cpu][OP][operandsize]==NULL)) //No opcode to handle at current CPU&operand size?
		{
			if (operandsize) //We have an operand size: switch to standard if possible!
			{
				operandsize = 0; //Not anymore!
				continue; //Try again!
			}
			else //No operand size: we're a standard, so go up one cpu and retry!
			{
				operandsize = CPU_Operand_size[cpu]; //Reset operand size!
				if (cpu) //We've got CPUs left?
				{
					--cpu; //Go up one CPU!
				}
				else //No CPUs left!
				{
					//dolog("CPU","Opcode not defined in jmptbl: %02X",OP); //OPCode not defined!
					unkOP0F_286(); //We're an unsupported 0F opcode, so behave like one!
					return; //Can't execute!
				}
			}
		}
		opcode0F_jmptbl[cpu][OP][operandsize](); //Now go execute the OPcode once in the runtime!
	}
	else
	{
		raiseError("80286","0F Opcode on a processor older than 80286?"); //No 0F OPCode found or supported!
	}
}