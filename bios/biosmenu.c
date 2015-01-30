#include "headers/types.h" //Types and linkage!
#include "headers/bios/bios.h" //Basic BIOS compatibility types etc including myself!
#include "headers/cpu/cpu.h" //CPU constants!
#include "headers/emu/gpu/gpu.h" //GPU compatibility!
#include "headers/bios/staticimage.h" //Static image compatibility!
#include "headers/bios/dynamicimage.h" //Dynamic image compatibility!
#include "headers/bios/io.h" //Basic I/O comp!
#include "headers/emu/input.h" //Basic key input!
#include "headers/mmu/bda.h" //Bios Data Area!
#include "headers/hardware/vga.h" //VGA!
#include "headers/cpu/interrupts.h" //Interrupt support (int10 mostly)
#include "headers/support/zalloc.h" //Memory allocation!
#include "headers/bios/biosmenu.h" //Our defines!
#include "headers/emu/emucore.h" //For init/doneEMU for memory reallocation.
#include "headers/fopen64.h" //64-bit fopen support!

#include "headers/emu/gpu/gpu_emu.h" //GPU emulator support!
#include "headers/emu/gpu/gpu_text.h" //For the framerate surface clearing!
#include "headers/interrupts/interrupt10.h" //GPU emulator support!

#include "headers/emu/directorylist.h" //Directory listing support!

#include "headers/support/log.h" //Logging for disk images!

#define __HW_DISABLED 0

//Force the BIOS to open?
#define FORCE_BIOS 0

//BIOS width in text mode!
#define BIOS_WIDTH GPU_TEXTSURFACE_WIDTH

extern BIOS_Settings_TYPE BIOS_Settings; //Currently loaded settings!
extern byte showchecksumerrors; //Show checksum errors?
extern GPU_TEXTSURFACE *frameratesurface;

BIOSMENU_FONT BIOSMenu_Fonts[3] = {
	{"Default",0x9F,0x17,0x71,0x70,0x70,0x71,0x1F,0x71,0x1F,0x17,0x71,0x1F,0x3F,0x30,0x77,0} //Default (our original font)
	,{"Phoenix Laptop",0x30,0x17,0x71,0x71,0x70,0x71,0x7F,0x70,0x07,0x17,0x17,0x1F,0x3F,0x30,0x77,0} //Phoenix Laptop BIOS font: src: Original Acer Aspire 7741ZG BIOS Setup menu.
	,{"Phoenix - Award Workstation",0x1F,0x11,0x11,0x19,0x1F,0x1E,0x4F,0x1E,0x4F,0x1F,0x1E,0x4F,0x1F,0x1F,0x11,0} //Src: http://www.computerhope.com/help/phoenixa.htm
};

/* BIOS PRESETS */

//Menu Font selection!
#define ActiveBIOSPreset BIOSMenu_Fonts[BIOS_Settings.BIOSmenu_font%NUMITEMS(BIOSMenu_Fonts)]

//Header background (header for section etc)
#define BIOSHEADER_ATTR ActiveBIOSPreset.Header

//Top font/background (BIOS HEADER TEXT)!
#define BIOSTOP_ATTR ActiveBIOSPreset.Menubar_active

//Now stuff for plain items!

//Gray background for entire BIOS (full screen) (|=0x70)
//Black font (plain text on gray)
#define BIOS_ATTR_TEXT ActiveBIOSPreset.Text
//Back color for selected item (white on dark blue)
#define BIOS_ATTR_ACTIVE ActiveBIOSPreset.Option_active
//Inactive item (dark blue on gray)
#define BIOS_ATTR_INACTIVE ActiveBIOSPreset.Option_inactive
//Background (Gray on Gray)
#define BIOS_ATTR_BACKGROUND ActiveBIOSPreset.Background

#define BIOS_ATTR_BLINKENABLE (ActiveBIOSPreset.HighestBitBlink>0)

/* END FLAG_OF BIOS PRESETS */




//How long to press for BIOS!
#define BIOS_TIME 10000000
#define INPUT_INTERVAL 100000

//First, global handler!
Handler BIOS_Menus[] =
{
	BIOS_MainMenu //The main menu is #0!
	//BIOS_TestMenu //The testing of menu's? ;is #0!
	,BIOS_DisksMenu //The Disks menu is #1!
	,BIOS_floppy0_selection //FLOPPY0 selection for the disks menu is #2!
	,BIOS_floppy1_selection //FLOPPY1 selection for the disks menu is #3!
	,BIOS_hdd0_selection //HDD0 selection for the disks menu is #4!
	,BIOS_hdd1_selection //HDD1 selection for the disks menu is #5!
	,BIOS_cdrom0_selection //CDROM0 selection for the disks menu is #6!
	,BIOS_cdrom1_selection //CDROM1 selection for the disks menu is #7!
	,BIOS_AdvancedMenu //Advanced menu is #8
	,BIOS_BootOrderOption //Boot Order option for the Advanced menu is #9!
	,BIOS_InstalledCPUOption //Installed CPU option for the Advanced menu is #10!
	,BIOS_GenerateStaticHDD //Generate Static HDD Disk Image is #11!
	,BIOS_GenerateDynamicHDD //Generate Dynamic HDD Disk Image is #12!
	,BIOS_DebugMode //Debug mode select is #13!
	,BIOS_MemReAlloc //Memory reallocation is #14!
	,BIOS_DirectPlotSetting //Direct Plot setting is #15!
	,BIOS_FontSetting //Font setting is #16!
	,BIOS_KeepAspectRatio //Keep aspect ratio setting is #17!
	,BIOSMenu_LoadDefaults //Load defaults setting is #18!
	,BIOS_ConvertStaticDynamicHDD //Convert static to dynamic HDD is #19!
	,BIOS_ConvertDynamicStaticHDD //Convert dynamic to static HDD is #20!
	,BIOS_DefragmentDynamicHDD //Defragment a dynamic HDD is #21!
};

//Not implemented?
#define NOTIMPLEMENTED NUMITEMS(BIOS_Menus)+1

sword BIOS_Menu = 0; //What menu are we opening (-1 for closing!)?
byte BIOS_SaveStat = 0; //To save the BIOS?
byte BIOS_Changed = 0; //BIOS Changed?

extern CPU_type CPU; //Active CPU!
//CPU_type CPU_Backup; //Backup of the CPU!

GPU_TEXTSURFACE *BIOS_Surface; //Our very own BIOS Surface!

void allocBIOSMenu() //Stuff that take extra video memory etc. for seperated BIOS allocation (so before MMU, because it may take it all)!
{
	if (__HW_DISABLED) return; //Abort!
	BIOS_Surface = alloc_GPUtext(); //Allocate a BIOS Surface!
	if (!BIOS_Surface)
	{
		raiseError("BIOS","Ran out of memory allocating BIOS Screen Layer!");
		return; //Just in case!
	}
	GPU_addTextSurface(BIOS_Surface,NULL); //Register our text surface!
}

byte EMU_Quit = 0; //Quitting emulator?
void freeBIOSMenu() //Free up all BIOS related memory!
{
	if (__HW_DISABLED) return; //Abort!
	GPU_removeTextSurface(BIOS_Surface); //Unregister!
	free_GPUtext(&BIOS_Surface); //Try to deallocate the BIOS Menu surface!
}

extern byte showchecksumerrors; //Show checksum errors?

int CheckBIOSMenu(uint_32 timeout) //To run the BIOS Menus! Result: to reboot?
{
	if (__HW_DISABLED) return 0; //Abort!

	int counter; //Wait X seconds for the BIOS!
	if (timeout) //Time specified before boot?
	{
		counter = timeout; //Wait this long!
	}
	else
	{
		counter = BIOS_TIME; //Default!
	}

	if (timeout) //Specified? We're before boot!
	{
		EMU_textcolor(0xE); //Yellow on black!
		GPU_EMU_printscreen(0,0,"Press SELECT to bring out the BIOS");
	}
	else //Normal BIOS POST!
	{
		printmsg(0xE,"Press SELECT to run BIOS SETUP");
	}
	
	showchecksumerrors = 0; //Don't show!
	BIOS_LoadData(); //Now load/reset the BIOS
	showchecksumerrors = 1; //Reset!
	
	while (counter>0) //Time left?
	{
		counter -= INPUT_INTERVAL; //One further!
		delay(INPUT_INTERVAL); //Intervals of one!
		if ((psp_inputkey() & BUTTON_SELECT) || BIOS_Settings.firstrun || FORCE_BIOS) //R trigger pressed or first run?
		{
			if (timeout) //Before boot?
			{
				GPU_EMU_printscreen(0,0,"                                  "); //Clear our text!
			}
			runBIOS(); //Run the BIOS!
			return 1; //We've to reset!
		}
	}
	if (timeout)
	{
		GPU_EMU_printscreen(0,0,"                                  "); //Clear our text!
	}
	return 0; //No reset!
}









byte EMU_RUNNING = 0; //Emulator is running (are we using the IN-EMULATOR limited menus?) 0=Not running, 1=Running with CPU, 2=Running no CPU (BIOS Menu running?)

void BIOS_clearscreen()
{
    //Clear the framerate surface!
    EMU_clearscreen(); //Clear the screen we're working on!
}

extern GPU_type GPU; //The GPU!

