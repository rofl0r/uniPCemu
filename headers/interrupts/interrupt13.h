#ifndef INTERRUPT13_H
#define INTERRUPT13_H

#include "headers/types.h" //Basic type support!
void int13_init(int floppy0, int floppy1, int hdd0, int hdd1, int cdrom0, int cdrom1); //Initialise interrupt 13h functionality!

void BIOS_int13(); //Interrupt #13h: (Low Level Disk Services)! Overridable!
byte getdiskbymount(int drive); //Drive to disk converter (reverse of int13_init)!
uint floppy_bps(uint_32 floppy_size);
int floppy_spt(uint_32 floppy_size);
uint_32 disksize(int disknumber);

#endif