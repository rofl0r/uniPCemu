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

byte readIMDSectorInfo(char* filename, byte track, byte head, byte sector, IMDIMAGE_SECTORINFO* result)
{
	byte physicalsectornr;
	byte physicalheadnr;
	byte physicalcylindernr;
	word physicalsectorsize; //Effective sector size!
	word sectornumber;
	word* sectorsizemap=NULL; //Sector size map!
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
	for (;;) //Skipping left?
	{
		if (emufread64(&trackinfo, 1, sizeof(trackinfo), f) != sizeof(trackinfo)) //Failed to read track info?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if ((trackinfo.cylinder == track) && ((trackinfo.head_extrabits&IMD_HEAD_HEADNUMBER)==head)) //Track&head found?
		{
			if (emufseek64(f, -((int)sizeof(trackinfo)), SEEK_CUR) < 0) //Found!
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			break; //Stop searching: we're found!
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
	if (emufseek64(f, trackinfo.sectorspertrack-sector-1, SEEK_CUR) < 0) //Skip the sector number map!
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
		if (emufseek64(f, trackinfo.sectorspertrack - sector - 1, SEEK_CUR) < 0) //Skip the cylinder number map!
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
		if (emufseek64(f, trackinfo.sectorspertrack - sector - 1, SEEK_CUR) < 0) //Skip the head number map!
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

	if (datarecordnumber==0) //Left to read? Reached the sector!
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
		invalidsectordataInfo:
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
						goto invalidsectordataInfo; //Error out!
					}
					physicalsectorsize = sectorsizemap[sectornumber]; //Physical sector size!
					//The sector is loaded and valid!
				sectorreadyInfo:
					//Fill up the sector information block!
					result->cylinderID = physicalcylindernr; //Physical cylinder number!
					result->headnumber = physicalheadnr; //Physical head number!
					result->sectorID = physicalsectornr; //Physical sector number!
					result->MFM_speedmode = trackinfo.mode; //The mode!
					result->sectorsize = physicalsectorsize; //Physical sector size!
					result->totalsectors = trackinfo.sectorspertrack; //How many sectors are on this track!
					result->datamark = ((data - 1) >> 1); //What is it marked as!
					sectorready_unreadableInfo:

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
						goto invalidsectordataInfo;
					}
					physicalsectorsize = SECTORSIZE_BYTES(trackinfo.SectorSize); //How large are we physically!

					goto sectorreadyInfo; //Handle ready sector information!
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
				goto sectorreadyInfo;
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
			goto sectorready_unreadableInfo; //Handle the unreadable sector result!
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


byte readIMDSector(char* filename, byte track, byte head, byte sector, word sectorsize, void* result)
{
	byte filldata;
	byte physicalsectornr;
	byte physicalheadnr;
	byte physicalcylindernr;
	word physicalsectorsize; //Effective sector size!
	word sectornumber;
	word* sectorsizemap=NULL; //Sector size map!
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
	for (;;) //Skipping left?
	{
		if (emufread64(&trackinfo, 1, sizeof(trackinfo), f) != sizeof(trackinfo)) //Failed to read track info?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if ((trackinfo.cylinder == track) && ((trackinfo.head_extrabits & IMD_HEAD_HEADNUMBER) == head)) //Track&head found?
		{
			if (emufseek64(f, -((int)sizeof(trackinfo)), SEEK_CUR) < 0) //Found!
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			break; //Stop searching: we're found!
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
	if (emufseek64(f, trackinfo.sectorspertrack - sector - 1, SEEK_CUR) < 0) //Skip the sector number map!
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
		if (emufseek64(f, trackinfo.sectorspertrack - sector - 1, SEEK_CUR) < 0) //Skip the cylinder number map!
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
		if (emufseek64(f, trackinfo.sectorspertrack - sector - 1, SEEK_CUR) < 0) //Skip the head number map!
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

	if (datarecordnumber==0) //Left to read? Reached the sector!
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
			invalidsectordataRead:
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
						goto invalidsectordataRead; //Error out!
					}
					//Check if we're valid!
					if (emufread64(result,1,sectorsizemap[sectornumber],f)!=sectorsizemap[sectornumber]) //Failed to skip the data part of the sector?
					{
						goto invalidsectordataRead; //Error out!
					}
					physicalsectorsize = sectorsizemap[sectornumber]; //Physical sector size!
					//The sector is loaded and valid!
				sectorreadyRead:
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
						goto invalidsectordataRead; //Error out!
					}
					if (emufread64(result, 1, SECTORSIZE_BYTES(trackinfo.SectorSize), f) < 0) //Errored out?
					{
						goto invalidsectordataRead; //Error out!
					}
					physicalsectorsize = SECTORSIZE_BYTES(trackinfo.SectorSize); //How large are we physically!

					goto sectorreadyRead; //Handle ready sector information!
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
					goto invalidsectordataRead; //Error out!
				}
				//Fill the result up!
				memset(result, filldata, physicalsectorsize); //Give the result: a sector filled with one type of data!
				goto sectorreadyRead;
			}
		}
		else //Invalid sector to read?
		{
			goto invalidsectordataRead; //Error out!
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

byte writeIMDSector(char* filename, byte track, byte head, byte sector, word sectorsize, void* sectordata)
{
	uint_32 fillsector_dataleft;
	byte *fillsector=NULL;
	byte *tailbuffer=NULL; //Buffer for the compressed sector until the end!
	byte *headbuffer=NULL; //Buffer for the compressed sector until the end!
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
	word* sectorsizemap=NULL; //Sector size map!
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
			goto validIMDheaderWrite; //Header is validly read!
		}
	}
	//Reached EOF without comment finishing?

	emufclose64(f); //Close the image!
	return 0; //Invalid IMD file!

