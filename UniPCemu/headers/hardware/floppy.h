#ifndef FLOPPY_H
#define FLOPPY_H

#include "headers/types.h" //Basic types!

typedef struct
{
	word KB;
	word SPT;
	word sides;
	byte tracks;
	byte boardjumpersetting; //The board jumper setting (0-3) for this drive!
	byte measurement; //0=5", 1=3.5"
	byte supportedrates; //Up to 4 supported rates (2 bits per rate) with this format!
	//Stuff needed for formatting the generated floppy disks:
	byte MediaDescriptorByte; //The floppy media descriptor byte!
	word ClusterSize; //Cluster size, multiple of 512 bytes!
	word FATSize; //FAT Size in sectors
	word DirectorySize; //Directory size in entries
	//More stuff for accurate emulation of errors:
	byte DoubleDensity; //Are we a double density drive?
	byte GAPLength; //The default GAP length used by this format!
	byte TapeDriveRegister; //Our Tape Drive Register value for this disk!
	word RPM; //Speed, either 300 or 360 RPM!
} FLOPPY_GEOMETRY; //All floppy geometries!

#define NUMFLOPPYGEOMETRIES 13

void initFDC(); //Initialise the floppy disk controller!

byte floppy_spt(uint_64 floppy_size);
byte floppy_tracks(uint_64 floppy_size);
byte floppy_sides(uint_64 floppy_size);
uint_32 floppy_LBA(byte floppy, word side, word track, word sector);
void updateFloppy(DOUBLE timepassed);
#endif
