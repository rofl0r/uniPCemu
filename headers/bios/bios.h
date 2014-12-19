#ifndef BIOS_H
#define BIOS_H

#include "headers/bios/biosmenu.h" //BIOS menu comp.
#include "headers/emu/input.h" //For INPUT_SETTINGS!
//First, the typedefs:

//BIOS Version!
#define BIOS_VERSION 1

typedef struct __attribute((packed))
{
	union __attribute((packed))
	{
		struct __attribute((packed))
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

			INPUT_SETTINGS input_settings; //Settings for input!

			byte VGA_AllowDirectPlot; //Allow VGA Direct Plot: 1 for automatic 1:1 mapping, 0 for always dynamic, 2 for force 1:1 mapping?
			uint_32 VRAM_size; //VGA VRAM size!
			byte keepaspectratio; //Keep the aspect ratio?

			byte BIOSmenu_font; //The selected font for the BIOS menu!
			byte firstrun; //Is this the first run of this BIOS?

			byte CMOS[0x80]; //The full saved CMOS!
			byte got_CMOS; //Gotten an CMOS?
		}; //Containing the data of the BIOS
		byte data[1700]; //Data for checksum!
	};
//Rest settings!
} BIOS_Settings_TYPE; //BIOS Settings!

//Debug modes:
//None:
#define DEBUGMODE_NONE 0
//Show, RTrigger=Step
#define DEBUGMODE_RTRIGGER 1
//Show, Step through.
#define DEBUGMODE_STEP 2
//Show, just run, ignore RTRIGGER buttons.
#define DEBUGMODE_SHOW_RUN 3
//Run advanced debugger (debug/*.* OR bootrom.dat)
#define DEBUGMODE_TEST 4
//Special case: TEST mode (see above) with forced STEP!
#define DEBUGMODE_TEST_STEP 5
//Special case: TEXT MODE debug!
#define DEBUGMODE_TEXT 6
//Special case: Load external BIOS (at the BIOS address) and run it!
#define DEBUGMODE_BIOS 7
//Special case: Run sound test!
#define DEBUGMODE_SOUND 8

//To debug the text mode characters on-screen?
#define DEBUG_TEXTMODE (BIOS_Settings.debugmode==DEBUGMODE_TEXT)

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

//Delay between steps!
#define BIOS_INPUTDELAY 250000

#endif