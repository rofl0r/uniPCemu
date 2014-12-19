#include "headers/hardware/vga.h" //VGA support!
#include "headers/emu/state.h" //Our own data and support etc.
#include "headers/support/crc32.h" //CRC32 support!

extern CPU_type CPU; //CPU!
extern GPU_type GPU; //GPU!
extern VGA_Type *ActiveVGA; //VGA!
extern Handler CBHandlers[CB_MAX]; //Handlers!

SAVED_CPU_STATE_HEADER SaveStatus_Header; //SaveStatus structure!


//Version of save state!
#define SAVESTATE_MAIN_VER 1
#define SAVESTATE_SUB_VER 0

void EMU_SaveStatus(char *filename) //Save the status to file or memory
{
	return; //Not working ATM!
//First, save custom status!
	SaveStatus_Header.CPU = sizeof(CPU); //CPU size!
	SaveStatus_Header.GPU = sizeof(GPU); //GPU size!
	SaveStatus_Header.VGA = sizeof(*ActiveVGA); //VGA size!
	SaveStatus_Header.MMU_size = MEMsize(); //MMU size!
	memcpy(&SaveStatus_Header.CBHandlers,&CBHandlers,sizeof(&CBHandlers)); //Callbacks!
	if (strcmp(filename,"")!=0) //Filename set (Save to file)?
	{
		FILE *f;
		f = fopen(filename,"wb"); //Open file for writing data!
		char header[2] = {'S','S'}; //Header!
		fwrite(&header,1,sizeof(header),f); //Identifier!
		int i;
		i = SAVESTATE_MAIN_VER;
		fwrite(&i,1,sizeof(i),f); //Main version!
		i = SAVESTATE_SUB_VER;
		fwrite(&i,1,sizeof(i),f); //Sub version!
		SaveStatus_Header.checksum = CRC32((char *)&SaveStatus_Header.data,sizeof(SaveStatus_Header.data)); //Generate checksum!
		fwrite(&SaveStatus_Header,1,sizeof(SaveStatus_Header),f); //Write SaveStatus Header!
		fwrite(&CPU,1,sizeof(CPU),f); //Write CPU!
		fwrite(&GPU,1,sizeof(GPU),f); //Write GPU!
		fwrite(ActiveVGA,1,sizeof(*ActiveVGA),f); //Write VGA!
		fwrite(ActiveVGA->VRAM,1,ActiveVGA->VRAM_size,f); //VRAM!
		fwrite(ActiveVGA->registers,1,sizeof(*ActiveVGA->registers),f); //Registers!
		fwrite(MMU.memory,1,SaveStatus_Header.MMU_size,f); //Write Memory contents!
		fclose(f); //Done!
	}
}

int EMU_LoadStatus(char *filename) //Load the status from file or memory (TRUE for success, FALSE for error)
{
	return FALSE; //Cannot load: not compatible yet!
	if (strcmp(filename,"")!=0) //Filename set (load from file)?
	{
		FILE *f;
		char header[2] = {'S','S'}; //Header!
		int i;
		f = fopen(filename,"rb"); //Open file for reading data!
		fread(header,1,sizeof(header),f); //Read header!
		if ((header[0]!='S') || (header[1]!='S')) //Invalid header?
		{
			fclose(f);
			return 0; //Invalid header!
		}
		fread(&i,1,sizeof(i),f); //Main version!
		if (i!=SAVESTATE_MAIN_VER)
		{
			fclose(f);
			return 0; //Invalid version!
		}
		fread(&i,1,sizeof(i),f); //Sub version!
		if (i!=SAVESTATE_SUB_VER)
		{
			fclose(f);
			return 0; //Invalid version!
		}
		fread(&SaveStatus_Header,1,sizeof(SaveStatus_Header),f); //Read SaveStatus header!
		if (SaveStatus_Header.checksum!=CRC32((char *)&SaveStatus_Header.data,sizeof(SaveStatus_Header.data))) //Invalid checksum?
		{
			fclose(f);
			return 0; //Invalid checksum!
		}
		if (SaveStatus_Header.MMU_size!=MEMsize()) //Invalid memory size?
		{
			fclose(f);
			return 0; //Invalid memory size to read back!
		}
		fread(&CPU,1,sizeof(CPU),f); //Read CPU!
		fread(&GPU,1,sizeof(GPU),f); //Read GPU!
		if (ActiveVGA) //VGA Loaded?
		{
			doneVGA(&ActiveVGA); //Stop VGA!
		}
		ActiveVGA = VGAalloc(0,1); //Initialise VGA VRAM etc to defaults.
		if (ActiveVGA) //Success?
		{
			fread(ActiveVGA,1,sizeof(*ActiveVGA),f); //Write VGA!
			fread(ActiveVGA->VRAM,1,ActiveVGA->VRAM_size,f); //VRAM!
			fread(ActiveVGA->registers,1,sizeof(*ActiveVGA->registers),f); //Registers!
			fread(MMU.memory,1,SaveStatus_Header.MMU_size,f); //Read back memory!
		}
		fclose(f); //Done using the file: correct!
		
		if (ActiveVGA)
		{
			return 1; //Success!
		}
	}

	return 0; //Invalid MMU size!
}