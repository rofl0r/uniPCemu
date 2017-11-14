#ifndef DYNAMICIMAGE_H
#define DYNAMICIMAGE_H

#include "headers/types.h" //Basic types!

byte is_dynamicimage(char *filename); //Is dynamic image, 1=Dynamic, 0=Static/non-existant!
byte dynamicimage_writesector(char *filename,uint_32 sector, void *buffer); //Write a 512-byte sector! Result=1 on success, 0 on error!
byte dynamicimage_readsector(char *filename,uint_32 sector, void *buffer); //Read a 512-byte sector! Result=1 on success, 0 on error!
FILEPOS dynamicimage_getsize(char *filename);
byte dynamicimage_getgeometry(char *filename, word *cylinders, word *heads, word *SPT);

FILEPOS generateDynamicImage(char *filename, FILEPOS size, int percentagex, int percentagey); //Generate dynamic image; returns size.
byte dynamicimage_readexistingsector(char *filename,uint_32 sector, void *buffer); //Has a 512-byte sector! Result=1 on present&filled(buffer filled), 0 on not present or error! Used for simply copying the sector to a different image!
sbyte dynamicimage_nextallocatedsector(char *filename, uint_32 *sector); //Finds the next allocated sector. 0=EOF reached, 1=Found sector, -1=Invalid or corrupt file.
#endif