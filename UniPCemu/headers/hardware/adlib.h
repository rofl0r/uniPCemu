#ifndef ADLIB_H
#define ADLIB_H

void initAdlib(); //Initialise adlib!
void doneAdlib(); //Finish adlib!

void cleanAdlib();
void updateAdlib(double timepassed, uint_32 MHZ14passed); //Sound tick. Executes every instruction.

//Special Sound Blaster support!
byte readadlibstatus();
void writeadlibaddr(byte value);
void writeadlibdata(byte value);

#endif