#include "headers/types.h" //Basic types!
#include "headers/emu/gpu/gpu.h" //Basic output!
#include "headers/emu/gpu/gpu_emu.h" //GPU emulator support!

FILEPOS staticimage_getsize(char *filename)
{
	FILE *f;
	f = fopen64(filename,"rb+"); //Open!
	if (!f) //Not found?
	{
		return 0; //No size!
	}
	fseek64(f,0,SEEK_END); //Find end!
	FILEPOS result;
	result = ftell64(f); //Current pos = size!
	fclose64(f); //Close the file!
	return result; //Give the result!
}

int staticimage_writesector(char *filename,uint_32 sector, void *buffer) //Write a 512-byte sector! Result=1 on success, 0 on error!
{
	FILE *f;
	f = fopen64(filename,"rb+"); //Open!
	fseek64(f,sector*512,SEEK_SET); //Find block info!
	if (ftell64(f)!=(sector*512)) //Not found?
	{
		fclose64(f); //Close the file!
		return FALSE; //Error!
	}
	if (fwrite64(buffer,1,512,f)==512) //Written?
	{
		fclose64(f); //Close!
		return TRUE; //OK!
	}
	fclose64(f); //Close!
	return FALSE; //Error!
}

int staticimage_readsector(char *filename,uint_32 sector, void *buffer) //Read a 512-byte sector! Result=1 on success, 0 on error!
{
	FILE *f;
	f = fopen64(filename,"rb"); //Open!
	fseek64(f,sector*512,SEEK_SET); //Find block info!
	if (ftell64(f)!=(sector*512)) //Not found?
	{
		fclose64(f); //Close the file!
		return FALSE; //Error!
	}
	if (fread64(buffer,1,512,f)==512) //Read?
	{
		fclose64(f); //Close!
		return TRUE; //OK!
	}
	fclose64(f); //Close!
	return FALSE; //Error!
}

void generateStaticImage(char *filename, FILEPOS size, int percentagex, int percentagey) //Generate a static image!
{
	FILE *f;
	f = fopen64(filename,"wb"); //Generate file!
	if ((percentagex!=-1) && (percentagey!=-1)) //To show percentage?
	{
		EMU_gotoxy(percentagex,percentagey); //Goto x,y coordinates!
		GPU_EMU_printscreen(-1,-1,"%2.1f%%",0.0f); //Show first percentage!
	}
	FILEPOS sizeleft = size; //Init size left!

	byte buffer[4096]; //Buffer!
	memset(buffer,0,sizeof(buffer)); //Clear!
	
	while (sizeleft) //Left?
	{
		sizeleft -= fwrite64(&buffer,1,sizeof(buffer),f); //We've processed some!
		if ((percentagex!=-1) && (percentagey!=-1)) //To show percentage?
		{
			EMU_gotoxy(percentagex,percentagey); //Goto x,y coordinates!
			GPU_EMU_printscreen(-1,-1,"%2.1f",((size-sizeleft)/size)*100.0f); //Show percentage!
			delay(1); //Allow update of the screen, if needed!
		}
	}
	fclose64(f);
	if ((percentagex!=-1) && (percentagey!=-1)) //To show percentage?
	{
		EMU_gotoxy(percentagex,percentagey); //Goto x,y coordinates!
		GPU_EMU_printscreen(-1,-1,"%2.1f%%",100.0f); //Show percentage!
	}
}