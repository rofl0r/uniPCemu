#include "headers/types.h" //Basic types!
#include "headers/support/zalloc.h" //Memory support!
#include "headers/hardware/ports.h" //I/O support!
#include "headers/support/locks.h" //Lock support!
#include "headers/emu/timers.h" //Timer support!
#include "headers/emu/input.h" //Input support!
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/emu/gpu/gpu_text.h" //GPU text surface support!

#include "headers/support/log.h" //Logging support!
#include "headers/support/highrestimer.h" //Time support!

//Player time update interval in us!
#define PLAYER_TIMEINTERVAL 100000

#include "headers/packed.h" //Packed!
typedef struct PACKED
{
	byte cSignature[8]; //BWRAWOPL
	word iVersionMajor; //Version number (high)
	word iVersionMinor; //Version number (low)
} DR0HEADER;
#include "headers/endpacked.h" //End packed!

enum IHARDWARETYPE01 {
	IHARDWARETYPE01_OPL2 = 0,
	IHARDWARETYPE01_OPL3 = 1,
	IHARDWARETYPE01_DUALOPL2 = 2
};

enum IHARDWARETYPE20 {
	IHARDWARETYPE20_OPL2 = 0,
	IHARDWARETYPE20_OPL3 = 1,
	IHARDWARETYPE20_DUALOPL2 = 2
};

enum COMMANDFORMAT {
	COMMANDFORMAT_INTERLEAVED = 0, //Interleaved commands/data
	COMMANDFORMAT_COMMANDS_DATA = 1 //First all commands, then all data
};

enum {
	DR0REGISTER_DELAY8 = 0, //Data byte following + 1 is the amount in milliseconds
	DR0REGISTER_DELAY16 = 1, //Same as above, but 16-bits
	DR0REGISTER_LOWOPLCHIP = 2, //Switch to Low OPL chip (#0)
	DR0REGISTER_HIGHOPLCHIP = 3, //Switch to High OPL chip (#1)
	DR0REGISTER_ESC = 4 //Escape: The next two bytes are normal register/value data.
} DR0REGISTER;

#include "headers/packed.h" //Packed!
typedef struct PACKED
{
	uint_32 iLengthMS; //Length of the song in ms
	uint_32 iLengthBytes; //Length of the song data in bytes
	byte iHardwareType; //Flag listing of the hardware used in the song
	byte iHardwareExtra[3]; //Rest of hardware type or song data. Must be 0. If nonzero, this is an early header.
} DR01HEADER;
#include "headers/endpacked.h" //End packed!


#include "headers/packed.h" //Packed!
typedef struct PACKED
{
	uint_32 iLengthMS; //Length of the song in ms
	uint_32 iLengthBytes; //Length of the song data in bytes
	byte iHardwareType; //Flag listing of the hardware used in the song
} DR01HEADEREARLY;
#include "headers/endpacked.h" //End packed!


#include "headers/packed.h" //Packed!
typedef struct PACKED
{
	uint_32 iLengthPairs; //Length of the song in register/value pairs
	uint_32 iLengthMS; //Length of the song data in milliseconds
	byte iHardwareType; //Flag listing of the hardware used in the song
	byte iFormat; //Data arrangement
	byte iCompression; //Compression type. 0=No compression. Currently only is used.
	byte iShortDelayCode; //Command code for short delay(1-256ms).
	byte iLongDelayCode; //Command for long delay(256ms)
	byte iCodemapLength; //Length of the codemap table, in bytes
} DR20HEADER;
#include "headers/endpacked.h" //End packed!

//The codemap table has a length of iCodemapLength entries of 1 byte.

