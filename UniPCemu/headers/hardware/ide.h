#ifndef __IDE_H
#define __IDE_H

void initATA();
void cleanATA(); //ATA timing reset!
void updateATA(double timepassed); //ATA timing!

byte ATA_allowDiskChange(int disk); //Are we allowing this disk to be changed?

//Geometry detection support for harddisks!
word get_SPT(uint_64 disk_size);
word get_heads(uint_64 disk_size);
word get_cylinders(uint_64 disk_size);

#endif