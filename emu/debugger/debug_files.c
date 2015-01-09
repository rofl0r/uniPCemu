#include "headers/types.h" //Basic types!
#include "headers/cpu/cpu.h" //CPU!
#include "headers/interrupts/interrupt10.h" //INT10 support!
#include "headers/support/log.h" //Log!
#include "headers/debugger/runromverify.h" //runROMVerify support!
#include "headers/emu/emu_misc.h" //file support!
#include "headers/emu/gpu/gpu_emu.h" //EMU debugging text support!
#include "headers/emu/gpu/gpu_text.h" //Text surface support!

/*

debug / *.bin debug routine!

*/

void DoDebugFiles() //Do the debug files!
{
	DIR *dir;
	struct dirent *dirent = 0;
	char file_name[256]; //Original!
	char file_nameres[256]; //Result!
	char finish_name[256]; //Original!
	char finish_nameres[256]; //Result!
	/* open directory */
	if ( ( dir = opendir( "debug" ) ) == 0 )
	{
		GPU_EMU_printscreen(0,GPU_TEXTSURFACE_HEIGHT-1,"Error: verification directory was not found. (debug)");
		sleep(); //Wait forever!
	}
	
	EMU_textcolor(0xF); //Default text color!
	dolog("ROM_log","START OF VERIFICATION PROCEDURE!"); //End!

	while ( ( dirent = readdir( dir ) ) != 0 ) //Files left to check?
	{
		if ( (dirent->d_name[0] == '.') ) continue; //. or ..?
		//Assume files only!
		bzero(file_name,sizeof(file_name));
		bzero(file_nameres,sizeof(file_nameres));
		bzero(finish_name,sizeof(finish_name));
		bzero(finish_nameres,sizeof(finish_nameres));
		//Source files
		sprintf(file_name,"debug/%s",dirent->d_name); //Original file!
		sprintf(file_nameres,"debug/res_%s",dirent->d_name); //Result file!
		//Succeeded files for moving!
		sprintf(finish_name,"debugsucceeded/%s",dirent->d_name); //Original file!
		sprintf(finish_nameres,"debugsucceeded/res_%s",dirent->d_name); //Result file!
		if (file_exists(file_name) && file_exists(file_nameres)) //Not a result?
		{
			GPU_EMU_printscreen(0,GPU_TEXTSURFACE_HEIGHT-1,"Verifying %s...",file_name);
			dolog("ROM_log","Start verifying %s!",file_name); //Start!
			refreshscreen(); //Update the screen now!
			int verified = 0;
			verified = runromverify(file_name,file_nameres); //Verified?
			if (verified)
			{
				GPU_EMU_printscreen(-1,-1,"Verified.");
				dolog("ROM_log","%s has been verified!",file_name);
				//Move the files to the finish directory!
				mkdir("debugsucceeded"); //Make sure the directory exists!
				move_file(file_name,finish_name); //Move file!
				move_file(file_nameres,finish_nameres); //Move file!
			}
			else
			{
				EMU_textcolor(0x4); //Error text color!
				GPU_EMU_printscreen(-1,-1,"Failed.");
				dolog("ROM_log","%s has gone wrong!",file_name);
				sleep();
			}
			refreshscreen(); //Update the screen now!
		} //Not a result file?
	}
	closedir(dir); //Close the directory!

	dolog("ROM_log","END OF VERIFICATION PROCEDURE!"); //End!

	GPU_EMU_printscreen(0,GPU_TEXTSURFACE_HEIGHT-1,"Verification complete!");
	refreshscreen(); //Update the screen now!
	halt(); //Quit if possible!
	sleep(); //Wait forever with the emulator active!
}