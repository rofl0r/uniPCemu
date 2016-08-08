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
#include "headers/hardware/ssource.h" //Sound Source volume knob support!
#include "headers/emu/gpu/gpu_framerate.h" //Framerate support!
#include "headers/support/highrestimer.h" //High resolution clock support!
#include "headers/emu/sound.h" //Recording support!
#include "headers/hardware/floppy.h" //Floppy disk support!
#include "headers/hardware/vga/vga_dacrenderer.h" //Renderer logging support of DAC colors!
#include "headers/hardware/vga/vga_vramtext.h" //VRAM font table logging support!
#include "headers/hardware/vga/vga_cga_mda.h" //CGA/MDA dumping support!
#include "headers/support/dro.h" //DRO file support!
#include "headers/support/bmp.h" //For dumping our full VGA RAM!
#include "headers/hardware/gameblaster.h" //Gameblaster volume knob support!

//Define below to enable the sound test with recording!
//#define SOUND_TEST

//Dump a 256-color 640x480 VRAM layout to a bitmap file!
//#define DUMP_VGATEST256COL

#ifdef SOUND_TEST
#include "headers/hardware/ports.h" //I/O support!
#endif

#define __HW_DISABLED 0

//Force the BIOS to open?
#define FORCE_BIOS 0

//BIOS width in text mode!
#define BIOS_WIDTH GPU_TEXTSURFACE_WIDTH

//Boot time in 2 seconds!
#define BOOTTIME 2000000

extern char diskpath[6]; //The full disk path used!

char soundfontpath[11] = "soundfonts";
char musicpath[6] = "music"; //Music directory containing all music!

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
extern byte exec_showchecksumerrors; //Show checksum errors?
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
void BIOS_AspectRatio(); //Keep aspect ratio?
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
void BIOS_VideoSettingsMenu(); //Manage stuff concerning video output.
void BIOS_VGAModeSetting(); //VGA Mode setting!
void BIOS_SoundMenu(); //Manage stuff concerning Sound.
void BIOS_SoundFont_selection(); //FLOPPY0 selection menu!
void BIOS_MusicPlayer(); //Music player!
void BIOS_Mouse(); //Mouse selection menu!
void BIOS_CPU(); //CPU menu!
void BIOS_CPUSpeed(); //CPU speed selection!
void BIOS_ClearCMOS(); //Clear the CMOS!
void BIOS_SoundSourceVolume(); //Set the Sound Source volume!
void BIOS_ShowFramerate(); //Show framerate setting!
void BIOS_DataBusSizeSetting(); //Data bus size setting!
void BIOS_ShowCPUSpeed(); //Show CPU speed setting!
void BIOS_SoundStartStopRecording(); //Start/stop recording sound!
void BIOS_GenerateFloppyDisk(); //Generate an floppy disk image!
void BIOS_usePCSpeaker();
void BIOS_useAdlib();
void BIOS_useLPTDAC();
void BIOS_VGASynchronization();
void BIOS_DumpVGA();
void BIOS_CGAModel();
void BIOS_gamingmodeJoystick(); //Use joystick instead of normal gaming mode?
void BIOS_JoystickReconnect(); //Reconnect the joystick (not SDL2)
void BIOS_useGameBlaster();
void BIOS_GameBlasterVolume();
void BIOS_useSoundBlaster();