void runBIOS() //Run the BIOS menu (whether in emulation or boot is by EMU_RUNNING)!
{
	if (__HW_DISABLED) return; //Abort!
	EMU_stopInput(); //Stop all emu input!
	terminateVGA(); //Terminate currently running VGA for a speed up!
	//dolog("BIOS","Running BIOS...");
	showchecksumerrors = 0; //Not showing any checksum errors!

//Now reset/save all we need to run the BIOS!
	byte frameratebackup = GPU.show_framerate; //Backup!

	GPU.show_framerate = 0; //Hide the framerate surface!	
	
//Now do the BIOS stuff!
	if (!EMU_RUNNING) //Not in emulator?
	{
		EMU_textcolor(0xF);
		printmsg(0xF,"\r\nLoading BIOS...");
		delay(500000); //0.5 sec!
	}
	
	GPU_textclearscreen(frameratesurface); //Make sure the surface is empty for a neat BIOS!
	
	BIOS_LoadData(); //Now load/reset the BIOS
	BIOS_Changed = 0; //Default: the BIOS hasn't been changed!
	BIOS_SaveStat = 0; //Default: not saving!
	showchecksumerrors = 0; //Default: not showing checksum errors!
	BIOS_clearscreen(); //Clear the screen!
	BIOS_Menu = 0; //We're opening the main menu!

	BIOS_MenuChooser(); //Show the BIOS's menu we've selected!
	
	if (BIOS_Settings.firstrun) //First run?
	{
		BIOS_Settings.firstrun = 0; //Not the first run anymore!
		BIOS_Changed = 1; //We've been changed!
	}
	
	if (BIOS_SaveStat && BIOS_Changed) //To save the BIOS and BIOS has been changed?
	{
		if (!BIOS_SaveData()) //Save our options and failed?
		{
			BIOS_clearscreen(); //Clear the screen!
			EMU_gotoxy(0,0); //First column,row!
			EMU_textcolor(0xF);
			GPU_EMU_printscreen(-1,-1,"Error: couldn't save the BIOS!");
			delay(5000000); //Wait 5 sec before rebooting!
		}
		else
		{
			BIOS_clearscreen(); //Clear the screen!

			if (!EMU_RUNNING) //Emulator isn't running?
			{
				EMU_gotoxy(0,0); //First column,row!
				EMU_textcolor(0xF);
				GPU_EMU_printscreen(-1,-1,"BIOS Saved!");
				delay(2000000); //Wait 2 sec before rebooting!
			}
			else //Emulator running?
			{
				EMU_gotoxy(0,0); //First column,row!
				EMU_textcolor(0xF);
				GPU_EMU_printscreen(-1,-1,"BIOS Saved (Returning to the emulator)!"); //Info!
				delay(2000000); //Wait 2 sec!
			}
		}

	}
	else //Discard changes?
	{
		EMU_gotoxy(0,0);
		EMU_textcolor(0xF);
		GPU_EMU_printscreen(-1,-1,"BIOS Discarded!"); //Info!
		BIOS_LoadData(); //Reload!
		delay(2000000); //Wait 2 sec!
	}

	BIOSDoneScreen(); //Clean up the screen!
//Now return to the emulator to reboot!
	BIOS_ValidateDisks(); //Validate&reload all disks!
	GPU_keepAspectRatio(BIOS_Settings.keepaspectratio); //Keep the aspect ratio?

//Restore all states saved for the BIOS!
	GPU.show_framerate = frameratebackup; //Restore!
	startVGA(); //Start the VGA up again!
	EMU_startInput(); //Start all emu input again!
}






















/*


Now comes our menus:


*/


/*

First all global stuff:

*/





//Calculates the middle of the screen!
#define CALCMIDDLE(rowwidth,textwidth) (rowwidth/2)-(textwidth/2)

void printcenter(char *text, int row) //Prints text centered on a row!
{
	if (text) GPU_EMU_printscreen(CALCMIDDLE(BIOS_WIDTH,safe_strlen(text,256)),row,text); //Show centered text!
}

void printscreencenter(char *text)
{
	if (text) printcenter(text,25/2); //Print text to the middle of the screen!
}

void clearrow(int row)
{
	int i=0; //Index for on-screen characters!
	for (;i<BIOS_WIDTH;) //Clear only one row!
	{
		GPU_EMU_printscreen(i++,row," "); //Clear BIOS header!
	}
}


void BIOSClearScreen() //Resets the BIOS's screen!
{
	if (__HW_DISABLED) return; //Abort!
	GPU_textclearscreen(frameratesurface); //Make sure the surface is empty for a neat BIOS!
	char BIOSText[] = "x86 BIOS"; //The BIOS's text!
	//cursorXY(0,0,0); //Goto top of the screen!

	EMU_textcolor(BIOS_ATTR_TEXT); //Plain text!
	/*CPU.registers->AX = VIDEOMODE_EMU; //Video mode 80x25 16 colors!
	BIOS_int10(); //Switch video mode+Clearscreen!
	*/
	
	EMU_textcolor(BIOSTOP_ATTR); //TOP FONT
	clearrow(0); //Clear first row!

	//Clear the rest of the screen!
	EMU_textcolor(BIOS_ATTR_BACKGROUND); //Attr backcolor
	int i=1; //From row 1-25 clearing!
	for (;i<25;) //Process all rows!
	{
		clearrow(i++); //Clear a row!
	}
	
	//Now the screen is set to go!
	EMU_textcolor(BIOSTOP_ATTR); //Switch to BIOS Header attribute!
	printcenter(BIOSText,0); //Show the BIOS's text!
	GPU_EMU_printscreen(BIOS_WIDTH-safe_strlen("MEM:12MB",256),0,"MEM:%02iMB",(BIOS_Settings.memory/1024768)); //Show ammount of memory to be able to use!
	EMU_textcolor(BIOS_ATTR_TEXT); //Std: display text!
}












void BIOSDoneScreen() //Cleans up the BIOS's screen!
{
	if (__HW_DISABLED) return; //Abort!
	EMU_textcolor(0x0F); //White on black!
	BIOS_clearscreen();
	EMU_gotoxy(0,0); //First row, first column!
}





/*

First the menu chooser:

*/

void BIOS_InvMenu() //Invalid menu!
{
	char InvMenuText[] = "Invalid menu or this menu is not implemented yet!";
	EMU_textcolor(BIOS_ATTR_TEXT); //Text fontcolor!
	printscreencenter(InvMenuText); //Show the invalid menu text!
	delay(5000000); //Wait 5 seconds before continuing to default menu!
	BIOS_Menu = 0; //Goto main menu!
}

void BIOS_MenuChooser() //The menu chooser!
{
	while (BIOS_Menu!=-1) //Still in the BIOS to open a menu?
	{
		BIOSClearScreen(); //Init the BIOS Background!
		if (BIOS_Menu>=0 && BIOS_Menu<NUMITEMS(BIOS_Menus)) //Within range of menus?
		{
			BIOS_Menus[BIOS_Menu](); //Call the menu!
		}
		else
		{
			BIOS_InvMenu(); //Invalid menu!
		}
	}
}





//Now the menus itself:







void BIOS_Title(char *text)
{
	EMU_textcolor(BIOSHEADER_ATTR); //Header fontcolor!
	printcenter(text,2); //Show title text!
}

char menuoptions[256][256]; //Going to contain the menu's for BIOS_ShowMenu!

//allowspecs: allow special keys to break?

//Flags for allowspecs:
//No specs!
#define BIOSMENU_SPEC_NONE 0
//Allow return using CIRCLE
#define BIOSMENU_SPEC_RETURN 1
//Allow L/R/LEFT/RIGHT button for more menus
#define BIOSMENU_SPEC_LR 2
//Allow SQUARE for special adjusted ENTER.
#define BIOSMENU_SPEC_SQUAREOPTION 3


//Results other than valid menu items:
#define BIOSMENU_SPEC_CANCEL -1
//LTRIGGER/LEFT
#define BIOSMENU_SPEC_LTRIGGER -2
//RTRIGGER/RIGHT
#define BIOSMENU_SPEC_RTRIGGER -3

//Stats:
//Plain normal option select or SPEC (see above)!
#define BIOSMENU_STAT_OK 0
//SQUARE option pressed, item selected returned.
#define BIOSMENU_STAT_SQUARE 1

