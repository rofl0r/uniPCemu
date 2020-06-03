/*

Copyright (C) 2019  Superfury

This file is part of UniPCemu.

UniPCemu is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

UniPCemu is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with UniPCemu.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "headers/mmu/mmuhandler.h" //Basic MMU handler support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/support/zalloc.h" //Allocation support!
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/cpu/paging.h" //Paging support for address decoding (for Turbo XT BIOS detection)!
#include "headers/support/locks.h" //Locking support!
#include "headers/fopen64.h" //64-bit fopen support!

//Comment this define to disable logging
//#define __ENABLE_LOGGING

#ifdef __ENABLE_LOGGING
#include "headers/support/log.h" //Logging support!
#else
//Ignore logging!
#define dolog(...)
#endif

byte EMU_BIOS[0x10000]; //Full custom BIOS from 0xF0000-0xFFFFF for the emulator itself to use!
byte EMU_VGAROM[0x10000]; //Maximum size custom BIOS VGA ROM!

byte* BIOS_combinedROM; //Combined ROM with all odd/even made linear!
uint_32 BIOS_combinedROM_size = 0; //The size of the combined ROM!
byte *BIOS_ROMS[0x100]; //All possible BIOS roms!
byte BIOS_ROMS_ext[0x100]; //Extended load used to load us?
uint_32 BIOS_ROM_size[0x100]; //All possible BIOS ROM sizes!

byte numOPT_ROMS = 0;
byte *OPT_ROMS[40]; //Up to 40 option roms!
uint_32 OPTROM_size[40]; //All possible OPT ROM sizes!
uint_64 OPTROM_location[40]; //All possible OPT ROM locations(low word) and end position(high word)!
char OPTROM_filename[40][256]; //All possible filenames for the OPTROMs loaded!

byte OPTROM_writeSequence[40]; //Current write sequence command state!
byte OPTROM_pendingAA_1555[40]; //Pending write AA to 1555?
byte OPTROM_pending55_0AAA[40]; //Pending write 55 to 0AAA?
byte OPTROM_writeSequence_waitingforDisable[40]; //Waiting for disable command?
byte OPTROM_writeenabled[40]; //Write enabled ROM?
DOUBLE OPTROM_writetimeout[40]; //Timeout until SDP is activated!
byte OPTROM_timeoutused = 0;

extern BIOS_Settings_TYPE BIOS_Settings;

int BIOS_load_VGAROM(); //Prototype: Load custom ROM from emulator itself!

byte ISVGA = 0; //VGA that's loaded!

char ROMpath[256] = "ROM";

extern byte is_XT; //Are we emulating an XT architecture?

uint_32 BIOSROM_BASE_Modern = 0xFFFF0000; //AT+ BIOS ROM base!
uint_32 BIOSROM_BASE_AT = 0xFF0000; //AT BIOS ROM base!
uint_32 BIOSROM_BASE_XT = 0xF0000; //XT BIOS ROM base!

extern byte is_Compaq; //Are we emulating a Compaq device?
extern byte is_PS2; //Are we emulating a Compaq with PS/2 mouse(modern) device?

void scanROM(char *device, char *filename, uint_32 size)
{
	//Special case: 32-bit uses Compaq ROMs!
	snprintf(filename, size, "%s/%s.%s.BIN", ROMpath, device, (is_PS2?"PS2":(is_Compaq ? "32" : (is_XT ? "XT" : "AT")))); //Create the filename for the ROM for the architecture!
	if (!file_exists(filename)) //This version doesn't exist? Then try the other version!
	{
		snprintf(filename, size, "%s/%s.%s.BIN", ROMpath, device, (is_Compaq ? "32" : (is_XT ? "XT" : "AT"))); //Create the filename for the ROM for the architecture!
		if (!file_exists(filename)) //This version doesn't exist? Then try the other version!
		{
			snprintf(filename, size, "%s/%s.%s.BIN", ROMpath, device, is_XT ? "XT" : "AT"); //Create the filename for the ROM for the architecture!
			if (!file_exists(filename)) //This version doesn't exist? Then try the other version!
			{
				snprintf(filename, size, "%s/%s.BIN", ROMpath, device); //CGA ROM!
			}
		}
	}
}

byte BIOS_checkOPTROMS() //Check and load Option ROMs!
{
	numOPT_ROMS = 0; //Initialise the number of OPTROMS!
	memset(&OPTROM_writeenabled, 0, sizeof(OPTROM_writeenabled)); //Disable all write enable flags by default!
	memset(&OPTROM_writeSequence, 0, sizeof(OPTROM_writeSequence)); //Disable all write enable flags by default!
	memset(&OPTROM_writeSequence_waitingforDisable, 0, sizeof(OPTROM_writeSequence_waitingforDisable)); //Disable all write enable flags by default!
	memset(&OPTROM_writetimeout,0,sizeof(OPTROM_writetimeout)); //Disable all timers for all ROMs!
	memset(&OPTROM_pending55_0AAA,0,sizeof(OPTROM_pending55_0AAA)); //Disable all timers for all ROMs!
	memset(&OPTROM_pendingAA_1555,0,sizeof(OPTROM_pendingAA_1555)); //Disable all timers for all ROMs!
	OPTROM_timeoutused = 0; //Not timing?
	byte i; //Current OPT ROM!
	uint_32 location; //The location within the OPT ROM area!
	location = 0; //Init location!
	ISVGA = 0; //Are we a VGA ROM?
	for (i=0;(i<NUMITEMS(OPT_ROMS)) && (location<0x20000);i++) //Process all ROMS we can process!
	{
		BIGFILE *f;
		char filename[256];
		memset(&filename,0,sizeof(filename)); //Clear/init!
		if (i) //Not Graphics Adapter ROM?
		{
			//Default!
			snprintf(filename,sizeof(filename), "%s/OPTROM.%s.%u.BIN", ROMpath,(is_Compaq?"32":(is_XT?"XT":"AT")), i); //Create the filename for the ROM for the architecture!
			if (!file_exists(filename)) //This version doesn't exist? Then try the other version!
			{
				snprintf(filename,sizeof(filename), "%s/OPTROM.%s.%u.BIN", ROMpath,is_XT?"XT":"AT", i); //Create the filename for the ROM for the architecture!
				if (!file_exists(filename)) //This version doesn't exist? Then try the other version!
				{
					snprintf(filename,sizeof(filename), "%s/OPTROM.%u.BIN", ROMpath, i); //Create the filename for the ROM!
				}
			}
		}
		else
		{
			ISVGA = 0; //No VGA!
			if (BIOS_Settings.VGA_Mode==4) //Pure CGA?
			{
				scanROM("CGAROM",&filename[0],sizeof(filename)); //Scan for a CGA ROM!
			}
			else if (BIOS_Settings.VGA_Mode==5) //Pure MDA?
			{
				scanROM("MDAROM",&filename[0],sizeof(filename)); //Scan for a MDA ROM!
			}
			else
			{	
				ISVGA = 1; //We're a VGA!
				if (BIOS_Settings.VGA_Mode==6) //ET4000?
				{
					scanROM("ET4000",&filename[0],sizeof(filename)); //Scan for a ET4000 ROM!
					//Provide emulator fallback support!
					if (file_exists(filename)) //Full ET4000?
					{
						ISVGA = 2; //ET4000!
						//ET4000 ROM!
					}
					else //VGA ROM?
					{
						safestrcpy(filename,sizeof(filename), ""); //VGA ROM!
					}
				}
				else if (BIOS_Settings.VGA_Mode == 7) //ET3000?
				{
					scanROM("ET3000",&filename[0],sizeof(filename)); //Scan for a ET3000 ROM!
					//Provide emulator fallback support!
					if (file_exists(filename)) //Full ET3000?
					{
						ISVGA = 3; //ET3000!
						//ET3000 ROM!
					}
					else //VGA ROM?
					{
						safestrcpy(filename,sizeof(filename), ""); //VGA ROM!
					}
				}
				else if (BIOS_Settings.VGA_Mode == 8) //EGA?
				{
					scanROM("EGAROM",&filename[0],sizeof(filename)); //Scan for a EGA ROM!
					//Provide emulator fallback support!
					if (file_exists(filename)) //Full EGA?
					{
						ISVGA = 4; //EGA!
						//EGA ROM!
					}
					else //VGA ROM?
					{
						safestrcpy(filename,sizeof(filename), ""); //VGA ROM!
					}
				}
				else //Plain VGA?
				{
					scanROM("VGAROM",&filename[0],sizeof(filename)); //Scan for a VGA ROM!
				}
			}
		}
		if (strcmp(filename,"")==0) //No ROM?
		{
			f = NULL; //No ROM!
		}
		else //Try to open!
		{
			f = emufopen64(filename,"rb");
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
		emufseek64(f,0,SEEK_END); //Goto EOF!
		if (emuftell64(f)) //Gotten size?
		{
			OPTROM_size[numOPT_ROMS] = (uint_32)emuftell64(f); //Save the size!
			emufseek64(f,0,SEEK_SET); //Goto BOF!
			if ((location+OPTROM_size[numOPT_ROMS])>0x20000) //Overflow?
			{
				if (!i) //First ROM is reserved by the VGA BIOS ROM. If not found, we're skipping it and using the internal VGA BIOS!
				{
					if (!((sizeof(EMU_VGAROM) + 0xC0000) > BIOSROM_BASE_XT)) //Not more than we can handle?
					{
						location = sizeof(EMU_VGAROM); //Allocate the Emulator VGA ROM for the first entry instead!
						BIOS_load_VGAROM(); //Load the BIOS VGA ROM!
					}
				}
				BIOS_ROM_size[numOPT_ROMS] = 0; //Reset!
				continue; //We're skipping this ROM: it's too big!
			}
			OPT_ROMS[numOPT_ROMS] = (byte *)nzalloc(OPTROM_size[numOPT_ROMS],filename,getLock(LOCK_CPU)); //Simple memory allocation for our ROM!
			if (!OPT_ROMS[numOPT_ROMS]) //Failed to allocate?
			{
				emufclose64(f); //Close the file!
				if (!i) //First ROM is reserved by the VGA BIOS ROM. If not found, we're skipping it and using the internal VGA BIOS!
				{
					if (!((sizeof(EMU_VGAROM) + 0xC0000) > BIOSROM_BASE_XT)) //Not more than we can handle?
					{
						unlock(LOCK_CPU);
						freez((void **)&OPT_ROMS[numOPT_ROMS], OPTROM_size[numOPT_ROMS], filename); //Release the ROM!
						lock(LOCK_CPU);
						location = sizeof(EMU_VGAROM); //Allocate the Emulator VGA ROM for the first entry instead!
						BIOS_load_VGAROM(); //Load the BIOS VGA ROM!
					}
				}
				continue; //Failed to allocate!
			}
			if (emufread64(OPT_ROMS[numOPT_ROMS],1,OPTROM_size[numOPT_ROMS],f)!=OPTROM_size[numOPT_ROMS]) //Not fully read?
			{
				freez((void **)&OPT_ROMS[numOPT_ROMS],OPTROM_size[numOPT_ROMS],filename); //Failed to read!
				emufclose64(f); //Close the file!
				if (!i) //First ROM is reserved by the VGA BIOS ROM. If not found, we're skipping it and using the internal VGA BIOS!
				{
					if (!((sizeof(EMU_VGAROM) + 0xC0000) > BIOSROM_BASE_XT)) //Not more than we can handle?
					{
						unlock(LOCK_CPU);
						freez((void **)&OPT_ROMS[numOPT_ROMS], OPTROM_size[numOPT_ROMS], filename); //Release the ROM!
						lock(LOCK_CPU);
						location = sizeof(EMU_VGAROM); //Allocate the Emulator VGA ROM for the first entry instead!
						BIOS_load_VGAROM(); //Load the BIOS VGA ROM!
					}
				}
				continue; //Failed to read!
			}
			emufclose64(f); //Close the file!
			
			OPTROM_location[numOPT_ROMS] = location; //The option ROM location we're loaded at!
			cleardata(&OPTROM_filename[numOPT_ROMS][0],sizeof(OPTROM_filename[numOPT_ROMS])); //Init filename!
			safestrcpy(OPTROM_filename[numOPT_ROMS],sizeof(OPTROM_filename[0]),filename); //Save the filename of the loaded ROM for writing to it, as well as releasing it!

			location += OPTROM_size[numOPT_ROMS]; //Next ROM position!
			OPTROM_location[numOPT_ROMS] |= ((uint_64)location<<32); //The end location of the option ROM!
			if (OPTROM_size[numOPT_ROMS]&0x7FF) //Not 2KB alligned?
			{
				location += 0x800-(OPTROM_size[numOPT_ROMS]&0x7FF); //2KB align!
			}
			++numOPT_ROMS; //We've loaded this many ROMS!
			if ((location+0xC0000) > BIOSROM_BASE_XT) //More ROMs loaded than we can handle?
			{
				--numOPT_ROMS; //Unused option ROM!
				location = (OPTROM_location[numOPT_ROMS]&0xFFFFFFFFU); //Reverse to start next ROM(s) at said location again!
				unlock(LOCK_CPU);
				freez((void **)&OPT_ROMS[numOPT_ROMS],OPTROM_size[numOPT_ROMS], filename); //Release the ROM!
				lock(LOCK_CPU);
			}
			continue; //Loaded!
		}
		
		emufclose64(f);
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
			char filename[256];
			memset(&filename,0,sizeof(filename)); //Clear/init!
			safestrcpy(filename,sizeof(filename),OPTROM_filename[i]); //Set the filename from the loaded ROM!
			freez((void **)&OPT_ROMS[i],OPTROM_size[i],filename); //Release the OPT ROM!
		}
	}
}

byte BIOS_ROM_type = 0;
uint_32 BIOS_ROM_U13_15_double = 0, BIOS_ROM_U13_15_single = 0;

#define BIOSROMTYPE_INVALID 0
#define BIOSROMTYPE_U18_19 1
#define BIOSROMTYPE_U34_35 2
#define BIOSROMTYPE_U27_47 3
#define BIOSROMTYPE_U13_15 4

int BIOS_load_ROM(byte nr)
{
	uint_32 ROMdst, ROMsrc;
	byte srcROM;
	byte tryext = 0; //Try extra ROMs for different architectures?
	uint_32 ROM_size=0; //The size of both ROMs!
	BIGFILE *f;
	char filename[100];
	memset(&filename,0,sizeof(filename)); //Clear/init!
retryext:
	if ((tryext&7)==0) //Extension ROM available?
	{
		if (EMULATED_CPU<CPU_80386) //Unusable CPU for 32-bit code?
		{
			++tryext; //Next try!
			goto retryext; //Skip 32-bit ROMs!
		}
	}
	switch (tryext&7)
	{
	case 0: //PS/2?
		if (is_PS2 == 0) //Not a PS/2 compatible architecture?
		{
			++tryext; //Next try!
			goto retryext; //Skip PS/2 ROMs!
		}
		if (BIOS_Settings.BIOSROMmode == BIOSROMMODE_DIAGNOSTICS) //Diagnostics mode?
		{
			snprintf(filename, sizeof(filename), "%s/BIOSROM.PS2.U%u.DIAGNOSTICS.BIN", ROMpath, nr); //Create the filename for the ROM!		
		}
		else //Normal mode?
		{
			snprintf(filename, sizeof(filename), "%s/BIOSROM.PS2.U%u.BIN", ROMpath, nr); //Create the filename for the ROM!		
		}
		break;
	case 1: //32-bit?
		if ((is_Compaq==0) && (is_PS2==0)) //Not a 32-bit compatible architecture?
		{
			++tryext; //Next try!
			goto retryext; //Skip 32-bit ROMs!
		}

		if (BIOS_Settings.BIOSROMmode == BIOSROMMODE_DIAGNOSTICS) //Diagnostics mode?
		{
			snprintf(filename, sizeof(filename), "%s/BIOSROM.32.U%u.DIAGNOSTICS.BIN", ROMpath, nr); //Create the filename for the ROM!		
		}
		else //Normal mode?
		{
			snprintf(filename, sizeof(filename), "%s/BIOSROM.32.U%u.BIN", ROMpath, nr); //Create the filename for the ROM!		
		}
		break;
	case 2: //Compaq?
		if (is_Compaq==0) //Not a Compaq compatible architecture?
		{
			++tryext; //Next try!
			goto retryext; //Skip 32-bit ROMs!
		}

		if (BIOS_Settings.BIOSROMmode == BIOSROMMODE_DIAGNOSTICS) //Diagnostics mode?
		{
			snprintf(filename, sizeof(filename), "%s/BIOSROM.COMPAQ.U%u.DIAGNOSTICS.BIN", ROMpath, nr); //Create the filename for the ROM!		
		}
		else //Normal mode?
		{
			snprintf(filename, sizeof(filename), "%s/BIOSROM.COMPAQ.U%u.BIN", ROMpath, nr); //Create the filename for the ROM!		
		}
		break;
	case 3: //AT?
		if (is_XT) //Not a AT compatible architecture?
		{
			++tryext; //Next try!
			goto retryext; //Skip PS/2 ROMs!
		}
		if (BIOS_Settings.BIOSROMmode == BIOSROMMODE_DIAGNOSTICS) //Diagnostics mode?
		{
				snprintf(filename, sizeof(filename), "%s/BIOSROM.AT.U%u.DIAGNOSTICS.BIN", ROMpath, nr); //Create the filename for the ROM!
		}
		else
		{
			snprintf(filename, sizeof(filename), "%s/BIOSROM.AT.U%u.BIN", ROMpath, nr); //Create the filename for the ROM!
		}
		break;
	case 4: //XT?
		if (BIOS_Settings.BIOSROMmode == BIOSROMMODE_DIAGNOSTICS) //Diagnostics mode?
		{
				snprintf(filename, sizeof(filename), "%s/BIOSROM.XT.U%u.DIAGNOSTICS.BIN", ROMpath, nr); //Create the filename for the ROM!
		}
		else
		{
			snprintf(filename, sizeof(filename), "%s/BIOSROM.XT.U%u.BIN", ROMpath, nr); //Create the filename for the ROM!
		}
		break;
	default:
	case 5: //Universal ROM?
		if (BIOS_Settings.BIOSROMmode == BIOSROMMODE_DIAGNOSTICS) //Diagnostics mode?
		{
				snprintf(filename, sizeof(filename), "%s/BIOSROM.U%u.DIAGNOSTICS.BIN", ROMpath, nr); //Create the filename for the ROM!
		}
		else
		{
			snprintf(filename, sizeof(filename), "%s/BIOSROM.U%u.BIN", ROMpath, nr); //Create the filename for the ROM!
		}
		break;
	}
	f = emufopen64(filename,"rb");
	if (!f)
	{
		++tryext; //Try second time and onwards!
		if (tryext<=5) //Extension try valid to be tried?
		{
			goto retryext;
		}
		return 0; //Failed to load!
	}
	emufseek64(f,0,SEEK_END); //Goto EOF!
	if (emuftell64(f)) //Gotten size?
 	{
		BIOS_ROM_size[nr] = (uint_32)emuftell64(f); //Save the size!
		emufseek64(f,0,SEEK_SET); //Goto BOF!
		BIOS_ROMS[nr] = (byte *)nzalloc(BIOS_ROM_size[nr],filename, getLock(LOCK_CPU)); //Simple memory allocation for our ROM!
		if (!BIOS_ROMS[nr]) //Failed to allocate?
		{
			emufclose64(f); //Close the file!
			return 0; //Failed to allocate!
		}
		if (emufread64(BIOS_ROMS[nr],1,BIOS_ROM_size[nr],f)!=BIOS_ROM_size[nr]) //Not fully read?
		{
			freez((void **)&BIOS_ROMS[nr],BIOS_ROM_size[nr],filename); //Failed to read!
			emufclose64(f); //Close the file!
			return 0; //Failed to read!
		}
		emufclose64(f); //Close the file!

		BIOS_ROMS_ext[nr] = ((BIOS_Settings.BIOSROMmode==BIOSROMMODE_DIAGNOSTICS)?4:0)|(tryext&3); //Extension enabled?

		switch (nr) //What ROM has been loaded?
		{
			case 18:
			case 19: //u18/u19 chips?
				if (BIOS_ROMS[18] && BIOS_ROMS[19]) //Both loaded?
				{
					BIOS_ROM_type = BIOSROMTYPE_U18_19; //u18/19 combo!
					ROM_size = BIOS_ROM_size[18]+BIOS_ROM_size[19]; //ROM size!
				}
				else
				{
					BIOS_ROM_type = BIOSROMTYPE_INVALID; //Invalid!
					ROM_size = 0; //No ROM!
				}
				break;
			case 34:
			case 35: //u34/u35 chips?
				if (BIOS_ROMS[34] && BIOS_ROMS[35]) //Both loaded?
				{
					BIOS_ROM_type = BIOSROMTYPE_U34_35; //u34/35 combo!
					ROM_size = BIOS_ROM_size[34]+BIOS_ROM_size[35]; //ROM size!
					//Preprocess the ROM into a linear one instead of interleaved!
					BIOS_combinedROM = (byte*)zalloc(ROM_size, "BIOS_combinedROM", getLock(LOCK_CPU));
					if (!BIOS_combinedROM) //Failed to allocate?
					{
						freez((void**)&BIOS_ROMS[nr], BIOS_ROM_size[nr], filename); //Failed to read!
						return 0; //Abort!
					}
					BIOS_ROMS_ext[nr] |= 0x10; //Tell we're using an combined ROM!
					BIOS_combinedROM_size = ROM_size; //The size of the combined ROM!
					//Combined ROM allocated?
					srcROM = 34; //The first byte is this ROM!
					ROMdst = ROMsrc = 0; //Init!
					for (; ROMdst < ROM_size;) //Process the entire ROM!
					{
						BIOS_combinedROM[ROMdst++] = BIOS_ROMS[srcROM][ROMsrc]; //Take a byte from the source ROM!
						srcROM = (srcROM == 34) ? 35 : 34; //Toggle the src ROM!
						if (srcROM == 34) ++ROMsrc; //Next position when two ROMs have been processed!
					}
				}
				else
				{
					BIOS_ROM_type = BIOSROMTYPE_INVALID; //Invalid!
					ROM_size = 0; //No ROM!
				}
				break;
			case 27:
			case 47: //u27/u47 chips?
				if (BIOS_ROMS[27] && BIOS_ROMS[47]) //Both loaded?
				{
					BIOS_ROM_type = BIOSROMTYPE_U27_47; //u27/47 combo!
					ROM_size = BIOS_ROM_size[27]+BIOS_ROM_size[47]; //ROM size!
					//Preprocess the ROM into a linear one instead of interleaved!
					BIOS_combinedROM = (byte*)zalloc(ROM_size, "BIOS_combinedROM", getLock(LOCK_CPU));
					if (!BIOS_combinedROM) //Failed to allocate?
					{
						freez((void**)&BIOS_ROMS[nr], BIOS_ROM_size[nr], filename); //Failed to read!
						return 0; //Abort!
					}
					BIOS_ROMS_ext[nr] |= 0x10; //Tell we're using an combined ROM!
					BIOS_combinedROM_size = ROM_size; //The size of the combined ROM!
					//Combined ROM allocated?
					srcROM = 27; //The first byte is this ROM!
					ROMdst = ROMsrc = 0; //Init!
					for (; ROMdst < ROM_size;) //Process the entire ROM!
					{
						BIOS_combinedROM[ROMdst++] = BIOS_ROMS[srcROM][ROMsrc]; //Take a byte from the source ROM!
						srcROM = (srcROM == 27) ? 47 : 27; //Toggle the src ROM!
						if (srcROM == 27) ++ROMsrc; //Next position when two ROMs have been processed!
					}
				}
				else
				{
					BIOS_ROM_type = BIOSROMTYPE_INVALID; //Invalid!
					ROM_size = 0; //No ROM!
				}
				break;
			case 13:
			case 15: //u13/u15 chips?
				if (BIOS_ROMS[13] && BIOS_ROMS[15]) //Both loaded?
				{
					BIOS_ROM_type = BIOSROMTYPE_U13_15; //u13/15 combo!
					ROM_size = (BIOS_ROM_size[13]+BIOS_ROM_size[15])<<1; //ROM size! The ROM is doubled in RAM(duplicated twice)
					BIOS_ROM_U13_15_double = ROM_size; //Save the loaded ROM size for easier processing!
					BIOS_ROM_U13_15_single = ROM_size>>1; //Half the ROM for easier lookup!
					//Preprocess the ROM into a linear one instead of interleaved!
					BIOS_combinedROM = (byte*)zalloc(BIOS_ROM_U13_15_single, "BIOS_combinedROM", getLock(LOCK_CPU));
					if (!BIOS_combinedROM) //Failed to allocate?
					{
						freez((void**)&BIOS_ROMS[nr], BIOS_ROM_size[nr], filename); //Failed to read!
						return 0; //Abort!
					}
					BIOS_ROMS_ext[nr] |= 0x10; //Tell we're using an combined ROM!
					BIOS_combinedROM_size = BIOS_ROM_U13_15_single; //The size of the combined ROM!
					//Combined ROM allocated?
					srcROM = 13; //The first byte is this ROM!
					ROMdst = ROMsrc = 0;
					for (; ROMdst < BIOS_combinedROM_size;) //Process the entire ROM!
					{
						BIOS_combinedROM[ROMdst++] = BIOS_ROMS[srcROM][ROMsrc]; //Take a byte from the source ROM!
						srcROM = (srcROM == 13) ? 15 : 13; //Toggle the src ROM!
						if (srcROM == 13) ++ROMsrc; //Next position when two ROMs have been processed!
					}
				}
				else
				{
					BIOS_ROM_type = BIOSROMTYPE_INVALID; //Invalid!
					ROM_size = 0; //No ROM!
					BIOS_ROM_U13_15_double = 0; //Save the loaded ROM size for easier processing!
					BIOS_ROM_U13_15_single = 0; //Half the ROM for easier lookup!
				}
				break;
			default:
				break;
		}
		
		//Recalculate based on ROM size!
		BIOSROM_BASE_AT = 0xFFFFFFU-(MIN(ROM_size,0x100000U)-1U); //AT ROM size! Limit to 1MB!
		BIOSROM_BASE_XT = 0xFFFFFU-(MIN(ROM_size,(is_XT?0x10000U:(is_Compaq?0x40000U:0x20000U)))-1U); //XT ROM size! Limit to 256KB(Compaq), 128KB(AT) or 64KB(XT)!
		BIOSROM_BASE_Modern = 0xFFFFFFFFU-(ROM_size-1U); //Modern ROM size!
		return 1; //Loaded!
	}
	
	emufclose64(f);
	return 0; //Failed to load!
}

//Custom loaded BIOS ROM (single only)!
byte *BIOS_custom_ROM;
uint_32 BIOS_custom_ROM_size;
char customROMname[256]; //Custom ROM name!
byte ROM_doubling = 0; //Double the ROM?

int BIOS_load_custom(char *path, char *rom)
{
	BIGFILE *f;
	char filename[256];
	memset(&filename,0,sizeof(filename)); //Clear/init!
	if (!path)
	{
		safestrcpy(filename,sizeof(filename),ROMpath); //Where to find our ROM!
	}
	else
	{
		safestrcpy(filename,sizeof(filename), path); //Where to find our ROM!
	}
	if (strcmp(filename, "") != 0) safestrcat(filename,sizeof(filename), "/"); //Only a seperator when not empty!
	safestrcat(filename,sizeof(filename),rom); //Create the filename for the ROM!
	f = emufopen64(filename,"rb");
	if (!f)
	{
		return 0; //Failed to load!
	}
	emufseek64(f,0,SEEK_END); //Goto EOF!
	if (emuftell64(f)) //Gotten size?
 	{
		BIOS_custom_ROM_size = (uint_32)emuftell64(f); //Save the size!
		emufseek64(f,0,SEEK_SET); //Goto BOF!
		BIOS_custom_ROM = (byte *)nzalloc(BIOS_custom_ROM_size,filename, getLock(LOCK_CPU)); //Simple memory allocation for our ROM!
		if (!BIOS_custom_ROM) //Failed to allocate?
		{
			emufclose64(f); //Close the file!
			return 0; //Failed to allocate!
		}
		if (emufread64(BIOS_custom_ROM,1,BIOS_custom_ROM_size,f)!=BIOS_custom_ROM_size) //Not fully read?
		{
			freez((void **)&BIOS_custom_ROM,BIOS_custom_ROM_size,filename); //Failed to read!
			emufclose64(f); //Close the file!
			return 0; //Failed to read!
		}
		emufclose64(f); //Close the file!
		safestrcpy(customROMname,sizeof(customROMname),filename); //Custom ROM name for easy dealloc!
		//Update the base address to use for this CPU!
		ROM_doubling = 0; //Default: no ROM doubling!
		if (BIOS_custom_ROM_size<=0x8000) //Safe to double?
		{
			if (EMULATED_CPU>=CPU_80386 && (is_XT==0)) //We're to emulate a Compaq Deskpro 386?
			{
				ROM_doubling = 1; //Double the ROM!
			}
		}

		//Also limit the ROM base addresses accordingly(only last block).
		BIOSROM_BASE_AT = 0xFFFFFF-(MIN(BIOS_custom_ROM_size<<ROM_doubling,0x100000)-1); //AT ROM size!
		BIOSROM_BASE_XT = 0xFFFFF-(MIN(BIOS_custom_ROM_size<<ROM_doubling,(is_XT?0x10000U:(is_Compaq?0x40000U:0x20000U)))-1U); //XT ROM size! XT has a 64K limit(0xF0000 min) because of the EMS mapped at 0xE0000(64K), while AT and up has 128K limit(0xE0000) because the memory is unused(no expansion board present, allowing all addresses to be used up to the end of the expansion ROM area(0xE0000). Compaq and up limits to 256KB instead(addresses from 0xC0000 and up)).
		BIOSROM_BASE_Modern = 0xFFFFFFFF-(MIN(BIOS_custom_ROM_size<<ROM_doubling,0x10000000)-1); //Modern ROM size!
		return 1; //Loaded!
	}
	
	emufclose64(f);
	return 0; //Failed to load!
}


void BIOS_free_ROM(byte nr)
{
	char filename[100];
	memset(&filename,0,sizeof(filename)); //Clear/init!
	switch (BIOS_ROMS_ext[nr]&7) //ROM type?
	{
	case 0: //PS/2 ROM?
			if (BIOS_ROMS_ext[nr] & 8) //Diagnostic ROM?
			{
				snprintf(filename, sizeof(filename), "BIOSROM.PS2.U%u.DIAGNOSTICS.BIN", nr); //Create the filename for the ROM!
			}
			else //Normal ROM?
			{
				snprintf(filename, sizeof(filename), "BIOSROM.PS2.U%u.BIN", nr); //Create the filename for the ROM!
			}
			break;
	case 1: //32-bit ROM?
			if (BIOS_ROMS_ext[nr] & 8) //Diagnostic ROM?
			{
				snprintf(filename, sizeof(filename), "BIOSROM.32.U%u.DIAGNOSTICS.BIN", nr); //Create the filename for the ROM!
			}
			else //Normal ROM?
			{
				snprintf(filename, sizeof(filename), "BIOSROM.32.U%u.BIN", nr); //Create the filename for the ROM!
			}
			break;
	case 2: //Compaq ROM?
			if (BIOS_ROMS_ext[nr] & 8) //Diagnostic ROM?
			{
				snprintf(filename, sizeof(filename), "BIOSROM.COMPAQ.U%u.DIAGNOSTICS.BIN", nr); //Create the filename for the ROM!
			}
			else //Normal ROM?
			{
				snprintf(filename, sizeof(filename), "BIOSROM.COMPAQ.U%u.BIN", nr); //Create the filename for the ROM!
			}
			break;
	case 3: //AT ROM?
			if (BIOS_ROMS_ext[nr]&8) //Diagnostic ROM?
			{
				snprintf(filename,sizeof(filename),"BIOSROM.AT.U%u.DIAGNOSTICS.BIN",nr); //Create the filename for the ROM!
			}
			else //Normal ROM?
			{
				snprintf(filename,sizeof(filename),"BIOSROM.AT.U%u.BIN",nr); //Create the filename for the ROM!
			}
	case 4: //XT ROM?
			if (BIOS_ROMS_ext[nr]&8) //Diagnostic ROM?
			{
				snprintf(filename,sizeof(filename),"BIOSROM.XT.U%u.DIAGNOSTICS.BIN",nr); //Create the filename for the ROM!
			}
			else //Normal ROM?
			{
				snprintf(filename,sizeof(filename),"BIOSROM.XT.U%u.BIN",nr); //Create the filename for the ROM!
			}
			break;
	default:
	case 5: //Universal ROM?
			if (BIOS_ROMS_ext[nr]&8) //Diagnostic ROM?
			{
				snprintf(filename,sizeof(filename),"BIOSROM.U%u.DIAGNOSTICS.BIN",nr); //Create the filename for the ROM!
			}
			else //Normal ROM?
			{
				snprintf(filename,sizeof(filename),"BIOSROM.U%u.BIN",nr); //Create the filename for the ROM!
			}
			break;
	}
	if (BIOS_ROM_size[nr]) //Has size?
	{
		if (BIOS_ROMS_ext[nr] & 0x10) //Needs freeing of the combined ROM as well?
		{
			freez((void **)&BIOS_combinedROM,BIOS_combinedROM_size,"BIOS_combinedROM"); //Free the combined ROM!
		}
		freez((void **)&BIOS_ROMS[nr],BIOS_ROM_size[nr],filename); //Release the BIOS ROM!
	}
}

void BIOS_free_custom(char *rom)
{
	char filename[256];
	memset(&filename,0,sizeof(filename)); //Clear/init!
	if (rom==NULL) //NULL ROM (Autodetect)?
	{
		rom = &customROMname[0]; //Use custom ROM name!
	}
	safestrcpy(filename,sizeof(filename),rom); //Create the filename for the ROM!
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
		safestrcpy(path,sizeof(path),ROMpath); //Current ROM path!
		safestrcat(path,sizeof(path),"/SYSROM.DMP.BIN"); //Dump path!
		//Dump our own BIOS ROM!
		BIGFILE *f;
		f = emufopen64(path, "wb");
		emufwrite64(&EMU_BIOS, 1, sizeof(EMU_BIOS), f); //Save our BIOS!
		emufclose64(f);
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

extern uint_32 memory_dataread;
extern byte memory_datasize; //The size of the data that has been read!
byte OPTROM_readhandler(uint_32 offset, byte index)    /* A pointer to a handler function */
{
	uint_32 ROMsize;
	INLINEREGISTER uint_64 basepos, currentpos, temppos; //Current position!
	basepos = currentpos = offset; //Load the offset!
	if (unlikely((basepos >= 0xC0000) && (basepos<0xF0000))) basepos = 0xC0000; //Our base reference position!
	else //Out of range (16-bit)?
	{
		if (unlikely((basepos >= 0xC0000000) && (basepos < 0xF0000000))) basepos = 0xC0000000; //Our base reference position!
		else return 0; //Our of range (32-bit)?
	}
	currentpos -= basepos; //Calculate from the base position!
	if (unlikely((offset>=0xE0000) && (offset<=0xFFFFF) && (BIOSROM_DisableLowMemory))) return 0; //Disabled for Compaq RAM!
	basepos = currentpos; //Save a backup!
	INLINEREGISTER byte i=0,j=numOPT_ROMS;
	if (unlikely(!numOPT_ROMS)) goto noOPTROMSR;
	do //Check OPT ROMS!
	{
		currentpos = OPTROM_location[i]; //Load the current location for analysis and usage!
		ROMsize = (currentpos>>32); //Save ROM end location!
		if (likely(OPT_ROMS[i] && (ROMsize>basepos))) //Before the end location and valid rom?
		{
			currentpos &= 0xFFFFFFFF; //The location of the ROM itself!
			ROMsize -= (uint_32)currentpos; //Convert ROMsize to the actual ROM size to use!
			if (likely(currentpos <= basepos)) //At/after the start location? We've found the ROM!
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
					if (likely((index & 3) == 0))
					{
						if (likely(((ROMsize - temppos - 1) >= 3) && (((ROMsize - temppos - 4) & 3) == 0))) //Enough to read a dword?
						{
							memory_dataread = SDL_SwapLE32(*((uint_32*)(&OPT_ROMS[i][ROMsize - temppos - 4]))); //Read the data from the ROM, reversed!
							memory_datasize = 4; //A whole dword!
							return 1; //Done: we've been read!
						}
						else if (likely(((ROMsize - temppos - 1) >= 1) && (((ROMsize - temppos - 2) & 1) == 0))) //Enough to read a word, aligned?
						{
							memory_dataread = SDL_SwapLE16(*((word*)(&OPT_ROMS[i][ROMsize - temppos - 2]))); //Read the data from the ROM, reversed!
							memory_datasize = 2; //Only 2 bytes!
							return 1; //Done: we've been read!				
						}
						else //Enough to read a byte only?
						{
							memory_dataread = OPT_ROMS[i][ROMsize - temppos - 1]; //Read the data from the ROM, reversed!
							memory_datasize = 1; //Only 1 byte!
							return 1; //Done: we've been read!				
						}
					}
					else //Enough to read a byte only?
					{
						memory_dataread = OPT_ROMS[i][ROMsize - temppos - 1]; //Read the data from the ROM, reversed!
						memory_datasize = 1; //Only 1 byte!
						return 1; //Done: we've been read!				
					}
				}
				if (likely((index & 3) == 0))
				{
					if (likely(((temppos & 3) == 0) && ((temppos | 3) <= ROMsize))) //Enough to read a dword?
					{
						memory_dataread = SDL_SwapLE32(*((uint_32*)(&OPT_ROMS[i][temppos]))); //Read the data from the ROM!
						memory_datasize = 4; //A whole dword!
						return 1; //Done: we've been read!
					}
					else if (likely(((temppos & 1) == 0) && ((temppos | 1) <= ROMsize))) //Enough to read a word, aligned?
					{
						memory_dataread = SDL_SwapLE16(*((word*)(&OPT_ROMS[i][temppos]))); //Read the data from the ROM, reversed!
						memory_datasize = 2; //Only 2 bytes!
						return 1; //Done: we've been read!				
					}
					else //Enough to read a byte only?
					{
						memory_dataread = OPT_ROMS[i][temppos]; //Read the data from the ROM, reversed!
						memory_datasize = 1; //Only 1 byte!
						return 1; //Done: we've been read!				
					}
				}
				else //Enough to read a byte only?
				{
					memory_dataread = OPT_ROMS[i][temppos]; //Read the data from the ROM, reversed!
					memory_datasize = 1; //Only 1 byte!
					return 1; //Done: we've been read!				
				}
			}
		}
		++i;
	} while (--j);
	noOPTROMSR:
	if (BIOS_custom_VGAROM_size) //Custom VGA ROM mounted?
	{
		if (likely(basepos < BIOS_custom_VGAROM_size)) //OK?
		{
			if (likely((index & 3) == 0))
			{
				if (likely(((basepos & 3) == 0) && ((basepos | 3) <= BIOS_custom_VGAROM_size))) //Enough to read a dword?
				{
					memory_dataread = SDL_SwapLE32(*((uint_32*)(&BIOS_custom_VGAROM[basepos]))); //Read the data from the ROM!
					memory_datasize = 4; //A whole dword!
					return 1; //Done: we've been read!
				}
				else if (likely(((basepos & 1) == 0) && ((basepos | 1) <= BIOS_custom_VGAROM_size))) //Enough to read a word, aligned?
				{
					memory_dataread = SDL_SwapLE16(*((word*)(&BIOS_custom_VGAROM[basepos]))); //Read the data from the ROM, reversed!
					memory_datasize = 2; //Only 2 bytes!
					return 1; //Done: we've been read!				
				}
				else //Enough to read a byte only?
				{
					memory_dataread = BIOS_custom_VGAROM[basepos]; //Read the data from the ROM, reversed!
					memory_datasize = 1; //Only 1 byte!
					return 1; //Done: we've been read!				
				}
			}
			else //Enough to read a byte only?
			{
				memory_dataread = BIOS_custom_VGAROM[basepos]; //Read the data from the ROM, reversed!
				memory_datasize = 1; //Only 1 byte!
				return 1; //Done: we've been read!				
			}
			return 1;
		}
	}
	return 0; //No ROM here, allow read from nroaml memory!
}

