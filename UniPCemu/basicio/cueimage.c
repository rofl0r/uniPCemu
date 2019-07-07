#include "headers/basicio/io.h"
#include "headers/fopen64.h"
#include "headers/emu/directorylist.h"

byte is_cueimage(char *filename)
{
	if (!isext(filename, "cue")) return 0; //Not a cue sheet!
	BIGFILE *f;
	f = emufopen64(filename, "rb"); //Open file!
	if (!f)
	{
		return 0; //Invalid file: file not found!
	}

	emufclose64(f); //Close the file!
	return 1; //CUE image!
}

FILEPOS cueimage_getsize(char *filename)
{
	if (strcmp(filename, "") == 0) return 0; //Not mountable!
	if (!isext(filename, "cue")) return 0; //Not a cue sheet!
	BIGFILE *f;
	f = emufopen64(filename, "rb"); //Open!
	if (!f) //Not found?
	{
		return 0; //No size!
	}
	emufclose64(f); //Close the file!
	return 0; //Give the result! We can't be used by normal read accesses!
}

char cuesheet_line[256]; //A line of the cue sheet!
char cuesheet_line_lc[256]; //A line of the cue sheet in lower case!

byte cuesheet_readline(BIGFILE *f)
{
	byte startedparsingline = 0; //Default: not a valid line with non-whitespace found!
	char c;
	if (emufeof64(f)) return 0; //EOF?
	memset(&cuesheet_line, 0, sizeof(cuesheet_line)); //Init!
	memset(&cuesheet_line_lc, 0, sizeof(cuesheet_line_lc)); //Init!
	for (; !emufeof64(f);) //Not EOF?
	{
		c = emufread64(&c, 1, 1, f); //Read a character from the file!
		switch (c) //What character?
		{
		case 0: //Invalid?
			break; //Ignore!
		case '\n':
		case '\r': //Possible newline?
			if (startedparsingline) //Started parsing the line?
			{
				return 1; //Finished!
			}
		case ' ': //Space character to ignore?
		case '\t': //Tab character to ignore?
			if (startedparsingline) //Started parsing the line?
			{
				safe_scatnprintf(cuesheet_line, sizeof(cuesheet_line), "%c", c); //Add to the line!
				safe_scatnprintf(cuesheet_line_lc, sizeof(cuesheet_line_lc), "%c", tolower(c)); //Add to the line!
			}
			break; //Counted in!
		default: //Unknown character? Parse in the line!
			startedparsingline = 1; //We've started parsing the line if we didn't yet!
			safe_scatnprintf(cuesheet_line, sizeof(cuesheet_line), "%c", c); //Add to the line!
			safe_scatnprintf(cuesheet_line_lc, sizeof(cuesheet_line_lc), "%c", tolower(c)); //Add to the line!
			break; //Counted as a normal character!
		}
	}
	if (startedparsingline) //Started parsing the line?
	{
		return 1; //OK: Give the final line!
	}
	return 0; //Nothing found!
}

typedef struct
{
	char identifier[256]; //Identifier
	word sectorsize;
	byte mode;
} CDROM_TRACK_MODE;

typedef struct
{
	FILEPOS datafilepos; //The current data file position(cumulatively added for non-changing files, by adding the indexes)!
	char MCN[13]; //The MCN for the cue sheet(if any)!
	byte got_MCN;
	char filename[256]; //Filename status!
	char file_type[256]; //Filename type!
	byte got_file; //Gotten?
	byte track_number; //Track number!
	CDROM_TRACK_MODE *track_mode; //Track mode!
	byte got_track; //Gotten?
	char ISRC[12]; //ISRC
	byte got_ISRC; //Gotten ISRC?
	byte index; //Current index!
	byte M; //Current M!
	byte S; //Current S!
	byte F; //Current F!
	byte got_index; //Gotten an index?
} CUESHEET_STATUS;

typedef struct
{
	byte is_present; //Are we a filled in!
	CUESHEET_STATUS status; //The status at the moment the index entry was detected!
	byte endM; //Ending M of the index!
	byte endS; //Ending S of the index!
	byte endF; //Ending F of the index!
	//Ending MSF-1 = last index we're able to use!
} CUESHEET_ENTRYINFO;

