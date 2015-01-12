#include "headers/types.h" //Only global stuff!
#include "headers/emu/gpu/gpu.h" //GPU stuff!
#include "headers/emu/gpu/gpu_emu.h" //GPU emulator support!
#include "headers/fopen64.h" //64-bit fopen support!

//A dynamic image .DAT data:
typedef struct
{
byte present; //Present data, bit-encoded!
int_64 sector[8]; //What sector in the file, signed 64-bit format!
} DYNAMICIMAGE_ENTRY; //Dynamic image .DAT entry, representing a 4096 byte block!

byte SIG[4] = {'D','Y','N',0}; //Signature!

typedef struct
{
byte SIG[4]; //DYN\0
uint_32 headersize; //The size of this header!
int_64 filesize; //The size of the dynamic image, in sectors.
word sectorsize; //The size of a sector (512)
} DYNAMICIMAGE_HEADER; //Dynamic image .DAT header.

static int readdynamicheader(char *filename, DYNAMICIMAGE_HEADER *header)
{
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
	char imageinfo[256];
	bzero(&imageinfo,sizeof(imageinfo)); //Reset
	strcpy(imageinfo,filename); //Init!
	strcat(imageinfo,".DAT"); //Info file!
	DYNAMICIMAGE_HEADER header; //Header to read!
	return readdynamicheader(&imageinfo[0],&header); //Is dynamic?
}

FILEPOS dynamicimage_getsize(char *filename)
{
	char imageinfo[256];
	bzero(&imageinfo,sizeof(imageinfo)); //Reset
	strcpy(imageinfo,filename); //Init!
	strcat(imageinfo,".DAT"); //Info file!
	DYNAMICIMAGE_HEADER header; //Header to read!
	if (readdynamicheader(&imageinfo[0],&header)) //Is dynamic?
	{
		return header.filesize*header.sectorsize; //Give the size!
	}
	else
	{
		return 0; //No size!
	}
}

int dynamicimage_datapresent(char *filename,uint_32 sector) //Get present?
{
	FILE *f;
	DYNAMICIMAGE_ENTRY entry; //An entry for our usage!
	char imageinfo[256];
	bzero(&imageinfo,sizeof(imageinfo)); //Reset
	strcpy(imageinfo,filename); //Init!
	strcat(imageinfo,".DAT"); //Info file!
	DYNAMICIMAGE_HEADER header;
	if (!readdynamicheader(imageinfo,&header)) //Not dynamic?
	{
		return -1; //Error: not dynamic!
	}
	f = fopen64(imageinfo,"rb+"); //Open!
	fseek64(f,sizeof(DYNAMICIMAGE_HEADER)+(sector/NUMITEMS(entry.sector))*sizeof(entry),SEEK_SET); //Find block info!
	if (feof(f) || ftell64(f)!=(sector/NUMITEMS(entry.sector))*sizeof(entry)) //EOF or not found?
	{
		fclose64(f); //Close!
		return -1; //Cannot be present!
	}
	fread64(&entry,1,sizeof(entry),f); //Read info!
	fclose64(f); //Close!
	
	byte bitnr = 7-SAFEMOD(sector,NUMITEMS(entry.sector)); //Bit number, first is first etc. from high to low!
	return (entry.present&(1<<bitnr)>>bitnr); //Present?
}

static int_64 dynamicimage_getindex(char *filename, uint_32 sector) //Get index!
{
	if (!dynamicimage_datapresent(filename,sector)) //No data is no index?
	{
		return 0; //Abort: we can't have an index of a non-existing sector!
	}

	FILE *f;
	DYNAMICIMAGE_ENTRY entry; //An entry for our usage!
	char imageinfo[256];
	bzero(&imageinfo,sizeof(imageinfo)); //Reset
	strcpy(imageinfo,filename); //Init!
	strcat(imageinfo,".DAT"); //Info file!
	DYNAMICIMAGE_HEADER header;
	if (!readdynamicheader(imageinfo,&header)) //Not dynamic?
	{
		return -1; //Error: not dynamic!
	}
	f = fopen64(imageinfo,"rb"); //Open!
	fseek64(f,sizeof(DYNAMICIMAGE_HEADER)+(sector/NUMITEMS(entry.sector))*sizeof(entry),SEEK_SET); //Find block info!
	if (feof(f) || ftell64(f)!=(sector/NUMITEMS(entry.sector))*sizeof(entry)) //EOF or not found?
	{
		fclose64(f); //Close!
		return 0; //Cannot be present!
	}
	fread64(&entry,1,sizeof(entry),f); //Read info!
	fclose64(f); //Close!
	
	byte sectornr = SAFEMOD(sector,NUMITEMS(entry.sector)); //Bit number, first is first etc. from high to low!
	return entry.sector[sectornr]; //Give the index!
}

