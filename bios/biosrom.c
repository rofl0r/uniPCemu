#include "headers/mmu/mmuhandler.h" //Basic MMU handler support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/support/zalloc.h" //Allocation support!
#include "headers/cpu/cpu.h" //CPU support!

byte EMU_BIOS[0x10000]; //Full custom BIOS from 0xF0000-0xFFFFF for the emulator itself to use!
byte EMU_VGAROM[0x8000]; //Maximum size custom BIOS VGA ROM!

byte *BIOS_ROMS[0x100]; //All possible BIOS roms!
uint_32 BIOS_ROM_size[0x100]; //All possible BIOS ROM sizes!

byte *OPT_ROMS[40]; //Up to 40 option roms!
uint_32 OPTROM_size[40]; //All possible OPT ROM sizes!
word OPTROM_location[40]; //All possible OPT ROM locations!

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
		sprintf(filename,"ROM/OPTROM.%i",i+1); //Create the filename for the ROM!
		f = fopen(filename,"rb");
		if (!f)
		{
			continue; //Failed to load!
		}
		fseek(f,0,SEEK_END); //Goto EOF!
		if (ftell(f)) //Gotten size?
		{
			OPTROM_size[i] = ftell(f); //Save the size!
			fseek(f,0,SEEK_SET); //Goto BOF!
			if ((location+OPTROM_size[i])>0x20000) //Overflow?
			{
				BIOS_ROM_size[i] = 0; //Reset!
				continue; //We're skipping this ROM: it's too big!
			}
			OPT_ROMS[i] = (byte *)nzalloc(OPTROM_size[i],filename,NULL); //Simple memory allocation for our ROM!
			if (!OPT_ROMS[i]) //Failed to allocate?
			{
				fclose(f); //Close the file!
				continue; //Failed to allocate!
			}
			if (fread(OPT_ROMS[i],1,OPTROM_size[i],f)!=OPTROM_size[i]) //Not fully read?
			{
				freez((void **)&OPT_ROMS[i],OPTROM_size[i],filename); //Failed to read!
				fclose(f); //Close the file!
				continue; //Failed to read!
			}
			fclose(f); //Close the file!
			
			OPTROM_location[i] = location; //The option ROM location we're loaded at!
			
			location += OPTROM_size[i]; //Next ROM position!
			if (OPTROM_size[i]&0x7FF) //Not 2KB alligned?
			{
				location += 0x800-(OPTROM_size[i]&0x7FF); //2KB align!
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
			sprintf(filename,"ROM/OPTROM.%i",i); //Create the filename for the ROM!
			freez((void **)&OPT_ROMS[i],OPTROM_size[i],filename); //Release the OPT ROM!
		}
	}
}

int BIOS_load_ROM(byte nr)
{
	FILE *f;
	char filename[100];
	memset(&filename,0,sizeof(filename)); //Clear/init!
	sprintf(filename,"ROM/BIOSROM.u%i",nr); //Create the filename for the ROM!
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
		BIOS_ROMS[nr] = (byte *)nzalloc(BIOS_ROM_size[nr],filename,getLock("ROM")); //Simple memory allocation for our ROM!
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
		BIOS_custom_ROM = (byte *)nzalloc(BIOS_custom_ROM_size,filename,getLock("ROM")); //Simple memory allocation for our ROM!
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
}

int BIOS_load_systemROM() //Load custom ROM from emulator itself!
{
	BIOS_free_custom(NULL); //Free the custom ROM, if needed and known!
	BIOS_custom_ROM_size = sizeof(EMU_BIOS); //Save the size!
	BIOS_custom_ROM = &EMU_BIOS[0]; //Simple memory allocation for our ROM!
	return 1; //Loaded!
}

void BIOS_DUMPSYSTEMROM() //Dump the SYSTEM ROM currently set (debugging purposes)!
{
	if (BIOS_custom_ROM == &EMU_BIOS[0]) //We're our own BIOS?
	{
		//Dump our own BIOS ROM!
		FILE *f;
		f = fopen("SYSROM.DMP", "wb");
		fwrite(&EMU_BIOS, 1, sizeof(EMU_BIOS), f); //Save our BIOS!
		fclose(f);
	}
}


//VGA support!

byte *BIOS_custom_VGAROM;
uint_32 BIOS_custom_VGAROM_size;
char customVGAROMname[256] = "EMU_VGAROM"; //Custom ROM name!

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


byte OPTROM_readhandler(uint_32 baseoffset, uint_32 reloffset, byte *value)    /* A pointer to a handler function */
{
	byte i;
	for (i=0;i<NUMITEMS(OPT_ROMS);i++) //Check OPT ROMS!
	{
		if (OPT_ROMS[i]) //Enabled?
		{
			if (OPTROM_location[i]<=reloffset && (OPTROM_location[i]+OPTROM_size[i])>reloffset) //Found ROM?
			{
				*value = OPT_ROMS[i][reloffset-OPTROM_location[i]]; //Read the data!
				unlock("ROM"); //Finished!
				return 1; //Done: we've been read!
			}
		}
	}
	if (BIOS_custom_VGAROM_size) //Custom VGA ROM mounted?
	{
		if (reloffset < BIOS_custom_VGAROM_size) //OK?
		{
			*value = BIOS_custom_VGAROM[reloffset]; //Give the value!
			unlock("ROM"); //Finished!
			return 1;
		}
	}
	unlock("ROM"); //Finished!
	return 0; //No ROM here, allow read from nroaml memory!
}