int BIOS_ShowMenu(int numitems, int startrow, int allowspecs, word *stat)
{
	*stat = BIOSMENU_STAT_OK; //Plain status for default!
	int key = 0; //Currently pressed key(s)
	int option = 0; //What option to choose?
	int oldoption = -1; //Old option!
	while (key!=BUTTON_CROSS) //Wait for the key to choose something!
	{
		key = psp_inputkeydelay(BIOS_INPUTDELAY); //Input a key with delay!
		if ((key & BUTTON_UP)>0) //Up pressed?
		{
			if (option>0) //Past first?
			{
				option--; //Previous option!
			}
			else //Top option?
			{
				if (numitems>1) //More than one item?
				{
					option = numitems-1; //Goto bottom item!
				}
			}
		}
		else if ((key & BUTTON_DOWN)>0) //Down pressed?
		{
			if (option<(numitems-1)) //Not last item?
			{
				option++; //Next option!
			}
			else //Last item?
			{
				option = 0; //Goto first item from bottom!
			}
		}
		else if (((key & BUTTON_CIRCLE)>0) && ((allowspecs&BIOSMENU_SPEC_RETURN)>0)) //Cancel pressed and allowed?
		{
			option = BIOSMENU_SPEC_CANCEL; //Cancelled!
			break; //Exit loop!
		}
		else if (
		    ( //Keys pressed?
		        (((key & BUTTON_LTRIGGER)>0) || ((key & BUTTON_RTRIGGER)>0)) ||
		        (((key & BUTTON_LEFT)>0) || ((key & BUTTON_RIGHT)>0))
		    )
		    && ((allowspecs&BIOSMENU_SPEC_LR)>0)) //L/R/LEFT/RIGHT pressed and allowed?
		{
			if (((key & BUTTON_LTRIGGER)>0) || ((key & BUTTON_LEFT)>0)) //LTRIGGER/LEFT?
			{
				option = BIOSMENU_SPEC_LTRIGGER; //LTRIGGER!
				break; //Exit loop!
			}
			else if (((key & BUTTON_RTRIGGER)>0) || ((key & BUTTON_RIGHT)>0)) //RTRIGGER/RIGHT?
			{
				option = BIOSMENU_SPEC_RTRIGGER; //RTRIGGER!
				break; //Exit loop!
			}
		}
		else if (((key&BUTTON_SQUARE)>0) && ((allowspecs&BIOSMENU_SPEC_SQUAREOPTION)>0)) //SQUARE and allowed?
		{
			*stat = BIOSMENU_STAT_SQUARE; //Square special option!
			break; //Exit loop!
		}

//Now that the options have been chosen, show them:

		if (oldoption!=option) //Option changed?
		{
			int cur = 0; //Current option
			for (cur=0; cur<numitems; cur++) //Process all options!
			{
				EMU_gotoxy(0,startrow+cur); //Goto start of item row!
				if (cur==option) //Option?
				{
					EMU_textcolor(BIOS_ATTR_ACTIVE); //Active item!
					GPU_EMU_printscreen(0,startrow+cur,"> %s",menuoptions[cur]);
				}
				else //Not option?
				{
					EMU_textcolor(BIOS_ATTR_INACTIVE); //Inactive item!
					GPU_EMU_printscreen(0,startrow+cur,"  %s",menuoptions[cur]); //Plain option!
				}
			}
			oldoption = option; //Save our changes!
		}
		delay(1); //Wait for other threads!
	}
	return option; //Give the chosen option!
}

//File list functions!

//Ammount of files in the list MAX
#define ITEMLIST_MAXITEMS 0x100
char itemlist[ITEMLIST_MAXITEMS][256]; //Max X files listed!
int numlist = 0; //Number of files!

void clearList()
{
	memset(itemlist,0,sizeof(itemlist)); //Init!
	numlist = 0; //Nothin in there yet!
}

void addList(char *text)
{
	if (numlist<ITEMLIST_MAXITEMS) //Maximum not reached yet?
	{
		strcpy(itemlist[numlist++],text); //Add the item and increase!
	}
}

//Generate file list based on extension!
void generateFileList(char *extensions, int allowms0, int allowdynamic)
{
	numlist = 0; //Reset ammount of files!
	clearList(); //Clear the list!
	if (allowms0) //Allow Memory Stick option?
	{
        #ifdef __psp__
               addList("ms0:\0"); //Add filename (Memory Stick)!
        #endif
	}
	char direntry[256];
	byte isfile;
	DirListContainer_t dir;
	if (opendirlist(&dir,".",&direntry[0],&isfile))
	{
		/* print all the files and directories within directory */
		do //Files left to check?
		{
			if (isfile) //It's a file?
			{
				if (isext(direntry, extensions)) //Check extension!
				{
					int allowed = 0;
					allowed = ((allowdynamic && is_dynamicimage(direntry)) || (!is_dynamicimage(direntry))); //Allowed when not dynamic or dynamic is allowed!
					if (allowed) //Allowed?
					{
						addList(direntry); //Set filename!
					}
				}
			}
		}
		while (readdirlist(&dir,&direntry[0],&isfile)); //Files left to check?)
		closedirlist(&dir);
	}
}

//Cancelled!
#define FILELIST_CANCEL -1
//No files with this extension!
#define FILELIST_NOFILES -2
//Default item!
#define FILELIST_DEFAULT -3

int cmpinsensitive(char *str1, char *str2, uint_32 maxlen) //Compare, case insensitive!
{
	if (str1==NULL || str2==NULL) return 0; //Error: give not equal!
	if (strlen(str1)!=strlen(str2)) //Not equal in length?
	{
		return 0; //Not equal in length!
	}
	int length = strlen(str1); //Length!
	int counter = 0;
	while (toupper((int)*str1)==toupper((int)*str2) && *str1!='\0' && counter<length) //Equal and no overflow?
	{
		str1++; //Next character!
		str2++; //Next character!
		counter++; //For overflow check!
	}
	if (counter==length && (toupper((int)*str1)==toupper((int)*str2))) //Whole string checked and equal?
	{
		return 1; //Equal!
	}
	return 0; //Not equal!
}

void printCurrent(int x, int y, char *text, int maxlen) //Create the current item with maximum length!
{
	char buffer[1024]; //The buffered text!
	char filler[1024]; //The filler data!
	memset(buffer,'\0',sizeof(buffer)); //Init buffer to unused!
	memset(filler,'\0',sizeof(filler)); //Init filler to unused!
	int i,j;
	int max = safe_strlen(text,256); //Default: maximum the size of the destination!
	if (max>maxlen) //More than maximum length?
	{
		max = maxlen; //Truncate to maximum length!
	}
	if (max>(sizeof(buffer)-1)) //Too much?
	{
		max = sizeof(buffer)-1; //Limit to buffer!
	}
	//First: compose text with limit!
	for (i=0;i<max;) //Process the entire length, up to maximum processable length!
	{
		buffer[i] = text[i]; //Set as text!
		++i; //Next item!
	}
	//Next: compose filler!
	j = 0; //Init second filler!
	int max2 = maxlen; //Maximum length!
	if (max2>(sizeof(filler)-1)) //Limit breached?
	{
		max2 = sizeof(filler)-1; //Limit!
	}
	for (;i<max2;i++) //Anything left?
	{
		filler[j++] = ' '; //Fill filler!
	}
	
	//Finally, print the output!
	EMU_textcolor(BIOS_ATTR_ACTIVE); //Active item!
	GPU_EMU_printscreen(x,y,"%s",buffer); //Show item with maximum length or less!

	EMU_textcolor(BIOS_ATTR_BACKGROUND); //Background of the current item!
	GPU_EMU_printscreen(-1,-1,"%s",filler); //Show rest with filler color, update!
}

//x,y = coordinates of file list
//maxlen = ammount of characters for the list (width of the list)

int ExecuteList(int x, int y, char *defaultentry, int maxlen) //Runs the file list!
{
//First, no file check!
	if (!numlist) //No files?
	{
		EMU_gotoxy(x,y); //Goto position of output!
		EMU_textcolor(BIOS_ATTR_TEXT); //Plain text!
		GPU_EMU_printscreen(x,y,"No files found!");
		return FILELIST_NOFILES; //Error: no files found!
	}

//Now, find the default!
	int result = 0; //Result!
	for (result=0; result<numlist; result++) //Search for our file!
	{
		if (cmpinsensitive(itemlist[result],defaultentry,sizeof(itemlist[result]))) //Default found?
		{
			break; //Use this default!
		}
	}
	if (!cmpinsensitive(itemlist[result],defaultentry,sizeof(itemlist[result])) || strcmp(defaultentry,"")==0) //Default not found or no default?
	{
		result = 0; //Goto first file: we don't have the default!
	}

	printCurrent(x,y,itemlist[result],maxlen); //Create our current entry!
	
	while (1) //Doing selection?
	{
		int key = 0;
		key = psp_inputkeydelay(BIOS_INPUTDELAY); //Input key!

		if ((key&BUTTON_UP)>0) //UP?
		{
			if (result>0) //Not first?
			{
				--result; //Up one item!
			}
			else //First?
			{
				result = numlist-1; //Bottom of the list!
			}
			result &= 0xFF; //Only 255 entries max!
			printCurrent(x,y,itemlist[result],maxlen); //Create our current entry!
		}
		else if ((key&BUTTON_DOWN)>0) //DOWN?
		{
			if (result<(numlist-1)) //Not at the bottom?
			{
				++result; //Down one item!
			}
			else //At the bottom?
			{
				result = 0; //Top of the list!
			}
			result &= 0xFF; //Only 255 entries max!
			printCurrent(x, y, itemlist[result], maxlen); //Create our current entry!
		}
		else if ((key&BUTTON_CROSS)>0) //SELECT?
		{
			return result; //Give the result!
		}
		else if ((key&BUTTON_CIRCLE)>0) //CANCEL?
		{
			return FILELIST_CANCEL; //Cancelled!
		}
		else if ((key&BUTTON_TRIANGLE)>0) //DEFAULT?
		{
			return FILELIST_DEFAULT; //Unmount!
		}
	}
}

//Menus itself:

//Selection menus for disk drives!

void BIOS_floppy0_selection() //FLOPPY0 selection menu!
{
	BIOS_Title("Mount FLOPPY A");
	generateFileList("img",0,0); //Generate file list for all .img files!
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Disk image: "); //Show selection init!
	int file = ExecuteList(12,4,BIOS_Settings.floppy0,256); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.floppy0,""); //Unmount!
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_NOFILES: //No files?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.floppy0,""); //Unmount: no files!
		break;
	default: //File?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.floppy0,itemlist[file]); //Use this file!
	}
	BIOS_Menu = 1; //Return to image menu!
}

void BIOS_floppy1_selection() //FLOPPY1 selection menu!
{
	BIOS_Title("Mount FLOPPY B");
	generateFileList("img",0,0); //Generate file list for all .img files!
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Disk image: "); //Show selection init!
	int file = ExecuteList(12,4,BIOS_Settings.floppy1,256); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.floppy1,""); //Unmount!
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_NOFILES: //No files?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.floppy1,""); //Unmount: no files!
		break;
	default: //File?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.floppy1,itemlist[file]); //Use this file!
	}
	BIOS_Menu = 1; //Return to image menu!
}

