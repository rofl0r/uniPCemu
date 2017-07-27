#ifndef PAGING_H
#define PAGING_H

#include "headers/types.h"

byte is_paging();
uint_32 mappage(uint_32 address, byte iswrite, byte CPL); //Maps a page to real memory when needed!
byte CPU_Paging_checkPage(uint_32 address, byte readflags, byte CPL); //Do we have paging without error? userlevel=CPL usually.

typedef struct
{
	uint_32 data; //80386 4-way associative TLB results!
	uint_32 TAG; //All TAGs used with the respective TLB!
	double age; //The age of a TAG, which is incremented in time, reset by accesses to it!
} TLBEntry;

typedef struct
{
	TLBEntry TLB[4][8]; //All TLB entries to use!
} CPU_TLB; //A TLB to use for the CPU!

void Paging_clearTLB(); //Clears the TLB for further fetching!
void Paging_writeTLB(uint_32 logicaladdress, byte RW, byte US, uint_32 result);
byte Paging_readTLB(uint_32 logicaladdress, byte RW, byte US, uint_32 *result);
void Paging_ticktime(double timepassed); //Update Paging timers for determining the oldest entry!
#endif