void BIOSROM_updateTimers(DOUBLE timepassed)
{
	byte i, timersleft;
	if (unlikely(OPTROM_timeoutused))
	{
		timersleft = 0; //Default: finished!
		for (i=0;i<numOPT_ROMS;++i)
		{
			if (unlikely(OPTROM_writetimeout[i])) //Timing?
			{
				OPTROM_writetimeout[i] -= timepassed; //Time passed!
				if (unlikely(OPTROM_writetimeout[i]<=0.0)) //Expired?
				{
					OPTROM_writetimeout[i] = (DOUBLE)0; //Finish state!
					OPTROM_writeenabled[i] = 0; //Disable writes!
				}
				else timersleft = 1; //Still running?
			}
		}
		if (timersleft==0) OPTROM_timeoutused = 0; //Finished all timers!
	}
}


extern uint_32 BIU_cachedmemoryaddr;
extern uint_32 BIU_cachedmemoryread;
extern byte BIU_cachedmemorysize; //To invalidate the BIU cache!
byte OPTROM_writehandler(uint_32 offset, byte value)    /* A pointer to a handler function */
{
	INLINEREGISTER uint_32 basepos, currentpos;
	basepos = currentpos = offset; //Load the offset!
	if (unlikely((basepos>=0xC0000) && (basepos<0xF0000))) basepos = 0xC0000; //Our base reference position!
	else //Out of range (16-bit)?
	{
		if (unlikely((basepos>=0xC0000000) && (basepos<0xF0000000))) basepos = 0xC0000000; //Our base reference position!
		else return 0; //Our of range (32-bit)?
	}
	currentpos -= basepos; //Calculate from the base position!
	if (unlikely((offset>=0xE0000) && (offset<=0xFFFFF) && (BIOSROM_DisableLowMemory))) return 0; //Disabled for Compaq RAM!
	basepos = currentpos; //Write back!
	INLINEREGISTER uint_64 OPTROM_address, OPTROM_loc; //The address calculated in the EEPROM!
	INLINEREGISTER byte i=0,j=numOPT_ROMS;
	if (unlikely(!numOPT_ROMS)) goto noOPTROMSW;
	do //Check OPT ROMS!
	{
		if (likely(OPT_ROMS[i])) //Enabled?
		{
			OPTROM_loc = OPTROM_location[i]; //Load the current location!
			if (likely((OPTROM_loc>>32)>basepos)) //Before the end of the ROM?
			{
				OPTROM_loc &= 0xFFFFFFFF;
				if (likely(OPTROM_loc <= basepos)) //After the start of the ROM?
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
					byte OPTROM_inhabitwrite = 0; //Are we to inhabit the current write(pending buffered)?
					switch (OPTROM_address)
					{
					case 0x1555:
						if ((value == 0xAA) && !OPTROM_writeSequence[i]) //Start sequence!
						{
							OPTROM_writeSequence[i] = 1; //Next step!
							OPTROM_pendingAA_1555[i] = 1; //We're pending to write!
							OPTROM_inhabitwrite = 1; //We're inhabiting the write!
						}
						else if (OPTROM_writeSequence[i] == 2) //We're a command byte!
						{
							switch (value)
							{
							case 0xA0: //Enable write protect!
								OPTROM_writeSequence_waitingforDisable[i] = 0; //Not waiting anymore!
								OPTROM_writeSequence[i] = 0; //Finished write sequence!
								#ifdef IS_LONGDOUBLE
								OPTROM_writetimeout[i] = 10000000.0L; //We're disabling writes to the EEPROM 10ms after this write, the same applies to the following writes!
								#else
								OPTROM_writetimeout[i] = 10000000.0; //We're disabling writes to the EEPROM 10ms after this write, the same applies to the following writes!
								#endif
								OPTROM_timeoutused = 1; //Timing!
								OPTROM_pending55_0AAA[i] = OPTROM_pendingAA_1555[i] = 0; //Not pending anymore!
								OPTROM_inhabitwrite = 1; //We're preventing us from writing!
								break;
							case 0x80: //Wait for 0x20 to disable write protect!
								OPTROM_writeSequence_waitingforDisable[i] = 1; //Waiting for disable!
								OPTROM_writeSequence[i] = 0; //Finished write sequence!
								OPTROM_inhabitwrite = 1; //We're preventing us from writing!
								OPTROM_pendingAA_1555[i] = OPTROM_pending55_0AAA[i] = 0; //Not pending anymore!
								break;
							case 0x20: //Disable write protect!
								if (OPTROM_writeSequence_waitingforDisable[i]) //Waiting for disable?
								{
									OPTROM_writeenabled[i] = OPTROM_writeenabled[i]?1:2; //We're enabling writes to the EEPROM now/before next write!
									OPTROM_pending55_0AAA[i] = OPTROM_pendingAA_1555[i] = 0; //Not pending anymore!
									OPTROM_inhabitwrite = (OPTROM_writeenabled[i]==1)?1:0; //We're preventing us from writing!
									OPTROM_writeSequence_waitingforDisable[i] = 0; //Not waiting anymore!
									OPTROM_writeSequence[i] =  0; //Reset the sequence!
								}
								else
								{
									OPTROM_writeSequence_waitingforDisable[i] = 0; //Not waiting anymore!
									OPTROM_writeSequence[i] = 0; //Finished write sequence!
									OPTROM_pendingAA_1555[i] = 0;
								}
								break;
							default: //Not a command!
								OPTROM_writeSequence_waitingforDisable[i] = 0; //Not waiting anymore!
								OPTROM_writeSequence[i] = 0; //Finished write sequence!
								OPTROM_pendingAA_1555[i] = 0;
								break;
							}
						}
						else
						{
							OPTROM_writeSequence_waitingforDisable[i] = 0; //Not waiting anymore!
							OPTROM_writeSequence[i] = 0; //Finished write sequence!
							OPTROM_pendingAA_1555[i] = 0; //Not pending anymore!
						}
						break;
					case 0x0AAA:
						if ((value == 0x55) && (OPTROM_writeSequence[i] == 1)) //Start of valid sequence which is command-specific?
						{
							OPTROM_writeSequence[i] = 2; //Start write command sequence!
							OPTROM_pending55_0AAA[i] = 1; //We're pending to write!
							OPTROM_inhabitwrite = 1; //We're inhabiting the write!
						}
						else
						{
							OPTROM_writeSequence_waitingforDisable[i] = 0; //Not waiting anymore!
							OPTROM_writeSequence[i] = 0; //Finished write sequence!
							OPTROM_pending55_0AAA[i] = 0; //Not pending anymore!
						}
						break;
					default: //Any other address!
						OPTROM_writeSequence_waitingforDisable[i] = 0; //Not waiting anymore!
						OPTROM_writeSequence[i] = 0; //No sequence running!
						break;
					}
					uint_32 originaladdress = (uint_32)OPTROM_address; //Save the address we're writing to!
					if ((!OPTROM_writeenabled[i]) || OPTROM_inhabitwrite) return 1; //Handled: ignore writes to ROM or protected ROM!
					else if (OPTROM_writeenabled[i]==2)
					{
						OPTROM_writeenabled[i] = 1; //Start next write!
						return 1; //Disable this write, enable next write!
					}
					if (OPTROM_writetimeout[i]) //Timing?
					{
						#ifdef IS_LONGDOUBLE
						OPTROM_writetimeout[i] = 10000000.0L; //Reset timer!
						#else
						OPTROM_writetimeout[i] = 10000000.0; //Reset timer!
						#endif
						OPTROM_timeoutused = 1; //Timing!
					}
					processPendingWrites:
					if ((ISVGA==4) && (i==0)) //EGA ROM is reversed?
					{
						OPTROM_address = ((OPTROM_location[i]>>32)-OPTROM_address)-1; //The ROM is reversed, so reverse write too!
					}
					//We're a EEPROM with write protect disabled!
					BIGFILE *f; //For opening the ROM file!
					f = emufopen64(OPTROM_filename[i], "rb+"); //Open the ROM for writing!
					if (!f) return 1; //Couldn't open the ROM for writing!
					if (emufseek64(f, (uint_32)OPTROM_address, SEEK_SET)) //Couldn't seek?
					{
						emufclose64(f); //Close the file!
						return 1; //Abort!
					}
					if (emuftell64(f) != OPTROM_address) //Failed seek position?
					{
						emufclose64(f); //Close the file!
						return 1; //Abort!
					}
					if (emufwrite64(&value, 1, 1, f) != 1) //Failed to write the data to the file?
					{
						emufclose64(f); //Close thefile!
						return 1; //Abort!
					}
					emufclose64(f); //Close the file!
					OPT_ROMS[i][OPTROM_address] = value; //Write the data to the ROM in memory!
					if (unlikely(BIU_cachedmemorysize && (BIU_cachedmemoryaddr <= offset) && ((BIU_cachedmemoryaddr + BIU_cachedmemorysize) > offset))) //Matched an active read cache(allowing self-modifying code)?
					{
						memory_datasize = 0; //Only 1 byte invalidated!
						BIU_cachedmemorysize = 0; //Make sure that the BIU has an updated copy of this in it's cache!
					}
					if (OPTROM_pending55_0AAA[i] && ((OPTROM_location[i]>>32)>0x0AAA)) //Pending write and within ROM range?
					{
						OPTROM_pending55_0AAA[i] = 0; //Not pending anymore, processing now!
						value = 0x55; //We're writing this anyway!
						OPTROM_address = 0x0AAA; //The address to write to!
						if (originaladdress!=0x0AAA) goto processPendingWrites; //Process the pending write!
					}
					if (OPTROM_pendingAA_1555[i] && ((OPTROM_location[i]>>32)>0x1555)) //Pending write and within ROM range?
					{
						OPTROM_pendingAA_1555[i] = 0; //Not pending anymore, processing now!
						value = 0xAA; //We're writing this anyway!
						OPTROM_address = 0x1555; //The address to write to!
						if (originaladdress!=0x1555) goto processPendingWrites; //Process the pending write!
					}
					return 1; //Ignore writes to memory: we've handled it!
				}
			}
		}
		++i;
	} while (--j);
	noOPTROMSW:
	if (BIOS_custom_VGAROM_size) //Custom VGA ROM mounted?
	{
		if (likely(basepos < BIOS_custom_VGAROM_size)) //OK?
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
		if (unlikely(basepos < 0x100000)) basepos = BIOSROM_BASE_XT; //Our base reference position(low memory)!
		else if (unlikely((basepos >= BIOSROM_BASE_Modern) && (EMULATED_CPU >= CPU_80386))) basepos = BIOSROM_BASE_Modern; //Our base reference position(high memory 386+)!
		else if (unlikely((basepos >= BIOSROM_BASE_AT) && (EMULATED_CPU == CPU_80286) && (basepos < 0x1000000))) basepos = BIOSROM_BASE_AT; //Our base reference position(high memmory 286)
		else return OPTROM_writehandler(offset, value); //OPTROM? Out of range (32-bit)?
	}
	else return OPTROM_writehandler(offset, value); //Our of range (32-bit)?

	tempoffset -= basepos; //Calculate from the base position!
	if ((offset>=0xE0000) && (offset<=0xFFFFF) && (BIOSROM_DisableLowMemory)) return 0; //Disabled for Compaq RAM!
	basepos = tempoffset; //Save for easy reference!

	if (unlikely(BIOS_custom_ROM)) //Custom/system ROM loaded?
	{
		if (likely(BIOS_custom_ROM_size == 0x10000))
		{
			if (likely(tempoffset<0x10000)) //Within range?
			{
				tempoffset &= 0xFFFF; //16-bit ROM!
				return 1; //Ignore writes!
			}
		}
		if ((EMULATED_CPU>=CPU_80386) && (is_XT==0)) //Compaq compatible?
		{
			if (unlikely(tempoffset>=BIOS_custom_ROM_size)) //Doubled copy?
			{
				tempoffset -= BIOS_custom_ROM_size; //Double in memory!
			}
		}
		if (likely(tempoffset<BIOS_custom_ROM_size)) //Within range?
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
		segment = basepos; //Load the offset!
		switch (BIOS_ROM_type) //What type of ROM is loaded?
		{
		case BIOSROMTYPE_U18_19: //U18&19 combo?
			originaloffset = basepos; //Save the original offset for reference!
			if (unlikely(basepos>=0x10000)) return 0; //Not us!
			basepos &= 0x7FFF; //Our offset within the ROM!
			if (originaloffset&0x8000) //u18?
			{
				if (likely(BIOS_ROM_size[18]>basepos)) //Within range?
				{
					return 1; //Ignore writes!
				}
			}
			else //u19?
			{
				if (likely(BIOS_ROM_size[19]>basepos)) //Within range?
				{
					return 1; //Ignore writes!
				}
			}
			break;
		case BIOSROMTYPE_U13_15: //U13&15 combo?
			if (likely(tempoffset<BIOS_ROM_U13_15_double)) //This is doubled in ROM!
			{
				if (unlikely(tempoffset>=(BIOS_ROM_U13_15_double>>1))) //Second copy?
				{
					tempoffset -= BIOS_ROM_U13_15_single; //Patch to first block to address!
				}
			}
			tempoffset >>= 1; //The offset is at every 2 bytes of memory!
			segment &= 1; //Even=u27, Odd=u47
			if (segment) //u47/u35/u15?
			{
				if (likely(BIOS_ROM_size[15]>tempoffset)) //Within range?
				{
					return 1; //Ignore writes!
				}					
			}
			else //u13/u15 combination?
			{
				if (likely(BIOS_ROM_size[13]>tempoffset)) //Within range?
				{
					return 1; //Ignore writes!
				}
			}
			break;
		case BIOSROMTYPE_U34_35: //U34/35 combo?
			tempoffset >>= 1; //The offset is at every 2 bytes of memory!
			segment &= 1; //Even=u27, Odd=u47
			if (segment)
			{
				if (likely(BIOS_ROM_size[35]>tempoffset)) //Within range?
				{
					return 1; //Ignore writes!
				}
			}
			else //u34/u35 combination?
			{
				if (likely(BIOS_ROM_size[34]>tempoffset)) //Within range?
				{
					return 1; //Ignore writes!
				}
			}
			break;
		case BIOSROMTYPE_U27_47: //U27/47 combo?
			tempoffset >>= 1; //The offset is at every 2 bytes of memory!
			segment &= 1; //Even=u27, Odd=u47
			if (segment) //Normal AT BIOS ROM?
			{
				if (likely(BIOS_ROM_size[47]>tempoffset)) //Within range?
				{
					return 1; //Ignore writes!
				}
			}
			else //Loaded?
			{
				if (likely(BIOS_ROM_size[27]>tempoffset)) //Within range?
				{
					return 1; //Ignore writes!
				}
			}
			break;
		default: break; //Unknown even/odd mapping!
		}

	return OPTROM_writehandler(offset, value); //Not recognised, use normal RAM or option ROM!
}

