#ifndef CPU_OP80286_H
#define CPU_OP80286_H

void generate_opcode0F_jmptbl();
void unkOP0F_286(); //0F unknown opcode handler on 286+?

//The 80286 instructions themselves!
void CPU286_OP63(); //ARPL r/m16,r16
void CPU286_OPD6(); //286+ SALC

//0F opcodes!
void CPU286_OP0F00(); //Various extended 286+ instructions GRP opcode.
void CPU286_OP0F01(); //Various extended 286+ instruction GRP opcode.
void CPU286_OP0F02(); //LAR /r
void CPU286_OP0F03(); //LSL /r
void CPU286_OP0F06(); //CLTS
void CPU286_OP0F0B(); //#UD instruction
void CPU286_OP0FB9(); //#UD instruction

void CPU286_OPF1(); //Undefined opcode, Don't throw any exception!

//FPU opcodes!
void FPU80287_OPDB();
void FPU80287_OPDF();
void FPU80287_OPDD();
void FPU80287_OPD9();

void FPU80287_noCOOP(); //Rest opcodes for the Escaped instructions!

#endif