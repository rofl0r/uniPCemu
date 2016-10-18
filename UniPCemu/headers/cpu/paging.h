#ifndef PAGING_H
#define PAGING_H

#include "headers/types.h"

int is_paging();
uint_32 mappage(uint_32 address); //Maps a page to real memory when needed!
byte CPU_Paging_checkPage(uint_32 address, byte readflags, byte CPL); //Do we have paging without error? userlevel=CPL usually.

#endif