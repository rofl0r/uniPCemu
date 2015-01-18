#include "headers/types.h" //Only global stuff!
#include "headers/emu/gpu/gpu.h" //GPU stuff!
#include "headers/emu/gpu/gpu_emu.h" //GPU emulator support!
#include "headers/fopen64.h" //64-bit fopen support!
#include "headers/emu/directorylist.h" //Directory list support.

//A dynamic image .DAT data:
byte SIG[6] = {'S','F','D','I','M','G'}; //Signature!

typedef struct
{
byte SIG[6]; //DYN\0
uint_32 headersize; //The size of this header!
int_64 filesize; //The size of the dynamic image, in sectors.
word sectorsize; //The size of a sector (512)
int_64 firstlevellocation; //The location of the first level, in bytes!
int_64 currentsize; //The current file size, in bytes!
} DYNAMICIMAGE_HEADER; //Dynamic image .DAT header.

int_64 lookuptable[4096]; //A full sector lookup table (4096 entries for either block or sector lookup)!

static int writedynamicheader(char *filename, DYNAMICIMAGE_HEADER *header)
{
	if (!isext(filename, "sfdimg")) //Not our dynamic image file?
	{
		return 0; //Not a dynamic image!
	}
	FILE *f = fopen64(filename, "rb+"); //Open!
	if (!f) return 0; //Failed!
	if (fwrite64(header, 1, sizeof(*header), f) != sizeof(*header)) //Failed to write?
	{
		fclose64(f);
		return 0; //Failed!
	}
	fclose64(f);
	return 1; //We've been updated!
}

static int readdynamicheader(char *filename, DYNAMICIMAGE_HEADER *header)
{
	if (!isext(filename, "sfdimg")) //Not our dynamic image file?
	{
		return 0; //Not a dynamic image!
	}
	int is_dynamic = 0; //Not dynamic by default!
	FILE *f = fopen64(filename,"rb"); //Open!
	if (f)
	{
		if (fread64(header,1,sizeof(header),f)!=sizeof(header)) //Failed to read the header?
		{
			is_dynamic = 0; //Not dynamic.
		}
		else
		{
			char *sig = (char *)&header->SIG; //The signature!
			if (!memcmp(sig,&SIG,sizeof(header->SIG)) && header->headersize==sizeof(*header)) //Dynamic image?
			{
				is_dynamic = 1; //Is dynamic!
			}
			else //Dynamic image invalid?
			{
				is_dynamic = 0; //Not dynamic.
			}
		}
		fclose64(f);
	}
	return is_dynamic;
}

int is_dynamicimage(char *filename)
{
	DYNAMICIMAGE_HEADER header; //Header to read!
	return readdynamicheader(filename,&header); //Is dynamic?
}

FILEPOS dynamicimage_getsize(char *filename)
{
	DYNAMICIMAGE_HEADER header; //Header to read!
	if (readdynamicheader(filename,&header)) //Is dynamic?
	{
		return header.filesize*header.sectorsize; //Give the size!
	}
	else
	{
		return 0; //No size!
	}
}

byte dynamicimage_updatesize(char *filename, int_64 size)
{
	DYNAMICIMAGE_HEADER header;
	if (!readdynamicheader(filename, &header)) //Header failed to read?
	{
		return 0; //Failed to update the size!
	}
	header.currentsize = size; //Update the size!
	return writedynamicheader(filename,&header); //Try to update the header!
}

byte dynamicimage_allocatelookuptable(char *filename, int_64 *location, int_64 numentries) //Allocate a table with numentries entries, give location of allocation!
{
	FILE *f;
	DYNAMICIMAGE_HEADER header;
	int_64 newsize, entrysize;
	if (readdynamicheader(filename, &header))
	{
		f = fopen64(filename, "rb+"); //Open the file!
		if (fseek64(f, header.currentsize, SEEK_SET) != 0) //Error seeking to EOF?
		{
			fclose64(f); //Close the file!
			return 0; //Error!
		}
		//We're at EOF!
		*location = header.currentsize; //The location we've found to use!
		entrysize = sizeof(lookuptable[0] * numentries); //Size of the entry!
		memset(&lookuptable, 0, (size_t)entrysize); //Init to empty block table!
		if (fwrite64(&lookuptable, 1, entrysize, f) == entrysize) //Block table allocated?
		{
			newsize = ftell64(f); //New file size!
			fclose64(f); //Close the file!
			return dynamicimage_updatesize(filename, newsize); //Size successfully updated?
		}
		fclose64(f); //Close the file!
	}
	return 0; //Error!
}

