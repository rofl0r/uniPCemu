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
uint_32 BIOS_ROM_size[0x100]; //All possible BIOS ROM sizes!

byte numOPT_ROMS = 0;
byte *OPT_ROMS[40]; //Up to 40 option roms!
uint_32 OPTROM_size[40]; //All possible OPT ROM sizes!
uint_64 OPTROM_location[40]; //All possible OPT ROM locations(low word) and end position(high word)!
char OPTROM_filename[40][256]; //All possible filenames for the OPTROMs loaded!

byte OPTROM_writeSequence[40]; //Current write sequence command state!
byte OPTROM_writeSequence_waitingforDisable[40]; //Waiting for disable command?
byte OPTROM_writeenabled[40]; //Write enabled ROM?

extern BIOS_Settings_TYPE BIOS_Settings;

int BIOS_load_VGAROM(); //Prototype: Load custom ROM from emulator itself!

byte ISVGA = 0; //VGA that's loaded!

char ROMpath[256] = "ROM";
char originalROMpath[256] = "ROM"; //Original ROM path!

extern byte is_XT; //Are we emulating an XT architecture?

uint_32 BIOSROM_BASE_Modern = 0xFFFF0000; //AT+ BIOS ROM base!
uint_32 BIOSROM_BASE_AT = 0xFF0000; //AT BIOS ROM base!
uint_32 BIOSROM_BASE_XT = 0xF0000; //XT BIOS ROM base!

