#include "headers/types.h" //Types!
#include "headers/basicio/io.h" //Need I/O support!
#include "headers/bios/bios.h" //Need BIOS support!
#include "headers/mmu/mmu.h" //MMU support!
#include "headers/support/isoreader.h" //Own type support!
#include "headers/emu/gpu/gpu.h" //GPU support!
#include "headers/cpu/cpu.h" //For boot segments!
#include "headers/support/zalloc.h" //Zero allocation support!
#include "headers/support/log.h" //Logging support!
#include "headers/emu/gpu/gpu_emu.h" //GPU emulator support!

//Stuff we need!
//Bootable indicator
#define CD_IND_BOOTABLE 0x88
//Sector size
#define CD_SEC_SIZE 0x800
#define CD_VIRT_SEC_SIZE 0x200
#define CD_BOOT_RECORD_VOL_SEC 0x11

#define CD_BOOT_DEFAULTENTRY 0x20

//Time for an iso to check for booting!
#define ISO_BOOT_TIME 5000000
#define INPUT_INTERVAL 10000

//Need TRUE/FALSE comp.
#define TRUE 1
#define FALSE 0

word ISOREADER_SEGMENT = 0x0000; //Segment read to load image!












OPTINLINE int WriteData(char *filename, void *buffer, uint_32 len) //Write buffer to file!
{
	FILE *f;
	f = fopen(filename,"wb"); //Open file for writing!
	if (f) //Opened?
	{
		int dummy = 0;
		dummy = fwrite(buffer,1,len,f); //Write data!
		fclose(f); //Close file!
		if (dummy!=len) //Error writing?
		{
			return FALSE; //Error!
		}
		return TRUE; //Data written!
	}
	else
	{
		return FALSE; //Error!
	}
}

#define DWORD uint_32
#define WORD word
#define BYTE byte

typedef union
{
	uint_32 thedword;
	byte b[4]; //Data!
} dwordconverter;




















/*
getBootImage: retrieves boot image from ISO disk (from device)
result: drive type (0x01=floppy (drive #0x00);0x80=hdd;0xFF=No emulation)
*/

