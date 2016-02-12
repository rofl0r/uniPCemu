#ifndef PAGING_H
#define PAGING_H

#include "headers/types.h"

int is_paging();
int isvalidpage(uint_32 address, byte iswrite, byte CPL); //Do we have paging without error? userlevel=CPL usually.
uint_32 mappage(uint_32 address); //Maps a page to real memory when needed!

#endif