//The DR0 file reader.
byte readDRO(char *filename, DR0HEADER *header, DR01HEADEREARLY *earlyheader, DR01HEADER *oldheader, DR20HEADER *newheader, byte *CodemapTable, byte **data, uint_32 *datasize)
{
	byte correctSignature[8] = {'D','B','R','A','W','O','P','L'};
	byte version = 0; //The version to return!
	word temp;
	uint_32 filesize;
	FILE *f; //The file!
	uint_32 oldpos;
	f = fopen(filename,"rb"); //Open the filename!
	byte empty[3] = {0,0,0}; //Empty data!
	if (fread(header,1,sizeof(*header),f)!=sizeof(*header))
	{
		fclose(f);
		return 0; //Error reading the file!
	}
	if (memcmp(&header->cSignature,&correctSignature,8)!=0) //Signature error?
	{
		fclose(f);
		return 0; //Error: Invalid signature!
	}
	if (((header->iVersionMajor==0) && (header->iVersionMinor==1)) || ((header->iVersionMajor == 1) && (header->iVersionMinor == 0))) //Version 1.0(old) or 0.1(new)?
	{
		oldpos = ftell(f); //Save the old position to jump back to!
		if (fread(oldheader,1,sizeof(*oldheader),f)!=sizeof(*oldheader)) //New header invalid size/read error?
		{
			fclose(f);
			return 0; //Error reading the file!
		}
		if (memcmp(&oldheader->iHardwareExtra,&empty,sizeof(oldheader->iHardwareExtra))!=0) //Maybe earlier version?
		{
			fseek(f,oldpos,SEEK_SET); //Return!
			if (fread(earlyheader,1,sizeof(*earlyheader),f)!=sizeof(*earlyheader)) //New header invalid size/read error?
			{
				fclose(f);
				return 0; //Error reading the file!
			}
			version = 1; //Early old-style header!
		}
		else //Old-style header?
		{
			version = 2; //Old-style header!
			memcpy(&earlyheader,&oldheader,sizeof(*earlyheader)); //Copy to the early header for easier reading, since it's only padded!
		}

		//Since we're old-style anyway, patch the codemap and 2.0 table values based on the old header now!
		for (temp=0;temp<0x100;++temp)
		{
			CodemapTable[temp] = (temp&0x7F); //All ascending, repeat, as per Dual-OPL2!
		}
		newheader->iCodemapLength = 0x00; //Maximum codemap length(No translation)!
		newheader->iLengthPairs = (oldheader->iLengthBytes>>1); //Length in pairs(2 bytes)!
		newheader->iLengthMS = oldheader->iLengthMS; //Length in pairs(2 bytes)!
		switch (oldheader->iHardwareType)
		{
		case IHARDWARETYPE01_OPL2:
			newheader->iHardwareType = IHARDWARETYPE20_OPL2; //Translate!
			break;
		case IHARDWARETYPE01_OPL3:
			newheader->iHardwareType = IHARDWARETYPE20_OPL3; //Translate!
			break;
		case IHARDWARETYPE01_DUALOPL2:
			newheader->iHardwareType = IHARDWARETYPE20_DUALOPL2; //Translate!
			break;
		}
		newheader->iFormat = COMMANDFORMAT_INTERLEAVED; //Default to interleaved format!
		newheader->iCompression = 0; //No compression!
		//Delay codes aren't used!
	}
	else if ((header->iVersionMajor==2) && (header->iVersionMinor==0)) //Version 2.0?
	{
		if (fread(newheader,1,sizeof(*newheader),f)!=sizeof(*newheader)) //New header invalid size/read error?
		{
			fclose(f);
			return 0; //Error reading the file!
		}
		if (!newheader->iCodemapLength) //Invalid code map length?
		{
			fclose(f);
			return 0; //Error reading the file: invalid code map length!
		}
		memset(CodemapTable,0,256); //Clear the entire table for the new file!
		if (fread(CodemapTable,1,newheader->iCodemapLength,f)!=newheader->iCodemapLength) //New header invalid size/read error?
		{
			fclose(f);
			return 0; //Error reading the file!
		}
		version = 3; //2.0 version!
	}
	else //Invalid version?
	{
		fclose(f);
		return 0; //Error: Invalid signature!
	}

	oldpos = ftell(f); //Save the old position to jump back to!
	fseek(f,0,SEEK_END);
	filesize = ftell(f); //File size!
	fseek(f,oldpos,SEEK_SET); //Return to the start of the data!
	filesize -= oldpos; //Difference is the file size!
	if (!filesize)
	{
		fclose(f);
		return 0; //Error: invalid file size!
	}

	*data = zalloc(filesize,"DROFILE",NULL); //Allocate a DR0 file's contents!
	if (!*data) //Failed to allocate?
	{
		fclose(f);
		return 0; //Error: ran out of memory!
	}

	if (fread(*data,1,filesize,f)!=filesize) //Error reading contents?
	{
		freez((void **)data,filesize,"DROFILE"); //Release the file!
		fclose(f);
		return 0; //Error: file couldn't be read!
	}
	fclose(f); //Finished, close the file!
	*datasize = filesize; //Save the filesize for reference!
	return version; //Successfully read the file!
}

