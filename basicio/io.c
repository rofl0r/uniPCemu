#include "headers/bios/io.h"
#include "headers/bios/dynamicimage.h" //Dynamic image support!
#include "headers/bios/staticimage.h" //Static image support!
#include "headers/emu/gpu/gpu.h" //Need GPU support for creating images!
#include "headers/support/log.h" //Logging support!
//Basic low level i/o functions!

IODISK disks[0x100]; //All disks available, up go 256 (drive 0-255) disks!

void ioInit() //Resets/unmounts all disks!
{
	memset(disks,0,sizeof(disks)); //Initialise disks!
}

int drivereadonly(int drive)
{
	if (drive<0 || drive>0xFF) return 1; //Readonly with unknown drives!
	switch (drive) //What drive?
	{
		case CDROM0:
			return 1; //CDROM always readonly!
		case CDROM1:
			return 1; //CDROM always readonly!
			break;
		default: //Any other drive?
			return disks[drive].readonly; //Read only?
			return 0; //Unknown, so NO!
	}
}

static FILEPOS getdisksize(int device) //Retrieve a dynamic/static image size!
{
	//Retrieve the disk size!
	byte dynamicimage;
	dynamicimage = is_dynamicimage(disks[device].filename); //Dynamic image?
	if (dynamicimage) //Dynamic image?
	{
		return dynamicimage_getsize(disks[device].filename); //Dynamic image size!
	}
	return staticimage_getsize(disks[device].filename); //Dynamic image size!
}

void loadDisk(int device, char *filename, uint_32 startpos, byte readonly, uint_32 customsize) //Disk mount routine!
{
	strcpy(disks[device].filename,filename); //Set file!
	disks[device].start = startpos; //Start pos!
	disks[device].readonly = readonly; //Read only!
	disks[device].size = (customsize>0)?customsize:getdisksize(device); //Get sizes!
	disks[device].dynamicimage = is_dynamicimage(filename); //Dynamic image?
	disks[device].readhandler = disks[device].dynamicimage?&dynamicimage_readsector:&staticimage_readsector; //What read sector function to use!
	disks[device].writehandler = disks[device].dynamicimage?&dynamicimage_writesector:&staticimage_writesector; //What write sector function to use!
}

void iofloppy0(char *filename, uint_32 startpos, byte readonly, uint_32 customsize)
{
	loadDisk(FLOPPY0,filename,startpos,readonly,customsize); //Load disk #0!
}

void iofloppy1(char *filename, uint_32 startpos, byte readonly, uint_32 customsize)
{
	loadDisk(FLOPPY1,filename,startpos,readonly,customsize); //Load disk #0!
}

void iohdd0(char *filename, uint_32 startpos, byte readonly, uint_32 customsize)
{
	loadDisk(HDD0,filename,startpos,readonly,customsize); //Load disk #0!
}

void iohdd1(char *filename, uint_32 startpos, byte readonly, uint_32 customsize)
{
	loadDisk(HDD1,filename,startpos,readonly,customsize); //Load disk #0!
}

void iocdrom0(char *filename, uint_32 startpos, byte readonly, uint_32 customsize)
{
	loadDisk(CDROM0,filename,startpos,readonly,customsize); //Load disk #0!
}

void iocdrom1(char *filename, uint_32 startpos, byte readonly, uint_32 customsize)
{
	loadDisk(CDROM1,filename,startpos,readonly,customsize); //Load disk #0!
}

#define TRUE 1
#define FALSE 0