void BIOS_hdd0_selection() //HDD0 selection menu!
{
	BIOS_Title("Mount First HDD");
	generateFileList("img|sfdimg",1,1); //Generate file list for all .img files!
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Disk image: "); //Show selection init!
	int file = ExecuteList(12,4,BIOS_Settings.hdd0,256); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.hdd0,""); //Unmount!
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_NOFILES: //No files?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.hdd0,""); //Unmount: no files!
		break;
	default: //File?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.hdd0,itemlist[file]); //Use this file!
	}
	BIOS_Menu = 1; //Return to image menu!
}

void BIOS_hdd1_selection() //HDD1 selection menu!
{
	BIOS_Title("Mount Second HDD");
	generateFileList("img|sfdimg",1,1); //Generate file list for all .img files!
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Disk image: "); //Show selection init!
	int file = ExecuteList(12,4,BIOS_Settings.hdd1,256); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.hdd1,""); //Unmount!
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_NOFILES: //No files?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.hdd1,""); //Unmount: no files!
		break;
	default: //File?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.hdd1,itemlist[file]); //Use this file!
	}
	BIOS_Menu = 1; //Return to image menu!
}

void BIOS_cdrom0_selection() //CDROM0 selection menu!
{
	BIOS_Title("Mount First CD-ROM");
	generateFileList("iso",0,0); //Generate file list for all .img files!
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Disk image: "); //Show selection init!
	int file = ExecuteList(12,4,BIOS_Settings.cdrom0,256); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.cdrom0,""); //Unmount!
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_NOFILES: //No files?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.cdrom0,""); //Unmount: no files!
		break;
	default: //File?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.cdrom0,itemlist[file]); //Use this file!
	}
	BIOS_Menu = 1; //Return to image menu!
}

void BIOS_cdrom1_selection() //CDROM1 selection menu!
{
	BIOS_Title("Mount Second CD-ROM");
	generateFileList("iso",0,0); //Generate file list for all .img files!
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Disk image: "); //Show selection init!
	int file = ExecuteList(12,4,BIOS_Settings.cdrom1,256); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.cdrom1,""); //Unmount!
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_NOFILES: //No files?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.cdrom1,""); //Unmount: no files!
		break;
	default: //File?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.cdrom1,itemlist[file]); //Use this file!
	}
	BIOS_Menu = 1; //Return to image menu!
}

word Menu_Stat; //Menu status!

void BIOS_InitDisksText()
{
	int i;
	for (i=0; i<11; i++)
	{
		bzero(menuoptions[i],sizeof(menuoptions[i])); //Init!
	}
	strcpy(menuoptions[0],"Floppy A: ");
	strcpy(menuoptions[1],"Floppy B: ");
	strcpy(menuoptions[2],"First HDD: ");
	strcpy(menuoptions[3],"Second HDD: ");
	strcpy(menuoptions[4],"First CD-ROM: ");
	strcpy(menuoptions[5],"Second CD-ROM: ");
	strcpy(menuoptions[6],"Generate Static HDD Image");
	strcpy(menuoptions[7],"Generate Dynamic HDD Image");
	strcpy(menuoptions[8], "Convert static to dynamic HDD Image");
	strcpy(menuoptions[9], "Convert dynamic to static HDD Image");
	strcpy(menuoptions[10], "Defragment a dynamic HDD Image");

//FLOPPY0
	if (strcmp(BIOS_Settings.floppy0,"")==0) //No disk?
	{
		strcat(menuoptions[0],"<NO DISK>"); //Add disk image!
	}
	else
	{
		strcat(menuoptions[0],BIOS_Settings.floppy0); //Add disk image!
		if (BIOS_Settings.floppy0_readonly) //Read-only?
		{
			strcat(menuoptions[0]," <R>"); //Show readonly tag!
		}
	}

//FLOPPY1
	if (strcmp(BIOS_Settings.floppy1,"")==0) //No disk?
	{
		strcat(menuoptions[1],"<NO DISK>"); //Add disk image!
	}
	else
	{
		strcat(menuoptions[1],BIOS_Settings.floppy1); //Add disk image!
		if (BIOS_Settings.floppy1_readonly) //Read-only?
		{
			strcat(menuoptions[1]," <R>"); //Show readonly tag!
		}
	}

//HDD0
	if (strcmp(BIOS_Settings.hdd0,"")==0) //No disk?
	{
		strcat(menuoptions[2],"<NO DISK>"); //Add disk image!
	}
	else
	{
		strcat(menuoptions[2],BIOS_Settings.hdd0); //Add disk image!
		if (BIOS_Settings.hdd0_readonly) //Read-only?
		{
			strcat(menuoptions[2]," <R>"); //Show readonly tag!
		}
	}

//HDD1
	if (strcmp(BIOS_Settings.hdd1,"")==0) //No disk?
	{
		strcat(menuoptions[3],"<NO DISK>"); //Add disk image!
	}
	else
	{
		strcat(menuoptions[3],BIOS_Settings.hdd1); //Add disk image!
		if (BIOS_Settings.hdd0_readonly) //Read-only?
		{
			strcat(menuoptions[2]," <R>"); //Show readonly tag!
		}
	}

//CDROM0
	if (strcmp(BIOS_Settings.cdrom0,"")==0) //No disk?
	{
		strcat(menuoptions[4],"<NO DISK>"); //Add disk image!
	}
	else
	{
		strcat(menuoptions[4],BIOS_Settings.cdrom0); //Add disk image!
	}

//CDROM1
	if (strcmp(BIOS_Settings.cdrom1,"")==0) //No disk?
	{
		strcat(menuoptions[5],"<NO DISK>"); //Add disk image!
	}
	else
	{
		strcat(menuoptions[5],BIOS_Settings.cdrom1); //Add disk image!
	}
}


void BIOS_DisksMenu() //Manages the mounted disks!
{
	BIOS_Title("Manage mounted drives");
	BIOS_InitDisksText(); //First, initialise texts!
	int menuresult = BIOS_ShowMenu(11,4,BIOSMENU_SPEC_LR|BIOSMENU_SPEC_SQUAREOPTION,&Menu_Stat); //Show the menu options, allow SQUARE!
	switch (menuresult)
	{
	case BIOSMENU_SPEC_LTRIGGER: //L: Main menu?
		BIOS_Menu = 0; //Goto Main menu!
		break;
	case BIOSMENU_SPEC_RTRIGGER: //R: Advanced menu?
		BIOS_Menu = 8; //Goto Advanced Menu!
		break;

	case 0: //First diskette?
		if (Menu_Stat==BIOSMENU_STAT_OK) //Plain select?
		{
			BIOS_Menu = 2; //FLOPPY0 selection!
		}
		else if ((Menu_Stat==BIOSMENU_STAT_SQUARE) && (strcmp(BIOS_Settings.floppy0,"")!=0)) //SQUARE=Trigger readonly!
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.floppy0_readonly = !BIOS_Settings.floppy0_readonly; //Trigger!
		}
		break;
	case 1: //Second diskette?
		if (Menu_Stat==BIOSMENU_STAT_OK) //Plain select?
		{
			BIOS_Menu = 3; //FLOPPY1 selection!
		}
		else if ((Menu_Stat==BIOSMENU_STAT_SQUARE) && (strcmp(BIOS_Settings.floppy1,"")!=0)) //SQUARE=Trigger readonly!
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.floppy1_readonly = !BIOS_Settings.floppy1_readonly; //Trigger!
		}
		break;
	case 2: //First HDD?
		if ((Menu_Stat==BIOSMENU_STAT_OK) && (!EMU_RUNNING)) //Plain select and not running (hard disks cannot be unmounted/changed during runtime)?
		{
			BIOS_Menu = 4; //HDD0 selection!
		}
		else if ((Menu_Stat==BIOSMENU_STAT_SQUARE) && (strcmp(BIOS_Settings.hdd0,"")!=0)) //SQUARE=Trigger readonly!
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.hdd0_readonly = !BIOS_Settings.hdd0_readonly; //Trigger!
		}
		break;
	case 3: //Second HDD?
		if ((Menu_Stat==BIOSMENU_STAT_OK) && (!EMU_RUNNING)) //Plain select and not running (hard disks cannot be unmounted/changed during runtime)?
		{
			BIOS_Menu = 5; //HDD1 selection!
		}
		else if ((Menu_Stat==BIOSMENU_STAT_SQUARE) && (strcmp(BIOS_Settings.hdd1,"")!=0)) //SQUARE=Trigger readonly!
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.hdd1_readonly = !BIOS_Settings.hdd1_readonly; //Trigger!
		}
		break;
	case 4: //First CDROM?
		if (Menu_Stat==BIOSMENU_STAT_OK) //Plain status?
		{
			BIOS_Menu = 6; //CDROM0 selection!
		}
		break;
	case 5: //Second CDROM?
		if (Menu_Stat==BIOSMENU_STAT_OK) //Plain status?
		{
			BIOS_Menu = 7; //CDROM1 selection!
		}
		break;
	case 6: //Generate Static HDD?
		if (Menu_Stat==BIOSMENU_STAT_OK) //Plain status?
		{
			BIOS_Menu = 11; //Generate Static HDD!
		}
		break;
	case 7: //Generate Dynamic HDD?
		if (Menu_Stat==BIOSMENU_STAT_OK) //Plain status?
		{
			BIOS_Menu = 12; //Generate Dynamic HDD!
		}
		break;
	case 8: //Convert static to dynamic HDD?
		if (Menu_Stat == BIOSMENU_STAT_OK) //Plain status?
		{
			BIOS_Menu = 19; //Convert static to dynamic HDD!
		}
		break;
	case 9: //Convert dynamic to static HDD?
		if (Menu_Stat == BIOSMENU_STAT_OK) //Plain status?
		{
			BIOS_Menu = 20; //Convert dynamic to static HDD!
		}
		break;
	case 10: //Defragment a dynamic HDD Image?
		if (Menu_Stat == BIOSMENU_STAT_OK) //Plain status?
		{
			BIOS_Menu = 21; //Defragment a dynamic HDD Image!
		}
		break;
	default: //Unknown option?
		BIOS_Menu = NOTIMPLEMENTED; //Not implemented yet!
		break;
	}
}

