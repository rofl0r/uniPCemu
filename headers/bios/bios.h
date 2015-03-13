#ifndef BIOS_H
#define BIOS_H

#include "headers/bios/biosmenu.h" //BIOS menu comp.
#include "headers/emu/input.h" //For INPUT_SETTINGS!
//First, the typedefs:

//Delay between steps!
#define BIOS_INPUTDELAY 250000

//BIOS Version!
#define BIOS_VERSION 1

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{
	union PACKED
	{
		struct PACKED
		{
			byte version; //The version number of the BIOS; should match with current version number!

//First, the mounted harddisks:
			char floppy0[256];
			char floppy1[256];
			char hdd0[256];
			char hdd1[256];
			char cdrom0[256];
			char cdrom1[256];

			int floppy0_readonly; //read-only?
			int floppy1_readonly; //read-only?
			int hdd0_readonly; //read-only?
			int hdd1_readonly; //read-only?

			uint_32 memory; //Memory used by the emulator!
			word emulated_CPU; //Emulated CPU?

			byte bootorder; //Boot order?
			byte debugmode; //What debug mode?
			byte debugger_log; //Log when using the debugger?

			INPUT_SETTINGS input_settings; //Settings for input!

			byte VGA_AllowDirectPlot; //Allow VGA Direct Plot: 1 for automatic 1:1 mapping, 0 for always dynamic, 2 for force 1:1 mapping?
			uint_32 VRAM_size; //VGA VRAM size!
			byte bwmonitor; //Are we a b/w monitor?
			byte keepaspectratio; //Keep the aspect ratio?

			byte BIOSmenu_font; //The selected font for the BIOS menu!
			byte firstrun; //Is this the first run of this BIOS?

			byte CMOS[0x80]; //The full saved CMOS!
			byte got_CMOS; //Gotten an CMOS?

			byte executionmode; //What mode to execute in during runtime?
		}; //Containing the data of the BIOS
		byte data[1701]; //Data for checksum!
	};
//Rest settings!
} BIOS_Settings_TYPE; //BIOS Settings!
#include "headers/endpacked.h" //We're packed!

//Debug modes:
//None:
#define DEBUGMODE_NONE 0
//Show, RTrigger=Step
#define DEBUGMODE_RTRIGGER 1
//Show, Step through.
#define DEBUGMODE_STEP 2
//Show, just run, ignore RTRIGGER buttons.
#define DEBUGMODE_SHOW_RUN 3

//Debugger log:
//None:
#define DEBUGGERLOG_NONE 0
//Debugging only:
#define DEBUGGERLOG_DEBUGGING 1
//Always
#define DEBUGGERLOG_ALWAYS 2

//Execution modes:
//None:
#define EXECUTIONMODE_NONE 0
//Run advanced debugger (debug / *.*):
#define EXECUTIONMODE_TEST 1
//Run bootrom.dat
#define EXECUTIONMODE_TESTROM 2
//VIDEO CARD MODE debug!
#define EXECUTIONMODE_VIDEOCARD 3
//Load external BIOS (at the BIOS address) and run it!
#define EXECUTIONMODE_BIOS 4
//Run sound test!
#define EXECUTIONMODE_SOUND 5
//Load external BIOS (at the BIOS adress) and run it, enable debugger!

//To debug the text mode characters on-screen?
#define DEBUG_VIDEOCARD (BIOS_Settings.executionmode==EXECUTIONMODE_VIDEOCARD)

//B/W monitor setting:
//Color mode
#define BWMONITOR_NONE 0
#define BWMONITOR_BLACK 1
#define BWMONITOR_GREEN 2
#define BWMONITOR_BROWN 3

void BIOS_LoadIO(int showchecksumerrors); //Loads basic I/O drives from BIOS!
void BIOS_ShowBIOS(); //Shows mounted drives etc!
void BIOS_ValidateDisks(); //Validate all disks and eject wrong ones!
void BIOS_LoadDefaults(int tosave); //Load BIOS defaults!

void BIOS_LoadData(); //Loads the data!
int BIOS_SaveData(); //Save BIOS settings!

//Stuff for other units:
uint_32 BIOS_GetMMUSize(); //For MMU!
int boot_system(); //Tries to boot using BIOS Boot Order. TRUE on success, FALSE on error/not booted.

void autoDetectMemorySize(int tosave); //Autodetect memory size! (tosave=To save BIOS?)
void forceBIOSSave(); //Forces BIOS to save!

void BIOSKeyboardInit(); //BIOS part of keyboard initialisation!

#endif