//Adlib support!

byte OPLlock = 0; //Default: not locked!

void lockOPL()
{
	if (OPLlock) return; //Prevent re-lock!
	lockEMUOPL();
	OPLlock = 1; //Locked!
}

void unlockOPL()
{
	if (OPLlock) //Prevent re-unlock!
	{
		unlockEMUOPL();
		OPLlock = 0; //Unlocked!
	}
}

void OPLXsetreg(byte version, byte newHardwareType, byte whatchip,byte reg,byte *CodemapTable,byte codemapLength,byte value)
{
	byte chip;
	if (version < 3) //Version 1.0?
	{
		switch (newHardwareType) //What hardware type?
		{
		case IHARDWARETYPE20_OPL2: //OPL2?
			chip = 0; //Only one chip(Dual OPL-2)/register bank(OPL-3)!
			break;
		case IHARDWARETYPE20_OPL3: //OPL3?
			chip = whatchip; //Not supported yet: High register bank!
			break;
		case IHARDWARETYPE20_DUALOPL2: //Dual OPL2?
			chip = whatchip; //Not supported yet: High chip!
			break;
		default: //Unknown hardware type?
			unlockOPL(); //We're finished with the OPL!
			return; //Unknown hardware: abort!
			break;
		}
	}
	else //Version 2.0?
	{
		switch (newHardwareType) //What hardware type?
		{
		case IHARDWARETYPE20_OPL2: //OPL2?
			break;
		case IHARDWARETYPE20_OPL3: //OPL3?
			break;
		case IHARDWARETYPE20_DUALOPL2: //Dual OPL2?
			break;
		default: //Unknown hardware type?
			unlockOPL(); //We're finished with the OPL!
			return; //Unknown hardware: abort!
			break;
		}
		chip = (reg & 0x80)?1:0; //Not supported yet: High register bank/chip!
		reg &= 0x7F; //Only the low bits are looked up!
		if (reg<codemapLength) reg = CodemapTable[reg]; //Translate reg through the Codemap Table when within range!
	}
	if (chip)
	{
		unlockOPL(); //We're finished with the OPL!
		return; //High chip/bank isn't supported yet!
	}
	//Ignore the chip!
	lockOPL(); //We need to lock the OPL now!
	PORT_OUT_B(0x388,reg);
	PORT_OUT_B(0x389,value);
}

int readStream(byte **stream, byte *eos)
{
	if (*stream!=eos) //Valid item to read?
		return (int)*((*stream)++);
	return -1; //Invalid item!
}

extern GPU_TEXTSURFACE *BIOS_Surface; //Our display(BIOS) text surface!

void waitTime(TicksHolder *time, uint_64 desttime)
{
	if (getuspassed_k(time)<desttime) //Do we need to wait?
	{
		unlockOPL(); //Unlock the OPL only if we're waiting at all!
		for (;getuspassed_k(time)<desttime;) delay(0); //Wait for the timing to catch up!
	}
}

