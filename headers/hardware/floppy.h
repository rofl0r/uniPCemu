#ifndef FLOPPY_H
#define FLOPPY_H

#include "headers/types.h" //Basic types!

void initFDC(); //Initialise the floppy disk controller!

byte floppy_spt(uint_64 floppy_size);
byte floppy_tracks(uint_64 floppy_size);
byte floppy_sides(uint_64 floppy_size);
uint_32 floppy_LBA(int floppy, word side, word track, word sector);
#endif