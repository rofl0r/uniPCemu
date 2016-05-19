#include "headers/emu/gpu/gpu.h" //GPU typedefs etc.
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/interrupts/interrupt10.h" //Interrupt 10h support!
#include "headers/header_dosbox.h" //BDA support!

extern GPU_type GPU; //GPU!

void dumpscreen()
{
	int emux;
	int emuy; //psp x&y!
//First, calculate the relative destination on the PSP screen!.

	int firstrow = 1;
	FILE *f;
	f = fopen("SCREEN.TXT","w"); //Open file!
	char lb[3];
	bzero(lb,sizeof(lb));
	strcpy(lb,"\r\n"); //Line break!

	char message[256];
	bzero(message,sizeof(message)); //Init!
	sprintf(message,"Screen width: %u",getscreenwidth());

	fwrite(&message,1,safe_strlen(message,sizeof(message)),f); //Write message!
	fwrite(&lb,1,safe_strlen(lb,sizeof(lb)),f); //Line break!
	firstrow = 0; //Not first anymore!

	for (emuy=0; emuy<GPU.xres; emuy++) //Process row!
	{
		if (!firstrow)
		{
			fwrite(&lb,1,safe_strlen(lb,sizeof(lb)),f); //Line break!
		}
		else
		{
			firstrow = 0; //Reset!
		}
		for (emux=0; emux<GPU.xres; emux++) //Process column!
		{
			char c;
			c = (GPU.emu_screenbuffer[(emuy*GPU.xres)+emux]!=0)?'X':' '; //Data!
			fwrite(&c,1,sizeof(c),f); //1 or 0!
		}
	}
	fclose(f); //Done!
}