extern IODISK disks[0x100]; //All disks available, up go 256 (drive 0-255) disks!

char identifier_CATALOG[8] = "catalog";
char identifier_FILE[5] = "file";
char identifier_TRACK[6] = "track";
char identifier_ISRC[5] = "isrc";
char identifier_PREGAP[7] = "pregap";
char identifier_INDEX[6] = "index";
char identifier_POSTGAP[8] = "postgap";

enum CDROM_MODES {
	MODE_AUDIO = 0,
	MODE_KARAOKE = 1,
	MODE_MODE1DATA = 2,
	MODE_MODEXA = 3,
	MODE_MODECDI = 4,
};

CDROM_TRACK_MODE cdrom_track_modes[10] = {
	{"AUDIO",2352,MODE_AUDIO},	//Audio / Music(2352 — 588 samples)
	{"CDG",2448,MODE_KARAOKE}, //Karaoke CD+G (2448)
	{"MODE1/2048",2048,MODE_MODE1DATA}, //CD - ROM Mode 1 Data(cooked)
	{"MODE1/2352",2352,MODE_MODE1DATA},	//CD - ROM Mode 1 Data(raw)
	{"MODE2/2048",2048,MODE_MODEXA},	//CD - ROM XA Mode 2 Data(form 1) *
	{"MODE2/2324",2324,MODE_MODEXA},	//CD - ROM XA Mode 2 Data(form 2) *
	{"MODE2/2336",2336,MODE_MODEXA},	//CD - ROM XA Mode 2 Data(form mix)
	{"MODE2/2352",2352,MODE_MODEXA},	//CD - ROM XA Mode 2 Data(raw)
	{"CDI/2336",2336,MODE_MODECDI},	//CDI Mode 2 Data
	{"CDI/2352",2352,MODE_MODECDI}	//CDI Mode 2 Data
};

uint_32 CUE_MSF2LBA(byte M, byte S, byte F)
{
	return (((M * 60) + S) * 75) + decodeBCD8ATA(F); //75 frames per second, 60 seconds in a minute!
}

void CUE_LBA2MSF(uint_32 LBA, byte *M, byte *S, byte *F)
{
	uint_32 rest;
	rest = LBA; //Load LBA!
	*M = rest / (60 * 75); //Minute!
	rest -= *M*(60 * 75); //Rest!
	*S = rest / 75; //Second!
	rest -= *S * 75;
	*F = rest % 75; //Frame, if any!
}

