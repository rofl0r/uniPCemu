#include "headers/types.h" //Basic types etc.
#include "headers/basicio/io.h" //Basic I/O support for BIOS!
#include "headers/support/crc32.h" //CRC32 support!
#include "headers/mmu/mmu.h" //CRC32 support!
#include "headers/bios/bios.h" //BIOS basic type support etc!
#include "headers/bios/boot.h" //For booting disks!
#include "headers/cpu/cpu.h" //For some constants concerning CPU!
#include "headers/emu/gpu/gpu.h" //Need GPU comp!
#include "headers/support/zalloc.h" //Memory allocation: freemem function!
#include "headers/support/log.h" //Logging support!
#include "headers/emu/gpu/gpu_emu.h" //GPU emulator support!
#include "headers/hardware/8042.h" //Basic 8042 support for keyboard initialisation!
#include "headers/emu/emu_misc.h" //FILE_EXISTS support!
#include "headers/hardware/ports.h" //Port I/O support!
#include "headers/emu/sound.h" //Volume support!

//Are we disabled?
#define __HW_DISABLED 0

BIOS_Settings_TYPE BIOS_Settings; //Currently loaded settings!
byte showchecksumerrors = 0; //Show checksum errors?

//Block size of memory (blocks of 16KB for IBM PC Compatibility)!
#define MEMORY_BLOCKSIZE 0x4000
//What to leave for functions! 1MB for normal operations, plus 5 screens for VGA rendering resizing (2 screens for double sizing(never x&y together) and 1 screen for the final result)!
#define FREEMEMALLOC (MBMEMORY+(5*(PSP_SCREEN_COLUMNS*PSP_SCREEN_ROWS*sizeof(uint_32))))

//What file to use for saving the BIOS!
#define BIOS_FILE "BIOS.DAT"
//Default values for new BIOS settings:
#define DEFAULT_BOOT_ORDER 0
#define DEFAULT_CPU CPU_80186
#define DEFAULT_DEBUGMODE DEBUGMODE_NONE
#define DEFAULT_EXECUTIONMODE EXECUTIONMODE_BIOS
#define DEFAULT_DEBUGGERLOG DEBUGGERLOG_NONE
#define DEFAULT_ASPECTRATIO 2
#ifdef __psp__
#define DEFAULT_DIRECTPLOT 0
#else
#define DEFAULT_DIRECTPLOT 2
#endif
#define DEFAULT_BWMONITOR BWMONITOR_NONE
#define DEFAULT_SSOURCEVOL 1.0f
#define DEFAULT_FRAMERATE 0

void forceBIOSSave()
{
	BIOS_SaveData(); //Save the BIOS, ignoring the result!
}

void autoDetectMemorySize(int tosave) //Auto detect memory size (tosave=save BIOS?)
{
	if (__HW_DISABLED) return; //Ignore updates to memory!
	debugrow("Detecting MMU memory size to use...");
	
	uint_32 freememory = freemem(); //The free memory available!
	int_32 memoryblocks = SAFEDIV((freememory-FREEMEMALLOC),MEMORY_BLOCKSIZE); //Calculate # of free memory size and prepare for block size!
	if (memoryblocks<0) memoryblocks = 0; //No memory left?
	BIOS_Settings.memory = memoryblocks * MEMORY_BLOCKSIZE; //Whole blocks of memory only!
	if (!memoryblocks) //Not enough memory (at least 16KB required)?
	{
		raiseError("BIOS","Ran out of enough memory to use! Free memory: ",BIOS_Settings.memory); //Show error&quit: not enough memory to work with!
		sleep(); //Wait forever!
	}
	//dolog("BIOS","Detected memory: %i bytes",BIOS_Settings.memory);

	if (tosave)
	{
		forceBIOSSave(); //Force BIOS save!
	}
}



