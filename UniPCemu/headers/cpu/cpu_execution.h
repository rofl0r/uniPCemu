#ifndef CPU_EXECUTION_H
#define CPU_EXECUTION_H
#include "headers/cpu/protection.h" //Protection typedef support!

void CPU_executionphase_newopcode(); //Starting a new opcode to handle?
void CPU_executionphase_startinterrupt(byte vectornr, byte type3, int_64 errorcode); //Starting a new interrupt to handle?
byte CPU_executionphase_starttaskswitch(int whatsegment, SEGDESCRIPTOR_TYPE *LOADEDDESCRIPTOR,word *segment, word destinationtask, byte isJMPorCALL, byte gated, int_64 errorcode); //Switching to a certain task?
void CPU_OP(); //Normal CPU opcode execution!
byte CPU_executionphase_busy(); //Are we busy(not ready to fetch a new instruction)?

#endif