//Result: -1: Out of range, 0: Failed to read, 1: Read successfully
sbyte cueimage_readsector(int device, byte M, byte S, byte F, void *buffer, word size) //Read a n-byte sector! Result=Type on success, 0 on error, -1 on not found!
{
	sbyte result=-1; //The result! Default: out of range!
	FILEPOS fsize;
	CUESHEET_STATUS cue_status;
	CUESHEET_ENTRYINFO cue_current, cue_next; //Current to check and next entries(if any)!
	char *c;
	byte file_wasescaped;
	char *file_string;
	char *file_stringstart;
	char *file_stringend;
	byte track_number_low;
	byte track_number_high;
	byte index_number;
	byte index_M;
	byte index_S;
	byte index_F;
	char *track_mode;
	CDROM_TRACK_MODE *curtrackmode;
	uint_32 LBA,prev_LBA;

	if ((device != CDROM0) && (device != CDROM1)) return 0; //Abort: invalid disk!
	if (!isext(disks[device].filename, "cue")) return 0; //Not a cue sheet!
	BIGFILE *f;
	f = emufopen64(disks[device].filename, "rb"); //Open the sheet!
	if (!f) //Not found?
	{
		return 0; //No size!
	}

	memset(&cue_status,0,sizeof(cue_status)); //Init the status!
	memset(&cue_current, 0, sizeof(cue_current)); //Init the current entry to parse!
	memset(&cue_next, 0, sizeof(cue_next)); //Init the next entry to parse!

	for (; cuesheet_readline(f);) //Read a line?
	{
		if (memcmp(&cuesheet_line_lc[0], &identifier_INDEX, safe_strlen(identifier_INDEX, sizeof(identifier_INDEX))) == 0) //File command?
		{
			if (!cue_status.got_track) continue; //If no track is specified, abort this command!
			
			//First, read the index!
			switch (cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 1)]) //Track number, high number!
			{
			case '0':
				track_number_high = 0;
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				track_number_high = cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 1)] - (byte)('1'); //Number!
				break;
			default:
				continue; //Ignore the command if incorrect!
				break;
			}
			track_mode = &cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 3)]; //Default: track mode space difference!
			switch (cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 2)]) //Track number, high number!
			{
			case '0':
				track_number_low = 0;
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				track_number_low = cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 2)] - (byte)('1'); //Number!
				break;
			case ' ': //Nothing? Single digit?
				track_number_low = track_number_high;
				track_number_high = 0;
				track_mode = &cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 2)]; //We're the start of the mode processing instead!
				break;
			default:
				continue; //Ignore the command if incorrect!
				break;
			}
			if (*track_mode != ' ') continue; //Ignore the command if incorrect!
			++track_mode; //Start of the MSF tracking!

			index_number = (track_number_high * 10) + track_number_low; //Save the index number!
											
			//Now, handle the MSF formatted text!
			//First, read the index!
			switch (*track_mode) //Track number, high number!
			{
			case '0':
				track_number_high = 0;
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				track_number_high = cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 1)] - (byte)('1'); //Number!
				break;
			default:
				continue; //Ignore the command if incorrect!
				break;
			}
			switch (*track_mode++) //Track number, high number!
			{
			case '0':
				track_number_low = 0;
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				track_number_low = cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 2)] - (byte)('1'); //Number!
				break;
			case ':': //Nothing? Single digit?
				track_number_low = track_number_high;
				track_number_high = 0;
				--track_mode; //We're the start of the mode processing instead!
				break;
			default:
				continue; //Ignore the command if incorrect!
				break;
			}
			if (*track_mode != ':') continue; //Ignore the command if incorrect!
			++track_mode; //Start of the MSF tracking!

			index_M = (track_number_high * 10) + track_number_low; //M!

//First, read the index!
			switch (*track_mode) //Track number, high number!
			{
			case '0':
				track_number_high = 0;
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				track_number_high = cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 1)] - (byte)('1'); //Number!
				break;
			default:
				continue; //Ignore the command if incorrect!
				break;
			}
			switch (*track_mode++) //Track number, high number!
			{
			case '0':
				track_number_low = 0;
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				track_number_low = cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 2)] - (byte)('1'); //Number!
				break;
			case ':': //Nothing? Single digit?
				track_number_low = track_number_high;
				track_number_high = 0;
				--track_mode; //We're the start of the mode processing instead!
				break;
			default:
				continue; //Ignore the command if incorrect!
				break;
			}
			if (*track_mode != ':') continue; //Ignore the command if incorrect!
			++track_mode; //Start of the MSF tracking!

			index_S = (track_number_high * 10) + track_number_low; //S!