//Startpos=sector number (start/512 bytes)!
int readdata(int device, void *buffer, uint_32 startpos, uint_32 bytestoread)
{
	if ((device&0xFF)!=device) //Invalid device?
	{
		dolog("IO","io.c: Unknown device: %i!",device);
		return FALSE; //Unknown device!
	}

	char dev[256]; //Our device!
	bzero(dev,sizeof(dev)); //Init device string!

	FILEPOS readpos; //Read pos!
	FILEPOS basepos; //Base pos!
	basepos = disks[device].start;
	if (disks[device].customdisk.used) //Custom disk?
	{
		if (startpos+bytestoread<=disks[device].customdisk.imagesize) //Within bounds?
		{
			return readdata(disks[device].customdisk.device,buffer,disks[device].customdisk.startpos,bytestoread); //Read from custom disk!
		}
		else
		{
			return FALSE; //Out of bounds!
		}
	}
	strcpy(dev,disks[device].filename); //Use floppy0!
		
		
	if (strcmp(dev,"")==0) //Failed to open or not assigned
	{
		//dolog("IO","io.c: Device couldnt be opened or isn't mounted: %i!",device);
		return FALSE; //Error: device not found!
	}

	readpos = basepos+startpos; //Startpos!

	uint_32 sector; //Current sector!
	sector = readpos/512; //The sector we need must be a multiple of 512 bytes (standard sector size)!
	FILEPOS bytesread = 0; //Init bytesread!
	
	SECTORHANDLER handler = disks[device].readhandler; //Our handler!
	for (;bytesread<bytestoread;) //Still left to read?
	{
		if (!handler(dev,sector,buffer+bytesread)) //Append at the buffer failed!
		{
			if (disks[device].dynamicimage) //Dynamic?
			{
				dolog("IO","io.c: Couldn't read dynamic image %s sector %i",dev,sector);
			}
			else //Static?
			{
				dolog("IO","io.c: Couldn't read static image %s sector %i",dev,sector);
			}
			return 0; //Error!
		}
		bytesread += 512; //1 sector read!
		++sector; //Next sector!
	}
	
	return 1; //OK!
}

int has_drive(int drive) //Device inserted?
{
	if (drive<0 || drive>0xFF) return 0; //No disk available!
	byte buf[512];
	if (!readdata(drive,&buf,0,512)) //First sector invalid?
	{
		return FALSE; //No drive!
	}
	return TRUE; //Have drive!
}

int writedata(int device, void *buffer, uint_32 startpos, uint_32 bytestowrite)
{
	char dev[256]; //Our device!
	bzero(dev,sizeof(dev)); //Init device string!
	uint_32 writepos; //Read pos!
	uint_32 basepos; //Base pos!
	byte readonly; //Read only?

	if ((device&0xFF)!=device) //Invalid device?
	{
		dolog("IO","io.c: Unknown device: %i!",device);
		return FALSE; //Unknown device!
	}
	
	if (disks[device].customdisk.used) //Read only custom disk passthrough?
	{
		return FALSE;
	}
	//Load basic data!
	basepos = disks[device].start;
	readonly = disks[device].readonly;
	strcpy(dev,disks[device].filename); //Use this!

	if (strcmp(dev,"")==0) //Disk not found?
	{
		return FALSE; //Disk not found!
	}

	if (readonly) //Read only (not allowed to write?)
	{
		return FALSE; //No writing allowed!
	}

	writepos = basepos + startpos; //Place to write!

	uint_32 sector; //Current sector!
	sector = writepos/512; //The sector we need!
	FILEPOS byteswritten = 0; //Init byteswritten!

	SECTORHANDLER handler = disks[device].writehandler; //Our handler!

	for (;byteswritten<bytestowrite;) //Still left to written?
	{
		if (!handler(dev,sector,buffer+byteswritten)) //Write failed!
		{
			if (disks[device].dynamicimage) //Dynamic image?
			{
				dolog("IO","Error writing sector #%i to dynamic image %s",sector,dev);
			}
			else //Static image?
			{
				dolog("IO","Error writing sector #%i to static image %s",sector,dev);
			}
			return 0; //Error!
		}
		//Successfully written?
		byteswritten += 512; //1 sector written!
		++sector; //Next sector!
	}
	
	return 1; //OK!
}