//First, global handler!
Handler BIOS_Menus[] =
{
	BIOS_MainMenu //The main menu is #0!
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
	,BIOS_AspectRatio //Aspect ratio setting is #17!
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
	,BIOS_VideoSettingsMenu //Manage stuff concerning Video Settings is #29!
	,BIOS_VGAModeSetting //VGA Mode setting is #30!
	,BIOS_SoundMenu //Sound settings menu is #31!
	,BIOS_SoundFont_selection //Soundfont selection menu is #32!
	,BIOS_MusicPlayer //Music Player is #33!
	,BIOS_Mouse //Mouse menu is #34!
	,BIOS_CPU //BIOS CPU menu is #35!
	,BIOS_CPUSpeed //BIOS CPU speed is #36!
	,BIOS_ClearCMOS //BIOS CMOS clear is #37!
	,BIOS_SoundSourceVolume //Sound Source Volume is #38!
	,BIOS_ShowFramerate //Show Framerate is #39!
	,BIOS_DataBusSizeSetting //Data Bus size setting is #40!
	,BIOS_ShowCPUSpeed //Show CPU speed is #41!
	,BIOS_SoundStartStopRecording //Start/stop recording sound is #42!
	,BIOS_GenerateFloppyDisk //Generate a floppy disk is #43!
	,BIOS_usePCSpeaker //Use PC Speaker is #44!
	,BIOS_useAdlib //Use Adlib is #45!
	,BIOS_useLPTDAC //Use LPT DAC is #46!
	,BIOS_VGASynchronization //Change VGA Synchronization setting is #47!
	,BIOS_DumpVGA //Dump the VGA fully is #48!
	,BIOS_CGAModel //Select the CGA Model is #49!
	,BIOS_gamingmodeJoystick //Use Joystick is #50!
	,BIOS_JoystickReconnect //Reconnect Joystick is #51!
	,BIOS_useGameBlaster //Use Game Blaster is #52!
	,BIOS_GameBlasterVolume //Game Blaster Volume is #53!
	,BIOS_useSoundBlaster //Use Sound Blaster is #54!
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

byte BIOS_printopentext(uint_32 timeout)
{
	byte result=SETXYCLICKED_OK;
	if (timeout) //Specified? We're before boot!
	{
		GPU_text_locksurface(BIOS_Surface);
		GPU_textgotoxy(BIOS_Surface,0,0); //Goto our location!
		result = GPU_textprintfclickable(BIOS_Surface,getemucol16(0xE),getemucol16(0x0), "Press SELECT to bring out the Settings menu");
		GPU_text_releasesurface(BIOS_Surface);
	}
	return result; //Give the result!
}

byte bootBIOS = 0; //Boot into BIOS?

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
	
	exec_showchecksumerrors = 0; //Don't show!
	BIOS_LoadData(); //Now load/reset the BIOS
	exec_showchecksumerrors = 1; //Reset!

	if (!timeout) //Normal opening the BIOS?
	{
		EMU_locktext();
		printmsg(0xE, "Press SELECT to run BIOS SETUP");
		EMU_unlocktext();
	}
	
	byte BIOSClicked = 0;
	while (counter>0) //Time left?
	{
		BIOSClicked = (BIOS_printopentext(timeout)&SETXYCLICKED_CLICKED); //Are we clicked?
		counter -= INPUT_INTERVAL; //One further!
		delay(INPUT_INTERVAL); //Intervals of one!
		if (shuttingdown()) //Request shutdown?
		{
			return 0; //No reset!
		}
		if ((psp_inputkey() & BUTTON_SELECT) || BIOS_Settings.firstrun || bootBIOS || FORCE_BIOS || BIOSClicked) //R trigger pressed or first run? Also when clicked!
		{
			bootBIOS = 0; //Not booting into BIOS anymore!
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
		GPU_EMU_printscreen(0,0,"                                           "); //Clear our text!
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
	byte oldVGAMode;
	if (__HW_DISABLED) return 0; //Abort!
	EMU_stopInput(); //Stop all emu input!
	terminateVGA(); //Terminate currently running VGA for a speed up!
	//dolog("BIOS","Running BIOS...");
	exec_showchecksumerrors = 0; //Not showing any checksum errors!

//Now reset/save all we need to run the BIOS!
	GPU.show_framerate = 0; //Hide the framerate surface!	
	
//Now do the BIOS stuff!
	if (showloadingtext) //Not in emulator?
	{
		EMU_textcolor(0xF);
		printmsg(0xF,"\r\nLoading Settings...");
		delay(500000); //0.5 sec!
	}

	stopEMUTimers(); //Stop our timers!
	
	GPU_text_locksurface(frameratesurface);
	GPU_textclearscreen(frameratesurface); //Make sure the surface is empty for a neat BIOS!
	GPU_text_releasesurface(frameratesurface);
	
	BIOS_LoadData(); //Now load/reset the BIOS
	BIOS_Changed = 0; //Default: the BIOS hasn't been changed!
	BIOS_SaveStat = 0; //Default: not saving!
	exec_showchecksumerrors = 0; //Default: not showing checksum errors!
	BIOS_clearscreen(); //Clear the screen!
	BIOS_Menu = 0; //We're opening the main menu!

	oldVGAMode = BIOS_Settings.VGA_Mode; //Our old VGA mode!

	reboot_needed = 0; //Do we need to reboot?
	BIOS_MenuChooser(); //Show the BIOS's menu we've selected!
	
	if (BIOS_Settings.firstrun) //First run?
	{
		BIOS_Settings.firstrun = 0; //Not the first run anymore!
		forceBIOSSave(); //Save: we're not running again!
	}
	
	if (BIOS_SaveStat && BIOS_Changed) //To save the BIOS and BIOS has been changed?
	{
		if (!BIOS_SaveData()) //Save our options and failed?
		{
			EMU_locktext();
			BIOS_clearscreen(); //Clear the screen!
			EMU_gotoxy(0,0); //First column,row!
			EMU_textcolor(0xF);
			GPU_EMU_printscreen(0,0,"Error: couldn't save the settings!");
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
				GPU_EMU_printscreen(0,0,"Settings Saved!");
				EMU_unlocktext();
				delay(2000000); //Wait 2 sec before rebooting!
			}
			else //Emulator running?
			{
				EMU_locktext();
				EMU_gotoxy(0,0); //First column,row!
				EMU_textcolor(0xF);
				GPU_EMU_printscreen(0,0,"Settings Saved (Returning to the emulator)!"); //Info!
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
		GPU_EMU_printscreen(0,0,"Settings Discarded!"); //Info!
		EMU_unlocktext();
		BIOS_LoadData(); //Reload!
		delay(2000000); //Wait 2 sec!
	}

	BIOSDoneScreen(); //Clean up the screen!
//Now return to the emulator to reboot!
	BIOS_ValidateData(); //Validate&reload all disks!

//Restore all states saved for the BIOS!
	startEMUTimers(); //Start our timers up again!
	if (shuttingdown()) return 0; //We're shutting down, discard!
	lock(LOCK_MAINTHREAD); //Lock the main thread!
	startVGA(); //Start the VGA up again!
	EMU_startInput(); //Start all emu input again!

	EMU_update_VGA_Settings(); //Update the VGA Settings to it's default value!
	if (BIOS_Settings.VGA_Mode!=oldVGAMode) //Mode changed?
	{
		VGA_initIO(); //Initialise/update the VGA if needed!
	}
	ssource_setVolume((float)BIOS_Settings.SoundSource_Volume); //Set the current volume!
	GameBlaster_setVolume((float)BIOS_Settings.GameBlaster_Volume); //Set the current volume!
	GPU_AspectRatio(BIOS_Settings.aspectratio); //Keep the aspect ratio?
	setGPUFramerate(BIOS_Settings.ShowFramerate); //Show the framerate?
	unlock(LOCK_MAINTHREAD); //Continue!

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
	char BIOSText[] = "UniPCemu Settings"; //The BIOS's text!
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
		if (BIOS_Menu>=0 && BIOS_Menu<(sword)(NUMITEMS(BIOS_Menus))) //Within range of menus?
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
	BIOSClearScreen(); //Clear our screen first!
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

byte BIOS_printscreen(word x, word y, byte attr, char *text, ...)
{
	char buffer[256]; //Going to contain our output data!
	va_list args; //Going to contain the list!
	va_start(args, text); //Start list!
	vsprintf(buffer, text, args); //Compile list!

	//Now display and return!
	GPU_textgotoxy(BIOS_Surface,x,y); //Goto coordinates!
	return GPU_textprintfclickable(BIOS_Surface,getemucol16(attr&0xF),getemucol16((attr>>4)&0xF),buffer); //Give the contents!
}

extern byte GPU_surfaceclicked; //Surface clicked to handle?

int BIOS_ShowMenu(int numitems, int startrow, int allowspecs, word *stat)
{
	*stat = BIOSMENU_STAT_OK; //Plain status for default!
	int key = 0; //Currently pressed key(s)
	int option = 0; //What option to choose?
	byte dirty = 1; //We're dirty! We need to be updated on the screen!
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
				dirty = 1; //We're dirty!
			}
			else //Top option?
			{
				if (numitems>1) //More than one item?
				{
					option = numitems-1; //Goto bottom item!
					dirty = 1; //We're dirty!
				}
			}
		}
		else if ((key & BUTTON_DOWN)>0) //Down pressed?
		{
			if (option<(numitems-1)) //Not last item?
			{
				option++; //Next option!
				dirty = 1; //We're dirty!
			}
			else if (numitems>1) //Last item?
			{
				option = 0; //Goto first item from bottom!
				dirty = 1; //We're dirty!
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

		lock(LOCK_INPUT);
		if ((GPU_surfaceclicked&0x81)==1) //We're signalled?
		{
			dirty = 1; //We're dirty!
			GPU_surfaceclicked |= 0x80; //We acnowledge this action!
		}
		unlock(LOCK_INPUT);

//Now that the options have been chosen, show them:

		if (dirty) //Do we need to update the screen and check for input?
		{
			dirty = 0; //Acnowledge being acted upon!
			int cur = 0; //Current option
			char selected[2][256] = { "  %s","> %s" }; //Selector!
			char *selector;
			byte selectorattribute;
			EMU_locktext();
			cur = 0; //Initilialize current item!
			do //Process all options!
			{
				selector = selected[(cur == option) ? 1 : 0]; //The current selector to use!
				selectorattribute = (cur == option) ?BIOS_ATTR_ACTIVE:BIOS_ATTR_INACTIVE; //Active/inactive selector!
				if (BIOS_printscreen(0,startrow+cur,selectorattribute,selector,menuoptions[cur])&SETXYCLICKED_CLICKED) //Clicked?
				{
					EMU_unlocktext();
					return cur; //This item has been chosen!
				}
			} while (++cur<numitems);
			EMU_unlocktext();
		}
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
void generateFileList(char *path, char *extensions, int allowms0, int allowdynamic)
{
	numlist = 0; //Reset ammount of files!
	clearList(); //Clear the list!
	if (allowms0) //Allow Memory Stick option?
	{
        #ifdef IS_PSP
               addList("ms0:"); //Add filename (Memory Stick)!
        #endif
	}
	char direntry[256];
	byte isfile;
	DirListContainer_t dir;
	if (opendirlist(&dir,path,&direntry[0],&isfile))
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
	if (safe_strlen(str1,maxlen)!=safe_strlen(str2,maxlen)) //Not equal in length?
	{
		return 0; //Not equal in length!
	}
	int length = safe_strlen(str1,maxlen); //Length!
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
	if (max>(int)(sizeof(buffer)-1)) //Too much?
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
	if (max2>(int)(sizeof(filler)-1)) //Limit breached?
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
	char path[256];
	memset(&path,0,sizeof(path));
	strcpy(path,diskpath);
	strcat(path,"/");
	strcat(path,filename);
	FILEPOS size;
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	if (is_dynamicimage(path)) //Dynamic image?
	{
		size = dynamicimage_getsize(path); //Get the filesize!
		GPU_EMU_printscreen(0, 6, "This is a Superfury Dynamic Disk Image file."); //Show selection init!
		GPU_EMU_printscreen(0, 7, "Disk size: %08i MB %04i KB", (uint_32)(size / MBMEMORY), (uint_32)((size % MBMEMORY) / 1024)); //Show size too!
	}
	else if (is_DSKimage(path)) //DSK disk image?
	{
		DISKINFORMATIONBLOCK dskinfo;
		if (!readDSKInfo(path, &dskinfo)) goto unknownimage;
		size = dskinfo.NumberOfSides*dskinfo.NumberOfTracks*dskinfo.TrackSize; //Get the total disk image size!
		size = dynamicimage_getsize(path); //Get the filesize!
		GPU_EMU_printscreen(0, 6, "This is a DSK disk image file.              "); //Show selection init!
		GPU_EMU_printscreen(0, 7, "Disk size: %08i MB %04i KB", (uint_32)(size / MBMEMORY), (uint_32)((size % MBMEMORY) / 1024)); //Show size too!
	}
	else if (is_staticimage(path)) //Static image?
	{
		size = staticimage_getsize(path); //Get the filesize!
		GPU_EMU_printscreen(0, 6, "This is a Static disk image file.           "); //Show selection init!
		GPU_EMU_printscreen(0, 7, "Disk size: %08i MB %04i KB", (uint_32)(size / MBMEMORY), (uint_32)((size % MBMEMORY) / 1024)); //Show size too!
	}
	else //Unknown file type: no information?
	{
	unknownimage: //Unknown disk image?
		GPU_EMU_printscreen(0, 6, "This is an unknown disk image file.         "); //Show selection init!
		GPU_EMU_printscreen(0, 7, "                              "); //Clear file size info!
	}
}

//Menus itself:

//Selection menus for disk drives!

void BIOS_floppy0_selection() //FLOPPY0 selection menu!
{
	BIOS_Title("Mount FLOPPY A");
	generateFileList(diskpath,"img|ima|dsk",0,0); //Generate file list for all .img files!
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
	generateFileList(diskpath,"img|ima|dsk",0,0); //Generate file list for all .img files!
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
	generateFileList(diskpath,"img|sfdimg",1,1); //Generate file list for all .img files!
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
	generateFileList(diskpath,"img|sfdimg",1,1); //Generate file list for all .img files!
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
	generateFileList(diskpath,"iso",0,0); //Generate file list for all .img files!
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
	generateFileList(diskpath,"iso",0,0); //Generate file list for all .img files!
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
	for (i=0; i<12; i++)
	{
		bzero(menuoptions[i],sizeof(menuoptions[i])); //Init!
	}
	strcpy(menuoptions[0],"Floppy A: ");
	strcpy(menuoptions[1],"Floppy B: ");
	strcpy(menuoptions[2],"First HDD: ");
	strcpy(menuoptions[3],"Second HDD: ");
	strcpy(menuoptions[4],"First CD-ROM: ");
	strcpy(menuoptions[5],"Second CD-ROM: ");
	strcpy(menuoptions[6],"Generate Floppy Image");
	strcpy(menuoptions[7],"Generate Static HDD Image");
	strcpy(menuoptions[8],"Generate Dynamic HDD Image");
	strcpy(menuoptions[9], "Convert static to dynamic HDD Image");
	strcpy(menuoptions[10], "Convert dynamic to static HDD Image");
	strcpy(menuoptions[11], "Defragment a dynamic HDD Image");

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
	int menuresult = BIOS_ShowMenu(12,4,BIOSMENU_SPEC_LR|BIOSMENU_SPEC_SQUAREOPTION,&Menu_Stat); //Show the menu options, allow SQUARE!
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
	case 6: //Generate Floppy Image?
		if (Menu_Stat==BIOSMENU_STAT_OK) //Plain status?
		{
			BIOS_Menu = 43; //Generate Floppy Image!
		}
		break;
	case 7: //Generate Static HDD?
		if (Menu_Stat==BIOSMENU_STAT_OK) //Plain status?
		{
			BIOS_Menu = 11; //Generate Static HDD!
		}
		break;
	case 8: //Generate Dynamic HDD?
		if (Menu_Stat==BIOSMENU_STAT_OK) //Plain status?
		{
			BIOS_Menu = 12; //Generate Dynamic HDD!
		}
		break;
	case 9: //Convert static to dynamic HDD?
		if (Menu_Stat == BIOSMENU_STAT_OK) //Plain status?
		{
			BIOS_Menu = 19; //Convert static to dynamic HDD!
		}
		break;
	case 10: //Convert dynamic to static HDD?
		if (Menu_Stat == BIOSMENU_STAT_OK) //Plain status?
		{
			BIOS_Menu = 20; //Convert dynamic to static HDD!
		}
		break;
	case 11: //Defragment a dynamic HDD Image?
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
	BIOS_Menu = 35; //Return to CPU menu!
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
	strcpy(itemlist[CPU_8086],"Intel 8086/8088"); //Set filename from options!
	strcpy(itemlist[CPU_NECV30],"NEC V20/V30"); //Set filename from options!
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
	else //NEC V20/V30 (default)?
	{
		current = CPU_NECV30; //NEC V20/V30!
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
			case CPU_NECV30: //NEC V20/V30?
				BIOS_Settings.emulated_CPU = CPU_NECV30; //Use the NEC V20/V30!
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

void BIOS_InitAdvancedText()
{
	advancedoptions = 0; //Init!
	int i;
	for (i = 0; i<12; i++) //Clear all possibilities!
	{
		bzero(menuoptions[i], sizeof(menuoptions[i])); //Init!
	}

	optioninfo[advancedoptions] = 0; //CPU menu!
	strcpy(menuoptions[advancedoptions++], "CPU Settings"); //Change installed CPU options!

	optioninfo[advancedoptions] = 1; //Video Settings
	strcpy(menuoptions[advancedoptions++], "Video Settings");

	optioninfo[advancedoptions] = 2; //Sound Settings
	strcpy(menuoptions[advancedoptions++], "Sound Settings");

	optioninfo[advancedoptions] = 3;
	strcpy(menuoptions[advancedoptions++], "Input Settings");

	optioninfo[advancedoptions] = 4; //Clear CMOS!
	strcpy(menuoptions[advancedoptions++], "Clear CMOS data");


	if (!EMU_RUNNING) //Emulator not running (allow memory size change?)
	{
		optioninfo[advancedoptions] = 5; //Memory detect!
		strcpy(menuoptions[advancedoptions++], "Redetect available memory");
	}

	optioninfo[advancedoptions] = 6; //Select BIOS Font!
	strcpy(menuoptions[advancedoptions], "Settings menu Font: ");
	strcat(menuoptions[advancedoptions++], ActiveBIOSPreset.name); //BIOS font selected!
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
	case 7: //Valid option?
		switch (optioninfo[menuresult]) //What option has been chosen, since we are dynamic size?
		{
		case 0:
			BIOS_Menu = 35; //CPU Menu!
			break;
		case 1:
			BIOS_Menu = 29; //Video Settings setting!
			break;
		case 2:
			BIOS_Menu = 31; //Sound Settings menu!
			break;
		case 3:
			BIOS_Menu = 25; //Input submenu!
			break;
		case 4:
			BIOS_Menu = 37; //Clear CMOS!
			break;
		case 5:
			BIOS_Menu = 14; //Memory reallocation!
			break;
		case 6:
			BIOS_Menu = 16; //BIOS Font setting!
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

	if (EMU_RUNNING) //Emulator is running?
	{
		optioninfo[advancedoptions] = 3; //Restart emulator option!
		strcpy(menuoptions[advancedoptions++], "Restart emulator"); //Restart emulator option!
		optioninfo[advancedoptions] = 4; //Restart emulator and enter BIOS menu option!
		strcpy(menuoptions[advancedoptions++], "Restart emulator and enter Settings menu"); // Restart emulator and enter BIOS menu option!
	}
	
	
	if (!EMU_RUNNING) //Emulator isn't running?
	{
		optioninfo[advancedoptions] = 2; //Load defaults option!
		strcpy(menuoptions[advancedoptions++],"Load Setting defaults"); //Load defaults option!
	}

	int menuresult = BIOS_ShowMenu(advancedoptions,4,BIOSMENU_SPEC_LR,&Menu_Stat); //Plain menu, allow L&R triggers!

	switch (menuresult) //What option has been chosen?
	{
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
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
		case 3: //Restart emulator?
			bootBIOS = 0; //Not a forced first run!
			BIOS_Menu = -1; //Quit!
			reboot_needed = 2; //We need a reboot!
			break;
		case 4: //Restart emulator and enter BIOS menu?
			bootBIOS = 1; //Forced first run!
			BIOS_Menu = -1; //Quit!
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

FILEPOS ImageGenerator_GetImageSize(byte x, byte y) //Retrieve the size, or 0 for none!
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

extern byte input_buffer_shift; //Ctrl-Shift-Alt Status for the pressed key!
extern sword input_buffer; //To contain the pressed key!
extern byte input_buffer_mouse; //Mouse button input also supported!

byte BIOS_InputText(byte x, byte y, char *filename, uint_32 maxlength)
{
	delay(100000); //Wait a bit to make sure nothing's pressed!
	enableKeyboard(1); //Buffer input!
	char input[256];
	memset(&input, 0, sizeof(input)); //Init input to empty!
	TicksHolder ticks;
	initTicksHolder(&ticks); //Initialise!
	getnspassed(&ticks); //Initialise counter!

	lock(LOCK_INPUT);
	goto updatescreeninput; //Start screen with input&cursor!

	for (;;) //Main input loop!
	{
		if (shuttingdown()) //Are we shutting down?
		{
			disableKeyboard(); //Disable the keyboard!
			return 0; //Cancel!
		}
		delay(0); //Wait a bit for input, depending on input done!
		updateKeyboard(getnspassed(&ticks)); //Update the OSK keyboard with a little time!
		lock(LOCK_INPUT);
		if (input_buffer!=-1) //Given input yet?
		{
			if (EMU_keyboard_handler_idtoname(input_buffer,&input[0])) //Valid key(Don't count shift statuses only)?
			{
				if (!strcmp(input, "enter") || !strcmp(input,"esc")) //Enter or Escape? We're finished!
				{
					unlock(LOCK_INPUT);
					disableKeyboard(); //Disable the keyboard!
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
									input[0] += (char)((int)'A' - (int)'a'); //Convert to uppercase!
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

				updatescreeninput:
				EMU_locktext();
				EMU_gotoxy(x, y); //Goto position for info!
				EMU_textcolor(BIOS_ATTR_TEXT);
				GPU_EMU_printscreen(x, y, "%s", filename); //Show the filename!
				EMU_textcolor(BIOS_ATTR_ACTIVE); //Active color!
				GPU_EMU_printscreen(-1, -1, "_"); //Cursor indicator!
				EMU_textcolor(BIOS_ATTR_TEXT); //Back to text!
				GPU_EMU_printscreen(-1, -1, " "); //Clear output after!
				EMU_unlocktext();
				input_buffer_shift = 0; //Reset!
				input_buffer_mouse = 0; //Reset!
				input_buffer = -1; //Nothing input!
			}
		}
		else if (input_buffer_shift || input_buffer_mouse) //Shift/mouse are ignored!
		{
			input_buffer_shift = input_buffer_mouse = 0; //Ignore!
		}
		unlock(LOCK_INPUT);
	}
}

void BIOS_GenerateStaticHDD() //Generate Static HDD Image!
{
	char filename[256]; //Filename container!
	char fullfilename[256]; //Full filename container!
	FILEPOS size = 0;
	BIOSClearScreen(); //Clear the screen!
	bzero(filename,sizeof(filename)); //Init!
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
				size = ImageGenerator_GetImageSize(12, 5); //Get the size!
				if (size != 0) //Got size?
				{
					EMU_locktext();
					GPU_EMU_printscreen(12, 5, "%08i MB %04i KB", (uint_32)(size / MBMEMORY), (uint_32)((size % MBMEMORY) / 1024)); //Show size too!
					EMU_gotoxy(0, 6); //Next row!
					GPU_EMU_printscreen(0, 6, "Generating image: "); //Start of percentage!
					EMU_unlocktext();
					domkdir(diskpath);
					memset(fullfilename,0,sizeof(fullfilename));
					strcpy(fullfilename,diskpath);
					strcat(fullfilename,"/");
					strcat(fullfilename,filename);
					generateStaticImage(fullfilename, size, 18, 6); //Generate a static image!
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
	char fullfilename[256]; //Full filename container!
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
				size = ImageGenerator_GetImageSize(12, 5); //Get the size!
				if (size != 0) //Got size?
				{
					EMU_locktext();
					GPU_EMU_printscreen(12, 5, "%08i MB %04i KB", (uint_32)(size / MBMEMORY), (uint_32)((size % MBMEMORY) / 1024)); //Show size too!
					EMU_gotoxy(0, 6); //Next row!
					GPU_EMU_printscreen(0, 6, "Generating image: "); //Start of percentage!
					EMU_unlocktext();
					domkdir(diskpath);
					memset(fullfilename, 0, sizeof(fullfilename));
					strcpy(fullfilename, diskpath);
					strcat(fullfilename, "/");
					strcat(fullfilename, filename);
					generateDynamicImage(fullfilename, size, 18, 6); //Generate a dynamic image!
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
	char fullfilename[256]; //Full filename container!
	bzero(filename, sizeof(filename)); //Init!
	FILEPOS size = 0;
	BIOS_Title("Convert static to dynamic HDD Image"); //Full clear!
	generateFileList(diskpath,"img", 0, 0); //Generate file list for all .img files!
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
		if (strcmp(filename, "") != 0) //Got input?
		{
			BIOS_Title("Convert static to dynamic HDD Image"); //Full clear!
			EMU_locktext();
			EMU_textcolor(BIOS_ATTR_TEXT);
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
				domkdir(diskpath);
				memset(fullfilename, 0, sizeof(fullfilename));
				strcpy(fullfilename, diskpath);
				strcat(fullfilename, "/");
				strcat(fullfilename, filename);
				sizecreated = generateDynamicImage(fullfilename, size, 18, 6); //Generate a dynamic image!
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
						if (!(sectornr % SECTORUPDATEINTERVAL)) //Update every 10000 sectors!
						{
							EMU_locktext();
							GPU_EMU_printscreen(18, 6, "%u%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
							EMU_unlocktext();
						}
						sectornr += datatotransfer; //Next sector block!
					}
					EMU_locktext();
					GPU_EMU_printscreen(18, 6, "%u%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
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
							if (!(sectornr % SECTORUPDATEINTERVAL)) //Update every 10000 sectors!
							{
								EMU_locktext();
								GPU_EMU_printscreen(18, 7, "%u%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
								EMU_unlocktext();
							}
							sectornr += datatotransfer; //Next sector!
						}
						EMU_locktext();
						GPU_EMU_printscreen(18, 7, "%u%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
						EMU_unlocktext();
						if (error) //Error occurred?
						{
							remove(fullfilename); //Try to remove the generated file!
							dolog(filename, "Error %u validating dynamic image sector %u/%u@byte %u", error, sectornr / 512, size / 512, sectorposition); //Error at this sector!
						}
					}
					else //Error occurred?
					{
						dolog(filename, "Error #%u copying static image sector %u/%u", error, sectornr / 512, sizecreated / 512); //Error at this sector!
						if (!remove(fullfilename)) //Defragmented file can be removed?
						{
							dolog(filename, "Error cleaning up the new defragmented image!");
						}
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
	char fullfilename[256];
	bzero(filename, sizeof(filename)); //Init!
	FILEPOS size = 0;
	BIOS_Title("Convert dynamic to static HDD Image"); //Full clear!
	generateFileList(diskpath,"sfdimg", 0, 1); //Generate file list for all .img files!
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
		if (strcmp(filename, "") != 0) //Got input?
		{
			BIOS_Title("Convert dynamic to static HDD Image"); //Full clear!
			EMU_locktext();
			EMU_textcolor(BIOS_ATTR_TEXT);
			EMU_gotoxy(0, 4); //Goto position for info!
			GPU_EMU_printscreen(0, 4, "Filename: %s  ", filename); //Show the filename!
			EMU_gotoxy(0, 5); //Next row!
			GPU_EMU_printscreen(0, 5, "Image size: "); //Show image size selector!!
			EMU_unlocktext();
			iohdd0(filename, 0, 1, 0); //Mount the source disk!
			strcat(filename, ".img"); //Generate destination filename!
			size = getdisksize(HDD0); //Get the original size!
			//dolog("BIOS", "Dynamic disk size: %u bytes = %u sectors", size, (size >> 9));
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
				domkdir(diskpath);
				memset(fullfilename, 0, sizeof(fullfilename));
				strcpy(fullfilename, diskpath);
				strcat(fullfilename, "/");
				strcat(fullfilename, filename);
				dest = emufopen64(fullfilename, "wb"); //Open the destination!
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
						if (emufwrite64(&sector,1,datatotransfer,dest)!=(int_64)datatotransfer) //Error writing a sector?
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
						EMU_locktext();
						GPU_EMU_printscreen(18, 6, "%u%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
						EMU_unlocktext();
					}
					sectornr += datatotransfer; //Next sector!
				}
				emufclose64(dest); //Close the file!

				EMU_locktext();
				GPU_EMU_printscreen(18, 6, "%u%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
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
							EMU_locktext();
							GPU_EMU_printscreen(18, 7, "%u%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
							EMU_unlocktext();
						}
						sectornr += datatotransfer; //Next sector!
					}
					EMU_locktext();
					GPU_EMU_printscreen(18, 6, "%u%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
					EMU_unlocktext();
					if (error) //Error occurred?
					{
						remove(fullfilename); //Try to remove the generated file!
						dolog(filename, "Error #%u validating static image sector %u/%u@byte %u", error, sectornr / 512, size / 512,sectorposition); //Error at this sector!
					}
				}
				else //Error occurred?
				{
					dolog(filename, "Error #%u copying dynamic image sector %u/%u", error, sectornr / 512, size / 512); //Error at this sector!
					if (!remove(fullfilename)) //Defragmented file can be removed?
					{
						dolog(filename, "Error cleaning up the new defragmented image!");
					}
				}
			}
		}
		break;
	}
	BIOS_Menu = 1; //Return to Disk Menu!
}

void BIOS_DefragmentDynamicHDD() //Defragment a dynamic HDD Image!
{
	char errorlog[256]; //Error log string!
	FILEPOS updateinterval=1,updatenr;
	char filename[256], originalfilename[256]; //Filename container!
	char fullfilename[256], fulloriginalfilename[256]; //Full filename container!
	sbyte srcstatus=-1,deststatus=-1; //Status on the two dynamic disk images!
	bzero(filename, sizeof(filename)); //Init!
	FILEPOS size = 0, sectorposition;
	BIOS_Title("Defragment a dynamic HDD Image"); //Full clear!
	generateFileList(diskpath,"sfdimg", 0, 1); //Generate file list for all .img files!
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

		if (strcmp(filename, "") != 0) //Got input?
		{
			BIOS_Title("Defragment a dynamic HDD Image"); //Full clear!
			EMU_locktext();
			EMU_textcolor(BIOS_ATTR_TEXT);
			EMU_gotoxy(0, 4); //Goto position for info!
			GPU_EMU_printscreen(0, 4, "Filename: %s ", filename); //Show the filename!
			EMU_gotoxy(0, 5); //Next row!
			GPU_EMU_printscreen(0, 5, "Image size: "); //Show image size selector!!
			EMU_unlocktext();
			bzero(&originalfilename, sizeof(originalfilename)); //Init!
			strcpy(originalfilename, filename); //The original filename!
			strcat(filename, ".tmp.sfdimg"); //Generate destination filename!

			domkdir(diskpath);
			memset(fullfilename, 0, sizeof(fullfilename));
			strcpy(fullfilename, diskpath);
			strcat(fullfilename, "/");
			strcat(fullfilename, filename);

			memset(fulloriginalfilename, 0, sizeof(fulloriginalfilename));
			strcpy(fulloriginalfilename, diskpath);
			strcat(fulloriginalfilename, "/");
			strcat(fulloriginalfilename, filename);

			size = dynamicimage_getsize(fulloriginalfilename); //Get the original size!
			if (size != 0) //Got size?
			{
				EMU_locktext();
				EMU_gotoxy(0, 6); //Next row!
				GPU_EMU_printscreen(0, 6, "Defragmenting image: "); //Start of percentage!
				EMU_unlocktext();
				FILEPOS sizecreated;
				sizecreated = generateDynamicImage(fullfilename, size, 21, 6); //Generate a dynamic image!
				if (sizecreated >= size) //Correct size?
				{
					EMU_locktext();
					GPU_EMU_printscreen(21, 6, "      "); //Clear the creation process!
					GPU_EMU_printscreen(12, 5, "      "); //Clear the creation process!
					GPU_EMU_printscreen(12, 5, "%iMB", (sizecreated / MBMEMORY)); //Image size
					EMU_unlocktext();
					uint_32 sectornr,destsectornr,previoussectornr=0,previousdestsectornr=0;
					byte error = 0;
					size >>= 9; //Convert to actual 512-byte sector numbers: we're allowed in this case!
					updateinterval = (size/100); //Update interval in sectors: every 1% updated!
					if (!updateinterval) updateinterval = 1; //Minimum of 1 sector interval!
					updatenr = 0; //Reset update number!
					srcstatus = 0; //Initialize to EOF!
					for (sectornr = 0; sectornr < size;) //Process all sectors from the source image!
					{
						sectorposition = 0; //Default: no position!
						if (shuttingdown())
						{
							error = 4; //Give the fourth error!
							break;
						}
						
						if (dynamicimage_readexistingsector(fulloriginalfilename,sectornr,&sector)) //Sector exists and non-empty? Then try to copy it to the new disk image!
						{
							if (!dynamicimage_writesector(fullfilename, sectornr, &sector)) //Error writing a sector?
							{
								error = 2;
								break; //Stop reading!
							}
						}
						previoussectornr = sectornr; //Last sector number to compare to!
						srcstatus = dynamicimage_nextallocatedsector(fulloriginalfilename,&sectornr); //Next sector or block etc. which is available!
						switch (srcstatus) //What status?
						{
							case 0: //EOF reached?
								goto finishedphase1; //Finished transferring!
							case -1: //Error in file?
								error = 1;
								goto finishedphase1; //Finished transferring!
							default: //Unknown?
							case 1: //Next sector to process?
								break; //Continue running on the next sector to process!
						}
						updatenr += sectornr-previoussectornr; //Last processed sector number difference!
						if (updatenr>=updateinterval) //Update every 1% sectors!
						{
							updatenr = 0; //Reset!
							EMU_locktext();
							GPU_EMU_printscreen(21, 6, "%u%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
							EMU_unlocktext();
						}
					}
					finishedphase1:
					EMU_locktext();
					GPU_EMU_printscreen(21, 6, "%u%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
					EMU_unlocktext();

					//Verification!
					if ((error==0) && (srcstatus==0)) //OK and fully processed?
					{
						EMU_locktext();
						GPU_EMU_printscreen(0, 7, "Validating image: "); //Start of percentage!
						EMU_unlocktext();
						updatenr = 0; //Reset update number!
						destsectornr = previoussectornr = previousdestsectornr = 0; //Destination starts at sector #0 too!
						error = 0; //Default: no error!
						for (sectornr = 0; sectornr < size;) //Process all sectors!
						{
							sectorposition = 0; //Default: no position!
							if (shuttingdown())
							{
								error = 4; //Give the fourth error!
								break;
							}

							if (dynamicimage_readexistingsector(fulloriginalfilename,sectornr,&sector)) //Sector exists in the old disk image? Then try to check it to the new disk image!
							{
								if (dynamicimage_readexistingsector(fullfilename,sectornr,&verificationsector)) //Sector exists in the new disk image? Then try to check it from the new disk image!
								{
									if ((sectorposition = memcmp(&sector, &verificationsector, 512)) != 0) //Data error?
									{
										error = 3; //Verification error!
										break; //Stop reading!
									}
									//We're a valid written sector!
								}
								else //Missing defragmented sector!
								{
									error = 3; //Verification error!
									break; //Stop reading!
								}
							}
							else if (dynamicimage_readexistingsector(fullfilename,sectornr,&verificationsector)) //Sector exists in the new disk image but not in the old disk image?
							{
								error = 3; //Verification error: exists within defragemented image but not in source image!
								break; //Stop reading!
							}
							//We're a valid written or non-existing sector!

							previoussectornr = sectornr; //Last sector number to compare to!
							previousdestsectornr = destsectornr; //Save the previous for comparing!
							deststatus = dynamicimage_nextallocatedsector(fullfilename,&destsectornr); //Next sector or block etc. which is available!
							srcstatus = dynamicimage_nextallocatedsector(fulloriginalfilename,&sectornr); //Next sector or block etc. which is available!
							if ((deststatus!=srcstatus) || (sectornr!=destsectornr)) //Next status or sector number differs?
							{
								error = 2; //Position/status error!
								goto finishedphase2;
							}
							switch (srcstatus) //What status?
							{
								case 0: //EOF reached?
									goto finishedphase2; //Finished transferring!
								case -1: //Error in file?
									error = 1; //Error!
									goto finishedphase2; //Finished transferring!
								default: //Unknown?
								case 1: //Next sector to process?
									break; //Continue running on the next sector to process!
							}
							updatenr += sectornr-previoussectornr; //Last processed sector number difference!
							if (updatenr>=updateinterval) //Update every 1% sectors!
							{
								updatenr = 0; //Reset!
								EMU_locktext();
								GPU_EMU_printscreen(18, 7, "%u%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
								EMU_unlocktext();
							}
						}
						finishedphase2:
						EMU_locktext();
						GPU_EMU_printscreen(18, 7, "%u%%", (int)(((float)sectornr / (float)size)*100.0f)); //Current progress!
						EMU_unlocktext();
						if (error) //Error occurred?
						{
							bzero(&errorlog,sizeof(errorlog)); //Clear the error log data!
							if (error==2) //Position/status error?
							{
								sprintf(errorlog,"Position/status error: Source status: %i, Destination status: %i, Error source sector: %u, error destination sector: %u",srcstatus,deststatus,sectornr,destsectornr);
							}
							switch (srcstatus) //What status?
							{
								case 0: //EOF reached?
									strcat(errorlog,"\nSource: EOF"); //Finished transferring!
									break;
								case -1: //Error in file?
									strcat(errorlog,"\nSource: ERROR"); //Finished transferring!
									goto finishedphase2; //Finished transferring!
								default: //Unknown?
								case 1: //Next sector to process?
									sprintf(errorlog,"%s\nSource: sector %u",errorlog,sectornr); //This sector!
									break; //Continue running on the next sector to process!
							}
							switch (deststatus) //What status?
							{
								case 0: //EOF reached?
									strcat(errorlog,"\nDestination: EOF"); //Finished transferring!
									break;
								case -1: //Error in file?
									strcat(errorlog,"\nDestination: ERROR"); //Finished transferring!
									goto finishedphase2; //Finished transferring!
								default: //Unknown?
								case 1: //Next sector to process?
									sprintf(errorlog,"%s\nDestination: sector %u",errorlog,sectornr); //This sector!
									break; //Continue running on the next sector to process!
							}
							sprintf(errorlog,"%s\nPrevious source sector: %u\nPrevious destination sector: %u",errorlog,previoussectornr,previousdestsectornr); //Previous sector numbers!
							dolog(originalfilename, "Error %u validating dynamic image sector %u/%u@byte %u", error, sectornr, size, sectorposition); //Error at this sector!
							dolog(originalfilename, "\n%s",errorlog); //Error at this sector information!
						}
						else //We've been defragmented?
						{
							if (!remove(fulloriginalfilename)) //Original can be removed?
							{
								if (!rename(fullfilename, fulloriginalfilename)) //The destination is the new original!
								{
									dolog(originalfilename, "Error renaming the new defragmented image to the original filename!");
								}
							}
							else
							{
								dolog(originalfilename, "Error replacing the old image with the defragmented image!");
							}
						}
					}
					else //Error occurred?
					{
						dolog(originalfilename, "Error #%u copying dynamic image sector to defragmented image sector %u/%u", error, sectornr, size); //Error at this sector!
						switch (srcstatus) //What status?
						{
							case 0: //EOF reached?
								strcat(errorlog,"Source: EOF"); //Finished transferring!
								break;
							case -1: //Error in file?
								strcat(errorlog,"Source: ERROR"); //Finished transferring!
								break;
							default: //Unknown?
							case 1: //Next sector to process?
								sprintf(errorlog,"Source: sector %u",sectornr); //This sector!
								break; //Continue running on the next sector to process!
						}
						if (!remove(fullfilename)) //Defragmented file can be removed?
						{
							dolog(originalfilename, "Error cleaning up the new defragmented image!");
						}
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
	BIOS_Menu = 35; //Goto CPU menu!
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
	switch (BIOS_Settings.executionmode) //What execution mode?
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
	BIOS_Menu = 35; //Goto CPU menu!
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
	numlist = 4; //Ammount of Execution modes!
	for (i = 0; i<numlist; i++) //Process options!
	{
		bzero(itemlist[i], sizeof(itemlist[i])); //Reset!
	}

	strcpy(itemlist[DEBUGGERLOG_NONE], "Don't log"); //Set filename from options!
	strcpy(itemlist[DEBUGGERLOG_DEBUGGING], "Only when debugging"); //Set filename from options!
	strcpy(itemlist[DEBUGGERLOG_ALWAYS], "Always log"); //Set filename from options!
	strcpy(itemlist[DEBUGGERLOG_INT],"Interrupt calls only");

	int current = 0;
	switch (BIOS_Settings.debugger_log) //What debugger log mode?
	{
	case DEBUGGERLOG_NONE: //None
	case DEBUGGERLOG_DEBUGGING: //Only when debugging
	case DEBUGGERLOG_ALWAYS: //Always
	case DEBUGGERLOG_INT: //Interrupt calls only
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
	case DEBUGGERLOG_INT: //Interrupt calls only
	default: //Changed?
		if (file != current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.debugger_log = file; //Select Debug Mode!
		}
		break;
	}
	BIOS_Menu = 35; //Goto CPU menu!
}
extern byte force_memoryredetect; //From the MMU: force memory redetect on load?

void BIOS_MemReAlloc() //Reallocates BIOS memory!
{
	BIOS_Menu = 8; //Goto Advanced menu!
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
	BIOS_Menu = 29; //Goto Video Settings menu!
}

void BIOS_FontSetting()
{
	BIOS_Title("Font");
	EMU_locktext();
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"Settings menu Font: "); //Show selection init!
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
	int font = ExecuteList(21,4,itemlist[current],256,NULL); //Show options for the installed CPU!
	switch (font) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		font = 0; //Default font: Standard!
	default: //Changed?
		if (font!=current && current<(int)NUMITEMS(BIOSMenu_Fonts)) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.BIOSmenu_font = font; //Select Direct Plot setting!
		}
		break;
	}
	BIOS_Menu = 8; //Goto Advanced menu!
}

void BIOS_AspectRatio()
{
	BIOS_Title("Aspect ratio");
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Aspect ratio: "); //Show selection init!
	EMU_unlocktext();
	int i = 0; //Counter!
	numlist = 6; //Ammount of Aspect Ratio modes!
	for (i = 0; i<numlist; i++) //Process options!
	{
		bzero(itemlist[i], sizeof(itemlist[i])); //Reset!
	}
	strcpy(itemlist[0], "Fullscreen stretching"); //Set filename from options!
	strcpy(itemlist[1], "Keep the same"); //Set filename from options!
	strcpy(itemlist[2], "Force 4:3(VGA)"); //Set filename from options!
	strcpy(itemlist[3], "Force CGA"); //Set filename from options!
	strcpy(itemlist[4], "Force 4:3(SVGA 768p)"); //Set filename from options!
	strcpy(itemlist[5], "Force 4:3(SVGA 1080p)"); //Set filename from options!
	int current = 0;
	switch (BIOS_Settings.aspectratio) //What direct plot?
	{
	case 0: //Valid
	case 1: //Valid
	case 2: //Valid
	case 3: //Valid
	case 4: //Valid
	case 5: //Valid
		current = BIOS_Settings.aspectratio; //Valid: use!
		break;
	default: //Invalid
		current = 0; //Default: none!
		break;
	}
	if (BIOS_Settings.aspectratio != current) //Invalid?
	{
		BIOS_Settings.aspectratio = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int file = ExecuteList(15, 4, itemlist[current], 256, NULL); //Show options for the installed CPU!
	switch (file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = 0; //Default direct plot: None!

	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	default: //Changed?
		if (file != current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.aspectratio = file; //Select Aspect Ratio setting!
		}
		break;
	}
	BIOS_Menu = 29; //Goto Video menu!
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
	strcpy(itemlist[BWMONITOR_WHITE], "B/W monitor: white"); //Set filename from options!
	strcpy(itemlist[BWMONITOR_GREEN], "B/W monitor: green"); //Set filename from options!
	strcpy(itemlist[BWMONITOR_AMBER], "B/W monitor: amber"); //Set filename from options!

	int current = 0;
	switch (BIOS_Settings.bwmonitor) //What B/W monitor mode?
	{
	case BWMONITOR_NONE: //None
	case BWMONITOR_WHITE: //Black/White
	case BWMONITOR_GREEN: //Green
	case BWMONITOR_AMBER: //Amber
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
	case BWMONITOR_WHITE: //Black/White
	case BWMONITOR_GREEN: //Greenscale
	case BWMONITOR_AMBER: //Amberscale
	default: //Changed?
		if (file != current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.bwmonitor = file; //Select Debug Mode!
		}
		break;
	}
	BIOS_Menu = 29; //Goto Video Settings menu!
}

void BIOSMenu_LoadDefaults() //Load the defaults option!
{
	if (__HW_DISABLED) return; //Abort!
	int showchecksumerrors_backup = exec_showchecksumerrors; //Keep this!
	exec_showchecksumerrors = 0; //Don't show checksum errors!
	BIOS_LoadDefaults(0); //Load BIOS Defaults, don't save!
	exec_showchecksumerrors = showchecksumerrors_backup; //Restore!
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

setMousetext: //For fixing it!
	optioninfo[advancedoptions] = 2; //Mouse!
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
setJoysticktext: //For fixing it!
	optioninfo[advancedoptions] = 3; //Joystick!
	strcpy(menuoptions[advancedoptions], "Gaming mode: ");
	switch (BIOS_Settings.input_settings.gamingmode_joystick) //Joystick?
	{
	case 0:
		strcat(menuoptions[advancedoptions++], "Normal gaming mode mapped input");
		break;
	case 1:
		strcat(menuoptions[advancedoptions++], "Joystick, Cross=Button 1, Circle=Button 2");
		break;
	case 2:
		strcat(menuoptions[advancedoptions++], "Joystick, Cross=Button 2, Circle=Button 1");
		break;
	case 3:
		strcat(menuoptions[advancedoptions++], "Joystick, Gravis Gamepad");
		break;
	case 4:
		strcat(menuoptions[advancedoptions++], "Joystick, Gravis Analog Pro");
		break;
	case 5:
		strcat(menuoptions[advancedoptions++], "Joystick, Logitech WingMan Extreme Digital");
		break;
	default: //Error: fix it!
		BIOS_Settings.input_settings.gamingmode_joystick = 0; //Reset/Fix!
		BIOS_Changed = 1; //We've changed!
		goto setJoysticktext; //Goto!
		break;
	}

#ifndef SDL2
#ifndef IS_PSP
	optioninfo[advancedoptions] = 4; //Reconnect joystick
	strcpy(menuoptions[advancedoptions++], "Detect joystick"); //Detect the new joystick!
#endif
#endif
}

void BIOS_inputMenu() //Manage stuff concerning input.
{
	BIOS_Title("Input Settings Menu");
	BIOS_InitInputText(); //Init text!
	int menuresult = BIOS_ShowMenu(advancedoptions, 4, BIOSMENU_SPEC_RETURN, &Menu_Stat); //Show the menu options!
	switch (menuresult)
	{
	case BIOSMENU_SPEC_CANCEL: //Return?
		BIOS_Menu = 8; //Goto Advanced Menu!
		break;
	case 0:
	case 1:
	case 2:
	case 3:
	case 4: //Valid option?
		switch (optioninfo[menuresult]) //What option has been chosen, since we are dynamic size?
		{
		case 0: //Gaming mode buttons?
			BIOS_Menu = 26; //Map gaming mode buttons Menu!
			break;
		case 1: //Keyboard colors?
			BIOS_Menu = 27; //Assign keyboard colors Menu!
			break;
		case 2:
			if (!EMU_RUNNING) BIOS_Menu = 34; //Mouse option!
			break;
		case 3:
			BIOS_Menu = 50; //Joystick option!
			break;
		case 4:
			BIOS_Menu = 51; //Joystick connect option!
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
	byte shiftstatus;
	byte mousestatus;	

	char name[256]; //A little buffer for a name!
	if ((BIOS_Settings.input_settings.keyboard_gamemodemappings[inputnumber] != -1) || (BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[inputnumber]) || (BIOS_Settings.input_settings.mouse_gamemodemappings[inputnumber])) //Got anything?
	{
		shiftstatus = BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[inputnumber]; //Load shift status!
		input_key = BIOS_Settings.input_settings.keyboard_gamemodemappings[inputnumber]; //Load shift status!
		mousestatus = BIOS_Settings.input_settings.mouse_gamemodemappings[inputnumber]; //Load mouse status!
		if (shiftstatus) //Gotten alt status?
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
			if ((input_key != -1) || mousestatus) //Gotten a key/mouse?
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
			if (mousestatus)
			{
				strcat(s, "-"); //Seperator!
			}
		}
		if (mousestatus) //Gotten a mouse input?
		{
			if (mousestatus&1) //Left button?
			{
				strcat(s,"Mouse left");
				if ((mousestatus&1)!=mousestatus) //More buttons?
				{
					strcat(s,"-"); //Seperator!
				}
			}
			if (mousestatus&2) //Right button?
			{
				strcat(s,"Mouse right");
				if ((mousestatus&3)!=mousestatus) //More buttons?
				{
					strcat(s,"-");
				}
			}
			if (mousestatus&4) //Middle button?
			{
				strcat(s,"Mouse middle");
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
	for (i = 0; i<15; i++) //Set all possibilities!
	{
		bzero(menuoptions[i], sizeof(menuoptions[i])); //Init!
		optioninfo[advancedoptions] = i; //The key!
		switch (i) //What key?
		{
			case GAMEMODE_START:
				strcpy(menuoptions[advancedoptions], "Start:        "); //Gaming mode buttons!
				BIOS_addInputText(&menuoptions[advancedoptions++][0], i);
				break;
			case GAMEMODE_LEFT:
				strcpy(menuoptions[advancedoptions], "Left:         "); //Assign keyboard colors!
				BIOS_addInputText(&menuoptions[advancedoptions++][0], i);
				break;
			case GAMEMODE_UP:
				strcpy(menuoptions[advancedoptions], "Up:           "); //Assign keyboard colors!
				BIOS_addInputText(&menuoptions[advancedoptions++][0], i);
				break;
			case GAMEMODE_RIGHT:
				strcpy(menuoptions[advancedoptions], "Right:        "); //Assign keyboard colors!
				BIOS_addInputText(&menuoptions[advancedoptions++][0], i);
				break;
			case GAMEMODE_DOWN:
				strcpy(menuoptions[advancedoptions], "Down:         "); //Assign keyboard colors!
				BIOS_addInputText(&menuoptions[advancedoptions++][0], i);
				break;
			case GAMEMODE_LTRIGGER:
				strcpy(menuoptions[advancedoptions], "L:            "); //Assign keyboard colors!
				BIOS_addInputText(&menuoptions[advancedoptions++][0], i);
				break;
			case GAMEMODE_RTRIGGER:
				strcpy(menuoptions[advancedoptions], "R:            "); //Assign keyboard colors!
				BIOS_addInputText(&menuoptions[advancedoptions++][0], i);
				break;
			case GAMEMODE_TRIANGLE:
				strcpy(menuoptions[advancedoptions], "Triangle:     "); //Assign keyboard colors!
				BIOS_addInputText(&menuoptions[advancedoptions++][0], i);
				break;
			case GAMEMODE_CIRCLE:
				strcpy(menuoptions[advancedoptions], "Circle:       "); //Assign keyboard colors!
				BIOS_addInputText(&menuoptions[advancedoptions++][0], i);
				break;
			case GAMEMODE_CROSS:
				strcpy(menuoptions[advancedoptions], "Cross:        "); //Assign keyboard colors!
				BIOS_addInputText(&menuoptions[advancedoptions++][0], i);
				break;
			case GAMEMODE_SQUARE:
				strcpy(menuoptions[advancedoptions], "Square:       "); //Assign keyboard colors!
				BIOS_addInputText(&menuoptions[advancedoptions++][0], i);
				break;
			case GAMEMODE_ANALOGLEFT:
				strcpy(menuoptions[advancedoptions], "Analog left:  "); //Assign keyboard colors!
				BIOS_addInputText(&menuoptions[advancedoptions++][0], i);
				break;
			case GAMEMODE_ANALOGUP:
				strcpy(menuoptions[advancedoptions], "Analog up:    "); //Assign keyboard colors!
				BIOS_addInputText(&menuoptions[advancedoptions++][0], i);
				break;
			case GAMEMODE_ANALOGRIGHT:
				strcpy(menuoptions[advancedoptions], "Analog right: "); //Assign keyboard colors!
				BIOS_addInputText(&menuoptions[advancedoptions++][0], i);
				break;
			case GAMEMODE_ANALOGDOWN:
				strcpy(menuoptions[advancedoptions], "Analog down:  "); //Assign keyboard colors!
				BIOS_addInputText(&menuoptions[advancedoptions++][0], i);
				break;
			default: //Unknown? Don't handle unknown cases!
				break;
		}
	}
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
	case GAMEMODE_START:
	case GAMEMODE_LEFT:
	case GAMEMODE_UP:
	case GAMEMODE_RIGHT:
	case GAMEMODE_DOWN:
	case GAMEMODE_LTRIGGER:
	case GAMEMODE_RTRIGGER:
	case GAMEMODE_TRIANGLE:
	case GAMEMODE_CIRCLE:
	case GAMEMODE_CROSS:
	case GAMEMODE_SQUARE:
	case GAMEMODE_ANALOGLEFT:
	case GAMEMODE_ANALOGUP:
	case GAMEMODE_ANALOGRIGHT:
	case GAMEMODE_ANALOGDOWN:
		if (Menu_Stat == BIOSMENU_STAT_SQUARE) //Square pressed on an item?
		{
			BIOS_Changed |= ((BIOS_Settings.input_settings.keyboard_gamemodemappings[menuresult] != -1) || (BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[menuresult]) || (BIOS_Settings.input_settings.mouse_gamemodemappings[menuresult])); //Did we change?
			BIOS_Settings.input_settings.keyboard_gamemodemappings[menuresult] = -1; //Set the new key!
			BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[menuresult] = 0; //Set the shift status!
			BIOS_Settings.input_settings.mouse_gamemodemappings[menuresult] = 0; //Set the mouse status!
		}
		else //Normal option selected?
		{
			//Valid option?
			delay(100000); //Wait a bit!
			enableKeyboard(1); //Buffer input!
			TicksHolder ticks;
			initTicksHolder(&ticks); //Initialise!
			getnspassed(&ticks); //Initialise counter!
			for (;;)
			{
				updateKeyboard(getnspassed(&ticks)); //Update the OSK keyboard!
				lock(LOCK_INPUT);
				if ((input_buffer!=-1) || (input_buffer_shift) || (input_buffer_mouse)) //Given input yet?
				{
					BIOS_Changed |= ((BIOS_Settings.input_settings.keyboard_gamemodemappings[menuresult] != input_buffer) || (BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[menuresult] != input_buffer_shift) || (BIOS_Settings.input_settings.mouse_gamemodemappings[menuresult] != input_buffer_mouse)); //Did we change?
					BIOS_Settings.input_settings.keyboard_gamemodemappings[menuresult] = input_buffer; //Set the new key!
					BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[menuresult] = input_buffer_shift; //Set the shift status!
					BIOS_Settings.input_settings.mouse_gamemodemappings[menuresult] = input_buffer_mouse; //Set the shift status!
					unlock(LOCK_INPUT); //We're done with input: release our lock!
					disableKeyboard(); //Disable the keyboard!
					break; //Break out of the loop: we're done!
				}
				unlock(LOCK_INPUT);
				delay(0); //Wait for the key input!
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
		strcat(s, "<UNKNOWN. CHECK SETTINGS VERSION>"); //Set color from options!
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
	switch (BIOS_Settings.input_settings.colors[gamingKeyboardColor]) //What color?
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

void BIOS_VGAModeSetting()
{
	BIOS_Title("VGA Mode");
	EMU_locktext();
	EMU_gotoxy(0,4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0,4,"VGA Mode: "); //Show selection init!
	EMU_unlocktext();
	int i = 0; //Counter!
	numlist = 8; //Ammount of VGA modes! Only use two modes, as the precursor compatibility mode(CGA) isn't finished yet!
	for (i=0; i<numlist; i++) //Process options!
	{
		bzero(itemlist[i],sizeof(itemlist[i])); //Reset!
	}
	strcpy(itemlist[0],"Pure VGA"); //Set filename from options!
	strcpy(itemlist[1],"VGA with NMI"); //Set filename from options!
	strcpy(itemlist[2],"VGA with CGA"); //Special CGA compatibility mode!
	strcpy(itemlist[3],"VGA with MDA"); //Special MDA compatibility mode!
	strcpy(itemlist[4],"Pure CGA"); //Special CGA pure mode!
	strcpy(itemlist[5],"Pure MDA"); //Special MDA pure mode!
	strcpy(itemlist[6],"Tseng ET4000"); //Tseng ET4000 card!
	strcpy(itemlist[7],"Tseng ET3000"); //Tseng ET4000 card!

	int current = 0;
	switch (BIOS_Settings.VGA_Mode) //What setting?
	{
	case 0: //Valid
	case 1: //Valid
	case 2: //Valid
	case 3: //Valid
	case 4: //Valid
	case 5: //Valid
	case 6: //Valid
	case 7: //Valid
		current = BIOS_Settings.VGA_Mode; //Valid: use!
		break;
	default: //Invalid
		current = 0; //Default: none!
		break;
	}
	if (BIOS_Settings.VGA_Mode!=current) //Invalid?
	{
		BIOS_Settings.VGA_Mode = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int file = ExecuteList(10,4,itemlist[current],256,NULL); //Show options for the installed CPU!
	switch (file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = 0; //Default setting: Disabled!

	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	default: //Changed?
		if (file!=current) //Not current?
		{
			byte isSVGA = ((file==6) || (file==7)); //Chosen SVGA card?
			byte wasSVGA = ((current==6) || (current==7)); //Was SVGA card?
			if (isSVGA!=wasSVGA) //Switching to/from SVGA mode?
			{
				BIOS_Settings.VRAM_size = 0; //Autodetect current memory size!
			}
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.VGA_Mode = file; //Select VGA Mode setting!
		}
		break;
	}
	BIOS_Menu = 29; //Goto Video Settings menu!
}

void BIOS_InitVideoSettingsText()
{
	advancedoptions = 0; //Init!
	int i;
	for (i=0; i<6; i++) //Clear all possibilities!
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

setaspectratiotext:
	optioninfo[advancedoptions] = 4; //Keep aspect ratio!
	strcpy(menuoptions[advancedoptions], "Aspect ratio: ");
	switch (BIOS_Settings.aspectratio) //Keep aspect ratio?
	{
	case 0:
		strcat(menuoptions[advancedoptions++], "Fullscreen stretching");
		break;
	case 1:
		strcat(menuoptions[advancedoptions++], "Keep the same");
		break;
	case 2:
		strcat(menuoptions[advancedoptions++], "Force 4:3(VGA)");
		break;
	case 3:
		strcat(menuoptions[advancedoptions++], "Force CGA");
		break;
	case 4:
		strcat(menuoptions[advancedoptions++], "Force 4:3(SVGA 768p)");
		break;
	case 5:
		strcat(menuoptions[advancedoptions++], "Force 4:3(SVGA 1080p)");
		break;
	default:
		BIOS_Settings.aspectratio = 0; //Reset/Fix!
		BIOS_Changed = 1; //We've changed!
		goto setaspectratiotext;
		break;
	}

setmonitortext: //For fixing it!
	optioninfo[advancedoptions] = 1; //Monitor!
	strcpy(menuoptions[advancedoptions], "Monitor: ");
	switch (BIOS_Settings.bwmonitor) //B/W monitor?
	{
	case BWMONITOR_WHITE:
		strcat(menuoptions[advancedoptions++], "B/W monitor: white");
		break;
	case BWMONITOR_GREEN:
		strcat(menuoptions[advancedoptions++], "B/W monitor: green");
		break;
	case BWMONITOR_AMBER:
		strcat(menuoptions[advancedoptions++], "B/W monitor: amber");
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

setVGAModetext: //For fixing it!
	optioninfo[advancedoptions] = 2; //VGA Mode!
	strcpy(menuoptions[advancedoptions], "VGA Mode: ");
	switch (BIOS_Settings.VGA_Mode) //VGA Mode?
	{
	case 0:
		strcat(menuoptions[advancedoptions++], "Pure VGA");
		break;
	case 1:
		strcat(menuoptions[advancedoptions++], "VGA with NMI");
		break;
	case 2:
		strcat(menuoptions[advancedoptions++], "VGA with CGA");
		break;
	case 3:
		strcat(menuoptions[advancedoptions++], "VGA with MDA");
		break;
	case 4:
		strcat(menuoptions[advancedoptions++], "Pure CGA");
		break;
	case 5:
		strcat(menuoptions[advancedoptions++], "Pure MDA");
		break;
	case 6:
		strcat(menuoptions[advancedoptions++], "Tseng ET4000"); //Tseng ET4000 SVGA card!
		break;
	case 7:
		strcat(menuoptions[advancedoptions++], "Tseng ET3000"); //Tseng ET3000 SVGA card!
		break;
	default: //Error: fix it!
		BIOS_Settings.VGA_Mode = 0; //Reset/Fix!
		BIOS_Changed = 1; //We've changed!
		goto setVGAModetext; //Goto!
		break;
	}

setCGAModeltext: //For fixing it!
	optioninfo[advancedoptions] = 3; //CGA Model!
	strcpy(menuoptions[advancedoptions], "CGA Model: ");
	switch (BIOS_Settings.CGAModel) //CGA Model?
	{
	case 0:
		strcat(menuoptions[advancedoptions++], "Old-style RGB");
		break;
	case 1:
		strcat(menuoptions[advancedoptions++], "Old-style NTSC");
		break;
	case 2:
		strcat(menuoptions[advancedoptions++], "New-style RGB");
		break;
	case 3:
		strcat(menuoptions[advancedoptions++], "New-style NTSC");
		break;
	default: //Error: fix it!
		BIOS_Settings.CGAModel = 0; //Reset/Fix!
		BIOS_Changed = 1; //We've changed!
		goto setCGAModeltext; //Goto!
		break;
	}

	optioninfo[advancedoptions] = 5; //Show framerate!
	strcpy(menuoptions[advancedoptions], "Show framerate: ");
	if (BIOS_Settings.ShowFramerate)
	{
		strcat(menuoptions[advancedoptions++], "Enabled");
	}
	else
	{
		strcat(menuoptions[advancedoptions++], "Disabled");
	}

	optioninfo[advancedoptions] = 6; //VGA Synchronization!
	strcpy(menuoptions[advancedoptions], "VGA Synchronization: ");
	switch (BIOS_Settings.VGASynchronization)
	{
		default: //Unknown?
		case 0: //Old synchronization method?
			strcat(menuoptions[advancedoptions++], "Old synchronization depending on host");
			break;
		case 1: //Synchronize depending on the Host?
			strcat(menuoptions[advancedoptions++], "Synchronize depending on host");
			break;
		case 2: //Full CPU synchronization?
			strcat(menuoptions[advancedoptions++], "Full CPU synchronization");
			break;
	}

	optioninfo[advancedoptions] = 7; //Dump VGA!
	strcpy(menuoptions[advancedoptions++],"Dump VGA");
}

void BIOS_VideoSettingsMenu() //Manage stuff concerning input.
{
	BIOS_Title("Video Settings Menu");
	BIOS_InitVideoSettingsText(); //Init text!
	int menuresult = BIOS_ShowMenu(advancedoptions, 4, BIOSMENU_SPEC_RETURN, &Menu_Stat); //Show the menu options!
	switch (menuresult)
	{
	case BIOSMENU_SPEC_CANCEL: //Return?
		BIOS_Menu = 8; //Goto Advanced Menu!
		break;
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7: //Valid option?
		switch (optioninfo[menuresult]) //What option has been chosen, since we are dynamic size?
		{
		case 0: //Direct plot setting?
			BIOS_Menu = 15; //Direct plot setting!
			break;
		case 1: //Monitor?
			BIOS_Menu = 22; //Monitor setting!
			break;
		case 2: //VGA Mode?
			if (!EMU_RUNNING) BIOS_Menu = 30; //VGA Mode setting!
			break;
		case 3: //CGA Model
			BIOS_Menu = 49; //CGA Model!
			break;
		case 4: //Aspect ratio setting!
			BIOS_Menu = 17; //Aspect ratio setting!
			break;
		case 5: //Show framerate setting!
			BIOS_Menu = 39; //Show framerate setting!
			break;
		case 6: //VGA Synchronization setting!
			if (!EMU_RUNNING) BIOS_Menu = 47; //VGA Synchronization setting!
			break;
		case 7: //Dump VGA?
			BIOS_Menu = 48; //Dump VGA!
			break;
		default:
			BIOS_Menu = NOTIMPLEMENTED; //Not implemented yet!
			break;
		}
		break;
	default: //Unknown option?
		BIOS_Menu = NOTIMPLEMENTED; //Not implemented yet!
		break;
	}
}

void BIOS_InitSoundText()
{
	advancedoptions = 0; //Init!
	int i;
	for (i = 0; i<9; i++) //Clear all possibilities!
	{
		bzero(menuoptions[i], sizeof(menuoptions[i])); //Init!
	}

	optioninfo[advancedoptions] = 0; //MPU Soundfont!
	strcpy(menuoptions[advancedoptions], "MPU Soundfont: ");
	if (strcmp(BIOS_Settings.SoundFont, "") != 0)
	{
		strcat(menuoptions[advancedoptions++], BIOS_Settings.SoundFont); //The selected soundfont!
	}
	else
	{
		strcat(menuoptions[advancedoptions++], "<None>");
	}

	optioninfo[advancedoptions] = 1; //PC Speaker!
	strcpy(menuoptions[advancedoptions], "PC Speaker: ");
	if (BIOS_Settings.usePCSpeaker)
	{
		strcat(menuoptions[advancedoptions++], "Sound");
	}
	else
	{
		strcat(menuoptions[advancedoptions++], "No sound");
	}

	optioninfo[advancedoptions] = 2; //Adlib!
	strcpy(menuoptions[advancedoptions], "Adlib: ");
	if (BIOS_Settings.useAdlib)
	{
		strcat(menuoptions[advancedoptions++], "Enabled");
	}
	else
	{
		strcat(menuoptions[advancedoptions++], "Disabled");
	}

	optioninfo[advancedoptions] = 3; //LPT DAC!
	strcpy(menuoptions[advancedoptions], "LPT DAC: ");
	if (BIOS_Settings.useLPTDAC)
	{
		strcat(menuoptions[advancedoptions++], "Enabled");
	}
	else
	{
		strcat(menuoptions[advancedoptions++], "Disabled");
	}

	optioninfo[advancedoptions] = 4; //Game Blaster!
	strcpy(menuoptions[advancedoptions], "Game Blaster: ");
	if (BIOS_Settings.useGameBlaster)
	{
		strcat(menuoptions[advancedoptions++], "Enabled");
	}
	else
	{
		strcat(menuoptions[advancedoptions++], "Disabled");
	}

	optioninfo[advancedoptions] = 5; //Sound Blaster!
	strcpy(menuoptions[advancedoptions], "Sound Blaster: ");
	if (BIOS_Settings.useSoundBlaster)
	{
		strcat(menuoptions[advancedoptions++], "Enabled");
	}
	else
	{
		strcat(menuoptions[advancedoptions++], "Disabled");
	}

	optioninfo[advancedoptions] = 6; //Sound Source Volume!
	sprintf(menuoptions[advancedoptions],"Sound Source Volume: %i",(int)(BIOS_Settings.SoundSource_Volume)); //Sound source volume as a whole number!
	strcat(menuoptions[advancedoptions++],"%%"); //The percentage sign goes wrong with sprintf! Also, when converted to text layer we need to be double! This is the fix!

	optioninfo[advancedoptions] = 7; //Game Blaster Volume!
	sprintf(menuoptions[advancedoptions],"Game Blaster Volume: %i",(int)(BIOS_Settings.GameBlaster_Volume)); //Sound source volume as a whole number!
	strcat(menuoptions[advancedoptions++],"%%"); //The percentage sign goes wrong with sprintf! Also, when converted to text layer we need to be double! This is the fix!

	if (!EMU_RUNNING)
	{
		optioninfo[advancedoptions] = 8; //Music player!
		strcpy(menuoptions[advancedoptions++], "Music Player");
	}

	optioninfo[advancedoptions] = 9; //Start/stop recording sound!
	if (!sound_isRecording()) //Not recording yet?
	{
		strcpy(menuoptions[advancedoptions++], "Start recording sound"); //Sound source volume as a whole number!
	}
	else
	{
		strcpy(menuoptions[advancedoptions++], "Stop recording sound"); //Sound source volume as a whole number!
	}
}

void BIOS_SoundMenu() //Manage stuff concerning input.
{
	BIOS_Title("Sound Settings Menu");
	BIOS_InitSoundText(); //Init text!
	int menuresult = BIOS_ShowMenu(advancedoptions, 4, BIOSMENU_SPEC_RETURN, &Menu_Stat); //Show the menu options!
	switch (menuresult)
	{
	case BIOSMENU_SPEC_CANCEL: //Return?
		BIOS_Menu = 8; //Goto Advanced Menu!
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
	case 9: //Valid option?
		switch (optioninfo[menuresult]) //What option has been chosen, since we are dynamic size?
		{
		case 0: //Soundfont selection?
			if (!EMU_RUNNING) BIOS_Menu = 32; //Soundfont setting!
			break;
		case 1: //PC Speaker?
			if (!EMU_RUNNING) BIOS_Menu = 44; //PC Speaker setting!
			break;
		case 2: //Adlib?
			if (!EMU_RUNNING) BIOS_Menu = 45; //Adlib setting!
			break;
		case 3: //LPT DAC?
			if (!EMU_RUNNING) BIOS_Menu = 46; //LPT DAC setting!
			break;
		case 4: //Game Blaster?
			if (!EMU_RUNNING) BIOS_Menu = 52; //Game Blaster setting!
			break;
		case 5: //Sound Blaster?
			if (!EMU_RUNNING) BIOS_Menu = 54; //Game Blaster setting!
			break;
		case 6: //Sound Source Volume?
			BIOS_Menu = 38; //Sound Source Volume setting!
			break;
		case 7: //Game Blaster Volume?
			BIOS_Menu = 53; //Game Blaster Volume setting!
			break;
		case 8: //Play Music file(s)?
			BIOS_Menu = 33; //Play Music file(s)!
			break;
		case 9: //Sound recording?
			BIOS_Menu = 42; //Start/stop sound recording!
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
	generateFileList(soundfontpath,"sf2", 0, 0); //Generate file list for all .sf2 files!
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
		if (strcmp(BIOS_Settings.SoundFont, itemlist[file])!=0) //Changed?
		{
			BIOS_Changed = 1; //Changed!
			reboot_needed = 1; //We need to reboot!
		}
		strcpy(BIOS_Settings.SoundFont, itemlist[file]); //Use this file!
		break;
	}
	BIOS_Menu = 31; //Return to the Sound menu!
}

int Sound_file = 0; //The file selected!

int BIOS_Sound_selection() //Music selection menu, custom for this purpose!
{
	BIOS_Title("Select a music file to play");
	generateFileList(musicpath,"mid|midi|dro", 0, 0); //Generate file list for all Sound files!
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Music file: "); //Show selection init!
	EMU_unlocktext();
	BIOS_EnablePlay = 1; //Enable Play=OK!
	int file = ExecuteList(12, 4, itemlist[Sound_file], 256,NULL); //Show menu for the disk image!
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

byte sound_playSoundfile(byte showinfo)
{
	char songpath[256];
	memset(songpath,0,sizeof(songpath)); //Init our path!
	Sound_file = 0; //Init selected file!
	for (;;) //Music selection loop!
	{
		Sound_file = BIOS_Sound_selection(); //Allow the user to select a Music file!
		if (Sound_file < 0) //Not selected?
		{
			Sound_file = 0;
			if (Sound_file == -2) //Default selected?
			{
				break; //Stop selection of the Music file!
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
		if (isext(&itemlist[Sound_file][0],"mid|midi")) //MIDI file?
		{
			strcpy(songpath,musicpath); //Load the path!
			strcat(songpath,"/");
			strcat(songpath,itemlist[Sound_file]); //The full filename!
			playMIDIFile(&songpath[0], showinfo); //Play the MIDI file!
		}
		else if (isext(&itemlist[Sound_file][0],"dro")) //DRO file?
		{
			strcpy(songpath, musicpath); //Load the path!
			strcat(songpath, "/");
			strcat(songpath, itemlist[Sound_file]); //The full filename!
			playDROFile(&songpath[0], showinfo); //Play the DRO file!
		}
		EMU_locktext();
		GPU_EMU_printscreen(0, GPU_TEXTSURFACE_HEIGHT - 1, "          "); //Show playing finished!
		EMU_unlocktext();
	}
	return 1; //Plain finish: just execute whatever you want!
}

void BIOS_MusicPlayer() //Music Player!
{
	sound_playSoundfile(0); //Play one or more Music files! Don't show any information!
	BIOS_Menu = 31; //Return to the Sound menu!
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
	switch (BIOS_Settings.PS2Mouse) //What setting?
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
			BIOS_Settings.PS2Mouse = file; //Select PS/2 Mouse setting!
		}
		break;
	}
	BIOS_Menu = 25; //Goto Input menu!
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
		strcat(menuoptions[advancedoptions++], "Intel 8086/8088"); //Add installed CPU!
		break;
	case CPU_NECV30: //NEC V20/V30?
		strcat(menuoptions[advancedoptions++], "NEC V20/V30"); //Add installed CPU!
		break;
	case CPU_80286: //80286?
		strcat(menuoptions[advancedoptions++], "Intel 80286(unfinished)"); //Add installed CPU!
		break;
	default:
		strcat(menuoptions[advancedoptions++], "<UNKNOWN. CHECK SETTINGS VERSION>"); //Add uninstalled CPU!
		break;
	}
setDataBusSize: //For fixing it!
	optioninfo[advancedoptions] = 1; //Data bus size!
	strcpy(menuoptions[advancedoptions], "Data bus size: ");
	switch (BIOS_Settings.DataBusSize) //Data bus size?
	{
	case 0:
		strcat(menuoptions[advancedoptions++], "16/32-bit data bus");
		break;
	case 1:
		strcat(menuoptions[advancedoptions++], "8-bit data bus when possible");
		break;
	default: //Error: fix it!
		BIOS_Settings.DataBusSize = 0; //Reset/Fix!
		BIOS_Changed = 1; //We've changed!
		goto setDataBusSize; //Goto!
		break;
	}

	optioninfo[advancedoptions] = 2; //Change CPU speed!
	strcpy(menuoptions[advancedoptions], "CPU Speed: ");
	switch (BIOS_Settings.CPUSpeed) //What CPU speed limit?
	{
	case 0: //Default cycles?
		strcat(menuoptions[advancedoptions++], "Default"); //Default!
		break;
	default: //Limited cycles?
		sprintf(menuoptions[advancedoptions], "%sLimited to %u cycles",menuoptions[advancedoptions],BIOS_Settings.CPUSpeed); //Cycle limit!
		++advancedoptions;
		break;
	}

setShowCPUSpeed:
	optioninfo[advancedoptions] = 3; //Change CPU speed!
	strcpy(menuoptions[advancedoptions], "Show CPU Speed: ");
	switch (BIOS_Settings.ShowCPUSpeed) //What CPU speed limit?
	{
	case 0: //No?
		strcat(menuoptions[advancedoptions++], "Disabled"); //Disabled!
		break;
	case 1: //Yes?
		strcat(menuoptions[advancedoptions++], "Enabled"); //Enabled!
		break;
	default: //Error: fix it!
		BIOS_Settings.ShowCPUSpeed = 0; //Reset/Fix!
		BIOS_Changed = 1; //We've changed!
		goto setShowCPUSpeed; //Goto!
		break;
	}

	optioninfo[advancedoptions] = 4; //Boot Order!
	strcpy(menuoptions[advancedoptions], "Boot Order: "); //Change boot order!
	strcat(menuoptions[advancedoptions++], BOOT_ORDER_STRING[BIOS_Settings.bootorder]); //Add boot order after!

	optioninfo[advancedoptions] = 5; //Execution mode!
	strcpy(menuoptions[advancedoptions], "Execution mode: ");
	switch (BIOS_Settings.executionmode) //What execution mode is active?
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
		strcat(menuoptions[advancedoptions++], "<UNKNOWN. CHECK SETTINGS VERSION>");
		break;
	}

	optioninfo[advancedoptions] = 6; //Debug mode!
	strcpy(menuoptions[advancedoptions], "Debug mode: ");
	switch (BIOS_Settings.debugmode) //What debug mode is active?
	{
	case DEBUGMODE_NONE:
		strcat(menuoptions[advancedoptions++], "No debugger enabled"); //Set filename from options!
		break;
	case DEBUGMODE_RTRIGGER:
		strcat(menuoptions[advancedoptions++], "Enabled, RTrigger=Step"); //Set filename from options!
		break;
	case DEBUGMODE_STEP:
		strcat(menuoptions[advancedoptions++], "Enabled, Step through"); //Set filename from options!
		break;
	case DEBUGMODE_SHOW_RUN:
		strcat(menuoptions[advancedoptions++], "Enabled, just run, ignore shoulder buttons"); //Set filename from options!
		break;
	default:
		strcat(menuoptions[advancedoptions++], "<UNKNOWN. CHECK SETTINGS VERSION>");
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
	case DEBUGGERLOG_INT: //Interrupt calls only
		strcat(menuoptions[advancedoptions++], "Interrupt calls only");
		break;
	default:
		strcat(menuoptions[advancedoptions++], "Never"); //Set filename from options!
		break;
	}
}

void BIOS_CPU() //CPU menu!
{
	BIOS_Title("CPU Settings Menu");
	BIOS_InitCPUText(); //Init text!
	int menuresult = BIOS_ShowMenu(advancedoptions, 4, BIOSMENU_SPEC_RETURN, &Menu_Stat); //Show the menu options!
	switch (menuresult)
	{
	case BIOSMENU_SPEC_CANCEL: //R: Main menu?
		BIOS_Menu = 8; //Goto Advanced Menu!
		break;

	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7: //Valid option?
		switch (optioninfo[menuresult]) //What option has been chosen, since we are dynamic size?
		{
		//CPU settings
		case 0: //Installed CPU?
			if (!EMU_RUNNING) BIOS_Menu = 10; //Installed CPU selection!
			break;
		case 1: //Data bus size?
			if (!EMU_RUNNING) BIOS_Menu = 40; //Data bus size!
			break;
		case 2: //CPU speed?
			BIOS_Menu = 36; //CPU speed selection!
			break;
		case 3: //CPU speed display setting?
			BIOS_Menu = 41; //CPU speed display setting!
			break;
		//Basic execution information
		case 4: //Boot order?
			BIOS_Menu = 9; //Boot Order Menu!
			break;
		case 5:
			BIOS_Menu = 24; //Execution mode option!
			break;
		//Debugger information
		case 6: //Debug mode?
			BIOS_Menu = 13; //Debug mode option!
			break;
		case 7:
			BIOS_Menu = 23; //Debugger log setting!
			break;
		}
		break;
	default: //Unknown option?
		BIOS_Menu = NOTIMPLEMENTED; //Not implemented yet!
		break;
	}
}

int_64 GetCPUSpeed(byte x, byte y, uint_32 CPUSpeed) //Retrieve the size, or 0 for none!
{
	int key = 0;
	key = psp_inputkeydelay(BIOS_INPUTDELAY);
	while ((key&BUTTON_CROSS)>0) //Pressed? Wait for release!
	{
		key = psp_inputkeydelay(BIOS_INPUTDELAY);
	}
	uint_32 result = CPUSpeed; //Size: result; default 0 for none! Must be a multiple of 4096 bytes for HDD!
	uint_32 oldvalue; //To check for high overflow!
	for (;;) //Get input; break on error!
	{
		EMU_locktext();
		EMU_textcolor(BIOS_ATTR_ACTIVE); //We're using active color for input!
		if (!result) //Default cycles?
		{
			GPU_EMU_printscreen(x, y, "Default cycles                                         ", result); //Show current size!
		}
		else
		{
			GPU_EMU_printscreen(x, y, "Limited to %u cycles", result); //Show current size!
		}
		EMU_unlocktext();
		key = psp_inputkeydelay(BIOS_INPUTDELAY); //Input key!
		
		//1GB steps!
		if ((key & BUTTON_LTRIGGER)>0) //1000 step down?
		{
			if (result == 0) {}
			else
			{
				oldvalue = result; //Load the old value!
				result -= (key&BUTTON_RIGHT) ? 100000 : ((key&BUTTON_LEFT) ? 10000 : 1000); //x100 or x10 or x1!
				if (result>oldvalue) result = 0; //Underflow!
			}
		}
		else if ((key & BUTTON_RTRIGGER)>0) //1000 step up?
		{
			oldvalue = result; //Save the old value!
			result += (key&BUTTON_RIGHT) ? 100000 : ((key&BUTTON_LEFT) ? 10000 : 1000); //x100 or x10 or x1!
			if (result < oldvalue) result = oldvalue; //We've overflown?
		}
		else if ((key & BUTTON_DOWN)>0) //1 step up?
		{
			if (result == 0) {}
			else
			{
				oldvalue = result;
				result -= (key&BUTTON_RIGHT)?100:((key&BUTTON_LEFT)?10:1); //x100 or x10 or x1!
				if (result>oldvalue) result = 0; //Underflow!
			}
		}
		else if ((key & BUTTON_UP)>0) //1 step down?
		{
			oldvalue = result; //Save the old value!
			result += (key&BUTTON_RIGHT) ? 100 : ((key&BUTTON_LEFT) ? 10 : 1); //x100 or x10 or x1!
			if (result < oldvalue) result = oldvalue; //We've overflown?
		}
		//Confirmation buttons etc.
		else if ((key & BUTTON_CROSS)>0)
		{
			while ((key&BUTTON_CROSS)>0) //Wait for release!
			{
				key = psp_inputkeydelay(BIOS_INPUTDELAY); //Input key!
			}
			return (int_64)result;
		}
		else if ((key & BUTTON_CIRCLE)>0)
		{
			while ((key&BUTTON_CIRCLE)>0) //Wait for release!
			{
				key = psp_inputkeydelay(BIOS_INPUTDELAY); //Input key!
			}
			break; //Cancel!
		}
		else if ((key & BUTTON_TRIANGLE)>0)
		{
			while ((key&BUTTON_TRIANGLE)>0) //Wait for release!
			{
				key = psp_inputkeydelay(BIOS_INPUTDELAY); //Input key!
			}
			return 0; //Default!
		}
		else if (shuttingdown()) break; //Cancel because of shutdown?
	}
	return FILELIST_CANCEL; //No size: cancel!
}

void BIOS_CPUSpeed() //CPU speed selection!
{
	BIOS_Title("CPU speed");
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "CPU speed: "); //Show selection init!
	EMU_unlocktext();
	int_64 file = GetCPUSpeed(11, 4, BIOS_Settings.CPUSpeed); //Show options for the CPU speed!
	switch (file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected speed!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = 0; //Default setting: Disabled!
	default: //Changed?
		if (file != BIOS_Settings.CPUSpeed) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.CPUSpeed = (uint_32)file; //Select CPU speed setting!
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
		memset(&BIOS_Settings.CMOS, 0, sizeof(BIOS_Settings.CMOS));
		BIOS_Settings.got_CMOS = 0; //We haven't gotten a CMOS!
		BIOS_Changed = 1; //We've changed!
		reboot_needed = 2; //We're needing a reboot!
	}
	BIOS_Menu = 8; //Goto Advanced Menu!
}

uint_32 GetPercentage(byte x, byte y, uint_32 Percentage) //Retrieve the size, or 0 for none!
{
	int key = 0;
	key = psp_inputkeydelay(BIOS_INPUTDELAY);
	while ((key&BUTTON_CROSS)>0) //Pressed? Wait for release!
	{
		key = psp_inputkeydelay(BIOS_INPUTDELAY);
	}
	uint_32 result = Percentage; //Size: result; default 0 for none! Must be a multiple of 4096 bytes for HDD!
	uint_32 oldvalue; //To check for high overflow!
	for (;;) //Get input; break on error!
	{
		EMU_locktext();
		EMU_textcolor(BIOS_ATTR_ACTIVE); //We're using active color for input!
		GPU_EMU_printscreen(x, y, "%i%%                                                      ", result); //Show current percentage!
		EMU_unlocktext();
		key = psp_inputkeydelay(BIOS_INPUTDELAY); //Input key!
												  //1GB steps!
		if ((key & BUTTON_LTRIGGER)>0) //1000 step down?
		{
			if (result == 0) {}
			else
			{
				oldvalue = result; //Load the old value!
				result -= (key&BUTTON_RIGHT) ? 100000 : ((key&BUTTON_LEFT) ? 10000 : 1000); //x100 or x10 or x1!
				if (result>oldvalue) result = 0; //Underflow!
			}
		}
		else if ((key & BUTTON_RTRIGGER)>0) //1000 step up?
		{
			oldvalue = result; //Save the old value!
			result += (key&BUTTON_RIGHT) ? 100000 : ((key&BUTTON_LEFT) ? 10000 : 1000); //x100 or x10 or x1!
			if (result < oldvalue) result = oldvalue; //We've overflown?
		}
		else if ((key & BUTTON_DOWN)>0) //1 step up?
		{
			if (result == 0) {}
			else
			{
				oldvalue = result;
				result -= (key&BUTTON_RIGHT) ? 100 : ((key&BUTTON_LEFT) ? 10 : 1); //x100 or x10 or x1!
				if (result>oldvalue) result = 0; //Underflow!
			}
		}
		else if ((key & BUTTON_UP)>0) //1 step down?
		{
			oldvalue = result; //Save the old value!
			result += (key&BUTTON_RIGHT) ? 100 : ((key&BUTTON_LEFT) ? 10 : 1); //x100 or x10 or x1!
			if (result < oldvalue) result = oldvalue; //We've overflown?
		}
		//Confirmation buttons etc.
		else if ((key & BUTTON_CROSS)>0)
		{
			while ((key&BUTTON_CROSS)>0) //Wait for release!
			{
				key = psp_inputkeydelay(BIOS_INPUTDELAY); //Input key!
			}
			return result; //Convert back to an ordinary factor!
		}
		else if ((key & BUTTON_CIRCLE)>0)
		{
			while ((key&BUTTON_CIRCLE)>0) //Wait for release!
			{
				key = psp_inputkeydelay(BIOS_INPUTDELAY); //Input key!
			}
			break; //Cancel!
		}
		else if ((key & BUTTON_TRIANGLE)>0)
		{
			while ((key&BUTTON_TRIANGLE)>0) //Wait for release!
			{
				key = psp_inputkeydelay(BIOS_INPUTDELAY); //Input key!
			}
			return 0; //Default!
		}
		else if (shuttingdown()) break; //Cancel because of shutdown?
	}
	return FILELIST_CANCEL; //No size: cancel!
}

void BIOS_SoundSourceVolume()
{
	BIOS_Title("Sound Source Volume");
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Sound Source Volume: "); //Show selection init!
	EMU_unlocktext();
	uint_32 file = GetPercentage(21, 4, BIOS_Settings.SoundSource_Volume); //Show options for the installed CPU!
	switch ((int)file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected percentage!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = 0; //Default setting: Quiet!
	default: //Changed?
		if (file != BIOS_Settings.SoundSource_Volume) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.SoundSource_Volume = file; //Select Sound Source Volume setting!
		}
		break;
	}
	BIOS_Menu = 31; //Goto Advanced menu!
}

void BIOS_ShowFramerate()
{
	BIOS_Settings.ShowFramerate = !BIOS_Settings.ShowFramerate; //Reverse!
	BIOS_Changed = 1; //We've changed!
	BIOS_Menu = 29; //Goto Video Settings menu!
}

void BIOS_DataBusSizeSetting()
{
	BIOS_Title("Data bus size");
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Data bus size: "); //Show selection init!
	EMU_unlocktext();
	int i = 0; //Counter!
	numlist = 2; //Ammount of Direct modes!
	for (i = 0; i<numlist; i++) //Process options!
	{
		bzero(itemlist[i], sizeof(itemlist[i])); //Reset!
	}
	strcpy(itemlist[0], "16/32-bit data bus"); //Set filename from options!
	strcpy(itemlist[1], "8-bit data bus when possible"); //Set filename from options!
	int current = 0;
	switch (BIOS_Settings.DataBusSize) //What setting?
	{
	case 0: //Valid
	case 1: //Valid
		current = BIOS_Settings.DataBusSize; //Valid: use!
		break;
	default: //Invalid
		current = 0; //Default: none!
		break;
	}
	if (BIOS_Settings.DataBusSize != current) //Invalid?
	{
		BIOS_Settings.DataBusSize = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int file = ExecuteList(15, 4, itemlist[current], 256, NULL); //Show options for the installed CPU!
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
			reboot_needed = 1; //A reboot is needed!
			BIOS_Settings.DataBusSize = file; //Select Data bus size setting!
		}
		break;
	}
	BIOS_Menu = 35; //Goto CPU menu!
}

void BIOS_ShowCPUSpeed()
{
	BIOS_Settings.ShowCPUSpeed = !BIOS_Settings.ShowCPUSpeed; //Reverse!
	BIOS_Changed = 1; //We've changed!
	BIOS_Menu = 35; //Goto CPU menu!
}

void BIOS_SoundStartStopRecording()
{
	if (sound_isRecording()) //Are we recording?
	{
		sound_stopRecording(); //Stop recording!
		#ifdef SOUND_TEST
		PORT_OUT_B(0x330,0xB0); //Control change!
		PORT_OUT_B(0x330,0x7B); //All notes off!
		PORT_OUT_B(0x330,0x00); //Nothing more!

		PORT_OUT_B(0x331,0xFF); //Reset ourselves!
		PORT_OUT_B(0x330,0xFF); //Reset ourselves!
		#endif
	}
	else
	{
		sound_startRecording(); //Start recording!
		#ifdef SOUND_TEST
		PORT_OUT_B(0x331,0xFF); //Reset ourselves!
		PORT_OUT_B(0x330,0xFF); //Reset ourselves!

		PORT_OUT_B(0x330,0xB0); //Control change!
		PORT_OUT_B(0x330,0x00); //Bank change MSB!
		PORT_OUT_B(0x330,0x00); //Bank change 0!
		PORT_OUT_B(0x330,0x20); //Bank change LSB!
		PORT_OUT_B(0x330,0x00); //Bank change 0!

		PORT_OUT_B(0x330,0xC0); //Instrument change!
		PORT_OUT_B(0x330,73); //Instrument change to flute!

		PORT_OUT_B(0x330,0x90); //Start note!
		PORT_OUT_B(0x330,0x60); //Central C+32 tones!
		PORT_OUT_B(0x330,0x40); //At default volume!
		#endif
	}
	BIOS_Menu = 31; //Goto Sound menu!
}


extern FLOPPY_GEOMETRY floppygeometries[NUMFLOPPYGEOMETRIES]; //All possible floppy geometries to create!

void BIOS_GenerateFloppyDisk()
{
	char fullfilename[256];
	word size; //The size to generate, in KB!
	byte i;
	char filename[256]; //Filename container!
	bzero(filename, sizeof(filename)); //Init!
	for (i=0;i<NUMFLOPPYGEOMETRIES;i++) //Process all geometries into a list!
	{
		bzero(itemlist[i],sizeof(itemlist[i])); //Reset!
		if (floppygeometries[i].KB>=1024) //1024K+?
		{
			if (floppygeometries[i].measurement) //3.5"?
			{
				if (floppygeometries[i].KB%1000) //Not whole MB?
				{
					if (floppygeometries[i].KB%10) //3 digits?
					{
						sprintf(itemlist[i],"%01.3fMB disk 3.5\"",floppygeometries[i].KB/1000.0f); //Disk!
					}
					else if (floppygeometries[i].KB%100) //2 digits?
					{
						sprintf(itemlist[i],"%01.2fMB disk 3.5\"",floppygeometries[i].KB/1000.0f); //Disk!
					}
					else //1 digit?
					{
						sprintf(itemlist[i],"%01.1fMB disk 3.5\"",floppygeometries[i].KB/1000.0f); //Disk!
					}
				}
				else //Whole MB?
				{
					sprintf(itemlist[i],"%uMB disk 3.5\"",(uint_32)(floppygeometries[i].KB/1000)); //Disk!
				}
			}
			else //5.25"?
			{
				if (floppygeometries[i].KB%1000) //Not whole MB?
				{
					if (floppygeometries[i].KB%10) //3 digits?
					{
						sprintf(itemlist[i],"%01.3fMB disk 5.25\"",floppygeometries[i].KB/1000.0f); //Disk!
					}
					else if (floppygeometries[i].KB%100) //2 digits?
					{
						sprintf(itemlist[i],"%01.2fMB disk 5.25\"",floppygeometries[i].KB/1000.0f); //Disk!
					}
					else //1 digit?
					{
						sprintf(itemlist[i],"%01.1fMB disk 5.25\"",floppygeometries[i].KB/1000.0f); //Disk!
					}
				}
				else //Whole MB?
				{
					sprintf(itemlist[i],"%uMB disk 5.25\"",(uint_32)(floppygeometries[i].KB/1000)); //Disk!
				}
			}
		}
		else //<1MB?
		{
			if (floppygeometries[i].measurement) //3.5"?
			{
				sprintf(itemlist[i],"%uKB disk 3.5\"",floppygeometries[i].KB); //Disk!
			}
			else //5.25"?
			{
				sprintf(itemlist[i],"%uKB disk 5.25\"",floppygeometries[i].KB); //Disk!
			}
		}
	}
	numlist = NUMFLOPPYGEOMETRIES; //The size of the list!

	BIOS_Title("Generate floppy image");
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Floppy image size: "); //Show selection init!
	EMU_unlocktext();
	int result;
	result = ExecuteList(19,4,itemlist[0],256,NULL); //Get our result!
	if ((result>=0) && (result<NUMFLOPPYGEOMETRIES)) //Valid item?
	{
		EMU_locktext();
		EMU_gotoxy(0, 4); //Goto position for info!
		GPU_EMU_printscreen(0, 5, "Name: "); //Show the filename!
		EMU_unlocktext();
		if (BIOS_InputText(6, 5, &filename[0], 255-4)) //Input text confirmed?
		{
			if (strcmp(filename, "") != 0) //Got input?
			{
				if (strlen(filename) <= (255 - 4)) //Not too long?
				{
					strcat(filename, ".img"); //Add the extension!
					EMU_locktext();
					EMU_gotoxy(0, 5); //Goto position for info!
					GPU_EMU_printscreen(0, 5, "Filename: %s", filename); //Show the filename!
					EMU_gotoxy(0, 5); //Next row!
					GPU_EMU_printscreen(0, 6, "Image size: "); //Show image size selector!!
					EMU_unlocktext();
					size = floppygeometries[result].KB; //The size of the floppy in KB!
					if (size != 0) //Got size?
					{
						EMU_locktext();
						GPU_EMU_printscreen(12, 6, "%s", itemlist[result]); //Show size we selected!
						EMU_gotoxy(0, 6); //Next row!
						GPU_EMU_printscreen(0, 7, "Generating image: "); //Start of percentage!
						EMU_unlocktext();

						memset(fullfilename, 0, sizeof(fullfilename));
						strcpy(fullfilename, diskpath);
						strcat(fullfilename, "/");
						strcat(fullfilename, filename);

						generateFloppyImage(fullfilename, &floppygeometries[result], 18, 7); //Generate a floppy image according to geometry data!
						//Check for disk changes on mounted floppy disks (we might be getting a new size, when we're recreaten)!
						if (!memcmp(BIOS_Settings.floppy0,filename,sizeof(BIOS_Settings.floppy0))) //Floppy #0 changed?
						{
							iofloppy0("",0,BIOS_Settings.floppy0_readonly,0); //Unmount!
							iofloppy0(BIOS_Settings.floppy0,0,BIOS_Settings.floppy0_readonly,0); //Remount to update!
						}
						if (!memcmp(BIOS_Settings.floppy1,filename,sizeof(BIOS_Settings.floppy1))) //Floppy #1 changed?
						{
							iofloppy1("",0,BIOS_Settings.floppy1_readonly,0); //Unmount!
							iofloppy1(BIOS_Settings.floppy1,0,BIOS_Settings.floppy1_readonly,0); //Remount to update!
						}
					}
				}
				//If we're too long, ignore it!
			}
		}
	}
	BIOS_Menu = 1; //Return to Disk Menu!
}

void BIOS_usePCSpeaker()
{
	BIOS_Settings.usePCSpeaker = !BIOS_Settings.usePCSpeaker; //Reverse!
	BIOS_Changed = 1; //We've changed!
	reboot_needed = 1; //A reboot is needed!
	BIOS_Menu = 31; //Goto Sound menu!
}

void BIOS_useAdlib()
{
	BIOS_Settings.useAdlib = !BIOS_Settings.useAdlib; //Reverse!
	BIOS_Changed = 1; //We've changed!
	reboot_needed = 1; //A reboot is needed!
	BIOS_Menu = 31; //Goto Sound menu!
}

void BIOS_useLPTDAC()
{
	BIOS_Settings.useLPTDAC = !BIOS_Settings.useLPTDAC; //Reverse!
	BIOS_Changed = 1; //We've changed!
	reboot_needed = 1; //A reboot is needed!
	BIOS_Menu = 31; //Goto Sound menu!
}

void BIOS_VGASynchronization()
{
	BIOS_Title("VGA Synchronization");
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "VGA Synchronization: "); //Show selection init!
	EMU_unlocktext();
	int i = 0; //Counter!
	numlist = 3; //Ammount of Synchronization modes!
	for (i = 0; i<numlist; i++) //Process options!
	{
		bzero(itemlist[i], sizeof(itemlist[i])); //Reset!
	}
	strcpy(itemlist[0], "Old synchronization depending on host"); //Set filename from options!
	strcpy(itemlist[1], "Synchronize depending on host"); //Set filename from options!
	strcpy(itemlist[2], "Full CPU synchronization"); //Set filename from options!
	int current = 0;
	switch (BIOS_Settings.VGASynchronization) //What setting?
	{
	case 0: //Valid
	case 1: //Valid
	case 2: //Valid
		current = BIOS_Settings.VGASynchronization; //Valid: use!
		break;
	default: //Invalid
		current = 0; //Default: none!
		break;
	}
	if (BIOS_Settings.VGASynchronization != current) //Invalid?
	{
		BIOS_Settings.VGASynchronization = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int file = ExecuteList(21, 4, itemlist[current], 256, NULL); //Show options for the installed CPU!
	switch (file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = 0; //Default setting: Disabled!

	case 0:
	case 1:
	case 2:
	default: //Changed?
		if (file != current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.VGASynchronization = file; //Select Data bus size setting!
		}
		break;
	}
	BIOS_Menu = 29; //Goto Video menu!
}

void BIOS_DumpVGA()
{
	FILE *f;
	int DACIndex;
	uint_32 DACPos;
	BIOS_Title("Dumping VGA data");
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Dumping VGA..."); //Show selection init!
	EMU_unlocktext();

	//Now dump the VGA data...
	VGA_Type *VGA = getActiveVGA(); //Get the current VGA!
	if (VGA) //Valid VGA?
	{
		domkdir("captures"); //Make sure to create the directory we need!
		f = fopen("captures/vga_vram.dat","wb");
		if (f)
		{
			fwrite(VGA->VRAM,1,VGA->VRAM_size,f); //Write the VRAM to the file!
			fclose(f); //We've written the VRAM to the file!
		}
		f = fopen("captures/vga_graphregs.dat","wb");
		if (f)
		{
			fwrite(&VGA->registers->GraphicsRegisters.DATA,1,sizeof(VGA->registers->GraphicsRegisters.DATA),f);
			fclose(f); //We've written the Graphics Registers to the file!
		}
		f = fopen("captures/vga_seqregs.dat","wb");
		if (f)
		{
			fwrite(&VGA->registers->SequencerRegisters.DATA,1,sizeof(VGA->registers->SequencerRegisters.DATA),f);
			fclose(f); //We've written the Sequencer Registers to the file!
		}

		f = fopen("captures/vga_attrregs.dat","wb");
		if (f)
		{
			fwrite(&VGA->registers->AttributeControllerRegisters.DATA,1,sizeof(VGA->registers->AttributeControllerRegisters.DATA),f);
			fclose(f); //We've written the Attribute Controller Registers to the file!
		}

		f = fopen("captures/vga_crtcregs.dat","wb");
		if (f)
		{
			fwrite(&VGA->registers->CRTControllerRegisters.DATA,1,sizeof(VGA->registers->CRTControllerRegisters.DATA),f);
			fclose(f); //We've written the Graphics Registers to the file!
		}

		f = fopen("captures/vga_dacregs.dat","wb");
		if (f)
		{
			DACPos = 0; //Start with the first entry!
			for (DACIndex=0;DACIndex<0x100;DACIndex++) //Process DAC entries!
			{
				fwrite(&VGA->registers->DAC[DACPos++],1,1,f); //Write the DAC R!
				fwrite(&VGA->registers->DAC[DACPos++],1,1,f); //Write the DAC G!
				fwrite(&VGA->registers->DAC[DACPos++],1,1,f); //Write the DAC B!
				++DACPos; //Skip the DAC entry for the fourth entry: we're unused!
			}
			fwrite(&VGA->registers->DACMaskRegister,1,1,f); //Finish with the DAC mask register!
			fclose(f); //We've written the Graphics Registers to the file!
		}
		
		f = fopen("captures/vga_colorregs.dat","wb");
		fwrite(&VGA->registers->ColorRegisters,1,sizeof(VGA->registers->ColorRegisters),f); //Literal color registers!
		fclose(f);

		f = fopen("captures/vga_externalregs.dat","wb");
		fwrite(&VGA->registers->ExternalRegisters,1,sizeof(VGA->registers->ExternalRegisters),f); //Literal color registers!
		fclose(f);

		if (VGA->registers->specialCGAflags&1) //CGA compatiblity enabled?
		{
			f = fopen("captures/vga_cgamdacrtcregs.dat","wb");
			fwrite(&VGA->registers->CGARegisters,1,sizeof(VGA->registers->CGARegisters),f); //CGA CRTC registers!
			fclose(f);

			f = fopen("captures/vga_cgamodecontrol.dat","wb");
			fwrite(&VGA->registers->Compatibility_CGAModeControl,1,1,f); //CGA mode control register!
			fclose(f);
			
			f = fopen("captures/vga_cgapaletteregister.dat","wb");
			fwrite(&VGA->registers->Compatibility_CGAPaletteRegister,1,1,f); //CGA mode control register!
			fclose(f);

			f = fopen("captures/vga_mdamodecontrol.dat","wb");
			fwrite(&VGA->registers->Compatibility_MDAModeControl,1,1,f); //MDA mode control register!
			fclose(f);
		}
		else //Clean up CGA compatiblity register dumps: we're not supposed to be used!
		{
			delete_file("captures","vga_cgamdacrtcregs.dat");
			delete_file("captures","vga_cgamodecontrol.dat");
			delete_file("captures","vga_cgapaletteregister.dat");
			delete_file("captures","vga_mdamodecontrol.dat");
		}

		VGA_DUMPColors(); //Dump all colors!
		dumpVGATextFonts(); //Dump all fonts used!
		dump_CRTCTiming(); //Dump all CRTC timing currently in use!

	#ifdef DUMP_VGATEST256COL
		uint_32 rowwidth = (getActiveVGA()->precalcs.rowsize<<2); //The row width, in bytes and pixels!
		uint_32 activewidth = (getActiveVGA()->precalcs.horizontaldisplayend-getActiveVGA()->precalcs.horizontaldisplaystart); //Width of the active display!
		uint_32 activeheight = getActiveVGA()->precalcs.verticaldisplayend; //The height of the active display!
		activewidth = MAX(rowwidth,activewidth); //Take the bigger one, if any!
		uint_32 *pixels = (uint_32 *)zalloc(((activeheight*activewidth)<<2),"BMPDATA",NULL); //To draw our bitmap on!
		int x,y;
		for (y = 0;y<activeheight;) //Vertical active display!
		{
			for (x = 0;x < activewidth;) //Horizontal active display!
			{
				pixels[(y*activewidth)+x] = getActiveVGA()->precalcs.effectiveDAC[getActiveVGA()->VRAM[((y*rowwidth)+x)&(getActiveVGA()->VRAM_size-1)]]; //Linear VRAM assumed, converted through the DAC to a color!
				++x; //Next pixel!
			}
			++y; //Next row!
		}
		writeBMP("captures/VGA256col",pixels,activewidth,activeheight,0,0,activewidth); //Dump the VRAM direct to test!
		freez((void **)&pixels, ((activeheight*activewidth) << 2),"BMPDATA"); //Release the temporary data!
	#endif
	}

	BIOS_Menu = 29; //Goto Video menu!
}

void BIOS_CGAModel()
{
	BIOS_Title("CGA Model");
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "CGA Model: "); //Show selection init!
	EMU_unlocktext();
	int i = 0; //Counter!
	numlist = 4; //Ammount of CGA Models!
	for (i = 0; i<numlist; i++) //Process options!
	{
		bzero(itemlist[i], sizeof(itemlist[i])); //Reset!
	}
	strcpy(itemlist[0], "Old-style RGB"); //Old-style RGB!
	strcpy(itemlist[1], "Old-style NTSC"); //Old-style NTSC!
	strcpy(itemlist[2], "New-style RGB"); //New-style RGB!
	strcpy(itemlist[3], "New-style NTSC"); //New-style NTSC!
	int current = 0;
	switch (BIOS_Settings.CGAModel) //What setting?
	{
	case 0: //Valid
	case 1: //Valid
	case 2: //Valid
	case 3: //Valid
		current = BIOS_Settings.CGAModel; //Valid: use!
		break;
	default: //Invalid
		current = 0; //Default to the first option!
		break;
	}
	if (BIOS_Settings.CGAModel != current) //Invalid?
	{
		BIOS_Settings.CGAModel = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int file = ExecuteList(11, 4, itemlist[current], 256, NULL); //Show options for the installed CPU!
	switch (file) //Which file?
	{
	default: //Unknown result?
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = 0; //Default setting!

	case 0:
	case 1:
	case 2:
	case 3:
		if (file != current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.CGAModel = file; //Select Data bus size setting!
		}
		break;
	}
	BIOS_Menu = 29; //Goto Video menu!
}

void BIOS_gamingmodeJoystick()
{
	BIOS_Title("Gaming mode");
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Gaming mode: "); //Show selection init!
	EMU_unlocktext();
	int i = 0; //Counter!
	numlist = 6; //Ammount of Joysticks supported plus Gaming mode mapping!
	for (i = 0; i<numlist; i++) //Process options!
	{
		bzero(itemlist[i], sizeof(itemlist[i])); //Reset!
	}
	strcpy(itemlist[0], "Normal gaming mode mapped input"); //Default to mapped input!
	strcpy(itemlist[1], "Joystick, Cross=Button 1, Circle=Button 2"); //Joystick: Cross=Button 1, Circle=Button 2!
	strcpy(itemlist[2], "Joystick, Cross=Button 2, Circle=Button 1"); //Joystick: Cross=Button 2, Circle=Button 1!
	strcpy(itemlist[3], "Joystick, Gravis Gamepad"); //Gravis Gamepad!
	strcpy(itemlist[4], "Joystick, Gravis Analog Pro"); //Gravis Analog Pro!
	strcpy(itemlist[5], "Joystick, Logitech WingMan Extreme Digital"); //Logitech WingMan Extreme Digital!
	int current = 0;
	switch (BIOS_Settings.input_settings.gamingmode_joystick) //What setting?
	{
	case 0: //Valid
	case 1: //Valid
	case 2: //Valid
	case 3: //Valid
	case 4: //Valid
	case 5: //Valid
		current = BIOS_Settings.input_settings.gamingmode_joystick; //Valid: use!
		break;
	default: //Invalid
		current = 0; //Default to the first option!
		break;
	}
	if (BIOS_Settings.input_settings.gamingmode_joystick != current) //Invalid?
	{
		BIOS_Settings.input_settings.gamingmode_joystick = current; //Safety!
		BIOS_Changed = 1; //Changed!
	}
	int file = ExecuteList(13, 4, itemlist[current], 256, NULL); //Show options for the installed CPU!
	switch (file) //Which file?
	{
	default: //Unknown result?
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected disk!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = 0; //Default setting!

	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		if (file != current) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.input_settings.gamingmode_joystick = file; //Select Gaming mode Joystick setting!
		}
		break;
	}
	BIOS_Menu = 25; //Goto Input menu!
}

void BIOS_JoystickReconnect()
{
	reconnectJoystick0(); //Reconnect joystick #0!
	BIOS_Menu = 25; //Goto Input menu!
}

void BIOS_useGameBlaster()
{
	BIOS_Settings.useGameBlaster = !BIOS_Settings.useGameBlaster; //Reverse!
	BIOS_Changed = 1; //We've changed!
	reboot_needed = 1; //A reboot is needed!
	BIOS_Menu = 31; //Goto Sound menu!
}

void BIOS_GameBlasterVolume()
{
	BIOS_Title("Game Blaster Volume");
	EMU_locktext();
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(BIOS_ATTR_INACTIVE); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "Game Blaster Volume: "); //Show selection init!
	EMU_unlocktext();
	uint_32 file = GetPercentage(21, 4, BIOS_Settings.GameBlaster_Volume); //Show options for the installed CPU!
	switch ((int)file) //Which file?
	{
	case FILELIST_CANCEL: //Cancelled?
		//We do nothing with the selected percentage!
		break; //Just calmly return!
	case FILELIST_DEFAULT: //Default?
		file = 0; //Default setting: Quiet!
	default: //Changed?
		if (file != BIOS_Settings.GameBlaster_Volume) //Not current?
		{
			BIOS_Changed = 1; //Changed!
			BIOS_Settings.GameBlaster_Volume = file; //Select Sound Source Volume setting!
		}
		break;
	}
	BIOS_Menu = 31; //Goto Sound menu!
}

void BIOS_useSoundBlaster()
{
	BIOS_Settings.useSoundBlaster = !BIOS_Settings.useSoundBlaster; //Reverse!
	BIOS_Changed = 1; //We've changed!
	reboot_needed = 1; //A reboot is needed!
	BIOS_Menu = 31; //Goto Sound menu!
}