void BIOS_LoadDefaults(int tosave) //Load BIOS defaults, but not memory size!
{
	if (showchecksumerrors)
	{
		printmsg(0xF,"\r\nBIOS Checksum Error. "); //Checksum error.
	}
	
	uint_32 oldmem = BIOS_Settings.memory; //Memory installed!
	memset(&BIOS_Settings,0,sizeof(BIOS_Settings)); //Reset to empty!
	
	if (!file_exists(BIOS_FILE)) //New file?
	{
		BIOS_Settings.firstrun = 1; //We're the first run!
	}
	
	BIOS_Settings.memory = oldmem; //Keep this intact!
//Now load the defaults.

	bzero(BIOS_Settings.floppy0,sizeof(BIOS_Settings.floppy0));
	BIOS_Settings.floppy0_readonly = 0; //Not read-only!
	bzero(BIOS_Settings.floppy1,sizeof(BIOS_Settings.floppy1));
	BIOS_Settings.floppy1_readonly = 0; //Not read-only!
	bzero(BIOS_Settings.hdd0,sizeof(BIOS_Settings.hdd0));
	BIOS_Settings.hdd0_readonly = 0; //Not read-only!
	bzero(BIOS_Settings.hdd1,sizeof(BIOS_Settings.hdd1));
	BIOS_Settings.hdd1_readonly = 0; //Not read-only!

	bzero(BIOS_Settings.cdrom0,sizeof(BIOS_Settings.cdrom0));
	bzero(BIOS_Settings.cdrom1,sizeof(BIOS_Settings.cdrom1));
//CD-ROM always read-only!

	bzero(BIOS_Settings.SoundFont,sizeof(BIOS_Settings.SoundFont)); //Reset the currently mounted soundfont!

	BIOS_Settings.bootorder = DEFAULT_BOOT_ORDER; //Default boot order!
	BIOS_Settings.emulated_CPU = DEFAULT_CPU; //Which CPU to be emulated?

	BIOS_Settings.debugmode = DEFAULT_DEBUGMODE; //Default debug mode!
	BIOS_Settings.executionmode = DEFAULT_EXECUTIONMODE; //Default execution mode!
	BIOS_Settings.debugger_log = DEFAULT_DEBUGGERLOG; //Default debugger logging!

	keyboard_loadDefaults(); //Load the defaults for the keyboard font etc.!
	BIOS_Settings.VGA_AllowDirectPlot = DEFAULT_DIRECTPLOT; //Default: automatic 1:1 mapping!
	BIOS_Settings.aspectratio = DEFAULT_ASPECTRATIO; //Don't keep aspect ratio by default!
	BIOS_Settings.bwmonitor = DEFAULT_BWMONITOR; //Default B/W monitor setting!
	BIOS_Settings.SoundSource_Volume = DEFAULT_SSOURCEVOL; //Default soundsource volume knob!
	BIOS_Settings.ShowFramerate = DEFAULT_FRAMERATE; //Default framerate setting!
	
	
	BIOS_Settings.version = BIOS_VERSION; //Current version loaded!
	keyboard_loadDefaults(); //Load the defaults for the keyboard!
	
	if (tosave) //Save settings?
	{
		forceBIOSSave(); //Save the BIOS!
	}
	if (showchecksumerrors)
	{
		printmsg(0xF,"Defaults loaded.\r\n"); //Show that the defaults are loaded.
	}
}

int telleof(FILE *f) //Are we @eof?
{
	int curpos = 0; //Cur pos!
	int endpos = 0; //End pos!
	int result = 0; //Result!
	curpos = ftell(f); //Cur position!
	fseek(f,0,SEEK_END); //Goto EOF!
	endpos = ftell(f); //End position!

	fseek(f,curpos,SEEK_SET); //Return!
	result = (curpos==endpos); //@EOF?
	return result; //Give the result!
}

uint_32 BIOS_getChecksum() //Get the BIOS checksum!
{
	uint_32 result=0,total=sizeof(BIOS_Settings); //Initialise our info!
	byte *data = (byte *)&BIOS_Settings; //First byte of data!
	for (;total;) //Anything left?
	{
		result += (uint_32)*data; //Add the data to the result!
		--total; //One byte of data processed!
	}
	return result; //Give the simple checksum of the loaded settings!
}

