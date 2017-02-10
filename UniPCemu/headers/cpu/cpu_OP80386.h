#ifndef CPU_OP80386_H
#define CPU_OP80386_H

void unkOP0F_386(); //0F unknown opcode handler on 386+?

void CPU80386_OP01();
void CPU80386_OP03();
void CPU80386_OP05();
void CPU80386_OP09();
void CPU80386_OP0B();
void CPU80386_OP0D();
void CPU80386_OP11();
void CPU80386_OP13();
void CPU80386_OP15();
void CPU80386_OP19();
void CPU80386_OP1B();
void CPU80386_OP1D();
void CPU80386_OP21();
void CPU80386_OP23();
void CPU80386_OP25();
void CPU80386_OP27();
void CPU80386_OP29();
void CPU80386_OP2B();
void CPU80386_OP2D();
void CPU80386_OP2F();
void CPU80386_OP31();
void CPU80386_OP33();
void CPU80386_OP35();
void CPU80386_OP37();
void CPU80386_OP39();
void CPU80386_OP3B();
void CPU80386_OP3D();
void CPU80386_OP3F();
void CPU80386_OP40();
void CPU80386_OP41();
void CPU80386_OP42();
void CPU80386_OP43();
void CPU80386_OP44();
void CPU80386_OP45();
void CPU80386_OP46();
void CPU80386_OP47();
void CPU80386_OP48();
void CPU80386_OP49();
void CPU80386_OP4A();
void CPU80386_OP4B();
void CPU80386_OP4C();
void CPU80386_OP4D();
void CPU80386_OP4E();
void CPU80386_OP4F();
void CPU80386_OP50();
void CPU80386_OP51();
void CPU80386_OP52();
void CPU80386_OP53();
void CPU80386_OP54();
void CPU80386_OP55();
void CPU80386_OP56();
void CPU80386_OP57();
void CPU80386_OP58();
void CPU80386_OP59();
void CPU80386_OP5A();
void CPU80386_OP5B();
void CPU80386_OP5C();
void CPU80386_OP5D();
void CPU80386_OP5E();
void CPU80386_OP5F();
void CPU80386_OP85();
void CPU80386_OP87();
void CPU80386_OP89();
void CPU80386_OP8B();
void CPU80386_OP8D();
void CPU80386_OP90();
void CPU80386_OP91();
void CPU80386_OP92();
void CPU80386_OP93();
void CPU80386_OP94();
void CPU80386_OP95();
void CPU80386_OP96();
void CPU80386_OP97();
void CPU80386_OP98();
void CPU80386_OP99();
void CPU80386_OP9A();

//Our two calling methods for handling the jumptable!
//16-bits versions having a new 32-bit address size override!
void CPU80386_OPA0_16();
void CPU80386_OPA1_16();
void CPU80386_OPA2_16();
void CPU80386_OPA3_16();
//32-bits versions having a new 32-bit address size override and operand size override, except 8-bit instructions!
void CPU80386_OPA1_32();
void CPU80386_OPA3_32();

//Normal instruction again!
void CPU80386_OPA5();
void CPU80386_OPA7();
void CPU80386_OPA9();
void CPU80386_OPAB();
void CPU80386_OPAD();
void CPU80386_OPAF();
void CPU80386_OPB8();
void CPU80386_OPB9();
void CPU80386_OPBA();
void CPU80386_OPBB();
void CPU80386_OPBC();
void CPU80386_OPBD();
void CPU80386_OPBE();
void CPU80386_OPBF();
void CPU80386_OPC2();
void CPU80386_OPC3();
void CPU80386_OPC4();
void CPU80386_OPC5();
void CPU80386_OPC7();
void CPU80386_OPCA();
void CPU80386_OPCB();
void CPU80386_OPCC();
void CPU80386_OPCD();
void CPU80386_OPCE();
void CPU80386_OPCF();
void CPU80386_OPD4();
void CPU80386_OPD5();
void CPU80386_OPD6();
void CPU80386_OPD7();
void CPU80386_OPE0();
void CPU80386_OPE1();
void CPU80386_OPE2();
void CPU80386_OPE3();
void CPU80386_OPE5();
void CPU80386_OPE7();
void CPU80386_OPE8();
void CPU80386_OPE9();
void CPU80386_OPEA();
void CPU80386_OPEB();
void CPU80386_OPED();
void CPU80386_OPEF();
void CPU80386_OPF1();

/*

NOW COME THE GRP1-5 OPCODES:

*/

void CPU80386_OP81(); //GRP1 Ev,Iv
void CPU80386_OP83(); //GRP1 Ev,Ib
void CPU80386_OP8F(); //Undocumented GRP opcode 8F r/m32
void CPU80386_OPD1(); //GRP2 Ev,1
void CPU80386_OPD3(); //GRP2 Ev,CL
void CPU80386_OPF7(); //GRP3b Ev
void CPU80386_OPFF(); //GRP5 Ev

void unkOP_80386(); //Unknown opcode on 8086?

/*

80186 32-bit extensions

*/

void CPU386_OP60();
void CPU386_OP61();
void CPU386_OP62();
void CPU386_OP68();
void CPU386_OP69();
void CPU386_OP6B();
void CPU386_OP6D();

void CPU386_OP6F();

void CPU386_OPC1(); //GRP2 Ev,Ib

void CPU386_OPC8(); //ENTER Iw,Ib
void CPU386_OPC9(); //LEAVE

/*

No 80286 normal extensions exist.

*/

/*

0F opcodes of the 80286, extended.

*/

void CPU386_OP0F01(); //Various extended 286+ instruction GRP opcode.
void CPU386_OP0F07(); //Undocumented LOADALL instruction

extern byte didJump; //Did we jump?

//New: 16-bit and 32-bit variants of OP70-7F as a 0F opcode!
//16-bits variant
void CPU80386_OP0F80_16();
void CPU80386_OP0F81_16();
void CPU80386_OP0F82_16();
void CPU80386_OP0F83_16();
void CPU80386_OP0F84_16();
void CPU80386_OP0F85_16();
void CPU80386_OP0F86_16();
void CPU80386_OP0F87_16();
void CPU80386_OP0F88_16();
void CPU80386_OP0F89_16();
void CPU80386_OP0F8A_16();
void CPU80386_OP0F8B_16();
void CPU80386_OP0F8C_16();
void CPU80386_OP0F8D_16();
void CPU80386_OP0F8E_16();
void CPU80386_OP0F8F_16();
//32-bits variant
void CPU80386_OP0F80_32();
void CPU80386_OP0F81_32();
void CPU80386_OP0F82_32();
void CPU80386_OP0F83_32();
void CPU80386_OP0F84_32();
void CPU80386_OP0F85_32();
void CPU80386_OP0F86_32();
void CPU80386_OP0F87_32();
void CPU80386_OP0F88_32();
void CPU80386_OP0F89_32();
void CPU80386_OP0F8A_32();
void CPU80386_OP0F8B_32();
void CPU80386_OP0F8C_32();
void CPU80386_OP0F8D_32();
void CPU80386_OP0F8E_32();
void CPU80386_OP0F8F_32();

#endif