#include "headers/types.h" //Basic types!
#include "headers/mmu/MMU.h" //MMU support!
#include "headers/emu/gpu/gpu.h" //Read output module!
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/hardware/vga.h" //VGA support!
#include "headers/hardware/ports.h" //Port support!
#include "headers/mmu/mmuhandler.h" //Our typedefs!

extern BIOS_Settings_TYPE BIOS_Settings; //Settings!
extern MMU_type MMU; //MMU for direct access!

struct
{
MMU_WHANDLER writehandlers[100]; //Up to 100 write handlers!
uint_32 startoffsetw[100]; //Start offset of the handler!
uint_32 endoffsetw[100]; //End offset of the handler!
char modulew[100][20]; //Module names!
byte numw; //Ammount registered!

MMU_RHANDLER readhandlers[100]; //Up to 100 read handlers!
uint_32 startoffsetr[100]; //Start offset of the handler!
uint_32 endoffsetr[100]; //End offset of the handler!
char moduler[100][20]; //Module names!
byte numr; //Ammount registered!
} MMUHANDLER;

void MMUHANDLER_countwrites()
{
	MMUHANDLER.numw=NUMITEMS(MMUHANDLER.writehandlers); //Init!
	for (;MMUHANDLER.numw;)
	{
		if (MMUHANDLER.writehandlers[MMUHANDLER.numw-1]) //Found?
		{
			return; //Stop searching!
		}
		--MMUHANDLER.numw; //Next!
	}
}

void MMUHANDLER_countreads()
{
	MMUHANDLER.numr=NUMITEMS(MMUHANDLER.readhandlers); //Init!
	for (;MMUHANDLER.numr;)
	{
		if (MMUHANDLER.readhandlers[MMUHANDLER.numr-1]) //Found?
		{
			return; //Stop searching!
		}
		--MMUHANDLER.numr; //Next!
	}
}

void MMU_resetHandlers(char *module) //Initialise/reset handlers!
{
	char empty='\0'; //Empty string!
	byte i=0;
	if (!module) module=&empty; //Empty module patch!
	for(;i<NUMITEMS(MMUHANDLER.writehandlers);i++)
	{
		if ((strcmp(MMUHANDLER.modulew[i],module)==0) || (strcmp(module,"")==0)) //No module or current module?
		{
			MMUHANDLER.writehandlers[i] = NULL; //Reset!
			MMUHANDLER.startoffsetw[i] = 0; //Reset!
			MMUHANDLER.endoffsetw[i] = 0; //Reset!
		}

		if ((strcmp(MMUHANDLER.moduler[i],module)==0) || (strcmp(module,"")==0)) //No module or current module?
		{
			MMUHANDLER.readhandlers[i] = NULL; //Reset!
			MMUHANDLER.startoffsetr[i] = 0; //Reset!
			MMUHANDLER.endoffsetr[i] = 0; //Reset!
		}
	}

	if (!module) //All cleared?
	{
		MMUHANDLER.numw = 0; //Reset!
		MMUHANDLER.numr = 0; //Reset!
	}
	else //Cleared one module: search for the last one used!
	{
		MMUHANDLER_countwrites();
		MMUHANDLER_countreads();		
	}
}

byte MMU_registerWriteHandler(uint_32 startoffset, uint_32 endoffset, MMU_WHANDLER handler, char *module) //Register a write handler!
{
	byte i=0;
	for (;i<NUMITEMS(MMUHANDLER.writehandlers);i++)
	{
		if (!MMUHANDLER.writehandlers[i]) //Not set?
		{
			MMUHANDLER.writehandlers[i] = handler; //Set the handler to use!
			MMUHANDLER.startoffsetw[i] = startoffset;
			MMUHANDLER.endoffsetw[i] = endoffset;
			memset(&MMUHANDLER.modulew[i],0,sizeof(&MMUHANDLER.modulew[i])); //Init module!
			strcpy(MMUHANDLER.modulew[i],module); //Set module!
			MMUHANDLER_countwrites(); //Recount!
			return 1; //Registered!
		}
	}
	return 0; //Error: ran out of space!
}

byte MMU_registerReadHandler(uint_32 startoffset, uint_32 endoffset, MMU_RHANDLER handler, char *module) //Register a read handler!
{
	byte i=0;
	for (;i<NUMITEMS(MMUHANDLER.readhandlers);i++)
	{
		if (!MMUHANDLER.readhandlers[i]) //Not set?
		{
			MMUHANDLER.readhandlers[i] = handler; //Set the handler to use!
			MMUHANDLER.startoffsetr[i] = startoffset;
			MMUHANDLER.endoffsetr[i] = endoffset;
			memset(&MMUHANDLER.moduler[i],0,sizeof(&MMUHANDLER.moduler[i])); //Init module!
			strcpy(MMUHANDLER.moduler[i],module); //Set module!
			MMUHANDLER_countreads(); //Recount!
			return 1; //Registered!
		}
	}
	return 0; //Error: ran out of space!
}

//Handler for special MMU-based I/O, direct addresses used!
byte MMU_IO_writehandler(uint_32 offset, byte value)
{
	byte i=0;
	for (;((i<NUMITEMS(MMUHANDLER.writehandlers)) && (i<MMUHANDLER.numw));) //Search all available handlers!
	{
		if (MMUHANDLER.writehandlers[i]) //Set?
		{
			if (strcmp(MMUHANDLER.modulew[i],"")!=0) //Set name?
			{
				if (offset>=MMUHANDLER.startoffsetw[i] && offset<=MMUHANDLER.endoffsetw[i]) //Within range?
				{
					if (MMUHANDLER.writehandlers[i](MMUHANDLER.startoffsetw[i],offset-MMUHANDLER.startoffsetw[i],value)) //Success?
					{
						return 0; //Abort searching: we're processed!
					}
				}
			}
		}
		++i; //Next!
	}
	return 1; //Normal memory access!
}
//Reading only!
byte MMU_IO_readhandler(uint_32 offset, byte *value)
{
	byte i=0;
	for (;((i<NUMITEMS(MMUHANDLER.readhandlers)) && (i<MMUHANDLER.numr));) //Search all available handlers!
	{
		if (MMUHANDLER.readhandlers[i]) //Set?
		{
			if (strcmp(MMUHANDLER.moduler[i],"")!=0) //Set name?
			{
				if (offset>=MMUHANDLER.startoffsetr[i] && offset<=MMUHANDLER.endoffsetr[i]) //Within range?
				{
					if (MMUHANDLER.readhandlers[i](MMUHANDLER.startoffsetr[i],offset-MMUHANDLER.startoffsetr[i],value)) //Success reading?
					{
						return 0; //Abort searching: we're processed!
					}
				}
			}
		}
		++i; //Next!
	}
	return 1; //Normal memory access!
}