extern char BOOT_ORDER_STRING[15][30]; //Boot order, string values!

int advancedoptions = 0; //Number of advanced options!
byte optioninfo[0x10]; //Option info for what option!

void BIOS_InitAdvancedText()
{
	advancedoptions = 0; //Init!
	int i;
	for (i=0; i<3; i++) //Clear all possibilities!
	{
		bzero(menuoptions[i],sizeof(menuoptions[i])); //Init!
	}
	if (!EMU_RUNNING) //Just plain menu (not an running emu?)?
	{
		optioninfo[advancedoptions] = 0; //Boot Order!
		strcpy(menuoptions[advancedoptions],"Boot Order: "); //Change boot order!
		strcat(menuoptions[advancedoptions++],BOOT_ORDER_STRING[BIOS_Settings.bootorder]); //Add boot order after!
		optioninfo[advancedoptions] = 1; //Installed CPU!
		strcpy(menuoptions[advancedoptions],"Installed CPU: "); //Change installed CPU!
		if (BIOS_Settings.emulated_CPU==CPU_8086) //8086?
		{
			strcat(menuoptions[advancedoptions++],"Intel 8086"); //Add installed CPU!
		}
		else if (BIOS_Settings.emulated_CPU==CPU_80186) //80186?
		{
			strcat(menuoptions[advancedoptions++],"Intel 80186"); //Add installed CPU!
		}
		else //Unknown?
		{
			strcat(menuoptions[advancedoptions++],"<UNIMPLEMENTED>"); //Add installed CPU!
		}
	}

	optioninfo[advancedoptions] = 2; //Debug mode!
	strcpy(menuoptions[advancedoptions],"Debug mode: ");
	switch (BIOS_Settings.debugmode) //What debug mode is active?
	{
	case DEBUGMODE_NONE:
		strcat(menuoptions[advancedoptions++],"No debugger enabled"); //Set filename from options!
		break;
	case DEBUGMODE_RTRIGGER:
		strcat(menuoptions[advancedoptions++],"Enabled, RTrigger=Step"); //Set filename from options!
		break;
	case DEBUGMODE_STEP:
		strcat(menuoptions[advancedoptions++],"Enabled, Step through"); //Set filename from options!
		break;
	case DEBUGMODE_SHOW_RUN:
		strcat(menuoptions[advancedoptions++],"Enabled, just run, ignore shoulder buttons"); //Set filename from options!
		break;
	case DEBUGMODE_TEST:
		strcat(menuoptions[advancedoptions++],"Disabled, run debug directory files, else BIOSROM.DAT"); //Set filename from options!
		break;
	case DEBUGMODE_TEST_STEP:
		strcat(menuoptions[advancedoptions++],"Enabled, Step through, run debug directory files, else BIOSROM.DAT"); //Set filename from options!
		break;
	case DEBUGMODE_TEXT:
		strcat(menuoptions[advancedoptions++],"No debugger enabled, debug text-mode characters"); //Set filename from options!
		break;
	case DEBUGMODE_SOUND:
		strcat(menuoptions[advancedoptions++],"No debugger enabled, run sound test"); //Set filename from options!
		break;
	default:
		strcat(menuoptions[advancedoptions++],"<UNKNOWN. CHECK BIOS VERSION>");
		break;
	}

	optioninfo[advancedoptions] = 4; //We're direct plot setting!
	strcpy(menuoptions[advancedoptions],"VGA Direct Plot: ");

setdirectplottext: //For fixing it!
	switch (BIOS_Settings.VGA_AllowDirectPlot) //What direct plot setting?
	{
	case 2: //Forced?
		strcat(menuoptions[advancedoptions++],"Forced");
		break;
	case 1: //Yes?
		strcat(menuoptions[advancedoptions++],"Automatic");
		break;
	case 0: //No?
		strcat(menuoptions[advancedoptions++],"Disabled");
		break;
	default: //Error: fix it!
		BIOS_Settings.VGA_AllowDirectPlot = 0; //Reset/Fix!
		BIOS_Changed = 1; //We've changed!
		goto setdirectplottext; //Goto!
		break;
	}

	if (!EMU_RUNNING) //Emulator not running (allow memory size change?)
	{
		optioninfo[advancedoptions] = 3; //Memory detect!
		strcpy(menuoptions[advancedoptions++],"Redetect available memory");
	}

	optioninfo[advancedoptions] = 5; //Select BIOS Font!
	strcpy(menuoptions[advancedoptions],"BIOS Font: ");
	strcat(menuoptions[advancedoptions++],ActiveBIOSPreset.name); //BIOS font selected!
	
	optioninfo[advancedoptions] = 6; //Keep aspect ratio!
	strcpy(menuoptions[advancedoptions],"Aspect ratio: ");
	if (BIOS_Settings.keepaspectratio) //Keep aspect ratio?
	{
		strcat(menuoptions[advancedoptions++],"Keep the same");
	}
	else
	{
		strcat(menuoptions[advancedoptions++],"Fullscreen stretching");
	}
}

void BIOS_BootOrderOption() //Manages the boot order
{
	BIOS_Title("Boot Order");
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Boot Order: "); //Show selection init!
	numlist = NUMITEMS(BOOT_ORDER_STRING); //Ammount of files (orders)
	int i = 0; //Counter!
	for (i=0; i<numlist; i++) //Process options!
	{
		bzero(itemlist[i],sizeof(itemlist[i])); //Reset!
		strcpy(itemlist[i],BOOT_ORDER_STRING[i]); //Set filename from options!
	}
	int file = ExecuteList(12,4,BOOT_ORDER_STRING[BIOS_Settings.bootorder],256); //Show options for the boot order!
	switch (file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = 0; //First is default!
	default: //Changed?
		BIOS_Changed = 1; //Changed!
		BIOS_Settings.bootorder = (byte)file; //Use this option (need to typecast)!
	}
	BIOS_Menu = 8; //Return to Advanced menu!
}

void BIOS_InstalledCPUOption() //Manages the installed CPU!
{
	BIOS_Title("Installed CPU");
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Installed CPU: "); //Show selection init!
	int i = 0; //Counter!
	numlist = 2; //Ammount of CPU types!
	for (i=0; i<2; i++) //Process options!
	{
		bzero(itemlist[i],sizeof(itemlist[i])); //Reset!
	}
	strcpy(itemlist[CPU_8086],"Intel 8086"); //Set filename from options!
	strcpy(itemlist[CPU_80186],"Intel 80186"); //Set filename from options!
	int current = 0;
	if (BIOS_Settings.emulated_CPU==CPU_8086) //8086?
	{
		current = CPU_8086; //8086!
	}
	else //80186?
	{
		current = CPU_80186; //80186!
	}
	int file = ExecuteList(15,4,itemlist[current],256); //Show options for the installed CPU!
	switch (file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = 0; //Default CPU!
	default: //Changed?
		if (file!=current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			switch (file) //Which CPU?
			{
			case CPU_8086: //8086?
				BIOS_Settings.emulated_CPU = CPU_8086; //Use the 8086!
				break;
			case CPU_80186: //80186?
				BIOS_Settings.emulated_CPU = CPU_80186; //Use the 80186!
				break;
			default: //Unknown CPU?
				BIOS_Settings.emulated_CPU = CPU_8086; //Use the 8086!
				break;
			}
		}
		break;
	}
	BIOS_Menu = 8; //Return to Advanced menu!
}

void BIOS_AdvancedMenu() //Manages the boot order etc!
{
	BIOS_Title("Advanced Menu");
	BIOS_InitAdvancedText(); //Init text!
	int menuresult = BIOS_ShowMenu(advancedoptions,4,BIOSMENU_SPEC_LR,&Menu_Stat); //Show the menu options!
	switch (menuresult)
	{
	case BIOSMENU_SPEC_LTRIGGER: //L: Disk menu?
		BIOS_Menu = 1; //Goto Disk Menu!
		break;
	case BIOSMENU_SPEC_RTRIGGER: //R: Main menu?
		BIOS_Menu = 0; //Goto Main Menu!
		break;

	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6: //Valid option?
		switch (optioninfo[menuresult]) //What option has been chosen, since we are dynamic size?
		{
		case 0: //Boot order (plain)?
			BIOS_Menu = 9; //Boot Order Menu!
			break;
		case 1: //Installed CPU?
			BIOS_Menu = 10; //Installed CPU Menu!
			break;
		case 2: //Debug mode?
			BIOS_Menu = 13; //Debug mode option!
			break;
		case 3: //Memory reallocation?
			BIOS_Menu = 14; //Memory reallocation!
			break;
		case 4: //Direct plot setting?
			BIOS_Menu = 15; //Direct plot setting!
			break;
		case 5: //BIOS Font?
			BIOS_Menu = 16; //BIOS Font setting!
			break;
		case 6:
			BIOS_Menu = 17; //Aspect ratio setting!
			break;
		}
		break;
	default: //Unknown option?
		BIOS_Menu = NOTIMPLEMENTED; //Not implemented yet!
		break;
	}
}