byte BIOS_readhandler(uint_32 offset, byte index) /* A pointer to a handler function */
{
	INLINEREGISTER uint_32 basepos, tempoffset, baseposbackup;
	uint_64 endpos;
	basepos = tempoffset = offset;
	if (basepos>=BIOSROM_BASE_XT) //Inside 16-bit/32-bit range?
	{
		if (unlikely(basepos < 0x100000)) { basepos = BIOSROM_BASE_XT; endpos = 0x100000; } //Our base reference position(low memory)!
		else if (unlikely((basepos >= BIOSROM_BASE_Modern) && (EMULATED_CPU >= CPU_80386))) { basepos = BIOSROM_BASE_Modern; endpos = 0x100000000ULL; } //Our base reference position(high memory 386+)!
		else if (unlikely((basepos >= BIOSROM_BASE_AT) && (EMULATED_CPU == CPU_80286) && (basepos < 0x1000000))) { basepos = BIOSROM_BASE_AT; endpos = 0x1000000; } //Our base reference position(high memmory 286)
		else return OPTROM_readhandler(offset,index); //OPTROM or nothing? Out of range (32-bit)?
	}
	else return OPTROM_readhandler(offset,index); //OPTROM or nothing? Out of range (32-bit)?
	
	baseposbackup = basepos; //Store for end location reversal!
	tempoffset -= basepos; //Calculate from the base position!
	basepos = tempoffset; //Save for easy reference!
	if (unlikely(BIOS_custom_ROM)) //Custom/system ROM loaded?
	{
		if (BIOS_custom_ROM_size == 0x10000)
		{
			if (likely(tempoffset<0x10000)) //Within range?
			{
				tempoffset &= 0xFFFF; //16-bit ROM!
				if (likely((index & 3) == 0)) //First byte?
				{
					if (likely(((tempoffset & 3) == 0) && ((tempoffset | 3) <= BIOS_custom_ROM_size))) //Enough to read a dword?
					{
						memory_dataread = SDL_SwapLE32(*((uint_32*)(&BIOS_custom_ROM[tempoffset]))); //Read the data from the ROM!
						memory_datasize = 4; //A whole dword!
						return 1; //Done: we've been read!
					}
					else if (likely(((tempoffset & 1) == 0) && ((tempoffset | 1) <= BIOS_custom_ROM_size))) //Enough to read a word, aligned?
					{
						memory_dataread = SDL_SwapLE16(*((word*)(&BIOS_custom_ROM[tempoffset]))); //Read the data from the ROM, reversed!
						memory_datasize = 2; //Only 2 bytes!
						return 1; //Done: we've been read!				
					}
					else //Enough to read a byte only?
					{
						memory_dataread = BIOS_custom_ROM[tempoffset]; //Read the data from the ROM, reversed!
						memory_datasize = 1; //Only 1 byte!
						return 1; //Done: we've been read!				
					}
				}
				else //Enough to read a byte only?
				{
					memory_dataread = BIOS_custom_ROM[tempoffset]; //Read the data from the ROM, reversed!
					memory_datasize = 1; //Only 1 byte!
					return 1; //Done: we've been read!				
				}
			}
		}
		if ((EMULATED_CPU>=CPU_80386) && (is_XT==0)) //Compaq compatible?
		{
			if ((tempoffset<BIOS_custom_ROM_size) && ROM_doubling) //Doubled copy?
			{
				tempoffset += BIOS_custom_ROM_size; //Double in memory by patching to second block!
			}
		}
		tempoffset = (uint_32)(BIOS_custom_ROM_size-(endpos-(tempoffset+baseposbackup))); //Patch to the end block of the ROM instead of the start.
		if (likely(tempoffset<BIOS_custom_ROM_size)) //Within range?
		{
			if (likely((index & 3) == 0))
			{
				if (likely(((tempoffset & 3) == 0) && ((tempoffset | 3) <= BIOS_custom_ROM_size))) //Enough to read a dword?
				{
					memory_dataread = SDL_SwapLE32(*((uint_32*)(&BIOS_custom_ROM[tempoffset]))); //Read the data from the ROM!
					memory_datasize = 4; //A whole dword!
					return 1; //Done: we've been read!
				}
				else if (likely(((tempoffset & 1) == 0) && ((tempoffset | 1) <= BIOS_custom_ROM_size))) //Enough to read a word, aligned?
				{
					memory_dataread = SDL_SwapLE16(*((word*)(&BIOS_custom_ROM[tempoffset]))); //Read the data from the ROM, reversed!
					memory_datasize = 2; //Only 2 bytes!
					return 1; //Done: we've been read!				
				}
				else //Enough to read a byte only?
				{
					memory_dataread = BIOS_custom_ROM[tempoffset]; //Read the data from the ROM, reversed!
					memory_datasize = 1; //Only 1 byte!
					return 1; //Done: we've been read!				
				}
			}
			else //Enough to read a byte only?
			{
				memory_dataread = BIOS_custom_ROM[tempoffset]; //Read the data from the ROM, reversed!
				memory_datasize = 1; //Only 1 byte!
				return 1; //Done: we've been read!				
			}
		}
		else //Custom ROM, but nothing to give? Give 0x00!
		{
			memory_dataread = 0x00; //Dummy value for the ROM!
			return 1; //Abort!
		}
		tempoffset = basepos; //Restore the temporary offset!
	}

	INLINEREGISTER uint_32 segment; //Current segment!
	//dolog("CPU","BIOS Read handler: %08X+%08X",baseoffset,reloffset);
	switch (BIOS_ROM_type) //What ROM type are we emulating?
	{
		case BIOSROMTYPE_U18_19: //U18&19 combo?
			tempoffset = basepos;
			tempoffset &= 0x7FFF; //Our offset within the ROM!
			segment = (((basepos >> 15) & 1) ^ 1); //ROM number: 0x8000+:u18, 0+:u19
			segment += 18; //The ROM number!
			if (likely(BIOS_ROM_size[segment]>tempoffset)) //Within range?
			{
				if (likely((index & 3) == 0))
				{
					if (likely(((tempoffset & 3) == 0) && ((tempoffset | 3) <= BIOS_ROM_size[segment]))) //Enough to read a dword?
					{
						memory_dataread = SDL_SwapLE32(*((uint_32*)(&BIOS_ROMS[segment][tempoffset]))); //Read the data from the ROM!
						memory_datasize = 4; //A whole dword!
						return 1; //Done: we've been read!
					}
					else if (likely(((tempoffset & 1) == 0) && ((tempoffset | 1) <= BIOS_ROM_size[segment]))) //Enough to read a word, aligned?
					{
						memory_dataread = SDL_SwapLE16(*((word*)(&BIOS_ROMS[segment][tempoffset]))); //Read the data from the ROM, reversed!
						memory_datasize = 2; //Only 2 bytes!
						return 1; //Done: we've been read!				
					}
					else //Enough to read a byte only?
					{
						memory_dataread = BIOS_ROMS[segment][tempoffset]; //Read the data from the ROM, reversed!
						memory_datasize = 1; //Only 1 byte!
						return 1; //Done: we've been read!				
					}
				}
				else //Enough to read a byte only?
				{
					memory_dataread = BIOS_ROMS[segment][tempoffset]; //Read the data from the ROM, reversed!
					memory_datasize = 1; //Only 1 byte!
					return 1; //Done: we've been read!				
				}
			}
			break;
		case BIOSROMTYPE_U34_35: //Odd/even ROM
		case BIOSROMTYPE_U27_47: //Odd/even ROM
		case BIOSROMTYPE_U13_15: //U13&15 combo? Also Odd/even ROM!
			/*segment =*/ tempoffset = basepos; //Load the offset! General for AT+ ROMs!
			if (likely(tempoffset<BIOS_ROM_U13_15_double)) //This is doubled in ROM!
			{
				if (unlikely(tempoffset>=BIOS_ROM_U13_15_single)) //Second copy?
				{
					tempoffset -= BIOS_ROM_U13_15_single; //Patch to first block to address!
				}
			}
			/*
			tempoffset >>= 1; //The offset is at every 2 bytes of memory!
			segment &= 1; //Even=u27, Odd=u47
			segment <<= 1; //1:15(+2),0=13(+0).
			segment += 13; //The ROM number!
			if (likely(BIOS_ROM_size[segment]>tempoffset)) //Within range?
			{
				memory_dataread = BIOS_ROMS[segment][tempoffset]; //Give the value!
				memory_datasize = 1; //Only 1 byte!
				return 1;
			}
			*/

			if (likely(BIOS_combinedROM_size > tempoffset)) //Within range?
			{
				if ((index & 3) == 0)
				{
					if (likely(((tempoffset & 3) == 0) && ((tempoffset | 3) <= BIOS_combinedROM_size))) //Enough to read a dword?
					{
						memory_dataread = SDL_SwapLE32(*((uint_32*)(&BIOS_combinedROM[tempoffset]))); //Read the data from the ROM!
						memory_datasize = 4; //A whole dword!
						return 1; //Done: we've been read!
					}
					else if (likely(((tempoffset & 1) == 0) && ((tempoffset | 1) <= BIOS_combinedROM_size))) //Enough to read a word, aligned?
					{
						memory_dataread = SDL_SwapLE16(*((word*)(&BIOS_combinedROM[tempoffset]))); //Read the data from the ROM, reversed!
						memory_datasize = 2; //Only 2 bytes!
						return 1; //Done: we've been read!				
					}
					else //Enough to read a byte only?
					{
						memory_dataread = BIOS_combinedROM[tempoffset]; //Read the data from the ROM, reversed!
						memory_datasize = 1; //Only 1 byte!
						return 1; //Done: we've been read!				
					}
				}
				else //Enough to read a byte only?
				{
					memory_dataread = BIOS_combinedROM[tempoffset]; //Read the data from the ROM, reversed!
					memory_datasize = 1; //Only 1 byte!
					return 1; //Done: we've been read!				
				}
			}
			break;
		//case BIOSROMTYPE_U27_47: //U27&47 combo?
			/*segment =*/ /*tempoffset = basepos;*/ //Load the offset! General for AT+ ROMs!
			/*
			tempoffset >>= 1; //The offset is at every 2 bytes of memory!
			segment &= 1; //Even=u27, Odd=u47
			segment = (((segment << 3) + (segment << 1)) << 1); //Set to 20 for the (*10*2 using bit shifts) for the offset to the ROM number!
			segment += 27; //The base ROM number!
			if (likely(BIOS_ROM_size[segment]>tempoffset)) //Within range?
			{
				memory_dataread = BIOS_ROMS[segment][tempoffset]; //Give the value!
				return 1;
			}
			*/
			/*
			if (likely(BIOS_combinedROM_size > tempoffset)) //Within range?
			{
				if ((index & 3) == 0)
				{
					if (((tempoffset & 3) == 0) && ((tempoffset | 3) <= BIOS_combinedROM_size)) //Enough to read a dword?
					{
						memory_dataread = SDL_SwapLE32(*((uint_32*)(&BIOS_combinedROM[tempoffset]))); //Read the data from the ROM!
						memory_datasize = 4; //A whole dword!
						return 1; //Done: we've been read!
					}
					else if (((tempoffset & 1) == 0) && ((tempoffset | 1) <= BIOS_combinedROM_size)) //Enough to read a word, aligned?
					{
						memory_dataread = SDL_SwapLE16(*((word*)(&BIOS_combinedROM[tempoffset]))); //Read the data from the ROM, reversed!
						memory_datasize = 2; //Only 2 bytes!
						return 1; //Done: we've been read!				
					}
					else //Enough to read a byte only?
					{
						memory_dataread = BIOS_combinedROM[tempoffset]; //Read the data from the ROM, reversed!
						memory_datasize = 1; //Only 1 byte!
						return 1; //Done: we've been read!				
					}
				}
				else //Enough to read a byte only?
				{
					memory_dataread = BIOS_combinedROM[tempoffset]; //Read the data from the ROM, reversed!
					memory_datasize = 1; //Only 1 byte!
					return 1; //Done: we've been read!				
				}
			}
			break;
			*/
		//case BIOSROMTYPE_U34_35: //U34&35 combo?
			/*segment =*/ /*tempoffset = basepos;*/ //Load the offset! General for AT+ ROMs!
			/*
			tempoffset >>= 1; //The offset is at every 2 bytes of memory!
			segment &= 1; //Even=u27, Odd=u47
			segment += 34; //The segment number to use!
			if (likely(BIOS_ROM_size[segment]>tempoffset)) //Within range?
			{
				memory_dataread = BIOS_ROMS[segment][tempoffset]; //Give the value!
				return 1;
			}
			*/

			/*
			if (likely(BIOS_combinedROM_size > tempoffset)) //Within range?
			{
				if ((index & 3) == 0)
				{
					if (((tempoffset & 3) == 0) && ((tempoffset | 3) <= BIOS_combinedROM_size)) //Enough to read a dword?
					{
						memory_dataread = SDL_SwapLE32(*((uint_32*)(&BIOS_combinedROM[tempoffset]))); //Read the data from the ROM!
						memory_datasize = 4; //A whole dword!
						return 1; //Done: we've been read!
					}
					else if (((tempoffset & 1) == 0) && ((tempoffset | 1) <= BIOS_combinedROM_size)) //Enough to read a word, aligned?
					{
						memory_dataread = SDL_SwapLE16(*((word*)(&BIOS_combinedROM[tempoffset]))); //Read the data from the ROM, reversed!
						memory_datasize = 2; //Only 2 bytes!
						return 1; //Done: we've been read!				
					}
					else //Enough to read a byte only?
					{
						memory_dataread = BIOS_combinedROM[tempoffset]; //Read the data from the ROM, reversed!
						memory_datasize = 1; //Only 1 byte!
						return 1; //Done: we've been read!				
					}
				}
				else //Enough to read a byte only?
				{
					memory_dataread = BIOS_combinedROM[tempoffset]; //Read the data from the ROM, reversed!
					memory_datasize = 1; //Only 1 byte!
					return 1; //Done: we've been read!				
				}
			}
			break;
			*/
			default: break; //Unknown even/odd mapping!
	}
	return OPTROM_readhandler(offset,index); //OPTROM or nothing? Out of range (32-bit)?
}

