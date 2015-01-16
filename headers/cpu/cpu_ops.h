#ifndef CPU_OPS_H
#define CPU_OPS_H

#include "headers/types.h"
#include "headers/cpu/cpu.h" //CPU support!

//Easy functions for checking of registers etc.

//FI=Forget It!

/*

Info about the manual (important for adding opcodes):


The "Opcode" column gives the complete object code produced for each form of the instruction. When possible, the codes are given as hexadecimal bytes, in the same order in which they appear in memory. Definitions of entries other than hexadecimal bytes are as follows:

/digit: (digit is between 0 and 7) indicates that the ModR/M byte of the instruction uses only the r/m (register or memory) operand. The reg field contains the digit that provides an extension to the instruction's opcode.

/r: indicates that the ModR/M byte of the instruction contains both a register operand and an r/m operand.

cb, cw, cd, cp: a 1-byte (cb), 2-byte (cw), 4-byte (cd) or 6-byte (cp) value following the opcode that is used to specify a code offset and possibly a new value for the code segment register.

ib, iw, id: a 1-byte (ib), 2-byte (iw), or 4-byte (id) immediate operand to the instruction that follows the opcode, ModR/M bytes or scale-indexing bytes. The opcode determines if the operand is a signed value. All words and doublewords are given with the low-order byte first.

+rb, +rw, +rd: a register code, from 0 through 7, added to the hexadecimal byte given at the left of the plus sign to form a single opcode byte. The

codes are--

      rb         rw         rd
    REG_AL = 0     REG_AX = 0     REG_EAX = 0
    REG_CL = 1     REG_CX = 1     REG_ECX = 1
    REG_DL = 2     REG_DX = 2     REG_EDX = 2
    REG_BL = 3     REG_BX = 3     REG_EBX = 3
    REG_AH = 4     REG_SP = 4     REG_ESP = 4
    REG_CH = 5     REG_BP = 5     REG_EBP = 5
    REG_DH = 6     REG_SI = 6     REG_ESI = 6
    REG_BH = 7     REG_DI = 7     REG_EDI = 7




The "Instruction" column gives the syntax of the instruction statement as it would appear in an ASM386 program. The following is a list of the symbols used to represent operands in the instruction statements:

rel8: a relative address in the range from 128 bytes before the end of the instruction to 127 bytes after the end of the instruction.

rel16, rel32: a relative address within the same code segment as the instruction assembled. rel16 applies to instructions with an operand-size attribute of 16 bits; rel32 applies to instructions with an operand-size attribute of 32 bits.

ptr16:16, ptr16:32: a FAR pointer, typically in a code segment different from that of the instruction. The notation 16:16 indicates that the value of the pointer has two parts. The value to the right of the colon is a 16-bit selector or value destined for the code segment register. The value to the left corresponds to the offset within the destination segment. ptr16:16 is used when the instruction's operand-size attribute is 16 bits; ptr16:32 is used with the 32-bit attribute.

r8: one of the byte registers REG_AL, REG_CL, REG_DL, REG_BL, REG_AH, REG_CH, REG_DH, or REG_BH.

r16: one of the word registers REG_AX, REG_CX, REG_DX, REG_BX, REG_SP, REG_BP, REG_SI, or REG_DI.

r32: one of the doubleword registers REG_EAX, REG_ECX, REG_EDX, REG_EBX, REG_ESP, REG_EBP, REG_ESI, or REG_EDI.

imm8: an immediate byte value. imm8 is a signed number between -128 and +127 inclusive. For instructions in which imm8 is combined with a word or doubleword operand, the immediate value is sign-extended to form a word or doubleword. The upper byte of the word is filled with the topmost bit of the immediate value.

imm16: an immediate word value used for instructions whose operand-size attribute is 16 bits. This is a number between -32768 and +32767 inclusive.

imm32: an immediate doubleword value used for instructions whose operand-size attribute is 32-bits. It allows the use of a number between +2147483647 and -2147483648.

r/m8: a one-byte operand that is either the contents of a byte register (REG_AL, REG_BL, REG_CL, REG_DL, REG_AH, REG_BH, REG_CH, REG_DH), or a byte from memory.

r/m16: a word register or memory operand used for instructions whose operand-size attribute is 16 bits. The word registers are: REG_AX, REG_BX, REG_CX, REG_DX, REG_SP, REG_BP, REG_SI, REG_DI. The contents of memory are found at the address provided by the effective address computation.

r/m32: a doubleword register or memory operand used for instructions whose operand-size attribute is 32-bits. The doubleword registers are: REG_EAX, REG_EBX, REG_ECX, REG_EDX, REG_ESP, REG_EBP, REG_ESI, REG_EDI. The contents of memory are found at the address provided by the effective address computation.

m8: a memory byte addressed by REG_DS:REG_SI or REG_ES:REG_DI (used only by string instructions).

m16: a memory word addressed by REG_DS:REG_SI or REG_ES:REG_DI (used only by string instructions).

m32: a memory doubleword addressed by REG_DS:REG_SI or REG_ES:REG_DI (used only by string instructions).

m16:16, M16:32: a memory operand containing a far pointer composed of two numbers. The number to the left of the colon corresponds to the pointer's segment selector. The number to the right corresponds to its offset.

m16 & 32, m16 & 16, m32 & 32: a memory operand consisting of data item pairs whose sizes are indicated on the left and the right side of the ampersand. All memory addressing modes are allowed. m16 & 16 and m32 & 32 operands are used by the BOUND instruction to provide an operand containing an upper and lower bounds for array indices. m16 & 32 is used by LIDT and LGDT to provide a word with which to load the limit field, and a doubleword with which to load the base field of the corresponding Global and Interrupt Descriptor Table Registers.

moffs8, moffs16, moffs32: (memory offset) a simple memory variable of type BYTE, WORD, or DWORD used by some variants of the MOV instruction. The actual address is given by a simple offset relative to the segment base. No ModR/M byte is used in the instruction. The number shown with moffs indicates its size, which is determined by the address-size attribute of the instruction.

Sreg: a segment register. The segment register bit assignments are REG_ES=0, REG_CS=1, REG_SS=2, REG_DS=3, REG_FS=4, and REG_GS=5.


Clocks extra info:
Clock counts for instructions that have an r/m (register or memory) operand are separated by a slash. The count to the left is used for a register operand; the count to the right is used for a memory operand.



*/



/*
Here will come the opcode functions when all interpreted.
*/



















//Extra stuff for opcode support!

void CPU_RET16(); //Ret in 16-bit mode!
void CPU_RET32(); //Ret in 32-bit mode!
void CPU_RETI16(); //Return from 16-bit interrupt!
void CPU_RETI32(); //Return from 32-bit interrupt!

//Used by the CPU itself:
void CPU_PUSH_CSEIP(); //SAVE REG_CS:REG_EIP
void CPU_PUSH_CSIP(); //SAVE REG_CS:REG_EIP

void CPU_CALL16(word segment, word offset); //16-bit call!
void CPU_CALL32(word segment, uint_32 offset); //32-bit call!
#endif