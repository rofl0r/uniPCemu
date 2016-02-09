#include "headers/types.h" //Basic types!
#include "headers/emu/gpu/gpu.h" //Basic output!
#include "headers/emu/gpu/gpu_emu.h" //GPU emulator support!
#include "headers/fopen64.h" //64-bit fopen support!
#include "headers/hardware/floppy.h" //Disk image size suppor

byte is_staticimage(char *filename)
{
	FILE *f;
	f = emufopen64(filename, "rb"); //Open file!
	if (!f)
	{
		return 0; //Invalid file: file not found!
	}
	if (emufseek64(f, 0, SEEK_END)) //Failed to seek to EOF?
	{
		emufclose64(f);
		return 0; //Not static!
	}
	int_64 filesize;
	filesize = emuftell64(f); //Get the file size!
	emufclose64(f); //Close the file!
	if (filesize <= 0) //Invalid size or empty?
	{
		return 0; //Not static: invalid file size!
	}
	if (((filesize >> 9) << 9) != filesize) //Not a multiple of 512 bytes?
	{
		return 0; //Not static: invalid sector size!
	}
	return 1; //We're a static image: we're a multiple of 512 bytes and have contents!
}

FILEPOS staticimage_getsize(char *filename)
{
	if (strcmp(filename, "") == 0) return 0; //Not mountable!
	FILE *f;
	f = emufopen64(filename,"rb"); //Open!
	if (!f) //Not found?
	{
		return 0; //No size!
	}
	emufseek64(f,0,SEEK_END); //Find end!
	FILEPOS result;
	result = emuftell64(f); //Current pos = size!
	emufclose64(f); //Close the file!
	return result; //Give the result!
}

byte staticimage_writesector(char *filename,uint_32 sector, void *buffer) //Write a 512-byte sector! Result=1 on success, 0 on error!
{
	FILE *f;
	f = emufopen64(filename,"rb+"); //Open!
	++sector; //Find the next sector!
	if (emufseek64(f, (uint_64)sector << 9, SEEK_SET)) //Invalid sector!
	{
		emufclose64(f); //Close the file!
		return 0; //Limit broken!
	}
	if (emuftell64(f) != ((int_64)sector << 9)) //Invalid sector!
	{
		emufclose64(f); //Close the file!
		return 0; //Limit broken!
	}
	--sector; //Goto selected sector!
	emufseek64(f, (uint_64)sector << 9, SEEK_SET); //Find block info!
	if (emuftell64(f) != ((int_64)sector << 9)) //Not found?
	{
		emufclose64(f); //Close the file!
		return FALSE; //Error!
	}
	if (emufwrite64(buffer,1,512,f)==512) //Written?
	{
		emufclose64(f); //Close!
		return TRUE; //OK!
	}
	emufclose64(f); //Close!
	return FALSE; //Error!
}

byte staticimage_readsector(char *filename,uint_32 sector, void *buffer) //Read a 512-byte sector! Result=1 on success, 0 on error!
{
	FILE *f;
	f = emufopen64(filename,"rb"); //Open!
	emufseek64(f,(uint_64)sector<<9,SEEK_SET); //Find block info!
	if (emuftell64(f)!=((int_64)sector<<9)) //Not found?
	{
		emufclose64(f); //Close the file!
		return FALSE; //Error!
	}
	if (emufread64(buffer,1,512,f)==512) //Read?
	{
		emufclose64(f); //Close!
		return TRUE; //OK!
	}
	emufclose64(f); //Close!
	return FALSE; //Error!
}

void generateStaticImage(char *filename, FILEPOS size, int percentagex, int percentagey) //Generate a static image!
{
	FILEPOS sizeleft = size; //Init size left!
	byte buffer[4096]; //Buffer!
	double percentage;
	FILE *f;
	int_64 byteswritten, totalbyteswritten = 0;
	f = emufopen64(filename,"wb"); //Generate file!
	if ((percentagex!=-1) && (percentagey!=-1)) //To show percentage?
	{
		EMU_locktext();
		GPU_EMU_printscreen(percentagex,percentagey,"%2.1f%%",0.0f); //Show first percentage!
		EMU_unlocktext();
	}

	memset(buffer, 0, sizeof(buffer)); //Clear!

	while (sizeleft) //Left?
	{
		byteswritten = emufwrite64(&buffer,1,sizeof(buffer),f); //We've processed some!
		if (byteswritten != sizeof(buffer)) //An error occurred!
		{
			emufclose64(f); //Close the file!
			delete_file(".",filename); //Remove the file!
			return; //Abort!
		}
		if ((percentagex!=-1) && (percentagey!=-1)) //To show percentage?
		{
			sizeleft -= byteswritten; //Less left to write!
			totalbyteswritten += byteswritten; //Add to the ammount processed!
			percentage = (double)totalbyteswritten;
			percentage /= (double)size;
			percentage *= 100.0f;
			EMU_locktext();
			GPU_EMU_printscreen(percentagex,percentagey,"%2.1f%%",(float)percentage); //Show percentage!
			EMU_unlocktext();
			#ifdef __PSP__
				delay(0); //Allow update of the screen, if needed!
			#endif
		}
	}
	emufclose64(f);
	if ((percentagex!=-1) && (percentagey!=-1)) //To show percentage?
	{
		EMU_locktext();
		GPU_EMU_printscreen(percentagex,percentagey,"%2.1f%%",100.0f); //Show percentage!
		EMU_unlocktext();
	}
}

