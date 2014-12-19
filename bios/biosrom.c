#include "headers/mmu/mmuhandler.h" //Basic MMU handler support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/support/zalloc.h" //Allocation support!
#include "headers/cpu/cpu.h" //CPU support!

byte EMU_BIOS[0x10000]; //Full custom BIOS from 0xF0000-0xFFFFF for the emulator itself to use!

byte *BIOS_ROMS[0x100]; //All possible BIOS roms!
uint_32 BIOS_ROM_size[0x100]; //All possible BIOS ROM sizes!

byte *OPT_ROMS[40]; //Up to 40 option roms!
uint_32 BIOS_OPTROM_size[40]; //All possible OPT ROM sizes!
word BIOS_OPTROM_location[40]; //All possible OPT ROM locations!

extern BIOS_Settings_TYPE BIOS_Settings;

byte BIOS_checkOPTROMS() //Check and load Option ROMs!
{
	byte i; //Current OPT ROM!
	uint_32 location; //The location within the OPT ROM area!
	location = 0; //Init location!
	for (i=0;(i<NUMITEMS(OPT_ROMS)) && (location!=0x20000);i++) //Process all ROMS we can process!
	{
		FILE *f;
		char filename[100];
		memset(&filename,0,sizeof(filename)); //Clear/init!
		sprintf(filename,"BIOS/OPTROM.%i",i+1); //Create the filename for the ROM!
		f = fopen(filename,"rb");
		if (!f)
		{
			continue; //Failed to load!
		}
		fseek(f,0,SEEK_END); //Goto EOF!
		if (ftell(f)) //Gotten size?
		{
			BIOS_ROM_size[i] = ftell(f); //Save the size!
			fseek(f,0,SEEK_SET); //Goto BOF!
			if ((location+BIOS_ROM_size[i])>0x20000) //Overflow?
			{
				BIOS_ROM_size[i] = 0; //Reset!
				continue; //We're skipping this ROM: it's too big!
			}
			BIOS_ROMS[i] = nzalloc(BIOS_ROM_size[i],filename); //Simple memory allocation for our ROM!
			if (!BIOS_ROMS[i]) //Failed to allocate?
			{
				fclose(f); //Close the file!
				continue; //Failed to allocate!
			}
			if (fread(BIOS_ROMS[i],1,BIOS_ROM_size[i],f)!=BIOS_ROM_size[i]) //Not fully read?
			{
				freez((void *)&BIOS_ROMS[i],BIOS_ROM_size[i],filename); //Failed to read!
				fclose(f); //Close the file!
				continue; //Failed to read!
			}
			fclose(f); //Close the file!
			
			BIOS_OPTROM_location[i] = location; //The option ROM location we're loaded at!
			
			location += BIOS_ROM_size[i]; //Next ROM position!
			if (BIOS_ROM_size[i]&0x7FF) //Not 2KB alligned?
			{
				location += 0x800-(BIOS_ROM_size[i]&0x7FF); //2KB align!
			}
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
			sprintf(filename,"BIOS/OPTROM.%i",i); //Create the filename for the ROM!
			freez((void **)&OPT_ROMS[i],BIOS_OPTROM_size[i],filename); //Release the OPT ROM!
		}
	}
}

int BIOS_load_ROM(byte nr)
{
	FILE *f;
	char filename[100];
	memset(&filename,0,sizeof(filename)); //Clear/init!
	sprintf(filename,"BIOS/ROM.u%i",nr); //Create the filename for the ROM!
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
		BIOS_ROMS[nr] = nzalloc(BIOS_ROM_size[nr],filename); //Simple memory allocation for our ROM!
		if (!BIOS_ROMS[nr]) //Failed to allocate?
		{
			fclose(f); //Close the file!
			return 0; //Failed to allocate!
		}
		if (fread(BIOS_ROMS[nr],1,BIOS_ROM_size[nr],f)!=BIOS_ROM_size[nr]) //Not fully read?
		{
			freez((void *)&BIOS_ROMS[nr],BIOS_ROM_size[nr],filename); //Failed to read!
			fclose(f); //Close the file!
			return 0; //Failed to read!
		}
		fclose(f); //Close the file!
		return 1; //Loaded!
	}
	
	fclose(f);
	return 0; //Failed to load!
}

//Custom loaded BIOS ROM (single only)!
byte *BIOS_custom_ROM;
uint_32 BIOS_custom_ROM_size;
char customROMname[256]; //Custom ROM name!

