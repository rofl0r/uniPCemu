#ifndef CUEIMAGE_H
#define CUEIMAGE_H

#include "headers/types.h" //Basic types!

byte is_cueimage(char *filename);
FILEPOS cueimage_getsize(char *filename);
sbyte cueimage_readsector(int device, byte M, byte S, byte F, void *buffer, word size); //Read a n-byte sector! Result=Type on success, 0 on error, -1 on not found!
sbyte cueimage_getgeometry(int device, byte *M, byte *S, byte *F); //Result=Type on success, 0 on error, -1 on not found!

#endif