void BIOS_MainMenu() //Shows the main menu to process!
{
	BIOS_Title("Main menu");
	EMU_gotoxy(0,4); //First row of the BIOS!
	int i;
	for (i=0; i<2; i++)
	{
		bzero(menuoptions[i],sizeof(menuoptions[i])); //Init!
	}
	if (EMU_RUNNING) //Running?
	{
		strcpy(menuoptions[0],"Save Changes & Resume emulation"); //Option #0!
	}
	else
	{
		strcpy(menuoptions[0],"Save Changes & Reboot"); //Option #0!
	}
	if (EMU_RUNNING)
	{
		strcpy(menuoptions[1],"Discard Changes & Resume emulation"); //Option #1!
	}
	else
	{
		strcpy(menuoptions[1],"Discard Changes & Reboot"); //Option #1!
	}
	if (!EMU_RUNNING) //Emulator isn't running?
	{
		strcpy(menuoptions[i++],"Load BIOS defaults"); //Load defaults option!
	}

	int menuresult = BIOS_ShowMenu(i,4,BIOSMENU_SPEC_LR,&Menu_Stat); //Plain menu, allow L&R triggers!

	switch (menuresult) //What option has been chosen?
	{
	case 0: //Save&Quit?
		BIOS_Menu = -1; //Quit!
		BIOS_SaveStat = 1; //Save the BIOS!
		break;
	case 1: //Discard changes&Quit?
		BIOS_Menu = -1; //Quit!
		BIOS_SaveStat = 0; //Discard changes!
		break;
	case 2: //Load defaults?
		BIOSMenu_LoadDefaults(); //Load BIOS defaults option!
		BIOS_Changed = 1; //The BIOS has been changed!
		break;
	case BIOSMENU_SPEC_LTRIGGER: //L?
		BIOS_Menu = 8; //Goto Advanced menu!
		break;
	case BIOSMENU_SPEC_RTRIGGER: //R?
		BIOS_Menu = 1; //Goto Disk Menu!
		break;
	default: //Not implemented yet?
		BIOS_Menu = NOTIMPLEMENTED; //Go out-of-range for invalid/unrecognised menu!
		break;
	}
}

uint_32 ImageGenerator_GetImageSize(byte x, byte y, int dynamichdd) //Retrieve the size, or 0 for none!
{
	int key = 0;
	key = psp_inputkeydelay(BIOS_INPUTDELAY);
	while ((key&BUTTON_CROSS)>0) //Pressed? Wait for release!
	{
		key = psp_inputkeydelay(BIOS_INPUTDELAY);
	}
	uint_32 result = 0; //Size: result; default 0 for none! Must be a multiple of 4096 bytes for HDD!
	for (;;) //Get input; break on error!
	{
		EMU_textcolor(BIOS_ATTR_ACTIVE); //We're using active color for input!
		GPU_EMU_printscreen(x,y,"%08i MB %04i KB",(result/1024000),(result%1024000)/1024); //Show current size!
		key = psp_inputkeydelay(BIOS_INPUTDELAY); //Input key!
		if ((key & BUTTON_LEFT)>0)
		{
			if (result==0) { }
			else
			{
				if (((int_32)(result-1024000))<=0)
				{
					result = 0;    //1MB steps!
				}
				else
				{
					result -= 1024000;
				}
			}
		}
		else if ((key & BUTTON_RIGHT)>0)
		{
			result += 1024000; //Add 1MB!
		}
		else if ((key & BUTTON_UP)>0)
		{
			if (result==0) { }
			else
			{
				if (((int_32)(result-4096))<=0)
				{
					result = 0;    //4KB steps!
				}
				else
				{
					result -= 4096;
				}
			}
		}
		else if ((key & BUTTON_DOWN)>0)
		{
			result += 4096; //Add 4KB!
		}
		else if ((key & BUTTON_CROSS)>0)
		{
			while ((key&BUTTON_CROSS)>0) //Wait for release!
			{
				key = psp_inputkeydelay(BIOS_INPUTDELAY); //Input key!
			}
			return result;
		}
		else if ((key & BUTTON_CIRCLE)>0)
		{
			while ((key&BUTTON_CIRCLE)>0) //Wait for release!
			{
				key = psp_inputkeydelay(BIOS_INPUTDELAY); //Input key!
			}
			break; //Cancel!
		}
	}
	return 0; //No size: cancel!
}

void BIOS_GenerateStaticHDD() //Generate Static HDD Image!
{
	if (0) { //Disabled for now: this will use the OSK!
	BIOS_Title("Generate Static HDD Image");
	char filename[256]; //Filename container!
	bzero(filename,sizeof(filename)); //Init!
	uint_32 size = 0;
	BIOSClearScreen(); //Clear the screen!
	BIOS_Title("Generate Dynamic HDD Image"); //Full clear!
	EMU_textcolor(BIOS_ATTR_TEXT);
	if (strcmp(filename,"")!=0) //Got input?
	{
		EMU_gotoxy(0,4); //Goto position for info!
		GPU_EMU_printscreen(0,4,"Filename: %s",filename); //Show the filename!
		EMU_gotoxy(0,5); //Next row!
		GPU_EMU_printscreen(0,5,"Image size: "); //Show image size selector!!
		size = ImageGenerator_GetImageSize(12,5,0); //Get the size!
		if (size!=0) //Got size?
		{
			EMU_gotoxy(0,6); //Next row!
			GPU_EMU_printscreen(0,6,"Generating image: "); //Start of percentage!
			generateStaticImage(filename, size, 18, 6); //Generate a static image!
		}
	}
	}
	BIOS_Menu = 1; //Return to Disk Menu!
}
void BIOS_GenerateDynamicHDD() //Generate Static HDD Image!
{
	if (0) { //Disabled for now: 
	BIOS_Title("Generate Dynamic HDD Image");
	char filename[256]; //Filename container!
	bzero(filename,sizeof(filename)); //Init!
	uint_32 size = 0;
	BIOS_Title("Generate Dynamic HDD Image"); //Full clear!
	EMU_textcolor(BIOS_ATTR_TEXT);
	if (strcmp(filename,"")!=0) //Got input?
	{
		EMU_gotoxy(0,4); //Goto position for info!
		GPU_EMU_printscreen(0,4,"Filename: %s",filename); //Show the filename!
		EMU_gotoxy(0,5); //Next row!
		GPU_EMU_printscreen(0,5,"Image size: "); //Show image size selector!!
		size = ImageGenerator_GetImageSize(12,5,1); //Get the size!
		if (size!=0) //Got size?
		{
			EMU_gotoxy(0,6); //Next row!
			GPU_EMU_printscreen(0,6,"Generating image: "); //Start of percentage!
			uint_32 sizecreated;
			sizecreated = generateDynamicImage(filename, size, 18, 6); //Generate a dynamic image!
		}
	}
	}
	BIOS_Menu = 1; //Return to Disk Menu!
}

