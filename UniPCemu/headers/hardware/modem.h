#ifndef MODEM_H
#define MODEM_H

#include "headers/types.h" //Basic types!

void initModem(byte enabled); //Initialise modem!
void doneModem(); //Finish modem!

void cleanModem();
void updateModem(uint_32 timepassed); //Sound tick. Executes every instruction.

#endif