byte BIOS_checkOPTROMS() //Check and load Option ROMs!
{
	strcpy(originalROMpath,ROMpath); //Save the original ROM path for deallocation!
	numOPT_ROMS = 0; //Initialise the number of OPTROMS!
	memset(OPTROM_writeenabled, 0, sizeof(OPTROM_writeenabled)); //Disable all write enable flags by default!
	memset(OPTROM_writeSequence, 0, sizeof(OPTROM_writeSequence)); //Disable all write enable flags by default!
	memset(OPTROM_writeSequence_waitingforDisable, 0, sizeof(OPTROM_writeSequence_waitingforDisable)); //Disable all write enable flags by default!
	byte i; //Current OPT ROM!
	uint_32 location; //The location within the OPT ROM area!
	location = 0; //Init location!
	ISVGA = 0; //Are we a VGA ROM?
	for (i=0;(i<NUMITEMS(OPT_ROMS)) && (location!=0x20000);i++) //Process all ROMS we can process!
	{
		FILE *f;
		char filename[100];
		memset(&filename,0,sizeof(filename)); //Clear/init!
		if (i) //Not Graphics Adapter ROM?
		{
			ISVGA = 0; //No VGA!
			//Default!
			sprintf(filename, "%s/OPTROM.%s.%u.BIN", ROMpath,is_XT?"XT":"AT", i); //Create the filename for the ROM for the architecture!
			if (!file_exists(filename)) //This version doesn't exist? Then try the other version!
			{
				sprintf(filename, "%s/OPTROM.%u.BIN", ROMpath, i); //Create the filename for the ROM!
			}
		}
		else
		{
			ISVGA = 0; //No VGA!
			if (BIOS_Settings.VGA_Mode==4) //Pure CGA?
			{
				sprintf(filename, "%s/CGAROM.%s.BIN", ROMpath, is_XT ? "XT" : "AT"); //Create the filename for the ROM for the architecture!
				if (!file_exists(filename)) //This version doesn't exist? Then try the other version!
				{
					sprintf(filename,"%s/CGAROM.BIN",ROMpath); //CGA ROM!
				}
			}
			else if (BIOS_Settings.VGA_Mode==5) //Pure MDA?
			{
				sprintf(filename, "%s/MDAROM.%s.BIN", ROMpath, is_XT ? "XT" : "AT"); //Create the filename for the ROM for the architecture!
				sprintf(filename, "%s/MDAROM.BIN", ROMpath); //MDA ROM!
			}
			else
			{	
				ISVGA = 1; //We're a VGA!
				if (BIOS_Settings.VGA_Mode==6) //ET4000?
				{
					sprintf(filename, "%s/ET4000.%s.BIN", ROMpath, is_XT ? "XT" : "AT"); //Create the filename for the ROM for the architecture!
					if (!file_exists(filename)) //This version doesn't exist? Then try the other version!
					{
						sprintf(filename, "%s/ET4000.BIN", ROMpath); //ET4000 ROM!
					}
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
					sprintf(filename, "%s/ET3000.%s.BIN", ROMpath, is_XT ? "XT" : "AT"); //Create the filename for the ROM for the architecture!
					if (!file_exists(filename)) //This version doesn't exist? Then try the other version!
					{
						sprintf(filename, "%s/ET3000.BIN", ROMpath); //ET3000 ROM!
					}
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
				else //Plain VGA?
				{
					sprintf(filename, "%s/VGAROM.%s.BIN", ROMpath, is_XT ? "XT" : "AT"); //Create the filename for the ROM for the architecture!
					if (!file_exists(filename)) //This version doesn't exist? Then try the other version!
					{
						sprintf(filename, "%s/VGAROM.BIN", ROMpath); //VGA ROM!
					}
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
			bzero(&OPTROM_filename[i],0); //Init filename!
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
	uint_32 ROM_size=0; //The size of both ROMs!
	FILE *f;
	char filename[100];
	memset(&filename,0,sizeof(filename)); //Clear/init!
	sprintf(filename,"%s/BIOSROM.U%u.BIN",originalROMpath,nr); //Create the filename for the ROM!
	f = fopen(filename,"rb");
	if (!f)
	{
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
		}
		
		//Recalculate based on ROM size!
		BIOSROM_BASE_AT = 0xFFFFFF-(ROM_size-1); //AT ROM size!
		BIOSROM_BASE_XT = 0xFFFFF-(ROM_size-1); //XT ROM size!
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
		BIOSROM_BASE_AT = 0xFFFFFF-(BIOS_custom_ROM_size-1); //AT ROM size!
		BIOSROM_BASE_XT = 0xFFFFF-(BIOS_custom_ROM_size-1); //XT ROM size!
		BIOSROM_BASE_Modern = 0xFFFFFFFF-(BIOS_custom_ROM_size-1); //Modern ROM size!
		return 1; //Loaded!
	}
	
	fclose(f);
	return 0; //Failed to load!
}


void BIOS_free_ROM(byte nr)
{
	char filename[100];
	memset(&filename,0,sizeof(filename)); //Clear/init!
	sprintf(filename,"BIOSROM.U%u.BIN",nr); //Create the filename for the ROM!
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

byte OPTROM_readhandler(uint_32 offset, byte *value)    /* A pointer to a handler function */
{
	INLINEREGISTER uint_64 basepos, currentpos, temppos; //Current position!
	basepos = currentpos = offset; //Load the offset!
	if ((basepos >= 0xC0000) && (basepos<0xF0000)) basepos = 0xC0000; //Our base reference position!
	else //Out of range (16-bit)?
	{
		if ((basepos >= 0xC0000000) && (basepos < 0xF0000000)) basepos = 0xC0000000; //Our base reference position!
		else return 0; //Our of range (32-bit)?
	}
	currentpos -= basepos; //Calculate from the base position!
	basepos = currentpos; //Save a backup!
	INLINEREGISTER byte i=0,j=numOPT_ROMS;
	if (!numOPT_ROMS) goto noOPTROMSR;
	do //Check OPT ROMS!
	{
		currentpos = OPTROM_location[i]; //Load the current location for analysis and usage!
		if (OPT_ROMS[i] && ((currentpos>>32)>basepos)) //Before the end location and valid rom?
		{
			currentpos &= 0xFFFFFFFF; //The location of the ROM itself!
			if (currentpos <= basepos) //At/after the start location? We've found the ROM!
			{
				if (VGAROM_mapping!=0xFF) //Special mapping?
				{
					temppos = basepos-currentpos; //Calculate the offset!
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
				*value = OPT_ROMS[i][basepos-currentpos]; //Read the data from the ROM!
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
					if (VGAROM_mapping!=0xFF) //Special mapping?
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
							return 1; //Ignore write!
						}
						else if (OPTROM_writeSequence[i] == 2) //We're a command byte!
						{
							switch (value)
							{
							case 0xA0: //Enable write protect!
								OPTROM_writeSequence_waitingforDisable[i] = 0; //Not waiting anymore!
								OPTROM_writeSequence[i] = 0; //Finished write sequence!
								OPTROM_writeenabled[i] = 0; //We're disabling writes to the EEPROM!
								return 1; //Ignore write!
								break;
							case 0x80: //Wait for 0x20 to disable write protect!
								OPTROM_writeSequence_waitingforDisable[i] = 1; //Waiting for disable!
								OPTROM_writeSequence[i] = 0; //Finished write sequence!
								return 1; //Ignore write!
								break;
							case 0x20: //Disable write protect!
								if (OPTROM_writeSequence_waitingforDisable[i]) //Waiting for disable?
								{
									OPTROM_writeenabled[i] = 1; //We're enabling writes to the EEPROM!
								}
								OPTROM_writeSequence_waitingforDisable[i] = 0; //Not waiting anymore!
								OPTROM_writeSequence[i] = 0; //Finished write sequence!
								return 1; //Ignore write!
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
							return 1; //Ignore write!
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
		else if ((basepos >= BIOSROM_BASE_AT) && (EMULATED_CPU == CPU_80286)) basepos = BIOSROM_BASE_AT; //Our base reference position(high memmory 286)
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
				return 1; //Ignore writes!
			}
		}
		if (tempoffset<BIOS_custom_ROM_size) //Within range?
		{
			return 1; //Ignore writes!
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
			tempoffset >>= 1; //The offset is at every 2 bytes of memory!
			segment &= 1; //Even=u27, Odd=u47
			if (segment) //u47/u35?
			{
				if (BIOS_ROMS[35]) //u34/u35 combination?
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
			else //u27/u34?
			{
				if (BIOS_ROMS[34]) //u34/u35 combination?
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
		else if ((basepos >= BIOSROM_BASE_AT) && (EMULATED_CPU == CPU_80286)) basepos = BIOSROM_BASE_AT; //Our base reference position(high memmory 286)
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
		if (tempoffset<BIOS_custom_ROM_size) //Within range?
		{
			*value = BIOS_custom_ROM[tempoffset]; //Give the value!
			return 1; //ROM offset from the end of RAM used!
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
			tempoffset >>= 1; //The offset is at every 2 bytes of memory!
			segment &= 1; //Even=u27, Odd=u47
			if (segment) //u47/u35?
			{
				if (BIOS_ROMS[35]) //u34/u35 combination?
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
			else //u27/u34?
			{
				if (BIOS_ROMS[34]) //u34/u35 combination?
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
			rom_value = MMU_rb(CPU_SEGMENT_CS,0xF000,index+biosaddress,0); //The value in the ROM!
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
