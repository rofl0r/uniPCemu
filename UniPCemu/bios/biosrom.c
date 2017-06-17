#include "headers/mmu/mmuhandler.h" //Basic MMU handler support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/support/zalloc.h" //Allocation support!
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/cpu/paging.h" //Pagina support for address decoding (for Turbo XT BIOS detection)!
#include "headers/support/locks.h" //Locking support!

//Comment this define to disable logging
//#define __ENABLE_LOGGING

#ifdef __ENABLE_LOGGING
#include "headers/support/log.h" //Logging support!
#else
//Ignore logging!
#define dolog(...)
#endif

//Patch ET3000 ROM to:
// Use Sequencer Memory Mode register value 0E instead of 06 with mode 2Eh.
//Other patches: TODO

#define ET3000_PATCH

byte EMU_BIOS[0x10000]; //Full custom BIOS from 0xF0000-0xFFFFF for the emulator itself to use!
byte EMU_VGAROM[0x10000]; //Maximum size custom BIOS VGA ROM!

byte *BIOS_ROMS[0x100]; //All possible BIOS roms!
byte BIOS_ROMS_ext[0x100]; //Extended load used to load us?
uint_32 BIOS_ROM_size[0x100]; //All possible BIOS ROM sizes!

byte numOPT_ROMS = 0;
byte *OPT_ROMS[40]; //Up to 40 option roms!
uint_32 OPTROM_size[40]; //All possible OPT ROM sizes!
uint_64 OPTROM_location[40]; //All possible OPT ROM locations(low word) and end position(high word)!
char OPTROM_filename[40][256]; //All possible filenames for the OPTROMs loaded!

byte OPTROM_writeSequence[40]; //Current write sequence command state!
byte OPTROM_writeSequence_waitingforDisable[40]; //Waiting for disable command?
byte OPTROM_writeenabled[40]; //Write enabled ROM?
double OPTROM_writetimeout[40]; //Timeout until SDP is activated!
byte OPTROM_timeoutused = 0;

extern BIOS_Settings_TYPE BIOS_Settings;

int BIOS_load_VGAROM(); //Prototype: Load custom ROM from emulator itself!

byte ISVGA = 0; //VGA that's loaded!

char ROMpath[256] = "ROM";
char originalROMpath[256] = "ROM"; //Original ROM path!

extern byte is_XT; //Are we emulating an XT architecture?

uint_32 BIOSROM_BASE_Modern = 0xFFFF0000; //AT+ BIOS ROM base!
uint_32 BIOSROM_BASE_AT = 0xFF0000; //AT BIOS ROM base!
uint_32 BIOSROM_BASE_XT = 0xF0000; //XT BIOS ROM base!

extern byte is_Compaq; //Are we emulating a Compaq device?

void scanROM(char *device, char *filename)
{
	//Special case: 32-bit uses Compaq ROMs!
	sprintf(filename, "%s/%s.%s.BIN", ROMpath,device,(is_Compaq?"32":(is_XT?"XT":"AT"))); //Create the filename for the ROM for the architecture!
	if (!file_exists(filename)) //This version doesn't exist? Then try the other version!
	{
		sprintf(filename, "%s/%s.%s.BIN", ROMpath,device, is_XT ? "XT" : "AT"); //Create the filename for the ROM for the architecture!
		if (!file_exists(filename)) //This version doesn't exist? Then try the other version!
		{
			sprintf(filename,"%s/%s.BIN",ROMpath,device); //CGA ROM!
		}
	}
}

