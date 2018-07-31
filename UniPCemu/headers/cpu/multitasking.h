#ifndef MULTITASKING_H
#define MULTITASKING_H

#include "headers/cpu/protection.h" //Basic protection types!

byte CPU_switchtask(int whatsegment, SEGMENT_DESCRIPTOR *LOADEDDESCRIPTOR,word *segment, word destinationtask, byte isJMPorCALL, byte gated, int_64 errorcode); //Switching to a certain task?

void CPU_TSSFault(word segmentval, byte is_external, byte tbl);

#endif