void BIOS_LoadData() //Load BIOS settings!
{
	if (__HW_DISABLED) return; //Abort!
	FILE *f;
	size_t bytesread, bytestoread;
	uint_32 CheckSum = 0; //Read checksum!

	f = fopen(BIOS_FILE,"rb"); //Open BIOS file!

	if (!f) //Not loaded?
	{
		BIOS_LoadDefaults(1); //Load the defaults, save!
		return; //We've loaded the defaults!
	}

	bytesread = fread(&CheckSum,1,sizeof(CheckSum),f); //Read Checksum!
	if (bytesread!=sizeof(CheckSum) || feof(f)) //Not read?
	{
		fclose(f); //Close!
		dolog("BIOS","Error reading BIOS checksum.");
		BIOS_LoadDefaults(1); //Load the defaults, save!
		return; //We've loaded the defaults!
	}

	fseek(f, 0, SEEK_END); //Goto EOF!
	bytestoread = ftell(f); //How many bytes to read!
	bytestoread -= sizeof(CheckSum); //Without the checksum!
	if (bytestoread > sizeof(BIOS_Settings)) //Incompatible BIOS: we're newer than what we have?
	{
		dolog("BIOS","Error: BIOS is too large.");
		BIOS_LoadDefaults(1); //Load the defaults, save!
		return; //We've loaded the defaults because 
	}
	fseek(f, sizeof(CheckSum), SEEK_SET); //Goto start of settings!

	memset(&BIOS_Settings, 0, sizeof(BIOS_Settings)); //Clear all settings we have: we want to start older BIOSes empty!
	bytesread = fread(&BIOS_Settings,1,bytestoread,f); //Read settings!
	fclose(f); //Close!

//Verify the checksum!
	if (bytesread != bytestoread) //Error reading data?
	{
		dolog("BIOS","Error: BIOS data to read doesn't match bytes read.");
		BIOS_LoadDefaults(1); //Load the defaults, save!
		return; //We've loaded the defaults!
	}

	if (CheckSum!=BIOS_getChecksum()) //Checksum fault?
	{
		dolog("BIOS","Error: Invalid checksum.");
		BIOS_LoadDefaults(1); //Load the defaults, save!
		return; //We've loaded the defaults!
	}
//BIOS has been loaded.

	if (BIOS_Settings.version!=BIOS_VERSION) //Not compatible with our version?
	{
		dolog("BIOS","Error: Invalid BIOS version.");
		BIOS_LoadDefaults(1); //Load the defaults, save!
		return; //We've loaded the defaults because 
	}
}





int BIOS_SaveData() //Save BIOS settings!
{
	if (__HW_DISABLED) return 1; //Abort!
	uint_32 CheckSum = BIOS_getChecksum(); //CRC is over all but checksum!

	size_t byteswritten;
	FILE *f;
	f = fopen(BIOS_FILE,"wb"); //Open for saving!
	if (!f) //Not able to open?
	{
		return 0; //Failed to write!
	}
	
	byteswritten = fwrite(&CheckSum,1,sizeof(CheckSum),f); //Write checksum for error checking!
	if (byteswritten!=sizeof(CheckSum)) //Failed to save?
	{
		fclose(f); //Close!
		return 0; //Failed to write!
	}

	byteswritten = fwrite(&BIOS_Settings,1,sizeof(BIOS_Settings),f); //Write data!
	if (byteswritten!=sizeof(BIOS_Settings)) //Failed to save?
	{
		fclose(f); //Close!
		return 0; //Failed to write!
	}

	fclose(f); //Close!
	return 1; //BIOS Written & saved successfully!
}

uint_32 BIOS_GetMMUSize() //For MMU!
{
	if (__HW_DISABLED) return MBMEMORY; //Abort with default value (1MB memory)!
	return BIOS_Settings.memory; //Use all available memory always!
}

