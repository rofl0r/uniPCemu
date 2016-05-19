#ifndef __IDE_H
#define __IDE_H

void initATA();
void cleanATA(); //ATA timing reset!
void updateATA(double timepassed); //ATA timing!

#endif