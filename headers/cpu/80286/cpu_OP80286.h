#ifndef CPU_OP80286_H
#define CPU_OP80286_H

void unkOP0F_286(); //0F unknown opcode handler on 286+?
void CPU_OP0F_286(); //Special 2-byte opcode (286+)?
void THROWDESCGP(word segment); //Throw a general protection exception!
#endif