void BIOS_ValidateData() //Validates all data and unmounts/remounts if needed!
{
	if (__HW_DISABLED) return; //Abort!
	//Mount all devices!
	iofloppy0(BIOS_Settings.floppy0,0,BIOS_Settings.floppy0_readonly,0);
	iofloppy1(BIOS_Settings.floppy1,0,BIOS_Settings.floppy1_readonly,0);
	iohdd0(BIOS_Settings.hdd0,0,BIOS_Settings.hdd0_readonly,0);
	iohdd1(BIOS_Settings.hdd1,0,BIOS_Settings.hdd1_readonly,0);
	iocdrom0(BIOS_Settings.cdrom0,0,1,0);
	iocdrom1(BIOS_Settings.cdrom1,0,1,0);

	byte buffer[512]; //Little buffer for checking the files!
	int bioschanged = 0; //BIOS changed?
	bioschanged = 0; //Reset if the BIOS is changed!

	//dolog("IO","Checking FLOPPY A (%s)...",BIOS_Settings.floppy0);
	if ((!readdata(FLOPPY0,&buffer,0,sizeof(buffer))) && (strcmp(BIOS_Settings.floppy0,"")!=0)) //No disk mounted but listed?
	{
		if (!getDSKimage(FLOPPY0)) //NOT a DSK image?
		{
			bzero(BIOS_Settings.floppy0, sizeof(BIOS_Settings.floppy0)); //Unmount!
			BIOS_Settings.floppy0_readonly = 0; //Reset readonly flag!
			//dolog("BIOS","Floppy A invalidated!");
			bioschanged = 1; //BIOS changed!
		}
	}
	//dolog("IO","Checking FLOPPY B (%s)...",BIOS_Settings.floppy1);
	if ((!readdata(FLOPPY1,&buffer,0,sizeof(buffer))) && (strcmp(BIOS_Settings.floppy1,"")!=0)) //No disk mounted but listed?
	{
		if (!getDSKimage(FLOPPY1)) //NOT a DSK image?
		{
			bzero(BIOS_Settings.floppy1, sizeof(BIOS_Settings.floppy1)); //Unmount!
			BIOS_Settings.floppy1_readonly = 0; //Reset readonly flag!
			//dolog("BIOS","Floppy B invalidated!");
			bioschanged = 1; //BIOS changed!
		}
	}
	
	//dolog("IO","Checking First HDD (%s)...",BIOS_Settings.hdd0);
	if ((!readdata(HDD0,&buffer,0,sizeof(buffer))) && (strcmp(BIOS_Settings.hdd0,"")!=0)) //No disk mounted but listed?
	{
		bzero(BIOS_Settings.hdd0,sizeof(BIOS_Settings.hdd0)); //Unmount!
		BIOS_Settings.hdd0_readonly = 0; //Reset readonly flag!
		//dolog("BIOS","First HDD invalidated!");
		bioschanged = 1; //BIOS changed!
	}
	
	//dolog("IO","Checking Second HDD (%s)...",BIOS_Settings.hdd1);
	if ((!readdata(HDD1,&buffer,0,sizeof(buffer))) && (strcmp(BIOS_Settings.hdd1,"")!=0)) //No disk mounted but listed?
	{
		bzero(BIOS_Settings.hdd1,sizeof(BIOS_Settings.hdd1)); //Unmount!
		BIOS_Settings.hdd1_readonly = 0; //Reset readonly flag!
		//dolog("BIOS","Second HDD invalidated!");
		bioschanged = 1; //BIOS changed!
	}
	//dolog("IO","Checking First CD-ROM (%s)...",BIOS_Settings.cdrom0);
	if ((!readdata(CDROM0,&buffer,0,sizeof(buffer))) && (strcmp(BIOS_Settings.cdrom0,"")!=0)) //No disk mounted but listed?
	{
		bzero(BIOS_Settings.cdrom0,sizeof(BIOS_Settings.cdrom0)); //Unmount!
		bioschanged = 1; //BIOS changed!
		//dolog("BIOS","First CD-ROM invalidated!");
	}
	
	//dolog("IO","Checking Second CD-ROM (%s)...",BIOS_Settings.cdrom1);
	if ((!readdata(CDROM1,&buffer,0,sizeof(buffer))) && (strcmp(BIOS_Settings.cdrom1,"")!=0)) //No disk mounted but listed?
	{
		bzero(BIOS_Settings.cdrom1,sizeof(BIOS_Settings.cdrom1)); //Unmount!
		bioschanged = 1; //BIOS changed!
		//dolog("BIOS","Second CD-ROM invalidated!");
	}

	//Unmount/remount!
	iofloppy0(BIOS_Settings.floppy0,0,BIOS_Settings.floppy0_readonly,0);
	iofloppy1(BIOS_Settings.floppy1,0,BIOS_Settings.floppy1_readonly,0);
	iohdd0(BIOS_Settings.hdd0,0,BIOS_Settings.hdd0_readonly,0);
	iohdd1(BIOS_Settings.hdd1,0,BIOS_Settings.hdd1_readonly,0);
	iocdrom0(BIOS_Settings.cdrom0,0,1,0); //CDROM always read-only!
	iocdrom1(BIOS_Settings.cdrom1,0,1,0); //CDROM always read-only!

	if (BIOS_Settings.SoundFont[0]) //Gotten a soundfont set?
	{
		if (!FILE_EXISTS(BIOS_Settings.SoundFont)) //Not found?
		{
			memset(BIOS_Settings.SoundFont, 0, sizeof(BIOS_Settings.SoundFont)); //Invalid soundfont!
			bioschanged = 1; //BIOS changed!
		}
	}

	if (BIOS_Settings.DataBusSize > 1) //Invalid bus size?
	{
		BIOS_Settings.DataBusSize = 0; //Default bus size!
		bioschanged = 1; //BIOS changed!
	}

	if (bioschanged)
	{
		forceBIOSSave(); //Force saving!
	}
}

