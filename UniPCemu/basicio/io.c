#include "headers/basicio/io.h"
#include "headers/basicio/dynamicimage.h" //Dynamic image support!
#include "headers/basicio/staticimage.h" //Static image support!
#include "headers/basicio/dskimage.h" //DSK image support!
#include "headers/emu/gpu/gpu.h" //Need GPU support for creating images!
#include "headers/support/log.h" //Logging support!
//Basic low level i/o functions!

char diskpath[256] = "disks"; //The full disk path of the directory containing the disk images!

IODISK disks[0x100]; //All disks available, up go 256 (drive 0-255) disks!

void ioInit() //Resets/unmounts all disks!
{
	memset(&disks,0,sizeof(disks)); //Initialise disks!
}

byte drivereadonly(int drive)
{
	if (drive<0 || drive>0xFF) return 1; //Readonly with unknown drives!
	switch (drive) //What drive?
	{
		case CDROM0:
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

OPTINLINE void loadDisk(int device, char *filename, uint_64 startpos, byte readonly, uint_32 customsize) //Disk mount routine!
{
	char fullfilename[256]; //Full filename of the mount!
	memset(&fullfilename,0,sizeof(fullfilename));
	strcpy(fullfilename,diskpath); //Load the disk path!
	strcat(fullfilename,"/");
	strcat(fullfilename,filename); //The full filename to use!
	if (strcmp(filename, "")==0) //No filename specified?
	{
		strcpy(fullfilename,""); //No filename = no path to file!
	}
	byte dynamicimage = is_dynamicimage(fullfilename); //Dynamic image detection!
	if (!dynamicimage) //Might be a static image when not a dynamic image?
	{
		if (!is_DSKimage(fullfilename)) //Not a DSK image?
		{
			if (!is_staticimage(fullfilename)) //Not a static image? We're invalid!
			{
				memset(&disks[device], 0, sizeof(disks[device])); //Delete the entry!
				return; //Abort!
			}
		}
	}

	if (disks[device].diskchangedhandler)
	{
		if (strcmp(disks[device].filename, fullfilename) != 0) //Different disk?
		{
			disks[device].diskchangedhandler(device); //This disk has been changed!
		}
	}

	strcpy(disks[device].filename, fullfilename); //Set file!
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

void CDROM_selecttrack(int device, uint_32 track)
{
	if ((device & 0xFF) == device) //Valid device?
	{
		disks[device].selectedtrack = track; //Select the track to use, if any is supported!
	}
}

//Startpos=sector number (start/512 bytes)!
byte readdata(int device, void *buffer, uint_64 startpos, uint_32 bytestoread)
{
	byte *resultbuffer = (byte *)buffer; //The result buffer!
	byte sectorbuffer[512]; //Full buffered sector!
	if ((device&0xFF)!=device) //Invalid device?
	{
		dolog("IO","io.c: Unknown device: %i!",device);
		return FALSE; //Unknown device!
	}

	char dev[256]; //Our device!
	bzero(dev,sizeof(dev)); //Init device string!

	FILEPOS readpos; //Read pos!
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

	if ((device == CDROM0) || (device == CDROM1)) //CD-ROM devices support tracks?
	{
		if (disks[device].selectedtrack != 0) //Non-track 0 isn't supported for disk images!
		{
			return FALSE; //Error: invalid track!
		}
	}

	uint_64 sector; //Current sector!
	sector = (readpos>>9); //The sector we need must be a multiple of 512 bytes (standard sector size)!
	uint_64 sectorpos = startpos; //Start of the data to read!
	sectorpos -= (sector << 9); //Start within the sector!
	FILEPOS bytesread = 0; //Init bytesread!
	
	SECTORHANDLER handler = disks[device].readhandler; //Our handler!
	if (!handler) return 0; //Error: no handler registered!

	word currentbytestoread; //How many bytes to read this time?

	for (; bytesread<bytestoread;) //Still left to read?
	{
		if (!handler(dev,(uint_32)sector,&sectorbuffer)) //Read to buffer!
		{
			if (disks[device].dynamicimage) //Dynamic?
			{
				dolog("IO","io.c: Couldn't read dynamic image %s sector %i",dev,sector);
			}
			else //Static?
			{
				dolog("IO","io.c: Couldn't read static image %s sector %i",dev,sector);
			}
			return FALSE; //Error!
		}
		else //Successfully read?
		{
			currentbytestoread = 512;
			if ((currentbytestoread + sectorpos)>512) //Less than the 512-byte buffer?
			{
				currentbytestoread -= (word)sectorpos; //Bytes left to read!
			}
			if (currentbytestoread > (bytestoread - bytesread)) //More than we need?
			{
				currentbytestoread = (word)(bytestoread - bytesread); //Only take what we need!
			}
			memcpy(&resultbuffer[sectorpos], &sectorbuffer, currentbytestoread); //Copy the bytes from the current sector to the destination (either partly or fully)!
			sectorpos = 0; //The next sector is at position 0!
		}
		bytesread += currentbytestoread; //1 (partly) sector read!
		++sector; //Next sector!
	}
	
	return TRUE; //OK!
}

byte has_drive(int drive) //Device inserted?
{
	if (drive<0 || drive>0xFF) return 0; //No disk available!
	byte buf[512];
	if (!readdata(drive,&buf,0,512)) //First sector invalid?
	{
		return FALSE; //No drive!
	}
	return TRUE; //Have drive!
}

byte writedata(int device, void *buffer, uint_64 startpos, uint_32 bytestowrite)
{
	byte *readbuffer = (byte *)buffer; //Data buffer!
	byte sectorbuffer[512]; //A full sector buffered for editing!
	char dev[256]; //Our device!
	bzero(dev,sizeof(dev)); //Init device string!
	uint_64 writepos; //Read pos!
	uint_64 basepos; //Base pos!
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

	uint_64 sector; //Current sector!
	sector = (writepos>>9); //The sector we need!
	FILEPOS byteswritten = 0; //Init byteswritten!

	SECTORHANDLER writehandler = disks[device].writehandler; //Our handler!
	if (!writehandler) return 0; //Error: no handler registered!

	uint_64 sectorpos = startpos; //Start of the data to read!
	sectorpos -= (sector << 9); //Start within the sector!

	SECTORHANDLER readhandler = disks[device].readhandler; //Our handler!
	if (!readhandler) return 0; //Error: no handler registered!

	word currentbytestowrite; //How many bytes to write this time?

	for (; byteswritten<bytestowrite;) //Still left to read?
	{
		if (!readhandler(dev, (uint_32)sector, &sectorbuffer)) //Read to buffer!
		{
			if (disks[device].dynamicimage) //Dynamic?
			{
				dolog("IO", "io.c: Couldn't read dynamic image %s sector %i to overwrite", dev, sector);
			}
			else //Static?
			{
				dolog("IO", "io.c: Couldn't read static image %s sector %i to overwrite", dev, sector);
			}
			return FALSE; //Error!
		}
		else //Successfully read?
		{
			currentbytestowrite = 512;
			if ((currentbytestowrite + sectorpos)>512) //Less than the 512-byte buffer?
			{
				currentbytestowrite -= (word)sectorpos; //Bytes left to read!
			}
			if (currentbytestowrite > (bytestowrite - byteswritten)) //More than we need?
			{
				currentbytestowrite = (word)(bytestowrite - byteswritten); //Only take what we need!
			}
			memcpy(&sectorbuffer[sectorpos], &readbuffer[byteswritten], currentbytestowrite); //Copy the bytes from the current sector to the destination!
			if (!writehandler(dev, (uint_32)sector, &sectorbuffer)) //Write new buffer!
			{
				if (disks[device].dynamicimage) //Dynamic?
				{
					dolog("IO", "io.c: Couldn't write dynamic image %s sector %i to overwrite", dev, sector);
				}
				else //Static?
				{
					dolog("IO", "io.c: Couldn't write static image %s sector %i to overwrite", dev, sector);
				}
				return FALSE; //Error!
			}
			sectorpos = 0; //The next sector is at position 0!
		}
		byteswritten += currentbytestowrite; //1 (partly) sector written!
		++sector; //Next sector!
	}

	return TRUE; //OK!
}