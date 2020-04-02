#include "headers/basicio/imdimage.h" //Our own header!
#include "headers/fopen64.h" //64-bit fopen support!
#include "headers/support/zalloc.h" //Memory allocation support!
#include "headers/emu/directorylist.h"

#ifdef IS_PSP
#define SDL_SwapLE16(x) (x)
#endif

//One sector information block per sector.
#include "headers/packed.h" //PACKED support!
typedef struct PACKED
{
	byte mode; //0-5
	byte cylinder; //0-n
	byte head_extrabits; //Bit0=Head. Bit 6=Sector Head map present. Bit 7=Sector cylinder map present.
	byte sectorspertrack; //1+
	byte SectorSize; //128*(2^SectorSize)=Size. 0-6=Normal sector size. 0xFF=Table of 16-bit sector sizes with the sizes before the data records.
} TRACKINFORMATIONBLOCK;
#include "headers/endpacked.h" //End packed!

//Sector size to bytes translation!
#define SECTORSIZE_BYTES(SectorSize) (128<<(SectorSize))

//Head bits
#define IMD_HEAD_HEADNUMBER 0x01
#define IMD_HEAD_HEADMAPPRESENT 0x40
#define IMD_HEAD_CYLINDERMAPPRESENT 0x80

//Sector size reserved value
#define IMD_SECTORSIZE_SECTORSIZEMAPPRESENT 0xFF


//File structure:
/*

ASCII header, 0x1A terminated.
For each track:
1. TRACKINFORMATIONBLOCK
2. Sector numbering map
3. Sector cylinder map(optional)
4. Sector head map(optional)
5. Sector size map(optional)
6. Sector data records

Each data record starts with a ID, followed by one byte for compressed records(even number), nothing when zero(unavailable) or otherwise the full sector size. Valid when less than 9.

*/

byte is_IMDimage(char* filename) //Are we a IMD image?
{
	byte identifier[3];
	if (strcmp(filename, "") == 0) return 0; //Unexisting: don't even look at it!
	if (!isext(filename, "imd")) //Not our IMD image file?
	{
		return 0; //Not a IMD image!
	}
	BIGFILE* f;
	f = emufopen64(filename, "rb"); //Open the image!
	if (!f) return 0; //Not opened!
	if (emufread64(&identifier, 1, sizeof(identifier), f) != sizeof(identifier)) //Try to read the header?
	{
		emufclose64(f); //Close it!
		return 0; //Not a IMD image!
	}
	if ((identifier[0] != 'I') || (identifier[1] != 'M') || (identifier[2] != 'D')) //Invalid header?
	{
		emufclose64(f); //Close it!
		return 0; //Not a IMD image!
	}
	for (; !emufeof64(f);) //Not EOF yet?
	{
		if (emufread64(&identifier[0], 1, sizeof(identifier[0]), f) != sizeof(identifier[0])) //Failed to read an header comment byte?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (identifier[0] == 0x1A) //Finished comment identifier?
		{
			emufclose64(f); //Close the image!
			return 1; //Valid IMD file with a valid comment!
		}
	}
	//Reached EOF without comment finishing?

	emufclose64(f); //Close the image!
	return 0; //Invalid IMD file!
}