void BIOS_registerROM()
{
	//Register our read&write handlers for 16&32-bit CPUs!
	/*
	MMU_registerWriteHandler(&BIOS_writehandler,"BIOSOPTROM");
	MMU_registerReadHandler(&BIOS_readhandler,"BIOSOPTROM");
	*/
	//This is called directly now!
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
		BIGFILE *f;
		char filename[2][100];
		memset(&filename,0,sizeof(filename)); //Clear/init!
		snprintf(filename[0],sizeof(filename[0]), "%s/ROMDMP.%s.BIN", ROMpath,(is_Compaq?"32":(is_XT?"XT":"AT"))); //Create the filename for the ROM for the architecture!
		snprintf(filename[1],sizeof(filename[1]), "ROMDMP.%s.BIN",(is_Compaq?"32":(is_XT?"XT":"AT"))); //Create the filename for the ROM for the architecture!

		f = emufopen64(filename[0],"wb");
		if (!f) return;
		for (;baseloc<endloc;++baseloc)
		{
			if (BIOS_readhandler((uint_32)baseloc,0)) //Read directly!
			{
				if (!emufwrite64(&memory_dataread,1,1,f)) //Failed to write?
				{
					emufclose64(f); //close!
					delete_file(ROMpath,filename[1]); //Remove: invalid!
					return;
				}
			}
		}
		emufclose64(f); //close!
}