void BIOS_LoadIO(int showchecksumerrors) //Loads basic I/O drives from BIOS!
{
	if (__HW_DISABLED) return; //Abort!
	ioInit(); //Reset I/O system!
	showchecksumerrors = showchecksumerrors; //Allow checksum errors to be shown!
	BIOS_LoadData();//Load BIOS options!
	BIOS_ValidateData(); //Validate all data!
	GPU_AspectRatio(BIOS_Settings.aspectratio); //Keep the aspect ratio?
	showchecksumerrors = 0; //Don't Allow checksum errors to be shown!
}

void BIOS_ShowBIOS() //Shows mounted drives etc!
{
	if (__HW_DISABLED) return; //Abort!
	showchecksumerrors = 0; //No checksum errors to show!
	BIOS_LoadData();
	BIOS_ValidateData(); //Validate all data before continuing!

	printmsg(0xF,"Memory installed: ");
	printmsg(0xE,"%i blocks (%iKB / %iMB)\r\n",SAFEDIV(BIOS_GetMMUSize(),MEMORY_BLOCKSIZE),(SAFEDIV(BIOS_GetMMUSize(),1024)),(BIOS_GetMMUSize()/MBMEMORY));

	printmsg(0xF,"\r\n"); //A bit of space between memory and disks!
	int numdrives = 0;
	if (strcmp(BIOS_Settings.hdd0,"")!=0) //Have HDD0?
	{
		printmsg(0xF,"Primary master: %s",BIOS_Settings.hdd0);
		if (BIOS_Settings.hdd0_readonly) //Read-only?
		{
			printmsg(0x4," <R>");
		}
		printmsg(0xF,"\r\n"); //Newline!
		++numdrives;
	}
	if (strcmp(BIOS_Settings.hdd1,"")!=0) //Have HDD1?
	{
		printmsg(0xF,"Primary slave: %s",BIOS_Settings.hdd1);
		if (BIOS_Settings.hdd1_readonly) //Read-only?
		{
			printmsg(0x4," <R>");
		}
		printmsg(0xF,"\r\n"); //Newline!
		++numdrives;
	}
	if (strcmp(BIOS_Settings.cdrom0,"")!=0) //Have CDROM0?
	{
		printmsg(0xF,"Secondary master: %s\r\n",BIOS_Settings.cdrom0);
		++numdrives;
	}
	if (strcmp(BIOS_Settings.cdrom1,"")!=0) //Have CDROM1?
	{
		printmsg(0xF,"Secondary slave: %s\r\n",BIOS_Settings.cdrom1);
		++numdrives;
	}

	if (((strcmp(BIOS_Settings.floppy0,"")!=0) || (strcmp(BIOS_Settings.floppy1,"")!=0)) && numdrives>0) //Have drives and adding floppy?
	{
		printmsg(0xF,"\r\n"); //Insert empty row between floppy and normal disks!
	}

	if (strcmp(BIOS_Settings.floppy0,"")!=0) //Have FLOPPY0?
	{
		printmsg(0xF,"Floppy disk detected: %s",BIOS_Settings.floppy0);
		if (BIOS_Settings.floppy0_readonly) //Read-only?
		{
			printmsg(0x4," <R>");
		}
		printmsg(0xF,"\r\n"); //Newline!
		++numdrives;
	}

	if (strcmp(BIOS_Settings.floppy1,"")!=0) //Have FLOPPY1?
	{
		printmsg(0xF,"Floppy disk detected: %s",BIOS_Settings.floppy1);
		if (BIOS_Settings.floppy1_readonly) //Read-only?
		{
			printmsg(0x4," <R>");
		}
		printmsg(0xF,"\r\n"); //Newline!
		++numdrives;
	}

	if ((BIOS_Settings.emulated_CPU!=CPU_8086) && (BIOS_Settings.emulated_CPU!=CPU_80186) && (BIOS_Settings.emulated_CPU!=CPU_80286)) //Invalid CPU detected?
	{
		BIOS_Settings.emulated_CPU = DEFAULT_CPU; //Load default CPU!
		forceBIOSSave(); //Force the BIOS to be saved!
	}

	if (BIOS_Settings.emulated_CPU==CPU_8086) //8086?
	{
		if (BIOS_Settings.DataBusSize) //8-bit bus?
		{
			printmsg(0xF, "Installed CPU: Intel 8088\r\n"); //Emulated CPU!
		}
		else //16-bit bus?
		{
			printmsg(0xF,"Installed CPU: Intel 8086\r\n"); //Emulated CPU!
		}
	}
	else if (BIOS_Settings.emulated_CPU==CPU_80186) //80186?
	{
		if (BIOS_Settings.DataBusSize) //8-bit bus?
		{
			printmsg(0xF, "Installed CPU: Intel 80188\r\n"); //Emulated CPU!
		}
		else //16-bit bus?
		{
			printmsg(0xF, "Installed CPU: Intel 80186\r\n"); //Emulated CPU!
		}
	}
	else if (BIOS_Settings.emulated_CPU == CPU_80286) //80286?
	{
		printmsg(0xF, "Installed CPU: Intel 80286(unfinished)\r\n"); //Emulated CPU!
	}
	else //Unknown CPU?
	{
		printmsg(0x4,"Installed CPU: Unknown\r\n"); //Emulated CPU!
	}

	if (numdrives==0) //No drives?
	{
		printmsg(0x4,"Warning: no drives have been detected!\r\nPlease enter BIOS and specify some disks.\r\n");
	}
}