byte BIOS_checkOPTROMS() //Check and load Option ROMs!
{
	strcpy(originalROMpath,ROMpath); //Save the original ROM path for deallocation!
	numOPT_ROMS = 0; //Initialise the number of OPTROMS!
	memset(&OPTROM_writeenabled, 0, sizeof(OPTROM_writeenabled)); //Disable all write enable flags by default!
	memset(&OPTROM_writeSequence, 0, sizeof(OPTROM_writeSequence)); //Disable all write enable flags by default!
	memset(&OPTROM_writeSequence_waitingforDisable, 0, sizeof(OPTROM_writeSequence_waitingforDisable)); //Disable all write enable flags by default!
	memset(&OPTROM_writetimeout,0,sizeof(OPTROM_writetimeout); //Disable all timers for all ROMs!
	OPTROM_timeoutused = 0; //Not timing?
	byte i; //Current OPT ROM!
	uint_32 location; //The location within the OPT ROM area!
	location = 0; //Init location!
	ISVGA = 0; //Are we a VGA ROM?
	for (i=0;(i<NUMITEMS(OPT_ROMS)) && (location<0x20000);i++) //Process all ROMS we can process!
	{
		FILE *f;
		char filename[100];
		memset(&filename,0,sizeof(filename)); //Clear/init!
		if (i) //Not Graphics Adapter ROM?
		{
			//Default!
			sprintf(filename, "%s/OPTROM.%s.%u.BIN", ROMpath,(is_Compaq?"32":(is_XT?"XT":"AT")), i); //Create the filename for the ROM for the architecture!
			if (!file_exists(filename)) //This version doesn't exist? Then try the other version!
			{
				sprintf(filename, "%s/OPTROM.%s.%u.BIN", ROMpath,is_XT?"XT":"AT", i); //Create the filename for the ROM for the architecture!
				if (!file_exists(filename)) //This version doesn't exist? Then try the other version!
				{
					sprintf(filename, "%s/OPTROM.%u.BIN", ROMpath, i); //Create the filename for the ROM!
				}
			}
		}
		else
		{
			ISVGA = 0; //No VGA!
			if (BIOS_Settings.VGA_Mode==4) //Pure CGA?
			{
				scanROM("CGAROM",&filename[0]); //Scan for a CGA ROM!
			}
			else if (BIOS_Settings.VGA_Mode==5) //Pure MDA?
			{
				scanROM("MDAROM",&filename[0]); //Scan for a MDA ROM!
			}
			else
			{	
				ISVGA = 1; //We're a VGA!
				if (BIOS_Settings.VGA_Mode==6) //ET4000?
				{
					scanROM("ET4000",&filename[0]); //Scan for a ET4000 ROM!
					//Provide emulator fallback support!
					if (file_exists(filename)) //Full ET4000?
					{
						ISVGA = 2; //ET4000!
						//ET4000 ROM!
					}
					else //VGA ROM?
					{
						strcpy(filename, ""); //VGA ROM!
					}
				}
				else if (BIOS_Settings.VGA_Mode == 7) //ET3000?
				{
					scanROM("ET3000",&filename[0]); //Scan for a ET3000 ROM!
					//Provide emulator fallback support!
					if (file_exists(filename)) //Full ET3000?
					{
						ISVGA = 3; //ET3000!
						//ET3000 ROM!
					}
					else //VGA ROM?
					{
						strcpy(filename, ""); //VGA ROM!
					}
				}
				else if (BIOS_Settings.VGA_Mode == 8) //EGA?
				{
					scanROM("EGAROM",&filename[0]); //Scan for a EGA ROM!
					//Provide emulator fallback support!
					if (file_exists(filename)) //Full EGA?
					{
						ISVGA = 4; //EGA!
						//EGA ROM!
					}
					else //VGA ROM?
					{
						strcpy(filename, ""); //VGA ROM!
					}
				}
				else //Plain VGA?
				{
					scanROM("VGAROM",&filename[0]); //Scan for a VGA ROM!
				}
			}
		}
		if (strcmp(filename,"")==0) //No ROM?
		{
			f = NULL; //No ROM!
		}
		else //Try to open!
		{
			f = fopen(filename,"rb");
		}
		if (!f)
		{
			if (!i) //First ROM is reserved by the VGA BIOS ROM. If not found, we're skipping it and using the internal VGA BIOS!
			{
				if (ISVGA) //Are we the VGA ROM?
				{
					location = sizeof(EMU_VGAROM); //Allocate the Emulator VGA ROM for the first entry instead!
					BIOS_load_VGAROM(); //Load the BIOS VGA ROM!
				}
			}
			continue; //Failed to load!
		}
		fseek(f,0,SEEK_END); //Goto EOF!
		if (ftell(f)) //Gotten size?
		{
			OPTROM_size[i] = ftell(f); //Save the size!
			fseek(f,0,SEEK_SET); //Goto BOF!
			if ((location+OPTROM_size[i])>0x20000) //Overflow?
			{
				if (!i) //First ROM is reserved by the VGA BIOS ROM. If not found, we're skipping it and using the internal VGA BIOS!
				{
					location = sizeof(EMU_VGAROM); //Allocate the Emulator VGA ROM for the first entry instead!
					BIOS_load_VGAROM(); //Load the BIOS VGA ROM!
				}
				BIOS_ROM_size[i] = 0; //Reset!
				continue; //We're skipping this ROM: it's too big!
			}
			OPT_ROMS[i] = (byte *)nzalloc(OPTROM_size[i],filename,getLock(LOCK_CPU)); //Simple memory allocation for our ROM!
			if (!OPT_ROMS[i]) //Failed to allocate?
			{
				fclose(f); //Close the file!
				if (!i) //First ROM is reserved by the VGA BIOS ROM. If not found, we're skipping it and using the internal VGA BIOS!
				{
					location = sizeof(EMU_VGAROM); //Allocate the Emulator VGA ROM for the first entry instead!
					BIOS_load_VGAROM(); //Load the BIOS VGA ROM!
				}
				continue; //Failed to allocate!
			}
			if (fread(OPT_ROMS[i],1,OPTROM_size[i],f)!=OPTROM_size[i]) //Not fully read?
			{
				freez((void **)&OPT_ROMS[i],OPTROM_size[i],filename); //Failed to read!
				fclose(f); //Close the file!
				if (!i) //First ROM is reserved by the VGA BIOS ROM. If not found, we're skipping it and using the internal VGA BIOS!
				{
					location = sizeof(EMU_VGAROM); //Allocate the Emulator VGA ROM for the first entry instead!
					BIOS_load_VGAROM(); //Load the BIOS VGA ROM!
				}
				continue; //Failed to read!
			}
			fclose(f); //Close the file!
			
			OPTROM_location[i] = location; //The option ROM location we're loaded at!
			cleardata(&OPTROM_filename[i][0],sizeof(OPTROM_filename[i])); //Init filename!
			strcpy(OPTROM_filename[i],filename); //Save the filename of the loaded ROM for writing to it, as well as releasing it!

			location += OPTROM_size[i]; //Next ROM position!
			OPTROM_location[i] |= ((uint_64)location<<32); //The end location of the option ROM!
			if (OPTROM_size[i]&0x7FF) //Not 2KB alligned?
			{
				location += 0x800-(OPTROM_size[i]&0x7FF); //2KB align!
			}
			numOPT_ROMS = i+1; //We've loaded this many ROMS!
			continue; //Loaded!
		}
		
		fclose(f);
	}
	return 1; //Just run always!
}

void BIOS_freeOPTROMS()
{
	byte i;
	for (i=0;i<NUMITEMS(OPT_ROMS);i++)
	{
		if (OPT_ROMS[i]) //Loaded?
		{
			char filename[100];
			memset(&filename,0,sizeof(filename)); //Clear/init!
			strcpy(filename,OPTROM_filename[i]); //Set the filename from the loaded ROM!
			freez((void **)&OPT_ROMS[i],OPTROM_size[i],filename); //Release the OPT ROM!
		}
	}
}

int BIOS_load_ROM(byte nr)
{
	byte tryext = 0; //Try extra ROMs?
	uint_32 ROM_size=0; //The size of both ROMs!
	FILE *f;
	char filename[100];
	memset(&filename,0,sizeof(filename)); //Clear/init!
	retryext:
	if ((tryext==0) && (EMULATED_CPU>=CPU_80386)) //Extension ROM available?
	{
		if (BIOS_Settings.BIOSROMmode==BIOSROMMODE_DIAGNOSTICS) //Diagnostics mode?
		{
			sprintf(filename,"%s/BIOSROM.32.U%u.DIAGNOSTICS.BIN",originalROMpath,nr); //Create the filename for the ROM!		
		}
		else //Normal mode?
		{
			sprintf(filename,"%s/BIOSROM.32.U%u.BIN",originalROMpath,nr); //Create the filename for the ROM!		
		}
		tryext = 1; //We're trying an extension!
	}
	else //Normal ROM try?
	{
		if (BIOS_Settings.BIOSROMmode==BIOSROMMODE_DIAGNOSTICS) //Diagnostics mode?
		{
			sprintf(filename,"%s/BIOSROM.U%u.DIAGNOSTICS.BIN",originalROMpath,nr); //Create the filename for the ROM!
		}
		else
		{
			sprintf(filename,"%s/BIOSROM.U%u.BIN",originalROMpath,nr); //Create the filename for the ROM!
		}
	}
	f = fopen(filename,"rb");
	if (!f)
	{
		if (tryext==1) //Extension try and tried?
		{
			tryext = 2; //Try second time!
			goto retryext;
		}
		return 0; //Failed to load!
	}
	fseek(f,0,SEEK_END); //Goto EOF!
	if (ftell(f)) //Gotten size?
 	{
		BIOS_ROM_size[nr] = ftell(f); //Save the size!
		fseek(f,0,SEEK_SET); //Goto BOF!
		BIOS_ROMS[nr] = (byte *)nzalloc(BIOS_ROM_size[nr],filename, getLock(LOCK_CPU)); //Simple memory allocation for our ROM!
		if (!BIOS_ROMS[nr]) //Failed to allocate?
		{
			fclose(f); //Close the file!
			return 0; //Failed to allocate!
		}
		if (fread(BIOS_ROMS[nr],1,BIOS_ROM_size[nr],f)!=BIOS_ROM_size[nr]) //Not fully read?
		{
			freez((void **)&BIOS_ROMS[nr],BIOS_ROM_size[nr],filename); //Failed to read!
			fclose(f); //Close the file!
			return 0; //Failed to read!
		}
		fclose(f); //Close the file!

		BIOS_ROMS_ext[nr] = ((BIOS_Settings.BIOSROMmode==BIOSROMMODE_DIAGNOSTICS)?2:0)|((tryext==1)?1:0); //Extension enabled?

		switch (nr) //What ROM has been loaded?
		{
			case 18:
			case 19: //u18/u19 chips?
				ROM_size = BIOS_ROM_size[18]+BIOS_ROM_size[19]; //ROM size!
				break;
			case 34:
			case 35: //u34/u35 chips?
				ROM_size = BIOS_ROM_size[34]+BIOS_ROM_size[35]; //ROM size!
				break;
			case 27:
			case 47: //u27/u47 chips?
				ROM_size = BIOS_ROM_size[27]+BIOS_ROM_size[47]; //ROM size!
				break;
			case 13:
			case 15: //u13/u15 chips?
				ROM_size = (BIOS_ROM_size[13]+BIOS_ROM_size[15])<<1; //ROM size! The ROM is doubled in RAM(duplicated twice)
				break;
		}
		
		//Recalculate based on ROM size!
		BIOSROM_BASE_AT = 0xFFFFFF-(MIN(ROM_size,0x100000)-1); //AT ROM size! Limit to 1MB!
		BIOSROM_BASE_XT = 0xFFFFF-(MIN(ROM_size,0x10000)-1); //XT ROM size! Limit to 64KB!
		BIOSROM_BASE_Modern = 0xFFFFFFFF-(ROM_size-1); //Modern ROM size!
		return 1; //Loaded!
	}
	
	fclose(f);
	return 0; //Failed to load!
}

//Custom loaded BIOS ROM (single only)!
byte *BIOS_custom_ROM;
uint_32 BIOS_custom_ROM_size;
char customROMname[256]; //Custom ROM name!

int BIOS_load_custom(char *path, char *rom)
{
	byte ROM_doubling; //Double the ROM?
	FILE *f;
	char filename[100];
	memset(&filename,0,sizeof(filename)); //Clear/init!
	if (!path)
	{
		strcpy(filename,ROMpath); //Where to find our ROM!
	}
	else
	{
		strcpy(filename, path); //Where to find our ROM!
	}
	if (strcmp(filename, "") != 0) strcat(filename, "/"); //Only a seperator when not empty!
	strcat(filename,rom); //Create the filename for the ROM!
	f = fopen(filename,"rb");
	if (!f)
	{
		return 0; //Failed to load!
	}
	fseek(f,0,SEEK_END); //Goto EOF!
	if (ftell(f)) //Gotten size?
 	{
		BIOS_custom_ROM_size = ftell(f); //Save the size!
		fseek(f,0,SEEK_SET); //Goto BOF!
		BIOS_custom_ROM = (byte *)nzalloc(BIOS_custom_ROM_size,filename, getLock(LOCK_CPU)); //Simple memory allocation for our ROM!
		if (!BIOS_custom_ROM) //Failed to allocate?
		{
			fclose(f); //Close the file!
			return 0; //Failed to allocate!
		}
		if (fread(BIOS_custom_ROM,1,BIOS_custom_ROM_size,f)!=BIOS_custom_ROM_size) //Not fully read?
		{
			freez((void **)&BIOS_custom_ROM,BIOS_custom_ROM_size,filename); //Failed to read!
			fclose(f); //Close the file!
			return 0; //Failed to read!
		}
		fclose(f); //Close the file!
		strcpy(customROMname,filename); //Custom ROM name for easy dealloc!
		//Update the base address to use for this CPU!
		ROM_doubling = 0; //Default: no ROM doubling!
		if (BIOS_custom_ROM_size!=0x10000) //Safe to double?
		{
			if (EMULATED_CPU>=CPU_80386 && (is_XT==0)) //We're to emulate a Compaq Deskpro 386?
			{
				ROM_doubling = 1; //Double the ROM!
			}
		}
		BIOSROM_BASE_AT = 0xFFFFFF-((BIOS_custom_ROM_size<<ROM_doubling)-1); //AT ROM size!
		BIOSROM_BASE_XT = 0xFFFFF-((BIOS_custom_ROM_size<<ROM_doubling)-1); //XT ROM size!
		BIOSROM_BASE_Modern = 0xFFFFFFFF-((BIOS_custom_ROM_size<<ROM_doubling)-1); //Modern ROM size!
		return 1; //Loaded!
	}
	
	fclose(f);
	return 0; //Failed to load!
}


void BIOS_free_ROM(byte nr)
{
	char filename[100];
	memset(&filename,0,sizeof(filename)); //Clear/init!
	if (BIOS_ROMS_ext[nr]&1) //Extension ROM?
	{
		if (BIOS_ROMS_ext[nr]&2) //Diagnostic ROM?
		{
			sprintf(filename,"BIOSROM.32.U%u.DIAGNOSTICS.BIN",nr); //Create the filename for the ROM!
		}
		else //Normal ROM?
		{
			sprintf(filename,"BIOSROM.32.U%u.BIN",nr); //Create the filename for the ROM!
		}
	}
	else
	{
		if (BIOS_ROMS_ext[nr]&2) //Diagnostic ROM?
		{
			sprintf(filename,"BIOSROM.U%u.DIAGNOSTICS.BIN",nr); //Create the filename for the ROM!
		}
		else //Normal ROM?
		{
			sprintf(filename,"BIOSROM.U%u.BIN",nr); //Create the filename for the ROM!
		}
	}
	if (BIOS_ROM_size[nr]) //Has size?
	{
		freez((void **)&BIOS_ROMS[nr],BIOS_ROM_size[nr],filename); //Release the BIOS ROM!
	}
}

void BIOS_free_custom(char *rom)
{
	char filename[100];
	memset(&filename,0,sizeof(filename)); //Clear/init!
	if (rom==NULL) //NULL ROM (Autodetect)?
	{
		rom = &customROMname[0]; //Use custom ROM name!
	}
	strcpy(filename,rom); //Create the filename for the ROM!
	if (BIOS_custom_ROM_size) //Has size?
	{
		freez((void **)&BIOS_custom_ROM,BIOS_custom_ROM_size,filename); //Release the BIOS ROM!
	}
	BIOS_custom_ROM = NULL; //No custom ROM anymore!
}

int BIOS_load_systemROM() //Load custom ROM from emulator itself!
{
	BIOS_free_custom(NULL); //Free the custom ROM, if needed and known!
	BIOS_custom_ROM_size = sizeof(EMU_BIOS); //Save the size!
	BIOS_custom_ROM = &EMU_BIOS[0]; //Simple memory allocation for our ROM!
	BIOSROM_BASE_AT = 0xFFFFFF-(BIOS_custom_ROM_size-1); //AT ROM size!
	BIOSROM_BASE_XT = 0xFFFFF-(BIOS_custom_ROM_size-1); //XT ROM size!
	BIOSROM_BASE_Modern = 0xFFFFFFFF-(BIOS_custom_ROM_size-1); //Modern ROM size!
	return 1; //Loaded!
}

void BIOS_free_systemROM()
{
	BIOS_free_custom(NULL); //Free the custom ROM, if needed and known!
}

void BIOS_DUMPSYSTEMROM() //Dump the SYSTEM ROM currently set (debugging purposes)!
{
	char path[256];
	if (BIOS_custom_ROM == &EMU_BIOS[0]) //We're our own BIOS?
	{
		memset(&path,0,sizeof(path));
		strcpy(path,ROMpath); //Current ROM path!
		strcat(path,"/SYSROM.DMP.BIN"); //Dump path!
		//Dump our own BIOS ROM!
		FILE *f;
		f = fopen(path, "wb");
		fwrite(&EMU_BIOS, 1, sizeof(EMU_BIOS), f); //Save our BIOS!
		fclose(f);
	}
}


//VGA support!

byte *BIOS_custom_VGAROM;
uint_32 BIOS_custom_VGAROM_size;
char customVGAROMname[256] = "EMU_VGAROM"; //Custom ROM name!
byte VGAROM_mapping = 0xFF; //Default: all mapped in!

void BIOS_free_VGAROM()
{
	if (BIOS_custom_VGAROM_size) //Has size?
	{
		freez((void **)&BIOS_custom_VGAROM, BIOS_custom_VGAROM_size, &customVGAROMname[0]); //Release the BIOS ROM!
	}
}

int BIOS_load_VGAROM() //Load custom ROM from emulator itself!
{
	BIOS_free_VGAROM(); //Free the custom ROM, if needed and known!
	BIOS_custom_VGAROM_size = sizeof(EMU_VGAROM); //Save the size!
	BIOS_custom_VGAROM = (byte *)&EMU_VGAROM; //Simple memory allocation for our ROM!
	return 1; //Loaded!
}

byte BIOSROM_DisableLowMemory = 0; //Disable low-memory mapping of the BIOS and OPTROMs! Disable mapping of low memory locations E0000-FFFFF used on the Compaq Deskpro 386.

byte OPTROM_readhandler(uint_32 offset, byte *value)    /* A pointer to a handler function */
{
	uint_32 ROMsize;
	INLINEREGISTER uint_64 basepos, currentpos, temppos; //Current position!
	basepos = currentpos = offset; //Load the offset!
	if ((basepos >= 0xC0000) && (basepos<0xF0000)) basepos = 0xC0000; //Our base reference position!
	else //Out of range (16-bit)?
	{
		if ((basepos >= 0xC0000000) && (basepos < 0xF0000000)) basepos = 0xC0000000; //Our base reference position!
		else return 0; //Our of range (32-bit)?
	}
	currentpos -= basepos; //Calculate from the base position!
	if ((offset>=0xE0000) && (offset<=0xFFFFF) && (BIOSROM_DisableLowMemory)) return 0; //Disabled for Compaq RAM!
	basepos = currentpos; //Save a backup!
	INLINEREGISTER byte i=0,j=numOPT_ROMS;
	if (!numOPT_ROMS) goto noOPTROMSR;
	do //Check OPT ROMS!
	{
		currentpos = OPTROM_location[i]; //Load the current location for analysis and usage!
		ROMsize = (currentpos>>32); //Save ROM end location!
		if (OPT_ROMS[i] && (ROMsize>basepos)) //Before the end location and valid rom?
		{
			currentpos &= 0xFFFFFFFF; //The location of the ROM itself!
			ROMsize -= (uint_32)currentpos; //Convert ROMsize to the actual ROM size to use!
			if (currentpos <= basepos) //At/after the start location? We've found the ROM!
			{
				temppos = basepos-currentpos; //Calculate the offset within the ROM!
				if ((VGAROM_mapping!=0xFF) && (i==0)) //Special mapping for the VGA-reserved ROM?
				{
					switch (VGAROM_mapping) //What special mapping?
					{
						case 0: //C000-C3FF enabled
							if (temppos>=0x4000) return 0; //Unmapped!
							break;
						case 1: //ROM disabled (ET3K/4K-AX), C000-C5FFF(ET3K/4K-AF)
							return 0; //Disable the ROM!
						case 2: //C000-C5FF, C680-C7FF Enabled
							if ((temppos>=0x6000) && (temppos<0x6800)) return 0; //Unmapped in the mid-range!
							//Passthrough to the end mapping!
						case 3: //C000-C7FF Enabled
							if (temppos>=0x8000) return 0; //Disabled!
							break;
						default: //Don't handle specially?
							break;
					}
				}
				if ((ISVGA==4) && (i==0)) //EGA ROM is reversed?
				{
					*value = OPT_ROMS[i][ROMsize-temppos-1]; //Read the data from the ROM, reversed!
					return 1; //Done: we've been read!				
				}
				*value = OPT_ROMS[i][temppos]; //Read the data from the ROM!
				return 1; //Done: we've been read!
			}
		}
		++i;
	} while (--j);
	noOPTROMSR:
	if (BIOS_custom_VGAROM_size) //Custom VGA ROM mounted?
	{
		if (basepos < BIOS_custom_VGAROM_size) //OK?
		{
			*value = BIOS_custom_VGAROM[basepos]; //Give the value!
			return 1;
		}
	}
	return 0; //No ROM here, allow read from nroaml memory!
}

void BIOSROM_updateTimers(double timepassed)
{
	byte i, timersleft;
	if (OPTROM_timeoutused)
	{
		timersleft = 0; //Default: finished!
		for (i=0;i<NUMOPT_ROMS;++i)
		{
			if (OPTROM_writetimeout[i]) //Timing?
			{
				OPTROM_writetimeout[i] -= timepassed; //Time passed!
				if (OPTROM_writetimeout[i]<=0.0) //Expired?
				{
					OPTROM_writetimeout = (double)0; //Finish state!
					OPTROM_writeenabled[i] = 0; //Disable writes!
				}
				else timersleft = 1; //Still running?
			}
		}
		if (timersleft==0) OPTROM_timeoutused = 0; //Finished all timers!
	}
}

byte OPTROM_writehandler(uint_32 offset, byte value)    /* A pointer to a handler function */
{
	INLINEREGISTER uint_32 basepos, currentpos;
	basepos = currentpos = offset; //Load the offset!
	if ((basepos>=0xC0000) && (basepos<0xF0000)) basepos = 0xC0000; //Our base reference position!
	else //Out of range (16-bit)?
	{
		if ((basepos>=0xC0000000) && (basepos<0xF0000000)) basepos = 0xC0000000; //Our base reference position!
		else return 0; //Our of range (32-bit)?
	}
	currentpos -= basepos; //Calculate from the base position!
	if ((offset>=0xE0000) && (offset<=0xFFFFF) && (BIOSROM_DisableLowMemory)) return 0; //Disabled for Compaq RAM!
	basepos = currentpos; //Write back!
	INLINEREGISTER uint_64 OPTROM_address, OPTROM_loc; //The address calculated in the EEPROM!
	INLINEREGISTER byte i=0,j=numOPT_ROMS;
	if (!numOPT_ROMS) goto noOPTROMSW;
	do //Check OPT ROMS!
	{
		if (OPT_ROMS[i]) //Enabled?
		{
			OPTROM_loc = OPTROM_location[i]; //Load the current location!
			if ((OPTROM_loc>>32)>basepos) //Before the end of the ROM?
			{
				OPTROM_loc &= 0xFFFFFFFF;
				if (OPTROM_loc <= basepos) //After the start of the ROM?
				{
					OPTROM_address = basepos;
					OPTROM_address -= OPTROM_loc; //The location within the OPTROM!
					if ((VGAROM_mapping!=0xFF) && (i==0)) //Special mapping?
					{
						switch (VGAROM_mapping) //What special mapping?
						{
							case 0: //C000-C3FF enabled
								if (OPTROM_address>0x3FF0) return 0; //Unmapped!
								break;
							case 1: //ROM disabled (ET3K/4K-AX), C000-C5FFF(ET3K/4K-AF)
								return 0; //Disable the ROM!
							case 2: //C000-C5FF, C680-C7FF Enabled
								if ((OPTROM_address>=0xC600) && (OPTROM_address<0xC680)) return 0; //Unmapped in the mid-range!
								//Passthrough to the end mapping!
							case 3: //C000-C7FF Enabled
								if (OPTROM_address>0x8000) return 0; //Disabled!
								break;
							default: //Don't handle specially?
								break;
						}
					}
					switch (OPTROM_address)
					{
					case 0x1555:
						if ((value == 0xAA) && !OPTROM_writeSequence[i]) //Start sequence!
						{
							OPTROM_writeSequence[i] = 1; //Next step!
						}
						else if (OPTROM_writeSequence[i] == 2) //We're a command byte!
						{
							switch (value)
							{
							case 0xA0: //Enable write protect!
								OPTROM_writeSequence_waitingforDisable[i] = 0; //Not waiting anymore!
								OPTROM_writeSequence[i] = 0; //Finished write sequence!
								OPTROM_writetimeout[i] = 10000.0; //We're disabling writes to the EEPROM 10us after this write, the same applies to the following writes!
								OPTROM_timeoutused = 1; //Timing!
								break;
							case 0x80: //Wait for 0x20 to disable write protect!
								OPTROM_writeSequence_waitingforDisable[i] = 1; //Waiting for disable!
								OPTROM_writeSequence[i] = 0; //Finished write sequence!
								break;
							case 0x20: //Disable write protect!
								if (OPTROM_writeSequence_waitingforDisable[i]) //Waiting for disable?
								{
									OPTROM_writeenabled[i] = 1; //We're enabling writes to the EEPROM!
								}
								else
								{
									OPTROM_writeSequence_waitingforDisable[i] = 0; //Not waiting anymore!
									OPTROM_writeSequence[i] = 0; //Finished write sequence!
								}
								break;
							default: //Not a command!
								OPTROM_writeSequence_waitingforDisable[i] = 0; //Not waiting anymore!
								OPTROM_writeSequence[i] = 0; //Finished write sequence!
								break;
							}
						}
						else
						{
							OPTROM_writeSequence_waitingforDisable[i] = 0; //Not waiting anymore!
							OPTROM_writeSequence[i] = 0; //Finished write sequence!
						}
						break;
					case 0x0AAA:
						if ((value == 0x55) && (OPTROM_writeSequence[i] == 1)) //Start of valid sequence which is command-specific?
						{
							OPTROM_writeSequence[i] = 2; //Start write command sequence!
						}
						else
						{
							OPTROM_writeSequence_waitingforDisable[i] = 0; //Not waiting anymore!
							OPTROM_writeSequence[i] = 0; //Finished write sequence!
						}
						break;
					default:
						OPTROM_writeSequence_waitingforDisable[i] = 0; //Not waiting anymore!
						OPTROM_writeSequence[i] = 0; //No sequence running!
						break;
					}
					if (!OPTROM_writeenabled[i]) return 1; //Handled: ignore writes to ROM or protected ROM!
					if (OPTROM_writetimeout[i]) //Timing?
					{
						OPTROM_writetimeout[i] = 10000.0; //Reset timer!
						OPTROM_timeoutused = 1; //Timing!
					}
					if ((ISVGA==4) && (i==0)) //EGA ROM is reversed?
					{
						OPTROM_address = ((OPTROM_location[i]>>32)-OPTROM_address)-1; //The ROM is reversed, so reverse write too!
					}
					//We're a EEPROM with write protect disabled!
					FILE *f; //For opening the ROM file!
					f = fopen(OPTROM_filename[i], "rb+"); //Open the ROM for writing!
					if (!f) return 1; //Couldn't open the ROM for writing!
					if (fseek(f, (uint_32)OPTROM_address, SEEK_SET)) //Couldn't seek?
					{
						fclose(f); //Close the file!
						return 1; //Abort!
					}
					if (ftell(f) != OPTROM_address) //Failed seek position?
					{
						fclose(f); //Close the file!
						return 1; //Abort!
					}
					if (fwrite(&value, 1, 1, f) != 1) //Failed to write the data to the file?
					{
						fclose(f); //Close thefile!
						return 1; //Abort!
					}
					fclose(f); //Close the file!
					OPT_ROMS[i][OPTROM_address] = value; //Write the data to the ROM in memory!
					return 1; //Ignore writes to memory: we've handled it!
				}
			}
		}
		++i;
	} while (--j);
	noOPTROMSW:
	if (BIOS_custom_VGAROM_size) //Custom VGA ROM mounted?
	{
		if (basepos < BIOS_custom_VGAROM_size) //OK?
		{
			return 1; //Ignore writes!
		}
	}
	return 0; //No ROM here, allow writes to normal memory!
}

byte BIOS_writehandler(uint_32 offset, byte value)    /* A pointer to a handler function */
{
	INLINEREGISTER uint_32 basepos, tempoffset;
	basepos = tempoffset = offset; //Load the current location!
	if (basepos >= BIOSROM_BASE_XT) //Inside 16-bit/32-bit range?
	{
		if (basepos<0x100000) basepos = BIOSROM_BASE_XT; //Our base reference position(low memory)!
		else if ((basepos >= BIOSROM_BASE_Modern) && (EMULATED_CPU >= CPU_80386)) basepos = BIOSROM_BASE_Modern; //Our base reference position(high memory 386+)!
		else if ((basepos >= BIOSROM_BASE_AT) && (EMULATED_CPU == CPU_80286) && (basepos<0x1000000)) basepos = BIOSROM_BASE_AT; //Our base reference position(high memmory 286)
		else return 0; //Our of range (32-bit)?
	}
	else return 0; //Our of range (32-bit)?

	tempoffset -= basepos; //Calculate from the base position!
	if ((offset>=0xE0000) && (offset<=0xFFFFF) && (BIOSROM_DisableLowMemory)) return 0; //Disabled for Compaq RAM!
	basepos = tempoffset; //Save for easy reference!

	if (BIOS_custom_ROM) //Custom/system ROM loaded?
	{
		if (BIOS_custom_ROM_size == 0x10000)
		{
			if (tempoffset<0x10000) //Within range?
			{
				tempoffset &= 0xFFFF; //16-bit ROM!
				return 1; //Ignore writes!
			}
		}
		if ((EMULATED_CPU>=CPU_80386) && (is_XT==0)) //Compaq compatible?
		{
			if (tempoffset>=BIOS_custom_ROM_size) //Doubled copy?
			{
				tempoffset -= BIOS_custom_ROM_size; //Double in memory!
			}
		}
		if (tempoffset<BIOS_custom_ROM_size) //Within range?
		{
			return 1; //Ignore writes!
		}
		else //Custom ROM, but nothing to give? Special mapping!
		{
			return 1; //Abort!
		}
		tempoffset = basepos; //Restore the temporary offset!
	}

	INLINEREGISTER uint_32 originaloffset;
	INLINEREGISTER uint_32 segment; //Current segment!
	switch (EMULATED_CPU) //What CPU is being emulated?
	{
		case CPU_8086:
		case CPU_NECV30: //5160 PC!
			originaloffset = basepos; //Save the original offset for reference!
			if (basepos>=0x10000) return 0; //Not us!
			basepos &= 0x7FFF; //Our offset within the ROM!
			if (originaloffset&0x8000) //u18?
			{
				if (BIOS_ROMS[18]) //Set?
				{
					if (BIOS_ROM_size[18]>basepos) //Within range?
					{
						return 1; //Ignore writes!
					}
				}
			}
			else //u19?
			{
				if (BIOS_ROMS[19]) //Set?
				{
					if (BIOS_ROM_size[19]>basepos) //Within range?
					{
						return 1; //Ignore writes!
					}
				}
			}
			break;
		case CPU_80286:
		case CPU_80386:
		case CPU_80486:
		case CPU_PENTIUM: //5170 AT PC!
			segment = basepos; //Load the offset!
			if (BIOS_ROMS[15] && BIOS_ROMS[13]) //Compaq?
			{
				if (tempoffset<((BIOS_ROM_size[15]+BIOS_ROM_size[13])<<1)) //This is doubled in ROM!
				{
					if (tempoffset>=(BIOS_ROM_size[15]+BIOS_ROM_size[13])) //Second copy?
					{
						tempoffset -= (BIOS_ROM_size[15]+BIOS_ROM_size[13]); //Patch to first block to address!
					}
				}
			}
			tempoffset >>= 1; //The offset is at every 2 bytes of memory!
			segment &= 1; //Even=u27, Odd=u47
			if (segment) //u47/u35/u15?
			{
				if (BIOS_ROMS[15]) //u13/u15 combination?
				{
					if (BIOS_ROM_size[15]>tempoffset) //Within range?
					{
						return 1; //Ignore writes!
					}					
				}
				else if (BIOS_ROMS[35]) //u34/u35 combination?
				{
					if (BIOS_ROM_size[35]>tempoffset) //Within range?
					{
						return 1; //Ignore writes!
					}
				}
				else //Normal AT BIOS ROM?
				{
					if (BIOS_ROMS[47]) //Loaded?
					{
						if (BIOS_ROM_size[47]>tempoffset) //Within range?
						{
							return 1; //Ignore writes!
						}
					}
				}
			}
			else //u27/u34/u13?
			{
				if (BIOS_ROMS[13]) //u13/u15 combination?
				{
					if (BIOS_ROM_size[13]>tempoffset) //Within range?
					{
						return 1; //Ignore writes!
					}
				}
				else if (BIOS_ROMS[34]) //u34/u35 combination?
				{
					if (BIOS_ROM_size[34]>tempoffset) //Within range?
					{
						return 1; //Ignore writes!
					}
				}
				else //Normal AT BIOS ROM?
				{
					if (BIOS_ROMS[27]) //Loaded?
					{
						if (BIOS_ROM_size[27]>tempoffset) //Within range?
						{
							return 1; //Ignore writes!
						}
					}
				}
			}
			break;
		default: //Unknown CPU?
			break;
	}

	return 0; //Not recognised, use normal RAM!
}

byte BIOS_readhandler(uint_32 offset, byte *value) /* A pointer to a handler function */
{
	INLINEREGISTER uint_32 basepos, tempoffset;
	basepos = tempoffset = offset;
	if (basepos>=0xF0000) //Inside 16-bit/32-bit range?
	{
		if (basepos<0x100000) basepos = BIOSROM_BASE_XT; //Our base reference position(low memory)!
		else if ((basepos >= BIOSROM_BASE_Modern) && (EMULATED_CPU >= CPU_80386)) basepos = BIOSROM_BASE_Modern; //Our base reference position(high memory 386+)!
		else if ((basepos >= BIOSROM_BASE_AT) && (EMULATED_CPU == CPU_80286) && (basepos<0x1000000)) basepos = BIOSROM_BASE_AT; //Our base reference position(high memmory 286)
		else return 0; //Our of range (32-bit)?
	}
	else return 0; //Our of range (32-bit)?
	
	tempoffset -= basepos; //Calculate from the base position!
	basepos = tempoffset; //Save for easy reference!
	if (BIOS_custom_ROM) //Custom/system ROM loaded?
	{
		if (BIOS_custom_ROM_size == 0x10000)
		{
			if (tempoffset<0x10000) //Within range?
			{
				tempoffset &= 0xFFFF; //16-bit ROM!
				*value = BIOS_custom_ROM[tempoffset]; //Give the value!
				return 1; //Direct offset used!
			}
		}
		if ((EMULATED_CPU>=CPU_80386) && (is_XT==0)) //Compaq compatible?
		{
			if (tempoffset>=BIOS_custom_ROM_size) //Doubled copy?
			{
				tempoffset -= BIOS_custom_ROM_size; //Double in memory!
			}
		}
		if (tempoffset<BIOS_custom_ROM_size) //Within range?
		{
			*value = BIOS_custom_ROM[tempoffset]; //Give the value!
			return 1; //ROM offset from the end of RAM used!
		}
		else //Custom ROM, but nothing to give? Give 0x00!
		{
			*value = 0x00; //Dummy value for the ROM!
			return 1; //Abort!
		}
		tempoffset = basepos; //Restore the temporary offset!
	}

	INLINEREGISTER uint_32 segment; //Current segment!
	//dolog("CPU","BIOS Read handler: %08X+%08X",baseoffset,reloffset);
	switch (EMULATED_CPU) //What CPU is being emulated?
	{
		case CPU_8086:
		case CPU_NECV30: //5160 PC!
			tempoffset = basepos;
			if (basepos>=0x10000) return 0; //Not us!
			tempoffset &= 0x7FFF; //Our offset within the ROM!
			if (basepos&0x8000) //u18?
			{
				if (BIOS_ROMS[18]) //Set?
				{
					if (BIOS_ROM_size[18]>tempoffset) //Within range?
					{
						*value = BIOS_ROMS[18][tempoffset]; //Give the value!
						return 1;
					}
				}
			}
			else //u19?
			{
				if (BIOS_ROMS[19]) //Set?
				{
					if (BIOS_ROM_size[19]>tempoffset) //Within range?
					{
						*value = BIOS_ROMS[19][tempoffset]; //Give the value!
						return 1;
					}
				}
			}
			break;
		case CPU_80286:
		case CPU_80386:
		case CPU_80486:
		case CPU_PENTIUM: //5170 AT PC!
			segment = tempoffset = basepos; //Load the offset!
			if (BIOS_ROMS[15] && BIOS_ROMS[13]) //Compaq?
			{
				if (tempoffset<((BIOS_ROM_size[15]+BIOS_ROM_size[13])<<1)) //This is doubled in ROM!
				{
					if (tempoffset>=(BIOS_ROM_size[15]+BIOS_ROM_size[13])) //Second copy?
					{
						tempoffset -= (BIOS_ROM_size[15]+BIOS_ROM_size[13]); //Patch to first block to address!
					}
				}
			}
			tempoffset >>= 1; //The offset is at every 2 bytes of memory!
			segment &= 1; //Even=u27, Odd=u47
			if (segment) //u47/u35/u15?
			{
				if (BIOS_ROMS[15]) //u34/u35 combination?
				{
					if (BIOS_ROM_size[15]>tempoffset) //Within range?
					{
						*value = BIOS_ROMS[15][tempoffset]; //Give the value!
						return 1;
					}
				}
				else if (BIOS_ROMS[35]) //u34/u35 combination?
				{
					if (BIOS_ROM_size[35]>tempoffset) //Within range?
					{
						*value = BIOS_ROMS[35][tempoffset]; //Give the value!
						return 1;
					}
				}
				else //Normal AT BIOS ROM?
				{
					if (BIOS_ROMS[47]) //Loaded?
					{
						if (BIOS_ROM_size[47]>tempoffset) //Within range?
						{
							*value = BIOS_ROMS[47][tempoffset]; //Give the value!
							return 1;
						}
					}
				}
			}
			else //u27/u34/u13?
			{
				if (BIOS_ROMS[13]) //u34/u35 combination?
				{
					if (BIOS_ROM_size[13]>tempoffset) //Within range?
					{
						*value = BIOS_ROMS[13][tempoffset]; //Give the value!
						return 1;
					}
				}
				else if (BIOS_ROMS[34]) //u34/u35 combination?
				{
					if (BIOS_ROM_size[34]>tempoffset) //Within range?
					{
						*value = BIOS_ROMS[34][tempoffset]; //Give the value!
						return 1;
					}
				}
				else //Normal AT BIOS ROM?
				{
					if (BIOS_ROMS[27]) //Loaded?
					{
						if (BIOS_ROM_size[27]>tempoffset) //Within range?
						{
							*value = BIOS_ROMS[27][tempoffset]; //Give the value!
							return 1;
						}
					}
				}
			}
			break;
		default: //Unknown CPU?
			break;
	}

	return 0; //Not recognised, use normal RAM!
}

void BIOS_registerROM()
{
	//Register our read&write handlers for 16&32-bit CPUs!
	MMU_registerWriteHandler(&BIOS_writehandler,"BIOSROM");
	MMU_registerReadHandler(&BIOS_readhandler,"BIOSROM");

	MMU_registerReadHandler(&OPTROM_readhandler,"OPTROM");
	MMU_registerWriteHandler(&OPTROM_writehandler,"OPTROM");
}

extern word CPU_exec_CS;
extern uint_32 CPU_exec_EIP; //Currently executing address!

OPTINLINE byte checkTurboXTBIOS()
{
	word index;
	byte result;
	byte rom_value; //The value of the rom
	byte TurboID[32] = {0x54,0x75,0x72,0x62,0x6F,0x20,0x58,0x54,0x20,0x42,0x49,0x4F,0x53,0x20,0x76,0x32,0x2E,0x35,0x20,0x66,0x6F,0x72,0x20,0x38,0x30,0x38,0x38,0x2F,0x56,0x32,0x30,0x00}; //Our identifier string!
	uint_32 execaddress,biosaddress;
	biosaddress = 0x10000-0x2000; //Start of our BIOS identifier, 8K ROM!
	execaddress = mappage((CPU_exec_CS<<4)+(CPU_exec_EIP)); //We're the Current address! Don't do any protection for our sake!
	result = ((execaddress>=(0xF0000+biosaddress)) && (execaddress<=0xFFFFF)); //We're the Turbo XT BIOS if we're running in it's memory range!
	if (result) //Might be Turbo XT BIOS?
	{
		if (getcpumode()==CPU_MODE_PROTECTED)
		{
			result = 0; //No Turbo XT BIOS after all!
		}
		for (index=0;index<sizeof(TurboID);index++) //Check our ID for checking the BIOS!
		{
			rom_value = MMU_rb(CPU_SEGMENT_CS,0xF000,index+biosaddress,0,!CPU_Address_size[activeCPU]); //The value in the ROM!
			if (rom_value!=TurboID[index])
			{
				return 0; //Not Turbo XT BIOS after all!
			}
		}
	}
	return result; //Give the result!
}

byte isTurboXTBIOS() //Are we running the Turbo XT BIOS now?
{
	return (getcpumode()!=CPU_MODE_PROTECTED)?checkTurboXTBIOS():0;
}

void BIOSROM_dumpBIOS()
{
		uint_64 baseloc, endloc;
		if (is_XT) //XT?
		{
			baseloc = BIOSROM_BASE_XT;
			endloc = 0x100000;
		}
		else if (is_Compaq==1) //32-bit?
		{	
			baseloc = BIOSROM_BASE_Modern;
			endloc = 0x100000000LL;
		}
		else //AT?
		{
			baseloc = BIOSROM_BASE_AT;
			endloc = 0x1000000;
		}
		FILE *f;
		byte data;
		char filename[2][100];
		memset(&filename[0],0,sizeof(filename)); //Clear/init!
		sprintf(filename[0], "%s/ROMDMP.%s.BIN", ROMpath,(is_Compaq?"32":(is_XT?"XT":"AT"))); //Create the filename for the ROM for the architecture!
		sprintf(filename[1], "ROMDMP.%s.BIN",(is_Compaq?"32":(is_XT?"XT":"AT"))); //Create the filename for the ROM for the architecture!

		f = fopen(filename[0],"wb");
		if (!f) return;
		for (;baseloc<endloc;++baseloc)
		{
			if (BIOS_readhandler(baseloc,&data)) //Read directly!
			{
				if (!fwrite(&data,1,1,f)) //Failed to write?
				{
					fclose(f); //close!
					delete_file(ROMpath,filename[1]); //Remove: invalid!
					return;
				}
			}
		}
		fclose(f); //close!
}
