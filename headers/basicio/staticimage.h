#ifndef STATICIMAGE_H
#define STATICIMAGE_H

#include "headers/types.h" //Basic types!

byte is_staticimage(char *filename); //Are we a static image?
byte staticimage_writesector(char *filename, uint_32 sector, void *buffer); //Write a 512-byte sector! Result=1 on success, 0 on error!
byte staticimage_readsector(char *filename,uint_32 sector, void *buffer); //Read a 512-byte sector! Result=1 on success, 0 on error!
FILEPOS staticimage_getsize(char *filename);

void generateStaticImage(char *filename, FILEPOS size, int percentagex, int percentagey); //Generate a static image!
void generateFloppyImage(char *filename, word KB, int percentagex, int percentagey); //Generate a floppy image!

#endif