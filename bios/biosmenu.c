#include "headers/types.h" //Types and linkage!
#include "headers/bios/bios.h" //Basic BIOS compatibility types etc including myself!
#include "headers/cpu/cpu.h" //CPU constants!
#include "headers/emu/gpu/gpu.h" //GPU compatibility!
#include "headers/basicio/staticimage.h" //Static image compatibility!
#include "headers/basicio/dynamicimage.h" //Dynamic image compatibility!
#include "headers/basicio/io.h" //Basic I/O comp!
#include "headers/emu/input.h" //Basic key input!
#include "headers/mmu/bda.h" //Bios Data Area!
#include "headers/hardware/vga/vga.h" //VGA!
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

#include "headers/hardware/ps2_keyboard.h" //PS/2 keyboard key name support!

#include "headers/emu/input.h" //We need input using psp_inputkey.

#include "headers/emu/emu_vga.h" //VGA update support!

#include "headers/basicio/dskimage.h" //DSK image support!

#include "headers/support/mid.h" //MIDI player support!

#define __HW_DISABLED 0

//Force the BIOS to open?
#define FORCE_BIOS 0

//BIOS width in text mode!
#define BIOS_WIDTH GPU_TEXTSURFACE_WIDTH

//Boot time in 2 seconds!
#define BOOTTIME 2000000

typedef struct
{
char name[256]; //The name for display!
byte Header; //Header (BIOS Text) font/background!
byte Menubar_inactive; //Menu bar (inactive)
byte Menubar_active; //Menu bar (active)
byte Text; //Text font
byte Border; //Border font
byte Option_inactive; //Inactive option
byte Option_active; //Active option
byte Selectable_inactive; //Inactive selectable (with submenu)
byte Selectable_active; //Active selectable (with submenu)
byte Sub_border; //Selectable option border!
byte Sub_Option_inactive; //Sub Selectable option inactive
byte Sub_Option_active; //Sub Selectable option active
byte Bottom_key; //Bottom key(combination) text
byte Bottom_text; //Bottom text (information about the action of the key)
byte Background; //Simple background only! (Don't show text!)
byte HighestBitBlink; //Highest color bit is blinking? 0=Off: Use 16 colors, 1=On: Use 8 colors!
} BIOSMENU_FONT; //All BIOS fonts!

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

//Now for the seperate menus:
void BIOS_MainMenu(); //Main menu!
void BIOS_DisksMenu(); //Manages the mounted disks!
void BIOS_floppy0_selection(); //FLOPPY0 selection menu!
void BIOS_floppy1_selection(); //FLOPPY1 selection menu!
void BIOS_hdd0_selection(); //HDD0 selection menu!
void BIOS_hdd1_selection(); //HDD1 selection menu!
void BIOS_cdrom0_selection(); //CDROM0 selection menu!
void BIOS_cdrom1_selection(); //CDROM1 selection menu!
void BIOS_AdvancedMenu(); //Advanced menu!
void BIOS_BootOrderOption(); //Boot order option!
void BIOS_InstalledCPUOption(); //Manages the installed CPU!
void BIOS_GenerateStaticHDD(); //Generate Static HDD Image!
void BIOS_GenerateDynamicHDD(); //Generate Static HDD Image!
void BIOS_DebugMode(); //Switch BIOS Mode!
void BIOS_DebugLog(); //Debugger log!
void BIOS_ExecutionMode(); //Switch execution mode!
void BIOS_MemReAlloc(); //Reallocate memory!
void BIOS_DirectPlotSetting(); //Direct Plot Setting!
void BIOS_FontSetting(); //BIOS Font Setting!
void BIOS_KeepAspectRatio(); //Keep aspect ratio?
void BIOS_ConvertStaticDynamicHDD(); //Convert static to dynamic HDD?
void BIOS_ConvertDynamicStaticHDD(); //Generate Static HDD Image from a dynamic one!
void BIOS_DefragmentDynamicHDD(); //Defragment a dynamic HDD Image!
void BIOS_BWMonitor(); //Switch b/w monitor vs color monitor!
void BIOS_inputMenu(); //Manage stuff concerning input.
void BIOS_gamingModeButtonsMenu(); //Manage stuff concerning input.
void BIOS_gamingKeyboardColorsMenu(); //Manage stuff concerning input.
void BIOS_gamingKeyboardColor(); //Select a gaming keyboard color!
void BIOSMenu_LoadDefaults(); //Load the defaults option!
void BIOSClearScreen(); //Resets the BIOS's screen!
void BIOSDoneScreen(); //Cleans up the BIOS's screen!
void BIOS_VGASettingsMenu(); //Manage stuff concerning input.
void BIOS_VGANMISetting(); //VGA NMI setting!
void BIOS_MIDISettingsMenu(); //Manage stuff concerning MIDI.
void BIOS_SoundFont_selection(); //FLOPPY0 selection menu!
void BIOS_MIDIPlayer(); //MIDI player!
void BIOS_Mouse(); //Mouse selection menu!
void BIOS_CPU(); //CPU menu!
void BIOS_CPUSpeed(); //CPU speed selection!
void BIOS_ClearCMOS(); //Clear the CMOS!

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
	,BIOS_BWMonitor //Switch to a b/w monitor or color monitor is #22!
	,BIOS_DebugLog //Enable/disable debugger log is #23!
	,BIOS_ExecutionMode //Execution mode is #24!
	,BIOS_inputMenu //Input menu is #25!
	,BIOS_gamingModeButtonsMenu //Gaming mode buttons menu is #26!
	,BIOS_gamingKeyboardColorsMenu //Keyboard colors menu is #27!
	,BIOS_gamingKeyboardColor //Keyboard color menu is #28!
	,BIOS_VGASettingsMenu //Manage stuff concerning VGA Settings is #29!
	,BIOS_VGANMISetting //VGA NMI setting is #30!
	,BIOS_MIDISettingsMenu //MIDI settings menu is #31!
	,BIOS_SoundFont_selection //Soundfont selection menu is #32!
	,BIOS_MIDIPlayer //MIDI Player is #33!
	,BIOS_Mouse //Mouse menu is #34!
	,BIOS_CPU //BIOS CPU menu is #35!
	,BIOS_CPUSpeed //BIOS CPU speed is #36!
	,BIOS_ClearCMOS //BIOS CMOS clear is #37!
};

//Not implemented?
#define NOTIMPLEMENTED NUMITEMS(BIOS_Menus)+1

sword BIOS_Menu = 0; //What menu are we opening (-1 for closing!)?
byte BIOS_SaveStat = 0; //To save the BIOS?
byte BIOS_Changed = 0; //BIOS Changed?

byte BIOS_EnablePlay = 0; //Enable play button=OK?

GPU_TEXTSURFACE *BIOS_Surface; //Our very own BIOS Surface!

int advancedoptions = 0; //Number of advanced options!
byte optioninfo[0x10]; //Option info for what option!

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
		EMU_locktext();
		EMU_textcolor(0xE); //Yellow on black!
		GPU_EMU_printscreen(0,0,"Press SELECT to bring out the BIOS");
		EMU_unlocktext();
	}
	else //Normal BIOS POST!
	{
		EMU_locktext();
		printmsg(0xE,"Press SELECT to run BIOS SETUP");
		EMU_unlocktext();
	}
	
	showchecksumerrors = 0; //Don't show!
	BIOS_LoadData(); //Now load/reset the BIOS
	showchecksumerrors = 1; //Reset!
	
	while (counter>0) //Time left?
	{
		counter -= INPUT_INTERVAL; //One further!
		delay(INPUT_INTERVAL); //Intervals of one!
		if (shuttingdown()) //Request shutdown?
		{
			return 0; //No reset!
		}
		if ((psp_inputkey() & BUTTON_SELECT) || BIOS_Settings.firstrun || FORCE_BIOS) //R trigger pressed or first run?
		{
			if (timeout) //Before boot?
			{
				EMU_locktext();
				GPU_EMU_printscreen(0,0,"                                  "); //Clear our text!
				EMU_unlocktext();
			}
			if (runBIOS(!timeout)) //Run the BIOS! Show text if timeout is specified!
			{
				//We're dirty, so reset!
				return 1; //We've to reset!
			}
		}
	}
	if (timeout)
	{
		EMU_locktext();
		GPU_EMU_printscreen(0,0,"                                  "); //Clear our text!
		EMU_unlocktext();
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

byte reboot_needed = 0; //Default: no reboot needed!

void BIOS_MenuChooser(); //The menu chooser prototype for runBIOS!
byte runBIOS(byte showloadingtext) //Run the BIOS menu (whether in emulation or boot is by EMU_RUNNING)!
{
	if (__HW_DISABLED) return 0; //Abort!
	EMU_stopInput(); //Stop all emu input!
	terminateVGA(); //Terminate currently running VGA for a speed up!
	//dolog("BIOS","Running BIOS...");
	showchecksumerrors = 0; //Not showing any checksum errors!

//Now reset/save all we need to run the BIOS!
	byte frameratebackup = GPU.show_framerate; //Backup!

	GPU.show_framerate = 0; //Hide the framerate surface!	
	
//Now do the BIOS stuff!
	if (showloadingtext) //Not in emulator?
	{
		EMU_textcolor(0xF);
		printmsg(0xF,"\r\nLoading BIOS...");
		delay(500000); //0.5 sec!
	}

	stopEMUTimers(); //Stop our timers!
	
	GPU_text_locksurface(frameratesurface);
	GPU_textclearscreen(frameratesurface); //Make sure the surface is empty for a neat BIOS!
	GPU_text_releasesurface(frameratesurface);
	
	BIOS_LoadData(); //Now load/reset the BIOS
	BIOS_Changed = 0; //Default: the BIOS hasn't been changed!
	BIOS_SaveStat = 0; //Default: not saving!
	showchecksumerrors = 0; //Default: not showing checksum errors!
	BIOS_clearscreen(); //Clear the screen!
	BIOS_Menu = 0; //We're opening the main menu!

	reboot_needed = 0; //Do we need to reboot?
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
			EMU_locktext();
			BIOS_clearscreen(); //Clear the screen!
			EMU_gotoxy(0,0); //First column,row!
			EMU_textcolor(0xF);
			GPU_EMU_printscreen(0,0,"Error: couldn't save the BIOS!");
			EMU_unlocktext();
			delay(5000000); //Wait 5 sec before rebooting!
		}
		else
		{
			BIOS_clearscreen(); //Clear the screen!

			if (!EMU_RUNNING) //Emulator isn't running?
			{
				EMU_locktext();
				EMU_gotoxy(0,0); //First column,row!
				EMU_textcolor(0xF);
				GPU_EMU_printscreen(0,0,"BIOS Saved!");
				EMU_unlocktext();
				delay(2000000); //Wait 2 sec before rebooting!
			}
			else //Emulator running?
			{
				EMU_locktext();
				EMU_gotoxy(0,0); //First column,row!
				EMU_textcolor(0xF);
				GPU_EMU_printscreen(0,0,"BIOS Saved (Returning to the emulator)!"); //Info!
				EMU_unlocktext();
				delay(2000000); //Wait 2 sec!
			}
		}

	}
	else //Discard changes?
	{
		EMU_locktext();
		EMU_gotoxy(0,0);
		EMU_textcolor(0xF);
		GPU_EMU_printscreen(0,0,"BIOS Discarded!"); //Info!
		EMU_unlocktext();
		BIOS_LoadData(); //Reload!
		delay(2000000); //Wait 2 sec!
	}

	BIOSDoneScreen(); //Clean up the screen!
//Now return to the emulator to reboot!
	BIOS_ValidateData(); //Validate&reload all disks!
	GPU_keepAspectRatio(BIOS_Settings.keepaspectratio); //Keep the aspect ratio?

//Restore all states saved for the BIOS!
	GPU.show_framerate = frameratebackup; //Restore!
	startEMUTimers(); //Start our timers up again!
	startVGA(); //Start the VGA up again!
	EMU_startInput(); //Start all emu input again!

	EMU_update_VGA_Settings(); //Update the VGA Settings to it's default value!

	return (reboot_needed==2) || ((reboot_needed==1) && (BIOS_SaveStat && BIOS_Changed)); //Do we need to reboot: when required or chosen!
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
	if (text)
	{
		EMU_locktext();
		GPU_EMU_printscreen(CALCMIDDLE(BIOS_WIDTH, safe_strlen(text, 256)), row, text); //Show centered text!
		EMU_unlocktext();
	}
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
		EMU_locktext();
		GPU_EMU_printscreen(i++,row," "); //Clear BIOS header!
		EMU_unlocktext();
	}
}


