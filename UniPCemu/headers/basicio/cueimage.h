#ifndef CUEIMAGE_H
#define CUEIMAGE_H

#include "headers/types.h" //Basic types!

enum CDROM_MODES {
	MODE_AUDIO = 0,
	MODE_KARAOKE = 1,
	MODE_MODE1DATA = 2,
	MODE_MODEXA = 3,
	MODE_MODECDI = 4,
};

byte is_cueimage(char *filename);
FILEPOS cueimage_getsize(char *filename);
//Results of the below functions: -1: Sector not found, 0: Error, 1: Aborted(no buffer), 2+CDROM_MODES: Read a sector of said mode + 2.
sbyte cueimage_readsector(int device, byte M, byte S, byte F, void *buffer, word size); //Read a n-byte sector! Result=Type on success, 0 on error, -1 on not found!
sbyte cueimage_getgeometry(int device, byte *M, byte *S, byte *F, byte *startM, byte *startS, byte *startF, byte *endM, byte *endS, byte *endF); //Result=Type on success, 0 on error, -1 on not found!

#endif