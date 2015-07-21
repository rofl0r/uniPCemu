#include "headers/bios/io.h"
#include "headers/bios/dynamicimage.h" //Dynamic image support!
#include "headers/bios/staticimage.h" //Static image support!
#include "headers/bios/dskimage.h" //DSK image support!
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
			return ((disks[drive].readonly) || (disks[drive].customdisk.used)); //Read only or custom disk = read only!
	}
}

FILEPOS getdisksize(int device) //Retrieve a dynamic/static image size!
{
	//Retrieve the disk size!
	byte dynamicimage;
	dynamicimage = disks[device].dynamicimage; //Dynamic image?
	if (dynamicimage) //Dynamic image?
	{
		return dynamicimage_getsize(disks[device].filename); //Dynamic image size!
	}
	return staticimage_getsize(disks[device].filename); //Dynamic image size!
}

void register_DISKCHANGE(int device, DISKCHANGEDHANDLER diskchangedhandler) //Register a disk changed handler!
{
	switch (device)
	{
	case FLOPPY0:
	case FLOPPY1:
	case HDD0:
	case HDD1:
	case CDROM0:
	case CDROM1:
		disks[device].diskchangedhandler = diskchangedhandler; //Register disk changed handler!
		break;
	default: //Unknown disk?
		break;
	}
}

void loadDisk(int device, char *filename, uint_64 startpos, byte readonly, uint_32 customsize) //Disk mount routine!
{
	byte dynamicimage = is_dynamicimage(filename); //Dynamic image detection!
	if (!dynamicimage) //Might be a static image when not a dynamic image?
	{
		if (!is_DSKimage(filename)) //Not a DSK image?
		{
			if (!is_staticimage(filename)) //Not a static image? We're invalid!
			{
				memset(&disks[device], 0, sizeof(disks[device])); //Delete the entry!
				return; //Abort!
			}
		}
	}

	if (disks[device].diskchangedhandler)
	{
		if (strcmp(disks[device].filename, filename) != 0) //Different disk?
		{
			disks[device].diskchangedhandler(device); //This disk has been changed!
		}
	}

	strcpy(disks[device].filename, filename); //Set file!
	disks[device].start = startpos; //Start pos!
	disks[device].readonly = readonly; //Read only!
	disks[device].dynamicimage = dynamicimage; //Dynamic image!
	disks[device].DSKimage = dynamicimage ? 0 : is_DSKimage(filename); //DSK image?
	disks[device].size = (customsize>0) ? customsize : getdisksize(device); //Get sizes!
	disks[device].readhandler = disks[device].DSKimage?NULL:(disks[device].dynamicimage?&dynamicimage_readsector:&staticimage_readsector); //What read sector function to use!
	disks[device].writehandler = disks[device].DSKimage?NULL:(disks[device].dynamicimage?&dynamicimage_writesector:&staticimage_writesector); //What write sector function to use!
}

void iofloppy0(char *filename, uint_64 startpos, byte readonly, uint_32 customsize)
{
	loadDisk(FLOPPY0,filename,startpos,readonly,customsize); //Load disk #0!
}

void iofloppy1(char *filename, uint_64 startpos, byte readonly, uint_32 customsize)
{
	loadDisk(FLOPPY1,filename,startpos,readonly,customsize); //Load disk #0!
}

void iohdd0(char *filename, uint_64 startpos, byte readonly, uint_32 customsize)
{
	loadDisk(HDD0,filename,startpos,readonly,customsize); //Load disk #0!
}

void iohdd1(char *filename, uint_64 startpos, byte readonly, uint_32 customsize)
{
	loadDisk(HDD1,filename,startpos,readonly,customsize); //Load disk #0!
}

void iocdrom0(char *filename, uint_64 startpos, byte readonly, uint_32 customsize)
{
	loadDisk(CDROM0,filename,startpos,readonly,customsize); //Load disk #0!
}

void iocdrom1(char *filename, uint_64 startpos, byte readonly, uint_32 customsize)
{
	loadDisk(CDROM1,filename,startpos,readonly,customsize); //Load disk #0!
}

uint_64 disksize(int disknumber)
{
	if (disknumber<0 || disknumber>6) return 0; //Not used!
	return disks[disknumber].size; //Get the size of the disk!
}


#define TRUE 1
#define FALSE 0

char *getDSKimage(int drive)
{
	if (drive<0 || drive>0xFF) return NULL; //Readonly with unknown drives!
	return disks[drive].DSKimage?&disks[drive].filename[0]:NULL; //Filename for DSK images, NULL otherwise!
}

//Startpos=sector number (start/512 bytes)!
int readdata(int device, void *buffer, uint_64 startpos, uint_32 bytestoread)
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
	readpos = disks[device].start; //Base position!
	readpos += startpos; //Start position!
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

	uint_32 sector; //Current sector!
	sector = (readpos>>9); //The sector we need must be a multiple of 512 bytes (standard sector size)!
	FILEPOS bytesread = 0; //Init bytesread!
	
	SECTORHANDLER handler = disks[device].readhandler; //Our handler!
	if (!handler) return 0; //Error: no handler registered!

	for (; bytesread<bytestoread;) //Still left to read?
	{
		if (!handler(dev,sector,(byte *)buffer+bytesread)) //Append at the buffer failed!
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

int writedata(int device, void *buffer, uint_64 startpos, uint_32 bytestowrite)
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
		return FALSE; //Read only link!
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

	writepos = basepos; //Base position!
	writepos += startpos; //Start position!

	uint_32 sector; //Current sector!
	sector = (writepos>>9); //The sector we need!
	FILEPOS byteswritten = 0; //Init byteswritten!

	SECTORHANDLER handler = disks[device].writehandler; //Our handler!
	if (!handler) return 0; //Error: no handler registered!

	for (;byteswritten<bytestowrite;) //Still left to written?
	{
		if (!handler(dev,sector,(byte *)buffer+byteswritten)) //Write failed!
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