void generateFloppyImage(char *filename, FLOPPY_GEOMETRY *geometry, int percentagex, int percentagey) //Generate a floppy image!
{
	if (!geometry) return; //Invalid geometry?
	FILEPOS size = (FILEPOS)geometry->KB; //Init KB!
	FILEPOS sizeleft; //Init size left!
	FILEPOS block = 0; //Current block!
	size <<= 10; //Convert kb to Kilobytes of data!
	sizeleft = size; //Load the size that's left!
	byte buffer[1024]; //Buffer!
	double percentage;
	FILE *f;
	int_64 byteswritten, totalbyteswritten = 0;
	f = emufopen64(filename,"wb"); //Generate file!
	if ((percentagex!=-1) && (percentagey!=-1)) //To show percentage?
	{
		EMU_locktext();
		GPU_EMU_printscreen(percentagex,percentagey,"%2.1f%%",0.0f); //Show first percentage!
		EMU_unlocktext();
	}

	while (sizeleft) //Left?
	{
		if (!block) //First block?
		{
			//Create boot sector and first FAT entry!
			memset(buffer, 0, sizeof(buffer)); //Clear!
			buffer[0] = 0xEB; //JMP 3E
			buffer[1] = 0x3C;
			buffer[2] = 0x90; //NOP!
			buffer[3] = 'M';
			buffer[4] = 'S';
			buffer[5] = 'W';
			buffer[6] = 'I';
			buffer[7] = 'N';
			buffer[8] = '4';
			buffer[9] = '.';
			buffer[10] = '1'; //Microsoft recommens MSWIN4.1
			word bps = 512; //Bytes per sector! Use 512 in our case!
			buffer[11] = (bps&0xFF); //This is...
			buffer[12] = (bps>>8)&0xFF; //... X bytes per sector!
			byte counter=0x80; //Maximum value for sectors per cluster!
			word sectorsize = geometry->ClusterSize; //The sector size is actually the cluster size!
			for (;(((bps<<counter)>sectorsize) && counter);) counter >>= 1; //Try to find the cluster size if possible! Default to sector size!
			if (!counter) counter = 1; //Default to 512 bytes!
			buffer[13] = counter; //Sectors per cluster, multiple of 2!
			buffer[14] = 1; //This is...
			buffer[15] = 0; //Reserved sectors!
			buffer[16] = 1; //1 FAT copy!
			buffer[17] = 224; //Number of...
			buffer[18] = 0; //Root directory entries!
			buffer[19] = (geometry->KB>>8)&0xFF; //Ammount of sectors on the disk, in sectors!
			buffer[20] = ((geometry->KB>>8)>>8)&0xFF; //See above.
			buffer[21] = geometry->MediaDescriptorByte; //Our media descriptor byte!
			buffer[22] = 9; //Number of sectors per FAT!
			buffer[23] = 0; //High byte of above.
			buffer[24] = (geometry->SPT&0xFF);
			buffer[25] = (geometry->SPT>>8)&0xFF; //Sectors per track!
			buffer[26] = geometry->sides;
			buffer[27] = (geometry->sides>>8)&0xFF; //How many sides!
			buffer[28] = 0; //No hidden...
			buffer[29] = 0; //... Sectors!
			//Now the bootstrap!
			buffer[30] = 0xCD; //We're an
			buffer[31] = 0x18; //Non-bootable disk until overwritten by an OS!
			//Finally, our signature!
			buffer[510] = 0x55; //Signature 55 aa
			buffer[511] = 0xAA; //Signature 55 aa
			//Now the FAT itself (empty)!
			buffer[512] = geometry->MediaDescriptorByte; //Copy of the media descriptor byte!
			buffer[513] = 0xF8; //High 4 bits of the first entry is F. The second entry contains FF8 for EOF.
			buffer[514] = 0xFF; //High 8 bits of the EOF marker!
			//The rest of the FAT is initialised to 0 (unallocated).
			block = 1; //We're starting normal data!
		}
		else if (block==1) //Second block?
		{
			memset(buffer, 0, sizeof(buffer)); //Clear the buffer from now on!
			block = 2; //Start plain data to 0!
		}
		byteswritten = emufwrite64(&buffer,1,sizeof(buffer),f); //We've processed some!
		if (byteswritten != sizeof(buffer)) //An error occurred!
		{
			emufclose64(f); //Close the file!
			delete_file(".",filename); //Remove the file!
			return; //Abort!
		}
		if ((percentagex!=-1) && (percentagey!=-1)) //To show percentage?
		{
			sizeleft -= byteswritten; //Less left to write!
			totalbyteswritten += byteswritten; //Add to the ammount processed!
			percentage = (double)totalbyteswritten;
			percentage /= (double)size;
			percentage *= 100.0f;
			EMU_locktext();
			GPU_EMU_printscreen(percentagex,percentagey,"%2.1f%%",(float)percentage); //Show percentage!
			EMU_unlocktext();
			#ifdef __PSP__
				delay(0); //Allow update of the screen, if needed!
			#endif
		}
	}
	emufclose64(f);
	if ((percentagex!=-1) && (percentagey!=-1)) //To show percentage?
	{
		EMU_locktext();
		GPU_EMU_printscreen(percentagex,percentagey,"%2.1f%%",100.0f); //Show percentage!
		EMU_unlocktext();
	}
}