validIMDheaderWrite:
	//Now, skip tracks until we reach the selected track!
	trackskipleft = track; //How many tracks to skip!
	for (;;) //Skipping left?
	{
		if (emufread64(&trackinfo, 1, sizeof(trackinfo), f) != sizeof(trackinfo)) //Failed to read track info?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if ((trackinfo.cylinder == track) && ((trackinfo.head_extrabits & IMD_HEAD_HEADNUMBER) == head)) //Track&head found?
		{
			if (emufseek64(f, -((int)sizeof(trackinfo)), SEEK_CUR) < 0) //Found!
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			break; //Stop searching: we're found!
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
	if (emufseek64(f, trackinfo.sectorspertrack - sector - 1, SEEK_CUR) < 0) //Skip the sector number map!
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
		if (emufseek64(f, trackinfo.sectorspertrack - sector - 1, SEEK_CUR) < 0) //Skip the cylinder number map!
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
		if (emufseek64(f, trackinfo.sectorspertrack - sector - 1, SEEK_CUR) < 0) //Skip the head number map!
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

	if (datarecordnumber==0) //Left to read? Reached the sector!
	{
		compressedsectorpos = emuftell64(f); //What is the location of the compressed sector in the original file, if it's compressed!
		if (compressedsectorpos < 0) goto failedcompressedposWrite; //Failed detecting the compressed location?
		headbuffer = (byte*)zalloc(compressedsectorpos, "IMDIMAGE_FAILEDHEADBUFFER",NULL);
		if (emufread64(&data, 1, sizeof(data), f) != sizeof(data)) //Read the identifier!
		{
			failedcompressedposWrite:
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
			invalidsectordataWrite:
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
						goto invalidsectordataWrite; //Error out!
					}
					//Check if we're valid!
					if (emufwrite64(sectordata, 1, sectorsizemap[sectornumber], f) != sectorsizemap[sectornumber]) //Failed to skip the data part of the sector?
					{
						goto invalidsectordataWrite; //Error out!
					}
					physicalsectorsize = sectorsizemap[sectornumber]; //Physical sector size!
					//The sector is loaded and valid!
				sectorreadyWrite:
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
						goto invalidsectordataWrite; //Error out!
					}
					if (emufwrite64(sectordata, 1, SECTORSIZE_BYTES(trackinfo.SectorSize), f) < 0) //Errored out?
					{
						goto invalidsectordataWrite; //Error out!
					}
					physicalsectorsize = SECTORSIZE_BYTES(trackinfo.SectorSize); //How large are we physically!

					goto sectorreadyWrite; //Handle ready sector information!
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
					goto invalidsectordataWrite; //Error out!
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
						goto invalidsectordataWrite; //Error out!
					}
					if (emufwrite64(&compresseddata_byteval, 1, sizeof(compresseddata_byteval), f) != sizeof(compresseddata_byteval)) //Update the compressed data!
					{
						goto invalidsectordataWrite; //Error out!
					}
					//The compressed sector data has been updated!
					goto sectorreadyWrite; //Success!
				}
				else //We need to convert the compressed sector into a non-compressed sector!
				{
					//First, read all data that's following the compressed sector into memory!
					if (emufseek64(f, 0, SEEK_END) < 0) //Failed getting to EOF?
					{
						goto invalidsectordataWrite; //Error out!
					}
					if (emuftell64(f) < 0) //Invalid to update?
					{
						goto invalidsectordataWrite; //Error out!
					}
					eofpos = emuftell64(f); //What is the location of the EOF!
					if (emufseek64(f, compressedsectorpos + 2ULL, SEEK_SET) < 0) //Return to the compressed sector's following data!
					{
						goto invalidsectordataWrite;
					}
					//Now, allocate a buffer to contain it!
					tailbuffersize = (eofpos - compressedsectorpos - 2ULL); //How large should the tail buffer be?
					tailbuffer = (byte*)zalloc(tailbuffersize, "IMDIMAGE_FOOTERDATA", NULL); //Allocate room for the footer to be contained!
					if (tailbuffer == NULL) //Failed to allocate?
					{
						goto invalidsectordataWrite; //Error out!
					}
					//Tail buffer is ready, now fill it up!
					if (emufread64(tailbuffer, 1, tailbuffersize, f) != tailbuffersize) //Failed to read?
					{
						freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
						goto invalidsectordataWrite; //Error out!
					}
					//Tail buffer is filled, now return to the sector to write!
					if (emufseek64(f, compressedsectorpos, SEEK_SET) < 0) //Return to the compressed sector's following data!
					{
						goto invalidsectordataWrite;
					}
					//Now, try to write the sector ID and data!
					compresseddata_byteval = 0x01; //A valid sector ID!
					if (emufwrite64(&compresseddata_byteval, 1, sizeof(compresseddata_byteval), f) != sizeof(compresseddata_byteval)) //Write the sector ID!
					{
						goto invalidsectordataWrite; //Error out!
					}
					if (emufwrite64(sectordata, 1, physicalsectorsize, f) != physicalsectorsize) //Failed writing the physical sector?
					{
						//Perform an undo operation!
					undoUncompressedsectorwriteWrite: //Undo it!
						if (emufseek64(f, 0, SEEK_SET) < 0) //Failed getting to BOF?
						{
							goto invalidsectordataWrite; //Error out!
						}
						if (emufread64(headbuffer, 1, compressedsectorpos, f) != compressedsectorpos) //Failed reading the head?
						{
							goto invalidsectordataWrite; //Error out!
						}
						if (emufseek64(f, compressedsectorpos, SEEK_SET) < 0) //Failed to seek?
						{
							freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
							goto invalidsectordataWrite; //Error out!
						}
						retryingheaderror = 1; //Allow retrying rewrite once!
					retryclearedendWrite: //After writing the head buffer when not having reached EOF at the end of this!
						if (emufwrite64(&data, 1, sizeof(data), f) != sizeof(data)) //Failed to write the compressed ID?
						{
							freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
							goto invalidsectordataWrite; //Error out!
						}
						if (emufwrite64(&filldata, 1, sizeof(filldata), f) != sizeof(filldata)) //Failed writing the original data back?
						{
							freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
							goto invalidsectordataWrite; //Error out!
						}
						if (emufwrite64(tailbuffer, 1, tailbuffersize, f) != tailbuffersize) //Failed writing the original tail data back?
						{
							freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
							goto invalidsectordataWrite; //Error out!
						}
						if (emufseek64(f, 0, SEEK_END) < 0) //Couldn't seek to EOF?
						{
							freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
							goto invalidsectordataWrite; //Error out!
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
								goto invalidsectordataWrite; //Error out!
							}
							if (emufwrite64(headbuffer, 1, compressedsectorpos, f) != compressedsectorpos) //Failed writing the original head data back?
							{
								if (headbuffer) //Head buffer allocated?
								{
									freez((void**)&headbuffer, compressedsectorpos, "IMDIMAGE_FAILEDHEADBUFFER"); //Free the allocated sector size map!
								}
								freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
								goto invalidsectordataWrite; //Error out!
							}
							retryingheaderror = 0; //Fail if this occurs again, prevent infinite loop!
							goto retryclearedendWrite; //Retry with the end having been cleared!
						}

						freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
						if (headbuffer) //Head buffer allocated?
						{
							freez((void**)&headbuffer, compressedsectorpos, "IMDIMAGE_FAILEDHEADBUFFER"); //Free the allocated sector size map!
						}
						goto invalidsectordataWrite; //Error out always, since we couldn't update the real data!
					}
					if (emufwrite64(tailbuffer, 1, tailbuffersize, f) != tailbuffersize) //Couldn't update the remainder of the file correctly?
					{
						goto undoUncompressedsectorwriteWrite; //Perform an undo operation, if we can!
					}
					//We've successfully updated the file with a new sector!
					//The compressed sector data has been updated!
					freez((void**)&tailbuffer, tailbuffersize, "IMDIMAGE_FOOTERDATA"); //Release the tail buffer!
					if (headbuffer) //Head buffer allocated?
					{
						freez((void**)&headbuffer, compressedsectorpos, "IMDIMAGE_FAILEDHEADBUFFER"); //Free the allocated sector size map!
					}
					goto sectorreadyWrite; //Success!
				}
				//memset(result, filldata, physicalsectorsize); //Give the result: a sector filled with one type of data!
				goto invalidsectordataWrite; //Couldn't properly update the compressed sector data!
			}
		}
		else //Invalid sector to write?
		{
			goto invalidsectordataWrite; //Error out!
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

byte formatIMDTrack(char* filename, byte track, byte head, byte MFM, byte speed, byte filldata, byte numsectors, byte* sectordata)
{
	byte wasskippingtrack=0;
	word currentsector;
	byte b;
	word w;
	byte headskipped = 0;
	byte skippingtrack = 0; //Skipping this track once?
	uint_32 fillsector_dataleft;
	byte* sectordataptr = NULL;
	byte* fillsector = NULL;
	byte* tailbuffer = NULL; //Buffer for the compressed sector until the end!
	byte* headbuffer = NULL; //Buffer for the compressed sector until the end!
	byte* sectornumbermap = NULL; //Original sector number map!
	byte* cylindermap = NULL; //Original cylinder map!
	byte* headmap = NULL; //Original head map!
	byte* oldsectordata = NULL; //Old sector data!
	FILEPOS oldsectordatasize;
	FILEPOS tailbuffersize; //Size of the tail buffer!
	FILEPOS headbuffersize = 0; //Position of the compressed sector!
	FILEPOS tailpos; //Tail position!
	FILEPOS eofpos; //EOF position!
	byte searchingheadsize = 1; //Were we searching the head size?
	byte retryingheaderror; //Prevents infinite loop on file rewrite!
	byte compresseddata_byteval; //What is the compressed data, if it's compressed?
	byte physicalsectornr;
	byte physicalheadnr;
	byte physicalcylindernr;
	word physicalsectorsize; //Effective sector size!
	word sectornumber;
	word* sectorsizemap = NULL; //Original sector size map!
	uint_32 datarecordnumber;
	TRACKINFORMATIONBLOCK trackinfo, newtrackinfo; //Original and new track info!
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
			goto validIMDheaderFormat; //Header is validly read!
		}
	}
	//Reached EOF without comment finishing?

	emufclose64(f); //Close the image!
	return 0; //Invalid IMD file!

