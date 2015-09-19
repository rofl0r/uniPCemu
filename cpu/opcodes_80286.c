#include "headers/types.h" //Basic type support etc.
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/emu/gpu/gpu.h" //Need GPU comp!
#include "headers/cpu/cpu_OP8086.h" //8086 interrupt instruction support!
#include "headers/bios/bios.h" //BIOS Support!
#include "headers/emu/debugger/debugger.h" //Debugger support!
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

extern Handler CurrentCPU_opcode0F_jmptbl[512]; //Our standard internal standard opcode jmptbl!

void CPU_OP0F_286() //Special 2-byte opcode (286+)?
{
	byte OP = CPU_readOP(); //Read second OPcode!
	CPU[activeCPU].lastopcode = OP; //Last opcode is the 0F opcode specifier, the byte after 0F!
	CurrentCPU_opcode0F_jmptbl[(OP << 1) | CPU_Operand_size[activeCPU]](); //Execute the 0F opcode handler, if any, or fault it!
}