void BIOS_ConvertStaticDynamicHDD() //Generate Dynamic HDD Image from a static one!
{
	byte sector[512], verificationsector[512]; //Current sector!
	uint_32 sectorposition = 0; //Possible position of error!
	char filename[256]; //Filename container!
	bzero(filename, sizeof(filename)); //Init!
	uint_32 size = 0;
	BIOS_Title("Convert static to dynamic HDD Image"); //Full clear!
	generateFileList("img", 0, 0); //Generate file list for all .img files!
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Disk image: "); //Show selection init!
	int file = ExecuteList(12, 4, "", 256); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
		BIOS_Changed = 1; //Changed!
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_NOFILES: //No files?
		BIOS_Changed = 1; //Changed!
		break;
	default: //File?
		strcpy(filename, itemlist[file]); //Use this file!
		EMU_textcolor(BIOS_ATTR_TEXT);

		if (strcmp(filename, "") != 0) //Got input?
		{
			EMU_gotoxy(0, 4); //Goto position for info!
			GPU_EMU_printscreen(0, 4, "Filename: %s  ", filename); //Show the filename!
			EMU_gotoxy(0, 5); //Next row!
			GPU_EMU_printscreen(0, 5, "Image size: "); //Show image size selector!!
			iohdd0(filename, 0, 1, 0); //Mount the source disk!
			strcat(filename, ".sfdimg"); //Generate destination filename!
			size = getdisksize(HDD0); //Get the original size!
			if (size != 0) //Got size?
			{
				EMU_gotoxy(0, 6); //Next row!
				GPU_EMU_printscreen(0, 6, "Generating image: "); //Start of percentage!
				uint_64 sizecreated;
				sizecreated = generateDynamicImage(filename, size, 18, 6); //Generate a dynamic image!
				if (sizecreated >= size) //Correct size?
				{
					GPU_EMU_printscreen(18, 6, "      "); //Clear the creation process!
					GPU_EMU_printscreen(12, 5, "      "); //Clear the creation process!
					GPU_EMU_printscreen(12, 5, "%iMB", (sizecreated / 1024768)); //Image size
					iohdd1(filename, 0, 0, 0); //Mount the destination disk, allow writing!
					FILEPOS sectornr;
					EMU_gotoxy(0, 6); //Next row!
					GPU_EMU_printscreen(0, 6, "Generating image: "); //Start of percentage!
					byte error = 0;
					for (sectornr = 0; sectornr < sizecreated;) //Process all sectors!
					{
						if (readdata(HDD0, &sector, sectornr, 512)) //Read a sector?
						{
							if (!writedata(HDD1, &sector, sectornr, 512)) //Error writing a sector?
							{
								error = 2;
								break; //Stop reading!
							}
						}
						else //Error reading sector?
						{
							error = 1;
							break; //Stop reading!
						}
						if (!(sectornr % 5120)) //Update every 10 sectors!
						{
							GPU_EMU_printscreen(18, 6, "%i%%", (int)(((float)sectornr / (float)sizecreated)*100.0f)); //Current progress!
						}
						sectornr += 512; //Next sector!
					}
					GPU_EMU_printscreen(18, 6, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!

					//Verification!
					if (!error) //OK?
					{
						GPU_EMU_printscreen(0, 7, "Validating image: "); //Start of percentage!
						iohdd1(filename, 0, 1, 0); //Mount!
						for (sectornr = 0; sectornr < size;) //Process all sectors!
						{
							if (readdata(HDD0, &sector, sectornr, 512)) //Read a sector?
							{
								if (!readdata(HDD1, &verificationsector, sectornr, 512)) //Error reading a sector?
								{
									error = 2;
									break; //Stop reading!
								}
								else if ((sectorposition = memcmp(&sector, &verificationsector, 512)) != 0)
								{
									error = 3; //Verification error!
									break; //Stop reading!
								}
							}
							else //Error reading sector?
							{
								error = 1;
								break; //Stop reading!
							}
							if (!(sectornr % 5120)) //Update every 10 sectors!
							{
								GPU_EMU_printscreen(18, 7, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
							}
							sectornr += 512; //Next sector!
						}
						GPU_EMU_printscreen(18, 6, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
						if (error) //Error occurred?
						{
							remove(filename); //Try to remove the generated file!
							dolog(filename, "Error %i validating dynamic image sector %i/%i@byte %i", error, sectornr / 512, size / 512, sectorposition); //Error at this sector!
						}
					}
					else //Error occurred?
					{
						remove(filename); //Try to remove the generated file!
						dolog(filename, "Error #%i copying static image sector %i/%i", error, sectornr / 512, sizecreated / 512); //Error at this sector!
					}
				}
			}
		}
		break;
	}
	BIOS_Menu = 1; //Return to Disk Menu!
}

void BIOS_ConvertDynamicStaticHDD() //Generate Static HDD Image from a dynamic one!
{
	byte sector[512], verificationsector[512]; //Current sector!
	uint_32 sectorposition = 0; //Possible position of error!
	char filename[256]; //Filename container!
	bzero(filename, sizeof(filename)); //Init!
	uint_32 size = 0;
	BIOS_Title("Convert dynamic to static HDD Image"); //Full clear!
	generateFileList("sfdimg", 0, 1); //Generate file list for all .img files!
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Disk image: "); //Show selection init!
	int file = ExecuteList(12, 4, "", 256); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
		BIOS_Changed = 1; //Changed!
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_NOFILES: //No files?
		BIOS_Changed = 1; //Changed!
		break;
	default: //File?
		strcpy(filename, itemlist[file]); //Use this file!
		EMU_textcolor(BIOS_ATTR_TEXT);

		if (strcmp(filename, "") != 0) //Got input?
		{
			EMU_gotoxy(0, 4); //Goto position for info!
			GPU_EMU_printscreen(0, 4, "Filename: %s  ", filename); //Show the filename!
			EMU_gotoxy(0, 5); //Next row!
			GPU_EMU_printscreen(0, 5, "Image size: "); //Show image size selector!!
			iohdd0(filename, 0, 1, 0); //Mount the source disk!
			strcat(filename, ".img"); //Generate destination filename!
			size = getdisksize(HDD0); //Get the original size!
			dolog("BIOS", "Dynamic disk size: %i bytes = %i sectors", size, (size >> 9));
			if (size != 0) //Got size?
			{
				EMU_gotoxy(0, 6); //Next row!
				GPU_EMU_printscreen(0, 6, "Generating image: "); //Start of percentage!
				GPU_EMU_printscreen(18, 6, "      "); //Clear the creation process!
				GPU_EMU_printscreen(12, 5, "      "); //Clear the creation process!
				GPU_EMU_printscreen(12, 5, "%iMB", (size / 1024768)); //Image size
				FILEPOS sectornr;
				EMU_gotoxy(0, 6); //Next row!
				GPU_EMU_printscreen(0, 6, "Generating image: "); //Start of percentage!
				byte error = 0;
				FILE *dest;
				dest = fopen64(filename, "wb"); //Open the destination!
				for (sectornr = 0; sectornr < size;) //Process all sectors!
				{
					if (readdata(HDD0, &sector, sectornr, 512)) //Read a sector?
					{
						if (fwrite64(&sector,1,512,dest)!=512) //Error writing a sector?
						{
							error = 2;
							break; //Stop reading!
						}
					}
					else //Error reading sector?
					{
						error = 1;
						break; //Stop reading!
					}
					if (!(sectornr % 5120)) //Update every 10 sectors!
					{
						GPU_EMU_printscreen(18, 6, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
					}
					sectornr += 512; //Next sector!
				}
				fclose64(dest); //Close the file!

				GPU_EMU_printscreen(18, 6, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!

				//Verification!
				if (!error) //OK?
				{
					GPU_EMU_printscreen(0, 7, "Validating image: "); //Start of percentage!
					iohdd1(filename, 0, 1, 0); //Mount!
					for (sectornr = 0; sectornr < size;) //Process all sectors!
					{
						if (readdata(HDD0, &sector, sectornr, 512)) //Read a sector?
						{
							if (!readdata(HDD1, &verificationsector, sectornr, 512)) //Error reading a sector?
							{
								error = 2;
								break; //Stop reading!
							}
							else if ((sectorposition = memcmp(&sector,&verificationsector,512)) != 0)
							{
								error = 3; //Verification error!
								break; //Stop reading!
							}
						}
						else //Error reading sector?
						{
							error = 1;
							break; //Stop reading!
						}
						if (!(sectornr % 5120)) //Update every 10 sectors!
						{
							GPU_EMU_printscreen(18, 7, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
						}
						sectornr += 512; //Next sector!
					}
					GPU_EMU_printscreen(18, 6, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
					if (error) //Error occurred?
					{
						remove(filename); //Try to remove the generated file!
						dolog(filename, "Error #%i validating static image sector %i/%i@byte %i", error, sectornr / 512, size / 512,sectorposition); //Error at this sector!
					}
				}
				else //Error occurred?
				{
					remove(filename); //Try to remove the generated file!
					dolog(filename, "Error #%i copying dynamic image sector %i/%i", error, sectornr / 512, size / 512); //Error at this sector!
				}
			}
		}
		break;
	}
	BIOS_Menu = 1; //Return to Disk Menu!
}

void BIOS_DefragmentDynamicHDD() //Defragment a dynamic HDD Image!
{
	byte sector[512], verificationsector[512]; //Current sector!
	uint_32 sectorposition = 0; //Possible position of error!
	char filename[256], originalfilename[256]; //Filename container!
	bzero(filename, sizeof(filename)); //Init!
	uint_32 size = 0;
	BIOS_Title("Defragment a dynamic HDD Image"); //Full clear!
	generateFileList("sfdimg", 0, 1); //Generate file list for all .img files!
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Disk image: "); //Show selection init!
	int file = ExecuteList(12, 4, "", 256); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
		BIOS_Changed = 1; //Changed!
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_NOFILES: //No files?
		BIOS_Changed = 1; //Changed!
		break;
	default: //File?
		strcpy(filename, itemlist[file]); //Use this file!
		EMU_textcolor(BIOS_ATTR_TEXT);

		if (strcmp(filename, "") != 0) //Got input?
		{
			EMU_gotoxy(0, 4); //Goto position for info!
			GPU_EMU_printscreen(0, 4, "Filename: %s ", filename); //Show the filename!
			EMU_gotoxy(0, 5); //Next row!
			GPU_EMU_printscreen(0, 5, "Image size: "); //Show image size selector!!
			iohdd0(filename, 0, 1, 0); //Mount the source disk!
			bzero(&originalfilename, sizeof(originalfilename)); //Init!
			strcpy(originalfilename, filename); //The original filename!
			strcat(filename, ".tmp.sfdimg"); //Generate destination filename!
			size = getdisksize(HDD0); //Get the original size!
			if (size != 0) //Got size?
			{
				EMU_gotoxy(0, 6); //Next row!
				GPU_EMU_printscreen(0, 6, "Defragmenting image: "); //Start of percentage!
				uint_64 sizecreated;
				sizecreated = generateDynamicImage(filename, size, 21, 6); //Generate a dynamic image!
				if (sizecreated >= size) //Correct size?
				{
					GPU_EMU_printscreen(21, 6, "      "); //Clear the creation process!
					GPU_EMU_printscreen(12, 5, "      "); //Clear the creation process!
					GPU_EMU_printscreen(12, 5, "%iMB", (sizecreated / 1024768)); //Image size
					iohdd1(filename, 0, 0, 0); //Mount the destination disk, allow writing!
					FILEPOS sectornr;
					byte error = 0;
					for (sectornr = 0; sectornr < sizecreated;) //Process all sectors!
					{
						if (readdata(HDD0, &sector, sectornr, 512)) //Read a sector?
						{
							if (!writedata(HDD1, &sector, sectornr, 512)) //Error writing a sector?
							{
								error = 2;
								break; //Stop reading!
							}
						}
						else //Error reading sector?
						{
							error = 1;
							break; //Stop reading!
						}
						if (!(sectornr % 5120)) //Update every 10 sectors!
						{
							GPU_EMU_printscreen(21, 6, "%i%%", (int)(((float)sectornr / (float)sizecreated)*100.0f)); //Current progress!
						}
						sectornr += 512; //Next sector!
					}
					GPU_EMU_printscreen(21, 6, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!

					//Verification!
					if (!error) //OK?
					{
						GPU_EMU_printscreen(0, 7, "Validating image: "); //Start of percentage!
						iohdd1(filename, 0, 1, 0); //Mount!
						for (sectornr = 0; sectornr < size;) //Process all sectors!
						{
							if (readdata(HDD0, &sector, sectornr, 512)) //Read a sector?
							{
								if (!readdata(HDD1, &verificationsector, sectornr, 512)) //Error reading a sector?
								{
									error = 2;
									break; //Stop reading!
								}
								else if ((sectorposition = memcmp(&sector, &verificationsector, 512)) != 0)
								{
									error = 3; //Verification error!
									break; //Stop reading!
								}
							}
							else //Error reading sector?
							{
								error = 1;
								break; //Stop reading!
							}
							if (!(sectornr % 5120)) //Update every 10 sectors!
							{
								GPU_EMU_printscreen(18, 7, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
							}
							sectornr += 512; //Next sector!
						}
						GPU_EMU_printscreen(18, 6, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
						if (error) //Error occurred?
						{
							dolog(filename, "Error %i validating dynamic image sector %i/%i@byte %i", error, sectornr / 512, size / 512, sectorposition); //Error at this sector!
						}
						else //We've been defragmented?
						{
							if (!remove(originalfilename)) //Original can be removed?
							{
								if (!rename(filename, originalfilename)) //The destination is the new original!
								{
									dolog("BIOS", "Error renaming the new defragmented image to the original filename!");
								}
							}
							else
							{
								dolog("BIOS", "Error replacing the old image with the defragmented image!");
							}
						}
					}
					else //Error occurred?
					{
						dolog(filename, "Error #%i copying dynamic image sector %i/%i", error, sectornr / 512, sizecreated / 512); //Error at this sector!
					}
				}
			}
		}
		break;
	}
	BIOS_Menu = 1; //Return to Disk Menu!
}

void BIOS_DebugMode()
{
	BIOS_Title("Debug mode");
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Debug mode: "); //Show selection init!
	int i = 0; //Counter!
	numlist = 9; //Ammount of Debug modes!
	for (i=0; i<9; i++) //Process options!
	{
		bzero(itemlist[i],sizeof(itemlist[i])); //Reset!
	}
	strcpy(itemlist[DEBUGMODE_NONE],"No debugger enabled"); //Set filename from options!
	strcpy(itemlist[DEBUGMODE_RTRIGGER],"Enabled, RTrigger=Step"); //Set filename from options!
	strcpy(itemlist[DEBUGMODE_STEP],"Enabled, Step through"); //Set filename from options!
	strcpy(itemlist[DEBUGMODE_SHOW_RUN],"Enabled, just run, ignore shoulder buttons"); //Set filename from options!
	strcpy(itemlist[DEBUGMODE_TEST],"Enabled, run debug directory files, else TESTROM.DAT at 0000:0000"); //Set filename from options!
	strcpy(itemlist[DEBUGMODE_TEST_STEP],"Enabled, Step through, run debug directory files, else TESTROM.DAT at 0000:0000"); //Set filename from options!
	strcpy(itemlist[DEBUGMODE_TEXT],"No debugger enabled, debug text-mode characters"); //Set filename from options!
	strcpy(itemlist[DEBUGMODE_BIOS],"No debugger enabled, load BIOS from BIOSROM.DAT"); //Set filename from options!
	strcpy(itemlist[DEBUGMODE_SOUND],"No debugger enabled, run sound test"); //Debug sound test!
	int current = 0;
	switch (BIOS_Settings.debugmode) //What debug mode?
	{
	case DEBUGMODE_NONE: //Valid
	case DEBUGMODE_RTRIGGER: //Valid
	case DEBUGMODE_STEP: //Valid
	case DEBUGMODE_SHOW_RUN: //Valid
	case DEBUGMODE_TEST: //Test files or biosrom.dat!
	case DEBUGMODE_TEST_STEP: //Test files or biosrom.dat!
	case DEBUGMODE_TEXT: //Text character debugging?
	case DEBUGMODE_BIOS: //External BIOS?
	case DEBUGMODE_SOUND: //Sound test?
		current = BIOS_Settings.debugmode; //Valid: use!
		break;
	default: //Invalid
		current = DEBUGMODE_NONE; //Default: none!
		break;
	}
	if (BIOS_Settings.debugmode!=current) //Invalid?
	{
		BIOS_Settings.debugmode = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int file = ExecuteList(15,4,itemlist[current],256); //Show options for the installed CPU!
	switch (file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = DEBUGMODE_NONE; //Default debugmode: None!

	case DEBUGMODE_NONE:
	case DEBUGMODE_RTRIGGER:
	case DEBUGMODE_STEP:
	case DEBUGMODE_SHOW_RUN:
	case DEBUGMODE_TEST:
	case DEBUGMODE_TEST_STEP:
	case DEBUGMODE_TEXT:
	case DEBUGMODE_BIOS:
	case DEBUGMODE_SOUND:
	default: //Changed?
		if (file!=current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.debugmode = file; //Select Debug Mode!
		}
		break;
	}
	BIOS_Menu = 8; //Goto Advanced menu!
}

extern byte force_memoryredetect; //From the MMU: force memory redetect on load?

void BIOS_MemReAlloc() //Reallocates BIOS memory!
{
	return; //Disable due to the fact that memory allocations aren't 100% OK atm.

	force_memoryredetect = 1; //We're forcing memory redetect!
	doneEMU(); //Finish the old EMU memory!

	autoDetectMemorySize(0); //Check memory size if needed!
	initEMU(1); //Start a new EMU memory!
	
	BIOS_Changed = 1; //Changed!
	BIOS_Menu = 8; //Goto Advanced menu!
}

void BIOS_DirectPlotSetting()
{
	BIOS_Title("Direct plot");
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Direct plot: "); //Show selection init!
	int i = 0; //Counter!
	numlist = 3; //Ammount of Direct modes!
	for (i=0; i<3; i++) //Process options!
	{
		bzero(itemlist[i],sizeof(itemlist[i])); //Reset!
	}
	strcpy(itemlist[0],"Disabled"); //Set filename from options!
	strcpy(itemlist[1],"Automatic"); //Set filename from options!
	strcpy(itemlist[2],"Forced"); //Set filename from options!
	int current = 0;
	switch (BIOS_Settings.VGA_AllowDirectPlot) //What direct plot?
	{
	case 0: //Valid
	case 1: //Valid
	case 2: //Valid
		current = BIOS_Settings.VGA_AllowDirectPlot; //Valid: use!
		break;
	default: //Invalid
		current = 0; //Default: none!
		break;
	}
	if (BIOS_Settings.VGA_AllowDirectPlot!=current) //Invalid?
	{
		BIOS_Settings.VGA_AllowDirectPlot = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int file = ExecuteList(15,4,itemlist[current],256); //Show options for the installed CPU!
	switch (file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = 0; //Default direct plot: None!

	case 0:
	case 1:
	default: //Changed?
		if (file!=current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.VGA_AllowDirectPlot = file; //Select Direct Plot setting!
		}
		break;
	}
	BIOS_Menu = 8; //Goto Advanced menu!
}

void BIOS_FontSetting()
{
	BIOS_Title("Font");
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"BIOS Font: "); //Show selection init!
	int i = 0; //Counter!
	numlist = NUMITEMS(BIOSMenu_Fonts); //Ammount of Direct modes!
	for (i=0; i<numlist; i++) //Process options!
	{
		bzero(itemlist[i],sizeof(itemlist[i])); //Reset!
		strcpy(itemlist[i],BIOSMenu_Fonts[i].name); //Use the name!
	}
	int current = 0;
	if (BIOS_Settings.BIOSmenu_font<NUMITEMS(BIOSMenu_Fonts)) //Valid font?
	{
		current = BIOS_Settings.BIOSmenu_font; //Valid: use!
	}
	else
	{
		current = 0; //Default: none!
	}
	if (BIOS_Settings.BIOSmenu_font!=current) //Invalid?
	{
		BIOS_Settings.BIOSmenu_font = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int font = ExecuteList(11,4,itemlist[current],256); //Show options for the installed CPU!
	switch (font) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		font = 0; //Default font: Standard!
	default: //Changed?
		if (font!=current && current<NUMITEMS(BIOSMenu_Fonts)) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.BIOSmenu_font = font; //Select Direct Plot setting!
		}
		break;
	}
	BIOS_Menu = 8; //Goto Advanced menu!
}

void BIOS_KeepAspectRatio()
{
	if (BIOS_Settings.keepaspectratio)
	{
		BIOS_Settings.keepaspectratio = 0; //Reset!
	}
	else
	{
		BIOS_Settings.keepaspectratio = 1; //Set!
	}
	BIOS_Changed = 1; //We've changed!
	BIOS_Menu = 8; //Goto Advanced menu!
}

void BIOSMenu_LoadDefaults() //Load the defaults option!
{
	if (__HW_DISABLED) return; //Abort!
	int showchecksumerrors_backup = showchecksumerrors; //Keep this!
	showchecksumerrors = 0; //Don't show checksum errors!
	BIOS_LoadDefaults(0); //Load BIOS Defaults, don't save!
	showchecksumerrors = showchecksumerrors_backup; //Restore!
	BIOS_Changed = 1; //Changed!
	BIOS_Menu = 0; //Goto Main menu!
}
