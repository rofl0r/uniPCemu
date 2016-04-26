#include "headers/types.h" //Basic types!
#include "headers/support/zalloc.h" //Memory support!

#include "headers/packed.h" //Packed!
typedef struct PACKED
{
	byte cSignature[8]; //BWRAWOPL
	word iVersionMajor; //Version number (high)
	word iVersionMinor; //Version number (low)
} DR0HEADER;
#include "headers/endpacked.h" //End packed!

enum {
	OPL2 = 0,
	OPL3 = 1,
	DUALOPL2 = 2
} IHARDWARETYPE01;

enum {
	OPL2 = 0,
	OPL3 = 1,
	DUALOPL2 = 2
} IHARDWARETYPE20;

enum {
	INTERLEAVED = 0, //Interleaved commands/data
	COMMANDS_DATA = 1 //First all commands, then all data
} COMMANDFORMAT;

enum {
	DELAY8 = 0, //Data byte following + 1 is the amount in milliseconds
	DELAY16 = 1, //Same as above, but 16-bits
	LOWOPLCHIP = 2, //Switch to Low OPL chip (#0)
	HIGHOPLCHIP = 3, //Switch to High OPL chip (#1)
	ESC = 4 //Escape: The next two bytes are normal register/value data.
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
	uint_32 iLengthBytes; //Length of the song data in milliseconds
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
	if ((header->iVersionMajor==0) && (header->iVersionMinor==1)) //Version 1.0(old) or 0.1(new)?
	{
		oldpos = ftell(f); //Save the old position to jump back to!
		if (fread(oldheader,1,sizeof(*oldheader),f)!=sizeof(*oldheader)) //New header invalid size/read error?
		{
			fclose(f);
			return 0; //Error reading the file!
		}
		if (memcmp(&oldheader->iHardwareExtra,0,sizeof(oldheader->iHardwareExtra)!=0) //Maybe earlier version?
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
			CodemapTable[temp] = (temp&7F); //All ascending, repeat, as per Dual-OPL2!
		}
		newheader->iCodemapLength = 0xFF; //Maximum codemap length!
		newheader->iLengthPairs = (oldheader->iLengthBytes>>1); //Length in pairs(2 bytes)!
		newheader->iLengthMS = oldheader->iLengthMS;
		newheader->iHardwareType = oldheader->iHardwareType; //Virtually the same between the two versions!
		newheader->iFormat = COMMANDFORMAT.INTERLEAVED; //Default to interleaved format!
		newheader->iCompression = 0; //No compression!
		newheader->iShortDelayCode = DR0REGISTER.DELAY8; //Short delay code!
		newheader->iLongDelayCode = DR0REGISTER.DELAY16; //Long delay code!
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
		freez(data,filesize,"DROFILE"); //Release the file!
		fclose(f);
		return 0; //Error: file couldn't be read!
	}
	fclose(f); //Finished, close the file!
	*datasize = filesize; //Save the filesize for reference!
	return version; //Successfully read the file!
}

//Adlib support!
void adlibsetreg(byte reg,byte val)
{
	PORT_OUT_B(0x388,reg);
	PORT_OUT_B(0x389,val);
}

void lockOPL()
{
	lock(LOCK_CPU);
}

void unlockOPL()
{
	unlock(LOCK_CPU);
}

