#ifndef MULTITASKING_H
#define MULTITASKING_H

#include "headers/cpu/protection.h" //Basic protection types!

byte CPU_switchtask(int whatsegment, SEGDESCRIPTOR_TYPE *LOADEDDESCRIPTOR,word *segment, word destinationtask, byte isJMPorCALL, byte gated, byte is_external); //Switching to a certain task?

void CPU_TSSFault(word segmentval, byte is_external, byte tbl);

#endif