int BIOS_load_custom(char *rom)
{
	FILE *f;
	char filename[100];
	memset(&filename,0,sizeof(filename)); //Clear/init!
	strcpy(filename,rom); //Create the filename for the ROM!
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
		BIOS_custom_ROM = nzalloc(BIOS_custom_ROM_size,filename); //Simple memory allocation for our ROM!
		if (!BIOS_custom_ROM) //Failed to allocate?
		{
			fclose(f); //Close the file!
			return 0; //Failed to allocate!
		}
		if (fread(BIOS_custom_ROM,1,BIOS_custom_ROM_size,f)!=BIOS_custom_ROM_size) //Not fully read?
		{
			freez((void *)&BIOS_custom_ROM,BIOS_custom_ROM_size,filename); //Failed to read!
			fclose(f); //Close the file!
			return 0; //Failed to read!
		}
		fclose(f); //Close the file!
		strcpy(customROMname,filename); //Custom ROM name for easy dealloc!
		return 1; //Loaded!
	}
	
	fclose(f);
	return 0; //Failed to load!
}


void BIOS_free_ROM(byte nr)
{
	char filename[100];
	memset(&filename,0,sizeof(filename)); //Clear/init!
	sprintf(filename,"BIOSROM.u%i",nr); //Create the filename for the ROM!
	if (BIOS_ROM_size[nr]) //Has size?
	{
		freez((void *)&BIOS_ROMS[nr],BIOS_ROM_size[nr],filename); //Release the BIOS ROM!
		BIOS_ROMS[nr] = NULL; //Not allocated anymore!
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
		freez((void *)&BIOS_custom_ROM,BIOS_custom_ROM_size,filename); //Release the BIOS ROM!
	}
}

int BIOS_load_systemROM() //Load custom ROM from emulator itself!
{
	BIOS_free_custom(NULL); //Free the custom ROM, if needed and known!
	BIOS_custom_ROM_size = sizeof(EMU_BIOS); //Save the size!
	BIOS_custom_ROM = (byte *)&EMU_BIOS; //Simple memory allocation for our ROM!
	return 1; //Loaded!
}

byte OPTROM_readhandler(uint_32 baseoffset, uint_32 reloffset, byte *value)    /* A pointer to a handler function */
{
	byte i;
	for (i=0;i<NUMITEMS(OPT_ROMS);i++) //Check OPT ROMS!
	{
		if (OPT_ROMS[i]) //Enabled?
		{
			if (BIOS_OPTROM_location[i]<=reloffset && (BIOS_OPTROM_location[i]+BIOS_OPTROM_size[i])>=reloffset) //Found ROM?
			{
				*value = OPT_ROMS[i][reloffset-BIOS_OPTROM_location[i]]; //Read the data!
				return 1; //Done: we've been read!
			}
		}
	}
	return 0; //No ROM here, allow read from nroaml memory!
}

byte OPTROM_writehandler(uint_32 baseoffset, uint_32 reloffset, byte value)    /* A pointer to a handler function */
{
	byte i;
	for (i=0;i<NUMITEMS(OPT_ROMS);i++) //Check OPT ROMS!
	{
		if (OPT_ROMS[i]) //Enabled?
		{
			if (BIOS_OPTROM_location[i]<=reloffset && (BIOS_OPTROM_location[i]+BIOS_OPTROM_size[i])>=reloffset) //Found ROM?
			{
				return 1; //Handled: ignore writes to ROM!
			}
		}
	}
	return 0; //No ROM here, allow writes to normal memory!
}