int_64 dynamicimage_readlookuptable(char *filename, int_64 location, int_64 numentries, int_64 entry) //Read a table with numentries entries, give location of an entry!
{
	FILE *f;
	DYNAMICIMAGE_HEADER header;
	int_64 entrysize;
	if (readdynamicheader(filename, &header))
	{
		f = fopen64(filename, "rb+"); //Open the file!
		if (fseek64(f, location, SEEK_SET) != 0) //Error seeking to entry?
		{
			fclose64(f); //Close the file!
			return 0; //Error!
		}
		//We're at EOF!
		entrysize = sizeof(lookuptable[0])*numentries; //Size of the entry!
		memset(&lookuptable, 0, (size_t)entrysize); //Clear all entries!
		if (fread64(&lookuptable, 1, entrysize, f) == entrysize) //Block table read?
		{
			fclose64(f); //Close the file!
			if (entry < entrysize) //Lower than the ammount of entries?
			{
				return lookuptable[entry]; //Give the entry!
			}
			//We're invalid, passthrough!
		}
		fclose64(f); //Close the file!
	}
	return 0; //Error: not found!
}

byte dynamicimage_updatelookuptable(char *filename, int_64 location, int_64 numentries, int_64 entry, int_64 value) //Update a table with numentries entries, set location of an entry!
{
	FILE *f;
	DYNAMICIMAGE_HEADER header;
	int_64 entrysize;
	if (readdynamicheader(filename, &header))
	{
		f = fopen64(filename, "rb+"); //Open the file!
		if (fseek64(f, location, SEEK_SET) != 0) //Error seeking to entry?
		{
			fclose64(f); //Close the file!
			return 0; //Error!
		}
		//We're at EOF!
		entrysize = sizeof(lookuptable[0])*numentries; //Size of the entry!
		memset(&lookuptable, 0, (size_t)entrysize); //Clear all entries!
		if (fread64(&lookuptable, 1, entrysize, f) == entrysize) //Block table read?
		{
			fclose64(f); //Close the file!
			if (entrysize>entry) //Lower than the ammount of entries?
			{
				lookuptable[entry] = value; //Update the entry!
				if (fseek64(f, location, SEEK_SET) != 0) //Error seeking to entry?
				{
					fclose64(f);
					return 0; //Error!
				}
				if (fwrite64(&lookuptable, 1, entrysize, f) == entrysize) //Updated?
				{
					fclose64(f);
					return 1; //Updated!
				}
			}
			//We're invalid, passthrough!
		}
		fclose64(f); //Close the file!
	}
	return 0; //Error: not found!
}

static int_64 dynamicimage_getindex(char *filename, uint_32 sector) //Get index!
{
	DYNAMICIMAGE_HEADER header;
	int_64 index;
	if (!readdynamicheader(filename, &header)) //Not dynamic?
	{
		return -1; //Error: not dynamic!
	}
	if (!(index = dynamicimage_readlookuptable(filename, sizeof(header), 1024, ((sector >> 25) & 0x3FF)))) //First level lookup!
	{
		return 0; //Not present!
	}
	if (!(index = dynamicimage_readlookuptable(filename, index, 1024, ((sector >> 15) & 0x3FF)))) //Second level lookup!
	{
		return 0; //Not present!
	}
	if (!dynamicimage_readlookuptable(filename, sizeof(header), 4096, ((sector >> 3) & 0xFFF))) //Sector level lookup!
	{
		return 0; //Not present!
	}
	return index; //We're present at this index, if at all!
}

int dynamicimage_datapresent(char *filename, uint_32 sector) //Get present?
{
	int_64 index;
	index = dynamicimage_getindex(filename, sector); //Try to get the index!
	if (index == -1) //Not a dynamic image?
	{
		return 0; //Invalid file!
	}
	return (index!=0); //We're present?
}

