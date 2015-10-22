#include "headers/basicio/dskimage.h" //Our header!
#include "headers/emu/directorylist.h" //isext() support!

//First, functions to actually read&write the DSK file&sectors.

byte readDSKInformation(FILE *f, DISKINFORMATIONBLOCK *result)
{
	byte ID[8] = { 'M', 'V', ' ', '-', ' ', 'C', 'P', 'C' }; //Identifier!
	fseek(f, 0, SEEK_SET); //Goto BOF!
	if (ftell(f) != 0) return 0; //Failed!
	if (fread(result, 1, sizeof(*result), f) != sizeof(*result)) return 0; //Failed!
	if (memcmp(&result->ID, &ID, sizeof(ID)) != 0) return 0; //Invalid file type!
	return 1; //OK!
}

byte readDSKTrackInformation(FILE *f, byte side, byte track, DISKINFORMATIONBLOCK *info, TRACKINFORMATIONBLOCK *result)
{
	uint_32 position;
	word actualtracknr;
	actualtracknr = track;
	actualtracknr *= info->NumberOfSides; //Multiply by the number of sides!
	actualtracknr += side; //Add the side to the item to retrieve! This creates interleaved sides!
	position = sizeof(*info) + (info->TrackSize*actualtracknr);
	fseek(f, position, SEEK_SET); //Goto position of the track!
	if (ftell(f) != position) return 0; //Invalid track number!
	if (fread(result, 1, sizeof(*result), f) != sizeof(*result)) return 0; //Failed!
	if ((result->sidenumber != side) || (result->tracknumber != track)) return 0; //Failed: invalid side/track retrieved!
	return 1; //OK!
}

byte readDSKSectorInformation(FILE *f, byte side, word track, byte sector, DISKINFORMATIONBLOCK *info, TRACKINFORMATIONBLOCK *trackinfo, SECTORINFORMATIONBLOCK *result)
{
	uint_32 position;
	word actualtracknr;
	actualtracknr = track;
	actualtracknr *= info->NumberOfSides; //Multiply by the number of sides!
	actualtracknr += side; //Add the side to the item to retrieve! This creates interleaved sides!
	position = sizeof(*info) + (info->TrackSize*actualtracknr);
	position += sizeof(*trackinfo); //We follow directly after the track information!
	position += sizeof(*result)*sector; //Add to apply the sector number!
	fseek(f, position, SEEK_SET); //Goto position of the track!
	if (ftell(f) != position) return 0; //Invalid track number!
	if (fread(result, 1, sizeof(*result), f) != sizeof(*result)) return 0; //Failed!
	if ((result->side != side) || (result->track != track)) return 0; //Failed: invalid side/track retrieved!
	return 1; //OK!
}

word getDSKSectorBlockSize(TRACKINFORMATIONBLOCK *trackinfo)
{
	return (word)powf((long)2, (float)trackinfo->sectorsize); //Apply sector size!
}

word getDSKSectorSize(SECTORINFORMATIONBLOCK *sectorinfo)
{
	return (word)powf((long)2, (float)sectorinfo->SectorSize); //Apply sector size!
}

byte readDSKSector(FILE *f, byte side, word track, byte sector, DISKINFORMATIONBLOCK *info, TRACKINFORMATIONBLOCK *trackinfo, SECTORINFORMATIONBLOCK *sectorinfo, byte sectorsize, void *result)
{
	if (sectorinfo->SectorSize != sectorsize) return 0; //Wrong sector size!
	uint_32 position;
	word actualtracknr;
	actualtracknr = track;
	actualtracknr *= info->NumberOfSides; //Multiply by the number of sides!
	actualtracknr += side; //Add the side to the item to retrieve! This creates interleaved sides!
	position = sizeof(*info) + (info->TrackSize*actualtracknr);
	position += 100; //We always start 100 bytes after the track information block start!
	position += getDSKSectorBlockSize(trackinfo)*sector; //The start of the sector!
	fseek(f, position, SEEK_SET); //Goto position of the track!
	if (ftell(f) != position) return 0; //Invalid track number!
	if (fread(result, 1, getDSKSectorSize(sectorinfo), f) != getDSKSectorSize(sectorinfo)) return 0; //Failed!
	return 1; //Read!
}

byte writeDSKSector(FILE *f, byte side, word track, byte sector, DISKINFORMATIONBLOCK *info, TRACKINFORMATIONBLOCK *trackinfo, SECTORINFORMATIONBLOCK *sectorinfo, byte sectorsize, void *sectordata)
{
	if (sectorinfo->SectorSize != sectorsize) return 0; //Wrong sector size!
	uint_32 position;
	word actualtracknr;
	actualtracknr = track;
	actualtracknr *= info->NumberOfSides; //Multiply by the number of sides!
	actualtracknr += side; //Add the side to the item to retrieve! This creates interleaved sides!
	position = sizeof(*info) + (info->TrackSize*actualtracknr);
	position += 100; //We always start 100 bytes after the track information block start!
	position += getDSKSectorBlockSize(trackinfo)*sector; //The start of the sector!
	fseek(f, position, SEEK_SET); //Goto position of the track!
	if (ftell(f) != position) return 0; //Invalid track number!
	if (fwrite(sectordata, 1, getDSKSectorSize(sectorinfo), f) != getDSKSectorSize(sectorinfo)) return 0; //Failed!
	return 1; //Read!
}

//Our interfaced functions!