int getBootImage(int device, char *imagefile) //Returns TRUE on bootable (image assigned to floppy/harddisk), else FALSE!
{
	int result = 0;
	byte buffer[CD_SEC_SIZE]; //1MB buffer!
	int drivetype = 0; //Drive type!
	int success = 0;
	dwordconverter firstbootcatptr;
	DWORD BootCatalogSector;
	dwordconverter dwbuf;
	DWORD StartingSector;
	WORD VirtSectorCount;
	WORD ReadSecCount;
	DWORD dwLen;
	int do_boot, counter;
	byte sectorbuffer[CD_SEC_SIZE]; //A buffer to contain a sector!
	FILE *f;
	uint_32 byteswritten = 0; //Ammount of data bytes written!
	uint_32 data_size;

//First, read boot record in buffer!
	success = readdata(device,&buffer,CD_SEC_SIZE * CD_BOOT_RECORD_VOL_SEC, CD_SEC_SIZE); //Read boot-record-volume

	if (!success) //Failed?
	{
		return FALSE; //Failed to read boot-record-volume!
	}

	if ((buffer[0]!=0) || (buffer[6]!=1))
	{
		return FALSE; //Invalid boot-record volume!
	}

	buffer[6] = 0;
	buffer[30] = 0;
	if ((strcmp((char*)&buffer[1],"CD001")!=0) ||
	        (strcmp((char*)&buffer[7],"EL TORITO SPECIFICATION")!=0))
	{
		return FALSE; //Invalid signature in boot-record-volume!
	}

//ok, it is valid... now get the sector of the boot-catalog


	firstbootcatptr.b[0] = buffer[0x47];
	firstbootcatptr.b[1] = buffer[0x48];
	firstbootcatptr.b[2] = buffer[0x49];
	firstbootcatptr.b[3] = buffer[0x4A];

	BootCatalogSector = firstbootcatptr.thedword; //Boot catalog sector!

//Read the boot-catalog (should be at 36864 in test image):
	success = readdata(device,&buffer,CD_SEC_SIZE * BootCatalogSector,CD_SEC_SIZE); //Read the boot-catalog!
	if (!success)
	{
		return FALSE; //Error: couldn't read the boot-catalog!
	}

//Check the validation-entry:
	if (buffer[0]!=0x01 || (buffer[0x1E]!=0x55) || (buffer[0x1F!=0xAA]))
	{
		return FALSE; //Invalid validation-entry in boot-catalog!
	}

	switch (buffer[1])
	{
	case 0: //PlatformID: 80x86
		break; //We can use this!
	case 1: //PlatformID: PowerPC
	case 2: //PlatformID: Mac
	default:
		//printf("GBI: Unknown platform! Platform: %i! Disk %i!\n",(int)buffer[1],device);
		return FALSE; //PlatformID: Unknown
	}

//Now read the boot record etc.

//Ignore the checksum!

//Now read the initial/default entry!

	//result = WriteData("DefaultEntry.DAT",buffer,sizeof(buffer)); //Write DefaultEntry dump!

	if (buffer[CD_BOOT_DEFAULTENTRY]!=CD_IND_BOOTABLE) //Not bootable?
	{
		return FALSE;
	}

	if (buffer[CD_BOOT_DEFAULTENTRY+2]!=0 && buffer[CD_BOOT_DEFAULTENTRY+3]!=0) //Not standard boot segment?
	{
//Non-standard segment?
		ISOREADER_SEGMENT = (buffer[CD_BOOT_DEFAULTENTRY+2]<<8)|buffer[CD_BOOT_DEFAULTENTRY+3]; //Segment to load!
//printf("GBI: Custom loaded segment: %04x\n",ISOREADER_SEGMENT); //Custom segment!
	}
	else
	{
		ISOREADER_SEGMENT = BOOT_SEGMENT; //Use traditional segment!
	}

	dwbuf.b[0] = buffer[0x08+CD_BOOT_DEFAULTENTRY];
	dwbuf.b[1] = buffer[0x09+CD_BOOT_DEFAULTENTRY];
	dwbuf.b[2] = buffer[0x0A+CD_BOOT_DEFAULTENTRY];
	dwbuf.b[3] = buffer[0x0B+CD_BOOT_DEFAULTENTRY];

	StartingSector = dwbuf.thedword; //Starting sector

	VirtSectorCount = *((WORD *)&buffer[0x06+CD_BOOT_DEFAULTENTRY]); //Sector count

	ReadSecCount = 0; //Copy for now (default)!

	dwLen = 0;

	ReadSecCount = VirtSectorCount; //Use virtual ammount of sectors!

	switch ((buffer[1+CD_BOOT_DEFAULTENTRY]&0x0F)) //Emulation mode
	{
	case 0:
		drivetype = 0xFF; //No emulation
		//Don't change sector count!
		break;
	case 1:
		drivetype = 1; //1.2M diskette
		ReadSecCount = 2400; //1.2M diskette!
		break;
	case 2:
		drivetype = 1; //1.44M diskette
		ReadSecCount = 2880; //1.44M diskette!
		break;
	case 3:
		drivetype = 1; //2.88M diskette
		ReadSecCount = 5760; //2.88M diskette!
		break;
	case 4:
		drivetype = 0x80; //HDD (drive 80)
		//Don't change sector count!
		break;
	default:
		//printf("GBI: Unknown drive type: %02x! Disk %i!\n",(buffer[1+CD_BOOT_DEFAULTENTRY]&0x0F),device);
		return FALSE; //Unknown!
	}

	dwLen = ReadSecCount * CD_VIRT_SEC_SIZE; //Length of the data!

	switch (device)
	{
	case CDROM0: //First CDROM!
		GPU_EMU_printscreen(-1,-1,"Press any key to boot CDROM #1...\n");
		break;
	case CDROM1: //Second CDROM!
		GPU_EMU_printscreen(-1,-1,"Press any key to boot CDROM #2...\n");
		break;
	default: //Not a CD-ROM drive!
		return FALSE; //Don't boot from NON-CDROM!
	}

	do_boot = 0; //Do boot?
	counter = ISO_BOOT_TIME; //5 Seconds to wait!
	counter = ISO_BOOT_TIME; //Init!
	while (counter>=0)
	{
		counter -= INPUT_INTERVAL;
		delay(INPUT_INTERVAL); //Intervals of one!
		if (psp_inputkey()!=0) //Key pressed?
		{
			do_boot = 1; //We're to boot!
			break; //Start booting!
		}
	}

	if (!do_boot) //Not to boot?
	{
		return FALSE; //Not to boot!
	}

	/*byte *pBootImg; //Is going to contain the Boot image.
	pBootImg = zalloc(dwLen,"ISO_Image"); //Allocate memory for Boot image!
	*/

	/*if (!pBootImg)
	{
		dolog("cdrom_error","Ran out of memory allocating Boot Image buffer.");
		return FALSE; //Not to boot!
	}*/

//read the boot image
//Process the "boot sector"=boot image

	f = fopen(imagefile,"wb"); //Open the image file!
	if (!f)
	{
		dolog("ISOReader","Failed to boot from CD-ROM: could not open temporary image file %s.",imagefile); //Log our error!
		return FALSE;
	}

	while (byteswritten<dwLen) //Not fully writte yet?
	{
		if ((dwLen-byteswritten)>CD_SEC_SIZE) //More than the buffer?
		{
			data_size = CD_SEC_SIZE; //No more than our buffer!
		}
		else
		{
			data_size = dwLen-byteswritten; //What's left of the image!
		}
		if (!readdata(device,&sectorbuffer,(CD_SEC_SIZE * StartingSector)+byteswritten, data_size)) //Reading failed?
		{
			fclose(f); //Close the image file!
			delete_file(".",imagefile); //Remove the image file!
			dolog("ISOReader","Failed to boot from CD-ROM: could not read image file."); //Log our error!
			return FALSE; //No boot sector found!
		}
		if (fwrite(&sectorbuffer,1,data_size,f)!=data_size) //Write error?
		{
			fclose(f); //Close the image file!
			delete_file(".",imagefile); //Remove the image file!
			dolog("ISOReader","Failed to boot from CD-ROM: could not write to temporary image file %s. Disk full?",imagefile); //Log our error!
			return FALSE; //No boot sector found!
		}
	}
	fclose(f); //Close the image file: we've written it!

//Clean up before leaving!

	if (result) //Got a result?
	{
		return drivetype; //Use the drive's type as a result!
	}

//Default: image not gotten!
	return FALSE; //Default: not gotten!
}

