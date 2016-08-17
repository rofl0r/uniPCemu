#ifndef MULTITASKING_H
#define MULTITASKING_H

#include "headers/cpu/protection.h" //Basic protection types!

int TSS_PrivilegeChanges(int whatsegment,SEGDESCRIPTOR_TYPE *LOADEDDESCRIPTOR,word segment); //Error during privilege change = 1 else 0 on OK.
byte CPU_switchtask(int whatsegment, SEGDESCRIPTOR_TYPE *LOADEDDESCRIPTOR, word *segment, word destinationtask); //Switching to a certain task?

void CPU_TSSFault(uint_32 errorcode);

#endif