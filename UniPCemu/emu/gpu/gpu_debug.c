#include "headers/emu/gpu/gpu.h" //GPU typedefs etc.
#include "headers/interrupts/interrupt10.h" //getscreenwidth() support!

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
	cleardata(&lb[0],sizeof(lb));
	safestrcpy(lb,sizeof(lb),"\r\n"); //Line break!

	char message[256];
	cleardata(&message[0],sizeof(message)); //Init!
	snprintf(message,sizeof(message),"Screen width: %u",getscreenwidth());

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