void showTime(uint_64 playtime, uint_64 *oldplaytime)
{
	static char playtimetext[256] = ""; //Time in text format!
	if (playtime != *oldplaytime && (playtime>=(*oldplaytime+PLAYER_TIMEINTERVAL))) //Playtime updated?
	{
		convertTime(playtime, &playtimetext[0]); //Convert the time(in us)!
		GPU_text_locksurface(BIOS_Surface); //Lock!
		GPU_textgotoxy(BIOS_Surface, 0, 0); //For output!
		GPU_textprintf(BIOS_Surface, RGB(0xFF, 0xFF, 0xFF), RGB(0xBB, 0x00, 0x00), "Play time: %s", playtimetext); //Current CPU speed percentage!
		GPU_text_releasesurface(BIOS_Surface); //Lock!			
		*oldplaytime = playtime; //We're updated with this value!
	}
}

//The player itself!
byte playDROFile(char *filename, byte showinfo) //Play a MIDI file, CIRCLE to stop playback!
{
	word w; //Temporary 16-bit number.
	int streambuffer; //Stream data buffer for read data!
	byte value,channel=0; //OPL Register/value container!
	byte whatchip = 0; //Low/high chip selection!
	uint_64 playtime = 0; //Play time, in ms!
	uint_64 oldplaytime = 0xFFFFFFFFFFFFFFFF; //Old play time!
	TicksHolder timing; //Current time holder!
	//All file data itself:
	DR0HEADER header;
	DR01HEADEREARLY earlyheader;
	DR01HEADER oldheader;
	DR20HEADER newheader;
	byte CodemapTable[0x100]; //256-byte Codemap table!
	byte *data;
	byte *stream; //The stream to use.
	byte *eos; //The end of the stream is reached!
	uint_32 datasize;
	byte stoprunning = 0;
	byte droversion;
	//Start reading the file!

	lock(LOCK_CPU); //Lock the CPU: we're changing state!
	CPU[activeCPU].halt = 2; //Force us into HLT state!
	unlock(LOCK_CPU); //Release the CPU to be used!

	initTicksHolder(&timing); //Initialise our time container!
	getuspassed(&timing); //Initialise our time to 0!

	showTime(playtime,&oldplaytime);

	if ((droversion = readDRO(filename, &header, &earlyheader, &oldheader, &newheader, &CodemapTable[0], &data, &datasize))>0) //Loaded DRO file?
	{
		stopTimers(0); //Stop most timers for max compatiblity and speed!
		//Initialise our device!
		lockOPL();
		for (w=0;w<=0xFF;++w) //Clear all registers!
		{
			OPLXsetreg(droversion,newheader.iHardwareType,0,w,&CodemapTable[0],newheader.iCodemapLength,0); //Clear all registers, as per the DR0 specification!
			OPLXsetreg(droversion,newheader.iHardwareType,1,w,&CodemapTable[0],newheader.iCodemapLength,0); //Clear all registers, as per the DR0 specification!
		}
		unlockOPL();

		startTimers(1);
		startTimers(0); //Start our timers!

		stream = data; //Start processing the start of the stream!

		eos = &data[datasize]; //The end of the stream!

		for (;;) //Wait to end!
		{
			//Process input!
			streambuffer = readStream(&stream,eos); //Read the instruction from the stream!
			if (streambuffer==-1) break; //Stop if reached EOS!
			if (droversion==3) //v2.0 commands?
			{
				channel = (byte)streambuffer; //We're the channel!
				streambuffer = readStream(&stream, eos); //Read the value from the stream!
				if (streambuffer == -1) break; //Stop if reached EOS!
				value = (byte)streambuffer; //We're the value!
				if (channel==newheader.iShortDelayCode) //Short delay?
				{
					playtime += (1000 * (value + 1)); //Update player time!
					waitTime(&timing,playtime); //Delay until we're ready to play more!
					showTime(playtime, &oldplaytime); //Update time!
				}
				else if (channel==newheader.iLongDelayCode) //Long delay?
				{
					playtime += (1000*((value+1)<<8)); //Update player time!
					waitTime(&timing, playtime); //Delay until we're ready to play more!
					showTime(playtime,&oldplaytime); //Update time!
				}
				else goto runinstruction; //Check for v2.0 commands?
				goto nextinstruction;
			}
			//v1.0 commands!
			if (streambuffer==DR0REGISTER_DELAY8) //8-bit delay?
			{
				streambuffer = readStream(&stream,eos); //Read the instruction from the stream!
				if (streambuffer==-1) break; //Stop if reached EOS!
				playtime += (1000*(streambuffer+1)); //Update player time!
				waitTime(&timing, playtime); //Delay until we're ready to play more!
				showTime(playtime, &oldplaytime); //Update time!
			}
			else if (streambuffer==DR0REGISTER_DELAY16) //16-bit delay?
			{
				streambuffer = readStream(&stream,eos); //Read the instruction low byte from the stream!
				if (streambuffer==-1) break; //Stop if reached EOS!
				w = (streambuffer&0xFF); //Load low byte!
				streambuffer = readStream(&stream,eos); //Read the instruction high byte from the stream!
				if (streambuffer == -1) break; //Stop if reached EOS!
				w |= ((streambuffer & 0xFF)<<8); //Load high byte!
				playtime += (1000*(w+1)); //Update player time!
				waitTime(&timing, playtime); //Delay until we're ready to play more!
				showTime(playtime, &oldplaytime); //Update time!
			}
			else if ((streambuffer==DR0REGISTER_LOWOPLCHIP) && (newheader.iHardwareType!=IHARDWARETYPE20_OPL2)) //Low OPL chip and supported?
			{
				whatchip = 0; //Low channel in dual-OPL!
			}
			else if ((streambuffer==DR0REGISTER_HIGHOPLCHIP) && (newheader.iHardwareType != IHARDWARETYPE20_OPL2)) //High OPL chip and supported?
			{
				whatchip = 1; //High channel in dual-OPL!
			}
			else if (streambuffer==DR0REGISTER_ESC) //Escape?
			{
				streambuffer = readStream(&stream,eos); //Read the instruction from the stream!
				if (streambuffer==-1) break; //Stop if reached EOS!
				goto escapedinstruction; //Execute us as a normal instruction!
			}
			else //Normal instruction?
			{
				escapedinstruction: //Process a normal instruction!
				channel = (byte)streambuffer; //The first is the channel!
				streambuffer = readStream(&stream,eos); //Read the instruction from the stream!
				if (streambuffer==-1) break; //Stop if reached EOS!
				value = (byte)streambuffer; //The second is the value!
				runinstruction: //Run a normal instruction!
				OPLXsetreg(droversion,newheader.iHardwareType,whatchip,channel,&CodemapTable[0],newheader.iCodemapLength,value); //Set the register!
			}

			nextinstruction: //Execute next instruction!
			//Check for stopping the song!			
			if (psp_keypressed(BUTTON_CIRCLE) || psp_keypressed(BUTTON_STOP)) //Circle/stop pressed? Request to stop playback!
			{
				for (; (psp_keypressed(BUTTON_CIRCLE) || psp_keypressed(BUTTON_STOP));) delay(0); //Wait for release while pressed!
				stoprunning = 1; //Set termination flag to request a termination!
			}

			if (stoprunning) break; //Not running anymore? Start quitting!
		}

		showTime(playtime, &oldplaytime); //Update time!

		lockOPL(); //Make sure we're locked!
		for (w=0;w<=0xFF;++w) //Clear all registers!
		{
			OPLXsetreg(droversion,newheader.iHardwareType,0,(w&0x7F),&CodemapTable[0],newheader.iCodemapLength,0); //Clear all registers, as per the DR0 specification!
			OPLXsetreg(droversion,newheader.iHardwareType,1,(w&0x7F),&CodemapTable[0],newheader.iCodemapLength,0); //Clear all registers, as per the DR0 specification!
		}
		unlockOPL();

		freez((void **)&stream,datasize,"DROFILE");

		return 1; //Played without termination?
	}
	return 0; //Invalid file?
}