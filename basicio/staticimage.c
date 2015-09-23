#include "headers/types.h" //Basic types!
#include "headers/emu/gpu/gpu.h" //Basic output!
#include "headers/emu/gpu/gpu_emu.h" //GPU emulator support!
#include "headers/fopen64.h" //64-bit fopen support!

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
	if (emuftell64(f) != ((uint_64)sector << 9)) //Invalid sector!
	{
		emufclose64(f); //Close the file!
		return 0; //Limit broken!
	}
	--sector; //Goto selected sector!
	emufseek64(f, (uint_64)sector << 9, SEEK_SET); //Find block info!
	if (emuftell64(f) != ((uint_64)sector << 9)) //Not found?
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
	if (emuftell64(f)!=((uint_64)sector<<9)) //Not found?
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