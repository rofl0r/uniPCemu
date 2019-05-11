#ifndef CPU_OP80586_H
#define CPU_OP80586_H

//Not emulated yet. Bare minimum instruction to run!

void CPU586_CPUID();
void unkOP0F_586();

void CPU586_OP0F30(); //WRMSR
void CPU586_OP0F31(); //RSTDC
void CPU586_OP0F32(); //RDMSR
void CPU586_OP0FC7(); //CMPXCHG8B r/m32
#endif