//Defines for booting!
#define BOOT_FLOPPY 0
#define BOOT_HDD 1
#define BOOT_CDROM 2
#define BOOT_NONE 3

//Boot order for boot sequence!
byte BOOT_ORDER[15][3] =
{
//First full categories (3 active)
	{BOOT_FLOPPY, BOOT_CDROM, BOOT_HDD}, //Floppy, Cdrom, Hdd?
	{BOOT_FLOPPY, BOOT_HDD, BOOT_CDROM}, //Floppy, Hdd, Cdrom?
	{BOOT_CDROM, BOOT_FLOPPY, BOOT_HDD}, //Cdrom, Floppy, Hdd?
	{BOOT_CDROM, BOOT_HDD, BOOT_FLOPPY}, //Cdrom, Hdd, Floppy?
	{BOOT_HDD, BOOT_FLOPPY, BOOT_CDROM}, //Hdd, Floppy, Cdrom?
	{BOOT_HDD, BOOT_CDROM, BOOT_FLOPPY}, //Hdd, Cdrom, Floppy?
//Now advanced categories (2 active)!
	{BOOT_FLOPPY, BOOT_CDROM, BOOT_NONE}, //Floppy, Cdrom?
	{BOOT_FLOPPY, BOOT_HDD, BOOT_NONE}, //Floppy, Hdd?
	{BOOT_CDROM, BOOT_FLOPPY, BOOT_NONE}, //Cdrom, Floppy?
	{BOOT_CDROM, BOOT_HDD, BOOT_NONE}, //Cdrom, Hdd?
	{BOOT_HDD, BOOT_FLOPPY, BOOT_NONE}, //Hdd, Floppy?
	{BOOT_HDD, BOOT_CDROM, BOOT_NONE}, //Hdd, Cdrom?
//Finally single categories (1 active)
	{BOOT_FLOPPY, BOOT_NONE, BOOT_NONE}, //Floppy only?
	{BOOT_CDROM, BOOT_NONE, BOOT_NONE}, //CDROM only?
	{BOOT_HDD, BOOT_NONE, BOOT_NONE} //HDD only?
};

