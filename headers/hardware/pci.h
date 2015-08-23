#ifndef __PCI_H
#define __PCI_H

#include "headers/types.h" //Basic types!

void initPCI();
void register_PCI(void *config, byte size);

#endif