int getBootImageInfo(int device, BOOTIMGINFO *imagefile) //Returns TRUE on bootable (image info set to imagefile), else FALSE!
{
	byte buffer[CD_SEC_SIZE]; //1MB buffer!
	int drivetype = 0; //Drive type!
	int success = 0;
	dwordconverter firstbootcatptr;
	DWORD BootCatalogSector;
	dwordconverter dwbuf;
	DWORD StartingSector;
	WORD VirtSectorCount;
	WORD ReadSecCount;
	DWORD dwLen;
	int do_boot = 0; //Do boot?
	int counter = ISO_BOOT_TIME; //5 Seconds to wait!

//First, read boot record in buffer!
	success = readdata(device,&buffer,CD_SEC_SIZE * CD_BOOT_RECORD_VOL_SEC, CD_SEC_SIZE); //Read boot-record-volume

	if (!success) //Failed?
	{
		return FALSE; //Failed to read boot-record-volume!
	}

	if ((buffer[0]!=0) || (buffer[6]!=1))
	{
		return FALSE; //Invalid boot-record volume!
	}

	buffer[6] = 0;
	buffer[30] = 0;
	if ((strcmp((char*)&buffer[1],"CD001")!=0) ||
	        (strcmp((char*)&buffer[7],"EL TORITO SPECIFICATION")!=0))
	{
		return FALSE; //Invalid signature in boot-record-volume!
	}

//ok, it is valid... now get the sector of the boot-catalog

	firstbootcatptr.b[0] = buffer[0x47];
	firstbootcatptr.b[1] = buffer[0x48];
	firstbootcatptr.b[2] = buffer[0x49];
	firstbootcatptr.b[3] = buffer[0x4A];

	BootCatalogSector = firstbootcatptr.thedword; //Boot catalog sector!

//Read the boot-catalog (should be at 36864 in test image):
	success = readdata(device,&buffer,CD_SEC_SIZE * BootCatalogSector,CD_SEC_SIZE); //Read the boot-catalog!
	if (!success)
	{
		return FALSE; //Error: couldn't read the boot-catalog!
	}

//Check the validation-entry:
	if (buffer[0]!=0x01 || (buffer[0x1E]!=0x55) || (buffer[0x1F!=0xAA]))
	{
		return FALSE; //Invalid validation-entry in boot-catalog!
	}

	switch (buffer[1])
	{
	case 0: //PlatformID: 80x86
		break; //We can use this!
	case 1: //PlatformID: PowerPC
	case 2: //PlatformID: Mac
	default:
		//printf("GBI: Unknown platform! Platform: %i! Disk %i!\n",(int)buffer[1],device);
		return FALSE; //PlatformID: Unknown
	}

//Now read the boot record etc.

//Ignore the checksum!

//Now read the initial/default entry!

	WriteData("DefaultEntry.DAT",buffer,sizeof(buffer)); //Write DefaultEntry dump!

	if (buffer[CD_BOOT_DEFAULTENTRY]!=CD_IND_BOOTABLE) //Not bootable?
	{
		return FALSE;
	}

	if (buffer[CD_BOOT_DEFAULTENTRY+2]!=0 && buffer[CD_BOOT_DEFAULTENTRY+3]!=0) //Not standard boot segment?
	{
//Non-standard segment?
		ISOREADER_SEGMENT = (buffer[CD_BOOT_DEFAULTENTRY+2]<<8)|buffer[CD_BOOT_DEFAULTENTRY+3]; //Segment to load!
//printf("GBI: Custom loaded segment: %04x\n",ISOREADER_SEGMENT); //Custom segment!
	}
	else
	{
		ISOREADER_SEGMENT = BOOT_SEGMENT; //Use traditional segment!
	}

	dwbuf.b[0] = buffer[0x08+CD_BOOT_DEFAULTENTRY];
	dwbuf.b[1] = buffer[0x09+CD_BOOT_DEFAULTENTRY];
	dwbuf.b[2] = buffer[0x0A+CD_BOOT_DEFAULTENTRY];
	dwbuf.b[3] = buffer[0x0B+CD_BOOT_DEFAULTENTRY];

	StartingSector = dwbuf.thedword; //Starting sector

	VirtSectorCount = *((WORD *)&buffer[0x06+CD_BOOT_DEFAULTENTRY]); //Sector count

	ReadSecCount = 0; //Copy for now (default)!

	dwLen = 0;

	ReadSecCount = VirtSectorCount; //Use virtual ammount of sectors!

	switch ((buffer[1+CD_BOOT_DEFAULTENTRY]&0x0F)) //Emulation mode
	{
	case 0:
		drivetype = 0xFF; //No emulation
		//Don't change sector count!
		break;
	case 1:
		drivetype = 1; //1.2M diskette
		ReadSecCount = 2400; //1.2M diskette!
		break;
	case 2:
		drivetype = 1; //1.44M diskette
		ReadSecCount = 2880; //1.44M diskette!
		break;
	case 3:
		drivetype = 1; //2.88M diskette
		ReadSecCount = 5760; //2.88M diskette!
		break;
	case 4:
		drivetype = 0x80; //HDD (drive 80)
		//Don't change sector count!
		break;
	default:
		//printf("GBI: Unknown drive type: %02x! Disk %i!\n",(buffer[1+CD_BOOT_DEFAULTENTRY]&0x0F),device);
		return FALSE; //Unknown!
	}

	dwLen = ReadSecCount * CD_VIRT_SEC_SIZE; //Length of the data!

	if (device==CDROM0)
	{
		GPU_EMU_printscreen(-1,-1,"Press any key to boot from the first CDROM...\n");
	}
	else
	{
		GPU_EMU_printscreen(-1,-1,"Press any key to boot from the second CDROM...\n");
	}

	counter = ISO_BOOT_TIME; //Init!
	while (counter>=0)
	{
		counter -= INPUT_INTERVAL;
		delay(INPUT_INTERVAL); //Intervals of one!
		if (psp_inputkey()!=0) //Key pressed?
		{
			do_boot = 1; //We're to boot!
			break; //Start booting!
		}
	}

	if (!do_boot) //Not to boot?
	{
		return FALSE; //Not to boot!
	}

	imagefile->device = device; //The device to read from!
	imagefile->startpos = CD_SEC_SIZE * StartingSector; //Start pos!
	imagefile->imagesize = dwLen; //Length of the image!
	imagefile->used = 1; //Init used (default to used!)

	if (do_boot) //Got a result?
	{
		return drivetype; //Use the drive's type as a result!
	}

//Default: image not gotten!
	return FALSE; //Default: not gotten!
}