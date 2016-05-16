#ifndef ADLIB_H
#define ADLIB_H

void initAdlib(); //Initialise adlib!
void doneAdlib(); //Finish adlib!

void cleanAdlib();
void updateAdlib(double timepassed); //Sound tick. Executes every instruction.

#endif