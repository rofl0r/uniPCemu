#ifndef CPU_OP8086_H
#define CPU_OP8086_H

#include "headers/cpu/modrm.h" //MODR/M support!

//8086 processor used opcodes:

//Conditional JMPs opcodes:

//00+
void CPU8086_OP00();
void CPU8086_OP01();
void CPU8086_OP02();
void CPU8086_OP03();
void CPU8086_OP04();
void CPU8086_OP05();
void CPU8086_OP06();
void CPU8086_OP07();
void CPU8086_OP08();
void CPU8086_OP09();
void CPU8086_OP0A();
void CPU8086_OP0B();
void CPU8086_OP0C();
void CPU8086_OP0D();
void CPU8086_OP0E();
void CPU8086_OP0F();
//No 0F!
//10+
void CPU8086_OP10();
void CPU8086_OP11();
void CPU8086_OP12();
void CPU8086_OP13();
void CPU8086_OP14();
void CPU8086_OP15();
void CPU8086_OP16();
void CPU8086_OP17();
void CPU8086_OP18();
void CPU8086_OP19();
void CPU8086_OP1A();
void CPU8086_OP1B();
void CPU8086_OP1C();
void CPU8086_OP1D();
void CPU8086_OP1E();
void CPU8086_OP1F();
//20+
void CPU8086_OP20();
void CPU8086_OP21();
void CPU8086_OP22();
void CPU8086_OP23();
void CPU8086_OP24();
void CPU8086_OP25();
void CPU8086_OP26();
void CPU8086_OP27();
void CPU8086_OP28();
void CPU8086_OP29();
void CPU8086_OP2A();
void CPU8086_OP2B();
void CPU8086_OP2C();
void CPU8086_OP2D();
void CPU8086_OP2E();
void CPU8086_OP2F();
//30+
void CPU8086_OP30();
void CPU8086_OP31();
void CPU8086_OP32();
void CPU8086_OP33();
void CPU8086_OP34();
void CPU8086_OP35();
void CPU8086_OP36();
void CPU8086_OP37();
void CPU8086_OP38();
void CPU8086_OP39();
void CPU8086_OP3A();
void CPU8086_OP3B();
void CPU8086_OP3C();
void CPU8086_OP3D();
void CPU8086_OP3E();
void CPU8086_OP3F();
//40+
void CPU8086_OP40();
void CPU8086_OP41();
void CPU8086_OP42();
void CPU8086_OP43();
void CPU8086_OP44();
void CPU8086_OP45();
void CPU8086_OP46();
void CPU8086_OP47();
void CPU8086_OP48();
void CPU8086_OP49();
void CPU8086_OP4A();
void CPU8086_OP4B();
void CPU8086_OP4C();
void CPU8086_OP4D();
void CPU8086_OP4E();
void CPU8086_OP4F();
//50+
void CPU8086_OP50();
void CPU8086_OP51();
void CPU8086_OP52();
void CPU8086_OP53();
void CPU8086_OP54();
void CPU8086_OP55();
void CPU8086_OP56();
void CPU8086_OP57();
void CPU8086_OP58();
void CPU8086_OP59();
void CPU8086_OP5A();
void CPU8086_OP5B();
void CPU8086_OP5C();
void CPU8086_OP5D();
void CPU8086_OP5E();
void CPU8086_OP5F();
//No 60+
//void CPU8086_OP60(); //PUSHA
//void CPU8086_OP61(); //POPA
//70+ : Comparisions etc.
void CPU8086_OP70(); //JO rel8  : (FLAG_OF=1)
void CPU8086_OP71(); //JNO rel8 : (FLAG_OF=0)
void CPU8086_OP72(); //JB rel8  : (FLAG_CF=1)
void CPU8086_OP73(); //JNB rel8 : (FLAG_CF=0)
void CPU8086_OP74(); //JZ rel8  : (FLAG_ZF=1)
void CPU8086_OP75(); //JNZ rel8 : (FLAG_ZF=0)
void CPU8086_OP76(); //JBE rel8 : (FLAG_CF=1|FLAG_ZF=1)
void CPU8086_OP77(); //JA rel8  : (FLAG_CF=0&FLAG_ZF=0)
void CPU8086_OP78(); //JS rel8  : (FLAG_SF=1)
void CPU8086_OP79(); //JNS rel8 : (FLAG_SF=0)
void CPU8086_OP7A(); //JPE rel8 : (FLAG_PF=1)
void CPU8086_OP7B(); //JPO rel8 : (FLAG_PF=0)
void CPU8086_OP7C(); //JL rel8  : (FLAG_SF!=FLAG_OF)
void CPU8086_OP7D(); //JGE rel8 : (FLAG_SF=FLAG_OF)
void CPU8086_OP7E(); //JLE rel8 : (FLAG_ZF|(FLAG_SF!=FLAG_OF))
void CPU8086_OP7F(); //JG rel8  : ((FLAG_ZF=0)|FLAG_SF=FLAG_OF)
//80+
void CPU8086_OP80();
void CPU8086_OP81();
void CPU8086_OP82();
void CPU8086_OP83();
void CPU8086_OP84();
void CPU8086_OP85();
void CPU8086_OP86();
void CPU8086_OP87();
void CPU8086_OP88();
void CPU8086_OP89();
void CPU8086_OP8A();
void CPU8086_OP8B();
void CPU8086_OP8C();
void CPU8086_OP8D();
void CPU8086_OP8E();
void CPU8086_OP8F();
//90+
void CPU8086_OP90();
void CPU8086_OP91();
void CPU8086_OP92();
void CPU8086_OP93();
void CPU8086_OP94();
void CPU8086_OP95();
void CPU8086_OP96();
void CPU8086_OP97();
void CPU8086_OP98();
void CPU8086_OP99();
void CPU8086_OP9A();
void CPU8086_OP9B();
void CPU8086_OP9C();
void CPU8086_OP9D();
void CPU8086_OP9E();
void CPU8086_OP9F();
//A0+
void CPU8086_OPA0();
void CPU8086_OPA1();
void CPU8086_OPA2();
void CPU8086_OPA3();
void CPU8086_OPA4();
void CPU8086_OPA5();
void CPU8086_OPA6();
void CPU8086_OPA7();
void CPU8086_OPA8();
void CPU8086_OPA9();
void CPU8086_OPAA();
void CPU8086_OPAB();
void CPU8086_OPAC();
void CPU8086_OPAD();
void CPU8086_OPAE();
void CPU8086_OPAF();
//B0+
void CPU8086_OPB0();
void CPU8086_OPB1();
void CPU8086_OPB2();
void CPU8086_OPB3();
void CPU8086_OPB4();
void CPU8086_OPB5();
void CPU8086_OPB6();
void CPU8086_OPB7();
void CPU8086_OPB8();
void CPU8086_OPB9();
void CPU8086_OPBA();
void CPU8086_OPBB();
void CPU8086_OPBC();
void CPU8086_OPBD();
void CPU8086_OPBE();
void CPU8086_OPBF();
//C0+
void CPU8086_OPC2();
void CPU8086_OPC3();
void CPU8086_OPC4();
void CPU8086_OPC5();
void CPU8086_OPC6();
void CPU8086_OPC7();
void CPU8086_OPCA();
void CPU8086_OPCB();
void CPU8086_OPCC();
void CPU8086_OPCD();
void CPU8086_OPCE();
void CPU8086_OPCF();
//D0+
void CPU8086_OPD0();
void CPU8086_OPD1();
void CPU8086_OPD2();
void CPU8086_OPD3();
void CPU8086_OPD4();
void CPU8086_OPD5();
void CPU8086_OPD6();
void CPU8086_OPD7();
//E0+
void CPU8086_OPE0();
void CPU8086_OPE1();
void CPU8086_OPE2();
void CPU8086_OPE3();
void CPU8086_OPE4();
void CPU8086_OPE5();
void CPU8086_OPE6();
void CPU8086_OPE7();
void CPU8086_OPE8();
void CPU8086_OPE9();
void CPU8086_OPEA();
void CPU8086_OPEB();
void CPU8086_OPEC();
void CPU8086_OPED();
void CPU8086_OPEE();
void CPU8086_OPEF();
//F0+
void CPU8086_OPF0();
void CPU8086_OPF1();
void CPU8086_OPF2();
void CPU8086_OPF3();
void CPU8086_OPF4();
void CPU8086_OPF5();
void CPU8086_OPF6();
void CPU8086_OPF7();
void CPU8086_OPF8();
void CPU8086_OPF9();
void CPU8086_OPFA();
void CPU8086_OPFB();
void CPU8086_OPFC();
void CPU8086_OPFD();
void CPU8086_OPFE();
void CPU8086_OPFF();

