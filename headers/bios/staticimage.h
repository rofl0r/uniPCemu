#ifndef STATICIMAGE_H
#define STATICIMAGE_H

#include "headers/types.h" //Basic types!

int staticimage_writesector(char *filename,uint_32 sector, void *buffer); //Write a 512-byte sector! Result=1 on success, 0 on error!
int staticimage_readsector(char *filename,uint_32 sector, void *buffer); //Read a 512-byte sector! Result=1 on success, 0 on error!
FILEPOS staticimage_getsize(char *filename);

void generateStaticImage(char *filename, FILEPOS size, int percentagex, int percentagey); //Generate a static image!

#endif