byte OPTROM_writehandler(uint_32 baseoffset, uint_32 reloffset, byte value)    /* A pointer to a handler function */
{
	byte i;
	for (i=0;i<NUMITEMS(OPT_ROMS);i++) //Check OPT ROMS!
	{
		if (OPT_ROMS[i]) //Enabled?
		{
			if (OPTROM_location[i]<=reloffset && (OPTROM_location[i]+OPTROM_size[i])>reloffset) //Found ROM?
			{
				unlock("ROM"); //Finished!
				return 1; //Handled: ignore writes to ROM!
			}
		}
	}
	if (BIOS_custom_VGAROM_size) //Custom VGA ROM mounted?
	{
		if (reloffset < BIOS_custom_VGAROM_size) //OK?
		{
			unlock("ROM"); //Finished!
			return 1; //Ignore writes!
		}
	}
	unlock("ROM"); //Finished!
	return 0; //No ROM here, allow writes to normal memory!
}

byte BIOS_writehandler(uint_32 baseoffset, uint_32 reloffset, byte value)    /* A pointer to a handler function */
{
	uint_32 offset; //Current offset within the segment!
	if (BIOS_custom_ROM) //Custom/system ROM loaded?
	{
		offset = reloffset;
		offset &= 0xFFFF; //16-bit ROM!
		if (BIOS_custom_ROM_size == 0x10000)
		{
			unlock("ROM"); //Finished!
			return 1; //Unwritable BIOS!
		}
		if (offset>0xFFFF-BIOS_custom_ROM_size) //Within range?
		{
			unlock("ROM"); //Finished!
			return 1; //Ignore writes!
		}
	}

	uint_32 segment; //Current segment!
	switch (EMULATED_CPU) //What CPU is being emulated?
	{
		case CPU_8086:
		case CPU_80186: //5160 PC!
			offset = reloffset;
			offset &= 0x7FFF; //Our offset within the ROM!
			if (reloffset&0x8000) //u18?
			{
				if (BIOS_ROMS[18]) //Set?
				{
					if (BIOS_ROM_size[18]>offset) //Within range?
					{
						unlock("ROM"); //Finished!
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
						unlock("ROM"); //Finished!
						return 1; //Ignore writes!
					}
				}
			}
			break;
		case CPU_80286:
		case CPU_80386:
		case CPU_80486:
		case CPU_PENTIUM: //5170 AT PC!
			segment = offset = reloffset; //Load the offset!
			offset >>= 1; //The offset is at every 2 bytes of memory!
			segment &= 1; //Even=u27, Odd=u47
			if (segment) //u47?
			{
				if (BIOS_ROMS[47]) //Loaded?
				{
					if (BIOS_ROM_size[47]>offset) //Within range?
					{
						unlock("ROM"); //Finished!
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
						unlock("ROM"); //Finished!
						return 1; //Ignore writes!
					}
				}
			}
			break;
		default: //Unknown CPU?
			break;
	}

	unlock("ROM"); //Finished!
	return 0; //Not recognised, use normal RAM!
}

byte BIOS_readhandler(uint_32 baseoffset, uint_32 reloffset, byte *value) /* A pointer to a handler function */
{
	for (;!lock("ROM");) delay(0);
	uint_32 offset; //Current offset within the segment!
	if (BIOS_custom_ROM) //Custom/system ROM loaded?
	{
		offset = reloffset; //Reload!
		offset &= 0xFFFF; //16-bit ROM!
		if (BIOS_custom_ROM_size == 0x10000)
		{
			*value = BIOS_custom_ROM[offset]; //Give the value!
			unlock("ROM"); //Finished!
			return 1; //Direct offset used!
		}
		if (offset>(0xFFFF-BIOS_custom_ROM_size)) //Within range?
		{
			offset -= (0xFFFF - BIOS_custom_ROM_size)+1;
			*value = BIOS_custom_ROM[offset]; //Give the value!
			unlock("ROM"); //Finished!
			return 1; //ROM offset from the end of RAM used!
		}
	}

	uint_32 segment; //Current segment!
	//dolog("CPU","BIOS Read handler: %08X+%08X",baseoffset,reloffset);
	switch (EMULATED_CPU) //What CPU is being emulated?
	{
		case CPU_8086:
		case CPU_80186: //5160 PC!
			offset = reloffset;
			offset &= 0x7FFF; //Our offset within the ROM!
			if (reloffset&0x8000) //u18?
			{
				if (BIOS_ROMS[18]) //Set?
				{
					if (BIOS_ROM_size[18]>offset) //Within range?
					{
						*value = BIOS_ROMS[18][offset]; //Give the value!
						unlock("ROM"); //Finished!
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
						unlock("ROM"); //Finished!
						return 1;
					}
				}
			}
			break;
		case CPU_80286:
		case CPU_80386:
		case CPU_80486:
		case CPU_PENTIUM: //5170 AT PC!
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
						unlock("ROM"); //Finished!
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
						unlock("ROM"); //Finished!
						return 1;
					}
				}
			}
			break;
		default: //Unknown CPU?
			break;
	}

	unlock("ROM"); //Finished!
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