void BIOSClearScreen() //Resets the BIOS's screen!
{
	if (__HW_DISABLED) return; //Abort!
	GPU_text_locksurface(frameratesurface);
	GPU_textclearscreen(frameratesurface); //Make sure the surface is empty for a neat BIOS!
	GPU_text_releasesurface(frameratesurface);
	char BIOSText[] = "x86 BIOS"; //The BIOS's text!
	//cursorXY(0,0,0); //Goto top of the screen!

	EMU_textcolor(BIOS_ATTR_TEXT); //Plain text!
	/*CPU[activeCPU].registers->AX = VIDEOMODE_EMU; //Video mode 80x25 16 colors!
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
	EMU_locktext();
	EMU_textcolor(BIOSTOP_ATTR); //Switch to BIOS Header attribute!
	EMU_unlocktext();
	printcenter(BIOSText,0); //Show the BIOS's text!
	EMU_locktext();
	GPU_EMU_printscreen(BIOS_WIDTH-safe_strlen("MEM:12MB",256),0,"MEM:%02iMB",(BIOS_Settings.memory/MBMEMORY)); //Show ammount of memory to be able to use!
	EMU_textcolor(BIOS_ATTR_TEXT); //Std: display text!
	EMU_unlocktext();
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
		if (shuttingdown()) //Are we requesting a shutdown?
		{
			BIOS_SaveStat = 0; //Ignore any changes!
			return; //Shut down!
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
#define BIOSMENU_SPEC_SQUAREOPTION 4


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
		if (shuttingdown()) //Cancel?
		{
			option = BIOSMENU_SPEC_CANCEL;
			break;
		}
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
				EMU_locktext();
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
				EMU_unlocktext();
			}
			oldoption = option; //Save our changes!
		}
		delay(0); //Wait for other threads!
	}
	return option; //Give the chosen option!
}

//File list functions!

//Ammount of files in the list MAX
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

void printCurrent(int x, int y, char *text, int maxlen, list_information information_handler) //Create the current item with maximum length!
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
	EMU_locktext();
	EMU_textcolor(BIOS_ATTR_ACTIVE); //Active item!
	GPU_EMU_printscreen(x,y,"%s",buffer); //Show item with maximum length or less!

	EMU_textcolor(BIOS_ATTR_BACKGROUND); //Background of the current item!
	GPU_EMU_printscreen(-1,-1,"%s",filler); //Show rest with filler color, update!
	if (information_handler) //Gotten an information handler?
	{
		information_handler(text); //Execute the information handler!
	}
	EMU_unlocktext();
}

//x,y = coordinates of file list
//maxlen = ammount of characters for the list (width of the list)

int ExecuteList(int x, int y, char *defaultentry, int maxlen, list_information informationhandler) //Runs the file list!
{
	int key = 0;
	//First, no file check!
	if (!numlist) //No files?
	{
		EMU_locktext();
		EMU_gotoxy(x,y); //Goto position of output!
		EMU_textcolor(BIOS_ATTR_TEXT); //Plain text!
		GPU_EMU_printscreen(x,y,"No files found!");
		EMU_unlocktext();
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

	printCurrent(x,y,itemlist[result],maxlen,informationhandler); //Create our current entry!
	
	while (1) //Doing selection?
	{
		if (shuttingdown()) //Cancel?
		{
			return FILELIST_CANCEL; //Cancel!
		}
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
			printCurrent(x,y,itemlist[result],maxlen,informationhandler); //Create our current entry!
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
			printCurrent(x, y, itemlist[result], maxlen,informationhandler); //Create our current entry!
		}
		else if ((key&BUTTON_CROSS)>0 || (key&BUTTON_PLAY && BIOS_EnablePlay)) //OK?
		{
			delay(500000); //Wait a bit before continuing!
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

void hdd_information(char *filename) //Displays information about a harddisk to mount!
{
	FILEPOS size;
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	if (is_dynamicimage(filename)) //Dynamic image?
	{
		size = dynamicimage_getsize(filename); //Get the filesize!
		GPU_EMU_printscreen(0, 6, "This is a Superfury Dynamic Disk Image file."); //Show selection init!
		GPU_EMU_printscreen(0, 7, "Disk size: %08i MB %04i KB", (uint_32)(size / MBMEMORY), (uint_32)((size % MBMEMORY) / 1024)); //Show size too!
	}
	else if (is_staticimage(filename)) //Static image?
	{
		size = staticimage_getsize(filename); //Get the filesize!
		GPU_EMU_printscreen(0, 6, "This is a Static Disk Image file.           "); //Show selection init!
		GPU_EMU_printscreen(0, 7, "Disk size: %08i MB %04i KB", (uint_32)(size / MBMEMORY), (uint_32)((size % MBMEMORY) / 1024)); //Show size too!
	}
	else if (is_DSKimage(filename)) //DSK disk image?
	{
		DISKINFORMATIONBLOCK dskinfo;
		if (!readDSKInfo(filename, &dskinfo)) goto unknownimage;
		size = dskinfo.NumberOfSides*dskinfo.NumberOfTracks*dskinfo.TrackSize; //Get the total disk image size!
		size = dynamicimage_getsize(filename); //Get the filesize!
		GPU_EMU_printscreen(0, 6, "This is a DSK Disk Image file.              "); //Show selection init!
		GPU_EMU_printscreen(0, 7, "Disk size: %08i MB %04i KB", (uint_32)(size / MBMEMORY), (uint_32)((size % MBMEMORY) / 1024)); //Show size too!
	}
	else //Unknown file type: no information?
	{
	unknownimage: //Unknown disk image?
		GPU_EMU_printscreen(0, 6, "This is an unknown Disk Image file.         "); //Show selection init!
		GPU_EMU_printscreen(0, 7, "                              "); //Clear file size info!
	}
}

//Menus itself:

//Selection menus for disk drives!

void BIOS_floppy0_selection() //FLOPPY0 selection menu!
{
	BIOS_Title("Mount FLOPPY A");
	generateFileList("img|ima|dsk",0,0); //Generate file list for all .img files!
	EMU_locktext();
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Disk image: "); //Show selection init!
	EMU_unlocktext();

	int file = ExecuteList(12,4,BIOS_Settings.floppy0,256,&hdd_information); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
	case FILELIST_NOFILES: //No files?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.floppy0,""); //Unmount!
		BIOS_Settings.floppy0_readonly = 0; //Not readonly!
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break;
	default: //File?
		BIOS_Changed = 1; //Changed!
		if (strcmp(BIOS_Settings.floppy0,itemlist[file])!=0) BIOS_Settings.floppy0_readonly = 0; //Different resets readonly flag!
		strcpy(BIOS_Settings.floppy0,itemlist[file]); //Use this file!
	}
	BIOS_Menu = 1; //Return to image menu!
}

void BIOS_floppy1_selection() //FLOPPY1 selection menu!
{
	BIOS_Title("Mount FLOPPY B");
	generateFileList("img|ima|dsk",0,0); //Generate file list for all .img files!
	EMU_locktext();
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Disk image: "); //Show selection init!
	EMU_unlocktext();
	int file = ExecuteList(12,4,BIOS_Settings.floppy1,256,&hdd_information); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
	case FILELIST_NOFILES: //No files?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.floppy1,""); //Unmount!
		BIOS_Settings.floppy0_readonly = 0; //Different resets readonly flag!
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	default: //File?
		BIOS_Changed = 1; //Changed!
		if (strcmp(BIOS_Settings.floppy1, itemlist[file]) != 0) BIOS_Settings.floppy1_readonly = 0; //Different resets readonly flag!
		strcpy(BIOS_Settings.floppy1,itemlist[file]); //Use this file!
	}
	BIOS_Menu = 1; //Return to image menu!
}

void BIOS_hdd0_selection() //HDD0 selection menu!
{
	BIOS_Title("Mount First HDD");
	generateFileList("img|sfdimg",1,1); //Generate file list for all .img files!
	EMU_locktext();
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Disk image: "); //Show selection init!
	EMU_unlocktext();
	int file = ExecuteList(12,4,BIOS_Settings.hdd0,256,&hdd_information); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
	case FILELIST_NOFILES: //No files?
		BIOS_Changed = 1; //Changed!
		reboot_needed = 1; //We need to reboot to apply the ATA changes!
		BIOS_Settings.hdd0_readonly = 0; //Different resets readonly flag!
		strcpy(BIOS_Settings.hdd0,""); //Unmount!
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	default: //File?
		BIOS_Changed = 1; //Changed!
		reboot_needed = 1; //We need to reboot to apply the ATA changes!
		if (strcmp(BIOS_Settings.hdd0, itemlist[file]) != 0) BIOS_Settings.hdd0_readonly = 0; //Different resets readonly flag!
		strcpy(BIOS_Settings.hdd0,itemlist[file]); //Use this file!
	}
	BIOS_Menu = 1; //Return to image menu!
}

void BIOS_hdd1_selection() //HDD1 selection menu!
{
	BIOS_Title("Mount Second HDD");
	generateFileList("img|sfdimg",1,1); //Generate file list for all .img files!
	EMU_locktext();
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Disk image: "); //Show selection init!
	EMU_unlocktext();
	int file = ExecuteList(12,4,BIOS_Settings.hdd1,256,&hdd_information); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
	case FILELIST_NOFILES: //No files?
		BIOS_Changed = 1; //Changed!
		reboot_needed = 1; //We need to reboot to apply the ATA changes!
		BIOS_Settings.hdd1_readonly = 0; //Different resets readonly flag!
		strcpy(BIOS_Settings.hdd1,""); //Unmount!
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	default: //File?
		BIOS_Changed = 1; //Changed!
		reboot_needed = 1; //We need to reboot to apply the ATA changes!
		if (strcmp(BIOS_Settings.hdd1, itemlist[file]) != 0) BIOS_Settings.hdd1_readonly = 0; //Different resets readonly flag!
		strcpy(BIOS_Settings.hdd1,itemlist[file]); //Use this file!
	}
	BIOS_Menu = 1; //Return to image menu!
}

void BIOS_cdrom0_selection() //CDROM0 selection menu!
{
	BIOS_Title("Mount First CD-ROM");
	generateFileList("iso",0,0); //Generate file list for all .img files!
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Disk image: "); //Show selection init!
	EMU_unlocktext();
	int file = ExecuteList(12,4,BIOS_Settings.cdrom0,256,NULL); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
	case FILELIST_NOFILES: //No files?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.cdrom0,""); //Unmount!
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
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
	EMU_locktext();
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Disk image: "); //Show selection init!
	EMU_unlocktext();
	int file = ExecuteList(12,4,BIOS_Settings.cdrom1,256,NULL); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
	case FILELIST_NOFILES: //No files?
		BIOS_Changed = 1; //Changed!
		strcpy(BIOS_Settings.cdrom1,""); //Unmount!
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
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
		if (BIOS_Settings.hdd1_readonly) //Read-only?
		{
			strcat(menuoptions[3]," <R>"); //Show readonly tag!
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

void BIOS_InitAdvancedText()
{
	advancedoptions = 0; //Init!
	int i;
	for (i=0; i<12; i++) //Clear all possibilities!
	{
		bzero(menuoptions[i],sizeof(menuoptions[i])); //Init!
	}
	if (!EMU_RUNNING) //Just plain menu (not an running emu?)?
	{
		optioninfo[advancedoptions] = 0; //Boot Order!
		strcpy(menuoptions[advancedoptions],"Boot Order: "); //Change boot order!
		strcat(menuoptions[advancedoptions++],BOOT_ORDER_STRING[BIOS_Settings.bootorder]); //Add boot order after!
		optioninfo[advancedoptions] = 1; //CPU menu!
		strcpy(menuoptions[advancedoptions++],"CPU"); //Change installed CPU options!
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
	default:
		strcat(menuoptions[advancedoptions++],"<UNKNOWN. CHECK BIOS VERSION>");
		break;
	}

	optioninfo[advancedoptions] = 7; //We're debug log setting!
	strcpy(menuoptions[advancedoptions], "Debugger log: ");
	switch (BIOS_Settings.debugger_log)
	{
	case DEBUGGERLOG_NONE: //None
		strcat(menuoptions[advancedoptions++], "Don't log"); //Set filename from options!
		break;
	case DEBUGGERLOG_DEBUGGING: //Only when debugging
		strcat(menuoptions[advancedoptions++], "Only when debugging"); //Set filename from options!
		break;
	case DEBUGGERLOG_ALWAYS: //Always
		strcat(menuoptions[advancedoptions++], "Always log"); //Set filename from options!
		break;
		break;
	default:
		strcat(menuoptions[advancedoptions++], "Never"); //Set filename from options!
		break;
	}
	optioninfo[advancedoptions] = 8; //Execution mode!
	strcpy(menuoptions[advancedoptions], "Execution mode: ");
	switch (BIOS_Settings.executionmode) //What debug mode is active?
	{
	case EXECUTIONMODE_NONE:
		strcat(menuoptions[advancedoptions++], "Normal operations"); //Set filename from options!
		break;
	case EXECUTIONMODE_TEST:
		strcat(menuoptions[advancedoptions++], "Run debug directory files"); //Set filename from options!
		break;
	case EXECUTIONMODE_TESTROM:
		strcat(menuoptions[advancedoptions++], "Run TESTROM.DAT at 0000:0000"); //Set filename from options!
		break;
	case EXECUTIONMODE_VIDEOCARD:
		strcat(menuoptions[advancedoptions++], "Debug video card output"); //Set filename from options!
		break;
	case EXECUTIONMODE_BIOS:
		strcat(menuoptions[advancedoptions++], "Load BIOS from ROM directory."); //Set filename from options!
		break;
	case EXECUTIONMODE_SOUND:
		strcat(menuoptions[advancedoptions++], "Run sound test"); //Set filename from options!
		break;
	default:
		strcat(menuoptions[advancedoptions++], "<UNKNOWN. CHECK BIOS VERSION>");
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
	
	optioninfo[advancedoptions] = 4; //VGA Settings
	strcpy(menuoptions[advancedoptions++], "VGA Settings");

	optioninfo[advancedoptions] = 10;
	strcpy(menuoptions[advancedoptions++], "MIDI Settings");

	optioninfo[advancedoptions] = 9;
	strcpy(menuoptions[advancedoptions++], "Input options");

setMousetext: //For fixing it!
	optioninfo[advancedoptions] = 11; //Mouse!
	strcpy(menuoptions[advancedoptions], "Mouse: ");
	switch (BIOS_Settings.PS2Mouse) //Mouse?
	{
	case 0:
		strcat(menuoptions[advancedoptions++], "Serial");
		break;
	case 1:
		strcat(menuoptions[advancedoptions++], "PS/2");
		break;
	default: //Error: fix it!
		BIOS_Settings.PS2Mouse = 0; //Reset/Fix!
		BIOS_Changed = 1; //We've changed!
		goto setMousetext; //Goto!
		break;
	}

	optioninfo[advancedoptions] = 12; //Clear CMOS!
	strcpy(menuoptions[advancedoptions++], "Clear CMOS data");
}

void BIOS_BootOrderOption() //Manages the boot order
{
	BIOS_Title("Boot Order");
	EMU_locktext();
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Boot Order: "); //Show selection init!
	EMU_unlocktext();
	numlist = NUMITEMS(BOOT_ORDER_STRING); //Ammount of files (orders)
	int i = 0; //Counter!
	for (i=0; i<numlist; i++) //Process options!
	{
		bzero(itemlist[i],sizeof(itemlist[i])); //Reset!
		strcpy(itemlist[i],BOOT_ORDER_STRING[i]); //Set filename from options!
	}
	int file = ExecuteList(12,4,BOOT_ORDER_STRING[BIOS_Settings.bootorder],256,NULL); //Show options for the boot order!
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
	EMU_locktext();
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Installed CPU: "); //Show selection init!
	EMU_unlocktext();
	int i = 0; //Counter!
	numlist = 3; //Ammount of CPU types!
	for (i=0; i<3; i++) //Process options!
	{
		bzero(itemlist[i],sizeof(itemlist[i])); //Reset!
	}
	strcpy(itemlist[CPU_8086],"Intel 8086"); //Set filename from options!
	strcpy(itemlist[CPU_80186],"Intel 80186"); //Set filename from options!
	strcpy(itemlist[CPU_80286], "Intel 80286(unfinished)"); //Set filename from options!
	int current = 0;
	if (BIOS_Settings.emulated_CPU==CPU_8086) //8086?
	{
		current = CPU_8086; //8086!
	}
	else if (BIOS_Settings.emulated_CPU == CPU_80286) //80286?
	{
		current = CPU_80286; //80286!
	}
	else //80186 (default)?
	{
		current = CPU_80186; //80186!
	}
	int file = ExecuteList(15,4,itemlist[current],256,NULL); //Show options for the installed CPU!
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
			reboot_needed = 1; //We need to reboot: a different CPU has been selected!
			switch (file) //Which CPU?
			{
			case CPU_8086: //8086?
				BIOS_Settings.emulated_CPU = CPU_8086; //Use the 8086!
				break;
			case CPU_80186: //80186?
				BIOS_Settings.emulated_CPU = CPU_80186; //Use the 80186!
				break;
			case CPU_80286: //80286?
				BIOS_Settings.emulated_CPU = CPU_80286; //Use the 80286!
				break;
			default: //Unknown CPU?
				BIOS_Settings.emulated_CPU = CPU_8086; //Use the 8086!
				break;
			}
		}
		break;
	}
	BIOS_Menu = 35; //Return to CPU menu!
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
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12: //Valid option?
		switch (optioninfo[menuresult]) //What option has been chosen, since we are dynamic size?
		{
		case 0: //Boot order (plain)?
			BIOS_Menu = 9; //Boot Order Menu!
			break;
		case 1: //Installed CPU?
			BIOS_Menu = 35; //Installed CPU Menu!
			break;
		case 2: //Debug mode?
			BIOS_Menu = 13; //Debug mode option!
			break;
		case 3: //Memory reallocation?
			BIOS_Menu = 14; //Memory reallocation!
			break;
		case 4: //VGA Settings setting?
			BIOS_Menu = 29; //VGA Settings setting!
			break;
		case 5: //BIOS Font?
			BIOS_Menu = 16; //BIOS Font setting!
			break;
		case 6: //Aspect ratio setting!
			BIOS_Menu = 17; //Aspect ratio setting!
			break;
		case 7:
			BIOS_Menu = 23; //Debugger log setting!
			break;
		case 8:
			BIOS_Menu = 24; //Execution mode option!
			break;
		case 9:
			BIOS_Menu = 25; //Input submenu!
			break;
		case 10:
			BIOS_Menu = 31; //MIDI Settings menu!
			break;
		case 11:
			BIOS_Menu = 34; //Mouse menu!
			break;
		case 12:
			BIOS_Menu = 37; //Clear CMOS menu!
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
	advancedoptions = 0; //No advanced options!
	if (BIOS_Changed) //Changed?
	{
		optioninfo[advancedoptions] = 0; //Reboot option!
		if (!reboot_needed) //Running?
		{
			strcpy(menuoptions[advancedoptions++], "Save Changes & Resume emulation"); //Option #0!
		}
		else
		{
			strcpy(menuoptions[advancedoptions++], "Save Changes & Reboot"); //Option #0!
		}
	}

	optioninfo[advancedoptions] = 1; //Discard option!
	if (!(reboot_needed==2)) //Able to continue running: Reboot is optional?
	{
		strcpy(menuoptions[advancedoptions++],"Discard Changes & Resume emulation"); //Option #1!
	}
	else
	{
		strcpy(menuoptions[advancedoptions++], "Discard Changes & Reboot"); //Option #1!
	}
	
	
	if (!EMU_RUNNING) //Emulator isn't running?
	{
		optioninfo[advancedoptions] = 2; //Load defaults option!
		strcpy(menuoptions[advancedoptions++],"Load BIOS defaults"); //Load defaults option!
	}

	int menuresult = BIOS_ShowMenu(advancedoptions,4,BIOSMENU_SPEC_LR,&Menu_Stat); //Plain menu, allow L&R triggers!

	switch (menuresult) //What option has been chosen?
	{
	case 0:
	case 1:
	case 2:
		switch (optioninfo[menuresult]) //What option is chosen?
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
			reboot_needed = 2; //We need a reboot!
			break;
		}
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

FILEPOS ImageGenerator_GetImageSize(byte x, byte y, int dynamichdd) //Retrieve the size, or 0 for none!
{
	int key = 0;
	key = psp_inputkeydelay(BIOS_INPUTDELAY);
	while ((key&BUTTON_CROSS)>0) //Pressed? Wait for release!
	{
		key = psp_inputkeydelay(BIOS_INPUTDELAY);
	}
	FILEPOS result = 0; //Size: result; default 0 for none! Must be a multiple of 4096 bytes for HDD!
	FILEPOS oldvalue; //To check for high overflow!
	for (;;) //Get input; break on error!
	{
		EMU_locktext();
		EMU_textcolor(BIOS_ATTR_ACTIVE); //We're using active color for input!
		GPU_EMU_printscreen(x, y, "%08i MB %04i KB", (uint_32)(result/MBMEMORY), (uint_32)((result%MBMEMORY)/1024)); //Show current size!
		EMU_unlocktext();
		key = psp_inputkeydelay(BIOS_INPUTDELAY); //Input key!
		//1GB steps!
		if ((key & BUTTON_LTRIGGER)>0)
		{
			if (result == 0) {}
			else
			{
				if (((int_64)(result - (1024*MBMEMORY))) <= 0)
				{
					result = 0;    //1GB steps!
				}
				else
				{
					result -= 1024*MBMEMORY;
				}
			}
		}
		else if ((key & BUTTON_RTRIGGER)>0)
		{
			oldvalue = result; //Save the old value!
			result += 1024*MBMEMORY; //Add 1GB!
			if (result < oldvalue) //We've overflown?
			{
				result = oldvalue; //Undo: we've overflown!
			}
		}
		//1MB steps!
		else if ((key & BUTTON_LEFT)>0)
		{
			if (result==0) { }
			else
			{
				if (((int_64)(result-MBMEMORY))<=0)
				{
					result = 0;    //1MB steps!
				}
				else
				{
					result -= MBMEMORY;
				}
			}
		}
		else if ((key & BUTTON_RIGHT)>0)
		{
			oldvalue = result; //Save the old value!
			result += MBMEMORY; //Add 1MB!
			if (result < oldvalue) //We've overflown?
			{
				result = oldvalue; //Undo: we've overflown!
			}
		}
		//4KB steps!
		else if ((key & BUTTON_DOWN)>0)
		{
			if (result==0) { }
			else
			{
				if (((int_64)(result - 4096)) <= 0)
				{
					result = 0;    //4KB steps!
				}
				else
				{
					result -= 4096;
				}
			}
		}
		else if ((key & BUTTON_UP)>0)
		{
			oldvalue = result; //Save the old value!
			result += 4096; //Add 4KB!
			if (result < oldvalue) //We've overflown?
			{
				result = oldvalue; //Undo: we've overflown!
			}
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
		else if (shuttingdown()) break; //Cancel because of shutdown?
	}
	return 0; //No size: cancel!
}

extern SDL_sem *keyboard_lock; //For keyboard input!
extern int input_buffer_shift; //Ctrl-Shift-Alt Status for the pressed key!
extern int input_buffer; //To contain the pressed key!

byte BIOS_InputText(byte x, byte y, char *filename, uint_32 maxlength)
{
	delay(100000); //Wait a bit!
	enableKeyboard(1); //Buffer input!
	char input[256];
	memset(&input, 0, sizeof(input)); //Init input to empty!
	for (;;) //Main input loop!
	{
		if (shuttingdown()) //Are we shutting down?
		{
			disableKeyboard(); //Disable the keyboard!
			return 0; //Cancel!
		}
		delay(0); //Wait a bit for input!
		updateKeyboard(); //Update the input keyboard, based on timing!
		WaitSem(keyboard_lock)
		if (input_buffer_shift != -1) //Given input yet?
		{
			if (EMU_keyboard_handler_idtoname(input_buffer,&input[0])) //Valid key?
			{
				if (!strcmp(input, "enter") || !strcmp(input,"esc")) //Enter or Escape? We're finished!
				{
					disableKeyboard(); //Disable the keyboard!
					PostSem(keyboard_lock) //We're done with input: release our lock!
					EMU_locktext();
					EMU_gotoxy(x, y); //Goto position for info!
					EMU_textcolor(BIOS_ATTR_TEXT);
					GPU_EMU_printscreen(x, y, "%s", filename); //Show the filename!
					EMU_textcolor(BIOS_ATTR_ACTIVE); //Active color!
					GPU_EMU_printscreen(-1, -1, " "); //Clear cursor indicator!
					EMU_unlocktext();
					return (!strcmp(input, "enter")); //Enter=Confirm, Esc=Cancel!
				}
				//We're a normal key hit?
				else if (!strcmp(input, "bksp") || (!strcmp(input,"z") && (input_buffer_shift&SHIFTSTATUS_CTRL))) //Backspace OR CTRL-Z?
				{
					if (strlen(filename)) //Gotten length?
					{
						filename[strlen(filename) - 1] = '\0'; //Make us one shorter!
					}
				}
				else if (!strcmp(input, "space")) //Space?
				{
					if (strlen(filename) < maxlength) //Not max?
					{
						strcat(filename, " "); //Add a space!
					}
				}
				else if (strlen(input) == 1) //Single character?
				{
					if ((input[0] != '`') &&
						(input[0] != '-') &&
						(input[0] != '=') &&
						(input[0] != '\\') &&
						(input[0] != '[') &&
						(input[0] != ']') &&
						(input[0] != ';') &&
						(strcmp(input,"'")!=0) &&
						(input[0] != ',') &&
						(input[0] != '/')) //Not an invalid character?
					{
						if (strlen(filename) < maxlength) //Not max?
						{
							if (input_buffer_shift&SHIFTSTATUS_SHIFT) //Shift pressed?
							{
								if ((input[0] >= 'a') && (input[0] <= 'z')) //Able to use shift on this key?
								{
									input[0] += ((int)'A' - (int)'a'); //Convert to uppercase!
									strcat(filename, input); //Add the input to the filename!
								}
								//Invalid uppercase is ignored!
							}
							else //Non-shift valid character?
							{
								strcat(filename, input); //Add the input to the filename!
							}
						}
					}
				}
				EMU_locktext();
				EMU_gotoxy(x, y); //Goto position for info!
				EMU_textcolor(BIOS_ATTR_TEXT);
				GPU_EMU_printscreen(x, y, "%s", filename); //Show the filename!
				EMU_textcolor(BIOS_ATTR_ACTIVE); //Active color!
				GPU_EMU_printscreen(-1, -1, "_"); //Cursor indicator!
				EMU_textcolor(BIOS_ATTR_TEXT); //Back to text!
				GPU_EMU_printscreen(-1, -1, " "); //Clear output after!
				EMU_unlocktext();
				input_buffer_shift = -1; //Reset!
				input_buffer = -1; //Nothing input!
				delay(100000); //Wait a bit!
			}
		}
		PostSem(keyboard_lock)
	}
}

void BIOS_GenerateStaticHDD() //Generate Static HDD Image!
{
	BIOS_Title("Generate Static HDD Image");
	char filename[256]; //Filename container!
	bzero(filename,sizeof(filename)); //Init!
	FILEPOS size = 0;
	BIOSClearScreen(); //Clear the screen!
	BIOS_Title("Generate Static HDD Image"); //Full clear!
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto position for info!
	GPU_EMU_printscreen(0, 4, "Name: "); //Show the filename!
	EMU_unlocktext();
	if (BIOS_InputText(6, 4, &filename[0], 255-4)) //Input text confirmed?
	{
		if (strcmp(filename, "") != 0) //Got input?
		{
			if (strlen(filename) <= (255 - 4)) //Not too long?
			{
				strcat(filename, ".img"); //Add the extension!
				EMU_locktext();
				EMU_gotoxy(0, 4); //Goto position for info!
				GPU_EMU_printscreen(0, 4, "Filename: %s", filename); //Show the filename!
				EMU_gotoxy(0, 5); //Next row!
				GPU_EMU_printscreen(0, 5, "Image size: "); //Show image size selector!!
				EMU_unlocktext();
				size = ImageGenerator_GetImageSize(12, 5, 0); //Get the size!
				if (size != 0) //Got size?
				{
					EMU_locktext();
					GPU_EMU_printscreen(12, 5, "%08i MB %04i KB", (uint_32)(size / MBMEMORY), (uint_32)((size % MBMEMORY) / 1024)); //Show size too!
					EMU_gotoxy(0, 6); //Next row!
					GPU_EMU_printscreen(0, 6, "Generating image: "); //Start of percentage!
					EMU_unlocktext();
					generateStaticImage(filename, size, 18, 6); //Generate a static image!
					if (!strcmp(filename, BIOS_Settings.hdd0) || !strcmp(filename, BIOS_Settings.hdd1)) //Harddisk changed?
					{
						BIOS_Changed = 1; //We've changed!
						reboot_needed = 2; //We're in need of a reboot!
					}
				}
			}
			//If we're too long, ignore it!
		}
	}
	BIOS_Menu = 1; //Return to Disk Menu!
}

//Interval to update!
#define SECTORUPDATEINTERVAL 51200000

void BIOS_GenerateDynamicHDD() //Generate Static HDD Image!
{
	BIOS_Title("Generate Dynamic HDD Image");
	char filename[256]; //Filename container!
	bzero(filename,sizeof(filename)); //Init!
	FILEPOS size = 0;
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto position for info!
	GPU_EMU_printscreen(0, 4, "Name: "); //Show the filename!
	EMU_unlocktext();
	if (BIOS_InputText(6, 4, &filename[0], 255-7)) //Input text confirmed?
	{
		if (strcmp(filename, "") != 0) //Got input?
		{
			if (strlen(filename) <= (255 - 7)) //Not too long?
			{
				strcat(filename, ".sfdimg"); //Add the extension!
				EMU_locktext();
				EMU_textcolor(BIOS_ATTR_TEXT);
				EMU_gotoxy(0, 4); //Goto position for info!
				GPU_EMU_printscreen(0, 4, "Filename: %s", filename); //Show the filename!
				EMU_gotoxy(0, 5); //Next row!
				GPU_EMU_printscreen(0, 5, "Image size: "); //Show image size selector!!
				EMU_unlocktext();
				size = ImageGenerator_GetImageSize(12, 5, 1); //Get the size!
				if (size != 0) //Got size?
				{
					EMU_locktext();
					GPU_EMU_printscreen(12, 5, "%08i MB %04i KB", (uint_32)(size / MBMEMORY), (uint_32)((size % MBMEMORY) / 1024)); //Show size too!
					EMU_gotoxy(0, 6); //Next row!
					GPU_EMU_printscreen(0, 6, "Generating image: "); //Start of percentage!
					EMU_unlocktext();
					FILEPOS sizecreated;
					sizecreated = generateDynamicImage(filename, size, 18, 6); //Generate a dynamic image!
					if (!strcmp(filename, BIOS_Settings.hdd0) || !strcmp(filename, BIOS_Settings.hdd1)) //Harddisk changed?
					{
						BIOS_Changed = 1; //We've changed!
						reboot_needed = 2; //We're in need of a reboot!
					}
				}
			}
			//If we're too long, ignore it!
		}
	}
	BIOS_Menu = 1; //Return to Disk Menu!
}

//How many sectors to be able to transfer at once?
#define VERIFICATIONBLOCK 500

byte sector[VERIFICATIONBLOCK * 512], verificationsector[VERIFICATIONBLOCK * 512]; //Current sector!

void BIOS_ConvertStaticDynamicHDD() //Generate Dynamic HDD Image from a static one!
{
	uint_64 datatotransfer; //How many sectors to transfer this block?
	uint_32 sectorposition = 0; //Possible position of error!
	char filename[256]; //Filename container!
	bzero(filename, sizeof(filename)); //Init!
	FILEPOS size = 0;
	BIOS_Title("Convert static to dynamic HDD Image"); //Full clear!
	generateFileList("img", 0, 0); //Generate file list for all .img files!
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Disk image: "); //Show selection init!
	EMU_unlocktext();
	int file = ExecuteList(12, 4, "", 256,&hdd_information); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
	case FILELIST_NOFILES: //No files?
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	default: //File?
		strcpy(filename, itemlist[file]); //Use this file!
		EMU_locktext();
		EMU_textcolor(BIOS_ATTR_TEXT);
		EMU_unlocktext();

		if (strcmp(filename, "") != 0) //Got input?
		{
			EMU_locktext();
			EMU_gotoxy(0, 4); //Goto position for info!
			GPU_EMU_printscreen(0, 4, "Filename: %s  ", filename); //Show the filename!
			EMU_gotoxy(0, 5); //Next row!
			GPU_EMU_printscreen(0, 5, "Image size: "); //Show image size selector!!
			EMU_unlocktext();
			iohdd0(filename, 0, 1, 0); //Mount the source disk!
			strcat(filename, ".sfdimg"); //Generate destination filename!
			size = getdisksize(HDD0); //Get the original size!
			if (size != 0) //Got size?
			{
				EMU_locktext();
				EMU_gotoxy(0, 6); //Next row!
				GPU_EMU_printscreen(0, 6, "Generating image: "); //Start of percentage!
				EMU_unlocktext();
				FILEPOS sizecreated;
				sizecreated = generateDynamicImage(filename, size, 18, 6); //Generate a dynamic image!
				if (sizecreated >= size) //Correct size?
				{
					if (!strcmp(filename, BIOS_Settings.hdd0) || !strcmp(filename, BIOS_Settings.hdd1)) //Harddisk changed?
					{
						BIOS_Changed = 1; //We've changed!
						reboot_needed = 2; //We're in need of a reboot!
					}
					EMU_locktext();
					GPU_EMU_printscreen(18, 6, "      "); //Clear the creation process!
					GPU_EMU_printscreen(12, 5, "      "); //Clear the creation process!
					GPU_EMU_printscreen(12, 5, "%iMB", (sizecreated / MBMEMORY)); //Image size
					EMU_unlocktext();
					iohdd1(filename, 0, 0, 0); //Mount the destination disk, allow writing!
					FILEPOS sectornr;
					EMU_locktext();
					EMU_gotoxy(0, 6); //Next row!
					GPU_EMU_printscreen(0, 6, "Generating image: "); //Start of percentage!
					EMU_unlocktext();
					byte error = 0;
					for (sectornr = 0; sectornr < sizecreated;) //Process all sectors!
					{
						if (shuttingdown())
						{
							error = 4; //Give the fourth error!
							break;
						}
						if ((sizecreated - sectornr) > sizeof(sector)) //Too much to handle?
						{
							datatotransfer = sizeof(sector); //Limit to max!
						}
						else
						{
							datatotransfer = sizecreated;
							datatotransfer -= sectornr; //How many bytes of data to transfer?
						}
						if (readdata(HDD0, &sector, sectornr, (uint_32)datatotransfer)) //Read a sector?
						{
							if (!writedata(HDD1, &sector, sectornr, (uint_32)datatotransfer)) //Error writing a sector?
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
						if (!(sectornr % SECTORUPDATEINTERVAL)) //Update every 10 sectors!
						{
							GPU_EMU_printscreen(18, 6, "%i%%", (int)(((float)sectornr / (float)sizecreated)*100.0f)); //Current progress!
						}
						sectornr += datatotransfer; //Next sector block!
					}
					EMU_locktext();
					GPU_EMU_printscreen(18, 6, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
					EMU_unlocktext();

					//Verification!
					if (!error) //OK?
					{
						EMU_locktext();
						GPU_EMU_printscreen(0, 7, "Validating image: "); //Start of percentage!
						EMU_unlocktext();
						iohdd1(filename, 0, 1, 0); //Mount!
						for (sectornr = 0; sectornr < size;) //Process all sectors!
						{
							if (shuttingdown())
							{
								error = 4; //Give the fourth error!
								break;
							}
							if ((sizecreated - sectornr) > sizeof(sector)) //Too much to handle?
							{
								datatotransfer = sizeof(sector); //Limit to max!
							}
							else
							{
								datatotransfer = sizecreated;
								datatotransfer -= sectornr; //How many bytes of data to transfer?
							}
							if (readdata(HDD0, &sector, sectornr, (uint_32)datatotransfer)) //Read a sector?
							{
								if (!readdata(HDD1, &verificationsector, sectornr, (uint_32)datatotransfer)) //Error reading a sector?
								{
									error = 2;
									break; //Stop reading!
								}
								else if ((sectorposition = memcmp(&sector, &verificationsector, (size_t)datatotransfer)) != 0)
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
							if (!(sectornr % SECTORUPDATEINTERVAL)) //Update every 10 sectors!
							{
								GPU_EMU_printscreen(18, 7, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
							}
							sectornr += datatotransfer; //Next sector!
						}
						EMU_locktext();
						GPU_EMU_printscreen(18, 6, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
						EMU_unlocktext();
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
	uint_64 datatotransfer;
	uint_32 sectorposition = 0; //Possible position of error!
	char filename[256]; //Filename container!
	bzero(filename, sizeof(filename)); //Init!
	FILEPOS size = 0;
	BIOS_Title("Convert dynamic to static HDD Image"); //Full clear!
	generateFileList("sfdimg", 0, 1); //Generate file list for all .img files!
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Disk image: "); //Show selection init!
	EMU_unlocktext();
	int file = ExecuteList(12, 4, "", 256,&hdd_information); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
	case FILELIST_NOFILES: //No files?
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	default: //File?
		strcpy(filename, itemlist[file]); //Use this file!
		EMU_locktext();
		EMU_textcolor(BIOS_ATTR_TEXT);
		EMU_unlocktext();

		if (strcmp(filename, "") != 0) //Got input?
		{
			EMU_locktext();
			EMU_gotoxy(0, 4); //Goto position for info!
			GPU_EMU_printscreen(0, 4, "Filename: %s  ", filename); //Show the filename!
			EMU_gotoxy(0, 5); //Next row!
			GPU_EMU_printscreen(0, 5, "Image size: "); //Show image size selector!!
			EMU_unlocktext();
			iohdd0(filename, 0, 1, 0); //Mount the source disk!
			strcat(filename, ".img"); //Generate destination filename!
			size = getdisksize(HDD0); //Get the original size!
			dolog("BIOS", "Dynamic disk size: %i bytes = %i sectors", size, (size >> 9));
			if (size != 0) //Got size?
			{
				if (!strcmp(filename, BIOS_Settings.hdd0) || !strcmp(filename, BIOS_Settings.hdd1)) //Harddisk changed?
				{
					BIOS_Changed = 1; //We've changed!
					reboot_needed = 2; //We're in need of a reboot!
				}
				EMU_locktext();
				EMU_gotoxy(0, 6); //Next row!
				GPU_EMU_printscreen(0, 6, "Generating image: "); //Start of percentage!
				GPU_EMU_printscreen(18, 6, "      "); //Clear the creation process!
				GPU_EMU_printscreen(12, 5, "      "); //Clear the creation process!
				GPU_EMU_printscreen(12, 5, "%iMB", (size / MBMEMORY)); //Image size
				FILEPOS sectornr;
				EMU_gotoxy(0, 6); //Next row!
				GPU_EMU_printscreen(0, 6, "Generating image: "); //Start of percentage!
				EMU_unlocktext();
				byte error = 0;
				FILE *dest;
				dest = emufopen64(filename, "wb"); //Open the destination!
				for (sectornr = 0; sectornr < size;) //Process all sectors!
				{
					if (shuttingdown())
					{
						error = 4; //Give the fourth error!
						break;
					}
					if ((size - sectornr) > sizeof(sector)) //Too much to handle?
					{
						datatotransfer = sizeof(sector); //Limit to max!
					}
					else //What's left?
					{
						datatotransfer = size;
						datatotransfer -= sectornr; //How many bytes of data to transfer?
					}
					if (readdata(HDD0, &sector, sectornr, (uint_32)datatotransfer)) //Read a sector?
					{
						if (emufwrite64(&sector,1,datatotransfer,dest)!=datatotransfer) //Error writing a sector?
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
					if (!(sectornr % SECTORUPDATEINTERVAL)) //Update every 10000 sectors!
					{
						GPU_EMU_printscreen(18, 6, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
					}
					sectornr += datatotransfer; //Next sector!
				}
				emufclose64(dest); //Close the file!

				EMU_locktext();
				GPU_EMU_printscreen(18, 6, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
				EMU_unlocktext();

				//Verification!
				if (!error) //OK?
				{
					EMU_locktext();
					GPU_EMU_printscreen(0, 7, "Validating image: "); //Start of percentage!
					EMU_unlocktext();
					iohdd1(filename, 0, 1, 0); //Mount!
					for (sectornr = 0; sectornr < size;) //Process all sectors!
					{
						if (shuttingdown())
						{
							error = 4; //Give the fourth error!
							break;
						}
						if ((size - sectornr) > sizeof(sector)) //Too much to handle?
						{
							datatotransfer = sizeof(sector); //Limit to max!
						}
						else
						{
							datatotransfer = size;
							datatotransfer -= sectornr; //How many bytes of data to transfer?
						}
						if (readdata(HDD0, &sector, sectornr, (uint_32)datatotransfer)) //Read a sector?
						{
							if (!readdata(HDD1, &verificationsector, sectornr, (uint_32)datatotransfer)) //Error reading a sector?
							{
								error = 2;
								break; //Stop reading!
							}
							else if ((sectorposition = memcmp(&sector,&verificationsector,(size_t)datatotransfer)) != 0)
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
						if (!(sectornr % SECTORUPDATEINTERVAL)) //Update every 10000 sectors!
						{
							GPU_EMU_printscreen(18, 7, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
						}
						sectornr += datatotransfer; //Next sector!
					}
					EMU_locktext();
					GPU_EMU_printscreen(18, 6, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
					EMU_unlocktext();
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
	uint_32 datatotransfer;
	uint_32 sectorposition = 0; //Possible position of error!
	char filename[256], originalfilename[256]; //Filename container!
	bzero(filename, sizeof(filename)); //Init!
	FILEPOS size = 0;
	BIOS_Title("Defragment a dynamic HDD Image"); //Full clear!
	generateFileList("sfdimg", 0, 1); //Generate file list for all .img files!
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Disk image: "); //Show selection init!
	EMU_unlocktext();
	int file = ExecuteList(12, 4, "", 256,&hdd_information); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
	case FILELIST_NOFILES: //No files?
	case FILELIST_CANCEL: //Cancelled?
		break;
	default: //File?
		strcpy(filename, itemlist[file]); //Use this file!
		EMU_locktext();
		EMU_textcolor(BIOS_ATTR_TEXT);
		EMU_unlocktext();

		if (strcmp(filename, "") != 0) //Got input?
		{
			EMU_locktext();
			EMU_gotoxy(0, 4); //Goto position for info!
			GPU_EMU_printscreen(0, 4, "Filename: %s ", filename); //Show the filename!
			EMU_gotoxy(0, 5); //Next row!
			GPU_EMU_printscreen(0, 5, "Image size: "); //Show image size selector!!
			EMU_unlocktext();
			iohdd0(filename, 0, 1, 0); //Mount the source disk!
			bzero(&originalfilename, sizeof(originalfilename)); //Init!
			strcpy(originalfilename, filename); //The original filename!
			strcat(filename, ".tmp.sfdimg"); //Generate destination filename!
			size = getdisksize(HDD0); //Get the original size!
			if (size != 0) //Got size?
			{
				EMU_locktext();
				EMU_gotoxy(0, 6); //Next row!
				GPU_EMU_printscreen(0, 6, "Defragmenting image: "); //Start of percentage!
				EMU_unlocktext();
				FILEPOS sizecreated;
				sizecreated = generateDynamicImage(filename, size, 21, 6); //Generate a dynamic image!
				if (sizecreated >= size) //Correct size?
				{
					EMU_locktext();
					GPU_EMU_printscreen(21, 6, "      "); //Clear the creation process!
					GPU_EMU_printscreen(12, 5, "      "); //Clear the creation process!
					GPU_EMU_printscreen(12, 5, "%iMB", (sizecreated / MBMEMORY)); //Image size
					EMU_unlocktext();
					iohdd1(filename, 0, 0, 0); //Mount the destination disk, allow writing!
					FILEPOS sectornr;
					byte error = 0;
					for (sectornr = 0; sectornr < sizecreated;) //Process all sectors!
					{
						if (shuttingdown())
						{
							error = 4; //Give the fourth error!
							break;
						}
						if ((sizecreated - sectornr) > sizeof(sector)) //Too much to handle?
						{
							datatotransfer = sizeof(sector); //Limit to max!
						}
						else
						{
							datatotransfer = (uint_32)(sizecreated - sectornr); //How many bytes of data to transfer?
						}
						if (readdata(HDD0, &sector, sectornr, datatotransfer)) //Read a sector?
						{
							if (!writedata(HDD1, &sector, sectornr, datatotransfer)) //Error writing a sector?
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
						if (!(sectornr % SECTORUPDATEINTERVAL)) //Update every 10000 sectors!
						{
							GPU_EMU_printscreen(21, 6, "%i%%", (int)(((float)sectornr / (float)sizecreated)*100.0f)); //Current progress!
						}
						sectornr += datatotransfer; //Next sector!
					}
					EMU_locktext();
					GPU_EMU_printscreen(21, 6, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
					EMU_unlocktext();

					//Verification!
					if (!error) //OK?
					{
						EMU_locktext();
						GPU_EMU_printscreen(0, 7, "Validating image: "); //Start of percentage!
						EMU_unlocktext();
						iohdd1(filename, 0, 1, 0); //Mount!
						for (sectornr = 0; sectornr < size;) //Process all sectors!
						{
							if (shuttingdown())
							{
								error = 4; //Give the fourth error!
								break;
							}
							if ((sizecreated - sectornr) > sizeof(sector)) //Too much to handle?
							{
								datatotransfer = sizeof(sector); //Limit to max!
							}
							else
							{
								datatotransfer = (uint_32)(sizecreated-sectornr); //How many bytes of data to transfer?
							}
							if (readdata(HDD0, &sector, sectornr, datatotransfer)) //Read a sector?
							{
								if (!readdata(HDD1, &verificationsector, sectornr, datatotransfer)) //Error reading a sector?
								{
									error = 2;
									break; //Stop reading!
								}
								else if ((sectorposition = memcmp(&sector, &verificationsector, datatotransfer)) != 0)
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
							if (!(sectornr % SECTORUPDATEINTERVAL)) //Update every 10000 sectors!
							{
								GPU_EMU_printscreen(18, 7, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
							}
							sectornr += datatotransfer; //Next sector!
						}
						EMU_locktext();
						GPU_EMU_printscreen(18, 6, "%i%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
						EMU_unlocktext();
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
	EMU_locktext();
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Debug mode: "); //Show selection init!
	EMU_unlocktext();
	int i = 0; //Counter!
	numlist = 4; //Ammount of Debug modes!
	for (i=0; i<numlist; i++) //Process options!
	{
		bzero(itemlist[i],sizeof(itemlist[i])); //Reset!
	}

	strcpy(itemlist[DEBUGMODE_NONE],"Disabled"); //Set filename from options!
	strcpy(itemlist[DEBUGMODE_RTRIGGER],"Enabled, RTrigger=Step"); //Set filename from options!
	strcpy(itemlist[DEBUGMODE_STEP],"Enabled, Step through"); //Set filename from options!
	strcpy(itemlist[DEBUGMODE_SHOW_RUN],"Enabled, just run, ignore shoulder buttons"); //Set filename from options!

	int current = 0;
	switch (BIOS_Settings.debugmode) //What debug mode?
	{
	case DEBUGMODE_NONE: //Valid
	case DEBUGMODE_RTRIGGER: //Valid
	case DEBUGMODE_STEP: //Valid
	case DEBUGMODE_SHOW_RUN: //Valid
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
	int file = ExecuteList(15,4,itemlist[current],256,NULL); //Show options for the installed CPU!
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

void BIOS_ExecutionMode()
{
	BIOS_Title("Execution mode");
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Execution mode: "); //Show selection init!
	EMU_unlocktext();
	int i = 0; //Counter!
	numlist = 6; //Ammount of Execution modes!
	for (i = 0; i<numlist; i++) //Process options!
	{
		bzero(itemlist[i], sizeof(itemlist[i])); //Reset!
	}

	strcpy(itemlist[EXECUTIONMODE_NONE], "Normal operations"); //Set filename from options!
	strcpy(itemlist[EXECUTIONMODE_TEST], "Run debug directory files, else TESTROM.DAT at 0000:0000"); //Set filename from options!
	strcpy(itemlist[EXECUTIONMODE_TESTROM], "Run TESTROM.DAT at 0000:0000"); //Set filename from options!
	strcpy(itemlist[EXECUTIONMODE_VIDEOCARD], "Debug video card output"); //Set filename from options!
	strcpy(itemlist[EXECUTIONMODE_BIOS], "Load BIOS from ROM directory as BIOSROM.u* and OPTROM.*"); //Set filename from options!
	strcpy(itemlist[EXECUTIONMODE_SOUND], "Run sound test"); //Debug sound test!

	int current = 0;
	switch (BIOS_Settings.executionmode) //What debug mode?
	{
	case EXECUTIONMODE_NONE: //Valid
	case EXECUTIONMODE_TEST: //Test files or biosrom.dat!
	case EXECUTIONMODE_TESTROM: //Test ROM?
	case EXECUTIONMODE_VIDEOCARD: //Text character debugging?
	case EXECUTIONMODE_BIOS: //External BIOS?
	case EXECUTIONMODE_SOUND: //Sound test?
		current = BIOS_Settings.executionmode; //Valid: use!
		break;
	default: //Invalid
		current = EXECUTIONMODE_NONE; //Default: none!
		break;
	}
	if (BIOS_Settings.executionmode != current) //Invalid?
	{
		BIOS_Settings.executionmode = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int file = ExecuteList(16, 4, itemlist[current], 256,NULL); //Show options for the installed CPU!
	switch (file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = EXECUTIONMODE_NONE; //Default execution mode: None!

	case EXECUTIONMODE_NONE:
	case EXECUTIONMODE_TEST:
	case EXECUTIONMODE_TESTROM:
	case EXECUTIONMODE_VIDEOCARD:
	case EXECUTIONMODE_BIOS:
	case EXECUTIONMODE_SOUND:
	default: //Changed?
		if (file != current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.executionmode = file; //Select Debug Mode!
			reboot_needed = EMU_RUNNING; //We need to reboot when running: our execution mode has been changed!
		}
		break;
	}
	BIOS_Menu = 8; //Goto Advanced menu!
}

void BIOS_DebugLog()
{
	BIOS_Title("Debugger log");
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Debugger log: "); //Show selection init!
	EMU_unlocktext();
	int i = 0; //Counter!
	numlist = 3; //Ammount of Execution modes!
	for (i = 0; i<numlist; i++) //Process options!
	{
		bzero(itemlist[i], sizeof(itemlist[i])); //Reset!
	}

	strcpy(itemlist[DEBUGGERLOG_NONE], "Don't log"); //Set filename from options!
	strcpy(itemlist[DEBUGGERLOG_DEBUGGING], "Only when debugging"); //Set filename from options!
	strcpy(itemlist[DEBUGGERLOG_ALWAYS], "Always log"); //Set filename from options!

	int current = 0;
	switch (BIOS_Settings.debugger_log) //What debug mode?
	{
	case DEBUGGERLOG_NONE: //None
	case DEBUGGERLOG_DEBUGGING: //Only when debugging
	case DEBUGGERLOG_ALWAYS: //Always
		current = BIOS_Settings.debugger_log; //Valid: use!
		break;
	default: //Invalid
		current = EXECUTIONMODE_NONE; //Default: none!
		break;
	}
	if (BIOS_Settings.debugger_log != current) //Invalid?
	{
		BIOS_Settings.debugger_log = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int file = ExecuteList(14, 4, itemlist[current], 256,NULL); //Show options for the installed CPU!
	switch (file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = DEBUGGERLOG_NONE; //Default execution mode: None!

	case DEBUGGERLOG_NONE: //None
	case DEBUGGERLOG_DEBUGGING: //Only when debugging
	case DEBUGGERLOG_ALWAYS: //Always
	default: //Changed?
		if (file != current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.debugger_log = file; //Select Debug Mode!
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
	reboot_needed = 2; //We need to reboot!
}

void BIOS_DirectPlotSetting()
{
	BIOS_Title("Direct plot");
	EMU_locktext();
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Direct plot: "); //Show selection init!
	EMU_unlocktext();
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
	int file = ExecuteList(15,4,itemlist[current],256,NULL); //Show options for the installed CPU!
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
	BIOS_Menu = 29; //Goto VGA Settings menu!
}

void BIOS_FontSetting()
{
	BIOS_Title("Font");
	EMU_locktext();
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"BIOS Font: "); //Show selection init!
	EMU_unlocktext();
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
	int font = ExecuteList(11,4,itemlist[current],256,NULL); //Show options for the installed CPU!
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

void BIOS_BWMonitor()
{
	BIOS_Title("Monitor");
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Monitor: "); //Show selection init!
	EMU_unlocktext();
	int i = 0; //Counter!
	numlist = 4; //Ammount of Execution modes!
	for (i = 0; i<numlist; i++) //Process options!
	{
		bzero(itemlist[i], sizeof(itemlist[i])); //Reset!
	}

	strcpy(itemlist[BWMONITOR_NONE], "Color"); //Set filename from options!
	strcpy(itemlist[BWMONITOR_BLACK], "B/W monitor: black"); //Set filename from options!
	strcpy(itemlist[BWMONITOR_GREEN], "B/W monitor: green"); //Set filename from options!
	strcpy(itemlist[BWMONITOR_BROWN], "B/W monitor: brown"); //Set filename from options!

	int current = 0;
	switch (BIOS_Settings.bwmonitor) //What debug mode?
	{
	case BWMONITOR_NONE: //None
	case BWMONITOR_BLACK: //Black/White
	case BWMONITOR_GREEN: //Greenscale
	case BWMONITOR_BROWN: //Brownscale
		current = BIOS_Settings.bwmonitor; //Valid: use!
		break;
	default: //Invalid
		current = BWMONITOR_NONE; //Default: none!
		break;
	}
	if (BIOS_Settings.bwmonitor != current) //Invalid?
	{
		BIOS_Settings.bwmonitor = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int file = ExecuteList(10, 4, itemlist[current], 256,NULL); //Show options for the installed CPU!
	switch (file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = DEBUGGERLOG_NONE; //Default execution mode: None!

	case BWMONITOR_NONE: //None
	case BWMONITOR_BLACK: //Black/White
	case BWMONITOR_GREEN: //Greenscale
	case BWMONITOR_BROWN: //Brownscale
	default: //Changed?
		if (file != current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.bwmonitor = file; //Select Debug Mode!
		}
		break;
	}
	BIOS_Menu = 29; //Goto VGA Settings menu!
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

void BIOS_InitInputText()
{
	advancedoptions = 0; //Init!
	int i;
	for (i = 0; i<10; i++) //Clear all possibilities!
	{
		bzero(menuoptions[i], sizeof(menuoptions[i])); //Init!
	}
	optioninfo[advancedoptions] = 0; //Gaming mode buttons!
	strcpy(menuoptions[advancedoptions++], "Map gaming mode buttons"); //Gaming mode buttons!
	optioninfo[advancedoptions] = 1; //Keyboard colors!
	strcpy(menuoptions[advancedoptions++], "Assign keyboard colors"); //Assign keyboard colors!
}

void BIOS_inputMenu() //Manage stuff concerning input.
{
	BIOS_Title("Input Menu");
	BIOS_InitInputText(); //Init text!
	int menuresult = BIOS_ShowMenu(advancedoptions, 4, BIOSMENU_SPEC_RETURN, &Menu_Stat); //Show the menu options!
	switch (menuresult)
	{
	case BIOSMENU_SPEC_CANCEL: //Return?
		BIOS_Menu = 8; //Goto Advanced Menu!
		break;
	case 0:
	case 1: //Valid option?
		switch (optioninfo[menuresult]) //What option has been chosen, since we are dynamic size?
		{
		case 0: //Gaming mode buttons?
			BIOS_Menu = 26; //Map gaming mode buttons Menu!
			break;
		case 1: //Keyboard colors?
			BIOS_Menu = 27; //Assign keyboard colors Menu!
			break;
		}
		break;
	default: //Unknown option?
		BIOS_Menu = NOTIMPLEMENTED; //Not implemented yet!
		break;
	}
}

void BIOS_addInputText(char *s, byte inputnumber)
{
	int input_key;
	int shiftstatus;
	char name[256]; //A little buffer for a name!
	if ((BIOS_Settings.input_settings.keyboard_gamemodemappings[inputnumber] != -1) || (BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[inputnumber] >0)) //Got anything?
	{
		shiftstatus = BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[inputnumber]; //Load shift status!
		input_key = BIOS_Settings.input_settings.keyboard_gamemodemappings[inputnumber]; //Load shift status!
		if (shiftstatus > 0) //Gotten alt status?
		{
			if (shiftstatus&SHIFTSTATUS_CTRL)
			{
				strcat(s, "Ctrl");
				if ((shiftstatus&SHIFTSTATUS_CTRL)!=shiftstatus) //More?
				{
					strcat(s, "-"); //Seperator!
				}
			}
			if (shiftstatus&SHIFTSTATUS_ALT)
			{
				strcat(s, "Alt");
				if ((shiftstatus&(SHIFTSTATUS_CTRL | SHIFTSTATUS_ALT)) != shiftstatus) //More?
				{
					strcat(s, "-"); //Seperator!
				}
			}
			if (shiftstatus&SHIFTSTATUS_SHIFT)
			{
				strcat(s, "Shift");
			}
			if (input_key != -1) //Gotten a key?
			{
				strcat(s, "-"); //Seperator!
			}
		}
		if (input_key != -1) //Gotten a key?
		{
			memset(name, 0, sizeof(name)); //Init name!
			if (EMU_keyboard_handler_idtoname(input_key, &name[0]))
			{
				strcat(s, name); //Add the name of the key!
			}
			else
			{
				strcat(s, "<Unidentified key>");
			}
		}
	}
	else
	{
		strcat(s, "<Unassigned>"); //Not assigned!
	}
}

void BIOS_InitGamingModeButtonsText()
{
	advancedoptions = 0; //Init!
	int i;
	for (i = 0; i<15; i++) //Clear all possibilities!
	{
		bzero(menuoptions[i], sizeof(menuoptions[i])); //Init!
	}
	optioninfo[advancedoptions] = 0; //START!
	strcpy(menuoptions[advancedoptions], "Start:        "); //Gaming mode buttons!
	BIOS_addInputText(&menuoptions[advancedoptions++][0], 0);
	optioninfo[advancedoptions] = 1; //LEFT!
	strcpy(menuoptions[advancedoptions], "Left:         "); //Assign keyboard colors!
	BIOS_addInputText(&menuoptions[advancedoptions++][0], 1);
	optioninfo[advancedoptions] = 2; //UP!
	strcpy(menuoptions[advancedoptions], "Up:           "); //Assign keyboard colors!
	BIOS_addInputText(&menuoptions[advancedoptions++][0], 2);
	optioninfo[advancedoptions] = 3; //RIGHT!
	strcpy(menuoptions[advancedoptions], "Right:        "); //Assign keyboard colors!
	BIOS_addInputText(&menuoptions[advancedoptions++][0], 3);
	optioninfo[advancedoptions] = 4; //DOWN!
	strcpy(menuoptions[advancedoptions], "Down:         "); //Assign keyboard colors!
	BIOS_addInputText(&menuoptions[advancedoptions++][0], 4);
	optioninfo[advancedoptions] = 5; //L!
	strcpy(menuoptions[advancedoptions], "L:            "); //Assign keyboard colors!
	BIOS_addInputText(&menuoptions[advancedoptions++][0], 5);
	optioninfo[advancedoptions] = 6; //R!
	strcpy(menuoptions[advancedoptions], "R:            "); //Assign keyboard colors!
	BIOS_addInputText(&menuoptions[advancedoptions++][0], 6);
	optioninfo[advancedoptions] = 7; //TRIANGLE!
	strcpy(menuoptions[advancedoptions], "Triangle:     "); //Assign keyboard colors!
	BIOS_addInputText(&menuoptions[advancedoptions++][0], 7);
	optioninfo[advancedoptions] = 8; //CIRCLE!
	strcpy(menuoptions[advancedoptions], "Circle:       "); //Assign keyboard colors!
	BIOS_addInputText(&menuoptions[advancedoptions++][0], 8);
	optioninfo[advancedoptions] = 9; //CROSS!
	strcpy(menuoptions[advancedoptions], "Cross:        "); //Assign keyboard colors!
	BIOS_addInputText(&menuoptions[advancedoptions++][0], 9);
	optioninfo[advancedoptions] = 10; //SQUARE!
	strcpy(menuoptions[advancedoptions], "Square:       "); //Assign keyboard colors!
	BIOS_addInputText(&menuoptions[advancedoptions++][0], 10);
	optioninfo[advancedoptions] = 11; //ANALOG LEFT!
	strcpy(menuoptions[advancedoptions], "Analog left:  "); //Assign keyboard colors!
	BIOS_addInputText(&menuoptions[advancedoptions++][0], 11);
	optioninfo[advancedoptions] = 12; //ANALOG UP!
	strcpy(menuoptions[advancedoptions], "Analog up:    "); //Assign keyboard colors!
	BIOS_addInputText(&menuoptions[advancedoptions++][0], 12);
	optioninfo[advancedoptions] = 13; //ANALOG RIGHT!
	strcpy(menuoptions[advancedoptions], "Analog right: "); //Assign keyboard colors!
	BIOS_addInputText(&menuoptions[advancedoptions++][0], 13);
	optioninfo[advancedoptions] = 14; //ANALOG DOWN!
	strcpy(menuoptions[advancedoptions], "Analog down:  "); //Assign keyboard colors!
	BIOS_addInputText(&menuoptions[advancedoptions++][0], 14);
}

void BIOS_gamingModeButtonsMenu() //Manage stuff concerning input.
{
	BIOS_Title("Map gaming mode buttons");
	BIOS_InitGamingModeButtonsText(); //Init text!
	int menuresult = BIOS_ShowMenu(advancedoptions, 4, BIOSMENU_SPEC_RETURN|BIOSMENU_SPEC_SQUAREOPTION, &Menu_Stat); //Show the menu options!
	switch (menuresult)
	{
	case BIOSMENU_SPEC_CANCEL: //Return?
		BIOS_Menu = 25; //Goto Input Menu!
		break;
	case 0: //START
	case 1: //LEFT
	case 2: //UP
	case 3: //RIGHT
	case 4: //DOWN
	case 5: //L
	case 6: //R
	case 7: //TRIANGLE
	case 8: //CIRCLE
	case 9: //CROSS
	case 10: //SQUARE
	case 11: //LEFT (analog)
	case 12: //UP (analog)
	case 13: //RIGHT (analog)
	case 14: //DOWN (analog)
		if (Menu_Stat == BIOSMENU_STAT_SQUARE) //Square pressed on an item?
		{
			BIOS_Changed |= ((BIOS_Settings.input_settings.keyboard_gamemodemappings[menuresult] != -1) || (BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[menuresult] != 0)); //Did we change?
			BIOS_Settings.input_settings.keyboard_gamemodemappings[menuresult] = -1; //Set the new key!
			BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[menuresult] = 0; //Set the shift status!
		}
		else //Normal option selected?
		{
			//Valid option?
			delay(100000); //Wait a bit!
			enableKeyboard(1); //Buffer input!
			for (;;)
			{
				updateKeyboard();
				WaitSem(keyboard_lock)
				if (input_buffer_shift != -1) //Given input yet?
				{
					disableKeyboard(); //Disable the keyboard!
					BIOS_Changed |= ((BIOS_Settings.input_settings.keyboard_gamemodemappings[menuresult] != input_buffer) || (BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[menuresult] != input_buffer_shift)); //Did we change?
					BIOS_Settings.input_settings.keyboard_gamemodemappings[menuresult] = input_buffer; //Set the new key!
					BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[menuresult] = input_buffer_shift; //Set the shift status!
					PostSem(keyboard_lock) //We're done with input: release our lock!
					break; //Break out of the loop: we're done!
				}
				PostSem(keyboard_lock)
			}
			//Keep in our own menu: we're not changing after a choise has been made, but simply allowing to select another button!
		}
		break;
	default: //Unknown option?
		BIOS_Menu = NOTIMPLEMENTED; //Not implemented yet!
		break;
	}
}

char colors[0x10][15] = { "Black", "Blue", "Green", "Cyan", "Red", "Magenta", "Brown", "Light gray", "Dark gray", "Bright blue", "Bright green", "Bright cyan", "Bright red", "Bright magenta", "Yellow", "White" }; //Set color from options!
void BIOS_addColorText(char *s, byte color)
{
	if (color < 0x10) //Valid color?
	{
		strcat(s, colors[color]); //Take the color!
	}
	else
	{
		strcat(s, "<UNKNOWN. CHECK BIOS VERSION>"); //Set color from options!
	}
}

void BIOS_InitKeyboardColorsText()
{
	advancedoptions = 0; //Init!
	int i;
	for (i = 0; i<5; i++) //Clear all possibilities!
	{
		bzero(menuoptions[i], sizeof(menuoptions[i])); //Init!
	}
	optioninfo[advancedoptions] = 0; //Gaming mode buttons!
	strcpy(menuoptions[advancedoptions], "Text font color: "); //Gaming mode buttons!
	BIOS_addColorText(&menuoptions[advancedoptions++][0], BIOS_Settings.input_settings.colors[0]); //First color!
	optioninfo[advancedoptions] = 1; //Keyboard colors!
	strcpy(menuoptions[advancedoptions], "Text border color: "); //Assign keyboard colors!
	BIOS_addColorText(&menuoptions[advancedoptions++][0], BIOS_Settings.input_settings.colors[1]); //First color!
	optioninfo[advancedoptions] = 2; //Keyboard colors!
	strcpy(menuoptions[advancedoptions], "Text active border color: "); //Assign keyboard colors!
	BIOS_addColorText(&menuoptions[advancedoptions++][0], BIOS_Settings.input_settings.colors[2]); //First color!
	optioninfo[advancedoptions] = 3; //Gaming mode buttons!
	strcpy(menuoptions[advancedoptions], "LED Font color: "); //Gaming mode buttons!
	BIOS_addColorText(&menuoptions[advancedoptions++][0], BIOS_Settings.input_settings.colors[3]); //First color!
	optioninfo[advancedoptions] = 4; //Keyboard colors!
	strcpy(menuoptions[advancedoptions], "LED border color: "); //Assign keyboard colors!
	BIOS_addColorText(&menuoptions[advancedoptions++][0], BIOS_Settings.input_settings.colors[4]); //First color!
	optioninfo[advancedoptions] = 5; //Keyboard colors!
	strcpy(menuoptions[advancedoptions], "LED active border color: "); //Assign keyboard colors!
	BIOS_addColorText(&menuoptions[advancedoptions++][0], BIOS_Settings.input_settings.colors[5]); //First color!
}

byte gamingKeyboardColor = 0;

void BIOS_gamingKeyboardColor() //Select a gaming keyboard color!
{
	switch (gamingKeyboardColor) //What option?
	{
	case 0:
		BIOS_Title("Text font color");
		break;
	case 1:
		BIOS_Title("Text border color");
		break;
	case 2:
		BIOS_Title("Text active border color");
		break;
	case 3:
		BIOS_Title("LED font color");
		break;
	case 4:
		BIOS_Title("LED border color");
		break;
	case 5:
		BIOS_Title("LED active border color");
		break;
	}
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Color: "); //Show selection init!
	EMU_unlocktext();
	int i = 0; //Counter!
	numlist = 16; //Ammount of colors!
	for (i = 0; i<numlist; i++) //Process options!
	{
		bzero(itemlist[i], sizeof(itemlist[i])); //Reset!
		strcpy(itemlist[i], &colors[i][0]); //Set the color to use!
	}

	int current = 0;
	switch (BIOS_Settings.input_settings.colors[gamingKeyboardColor]) //What debug mode?
	{
	case 0:case 1:case 2:case 3:case 4:case 5:case 6:case 7:case 8:case 9:case 0xA:case 0xB:case 0xC:case 0xD:case 0xE:case 0xF:
		current = BIOS_Settings.input_settings.colors[gamingKeyboardColor]; //Valid: use!
		break;
	default: //Invalid
		keyboard_loadDefaultColor(gamingKeyboardColor); //Default: none!
		current = BIOS_Settings.input_settings.colors[gamingKeyboardColor]; //Valid: use!
		break;
	}
	if (BIOS_Settings.input_settings.colors[gamingKeyboardColor] != current) //Invalid?
	{
		BIOS_Settings.input_settings.colors[gamingKeyboardColor] = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int file = ExecuteList(7, 4, itemlist[current], 256,NULL); //Show options for the installed CPU!
	switch (file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		keyboard_loadDefaultColor(gamingKeyboardColor); //Load the default value!
		file = BIOS_Settings.input_settings.colors[gamingKeyboardColor]; //Load the default value!

	default: //Changed?
		if (file != current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.input_settings.colors[gamingKeyboardColor] = file; //Select Debug Mode!
		}
		break;
	}
	BIOS_Menu = 27; //Goto Colors menu!
}
void BIOS_gamingKeyboardColorsMenu() //Manage stuff concerning input.
{
	BIOS_Title("Assign keyboard colors");
	BIOS_InitKeyboardColorsText(); //Init text!
	int menuresult = BIOS_ShowMenu(advancedoptions, 4, BIOSMENU_SPEC_RETURN, &Menu_Stat); //Show the menu options!
	switch (menuresult)
	{
	case BIOSMENU_SPEC_CANCEL: //Return?
		BIOS_Menu = 25; //Goto Input Menu!
		break;
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5: //Valid option?
		gamingKeyboardColor = optioninfo[menuresult]; //What option has been chosen, since we are dynamic size?
		BIOS_Menu = 28; //Switch to our option!
		break;
	default: //Unknown option?
		BIOS_Menu = NOTIMPLEMENTED; //Not implemented yet!
		break;
	}
}

void BIOS_VGANMISetting()
{
	BIOS_Title("VGA NMI");
	EMU_locktext();
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"VGA NMI: "); //Show selection init!
	EMU_unlocktext();
	int i = 0; //Counter!
	numlist = 2; //Ammount of Direct modes!
	for (i=0; i<3; i++) //Process options!
	{
		bzero(itemlist[i],sizeof(itemlist[i])); //Reset!
	}
	strcpy(itemlist[0],"Disabled"); //Set filename from options!
	strcpy(itemlist[1],"Enabled"); //Set filename from options!
	int current = 0;
	switch (BIOS_Settings.VGA_NMIonPrecursors) //What setting?
	{
	case 0: //Valid
	case 1: //Valid
		current = BIOS_Settings.VGA_NMIonPrecursors; //Valid: use!
		break;
	default: //Invalid
		current = 0; //Default: none!
		break;
	}
	if (BIOS_Settings.VGA_NMIonPrecursors!=current) //Invalid?
	{
		BIOS_Settings.VGA_NMIonPrecursors = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int file = ExecuteList(15,4,itemlist[current],256,NULL); //Show options for the installed CPU!
	switch (file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = 0; //Default setting: Disabled!

	case 0:
	case 1:
	default: //Changed?
		if (file!=current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.VGA_NMIonPrecursors = file; //Select NMI on Precursors setting!
		}
		break;
	}
	BIOS_Menu = 29; //Goto VGA Settings menu!
}

void BIOS_InitVGASettingsText()
{
	advancedoptions = 0; //Init!
	int i;
	for (i=0; i<3; i++) //Clear all possibilities!
	{
		bzero(menuoptions[i],sizeof(menuoptions[i])); //Init!
	}

	optioninfo[advancedoptions] = 0; //We're direct plot setting!
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

setmonitortext: //For fixing it!
	optioninfo[advancedoptions] = 1; //Monitor!
	strcpy(menuoptions[advancedoptions], "Monitor: ");
	switch (BIOS_Settings.bwmonitor) //B/W monitor?
	{
	case BWMONITOR_BLACK:
		strcat(menuoptions[advancedoptions++], "B/W monitor: black");
		break;
	case BWMONITOR_GREEN:
		strcat(menuoptions[advancedoptions++], "B/W monitor: green");
		break;
	case BWMONITOR_BROWN:
		strcat(menuoptions[advancedoptions++], "B/W monitor: brown");
		break;
	case BWMONITOR_NONE:
		strcat(menuoptions[advancedoptions++], "Color monitor");
		break;
	default: //Error: fix it!
		BIOS_Settings.bwmonitor = 0; //Reset/Fix!
		BIOS_Changed = 1; //We've changed!
		goto setmonitortext; //Goto!
		break;
	}

setVGANMItext: //For fixing it!
	optioninfo[advancedoptions] = 2; //VGA NMI!
	strcpy(menuoptions[advancedoptions], "VGA NMI: ");
	switch (BIOS_Settings.VGA_NMIonPrecursors) //VGA NMI?
	{
	case 0:
		strcat(menuoptions[advancedoptions++], "Disabled");
		break;
	case 1:
		strcat(menuoptions[advancedoptions++], "Enabled");
		break;
	default: //Error: fix it!
		BIOS_Settings.VGA_NMIonPrecursors = 0; //Reset/Fix!
		BIOS_Changed = 1; //We've changed!
		goto setVGANMItext; //Goto!
		break;
	}
}

void BIOS_VGASettingsMenu() //Manage stuff concerning input.
{
	BIOS_Title("VGA Settings Menu");
	BIOS_InitVGASettingsText(); //Init text!
	int menuresult = BIOS_ShowMenu(advancedoptions, 4, BIOSMENU_SPEC_RETURN, &Menu_Stat); //Show the menu options!
	switch (menuresult)
	{
	case BIOSMENU_SPEC_CANCEL: //Return?
		BIOS_Menu = 8; //Goto Advanced Menu!
		break;
	case 0:
	case 1:
	case 2: //Valid option?
		switch (optioninfo[menuresult]) //What option has been chosen, since we are dynamic size?
		{
		case 0: //Direct plot setting?
			BIOS_Menu = 15; //Direct plot setting!
			break;
		case 1: //Monitor?
			BIOS_Menu = 22; //Monitor setting!
			break;
		case 2: //VGA NMI?
			BIOS_Menu = 30; //VGA NMI setting!
		}
		break;
	default: //Unknown option?
		BIOS_Menu = NOTIMPLEMENTED; //Not implemented yet!
		break;
	}
}

void BIOS_InitMIDISettingsText()
{
	advancedoptions = 0; //Init!
	int i;
	for (i = 0; i<2; i++) //Clear all possibilities!
	{
		bzero(menuoptions[i], sizeof(menuoptions[i])); //Init!
	}

	optioninfo[advancedoptions] = 0; //We're direct plot setting!
	strcpy(menuoptions[advancedoptions], "MPU Soundfont: ");
	if (strcmp(BIOS_Settings.SoundFont, "") != 0)
	{
		strcat(menuoptions[advancedoptions++], BIOS_Settings.SoundFont); //The selected soundfont!
	}
	else
	{
		strcat(menuoptions[advancedoptions++], "<None>");
	}

	if (!EMU_RUNNING)
	{
		optioninfo[advancedoptions] = 1; //Monitor!
		strcpy(menuoptions[advancedoptions++], "MIDI Player");
	}
}

void BIOS_MIDISettingsMenu() //Manage stuff concerning input.
{
	BIOS_Title("MIDI Settings Menu");
	BIOS_InitMIDISettingsText(); //Init text!
	int menuresult = BIOS_ShowMenu(advancedoptions, 4, BIOSMENU_SPEC_RETURN, &Menu_Stat); //Show the menu options!
	switch (menuresult)
	{
	case BIOSMENU_SPEC_CANCEL: //Return?
		BIOS_Menu = 8; //Goto Advanced Menu!
		break;
	case 0:
	case 1:
	case 2: //Valid option?
		switch (optioninfo[menuresult]) //What option has been chosen, since we are dynamic size?
		{
		case 0: //Soundfont selection?
			BIOS_Menu = 32; //Direct plot setting!
			break;
		case 1: //Play MIDI file(s)?
			BIOS_Menu = 33; //Play MIDI file(s)!
			break;
		}
		break;
	default: //Unknown option?
		BIOS_Menu = NOTIMPLEMENTED; //Not implemented yet!
		break;
	}
}

void BIOS_SoundFont_selection() //SoundFont selection menu!
{
	BIOS_Title("Mount Soundfont");
	generateFileList("sf2", 0, 0); //Generate file list for all .sf2 files!
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Soundfont: "); //Show selection init!
	EMU_unlocktext();

	int file = ExecuteList(12, 4, BIOS_Settings.SoundFont, 256,NULL); //Show menu for the disk image!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Unmount?
	case FILELIST_NOFILES: //No files?
		if (strcmp(BIOS_Settings.SoundFont, ""))
		{
			BIOS_Changed = 1; //Changed!
			reboot_needed = 1; //We need to reboot!
			strcpy(BIOS_Settings.SoundFont, ""); //Unmount!
		}
		break;
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	default: //File?
		if (strcmp(BIOS_Settings.SoundFont, itemlist[file])) //Changed?
		{
			BIOS_Changed = 1; //Changed!
			reboot_needed = 1; //We need to reboot!
		}
		strcpy(BIOS_Settings.SoundFont, itemlist[file]); //Use this file!
		break;
	}
	BIOS_Menu = 31; //Return to the MIDI menu!
}

int MIDI_file = 0; //The file selected!

int BIOS_MIDI_selection() //MIDI selection menu, custom for this purpose!
{
	BIOS_Title("Select MIDI file to play");
	generateFileList("mid|midi", 0, 0); //Generate file list for all .img files!
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "MIDI file: "); //Show selection init!
	EMU_unlocktext();
	BIOS_EnablePlay = 1; //Enable Play=OK!
	int file = ExecuteList(12, 4, itemlist[MIDI_file], 256,NULL); //Show menu for the disk image!
	BIOS_EnablePlay = 0; //Disable play again!
	switch (file) //Which file?
	{
	case FILELIST_DEFAULT: //Execute default selection?
		return -2; //Give to our caller to handle!
		break;
	case FILELIST_CANCEL: //Cancelled?
		return -1; //Not selected!
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_NOFILES: //No files?
		return -1; //Not selected!
		break;
	default: //File?
		return file; //Use this file!
	}
	return -1; //Just in case!
}

byte sound_playMIDIfile(byte showinfo)
{
	MIDI_file = 0; //Init selected file!
	for (;;) //MIDI selection loop!
	{
		MIDI_file = BIOS_MIDI_selection(); //Allow the user to select a MIDI file!
		if (MIDI_file < 0) //Not selected?
		{
			MIDI_file = 0;
			if (MIDI_file == -2) //Default selected?
			{
				break; //Stop selection of the MIDI file!
			}
			else //Full cancel to execute?
			{
				return 0; //Allow our caller to execute the next step!
			}
		}
		EMU_locktext();
		EMU_textcolor(0x04); //Green on black!
		GPU_EMU_printscreen(0, GPU_TEXTSURFACE_HEIGHT - 1, "Playing..."); //Show playing init!
		EMU_unlocktext();
		//Play the MIDI file!
		playMIDIFile(&itemlist[MIDI_file][0], showinfo); //Play the MIDI file!
		EMU_locktext();
		GPU_EMU_printscreen(0, GPU_TEXTSURFACE_HEIGHT - 1, "          "); //Show playing finished!
		EMU_unlocktext();
	}
	return 1; //Plain finish: just execute whatever you want!
}

void BIOS_MIDIPlayer() //MIDI Player!
{
	sound_playMIDIfile(0); //Play one or more MIDI files! Don't show any information!
	BIOS_Menu = 31; //Return to the MIDI menu!
}

void BIOS_Mouse()
{
	BIOS_Title("Mouse");
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Mouse: "); //Show selection init!
	EMU_unlocktext();
	int i = 0; //Counter!
	numlist = 2; //Ammount of Direct modes!
	for (i = 0; i<3; i++) //Process options!
	{
		bzero(itemlist[i], sizeof(itemlist[i])); //Reset!
	}
	strcpy(itemlist[0], "Serial"); //Set filename from options!
	strcpy(itemlist[1], "PS/2"); //Set filename from options!
	int current = 0;
	switch (BIOS_Settings.VGA_NMIonPrecursors) //What setting?
	{
	case 0: //Valid
	case 1: //Valid
		current = BIOS_Settings.PS2Mouse; //Valid: use!
		break;
	default: //Invalid
		current = 0; //Default: none!
		break;
	}
	if (BIOS_Settings.PS2Mouse != current) //Invalid?
	{
		BIOS_Settings.PS2Mouse = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int file = ExecuteList(7, 4, itemlist[current], 256,NULL); //Show options for the installed CPU!
	switch (file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
						  //We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = 0; //Default setting: Disabled!

	case 0:
	case 1:
	default: //Changed?
		if (file != current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.PS2Mouse = file; //Select NMI on Precursors setting!
		}
		break;
	}
	BIOS_Menu = 8; //Goto Advanced menu!
}

void BIOS_InitCPUText()
{
	advancedoptions = 0; //Init!
	int i;
	for (i = 0; i<2; i++) //Clear all possibilities!
	{
		bzero(menuoptions[i], sizeof(menuoptions[i])); //Init!
	}

	optioninfo[advancedoptions] = 0; //Installed CPU!
	strcpy(menuoptions[advancedoptions], "Installed CPU: "); //Change installed CPU!
	switch (BIOS_Settings.emulated_CPU) //8086?
	{
	case CPU_8086: //8086?
		strcat(menuoptions[advancedoptions++], "Intel 8086"); //Add installed CPU!
		break;
	case CPU_80186: //80186?
		strcat(menuoptions[advancedoptions++], "Intel 80186"); //Add installed CPU!
		break;
	case CPU_80286: //80286?
		strcat(menuoptions[advancedoptions++], "Intel 80286(unfinished)"); //Add installed CPU!
		break;
	default:
		strcat(menuoptions[advancedoptions++], "<UNKNOWN. CHECK BIOS VERSION>"); //Add uninstalled CPU!
		break;
	}

	optioninfo[advancedoptions] = 1; //Change CPU speed!
	strcpy(menuoptions[advancedoptions], "CPU Speed: ");
	switch (BIOS_Settings.CPUSpeed)
	{
	case 0:
		strcat(menuoptions[advancedoptions++], "Unlimited"); //Unlimited!
		break;
	case 1:
		strcat(menuoptions[advancedoptions++], "Limited(4.77 performance)"); //Add uninstalled CPU!
		break;
	case 2:
		strcat(menuoptions[advancedoptions++], "Limited(4.77 general)"); //Add uninstalled CPU!
		break;
	default:
		strcat(menuoptions[advancedoptions++], "<UNKNOWN. CHECK BIOS VERSION>"); //Add uninstalled CPU!
		break;
	}
}

void BIOS_CPU() //CPU menu!
{
	BIOS_Title("CPU Menu");
	BIOS_InitCPUText(); //Init text!
	int menuresult = BIOS_ShowMenu(advancedoptions, 4, BIOSMENU_SPEC_RETURN, &Menu_Stat); //Show the menu options!
	switch (menuresult)
	{
	case BIOSMENU_SPEC_CANCEL: //R: Main menu?
		BIOS_Menu = 8; //Goto Advanced Menu!
		break;

	case 0:
	case 1: //Valid option?
		switch (optioninfo[menuresult]) //What option has been chosen, since we are dynamic size?
		{
		case 0: //Installed CPU?
			BIOS_Menu = 10; //Installed CPU selection!
			break;
		case 1: //CPU speed?
			BIOS_Menu = 36; //CPU speed selection!
			break;
		}
		break;
	default: //Unknown option?
		BIOS_Menu = NOTIMPLEMENTED; //Not implemented yet!
		break;
	}
}
void BIOS_CPUSpeed() //CPU speed selection!
{
	BIOS_Title("CPU speed");
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "CPU speed: "); //Show selection init!
	EMU_unlocktext();
	int i = 0; //Counter!
	numlist = 3; //Ammount of Direct modes!
	for (i = 0; i<3; i++) //Process options!
	{
		bzero(itemlist[i], sizeof(itemlist[i])); //Reset!
	}
	strcpy(itemlist[0], "Unlimited"); //Set filename from options!
	strcpy(itemlist[1], "Limited(4.77 performance)"); //Set filename from options!
	strcpy(itemlist[2], "Limited(4.77 general)"); //Set filename from options!

	int current = 0;
	switch (BIOS_Settings.CPUSpeed) //What setting?
	{
	case 0: //Valid
	case 1: //Valid
	case 2: //Valid
		current = BIOS_Settings.CPUSpeed; //Valid: use!
		break;
	default: //Invalid
		current = 0; //Default: none!
		break;
	}
	if (BIOS_Settings.CPUSpeed != current) //Invalid?
	{
		BIOS_Settings.CPUSpeed = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int file = ExecuteList(11, 4, itemlist[current], 256,NULL); //Show options for the installed CPU!
	switch (file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
						  //We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = 0; //Default setting: Disabled!

	case 0:
	case 1:
	default: //Changed?
		if (file != current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.CPUSpeed = file; //Select CPU speed setting!
		}
		break;
	}
	BIOS_Menu = 35; //Goto CPU menu!
}

void BIOS_ClearCMOS() //Clear the CMOS!
{
	byte emptycmos[128];
	memset(&emptycmos, 0, sizeof(emptycmos)); //Empty CMOS for comparision!
	if ((BIOS_Settings.got_CMOS) || (memcmp(&BIOS_Settings.CMOS, emptycmos,sizeof(emptycmos)) != 0)) //Gotten a CMOS?
	{
		memset(BIOS_Settings.CMOS, 0, sizeof(BIOS_Settings.CMOS));
		BIOS_Settings.got_CMOS = 0; //We haven't gotten a CMOS!
		BIOS_Changed = 1; //We've changed!
		reboot_needed = 2; //We're needing a reboot!
	}
	BIOS_Menu = 8; //Goto Advanced Menu!
}