#ifndef DYNAMICIMAGE_H
#define DYNAMICIMAGE_H

#include "headers/types.h" //Basic types!

byte is_dynamicimage(char *filename); //Is dynamic image, 1=Dynamic, 0=Static/non-existant!
byte dynamicimage_writesector(char *filename,uint_32 sector, void *buffer); //Write a 512-byte sector! Result=1 on success, 0 on error!
byte dynamicimage_readsector(char *filename,uint_32 sector, void *buffer); //Read a 512-byte sector! Result=1 on success, 0 on error!
FILEPOS dynamicimage_getsize(char *filename);

FILEPOS generateDynamicImage(char *filename, FILEPOS size, int percentagex, int percentagey); //Generate dynamic image; returns size.
byte dynamicimage_hassector(char *filename,uint_32 sector); //Has a 512-byte sector! Result=1 on present&filled, 0 on not present or error! Used for simply copying the sector to a different image!
#endif