#include "headers/types.h" //Basic types!
#include "headers/cpu/cpu.h" //CPU!
#include "headers/interrupts/interrupt10.h" //INT10 support!
#include "headers/support/log.h" //Log!
#include "headers/debugger/runromverify.h" //runROMVerify support!
#include "headers/emu/emu_misc.h" //file support!
#include "headers/emu/gpu/gpu_emu.h" //EMU debugging text support!
#include "headers/emu/gpu/gpu_text.h" //Text surface support!

#include "headers/emu/directorylist.h" //Directory listing support!
#include "headers/emu/gpu/gpu_renderer.h" //For forcing a refresh of the screen!

/*

debug / *.bin debug routine!

*/

void DoDebugFiles() //Do the debug files!
{
	char file_name[256]; //Original!
	char file_nameres[256]; //Result!
	char finish_name[256]; //Original!
	char finish_nameres[256]; //Result!
	/* open directory */
	char direntry[256];
	byte isfile;
	DirListContainer_t dir;
    if (!opendirlist(&dir,"debug",&direntry[0],&isfile))
    {
		GPU_EMU_printscreen(0,GPU_TEXTSURFACE_HEIGHT-1,"Error: verification directory was not found. (debug)");
		sleep(); //Wait forever!
	}
	  /* print all the files and directories within directory */
	
	EMU_textcolor(0xF); //Default text color!
	dolog("ROM_log","START FLAG_OF VERIFICATION PROCEDURE!"); //End!

	
	do
	{
		if ( (direntry[0] == '.') ) continue; //. or ..?
		//Assume files only!
		bzero(file_name,sizeof(file_name));
		bzero(file_nameres,sizeof(file_nameres));
		bzero(finish_name,sizeof(finish_name));
		bzero(finish_nameres,sizeof(finish_nameres));
		//Source files
		sprintf(file_name,"debug/%s",direntry); //Original file!
		sprintf(file_nameres,"debug/res_%s",direntry); //Result file!
		//Succeeded files for moving!
		sprintf(finish_name,"debugsucceeded/%s",direntry); //Original file!
		sprintf(finish_nameres,"debugsucceeded/res_%s",direntry); //Result file!
		if (file_exists(file_name) && file_exists(file_nameres)) //Not a result?
		{
			GPU_EMU_printscreen(0,GPU_TEXTSURFACE_HEIGHT-1,"Verifying %s...",file_name);
			dolog("ROM_log","Start verifying %s!",file_name); //Start!
			refreshscreen(); //Update the screen now!
			int verified = 0, dummy=0;
			verified = runromverify(file_name,file_nameres); //Verified?
			if (verified)
			{
				GPU_EMU_printscreen(-1,-1," Verified.");
				refreshscreen(); //Update the screen now!
				dolog("ROM_log", "%s has been verified!", file_name);
				//Move the files to the finish directory!
				dummy = mkdir("debugsucceeded"); //Make sure the directory exists!
				move_file(file_name,finish_name); //Move file!
				move_file(file_nameres,finish_nameres); //Move file!
			}
			else
			{
				EMU_textcolor(0x4); //Error text color!
				GPU_EMU_printscreen(-1,-1," Failed.");
				refreshscreen(); //Update the screen now!
				dolog("ROM_log", "%s has gone wrong!", file_name);
				sleep();
			}
			refreshscreen(); //Update the screen now!
		} //Not a result file?
	}
	while (readdirlist(&dir,&direntry[0],&isfile)); //Files left to check?)
	closedirlist(&dir); //Close the directory!

	dolog("ROM_log","END FLAG_OF VERIFICATION PROCEDURE!"); //End!

	GPU_EMU_printscreen(0,GPU_TEXTSURFACE_HEIGHT-1,"Verification complete!");
	refreshscreen(); //Update the screen now!
	halt(); //Quit if possible!
	sleep(); //Wait forever with the emulator active!
}