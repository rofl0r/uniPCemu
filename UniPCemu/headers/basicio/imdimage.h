#ifndef IMDIMAGE_H
#define IMDIMAGE_H

#include "headers/types.h" //Type definitions!

enum {
	DATAMARK_NORMALDATA = 0,
	DATAMARK_DAM = 1,
	DATAMARK_DATAERROR = 2,
	DATAMARK_DELETEDDATA = 3,
	DATAMARK_INVALID = 0xFF
};

//Information about a track and sector!
typedef struct
{
	byte MFM_speedmode; //MFM/speed mode! <3=FM, >=3=MFM.
	byte sectorID; //Physical sector number!
	byte headnumber; //Physical head number!
	byte cylinderID; //Physical cylinder ID!
	word sectorsize; //Actual sector size!
	byte sectortype; //Extra information about the physical sector type's attributes!
	word totalsectors; //Total amount of physical sectors on this track!
	byte datamark; //What is the data marked as! 
} IMDIMAGE_SECTORINFO;

byte is_IMDimage(char* filename); //Are we a IMD image?
byte readIMDSectorInfo(char* filename, byte side, byte track, byte sector, IMDIMAGE_SECTORINFO* result);
byte readIMDSector(char* filename, byte side, byte track, byte sector, word sectorsize, void* result);

#endif