static int dynamicimage_setindex(char *filename, uint_32 sector, int_64 index)
{
	FILE *f;
	DYNAMICIMAGE_ENTRY entry; //An entry for our usage!
	char imageinfo[256];
	bzero(&imageinfo,sizeof(imageinfo)); //Reset
	strcpy(imageinfo,filename); //Init!
	strcat(imageinfo,".DAT"); //Info file!
	DYNAMICIMAGE_HEADER header;
	if (!readdynamicheader(imageinfo,&header)) //Not dynamic?
	{
		return -1; //Error: not dynamic!
	}
	f = fopen64(imageinfo,"rb+"); //Open!
	fseek64(f,sizeof(DYNAMICIMAGE_HEADER)+(sector/NUMITEMS(entry.sector))*sizeof(entry),SEEK_SET); //Find block info!
	if (feof(f) || ftell64(f)!=(sector/NUMITEMS(entry.sector))*sizeof(entry)) //EOF or not found?
	{
		fclose64(f); //Close!
		return -1; //Cannot be present!
	}
	fread64(&entry,1,sizeof(entry),f); //Read info!

	//Now adjust the sector number present!
	byte sectornr = SAFEMOD(sector,NUMITEMS(entry.sector)); //Bit number, first is first etc. from high to low!
	entry.sector[sectornr] = index; //Set the new index!
	entry.present |= (1<<(7-sectornr)); //Flag as present!
	//Finally, write it back to the file!
	fseek64(f,sizeof(DYNAMICIMAGE_HEADER)+(sector/NUMITEMS(entry.sector))*sizeof(entry),SEEK_SET); //Find block info again!
	if (fwrite64(&entry,1,sizeof(entry),f)!=sizeof(entry)) //Error?
	{
		fclose64(f);
		return FALSE; //Error!
	}
	fclose64(f); //Close!
	return TRUE; //OK!
}

static int dynamicimage_clearindex(char *filename, uint_32 sector)
{
	FILE *f;
	DYNAMICIMAGE_ENTRY entry; //An entry for our usage!
	char imageinfo[256];
	bzero(&imageinfo,sizeof(imageinfo)); //Reset
	strcpy(imageinfo,filename); //Init!
	strcat(imageinfo,".DAT"); //Info file!
	DYNAMICIMAGE_HEADER header;
	if (!readdynamicheader(imageinfo,&header)) //Not dynamic?
	{
		return -1; //Error: not dynamic!
	}
	f = fopen64(imageinfo,"rb+"); //Open!
	fseek64(f,sizeof(DYNAMICIMAGE_HEADER)+(sector/NUMITEMS(entry.sector))*sizeof(entry),SEEK_SET); //Find block info!
	if (feof(f) || ftell64(f)!=(sector/NUMITEMS(entry.sector))*sizeof(entry)) //EOF or not found?
	{
		fclose64(f); //Close!
		return -1; //Cannot be present!
	}
	fread64(&entry,1,sizeof(entry),f); //Read info!

	//Now adjust the sector number present!
	byte sectornr = SAFEMOD(sector,NUMITEMS(entry.sector)); //Bit number, first is first etc. from high to low!
	entry.present &= ~(1<<(7-sectornr)); //Flag as not present!
	//Finally, write it back to the file!
	fseek64(f,sizeof(DYNAMICIMAGE_HEADER)+(sector/NUMITEMS(entry.sector))*sizeof(entry),SEEK_SET); //Find block info again!
	if (fwrite64(&entry,1,sizeof(entry),f)!=sizeof(entry)) //Error?
	{
		fclose64(f);
		return FALSE; //Error!
	}
	fclose64(f); //Close!
	return TRUE; //OK!
}