//The player itself!
byte playDROFile(char *filename, byte showinfo) //Play a MIDI file, CIRCLE to stop playback!
{
	byte escaped = 0; //Is the current instruction escaped?
	byte b; //Temporary 8-bit number.
	word w; //Temporary 16-bit number.
	int streambuffer; //Stream data buffer for read data!
	byte channelregister,value,channel=0; //OPL Register/value container!
	byte whatchip = 0; //Low/high chip selection!
	//All file data itself:
	DR0HEADER header;
	DR01HEADEREARLY earlyheader;
	DR01HEADER oldheader;
	DR20HEADER newheader;
	byte CodemapTable[0x100]; //256-byte Codemap table!
	byte *data,
	byte *stream; //The stream to use.
	uint_32 datasize;
	byte stoprunning = 0;
	byte droversion;
	//Start reading the file!
	if (droversion = readDRO(char *filename, &header, &earlyheader, &oldheader, &newheader, &CodemapTable, &data, &datasize)) //Loaded DRO file?
	{
		stopTimers(0); //Stop most timers for max compatiblity and speed!
		//Initialise our device!
		lockOPL();
		for (w=0;w<=0xFF;++w) //Clear all registers!
		{
			adlibsetreg(w,0); //Clear all registers, as per the DR0 specification!
		}
		unlockOPL();

		startTimers(1);
		startTimers(0); //Start our timers!

		stream = data; //Start processing the start of the stream!

		for (;;) //Wait to end!
		{
			//Process input!
			streambuffer = readStream(&stream); //Read the instruction from the stream!
			if (streambuffer==-1) break; //Stop if reached EOS!
			if ((streambuffer==newheader->iShortDelayCode) && (droversion==3)) //Short delay?
			{
				streambuffer = readStream(&stream); //Read the instruction from the stream!
				if (streambuffer==-1) break; //Stop if reached EOS!
				delay(1000*(streambuffer+1)); //Delay!
			}
			else if ((streambuffer==newheader->iLongDelayCode) && (droversion==3)) //Long delay?
			{
				streambuffer = readStream(&stream); //Read the instruction from the stream!
				if (streambuffer==-1) break; //Stop if reached EOS!
				delay(1000*((streambuffer+1)*256)); //Delay the long period!
			}
			else //Normal instruction?
			{
				if ((streambuffer==DR0REGISTER.DELAY8) && (droversion!=3)) //8-bit delay?
				{
					streambuffer = readStream(&stream); //Read the instruction from the stream!
					if (streambuffer==-1) break; //Stop if reached EOS!
				}
				else if ((streambuffer==DR0REGISTER.DELAY16) && (droversion!=3)) //16-bit delay?
				{
					streambuffer = readStream(&stream); //Read the instruction from the stream!
					if (streambuffer==-1) break; //Stop if reached EOS!
				}
				else if ((streambuffer=DR0REGISTER.LOWOPLCHIP) && (droversion!=3)) //Low OPL chip?
				{
					streambuffer = readStream(&stream); //Read the instruction from the stream!
					if (streambuffer==-1) break; //Stop if reached EOS!
					whatchip = 0; //Low channel in dual-OPL!
				}
				else if ((streambuffer==DR0REGISTER.HIGHOPLCHIP) && (droversion!=3)) //High OPL chip?
				{
					streambuffer = readStream(&stream); //Read the instruction from the stream!
					if (streambuffer==-1) break; //Stop if reached EOS!
					whatchip = 1; //High channel in dual-OPL!
				}
				else if (streambuffer==DR0REGISTER.ESC) //Escape?
				{
					streambuffer = readStream(&stream); //Read the instruction from the stream!
					if (streambuffer==-1) break; //Stop if reached EOS!
					goto normalinstruction; //Execute us as a normal instruction!
				}
				else //Normal instruction?
				{
					normalinstruction: //Process a normal instruction!
					channel = streambuffer; //The first is the channel!
					streambuffer = readStream(&stream); //Read the instruction from the stream!
					if (streambuffer==-1) break; //Stop if reached EOS!
					value = streambuffer; //The second is the value!					
					lockOPL();
					adlibsetreg(whatchip,CodemapTable[(channel)&0x7F],value); //Set the register!
					unlockOPL();
				}

			//Check for stopping the song!			
			if (psp_keypressed(BUTTON_CIRCLE) || psp_keypressed(BUTTON_STOP)) //Circle/stop pressed? Request to stop playback!
			{
				for (; (psp_keypressed(BUTTON_CIRCLE) || psp_keypressed(BUTTON_STOP));) delay(0); //Wait for release while pressed!
				stoprunning = 1; //Set termination flag to request a termination!
			}

			if (stoprunning) break; //Not running anymore? Start quitting!
		}

		lockOPL();
		for (w=0;w<=0xFF;++w) //Clear all registers!
		{
			adlibsetreg(w,0); //Clear all registers, as per the DR0 specification!
		}
		unlockOPL();

		freez((void **)&stream,datasize,"DROFILE");

		return 1; //Played without termination?
	}
	return 0; //Invalid file?
}