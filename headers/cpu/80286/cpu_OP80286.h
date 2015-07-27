#ifndef CPU_OP80286_H
#define CPU_OP80286_H

void generate_opcode0F_jmptbl();
void CPU_OP0F_286(); //Special 2-byte opcode (286+)?
void THROWDESCGP(word segment); //Throw a general protection exception!
#endif