//Stuff for CoProcessor minimum:


/*void CPU8086_OPD9(); //For FPU!
void CPU8086_OPDB(); //For FPU!
void CPU8086_OPDD(); //For FPU!
void CPU8086_OPDF(); //For FPU!
*/

void CPU8086_noCOOP(); //Coprosessor opcodes handler!

//Extra support:
word getLEA(MODRM_PARAMS *params);

void CPU086_int(byte interrupt); //Executes an hardware interrupt (from tbl)

/*
//For 80186+

void CPU8086_internal_RCL8(byte *dest, byte rotation);
void CPU8086_internal_RCL16(word *dest, word rotation);

//Shift right and carry flag to LSB
void CPU8086_internal_RCR8(byte *dest, byte rotation);
void CPU8086_internal_RCR16(word *dest, word rotation);


void CPU8086_internal_ROL8(byte *dest, byte rotation);
void CPU8086_internal_ROL16(word *dest, word rotation);

//Shift right
void CPU8086_internal_ROR8(byte *dest, byte rotation);
void CPU8086_internal_ROR16(word *dest, word rotation);


void CPU8086_internal_SHL8(byte *dest, byte rotation);
void CPU8086_internal_SHL16(word *dest, word rotation);

void CPU8086_internal_SHR8(byte *dest, byte rotation);
void CPU8086_internal_SHR16(word *dest, word rotation);

void CPU8086_internal_SAR8(byte *dest, byte rotation);
void CPU8086_internal_SAR16(word *dest, word rotation);
*/

//For GRP Opcodes!
void CPU8086_internal_INC16(word *reg);
void CPU8086_internal_DEC16(word *reg);

void unkOP_8086(); //Unknown opcode on 8086?

//Rest for GRP opcodes:
void op_add16();
void op_sub16();

#endif