#ifndef PAGING_H
#define PAGING_H

#include "headers/types.h"

#define is_paging() CPU[activeCPU].is_paging
uint_32 mappage(uint_32 address, byte iswrite, byte CPL); //Maps a page to real memory when needed!
byte CPU_Paging_checkPage(uint_32 address, byte readflags, byte CPL); //Do we have paging without error? userlevel=CPL usually.

typedef struct
{
	uint_32 data; //80386 4-way associative TLB results!
	uint_32 TAG; //All TAGs used with the respective TLB!
	void *TLB_listnode; //The list node we're associated to!
} TLBEntry;

typedef struct
{
	TLBEntry *entry; //What entry are we?
	byte index; //What index is said entry?
	void *prev, *next; //Previous and next pointers!
	byte allocated; //Are we allocated?
} TLB_ptr;

typedef struct
{
	TLBEntry TLB[64]; //All TLB entries to use!
	TLB_ptr TLB_listnodes[64]; //All nodes for all TLB entries!
	TLB_ptr *TLB_freelist_head[16], *TLB_freelist_tail[16]; //Head and tail of the free list!
	TLB_ptr *TLB_usedlist_head[16], *TLB_usedlist_tail[16]; //Head and tail of the used list!
} CPU_TLB; //A TLB to use for the CPU!

void Paging_clearTLB(); //Clears the TLB for further fetching!
void Paging_writeTLB(sbyte TLB_way, uint_32 logicaladdress, byte W, byte U, byte D, byte S, uint_32 result);
byte Paging_readTLB(byte *TLB_way, uint_32 logicaladdress, byte W, byte U, byte D, byte S, uint_32 WDMask, uint_32 *result, byte updateAges);
void Paging_initTLB(); //Initialize the Paging TLB!
void Paging_Invalidate(uint_32 logicaladdress); //Invalidate a single address!
void Paging_TestRegisterWritten(byte TR); //A Test Register has been written to?

#endif
