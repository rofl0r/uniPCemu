#include "headers/types.h" //Basic types!
#include "headers/support/locks.h" //Lock support!
#include "headers/support/log.h" //Log!
#include "headers/emu/debugger/runromverify.h" //runROMVerify support!
#include "headers/emu/emu_misc.h" //file support!
#include "headers/emu/gpu/gpu_emu.h" //EMU debugging text support!
#include "headers/emu/gpu/gpu_text.h" //Text surface support!
#include "headers/emu/directorylist.h" //Directory listing support!
#include "headers/emu/gpu/gpu_renderer.h" //For forcing a refresh of the screen!
#include "headers/emu/emucore.h" //Core support!

extern char ROMpath[256]; //ROM path!

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
	char curdir[256], succeeddir[256];
	cleardata(&curdir[0],sizeof(curdir));
	strcpy(curdir, ROMpath); //Root dir!
	strcat(curdir,"/debug"); //Debug directory!
	cleardata(&succeeddir[0], sizeof(succeeddir));
	strcpy(succeeddir, ROMpath); //Root dir!
	strcat(succeeddir, "/debugsucceeded"); //Succeed directory!
	if (!opendirlist(&dir,curdir,&direntry[0],&isfile))
    {
		GPU_EMU_printscreen(0,GPU_TEXTSURFACE_HEIGHT-1,"Error: verification directory was not found. (debug)");
		sleep(); //Wait forever!
	}
	/* print all the files and directories within directory */

	EMU_textcolor(0xF); //Default text color!
	dolog("ROM_log","START FLAG_OF VERIFICATION PROCEDURE!"); //End!
	
	do
	{
		if (direntry[0] == '.') continue; //. or ..?
		//Assume files only!
		cleardata(&file_name[0],sizeof(file_name));
		cleardata(&file_nameres[0],sizeof(file_nameres));
		cleardata(&finish_name[0],sizeof(finish_name));
		cleardata(&finish_nameres[0],sizeof(finish_nameres));
		//Source files
		sprintf(file_name,"%s/%s",curdir,direntry); //Original file!
		sprintf(file_nameres,"%s/res_%s",curdir,direntry); //Result file!
		//Succeeded files for moving!
		sprintf(finish_name,"%s/%s",succeeddir,direntry); //Original file!
		sprintf(finish_nameres,"%s/res_%s",succeeddir,direntry); //Result file!
		if (file_exists(file_name) && file_exists(file_nameres)) //Not a result?
		{
			GPU_EMU_printscreen(0,GPU_TEXTSURFACE_HEIGHT-1,"Verifying %s...",file_name);
			dolog("ROM_log","Start verifying %s!",file_name); //Start!
			refreshscreen(); //Update the screen now!
			int verified = 0;
			verified = runromverify(file_name,file_nameres); //Verified?
			if (shuttingdown()) goto doshutdown;
			if (verified)
			{
				GPU_EMU_printscreen(-1,-1," Verified.");
				refreshscreen(); //Update the screen now!
				dolog("ROM_log", "%s has been verified!", file_name);
				//Move the files to the finish directory!
				domkdir(succeeddir); //Make sure the directory exists!
				move_file(file_name,finish_name); //Move file!
				move_file(file_nameres,finish_nameres); //Move file!
			}
			else
			{
				EMU_textcolor(0x4); //Error text color!
				GPU_EMU_printscreen(-1,-1," Failed.");
				refreshscreen(); //Update the screen now!
				dolog("ROM_log", "%s has gone wrong!", file_name);
				unlock(LOCK_CPU); //Done with the CPU!
				delay(5000000); //Wait a bit before closing!
				if (shuttingdown()) goto doshutdown;
				lock(LOCK_MAINTHREAD); //Lock the main thread!
				initEMU(0); //Make sure we still exist when terinating us!
				EMU_Shutdown(1); //Request shutdown!
				unlock(LOCK_MAINTHREAD); //We're allowing the main thread to finish!
				lock(LOCK_CPU); //Done with the CPU, so lock us to return!
				return; //Terminate us!
			}
			refreshscreen(); //Update the screen now!
		} //Not a result file?
		if (shuttingdown()) goto doshutdown;
	}
	while (readdirlist(&dir,&direntry[0],&isfile)); //Files left to check?)

	dolog("ROM_log","END FLAG_OF VERIFICATION PROCEDURE!"); //End!

	GPU_EMU_printscreen(0,GPU_TEXTSURFACE_HEIGHT-1,"Verification complete!");
	refreshscreen(); //Update the screen now!
doshutdown:
	closedirlist(&dir); //Close the directory!
	unlock(LOCK_CPU); //Done with the CPU!
	if (!shuttingdown()) delay(5000000); //Wait a bit before closing!
	lock(LOCK_MAINTHREAD); //Lock the main thread!
	initEMU(0); //Make sure we still exist when terinating us!
	EMU_Shutdown(1); //Request shutdown!
	unlock(LOCK_MAINTHREAD); //We're allowing the main thread to finish!
	lock(LOCK_CPU); //Done with the CPU, so lock us to return!
	return; //Terminate us!
}