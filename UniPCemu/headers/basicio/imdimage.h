#ifndef IMDIMAGE_H
#define IMDIMAGE_H

#include "headers/types.h" //Type definitions!

//Datamark values
enum {
	DATAMARK_NORMALDATA = 0,
	DATAMARK_DAM = 1,
	DATAMARK_DATAERROR = 2,
	DATAMARK_DELETEDDATA = 3,
	DATAMARK_INVALID = 0xFF
};

//MFM_speedmode defines. All but FORMATTED_MFM are used 
enum {
	FORMAT_SPEED_500 = 0, //Low 2 bits
	FORMAT_SPEED_300 = 1, //Low 2 bits
	FORMAT_SPEED_250 = 2, //Low 2 bits
	FORMAT_SPEED_1M = 3, //Low 2 bits
	FORMATTED_MFM = 4, //Single bitflag that's set in the mode!
};

//Formatting FM/MFM parameter
enum {
	FORMATTING_FM = 0, //Formatting as FM
	FORMATTING_MFM = 1 //Formatting as MFM
};

//Information about a track and sector!
typedef struct
{
	byte MFM_speedmode; //MFM/speed mode! <3=FM, >=3=MFM.
	byte sectorID; //Physical sector number!
	byte headnumber; //Physical head number!
	byte cylinderID; //Physical cylinder ID!
	word sectorsize; //Actual sector size!
	word totalsectors; //Total amount of physical sectors on this track!
	byte datamark; //What is the data marked as! 
} IMDIMAGE_SECTORINFO;

byte is_IMDimage(char* filename); //Are we a IMD image?
byte readIMDDiskInfo(char* filename, IMDIMAGE_SECTORINFO* result);
byte readIMDSectorInfo(char* filename, byte track, byte head, byte sector, IMDIMAGE_SECTORINFO* result);
byte readIMDSector(char* filename, byte track, byte head, byte sector, word sectorsize, void* result);
byte writeIMDSector(char* filename, byte track, byte head, byte sector, byte deleted, word sectorsize, void* sectordata);
byte formatIMDTrack(char* filename, byte track, byte head, byte MFM, byte speed, byte filldata, byte numsectors, byte* sectordata);

#endif