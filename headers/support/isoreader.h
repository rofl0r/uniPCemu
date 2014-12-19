#ifndef ISOREADER_H
#define ISOREADER_H
#include "headers/types.h"
typedef struct
{
	int device; //The device used!
	uint_32 startpos; //Startpos within the file!
	uint_32 imagesize; //The size of the image!
	int used; //Is this info used (0 for normal image, 1 for read-only BOOTIMGINFO)
} BOOTIMGINFO;

int getBootImage(int device, char *imagefile); //Returns TRUE on bootable (image written to imagefile), else FALSE!
int getBootImageInfo(int device, BOOTIMGINFO *imagefile); //Returns TRUE on bootable (image written to imagefile), else FALSE!
#endif