validIMDheaderFormat:
	//Now, skip tracks until we reach the selected track!
	trackskipleft = track; //How many tracks to skip!
	for (;;) //Skipping left?
	{
	format_skipFormattedTrack: //Skip the formatted track when formatting!
		if (emufread64(&trackinfo, 1, sizeof(trackinfo), f) != sizeof(trackinfo)) //Failed to read track info?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		if ((((trackinfo.cylinder == track) && ((trackinfo.head_extrabits & IMD_HEAD_HEADNUMBER) == head)) || (wasskippingtrack)) && (skippingtrack == 0)) //Track&head found?
		{
			wasskippingtrack = 0; //Not skipping anymore!
			if (emufseek64(f, -((int)sizeof(trackinfo)), SEEK_CUR) < 0) //Found!
			{
				emufclose64(f); //Close the image!
				return 0; //Invalid IMD file!
			}
			break; //Stop searching: we're found!
		}
		if (skippingtrack) //Skipping a track?
		{
			--skippingtrack; //One track has been skipped!
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

	//Save the head position!
	if (searchingheadsize) //Searching the head size?
	{
		if (emufeof64(f)) //At EOF? Invalid track!
		{
			goto errorOutFormat; //Erroring out on the formatting process!
		}
		//Now, we're at the specified track!
		if (emuftell64(f) < 0) //Can't find the head position&size?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}
		headbuffersize = emuftell64(f); //Head buffer size of previous tracks!

		if (emufread64(&trackinfo, 1, sizeof(trackinfo), f) != sizeof(trackinfo)) //Failed to read track info?
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}

		if (emufseek64(f, -((int)sizeof(trackinfo)), SEEK_CUR) < 0) //Go back to the track information!
		{
			emufclose64(f); //Close the image!
			return 0; //Invalid IMD file!
		}

		searchingheadsize = 0; //Not searching the head size anymore!
		skippingtrack = 1; //Skipping one track!
		wasskippingtrack = 1; //We were skipping tracks! Ignore the next track information's details!
		goto format_skipFormattedTrack; //Skip the track to format!
	}

	//Now, we're at the track after the track to format!
	tailpos = emuftell64(f); //The position of the tail in the original file!
	if (emufseek64(f, 0, SEEK_END) < 0) //Couldn't seek to EOF?
	{
		goto errorOutFormat;
	}

	tailbuffersize = emuftell64(f) - tailpos; //The size of the tail buffer! 

	if (emufseek64(f, headbuffersize, SEEK_SET) < 0) //Can't seek to the track we want to write?
	{
		goto errorOutFormat; //Error out!
	}

	if (emufread64(&trackinfo, 1, sizeof(trackinfo), f) != sizeof(trackinfo)) //Failed to read old track info?
	{
		emufclose64(f); //Close the image!
		return 0; //Invalid IMD file!
	}
	//Now, we have the start of the track, end of the track and end of the following tracks! We need to load the head, current track's data and following tracks into memory!
	if (headbuffersize)
	{
		headbuffer = (byte*)zalloc(headbuffersize, "IMDIMAGE_HEADBUFFER", NULL); //Allocate a head buffer!
	}
	if ((headbuffer == NULL) && headbuffersize) goto errorOutFormat; //Error out if we can't allocate!
	if (emufseek64(f, 0, SEEK_SET) < 0) //Failed to get to BOF?
	{
		goto errorOutFormat;
	}
	if (emufread64(headbuffer, 1, headbuffersize, f) != headbuffersize) goto errorOutFormat; //Couldn't read the old head!
	if (tailbuffersize) //Gotten a size to use?
	{
		tailbuffer = (byte*)zalloc(tailbuffersize, "IMDIMAGE_TAILBUFFER", NULL); //Allocate a tail buffer!
		if ((tailbuffer == NULL) && tailbuffersize) goto errorOutFormat; //Error out if we can't allocate!
	}
	if (emufseek64(f, tailpos, SEEK_SET) < 0) //Failed to get to next track?
	{
		goto errorOutFormat;
	}
	if (tailbuffer) //Have a tail buffer?
	{
		if (emufread64(tailbuffer, 1, tailbuffersize, f) != tailbuffersize) goto errorOutFormat; //Couldn't read the old tail!
	}
	if (emufseek64(f, headbuffersize + sizeof(trackinfo), SEEK_SET) < 0) goto errorOutFormat; //Couldn't goto sector number map!
	if (trackinfo.sectorspertrack) //Gotten sectors per track?
	{
		sectornumbermap = (byte*)zalloc(trackinfo.sectorspertrack, "IMDIMAGE_SECTORNUMBERMAP", NULL); //Allocate the sector number map!
		if (sectornumbermap == NULL) goto errorOutFormat;
		if (emufread64(sectornumbermap, 1, trackinfo.sectorspertrack, f) != trackinfo.sectorspertrack) goto errorOutFormat;
		if (trackinfo.head_extrabits & IMD_HEAD_CYLINDERMAPPRESENT) //Cylinder map following?
		{
			cylindermap = (byte*)zalloc(trackinfo.sectorspertrack, "IMDIMAGE_CYLINDERMAP", NULL); //Allocate the cylinder map!
			if (cylindermap == NULL) goto errorOutFormat; //Error out if we can't allocate!
			if (emufread64(cylindermap, 1, trackinfo.sectorspertrack, f) != trackinfo.sectorspertrack) goto errorOutFormat;
		}
		if (trackinfo.head_extrabits & IMD_HEAD_HEADMAPPRESENT) //Head map following?
		{
			headmap = (byte*)zalloc(trackinfo.sectorspertrack, "IMDIMAGE_HEADMAP", NULL); //Allocate the cylinder map!
			if (headmap == NULL) goto errorOutFormat; //Error out if we can't allocate!
			if (emufread64(headmap, 1, trackinfo.sectorspertrack, f) != trackinfo.sectorspertrack) goto errorOutFormat;
		}
		sectorsizemap = NULL; //Default: no sector size map was present!
		if (trackinfo.SectorSize == IMD_SECTORSIZE_SECTORSIZEMAPPRESENT) //Sector size map following?
		{
			sectorsizemap = zalloc((trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP", NULL); //Allocate a sector map to use!
			if (!sectorsizemap) //Failed to allocate?
			{
				goto errorOutFormat;
			}
			if (emufread64(sectorsizemap, 1, (trackinfo.sectorspertrack << 1), f) != (trackinfo.sectorspertrack << 1)) //Read the sector size map!
			{
				goto errorOutFormat;
			}
			//Leave the map alone for easier restoring in case of errors!
		}
	}

	oldsectordatasize = 0; //Initialize the old sector data size for the track!
	if (emuftell64(f) < 0) goto errorOutFormat; //Can't format if can't tell the size of the data!
	oldsectordatasize = emuftell64(f); //Old sector data size!
	oldsectordatasize = tailpos - oldsectordatasize; //The size of the old sector data!
	if (oldsectordatasize) //Gotten sector data?
	{
		oldsectordata = (byte*)zalloc(oldsectordatasize, "IMDIMAGE_OLDSECTORDATA", NULL); //Allocate the old sector data!
		if (oldsectordata == NULL) goto errorOutFormat; //Error out if we can't allocate!
		if (emufread64(oldsectordata, 1, oldsectordatasize, f) != oldsectordatasize) goto errorOutFormat; //Read the old sector data!
	}

	//Now, we have the entire track loaded in memory, along with the previous(head), the old track(both heads) and next(tail) tracks for restoration!

	//Here, we can reopen the file for formatting it, write a new track based on the information we now got form the disk and the new head data!
	emufclose64(f); //Close the old file first, we're recreating it!
	f = emufopen64(filename, "wb+"); //Open the image and clear it!
	if (!f) goto errorOutFormat; //Not opened!
	//First, create a new track header!
	newtrackinfo.cylinder = trackinfo.cylinder; //Same cylinder!
	newtrackinfo.head_extrabits = (trackinfo.head_extrabits & IMD_HEAD_HEADNUMBER) | IMD_HEAD_HEADMAPPRESENT | IMD_HEAD_CYLINDERMAPPRESENT; //Head of the track, with head map and cylinder map present!
	newtrackinfo.mode = MFM ? ((speed < 3) ? (3 + speed) : 0) : ((speed < 3) ? (speed) : 0); //Mode: MFM or FM in 500,300,250.
	newtrackinfo.SectorSize = 0xFF; //Custom map!
	newtrackinfo.sectorspertrack = numsectors; //Amount of sectors on this track!
	if (emufwrite64(&newtrackinfo, 1, sizeof(newtrackinfo), f) != sizeof(newtrackinfo)) //Write the new track info!
	{
		emufclose64(f); //Close the file!
		goto errorOutFormat_restore; //Error out and restore!
	}
	//Sector data bytes is in packet format: track,head,number,size!

	//First, sector number map!
	sectordataptr = &sectordata[2]; //Sector number used during formatting!
	for (currentsector = 0; currentsector < numsectors; ++currentsector) //Process all sector numbers!
	{
		b = *sectordataptr; //The sector number!
		if (emufwrite64(&b, 1, sizeof(b), f) != sizeof(b))
		{
			emufclose64(f); //Close the file!
			goto errorOutFormat_restore; //Error out and restore!
		}
		sectordataptr += 4; //Next record!
	}

	//Then, cylinder number map!
	sectordataptr = &sectordata[0]; //Cylinder number used during formatting!
	for (currentsector = 0; currentsector < numsectors; ++currentsector) //Process all cylinder numbers!
	{
		b = *sectordataptr; //The cylinder number!
		if (emufwrite64(&b, 1, sizeof(b), f) != sizeof(b))
		{
			emufclose64(f); //Close the file!
			goto errorOutFormat_restore; //Error out and restore!
		}
		sectordataptr += 4; //Next record!
	}
	//Then, head number map!
	sectordataptr = &sectordata[1]; //Head number used during formatting!
	for (currentsector = 0; currentsector < numsectors; ++currentsector) //Process all head numbers!
	{
		b = *sectordataptr; //The head number!
		if (emufwrite64(&b, 1, sizeof(b), f) != sizeof(b))
		{
			emufclose64(f); //Close the file!
			goto errorOutFormat_restore; //Error out and restore!
		}
		sectordataptr += 4; //Next record!
	}

	//Then, size map!
	sectordataptr = &sectordata[3]; //Size number used during formatting!
	for (currentsector = 0; currentsector < numsectors; ++currentsector) //Process all size numbers!
	{
		b = *sectordataptr; //The head number!
		w = (0x80 << b); //128*2^x is the cylinder size!
		if (emufwrite64(&w, 1, sizeof(w), f) != sizeof(w))
		{
			emufclose64(f); //Close the file!
			goto errorOutFormat_restore; //Error out and restore!
		}
		sectordataptr += 4; //Next record!
	}

	//Then, the sector data(is compressed for easy formatting)!

	for (currentsector = 0; currentsector < numsectors; ++currentsector) //Process all size numbers!
	{
		b = 0x02; //What kind of sector to write: compressed to 1 byte!
		if (emufwrite64(&b, 1, sizeof(b), f) != sizeof(b))
		{
			emufclose64(f); //Close the file!
			goto errorOutFormat_restore; //Error out and restore!
		}
		b = filldata; //What kind of byte to fill!
		if (emufwrite64(&b, 1, sizeof(b), f) != sizeof(b))
		{
			emufclose64(f); //Close the file!
			goto errorOutFormat_restore; //Error out and restore!
		}
	}

	//Finally, the footer!
	if (emufwrite64(tailbuffer, 1, tailbuffersize, f) != tailbuffersize) //Write the tail buffer!
	{
		emufclose64(f); //Close the file!
		goto errorOutFormat_restore; //Error out and restore!
	}
	//Finish up!
	if (sectorsizemap) //Allocated sector size map?
	{
		freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
	}
	if (headbuffer) //Head buffer allocated?
	{
		freez((void**)&headbuffer, headbuffersize, "IMDIMAGE_HEADBUFFER"); //Free the allocated head!
	}
	if (tailbuffer) //Tail buffer allocated?
	{
		freez((void**)&headbuffer, tailbuffersize, "IMDIMAGE_TAILBUFFER"); //Free the allocated tail!
	}
	if (sectornumbermap) //Map allocated?
	{
		freez((void**)&sectornumbermap, trackinfo.sectorspertrack, "IMDIMAGE_SECTORNUMBERMAP"); //Free the allocated sector number map!
	}
	if (cylindermap) //Map allocated?
	{
		freez((void**)&cylindermap, trackinfo.sectorspertrack, "IMDIMAGE_CYLINDERMAP"); //Free the allocated cylinder map!
	}
	if (headmap) //Map allocated?
	{
		freez((void**)&headmap, trackinfo.sectorspertrack, "IMDIMAGE_HEADMAP"); //Free the allocated head map!
	}
	if (oldsectordata) //Sector data allocated?
	{
		freez((void**)&oldsectordata, oldsectordatasize, "IMDIMAGE_OLDSECTORDATA"); //Free the allocated sector data map!
	}
	emufclose64(f); //Close the image!
	return 1; //Success!	

errorOutFormat_restore: //Error out on formatting and restore the file!
	f = emufopen64(filename, "wb+"); //Open the image and clear it!
	if (!f) goto errorOutFormat; //Not opened!
	//First, the previous tracks!
	if (headbuffer && headbuffersize) //Gotten a head buffer?
	{
		if (emufwrite64(headbuffer, 1, headbuffersize, f) != headbuffersize) goto errorOutFormat; //Write the previous tracks back!
	}
	//Now, reached the reformatted track!
	if (emufwrite64(&trackinfo, 1, sizeof(trackinfo), f) != sizeof(trackinfo)) goto errorOutFormat; //Write the track header!
	if (sectornumbermap)
	{
		if (emufwrite64(sectornumbermap, 1, trackinfo.sectorspertrack, f) != trackinfo.sectorspertrack) goto errorOutFormat; //Write the sector number map!
	}
	if (cylindermap) //Cylinder map was present?
	{
		if (emufwrite64(cylindermap, 1, trackinfo.sectorspertrack, f) != trackinfo.sectorspertrack) goto errorOutFormat; //Write the cylinder number map!
	}
	if (headmap) //Head map was present?
	{
		if (emufwrite64(headmap, 1, trackinfo.sectorspertrack, f) != trackinfo.sectorspertrack) goto errorOutFormat; //Write the head map!
	}
	if (sectorsizemap) //Sector size map was present?
	{
		if (emufwrite64(sectorsizemap, 1, (trackinfo.sectorspertrack << 1), f) != (trackinfo.sectorspertrack << 1)) goto errorOutFormat; //Write the sector size map!
	}
	if (oldsectordata)
	{
		if (emufwrite64(oldsectordata, 1, oldsectordatasize, f) != oldsectordatasize) goto errorOutFormat; //Write the sector data!
	}
	if (tailbuffer)
	{
		if (emufwrite64(tailbuffer, 1, tailbuffersize, f) != tailbuffersize) goto errorOutFormat; //Write the next tracks back!
	}
	//Now, the entire file has been restored to it's old state! Finish up the normal way below!

	//Couldn't find the track!
	errorOutFormat: //Erroring out on the formatting process!
	if (sectorsizemap) //Allocated sector size map?
	{
		freez((void**)&sectorsizemap, (trackinfo.sectorspertrack << 1), "IMDIMAGE_SECTORSIZEMAP"); //Free the allocated sector size map!
	}
	if (headbuffer) //Head buffer allocated?
	{
		freez((void**)&headbuffer, headbuffersize, "IMDIMAGE_HEADBUFFER"); //Free the allocated head!
	}
	if (tailbuffer) //Tail buffer allocated?
	{
		freez((void**)&headbuffer, tailbuffersize, "IMDIMAGE_TAILBUFFER"); //Free the allocated tail!
	}
	if (sectornumbermap) //Map allocated?
	{
		freez((void**)&sectornumbermap, trackinfo.sectorspertrack, "IMDIMAGE_SECTORNUMBERMAP"); //Free the allocated sector number map!
	}
	if (cylindermap) //Map allocated?
	{
		freez((void**)&cylindermap, trackinfo.sectorspertrack, "IMDIMAGE_CYLINDERMAP"); //Free the allocated cylinder map!
	}
	if (headmap) //Map allocated?
	{
		freez((void**)&headmap, trackinfo.sectorspertrack, "IMDIMAGE_HEADMAP"); //Free the allocated head map!
	}
	if (oldsectordata) //Sector data allocated?
	{
		freez((void**)&oldsectordata, oldsectordatasize, "IMDIMAGE_OLDSECTORDATA"); //Free the allocated sector data map!
	}
	emufclose64(f); //Close the image!
	return 0; //Invalid IMD file!
}