int dynamicimage_writesector(char *filename,uint_32 sector, void *buffer) //Write a 512-byte sector! Result=1 on success, 0 on error!
{
	FILE *dev;
	dev = fopen64(filename,"rb+"); //Open file for reading!
	int present = dynamicimage_datapresent(filename,sector); //Data present?
	static byte emptyblock[512]; //An empty block!
	static byte emptyready = 0;
	if (present!=-1) //Valid sector?
	{
		if (present) //Data present?
		{
			fseek64(dev,dynamicimage_getindex(filename,sector)*512,SEEK_SET); //Goto location!
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
					fclose64(dev); //Close the device!
					return TRUE; //Written!
				}
				else
				{
					fclose64(dev); //Close it!
					dynamicimage_clearindex(filename,sector); //Clear it: we're not used!
					return FALSE; //Error!
				}
			}
			else //Failed to set!
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
			uint_32 index;
			index = dynamicimage_getindex(filename,sector);
			fseek64(dev,index<<9,SEEK_SET); //Goto location!
			if (ftell64(dev)!=(index<<9)) //Seek failed?
			{
				fclose64(dev);
				return FALSE; //Error: file is corrupt?
			}
			if (fread64(buffer,1,512,dev)!=512); //Error reading sector?
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
	FILE *f;
	f = fopen64(filename,"w"); //Generate image!
	fclose64(f); //No data in image!
	char imageinfo[256]; //For info filename!
	bzero(&imageinfo,sizeof(imageinfo)); //Reset
	strcpy(imageinfo,filename); //Init!
	strcat(imageinfo,".DAT"); //Info file!
	f = fopen64(imageinfo,"wb"); //Start generating dynamic info!
	if ((percentagex!=-1) && (percentagey!=-1)) //To show percentage?
	{
		EMU_gotoxy(percentagex,percentagey); //Goto x,y coordinates!
		GPU_EMU_printscreen(-1,-1,"%2.1f%%",0.0f); //Show first percentage!
	}

	DYNAMICIMAGE_ENTRY entry; //An entry for our usage!
	memset(&entry,0,sizeof(entry)); //Clear entry!

	FILEPOS counter = 0; //For counting the current position!
	if (size!=0) //Has size?
	{
		DYNAMICIMAGE_HEADER header;
		memcpy(&header.SIG,SIG,sizeof(header.SIG)); //Set the signature!
		header.headersize = sizeof(header); //The size of the header to validate!
		FILEPOS numblocks;
		numblocks = counter = (size/4096) + (((size%4096)!=0)?1:0); //Ammount of blocks to process; Any data left will be rounded up to X sectors!
		header.filesize = (numblocks<<3); //Ammount of blocks!
		header.sectorsize = 512; //512 bytes per sector!
		if (fwrite64(&header,1,sizeof(header),f)!=sizeof(header)) //Failed to write the header?
		{
			fclose64(f); //Close the file!
			return 0; //Error: couldn't write the header!
		}
		for (;counter--;) //Loop blocks needed!
		{
			fwrite64(&entry,1,sizeof(entry),f); //Write an entry!
			if ((percentagex!=-1) && (percentagey!=-1)) //To show percentage?
			{
				EMU_gotoxy(percentagex,percentagey); //Goto x,y coordinates!
				GPU_EMU_printscreen(-1,-1,"%2.1f",(((numblocks-counter)/numblocks)*100.0f)); //Show percentage!
				delay(1); //Allow update of the screen, if needed!
			}
		}
	}
	
	if ((percentagex!=-1) && (percentagey!=-1)) //To show percentage?
	{
		EMU_gotoxy(percentagex,percentagey); //Goto x,y coordinates!
		GPU_EMU_printscreen(-1,-1,"%2.1f%%",100.0f); //Show final percentage!
	}
	fclose64(f); //Close info file!
	return (counter*4096); //Give generated size!
}