//First, read the index!
			switch (*track_mode) //Track number, high number!
			{
			case '0':
				track_number_high = 0;
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				track_number_high = cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 1)] - (byte)('1'); //Number!
				break;
			default:
				continue; //Ignore the command if incorrect!
				break;
			}
			switch (*track_mode++) //Track number, high number!
			{
			case '0':
				track_number_low = 0;
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				track_number_low = cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 2)] - (byte)('1'); //Number!
				break;
			case '\0': //Nothing? Single digit?
				track_number_low = track_number_high;
				track_number_high = 0;
				--track_mode; //We're the start of the mode processing instead!
				break;
			default:
				continue; //Ignore the command if incorrect!
				break;
			}
			if (*track_mode) continue; //Ignore the command if incorrect!
			++track_mode; //Start of the MSF tracking!

			index_F = (track_number_high * 10) + track_number_low; //F!

			if (index_F > 74) continue; //Incorrect Frame!

			//Now, the full entry is ready to be parsed!
			cue_status.index = index_number; //The index!
			cue_status.M = index_M; //The Minute!
			cue_status.S = index_S; //The Second!
			cue_status.F = index_F; //The Frame!
			cue_status.got_index = 1; //The index field is filled!

			memcpy(&cue_next.status,&cue_status,MIN(sizeof(cue_next.status),sizeof(cue_status))); //Fill the index entry of cue_next with the currently loaded cue status!
			cue_next.is_present = cue_status.got_index; //Present?
			if (cue_current.is_present && cue_next.is_present) //Handle the cue_current if it and cue_next are both present!
			{
				//Fill the end locations into the current entry, based on the next entry!
				LBA = CUE_MSF2LBA(cue_next.status.M, cue_next.status.S, cue_next.status.F); //Convert to LBA!
				if (CUE_MSF2LBA(cue_current.status.M, cue_current.status.S, cue_current.status.F) >= LBA) goto finishMSFscan; //Invalid to read(non-zero length)?
				--LBA; //Take the end position of us!
				CUE_LBA2MSF(LBA, &cue_current.endM, &cue_current.endS, &cue_current.endF); //Save the calculated end position of the selected index!
				if (disks[device].selectedtrack == cue_current.status.track_number) //Current track number to lookup?
				{
					if (cue_current.status.got_file && cue_current.status.got_index) //Got file and index to lookup? Otherwise, not found!
					{
						LBA = CUE_MSF2LBA(M, S, F); //What LBA are we going to try to read!
						if (CUE_MSF2LBA(cue_current.status.M, cue_current.status.S, cue_current.status.F) > LBA) goto finishMSFscan; //Invalid? Current LBA isn't in our range(we're below it)?
						if (CUE_MSF2LBA(cue_current.endM, cue_current.endS, cue_current.endF) < LBA) goto finishMSFscan; //Invalid? Current LBA isn't in our range(we're above it)!
						if (cue_current.status.index) //Not index 0(which isn't readable)?
						{
							goto foundMSF; //We've found the location of our data!
						}
					}
				}
			}
			finishMSFscan:
			if (cue_current.is_present && cue_next.is_present) //Got current to advance?
			{
				prev_LBA = CUE_MSF2LBA(cue_current.status.M, cue_current.status.S, cue_current.status.F); //Get the current LBA position we're advancing?
				LBA = CUE_MSF2LBA(cue_next.status.M, cue_next.status.S, cue_next.status.F); //Get the current LBA position we're advancing?
				cue_next.status.datafilepos += ((LBA-prev_LBA) * cue_next.status.track_mode->sectorsize); //The physical start of the data in the file with the specified mode!
			}
			else if (cue_next.is_present) //Only next?
			{
				LBA = CUE_MSF2LBA(cue_next.status.M, cue_next.status.S, cue_next.status.F); //Get the current LBA position we're advancing?
				cue_next.status.datafilepos = (LBA * cue_next.status.track_mode->sectorsize); //The physical start of the data in the file with the specified mode!
			}
			memcpy(&cue_current, &cue_next, sizeof(cue_current)); //Set cue_current to cue_next! The next becomes the new current!
			cue_next.is_present = 0; //Set cue_next to not present!


		}
		else if (memcmp(&cuesheet_line_lc[0], &identifier_PREGAP, safe_strlen(identifier_PREGAP, sizeof(identifier_PREGAP))) == 0) //PREGAP command?
		{
			//Handle as an special entry for MSF index only! Don't count towards the file size! Only if a track is specified!
		}
		else if (memcmp(&cuesheet_line_lc[0], &identifier_POSTGAP, safe_strlen(identifier_POSTGAP, sizeof(identifier_POSTGAP))) == 0) //POSTGAP command?
		{
			//Handle as an special entry for MSF index only! Don't count towards the file size! Only if a track is specified!
		}
		else if (memcmp(&cuesheet_line_lc[0], &identifier_TRACK, safe_strlen(identifier_TRACK, sizeof(identifier_TRACK))) == 0) //Track command?
		{
			//Specify a new track and track mode to use if a file is specified!
			if (!cue_status.got_file) continue; //Ingore if no file is specified!
			if (cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK))] != ' ') continue; //Ignore if the command is incorrect!
			switch (cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 1)]) //Track number, high number!
			{
			case '0':
				track_number_high = 0;
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				track_number_high = cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 1)] - (byte)('1'); //Number!
				break;
			default:
				continue; //Ignore the command if incorrect!
				break;
			}
			track_mode = &cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 3)]; //Default: track mode space difference!
			switch (cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 2)]) //Track number, high number!
			{
			case '0':
				track_number_low = 0;
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				track_number_low = cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 2)] - (byte)('1'); //Number!
				break;
			case ' ': //Nothing? Single digit?
				track_number_low = track_number_high;
				track_number_high = 0;
				track_mode = &cuesheet_line[safe_strlen(identifier_TRACK, sizeof(identifier_TRACK) + 2)]; //We're the start of the mode processing instead!
				break;
			default:
				continue; //Ignore the command if incorrect!
				break;
			}
			if (*track_mode != ' ') continue; //Ignore the command if incorrect!
			++track_mode; //Start of the mode tracking!
			for (curtrackmode = &cdrom_track_modes[0]; curtrackmode != &cdrom_track_modes[NUMITEMS(cdrom_track_modes)];++curtrackmode) //Check all CD-ROM track modes!
			{
				if (memcmp(&curtrackmode->identifier, track_mode, safe_strlen(curtrackmode->identifier, sizeof(curtrackmode->identifier)))==0) //Track mode found?
				{
					goto trackmodefound; //We're found!
				}
			}
			trackmodefound:
			if (curtrackmode == &cdrom_track_modes[NUMITEMS(cdrom_track_modes)]) continue; //Track mode not found?
			//Fill the information into the current track information!
			cue_status.track_mode = curtrackmode; //Set the current track mode!
			cue_status.track_number = (track_number_high*10)+track_number_low; //Set the current track number!
			cue_status.got_track = 1; //Track has been parsed!
		}
		else if (memcmp(&cuesheet_line_lc[0], &identifier_FILE, safe_strlen(identifier_FILE, sizeof(identifier_FILE))) == 0) //File command?
		{
			//Specify a new file and mode to use! Also, reset the virtual position in the file!
			if (cuesheet_line[safe_strlen(identifier_FILE, sizeof(identifier_FILE))] != ' ') continue; //Ignore if the command is incorrect!
			for (c = &cuesheet_line[safe_strlen(cuesheet_line, sizeof(cuesheet_line)) - 1];;--c) //Parse backwards from End of String(EOS)!
			{
				if (*c == ' ') //Space found? Maybe found the final identifier?
				{
					if (c == &cuesheet_line[safe_strlen(identifier_FILE, sizeof(identifier_FILE))]) //First space found instead? Incorrect parameters!
					{
						continue; //Ignore the invalid command!
					}
					else goto finalspacefound_FILE; //Handle!
				}
			}
		finalspacefound_FILE: //Final space has been found?
			file_wasescaped = 0; //Default: not escaped!
			file_string = &cuesheet_line[safe_strlen(identifier_FILE, sizeof(identifier_FILE)) + 1]; //Start of the file string!
			memset(&cue_status.filename, 0, sizeof(cue_status.filename)); //Init!
			memset(&cue_status.file_type, 0, sizeof(cue_status.file_type)); //Init!
			file_stringstart = file_string; //Safe a backup copy of detecting the start of the process!
			file_stringend = c - 1; //End of the file string for detecting!
			for (; (file_string != c); ++file_string) //Process the entire file string!
			{
				if (*file_string == '"') //Possible escape?
				{
					if ((file_string == file_stringstart) || (file_string == file_stringend)) //Start or end?
					{
						if (file_string == file_stringstart)
						{
							file_wasescaped = 1; //We were escaped!
						}
						if ((!file_wasescaped) || (file_string != file_stringend)) //Not escaped or the end?
						{
							safe_scatnprintf(cue_status.filename, sizeof(cue_status.filename), "%c", *file_string); //Add to the result!
						}
					}
				}
				else //Not an escape?
				{
					safe_scatnprintf(cue_status.filename, sizeof(cue_status.filename), "%c", *file_string); //Add to the result!
				}
			}

			++c; //Skip the final space character!
			safe_scatnprintf(cue_status.file_type, sizeof(cue_status.file_type), "%s", c); //Set the file type to use!
			for (c = &cue_status.file_type[0]; c != &cue_status.file_type[safe_strlen(cue_status.file_type, sizeof(cue_status).file_type)]; ++c) //Convert to lower case!
			{
				if (*c) //Valid?
				{
					*c = tolower(*c); //Convert to lower case!
				}
			}
			cue_status.got_file = 1; //File has been parsed!
			if (cue_status.got_index) //Index was loaded? Remove the memory of the next and current indexes, as a new file has been specified!
			{
				cue_status.got_index = 0; //Default: no index anymore!
				cue_next.is_present = 0; //No next!
				cue_current.is_present = 0; //No current!
			}
			cue_status.got_track = 0; //No track has been specified for this file!
			cue_status.datafilepos = 0; //Initialize the data file position to 0!
		}
		else if (memcmp(&cuesheet_line_lc[0], &identifier_ISRC, safe_strlen(identifier_ISRC, sizeof(identifier_ISRC))) == 0) //ISRC command?
		{
			//Set the IRSC to the value, if a track is specified!
			if (!((cue_status.got_ISRC) || (cue_status.got_track == 0))) //Not gotten yet or not on a track?
			{
				if (safe_strlen(cuesheet_line, sizeof(cuesheet_line)) == (safe_strlen(identifier_ISRC, sizeof(identifier_ISRC)) + sizeof(cue_status.ISRC) + 1)) //Valid length?
				{
					memcpy(&cue_status.ISRC, &cuesheet_line[safe_strlen(identifier_ISRC, sizeof(identifier_ISRC)) + 1], sizeof(cue_status.ISRC)); //Set the value of the MCN!
					cue_status.got_ISRC = 1; //Gotten a MCN now!
				}
			}
		}
		else if (memcmp(&cuesheet_line_lc[0], &identifier_CATALOG, safe_strlen(identifier_CATALOG, sizeof(identifier_CATALOG))) == 0) //Catalog command?
		{
			//Store the MCN if nothing else if set yet!
			if (cue_status.got_MCN == 0) //Valid to use?
			{
				if (safe_strlen(cuesheet_line, sizeof(cuesheet_line)) == (safe_strlen(identifier_CATALOG, sizeof(identifier_CATALOG)) + sizeof(cue_status.MCN) + 1)) //Valid length?
				{
					memcpy(&cue_status.MCN, &cuesheet_line[safe_strlen(identifier_CATALOG, sizeof(identifier_CATALOG)) + 1], sizeof(cue_status.MCN)); //Set the value of the MCN!
					cue_status.got_MCN = 1; //Gotten a MCN now!
				}
			}
		}
		//Otherwise, ignore the unknown command!
	}

	if (cue_current.is_present && (!cue_next.is_present) && cue_current.status.got_file && cue_current.status.got_track && cue_current.status.got_index) //Handle the cue_current if it and cue_next isn't present!
	{
		if (safe_strlen(cue_current.status.file_type,sizeof(cue_current.status.file_type)) != strlen("binary")) goto finishup; //Invalid file type!
		if (!(strcmp(cue_current.status.file_type, "binary") == 0)) //Not supported file backend type!
		{
			goto finishup; //Finish up!
		}
		//Fill the end locations into the current entry, based on the next entry!
		BIGFILE *source;
		source = emufopen64(cue_current.status.filename,"rb"); //Open the backend data file!
		if (!source) goto finishup; //Couldn't open the source!
		if (emufseek64(source, 0, SEEK_END) == 0) //Went to EOF?
		{
			fsize = emuftell64(source); //What is the size of the file!
			
			memcpy(&cue_next, &cue_current, sizeof(cue_current)); //Copy the current as the next!
			++cue_next.status.index; //Take the index one up!
			LBA = CUE_MSF2LBA(cue_current.status.M, cue_current.status.S, cue_current.status.F); //Get the LBA of the last block the entry as the start of the final block!
			fsize -= cue_current.status.datafilepos; //Get the address of the last block the entry starts as!
			LBA += (fsize/cue_current.status.track_mode->sectorsize); //What LBA are we going to try to read at most!
			CUE_LBA2MSF(LBA, &cue_next.status.M, &cue_next.status.S, &cue_next.status.F); //Convert the LBA back to MSF for the fake next record based on the file size(for purposes on the final index entry going until EOF of the source file)!
		}
		else //Couldn't goto EOF?
		{
			memcpy(&cue_next, &cue_current, sizeof(cue_next)); //Duplicate the current as next, size of 0!
		}
		emufclose64(source); //Close the source!

		if (CUE_MSF2LBA(cue_current.status.M, cue_current.status.S, cue_current.status.F) >= LBA) goto finishup; //Invalid to read(non-zero length)?
		--LBA; //Take the end position of us!
		CUE_LBA2MSF(LBA, &cue_current.endM, &cue_current.endS, &cue_current.endF); //Save the calculated end position of the selected index!
		if (disks[device].selectedtrack == cue_current.status.track_number) //Current track number to lookup?
		{
			if (cue_current.status.got_file && cue_current.status.got_index) //Got file and index to lookup? Otherwise, not found!
			{
				LBA = CUE_MSF2LBA(M, S, F); //What LBA are we going to try to read!
				if (CUE_MSF2LBA(cue_current.status.M, cue_current.status.S, cue_current.status.F) > LBA) goto finishup; //Invalid? Current LBA isn't in our range(we're below it)?
				if (CUE_MSF2LBA(cue_current.endM, cue_current.endS, cue_current.endF) < LBA) goto finishup; //Invalid? Current LBA isn't in our range(we're above it)!
				if (!cue_current.status.index) goto finishup; //Not a valid index(index 0 is a pregap)!
			foundMSF: //Found the location of our data?
				if (!(strcmp(cue_current.status.file_type, "binary") == 0)) //Not supported file backend type!
				{
					goto finishup; //Finish up!
				}
				emufclose64(f); //Close the sheet, we're done with it! All data we need is loaded into cue_current!
				//Check our parameters to be valid!
				if (size != cue_current.status.track_mode->sectorsize) return 0; //Invalid sector size not matching specified!
				source = emufopen64(cue_current.status.filename, "rb"); //Open the backend data file!
				if (!source) goto finishupno_f; //Couldn't open the source!
				if (emufseek64(source, 0, SEEK_END) == 0) //Went to EOF?
				{
					size = emuftell64(source); //What is the size of the file!
				}
				if (cue_current.status.datafilepos >= size) //Past EOF?
				{
					emufclose64(source);
					return 0; //Past EOF!
				}
				if (cue_current.status.datafilepos + (((CUE_MSF2LBA(M, S, F) - CUE_MSF2LBA(cue_current.status.M, cue_current.status.S, cue_current.status.F)))*cue_current.status.track_mode->sectorsize)) //Past EOF?
				{
					emufclose64(source);
					return 0; //Past EOF!
				}
				if (cue_current.status.datafilepos + ((((CUE_MSF2LBA(M, S, F) - CUE_MSF2LBA(cue_current.status.M, cue_current.status.S, cue_current.status.F)))+1)*cue_current.status.track_mode->sectorsize)) //Past EOF?
				{
					emufclose64(source);
					return 0; //Past EOF!
				}
				if (!emufseek64(source, (cue_current.status.datafilepos + (((CUE_MSF2LBA(M, S, F) - CUE_MSF2LBA(cue_current.status.M, cue_current.status.S, cue_current.status.F)))*cue_current.status.track_mode->sectorsize)), SEEK_SET) != 0) //Past EOF?
				{
					emufclose64(source);
					return 0; //Couldn't seek to sector!
				}
				if (emufread64(buffer, size, 1, source) != size) //Failed reading the data?
				{
					emufclose64(source);
					return 0; //Couldn't read the data!
				}
				//Data has been read from the backend file!
				emufclose64(source);
				return 1; //We've found the location of our data!
			}
		}
	}

	finishup:
	emufclose64(f); //Close the sheet!

	finishupno_f: //Realy finish up!
	return result; //Failed!
}