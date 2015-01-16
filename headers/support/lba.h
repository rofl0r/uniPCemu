#ifndef SUPPORT_H
#define SUPPORT_H
uint_32 CHS2LBA(word cylinder, byte head, byte sector, word nheads, uint_32 nsectors);
void LBA2CHS(uint_32 LBA, word *cylinder, byte *head, byte *sector, word nheads, uint_32 nsectors);
#endif