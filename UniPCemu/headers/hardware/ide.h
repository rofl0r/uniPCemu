/*

Copyright (C) 2019  Superfury

This file is part of UniPCemu.

UniPCemu is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

UniPCemu is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with UniPCemu.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef __IDE_H
#define __IDE_H

void initATA();
void doneATA();
void cleanATA(); //ATA timing reset!
void updateATA(DOUBLE timepassed); //ATA timing!

byte ATA_allowDiskChange(int disk, byte ejectRequested); //Are we allowing this disk to be changed?

//Geometry detection support for harddisks!
word get_SPT(int disk, uint_64 disk_size);
word get_heads(int disk, uint_64 disk_size);
word get_cylinders(int disk, uint_64 disk_size);

void HDD_classicGeometry(uint_64 disk_size, word *cylinders, word *heads, word *SPT);
void HDD_detectOptimalGeometry(uint_64 disk_size, word *cylinders, word *heads, word *SPT);

#endif