static int dynamicimage_setindex(char *filename, uint_32 sector, int_64 index)
{
	DYNAMICIMAGE_HEADER header;
	int_64 firstlevellocation,secondlevellocation,sectorlevellocation;
	int_64 firstlevelentry, secondlevelentry, sectorlevelentry;
	firstlevelentry = ((sector >> 25) & 0x3FF); //First level entry!
	secondlevelentry = ((sector >> 15) & 0x3FF); //Second level entry!
	sectorlevelentry = ((sector >> 3) & 0xFFF); //Sector level entry!

	if (!readdynamicheader(filename, &header)) //Not dynamic?
	{
		return -1; //Error: not dynamic!
	}
	firstlevellocation = header.firstlevellocation; //First level location!
	//First, check the first level lookup table is present!
	if (!firstlevellocation) //No first level present yet?
	{
		if (!index) return 1; //OK: we don't need to be allocated!
		if (!dynamicimage_allocatelookuptable(filename, &firstlevellocation, 1024)) //Lookup table failed to allocate?
		{
			dynamicimage_updatesize(filename, header.currentsize); //Revert!
			return 0; //Failed!
		}
		if (!readdynamicheader(filename, &header)) //Update header?
		{
			return 0; //Failed!
		}
		header.firstlevellocation = firstlevellocation; //Update the first level location!
		if (!writedynamicheader(filename, &header)) //Header failed to update?
		{
			return 0; //Failed: we can't process the dynamic image header!
		}
	}
	//We're present: process the first level lookup table!
	if (!(secondlevellocation = dynamicimage_readlookuptable(filename, firstlevellocation, 1024, firstlevelentry))) //First level lookup failed?
	{
		if (!index) return 1; //OK: we don't need to be allocated!
		if (!dynamicimage_allocatelookuptable(filename, &secondlevellocation, 1024)) //Lookup table failed to allocate?
		{
			dynamicimage_updatesize(filename, header.currentsize); //Revert!
			return 0; //Failed!
		}
		if (!dynamicimage_updatelookuptable(filename, firstlevellocation, 1024,firstlevelentry,secondlevellocation)) //Lookup table failed to assign?
		{
			dynamicimage_updatesize(filename, header.currentsize); //Revert!
			return 0; //Failed!
		}
		//Now, allow the next level to be updated: we're ready to process!
	}
	if (!(sectorlevellocation = dynamicimage_readlookuptable(filename, secondlevellocation, 1024,secondlevelentry))) //Second level lookup failed?
	{
		if (!index) return 1; //OK: we don't need to be allocated!
		if (!dynamicimage_allocatelookuptable(filename, &sectorlevellocation, 4096)) //Lookup table failed to allocate?
		{
			dynamicimage_updatesize(filename, header.currentsize); //Revert!
			return 0; //Failed!
		}
		if (!dynamicimage_updatelookuptable(filename, firstlevellocation, 4096, secondlevelentry, sectorlevellocation)) //Lookup table failed to assign?
		{
			dynamicimage_updatesize(filename, header.currentsize); //Revert!
			return 0; //Failed!
		}
		//Now, allow the next level to be updated: we're ready to process!
	}
	if (!dynamicimage_updatelookuptable(filename, sectorlevellocation, 4096, sectorlevelentry, index)) //Update the lookup table, if possible!
	{
		dynamicimage_updatesize(filename, header.currentsize); //Revert!
		return 0; //Failed!
	}
	return 1; //We've succeeded: the sector has been allocated and set!
}

