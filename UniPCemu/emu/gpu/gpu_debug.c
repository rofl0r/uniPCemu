#include "headers/emu/gpu/gpu.h" //GPU typedefs etc.
#include "headers/interrupts/interrupt10.h" //getscreenwidth() support!
#include "headers/fopen64.h" //64-bit fopen support!

extern GPU_type GPU; //GPU!

void dumpscreen()
{
	int emux;
	int emuy; //psp x&y!
//First, calculate the relative destination on the PSP screen!.

	BIGFILE *f;
	f = emufopen64("SCREEN.TXT","w"); //Open file!
	char lb[3];
	cleardata(&lb[0],sizeof(lb));
	safestrcpy(lb,sizeof(lb),"\r\n"); //Line break!

	char message[256];
	cleardata(&message[0],sizeof(message)); //Init!
	snprintf(message,sizeof(message),"Screen width: %u",getscreenwidth());

	emufwrite64(&message,1,safe_strlen(message,sizeof(message)),f); //Write message!
	emufwrite64(&lb,1,safe_strlen(lb,sizeof(lb)),f); //Line break!

	for (emuy=0; emuy<GPU.xres; emuy++) //Process row!
	{
		emufwrite64(&lb,1,safe_strlen(lb,sizeof(lb)),f); //Line break!
		for (emux=0; emux<GPU.xres; emux++) //Process column!
		{
			char c;
			c = (GPU.emu_screenbuffer[(emuy*GPU.xres)+emux]!=0)?'X':' '; //Data!
			emufwrite64(&c,1,sizeof(c),f); //1 or 0!
		}
	}
	emufclose64(f); //Done!
}