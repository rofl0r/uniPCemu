#ifndef __IDE_H
#define __IDE_H

void initATA();
void cleanATA(); //ATA timing reset!
void updateATA(double timepassed); //ATA timing!

byte ATA_allowDiskChange(int disk, byte ejectRequested); //Are we allowing this disk to be changed?

//Geometry detection support for harddisks!
word get_SPT(int disk, uint_64 disk_size);
word get_heads(int disk, uint_64 disk_size);
word get_cylinders(int disk, uint_64 disk_size);

void HDD_classicGeometry(uint_64 disk_size, word *cylinders, word *heads, word *SPT);
void HDD_detectOptimalGeometry(uint_64 disk_size, word *cylinders, word *heads, word *SPT);

#endif