int dynamicimage_writesector(char *filename,uint_32 sector, void *buffer) //Write a 512-byte sector! Result=1 on success, 0 on error!
{
	FILE *dev;
	dev = fopen64(filename,"rb+"); //Open file for reading!
	int present = dynamicimage_datapresent(filename,sector); //Data present?
	static byte emptyblock[512]; //An empty block!
	static byte emptyready = 0;
	int_64 newsize;
	if (present!=-1) //Valid sector?
	{
		if (present) //Data present?
		{
			fseek64(dev,dynamicimage_getindex(filename,sector),SEEK_SET); //Goto location!
			fwrite64(buffer,1,512,dev); //Write sector always!
		}
		else //Not written yet?
		{
			if (!emptyready)
			{
				memset(&emptyblock,0,sizeof(emptyblock)); //To detect an empty block!
				emptyready = 1; //We're ready to be used!
			}
			if (!memcmp(&emptyblock,buffer,sizeof(emptyblock))) //Empty?
			{
				fclose64(dev); //Not needed!
				return TRUE; //We don't need to allocate/write an empty block, as it's already empty by default!
			}
			fseek64(dev,0,SEEK_END); //Goto EOF!

			if (dynamicimage_setindex(filename,sector,ftell64(dev))) //Set to go?
			{
				if (fwrite64(buffer,1,512,dev)==512) //Write the buffer to the file!
				{
					newsize = ftell64(dev); //New file size!
					fclose64(dev); //Close the device!
					if (dynamicimage_updatesize(filename, newsize)) //Updated?
					{
						return TRUE; //OK!
					}
					else
					{
						dynamicimage_setindex(filename, sector, 0); //Clear the index: we've become invalid!
						return FALSE; //ERROR!
					}
				}
				else
				{
					dynamicimage_setindex(filename, sector, 0); //Clear our index! We don't exist anymore: write failed!
					fclose64(dev); //Close it!
					return FALSE; //Error!
				}
			}
			else //Failed to set index!
			{
				fclose64(dev);
				return FALSE; //Error!
			}
		}
	}
	else //Terminate loop: invalid sector!
	{
		fclose64(dev);
		return FALSE; //Error!
	}
	fclose64(dev); //Close it!
	return TRUE; //Written!
}

int dynamicimage_readsector(char *filename,uint_32 sector, void *buffer) //Read a 512-byte sector! Result=1 on success, 0 on error!
{
	FILE *dev;
	dev = fopen64(filename,"rb"); //Open file for reading!
	int present = dynamicimage_datapresent(filename,sector); //Data present?
	if (present!=-1) //Valid sector?
	{
		if (present) //Data present?
		{
			int_64 index;
			index = dynamicimage_getindex(filename,sector);
			if (fseek64(dev,index,SEEK_SET)) //Seek failed?
			{
				fclose64(dev);
				return FALSE; //Error: file is corrupt?
			}
			if (fread64(buffer,1,512,dev)!=512) //Error reading sector?
			{
				fclose64(dev);
				return FALSE; //Error: file is corrupt?
			}
		}
		else //Present, but not written yet?
		{
			memset(buffer,0,512); //Empty sector!
		}
	}
	else //Terminate loop: invalid sector!
	{
		fclose64(dev);
		return FALSE; //Error!
	}
	fclose64(dev); //Close it!
	return TRUE; //Written!
}

FILEPOS generateDynamicImage(char *filename, FILEPOS size, int percentagex, int percentagey)
{
	DYNAMICIMAGE_HEADER header;
	FILE *f;
	if ((percentagex!=-1) && (percentagey!=-1)) //To show percentage?
	{
		EMU_gotoxy(percentagex,percentagey); //Goto x,y coordinates!
		GPU_EMU_printscreen(-1,-1,"%2.1f%%",0.0f); //Show first percentage!
	}

	FILEPOS numblocks;
	numblocks = size;
	numblocks >>= 9; //Divide by 512 for the ammount of sectors!

	if (size != 0) //Has size?
	{
		f = fopen64(filename, "wb"); //Start generating dynamic info!
		memcpy(&header.SIG,SIG,sizeof(header.SIG)); //Set the signature!
		header.headersize = sizeof(header); //The size of the header to validate!
		header.filesize = numblocks; //Ammount of blocks!
		header.sectorsize = 512; //512 bytes per sector!
		header.currentsize = sizeof(header); //The current file size. This is updated as data is appended to the file.
		header.firstlevellocation = 0; //No first level createn yet!
		if (fwrite64(&header,1,sizeof(header),f)!=sizeof(header)) //Failed to write the header?
		{
			fclose64(f); //Close the file!
			return 0; //Error: couldn't write the header!
		}
		fclose64(f); //Close info file!
	}
	
	if ((percentagex!=-1) && (percentagey!=-1)) //To show percentage?
	{
		EMU_gotoxy(percentagex,percentagey); //Goto x,y coordinates!
		GPU_EMU_printscreen(-1,-1,"%2.1f%%",100.0f); //Show final percentage!
	}
	return (numblocks<<9); //Give generated size!
}