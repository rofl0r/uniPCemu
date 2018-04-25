#ifndef MODEM_H
#define MODEM_H

#include "headers/types.h" //Basic types!

void initModem(byte enabled); //Initialise modem!
void doneModem(); //Finish modem!

void cleanModem();
void updateModem(DOUBLE timepassed); //Sound tick. Executes every instruction.
void initPcap(); //PCAP initialization, when supported!
void termPcap(); //PCAP termination, when supported!

#endif