byte is_DSKimage(char *filename)
{
	if (strcmp(filename, "") == 0) return 0; //Unexisting: don't even look at it!
	if (!isext(filename, "dsk")) //Not our DSK image file?
	{
		return 0; //Not a dynamic image!
	}
	FILE *f;
	f = fopen(filename, "rb"); //Open the image!
	if (!f) return 0; //Not opened!
	DISKINFORMATIONBLOCK DSKInformation; //The read information!
	if (!readDSKInformation(f, &DSKInformation)) //Invalid header?
	{
		fclose(f); //Close the image!
		return 0; //Not a valid DSK file!
	}
	fclose(f); //Close the image!
	return 1; //Valid DSK file!
}

byte readDSKSectorInfo(char *filename, byte side, byte track, byte sector, SECTORINFORMATIONBLOCK *result)
{
	FILE *f;
	f = fopen(filename, "rb"); //Open the image!
	if (!f) return 0; //Not opened!
	DISKINFORMATIONBLOCK DSKInformation;
	TRACKINFORMATIONBLOCK TrackInformation;
	if (!readDSKInformation(f, &DSKInformation)) //Invalid header?
	{
		fclose(f); //Close the image!
		return 0; //Not a valid DSK file!
	}
	if (!readDSKTrackInformation(f, side, track, &DSKInformation, &TrackInformation)) //Invalid track?
	{
		fclose(f); //Close the image!
		return 0; //Not a valid DSK Track!
	}
	if (!readDSKSectorInformation(f, side, track, sector, &DSKInformation, &TrackInformation, result)) //Invalid sector?
	{
		fclose(f); //Close the image!
		return 0; //Not a valid DSK Sector!
	}
	fclose(f);
	return 1; //We have retrieved the sector information!
}

byte readDSKSectorData(char *filename, byte side, byte track, byte sector, byte sectorsize, void *result)
{
	FILE *f;
	f = fopen(filename, "rb"); //Open the image!
	if (!f) return 0; //Not opened!
	DISKINFORMATIONBLOCK DSKInformation;
	TRACKINFORMATIONBLOCK TrackInformation;
	SECTORINFORMATIONBLOCK SectorInformation;
	if (!readDSKInformation(f, &DSKInformation)) //Invalid header?
	{
		fclose(f); //Close the image!
		return 0; //Not a valid DSK file!
	}
	if (!readDSKTrackInformation(f, side, track, &DSKInformation, &TrackInformation)) //Invalid track?
	{
		fclose(f); //Close the image!
		return 0; //Not a valid DSK Track!
	}
	if (!readDSKSectorInformation(f, side, track, sector, &DSKInformation, &TrackInformation, &SectorInformation)) //Invalid sector?
	{
		fclose(f); //Close the image!
		return 0; //Not a valid DSK Sector!
	}
	if (!readDSKSector(f, side, track, sector, &DSKInformation, &TrackInformation, &SectorInformation,sectorsize, result)) //Failed reading the sector?
	{
		fclose(f); //Close the image!
		return 0; //Invalid DSK data!
	}
	//Sector has been read, give the valid result!
	fclose(f);
	return 1; //We have retrieved the sector information!
}

byte writeDSKSectorData(char *filename, byte side, byte track, byte sector, byte sectorsize, void *sectordata)
{
	FILE *f;
	f = fopen(filename, "rb+"); //Open the image!
	if (!f) return 0; //Not opened!
	DISKINFORMATIONBLOCK DSKInformation;
	TRACKINFORMATIONBLOCK TrackInformation;
	SECTORINFORMATIONBLOCK SectorInformation;
	if (!readDSKInformation(f, &DSKInformation)) //Invalid header?
	{
		fclose(f); //Close the image!
		return 0; //Not a valid DSK file!
	}
	if (!readDSKTrackInformation(f, side, track, &DSKInformation, &TrackInformation)) //Invalid track?
	{
		fclose(f); //Close the image!
		return 0; //Not a valid DSK Track!
	}
	if (!readDSKSectorInformation(f, side, track, sector, &DSKInformation, &TrackInformation, &SectorInformation)) //Invalid sector?
	{
		fclose(f); //Close the image!
		return 0; //Not a valid DSK Sector!
	}
	if (!writeDSKSector(f, side, track, sector, &DSKInformation, &TrackInformation, &SectorInformation, sectorsize, sectordata)) //Failed writing the sector?
	{
		fclose(f); //Close the image!
		return 0; //Invalid DSK data!
	}
	//Sector has been read, give the valid result!
	fclose(f);
	return 1; //We have retrieved the sector information!
}

byte readDSKInfo(char *filename, DISKINFORMATIONBLOCK *result)
{
	FILE *f;
	f = fopen(filename, "rb"); //Open the image!
	if (!f) return 0; //Not opened!
	if (!readDSKInformation(f, result)) //Invalid header?
	{
		fclose(f); //Close the image!
		return 0; //Not a valid DSK file!
	}
	fclose(f); //Close the image!
	return 1; //Valid DSK file!
}

byte readDSKTrackInfo(char *filename, byte side, byte track, TRACKINFORMATIONBLOCK *result)
{
	FILE *f;
	f = fopen(filename, "rb+"); //Open the image!
	if (!f) return 0; //Not opened!
	DISKINFORMATIONBLOCK DSKInformation;
	if (!readDSKInformation(f, &DSKInformation)) //Invalid header?
	{
		fclose(f); //Close the image!
		return 0; //Not a valid DSK file!
	}
	if (!readDSKTrackInformation(f, side, track, &DSKInformation, result)) //Invalid track?
	{
		fclose(f); //Close the image!
		return 0; //Not a valid DSK Track!
	}
	fclose(f); //Close the image!
	return 1; //Valid DSK Track!
}