#ifndef IO_H
#define IO_H

#include "headers/types.h"
#include "headers/support/isoreader.h" //Need for structure!

typedef int (*SECTORHANDLER)(char *filename,uint_32 sector, void *buffer); //Write/read a 512-byte sector! Result=1 on success, 0 on error!

typedef struct
{
char filename[256]; //The filename of the disk!
uint_32 start; //Base positions of the images in the files!
byte readonly; //Readonly!
uint_32 size; //Disk size!
BOOTIMGINFO customdisk; //Boot image info!
byte dynamicimage; //Are we a dynamic image?
SECTORHANDLER readhandler, writehandler; //Read&write handlers!
} IODISK; //I/O mounted disk info.

//Basic img/ms0 input/output for BIOS I/O
#define UNMOUNTED -1
#define FLOPPY0 0x00
#define FLOPPY1 0x01
#define HDD0 0x02
#define HDD1 0x03
#define CDROM0 0x04
#define CDROM1 0x05

void ioInit(); //Resets/unmounts all disks!
FILEPOS filesize(FILE *f); //Get filesize!
void iofloppy0(char *filename, uint_64 startpos, byte readonly, uint_32 customsize);
void iofloppy1(char *filename, uint_64 startpos, byte readonly, uint_32 customsize);
void iohdd0(char *filename, uint_64 startpos, byte readonly, uint_32 customsize);
void iohdd1(char *filename, uint_64 startpos, byte readonly, uint_32 customsize);
void iocdrom0(char *filename, uint_64 startpos, byte readonly, uint_32 customsize);
void iocdrom1(char *filename, uint_64 startpos, byte readonly, uint_32 customsize);
int readdata(int device, void *buffer, uint_64 startpos, uint_32 bytestoread);
int writedata(int device, void *buffer, uint_64 startpos, uint_32 bytestowrite);
int has_drive(int drive); //Have drive?
int drivereadonly(int drive); //Drive is read-only?
FILEPOS getdisksize(int device); //Retrieve a dynamic/static image size!
uint_64 disksize(int disknumber); //Currently mounted disk size!
#endif