byte readIMDSectorInfo(char* filename, byte side, byte track, byte sector, IMDIMAGE_SECTORINFO* result)
{
	byte physicalsectornr;
	byte physicalheadnr;
	byte physicalcylindernr;
	word physicalsectorsize; //Effective sector size!
	word sectornumber;
	word* sectorsizemap; //Sector size map!
	uint_32 datarecordnumber;
	TRACKINFORMATIONBLOCK trackinfo;
	word trackskipleft;
	byte identifier[3];
	byte data;
	if (strcmp(filename, "") == 0) return 0; //Unexisting: don't even look at it!
	if (!isext(filename, "imd")) //Not our IMD image file?
	{
		return 0; //Not a IMD image!
	}
	BIGFILE* f;
	f = emufopen64(filename, "rb"); //Open the image!
	if (!f) return 0; //Not opened!
	if (emufread64(&identifier, 1, sizeof(identifier), f) != sizeof(identifier)) //Try to read the header?
	{
		emufclose64(f); //Close it!
		return 0; //Not a IMD image!
	}
	if ((identifier[0] != 'I') || (identifier[1] != 'M') || (identifier[2] != 'D')) //Invalid header?
	{
		emufclose64(f); //Close it!
		return 0; //Not a IMD image!
	}
	for (; !emufeof64(f);) //Not EOF yet?
	{
		if (emufread64(&identifier[0], 1, sizeof(identifier[0]), f) != sizeof(identifier[0])) //Failed to read an header comment byte?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (identifier[0] == 0x1A) //Finished comment identifier?
		{
			goto validIMDheaderInfo; //Header is validly read!
		}
	}
	//Reached EOF without comment finishing?

	emufclose64(f); //Close the image!
	return 0; //Invalid IMD file!

validIMDheaderInfo:
	//Now, skip tracks until we reach the selected track!
	trackskipleft = track; //How many tracks to skip!
	for (; trackskipleft;) //Skipping left?
	{
		if (emufread64(&trackinfo, 1, sizeof(trackinfo), f) != sizeof(trackinfo)) //Failed to read track info?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		//Track info block has been read!
		if (emufseek64(f, trackinfo.sectorspertrack, SEEK_CUR) < 0) //Skip the sector number map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (trackinfo.head_extrabits & IMD_HEAD_CYLINDERMAPPRESENT) //Cylinder map following?
		{
			if (emufseek64(f, trackinfo.sectorspertrack, SEEK_CUR) < 0) //Skip the cylinder number map!
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
		}
		if (trackinfo.head_extrabits & IMD_HEAD_HEADMAPPRESENT) //Head map following?
		{
			if (emufseek64(f, trackinfo.sectorspertrack, SEEK_CUR) < 0) //Skip the head number map!
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
		}
		if (trackinfo.SectorSize == IMD_SECTORSIZE_SECTORSIZEMAPPRESENT) //Sector size map following?
		{
			sectorsizemap = zalloc((trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP", NULL); //Allocate a sector map to use!
			if (!sectorsizemap) //Failed to allocate?
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			if (emufread64(sectorsizemap, 1, (trackinfo.sectorspertrack<<1), f) < 0) //Read the sector size map!
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			sectornumber = 0;
			for (sectornumber = 0; sectornumber < trackinfo.sectorspertrack; ++sectornumber) //Patch as needed!
			{
				sectorsizemap[sectornumber] = SDL_SwapLE16(sectorsizemap[sectornumber]); //Swap all byte ordering to be readable!
			}
		}
		datarecordnumber = trackinfo.sectorspertrack; //How many records to read!
		sectornumber = 0; //Start at the first sector number!
		for (;datarecordnumber;) //Process all sectors on the track!
		{
			if (emufread64(&data, 1, sizeof(data), f)!=sizeof(data)) //Read the identifier!
			{
				freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			//Now, we have the identifier!
			if (data) //Not one that's unavailable?
			{
				if (data > 8) //Undefined value?
				{
					freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
					emufclose64(f); //Close the image!
					return 0; //Invalid IMD file!
				}
				if (data & 1) //Normal sector with or without mark, data error or deleted?
				{
					//Skip the sector's data!
					if (sectorsizemap) //Map used?
					{
						if (emufseek64(f, (sectorsizemap[sectornumber]), SEEK_CUR) < 0) //Errored out?
						{
							freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
							emufclose64(f); //Close the image!
							return 0; //Invalid IMD file!
						}
					}
					else
					{
						if (emufseek64(f, SECTORSIZE_BYTES(trackinfo.SectorSize), SEEK_CUR) < 0) //Errored out?
						{
							freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
							emufclose64(f); //Close the image!
							return 0; //Invalid IMD file!
						}
					}
				}
				else //Compressed?
				{
					if (emufseek64(f, 1, SEEK_CUR)<0) //Skip the compressed data!
					{
						if (sectorsizemap) //Allocated sector size map?
						{
							freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
						}
						emufclose64(f); //Close the image!
						return 0; //Invalid IMD file!
					}
				}
			}
			freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
			++sectornumber; //Process the next sector number!
			--datarecordnumber; //Processed!
		}
	}

	//Now, we're at the specified track!
	if (emufread64(&trackinfo, 1, sizeof(trackinfo), f) != sizeof(trackinfo)) //Failed to read track info?
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	//Track info block has been read!
	if (sector >= trackinfo.sectorspertrack) //Not enough to read?
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	if (emufseek64(f, sector, SEEK_CUR) < 0) //Skip the sector number map!
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	if (emufread64(&physicalsectornr, 1, sizeof(physicalsectornr), f)!=sizeof(physicalsectornr)) //Read the actual sector number!
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	if (emufseek64(f, trackinfo.sectorspertrack-sector, SEEK_CUR) < 0) //Skip the sector number map!
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	physicalcylindernr = (trackinfo.cylinder); //Default cylinder number!
	if (trackinfo.head_extrabits & IMD_HEAD_CYLINDERMAPPRESENT) //Cylinder map following?
	{
		if (emufseek64(f, sector, SEEK_CUR) < 0) //Skip the cylinder number map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (emufread64(&physicalcylindernr,1,sizeof(physicalcylindernr), f)!=sizeof(physicalcylindernr)) //Read the cylinder number!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (emufseek64(f, trackinfo.sectorspertrack - sector, SEEK_CUR) < 0) //Skip the cylinder number map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
	}
	physicalheadnr = (trackinfo.head_extrabits & IMD_HEAD_HEADNUMBER); //Default head number!
	if (trackinfo.head_extrabits & IMD_HEAD_HEADMAPPRESENT) //Head map following?
	{
		if (emufseek64(f, sector, SEEK_CUR) < 0) //Skip the head number map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (emufread64(&physicalheadnr, 1, sizeof(physicalheadnr), f) != sizeof(physicalheadnr)) //Read the cylinder number!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (emufseek64(f, trackinfo.sectorspertrack - sector, SEEK_CUR) < 0) //Skip the head number map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
	}
	if (trackinfo.SectorSize == IMD_SECTORSIZE_SECTORSIZEMAPPRESENT) //Sector size map following?
	{
		sectorsizemap = zalloc((trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP", NULL); //Allocate a sector map to use!
		if (!sectorsizemap) //Failed to allocate?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (emufread64(sectorsizemap, 1, (trackinfo.sectorspertrack << 1), f) < 0) //Read the sector size map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		sectornumber = 0;
		for (sectornumber = 0; sectornumber < trackinfo.sectorspertrack; ++sectornumber) //Patch as needed!
		{
			sectorsizemap[sectornumber] = SDL_SwapLE16(sectorsizemap[sectornumber]); //Swap all byte ordering to be readable!
		}
	}

	//Skip n sectors!
	datarecordnumber = MIN(trackinfo.sectorspertrack,sector); //How many records to read!
	sectornumber = 0; //Start at the first sector number!
	for (; datarecordnumber;) //Process all sectors on the track!
	{
		if (emufread64(&data, 1, sizeof(data), f) != sizeof(data)) //Read the identifier!
		{
			if (sectorsizemap) //Allocated sector size map?
			{
				freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
			}
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		//Now, we have the identifier!
		if (data) //Not one that's unavailable?
		{
			if (data > 8) //Undefined value?
			{
				if (sectorsizemap) //Allocated sector size map?
				{
					freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
				}
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			if (data & 1) //Normal sector with or without mark, data error or deleted?
			{
				//Skip the sector's data!
				if (sectorsizemap) //Map used?
				{
					if (emufseek64(f, (sectorsizemap[sectornumber]), SEEK_CUR) < 0) //Errored out?
					{
						if (sectorsizemap) //Allocated sector size map?
						{
							freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
						}
						emufclose64(f); //Close the image!
						return 0; //Invalid IMD file!
					}
				}
				else
				{
					if (emufseek64(f, SECTORSIZE_BYTES(trackinfo.SectorSize), SEEK_CUR) < 0) //Errored out?
					{
						if (sectorsizemap) //Allocated sector size map?
						{
							freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
						}
						emufclose64(f); //Close the image!
						return 0; //Invalid IMD file!
					}
				}
			}
			else //Compressed?
			{
				if (emufseek64(f, 1, SEEK_CUR) < 0) //Skip the compressed data!
				{
					if (sectorsizemap) //Allocated sector size map?
					{
						freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
					}
					emufclose64(f); //Close the image!
					return 0; //Invalid IMD file!
				}
			}
		}
		++sectornumber; //Process the next sector number!
		--datarecordnumber; //Processed!
	}

	if (datarecordnumber) //Left to read? Reached the sector!
	{
		if (emufread64(&data, 1, sizeof(data), f) != sizeof(data)) //Read the identifier!
		{
			if (sectorsizemap) //Allocated sector size map?
			{
				freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
			}
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		//Now, we have the identifier!

		if (data > 8) //Undefined value?
		{
		invalidsectordata:
			if (sectorsizemap) //Allocated sector size map?
			{
				freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
			}
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (data) //Not one that's unavailable?
		{
			if (data & 1) //Normal sector with or without mark, data error or deleted?
			{
				//Skip the sector's data!
				if (sectorsizemap) //Map used?
				{
					//Check if we're valid!
					if (emufseek64(f, sectorsizemap[sectornumber], SEEK_CUR) < 0) //Failed to skip the data part of the sector?
					{
						goto invalidsectordata; //Error out!
					}
					physicalsectorsize = sectorsizemap[sectornumber]; //Physical sector size!
					//The sector is loaded and valid!
				sectorready:
					//Fill up the sector information block!
					result->cylinderID = physicalcylindernr; //Physical cylinder number!
					result->headnumber = physicalheadnr; //Physical head number!
					result->sectorID = physicalsectornr; //Physical sector number!
					result->MFM_speedmode = trackinfo.mode; //The mode!
					result->sectorsize = physicalsectorsize; //Physical sector size!
					result->totalsectors = trackinfo.sectorspertrack; //How many sectors are on this track!
					result->datamark = ((data - 1) >> 1); //What is it marked as!
					sectorready_unreadable:

					//Finish up!
					if (sectorsizemap) //Allocated sector size map?
					{
						freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
					}
					emufclose64(f); //Close the image!
					return 1; //Gotten information about the record!
				}
				else
				{
					if (emufseek64(f, SECTORSIZE_BYTES(trackinfo.SectorSize), SEEK_CUR) < 0) //Errored out?
					{
						goto invalidsectordata;
					}
					physicalsectorsize = SECTORSIZE_BYTES(trackinfo.SectorSize); //How large are we physically!

					goto sectorready; //Handle ready sector information!
				}
			}
			else //Compressed?
			{
				if (emufseek64(f, 1, SEEK_CUR) < 0) //Skip the compressed data!
				{
					if (sectorsizemap) //Allocated sector size map?
					{
						freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
					}
					emufclose64(f); //Close the image!
					return 0; //Invalid IMD file!
				}
				if (sectorsizemap) //Map used?
				{
					physicalsectorsize = sectorsizemap[sectornumber]; //Errored out?
				}
				else
				{
					physicalsectorsize = SECTORSIZE_BYTES(trackinfo.SectorSize); //Errored out?
				}
				goto sectorready;
			}
		}
		else //Unreadable sector?
		{
			result->cylinderID = physicalcylindernr; //Physical cylinder number!
			result->headnumber = physicalheadnr; //Physical head number!
			result->sectorID = physicalsectornr; //Physical sector number!
			result->MFM_speedmode = trackinfo.mode; //The mode!
			result->sectorsize = 0; //Physical sector size!
			result->totalsectors = trackinfo.sectorspertrack; //How many sectors are on this track!
			result->datamark = DATAMARK_INVALID; //What is it marked as!
			goto sectorready_unreadable; //Handle the unreadable sector result!
		}
	}

	//Couldn't read it!

	if (sectorsizemap) //Allocated sector size map?
	{
		freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
	}
	emufclose64(f); //Close the image!
	return 0; //Invalid IMD file!
}


byte readIMDSector(char* filename, byte side, byte track, byte sector, word sectorsize, void* result)
{
	byte filldata;
	byte physicalsectornr;
	byte physicalheadnr;
	byte physicalcylindernr;
	word physicalsectorsize; //Effective sector size!
	word sectornumber;
	word* sectorsizemap; //Sector size map!
	uint_32 datarecordnumber;
	TRACKINFORMATIONBLOCK trackinfo;
	word trackskipleft;
	byte identifier[3];
	byte data;
	if (strcmp(filename, "") == 0) return 0; //Unexisting: don't even look at it!
	if (!isext(filename, "imd")) //Not our IMD image file?
	{
		return 0; //Not a IMD image!
	}
	BIGFILE* f;
	f = emufopen64(filename, "rb"); //Open the image!
	if (!f) return 0; //Not opened!
	if (emufread64(&identifier, 1, sizeof(identifier), f) != sizeof(identifier)) //Try to read the header?
	{
		emufclose64(f); //Close it!
		return 0; //Not a IMD image!
	}
	if ((identifier[0] != 'I') || (identifier[1] != 'M') || (identifier[2] != 'D')) //Invalid header?
	{
		emufclose64(f); //Close it!
		return 0; //Not a IMD image!
	}
	for (; !emufeof64(f);) //Not EOF yet?
	{
		if (emufread64(&identifier[0], 1, sizeof(identifier[0]), f) != sizeof(identifier[0])) //Failed to read an header comment byte?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (identifier[0] == 0x1A) //Finished comment identifier?
		{
			goto validIMDheaderRead; //Header is validly read!
		}
	}
	//Reached EOF without comment finishing?

	emufclose64(f); //Close the image!
	return 0; //Invalid IMD file!

validIMDheaderRead:
	//Now, skip tracks until we reach the selected track!
	trackskipleft = track; //How many tracks to skip!
	for (; trackskipleft;) //Skipping left?
	{
		if (emufread64(&trackinfo, 1, sizeof(trackinfo), f) != sizeof(trackinfo)) //Failed to read track info?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		//Track info block has been read!
		if (emufseek64(f, trackinfo.sectorspertrack, SEEK_CUR) < 0) //Skip the sector number map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (trackinfo.head_extrabits & IMD_HEAD_CYLINDERMAPPRESENT) //Cylinder map following?
		{
			if (emufseek64(f, trackinfo.sectorspertrack, SEEK_CUR) < 0) //Skip the cylinder number map!
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
		}
		if (trackinfo.head_extrabits & IMD_HEAD_HEADMAPPRESENT) //Head map following?
		{
			if (emufseek64(f, trackinfo.sectorspertrack, SEEK_CUR) < 0) //Skip the head number map!
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
		}
		if (trackinfo.SectorSize == IMD_SECTORSIZE_SECTORSIZEMAPPRESENT) //Sector size map following?
		{
			sectorsizemap = zalloc((trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP", NULL); //Allocate a sector map to use!
			if (!sectorsizemap) //Failed to allocate?
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			if (emufread64(sectorsizemap, 1, (trackinfo.sectorspertrack << 1), f) < 0) //Read the sector size map!
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			sectornumber = 0;
			for (sectornumber = 0; sectornumber < trackinfo.sectorspertrack; ++sectornumber) //Patch as needed!
			{
				sectorsizemap[sectornumber] = SDL_SwapLE16(sectorsizemap[sectornumber]); //Swap all byte ordering to be readable!
			}
		}
		datarecordnumber = trackinfo.sectorspertrack; //How many records to read!
		sectornumber = 0; //Start at the first sector number!
		for (; datarecordnumber;) //Process all sectors on the track!
		{
			if (emufread64(&data, 1, sizeof(data), f) != sizeof(data)) //Read the identifier!
			{
				freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			//Now, we have the identifier!
			if (data) //Not one that's unavailable?
			{
				if (data > 8) //Undefined value?
				{
					freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
					emufclose64(f); //Close the image!
					return 0; //Invalid IMD file!
				}
				if (data & 1) //Normal sector with or without mark, data error or deleted?
				{
					//Skip the sector's data!
					if (sectorsizemap) //Map used?
					{
						if (emufseek64(f, (sectorsizemap[sectornumber]), SEEK_CUR) < 0) //Errored out?
						{
							freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
							emufclose64(f); //Close the image!
							return 0; //Invalid IMD file!
						}
					}
					else
					{
						if (emufseek64(f, SECTORSIZE_BYTES(trackinfo.SectorSize), SEEK_CUR) < 0) //Errored out?
						{
							freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
							emufclose64(f); //Close the image!
							return 0; //Invalid IMD file!
						}
					}
				}
				else //Compressed?
				{
					if (emufseek64(f, 1, SEEK_CUR) < 0) //Skip the compressed data!
					{
						if (sectorsizemap) //Allocated sector size map?
						{
							freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
						}
						emufclose64(f); //Close the image!
						return 0; //Invalid IMD file!
					}
				}
			}
			freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
			++sectornumber; //Process the next sector number!
			--datarecordnumber; //Processed!
		}
	}

	//Now, we're at the specified track!
	if (emufread64(&trackinfo, 1, sizeof(trackinfo), f) != sizeof(trackinfo)) //Failed to read track info?
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	//Track info block has been read!
	if (sector >= trackinfo.sectorspertrack) //Not enough to read?
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	if (emufseek64(f, sector, SEEK_CUR) < 0) //Skip the sector number map!
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	if (emufread64(&physicalsectornr, 1, sizeof(physicalsectornr), f) != sizeof(physicalsectornr)) //Read the actual sector number!
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	if (emufseek64(f, trackinfo.sectorspertrack - sector, SEEK_CUR) < 0) //Skip the sector number map!
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	physicalcylindernr = (trackinfo.cylinder); //Default cylinder number!
	if (trackinfo.head_extrabits & IMD_HEAD_CYLINDERMAPPRESENT) //Cylinder map following?
	{
		if (emufseek64(f, sector, SEEK_CUR) < 0) //Skip the cylinder number map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (emufread64(&physicalcylindernr, 1, sizeof(physicalcylindernr), f) != sizeof(physicalcylindernr)) //Read the cylinder number!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (emufseek64(f, trackinfo.sectorspertrack - sector, SEEK_CUR) < 0) //Skip the cylinder number map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
	}
	physicalheadnr = (trackinfo.head_extrabits & IMD_HEAD_HEADNUMBER); //Default head number!
	if (trackinfo.head_extrabits & IMD_HEAD_HEADMAPPRESENT) //Head map following?
	{
		if (emufseek64(f, sector, SEEK_CUR) < 0) //Skip the head number map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (emufread64(&physicalheadnr, 1, sizeof(physicalheadnr), f) != sizeof(physicalheadnr)) //Read the cylinder number!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (emufseek64(f, trackinfo.sectorspertrack - sector, SEEK_CUR) < 0) //Skip the head number map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
	}
	if (trackinfo.SectorSize == IMD_SECTORSIZE_SECTORSIZEMAPPRESENT) //Sector size map following?
	{
		sectorsizemap = zalloc((trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP", NULL); //Allocate a sector map to use!
		if (!sectorsizemap) //Failed to allocate?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (emufread64(sectorsizemap, 1, (trackinfo.sectorspertrack << 1), f) < 0) //Read the sector size map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		sectornumber = 0;
		for (sectornumber = 0; sectornumber < trackinfo.sectorspertrack; ++sectornumber) //Patch as needed!
		{
			sectorsizemap[sectornumber] = SDL_SwapLE16(sectorsizemap[sectornumber]); //Swap all byte ordering to be readable!
		}
	}

	//Skip n sectors!
	datarecordnumber = MIN(trackinfo.sectorspertrack, sector); //How many records to read!
	sectornumber = 0; //Start at the first sector number!
	for (; datarecordnumber;) //Process all sectors on the track!
	{
		if (emufread64(&data, 1, sizeof(data), f) != sizeof(data)) //Read the identifier!
		{
			if (sectorsizemap) //Allocated sector size map?
			{
				freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
			}
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		//Now, we have the identifier!
		if (data) //Not one that's unavailable?
		{
			if (data > 8) //Undefined value?
			{
				if (sectorsizemap) //Allocated sector size map?
				{
					freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
				}
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			if (data & 1) //Normal sector with or without mark, data error or deleted?
			{
				//Skip the sector's data!
				if (sectorsizemap) //Map used?
				{
					if (emufseek64(f, (sectorsizemap[sectornumber]), SEEK_CUR) < 0) //Errored out?
					{
						if (sectorsizemap) //Allocated sector size map?
						{
							freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
						}
						emufclose64(f); //Close the image!
						return 0; //Invalid IMD file!
					}
				}
				else
				{
					if (emufseek64(f, SECTORSIZE_BYTES(trackinfo.SectorSize), SEEK_CUR) < 0) //Errored out?
					{
						if (sectorsizemap) //Allocated sector size map?
						{
							freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
						}
						emufclose64(f); //Close the image!
						return 0; //Invalid IMD file!
					}
				}
			}
			else //Compressed?
			{
				if (emufseek64(f, 1, SEEK_CUR) < 0) //Skip the compressed data!
				{
					if (sectorsizemap) //Allocated sector size map?
					{
						freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
					}
					emufclose64(f); //Close the image!
					return 0; //Invalid IMD file!
				}
			}
		}
		++sectornumber; //Process the next sector number!
		--datarecordnumber; //Processed!
	}

	if (datarecordnumber) //Left to read? Reached the sector!
	{
		if (emufread64(&data, 1, sizeof(data), f) != sizeof(data)) //Read the identifier!
		{
			if (sectorsizemap) //Allocated sector size map?
			{
				freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
			}
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		//Now, we have the identifier!
		if (data) //Not one that's unavailable?
		{
			if (data > 8) //Undefined value?
			{
			invalidsectordata:
				if (sectorsizemap) //Allocated sector size map?
				{
					freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
				}
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			if (data & 1) //Normal sector with or without mark, data error or deleted?
			{
				//Skip the sector's data!
				if (sectorsizemap) //Map used?
				{
					if (sectorsizemap[sectornumber] != sectorsize) //Sector size mismatch?
					{
						goto invalidsectordata; //Error out!
					}
					//Check if we're valid!
					if (emufread64(result,1,sectorsizemap[sectornumber],f)!=sectorsizemap[sectornumber]) //Failed to skip the data part of the sector?
					{
						goto invalidsectordata; //Error out!
					}
					physicalsectorsize = sectorsizemap[sectornumber]; //Physical sector size!
					//The sector is loaded and valid!
				sectorready:
					//Fill up the sector information block!
					/*
					result->cylinderID = physicalcylindernr; //Physical cylinder number!
					result->headnumber = physicalheadnr; //Physical head number!
					result->sectorID = physicalsectornr; //Physical sector number!
					result->MFM_speedmode = trackinfo.mode; //The mode!
					result->sectorsize = physicalsectorsize; //Physical sector size!
					*/

					//Finish up!
					if (sectorsizemap) //Allocated sector size map?
					{
						freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
					}
					emufclose64(f); //Close the image!
					return 1; //Gotten information about the record!
				}
				else
				{
					if (SECTORSIZE_BYTES(trackinfo.SectorSize) != sectorsize) //Sector size mismatch?
					{
						goto invalidsectordata; //Error out!
					}
					if (emufread64(result, 1, SECTORSIZE_BYTES(trackinfo.SectorSize), f) < 0) //Errored out?
					{
						goto invalidsectordata; //Error out!
					}
					physicalsectorsize = SECTORSIZE_BYTES(trackinfo.SectorSize); //How large are we physically!

					goto sectorready; //Handle ready sector information!
				}
			}
			else //Compressed?
			{
				if (emufread64(&filldata, 1, sizeof(filldata),f)!=sizeof(filldata)) //Read the compressed data!
				{
					if (sectorsizemap) //Allocated sector size map?
					{
						freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
					}
					emufclose64(f); //Close the image!
					return 0; //Invalid IMD file!
				}
				if (sectorsizemap) //Map used?
				{
					physicalsectorsize = sectorsizemap[sectornumber]; //Errored out?
				}
				else
				{
					physicalsectorsize = SECTORSIZE_BYTES(trackinfo.SectorSize); //Errored out?
				}
				if (physicalsectorsize != sectorsize) //Sector size mismatch?
				{
					goto invalidsectordata; //Error out!
				}
				//Fill the result up!
				memset(result, filldata, physicalsectorsize); //Give the result: a sector filled with one type of data!
				goto sectorready;
			}
		}
		else //Invalid sector to read?
		{
			goto invalidsectordata; //Error out!
		}
	}

	//Couldn't read it!

	if (sectorsizemap) //Allocated sector size map?
	{
		freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
	}
	emufclose64(f); //Close the image!
	return 0; //Invalid IMD file!
}

byte writeIMDSector(char* filename, byte side, byte track, byte sector, word sectorsize, void* sectordata)
{
	uint_32 fillsector_dataleft;
	byte *fillsector;
	byte *tailbuffer; //Buffer for the compressed sector until the end!
	byte *headbuffer; //Buffer for the compressed sector until the end!
	FILEPOS tailbuffersize; //Size of the tail buffer!
	FILEPOS compressedsectorpos=0; //Position of the compressed sector!
	FILEPOS eofpos; //EOF position!
	byte retryingheaderror; //Prevents infinite loop on file rewrite!
	byte filldata;
	byte compresseddata_byteval; //What is the compressed data, if it's compressed?
	byte physicalsectornr;
	byte physicalheadnr;
	byte physicalcylindernr;
	word physicalsectorsize; //Effective sector size!
	word sectornumber;
	word* sectorsizemap; //Sector size map!
	uint_32 datarecordnumber;
	TRACKINFORMATIONBLOCK trackinfo;
	word trackskipleft;
	byte identifier[3];
	byte data;
	if (strcmp(filename, "") == 0) return 0; //Unexisting: don't even look at it!
	if (!isext(filename, "imd")) //Not our IMD image file?
	{
		return 0; //Not a IMD image!
	}
	fillsector = (byte*)sectordata; //What sector is supposed to be filled with this byte!
	BIGFILE* f;
	f = emufopen64(filename, "rb+"); //Open the image!
	if (!f) return 0; //Not opened!
	if (emufread64(&identifier, 1, sizeof(identifier), f) != sizeof(identifier)) //Try to read the header?
	{
		emufclose64(f); //Close it!
		return 0; //Not a IMD image!
	}
	if ((identifier[0] != 'I') || (identifier[1] != 'M') || (identifier[2] != 'D')) //Invalid header?
	{
		emufclose64(f); //Close it!
		return 0; //Not a IMD image!
	}
	for (; !emufeof64(f);) //Not EOF yet?
	{
		if (emufread64(&identifier[0], 1, sizeof(identifier[0]), f) != sizeof(identifier[0])) //Failed to read an header comment byte?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (identifier[0] == 0x1A) //Finished comment identifier?
		{
			goto validIMDheaderRead; //Header is validly read!
		}
	}
	//Reached EOF without comment finishing?

	emufclose64(f); //Close the image!
	return 0; //Invalid IMD file!

validIMDheaderRead:
	//Now, skip tracks until we reach the selected track!
	trackskipleft = track; //How many tracks to skip!
	for (; trackskipleft;) //Skipping left?
	{
		if (emufread64(&trackinfo, 1, sizeof(trackinfo), f) != sizeof(trackinfo)) //Failed to read track info?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		//Track info block has been read!
		if (emufseek64(f, trackinfo.sectorspertrack, SEEK_CUR) < 0) //Skip the sector number map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (trackinfo.head_extrabits & IMD_HEAD_CYLINDERMAPPRESENT) //Cylinder map following?
		{
			if (emufseek64(f, trackinfo.sectorspertrack, SEEK_CUR) < 0) //Skip the cylinder number map!
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
		}
		if (trackinfo.head_extrabits & IMD_HEAD_HEADMAPPRESENT) //Head map following?
		{
			if (emufseek64(f, trackinfo.sectorspertrack, SEEK_CUR) < 0) //Skip the head number map!
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
		}
		if (trackinfo.SectorSize == IMD_SECTORSIZE_SECTORSIZEMAPPRESENT) //Sector size map following?
		{
			sectorsizemap = zalloc((trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP", NULL); //Allocate a sector map to use!
			if (!sectorsizemap) //Failed to allocate?
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			if (emufread64(sectorsizemap, 1, (trackinfo.sectorspertrack << 1), f) < 0) //Read the sector size map!
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			sectornumber = 0;
			for (sectornumber = 0; sectornumber < trackinfo.sectorspertrack; ++sectornumber) //Patch as needed!
			{
				sectorsizemap[sectornumber] = SDL_SwapLE16(sectorsizemap[sectornumber]); //Swap all byte ordering to be readable!
			}
		}
		datarecordnumber = trackinfo.sectorspertrack; //How many records to read!
		sectornumber = 0; //Start at the first sector number!
		for (; datarecordnumber;) //Process all sectors on the track!
		{
			if (emufread64(&data, 1, sizeof(data), f) != sizeof(data)) //Read the identifier!
			{
				freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			//Now, we have the identifier!
			if (data) //Not one that's unavailable?
			{
				if (data > 8) //Undefined value?
				{
					freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
					emufclose64(f); //Close the image!
					return 0; //Invalid IMD file!
				}
				if (data & 1) //Normal sector with or without mark, data error or deleted?
				{
					//Skip the sector's data!
					if (sectorsizemap) //Map used?
					{
						if (emufseek64(f, (sectorsizemap[sectornumber]), SEEK_CUR) < 0) //Errored out?
						{
							freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
							emufclose64(f); //Close the image!
							return 0; //Invalid IMD file!
						}
					}
					else
					{
						if (emufseek64(f, SECTORSIZE_BYTES(trackinfo.SectorSize), SEEK_CUR) < 0) //Errored out?
						{
							freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
							emufclose64(f); //Close the image!
							return 0; //Invalid IMD file!
						}
					}
				}
				else //Compressed?
				{
					if (emufseek64(f, 1, SEEK_CUR) < 0) //Skip the compressed data!
					{
						if (sectorsizemap) //Allocated sector size map?
						{
							freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
						}
						emufclose64(f); //Close the image!
						return 0; //Invalid IMD file!
					}
				}
			}
			freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
			++sectornumber; //Process the next sector number!
			--datarecordnumber; //Processed!
		}
	}

	//Now, we're at the specified track!
	if (emufread64(&trackinfo, 1, sizeof(trackinfo), f) != sizeof(trackinfo)) //Failed to read track info?
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	//Track info block has been read!
	if (sector >= trackinfo.sectorspertrack) //Not enough to read?
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	if (emufseek64(f, sector, SEEK_CUR) < 0) //Skip the sector number map!
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	if (emufread64(&physicalsectornr, 1, sizeof(physicalsectornr), f) != sizeof(physicalsectornr)) //Read the actual sector number!
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	if (emufseek64(f, trackinfo.sectorspertrack - sector, SEEK_CUR) < 0) //Skip the sector number map!
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	physicalcylindernr = (trackinfo.cylinder); //Default cylinder number!
	if (trackinfo.head_extrabits & IMD_HEAD_CYLINDERMAPPRESENT) //Cylinder map following?
	{
		if (emufseek64(f, sector, SEEK_CUR) < 0) //Skip the cylinder number map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (emufread64(&physicalcylindernr, 1, sizeof(physicalcylindernr), f) != sizeof(physicalcylindernr)) //Read the cylinder number!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (emufseek64(f, trackinfo.sectorspertrack - sector, SEEK_CUR) < 0) //Skip the cylinder number map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
	}
	physicalheadnr = (trackinfo.head_extrabits & IMD_HEAD_HEADNUMBER); //Default head number!
	if (trackinfo.head_extrabits & IMD_HEAD_HEADMAPPRESENT) //Head map following?
	{
		if (emufseek64(f, sector, SEEK_CUR) < 0) //Skip the head number map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (emufread64(&physicalheadnr, 1, sizeof(physicalheadnr), f) != sizeof(physicalheadnr)) //Read the cylinder number!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (emufseek64(f, trackinfo.sectorspertrack - sector, SEEK_CUR) < 0) //Skip the head number map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
	}
	if (trackinfo.SectorSize == IMD_SECTORSIZE_SECTORSIZEMAPPRESENT) //Sector size map following?
	{
		sectorsizemap = zalloc((trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP", NULL); //Allocate a sector map to use!
		if (!sectorsizemap) //Failed to allocate?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if (emufread64(sectorsizemap, 1, (trackinfo.sectorspertrack << 1), f) < 0) //Read the sector size map!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		sectornumber = 0;
		for (sectornumber = 0; sectornumber < trackinfo.sectorspertrack; ++sectornumber) //Patch as needed!
		{
			sectorsizemap[sectornumber] = SDL_SwapLE16(sectorsizemap[sectornumber]); //Swap all byte ordering to be readable!
		}
	}

	//Skip n sectors!
	datarecordnumber = MIN(trackinfo.sectorspertrack, sector); //How many records to read!
	sectornumber = 0; //Start at the first sector number!
	for (; datarecordnumber;) //Process all sectors on the track!
	{
		if (emufread64(&data, 1, sizeof(data), f) != sizeof(data)) //Read the identifier!
		{
			if (sectorsizemap) //Allocated sector size map?
			{
				freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
			}
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		//Now, we have the identifier!
		if (data) //Not one that's unavailable?
		{
			if (data > 8) //Undefined value?
			{
				if (sectorsizemap) //Allocated sector size map?
				{
					freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
				}
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			if (data & 1) //Normal sector with or without mark, data error or deleted?
			{
				//Skip the sector's data!
				if (sectorsizemap) //Map used?
				{
					if (emufseek64(f, (sectorsizemap[sectornumber]), SEEK_CUR) < 0) //Errored out?
					{
						if (sectorsizemap) //Allocated sector size map?
						{
							freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
						}
						emufclose64(f); //Close the image!
						return 0; //Invalid IMD file!
					}
				}
				else
				{
					if (emufseek64(f, SECTORSIZE_BYTES(trackinfo.SectorSize), SEEK_CUR) < 0) //Errored out?
					{
						if (sectorsizemap) //Allocated sector size map?
						{
							freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
						}
						emufclose64(f); //Close the image!
						return 0; //Invalid IMD file!
					}
				}
			}
			else //Compressed?
			{
				if (emufseek64(f, 1, SEEK_CUR) < 0) //Skip the compressed data!
				{
					if (sectorsizemap) //Allocated sector size map?
					{
						freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
					}
					emufclose64(f); //Close the image!
					return 0; //Invalid IMD file!
				}
			}
		}
		++sectornumber; //Process the next sector number!
		--datarecordnumber; //Processed!
	}

	if (datarecordnumber) //Left to read? Reached the sector!
	{
		compressedsectorpos = emuftell64(f); //What is the location of the compressed sector in the original file, if it's compressed!
		if (compressedsectorpos < 0) goto failedcompressedpos; //Failed detecting the compressed location?
		headbuffer = (byte*)zalloc(compressedsectorpos, "IMDIMAGE_FAILEDHEADBUFFER",NULL);
		if (emufread64(&data, 1, sizeof(data), f) != sizeof(data)) //Read the identifier!
		{
			failedcompressedpos:
			if (sectorsizemap) //Allocated sector size map?
			{
				freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
			}
			if (headbuffer) //Head buffer allocated?
			{
				freez((void**)&headbuffer, compressedsectorpos, "IMDIMAGE_FAILEDHEADBUFFER"); //Free the allocated sector size map!
			}
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		//Now, we have the identifier!
		if (data) //Not one that's unavailable?
		{
			if (data > 8) //Undefined value?
			{
			invalidsectordata:
				if (sectorsizemap) //Allocated sector size map?
				{
					freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
				}
				if (headbuffer) //Head buffer allocated?
				{
					freez((void**)&headbuffer, compressedsectorpos, "IMDIMAGE_FAILEDHEADBUFFER"); //Free the allocated sector size map!
				}
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			if (data & 1) //Normal sector with or without mark, data error or deleted?
			{
				//Skip the sector's data!
				if (sectorsizemap) //Map used?
				{
					if (sectorsizemap[sectornumber] != sectorsize) //Sector size mismatch?
					{
						goto invalidsectordata; //Error out!
					}
					//Check if we're valid!
					if (emufwrite64(sectordata, 1, sectorsizemap[sectornumber], f) != sectorsizemap[sectornumber]) //Failed to skip the data part of the sector?
					{
						goto invalidsectordata; //Error out!
					}
					physicalsectorsize = sectorsizemap[sectornumber]; //Physical sector size!
					//The sector is loaded and valid!
				sectorready:
					//Fill up the sector information block!
					/*
					result->cylinderID = physicalcylindernr; //Physical cylinder number!
					result->headnumber = physicalheadnr; //Physical head number!
					result->sectorID = physicalsectornr; //Physical sector number!
					result->MFM_speedmode = trackinfo.mode; //The mode!
					result->sectorsize = physicalsectorsize; //Physical sector size!
					*/

					//Finish up!
					if (sectorsizemap) //Allocated sector size map?
					{
						freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
					}
					if (headbuffer) //Head buffer allocated?
					{
						freez((void**)&headbuffer, compressedsectorpos, "IMDIMAGE_FAILEDHEADBUFFER"); //Free the allocated sector size map!
					}
					emufclose64(f); //Close the image!
					return 1; //Gotten information about the record!
				}
				else
				{
					if (SECTORSIZE_BYTES(trackinfo.SectorSize) != sectorsize) //Sector size mismatch?
					{
						goto invalidsectordata; //Error out!
					}
					if (emufwrite64(sectordata, 1, SECTORSIZE_BYTES(trackinfo.SectorSize), f) < 0) //Errored out?
					{
						goto invalidsectordata; //Error out!
					}
					physicalsectorsize = SECTORSIZE_BYTES(trackinfo.SectorSize); //How large are we physically!

					goto sectorready; //Handle ready sector information!
				}
			}
			else //Compressed?
			{
				if (emufread64(&filldata, 1, sizeof(filldata), f) != sizeof(filldata)) //Read the compressed data!
				{
					if (sectorsizemap) //Allocated sector size map?
					{
						freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
					}
					if (headbuffer) //Head buffer allocated?
					{
						freez((void**)&headbuffer, compressedsectorpos, "IMDIMAGE_FAILEDHEADBUFFER"); //Free the allocated sector size map!
					}
					emufclose64(f); //Close the image!
					return 0; //Invalid IMD file!
				}
				if (sectorsizemap) //Map used?
				{
					physicalsectorsize = sectorsizemap[sectornumber]; //Errored out?
				}
				else
				{
					physicalsectorsize = SECTORSIZE_BYTES(trackinfo.SectorSize); //Errored out?
				}
				if (physicalsectorsize != sectorsize) //Sector size mismatch?
				{
					goto invalidsectordata; //Error out!
				}
				//Fill the result up!
				compresseddata_byteval = fillsector[0]; //What byte are we checking to be compressed!
				fillsector_dataleft = physicalsectorsize; //How much data is left to check!

				for (; fillsector_dataleft;) //Check all bytes for the fill byte!
				{
					if (*(fillsector++) != compresseddata_byteval) //Not compressed anymore?
					{
						break; //Stop looking: we're not compressed data anymore!
					}
					--fillsector_dataleft; //One byte has been checked!
				}
				if (fillsector_dataleft == 0) //Compressed data stays compressed?
				{
					if (emufseek64(f, -1, SEEK_CUR) < 0) //Go back to the compressed byte!
					{
						goto invalidsectordata; //Error out!
					}
					if (emufwrite64(&compresseddata_byteval, 1, sizeof(compresseddata_byteval), f) != sizeof(compresseddata_byteval)) //Update the compressed data!
					{
						goto invalidsectordata; //Error out!
					}
					//The compressed sector data has been updated!
					goto sectorready; //Success!
				}
				else //We need to convert the compressed sector into a non-compressed sector!
				{
					//First, read all data that's following the compressed sector into memory!
					if (emufseek64(f, 0, SEEK_END) < 0) //Failed getting to EOF?
					{
						goto invalidsectordata; //Error out!
					}
					if (emuftell64(f) < 0) //Invalid to update?
					{
						goto invalidsectordata; //Error out!
					}
					eofpos = emuftell64(f); //What is the location of the EOF!
					if (emufseek64(f, compressedsectorpos + 2ULL, SEEK_SET) < 0) //Return to the compressed sector's following data!
					{
						goto invalidsectordata;
					}
					//Now, allocate a buffer to contain it!
					tailbuffersize = (eofpos - compressedsectorpos - 2ULL); //How large should the tail buffer be?
					tailbuffer = (byte*)zalloc(tailbuffersize, "IMDIMAGE_FOOTERDATA", NULL); //Allocate room for the footer to be contained!
					if (tailbuffer == NULL) //Failed to allocate?
					{
						goto invalidsectordata; //Error out!
					}
					//Tail buffer is ready, now fill it up!
					if (emufread64(tailbuffer, 1, tailbuffersize, f) != tailbuffersize) //Failed to read?
					{
						freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
						goto invalidsectordata; //Error out!
					}
					//Tail buffer is filled, now return to the sector to write!
					if (emufseek64(f, compressedsectorpos, SEEK_SET) < 0) //Return to the compressed sector's following data!
					{
						goto invalidsectordata;
					}
					//Now, try to write the sector ID and data!
					compresseddata_byteval = 0x01; //A valid sector ID!
					if (emufwrite64(&compresseddata_byteval, 1, sizeof(compresseddata_byteval), f) != sizeof(compresseddata_byteval)) //Write the sector ID!
					{
						goto invalidsectordata; //Error out!
					}
					if (emufwrite64(sectordata, 1, physicalsectorsize, f) != physicalsectorsize) //Failed writing the physical sector?
					{
						//Perform an undo operation!
					undoUncompressedsectorwrite: //Undo it!
						if (emufseek64(f, 0, SEEK_SET) < 0) //Failed getting to BOF?
						{
							goto invalidsectordata; //Error out!
						}
						if (emufread64(headbuffer, 1, compressedsectorpos, f) != compressedsectorpos) //Failed reading the head?
						{
							goto invalidsectordata; //Error out!
						}
						if (emufseek64(f, compressedsectorpos, SEEK_SET) < 0) //Failed to seek?
						{
							freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
							goto invalidsectordata; //Error out!
						}
						retryingheaderror = 1; //Allow retrying rewrite once!
					retryclearedend: //After writing the head buffer when not having reached EOF at the end of this!
						if (emufwrite64(&data, 1, sizeof(data), f) != sizeof(data)) //Failed to write the compressed ID?
						{
							freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
							goto invalidsectordata; //Error out!
						}
						if (emufwrite64(&filldata, 1, sizeof(filldata), f) != sizeof(filldata)) //Failed writing the original data back?
						{
							freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
							goto invalidsectordata; //Error out!
						}
						if (emufwrite64(tailbuffer, 1, tailbuffersize, f) != tailbuffersize) //Failed writing the original tail data back?
						{
							freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
							goto invalidsectordata; //Error out!
						}
						if (emufseek64(f, 0, SEEK_END) < 0) //Couldn't seek to EOF?
						{
							freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
							goto invalidsectordata; //Error out!
						}
						if ((emuftell64(f)!=eofpos) && retryingheaderror) //EOF has changed to an incorrect value?
						{
							emufclose64(f); //Close the image!
							f = emufopen64(filename, "wb"); //Open the image!
							if (!f) //Failed to reopen?
							{
								if (headbuffer) //Head buffer allocated?
								{
									freez((void**)&headbuffer, compressedsectorpos, "IMDIMAGE_FAILEDHEADBUFFER"); //Free the allocated sector size map!
								}
								freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
								goto invalidsectordata; //Error out!
							}
							if (emufwrite64(headbuffer, 1, compressedsectorpos, f) != compressedsectorpos) //Failed writing the original head data back?
							{
								if (headbuffer) //Head buffer allocated?
								{
									freez((void**)&headbuffer, compressedsectorpos, "IMDIMAGE_FAILEDHEADBUFFER"); //Free the allocated sector size map!
								}
								freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
								goto invalidsectordata; //Error out!
							}
							retryingheaderror = 0; //Fail if this occurs again, prevent infinite loop!
							goto retryclearedend; //Retry with the end having been cleared!
						}

						freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
						if (headbuffer) //Head buffer allocated?
						{
							freez((void**)&headbuffer, compressedsectorpos, "IMDIMAGE_FAILEDHEADBUFFER"); //Free the allocated sector size map!
						}
						goto invalidsectordata; //Error out always, since we couldn't update the real data!
					}
					if (emufwrite64(tailbuffer, 1, tailbuffersize, f) != tailbuffersize) //Couldn't update the remainder of the file correctly?
					{
						goto undoUncompressedsectorwrite; //Perform an undo operation, if we can!
					}
					//We've successfully updated the file with a new sector!
					//The compressed sector data has been updated!
					freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
					if (headbuffer) //Head buffer allocated?
					{
						freez((void**)&headbuffer, compressedsectorpos, "IMDIMAGE_FAILEDHEADBUFFER"); //Free the allocated sector size map!
					}
					goto sectorready; //Success!
				}
				//memset(result, filldata, physicalsectorsize); //Give the result: a sector filled with one type of data!
				goto invalidsectordata; //Couldn't properly update the compressed sector data!
			}
		}
		else //Invalid sector to write?
		{
			goto invalidsectordata; //Error out!
		}
	}

	//Couldn't find the sector!

	if (sectorsizemap) //Allocated sector size map?
	{
		freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
	}
	if (headbuffer) //Head buffer allocated?
	{
		freez((void**)&headbuffer, compressedsectorpos, "IMDIMAGE_FAILEDHEADBUFFER"); //Free the allocated sector size map!
	}
	emufclose64(f); //Close the image!
	return 0; //Invalid IMD file!
}