//Boot order (string representation)
char BOOT_ORDER_STRING[15][30] =
{
//Full categories (3 active)
	"FLOPPY, CDROM, HDD",
	"FLOPPY, HDD, CDROM",
	"CDROM, FLOPPY, HDD",
	"CDROM, HDD, FLOPPY",
	"HDD, FLOPPY, CDROM",
	"HDD, CDROM, FLOPPY",
//Advanced categories (2 active)
	"FLOPPY, CDROM",
	"FLOPPY, HDD",
	"CDROM, FLOPPY",
	"CDROM, HDD",
	"HDD, FLOPPY",
	"HDD, CDROM",
//Finally single categories (1 active)
	"FLOPPY ONLY",
	"CDROM ONLY",
	"HDD ONLY",
};

//Try to boot a category (BOOT_FLOPPY, BOOT_HDD, BOOT_CDROM)

int try_boot(byte category)
{
	if (__HW_DISABLED) return 0; //Abort!
	switch (category)
	{
	case BOOT_FLOPPY: //Boot FLOPPY?
		if (CPU_boot(FLOPPY0)) //Try floppy0!
		{
			return 1; //OK: booted!
		}
		else
		{
			return CPU_boot(FLOPPY1); //Try floppy1!
		}
	case BOOT_HDD: //Boot HDD?
		if (CPU_boot(HDD0)) //Try hdd0!
		{
			return 1; //OK: booted!
		}
		else
		{
			return CPU_boot(HDD1); //Try hdd1!
		}
	case BOOT_CDROM: //Boot CDROM?
		if (CPU_boot(CDROM0)) //Try cdrom0!
		{
			return 1; //OK: booted!
		}
		else
		{
			return CPU_boot(CDROM1); //Try cdrom1!
		}
	case BOOT_NONE: //No device?
		break; //Don't boot!
	default: //Default?
		break; //Don't boot!
	}
	return 0; //Not booted!
}

/*

boot_system: boots using BIOS boot order!
returns: TRUE on booted, FALSE on no bootable disk found.

*/

int boot_system()
{
	if (__HW_DISABLED) return 0; //Abort!
	int c;
	for (c=0; c<3; c++) //Try 3 boot devices!
	{
		if (try_boot(BOOT_ORDER[BIOS_Settings.bootorder][c])) //Try boot using currently specified boot order!
		{
			return 1; //Booted!
		}
	}
	return 0; //Not booted at all!
}

/*

Basic BIOS Keyboard support!

*/

void BIOS_writeKBDCMD(byte cmd)
{
	if (__HW_DISABLED) return; //Abort!
	write_8042(0x60,cmd); //Write the command directly to the controller!
}

extern byte force8042; //Force 8042 style handling?

void BIOSKeyboardInit() //BIOS part of keyboard initialisation!
{
	if (__HW_DISABLED) return; //Abort!
	byte result; //For holding the result from the hardware!
	force8042 = 1; //We're forcing 8042 style init!

	BIOS_writeKBDCMD(0xED); //Set/reset status indicators!
	if (!(PORT_IN_B(0x64)&0x2)) //No input data?
	{
		raiseError("Keyboard BIOS initialisation","No set/reset status indicator command result:2!");
	}

	result = PORT_IN_B(0x60); //Check the result!
	if (result!=0xFA) //NAC?
	{
		raiseError("Keyboard BIOS initialisation","Set/reset status indication command result: %02X",result);
	}

	write_8042(0x60,0x02); //Turn on NUM LOCK led!
	if (!(PORT_IN_B(0x64)&0x2)) //No input data?
	{
		raiseError("Keyboard BIOS initialisation","No turn on NUM lock led result!");
	}
	result = PORT_IN_B(0x60); //Must be 0xFA!
	if (result!=0xFA) //Error?
	{
		raiseError("Keyboard BIOS initialisation","Couldn't turn on Num Lock LED! Result: %02X",result);
	}

	PORT_OUT_B(0x64, 0xAE); //Enable first PS/2 port!

	BIOS_writeKBDCMD(0xF4); //Enable scanning!

	PORT_OUT_B(0x64, 0x20); //Read PS2ControllerConfigurationByte!
	byte PS2ControllerConfigurationByte;
	PS2ControllerConfigurationByte = PORT_IN_B(0x60); //Read result!

	PS2ControllerConfigurationByte |= 1; //Enable our interrupt!
	PORT_OUT_B(0x64, 0x60); //Write PS2ControllerConfigurationByte!
	PORT_OUT_B(0x60, PS2ControllerConfigurationByte); //Write the new configuration byte!
	force8042 = 0; //Disable 8042 style init!
}