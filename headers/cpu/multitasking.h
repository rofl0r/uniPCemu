#ifndef MULTITASKING_H
#define MULTITASKING_H

#include "headers/cpu/protection.h" //Basic protection types!

int TSS_PrivilegeChanges(int whatsegment,SEGDESCRIPTOR_TYPE *LOADEDDESCRIPTOR,word segment); //Error during privilege change = 1 else 0 on OK.

#endif