byte BIOS_writehandler(uint_32 baseoffset, uint_32 reloffset, byte value)    /* A pointer to a handler function */
{
	uint_32 segment; //Current segment!
	uint_32 offset; //Current offset within the segment!
	switch (EMULATED_CPU) //What CPU is being emulated?
	{
		case CPU_8086:
		case CPU_80186: //5160 PC!
			offset = reloffset;
			if (BIOS_custom_ROM) //Custom ROM loaded?
			{
				offset &= 0xFFFF; //64-bit ROM!
				if (BIOS_custom_ROM_size>offset) //Within range?
				{
					return 1; //Ignore writes!
				}
			}
			else //Normal BIOS ROM?
			{
				offset &= 0x7FFF; //Our offset within the ROM!
				if (reloffset&0x8000) //u18?
				{
					if (BIOS_ROMS[18]) //Set?
					{
						if (BIOS_ROM_size[18]>offset) //Within range?
						{
							return 1; //Ignore writes!
						}
					}
				}
				else //u19?
				{
					if (BIOS_ROMS[19]) //Set?
					{
						if (BIOS_ROM_size[19]>offset) //Within range?
						{
							return 1; //Ignore writes!
						}
					}
				}
			}
			break;
		case CPU_80286:
		case CPU_80386:
		case CPU_80486: //5170 AT PC!
			segment = offset = reloffset; //Load the offset!
			offset >>= 1; //The offset is at every 2 bytes of memory!
			segment &= 1; //Even=u27, Odd=u47
			if (segment) //u47?
			{
				if (BIOS_ROMS[47]) //Loaded?
				{
					if (BIOS_ROM_size[47]>offset) //Within range?
					{
						return 1; //Ignore writes!
					}
				}
			}
			else //u27?
			{
				if (BIOS_ROMS[27]) //Loaded?
				{
					if (BIOS_ROM_size[27]>offset) //Within range?
					{
						return 1; //Ignore writes!
					}
				}
			}
			break;
		default: //Unknown CPU?
			break;
	}
	return 0; //Not recognised, use normal RAM!
}

byte BIOS_readhandler(uint_32 baseoffset, uint_32 reloffset, byte *value) /* A pointer to a handler function */
{
	uint_32 segment; //Current segment!
	uint_32 offset; //Current offset within the segment!
	//dolog("CPU","BIOS Read handler: %08X+%08X",baseoffset,reloffset);
	switch (EMULATED_CPU) //What CPU is being emulated?
	{
		case CPU_8086:
		case CPU_80186: //5160 PC!
			offset = reloffset;
			if (BIOS_custom_ROM) //Custom ROM loaded?
			{
				offset &= 0xFFFF; //64-bit ROM!
				if (BIOS_custom_ROM_size>offset) //Within range?
				{
					*value = BIOS_custom_ROM[offset]; //Give the value!
					return 1;
				}
				//dolog("CPU","Custom ROM out of range!");
			}
			else //Normal BIOS ROM?
			{
				offset &= 0x7FFF; //Our offset within the ROM!
				if (reloffset&0x8000) //u18?
				{
					if (BIOS_ROMS[18]) //Set?
					{
						if (BIOS_ROM_size[18]>offset) //Within range?
						{
							*value = BIOS_ROMS[18][offset]; //Give the value!
							return 1;
						}
					}
				}
				else //u19?
				{
					if (BIOS_ROMS[19]) //Set?
					{
						if (BIOS_ROM_size[19]>offset) //Within range?
						{
							*value = BIOS_ROMS[19][offset]; //Give the value!
							return 1;
						}
					}
				}
			}
			break;
		case CPU_80286:
		case CPU_80386:
		case CPU_80486: //5170 AT PC!
			segment = offset = reloffset; //Load the offset!
			offset >>= 1; //The offset is at every 2 bytes of memory!
			segment &= 1; //Even=u27, Odd=u47
			if (segment) //u47?
			{
				if (BIOS_ROMS[47]) //Loaded?
				{
					if (BIOS_ROM_size[47]>offset) //Within range?
					{
						*value = BIOS_ROMS[47][offset]; //Give the value!
						return 1;
					}
				}
			}
			else //u27?
			{
				if (BIOS_ROMS[27]) //Loaded?
				{
					if (BIOS_ROM_size[27]>offset) //Within range?
					{
						*value = BIOS_ROMS[27][offset]; //Give the value!
						return 1;
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
	MMU_registerWriteHandler(0xF0000,0xFFFFF,&BIOS_writehandler,"BIOSROM16");
	MMU_registerReadHandler(0xF0000,0xFFFFF,&BIOS_readhandler,"BIOSROM16");
	MMU_registerWriteHandler(0xFFFF0000,0xFFFFFFFF,&BIOS_writehandler,"BIOSROM32");
	MMU_registerReadHandler(0xFFFF0000,0xFFFFFFFF,&BIOS_readhandler,"BIOSROM32");

	MMU_registerReadHandler(0xC0000,0xEFFFF,&OPTROM_readhandler,"OPTROM16");	
	MMU_registerWriteHandler(0xC0000,0xEFFFF,&OPTROM_writehandler,"OPTROM16");	
	MMU_registerReadHandler(0xFFFC0000,0xFFFEFFFF,&OPTROM_readhandler,"OPTROM32");
	MMU_registerWriteHandler(0xFFFC0000,0xFFFEFFFF,&OPTROM_writehandler,"OPTROM32");
}