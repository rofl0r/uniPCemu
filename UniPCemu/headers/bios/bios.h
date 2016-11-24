#ifndef BIOS_H
#define BIOS_H

#include "headers/emu/input.h" //For INPUT_SETTINGS!
#include "headers/hardware/cmos.h" //CMOS support!
//First, the typedefs:

//Delay between steps!
#define BIOS_INPUTDELAY 250000

//BIOS Version!
#define BIOS_VERSION 8

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{
	byte version; //The version number of the BIOS; should match with current version number!

//First, the mounted harddisks:
	CharacterType floppy0[256];
	CharacterType floppy1[256];
	CharacterType hdd0[256];
	CharacterType hdd1[256];
	CharacterType cdrom0[256];
	CharacterType cdrom1[256];
	CharacterType SoundFont[256]; //What soundfont to use?

	byte floppy0_readonly; //read-only?
	byte floppy1_readonly; //read-only?
	byte hdd0_readonly; //read-only?
	byte hdd1_readonly; //read-only?

	uint_32 memory; //Memory used by the emulator!
	word emulated_CPU; //Emulated CPU?

	byte bootorder; //Boot order?
	byte debugmode; //What debug mode?
	byte debugger_log; //Log when using the debugger?

	INPUT_SETTINGS input_settings; //Settings for input!

	byte VGA_AllowDirectPlot; //Allow VGA Direct Plot: 1 for automatic 1:1 mapping, 0 for always dynamic, 2 for force 1:1 mapping?
	uint_32 VRAM_size; //(S)VGA VRAM size!
	byte bwmonitor; //Are we a b/w monitor?
	byte aspectratio; //The aspect ratio to use?

	byte BIOSmenu_font; //The selected font for the BIOS menu!
	byte firstrun; //Is this the first run of this BIOS?

	CMOSDATA CMOS; //The full saved CMOS!
	byte got_CMOS; //Gotten an CMOS?

	byte executionmode; //What mode to execute in during runtime?
	byte VGA_Mode; //Enable VGA NMI on precursors?
	byte architecture; //Are we using the XT/AT/PS/2 architecture?
	uint_32 CPUSpeed;
	uint_32 SoundSource_Volume; //The sound source volume knob!
	byte ShowFramerate; //Show the frame rate?
	byte DataBusSize; //The size of the emulated BUS. 0=Normal bus, 1=8-bit bus when available for the CPU!
	byte ShowCPUSpeed; //Show the relative CPU speed together with the framerate?
	byte usePCSpeaker; //Emulate PC Speaker sound?
	byte useAdlib; //Emulate Adlib?
	byte useLPTDAC; //Emulate Covox/Disney Sound Source?
	byte VGASynchronization; //VGA synchronization setting. 0=Automatic synchronization based on Host CPU. 1=Tight VGA Synchronization with the CPU.
	byte CGAModel; //What kind of CGA is emulated? Bit0=NTSC, Bit1=New-style CGA
	byte useGameBlaster; //Emulate Game Blaster?
	uint_32 GameBlaster_Volume; //The Game Blaster volume knob!
	byte useSoundBlaster; //Emulate Sound Blaster?
	uint_32 TurboCPUSpeed;
	byte useTurboSpeed; //Are we to use Turbo CPU speed?
	sword diagnosticsportoutput_breakpoint; //Use a diagnostics port breakpoint?
	uint_32 diagnosticsportoutput_timeout; //Breakpoint timeout used!
	byte CPUSpeedMode; //CPU Speed mode. 0=Instructions per millisecond, 1=1kHz cycles per second.
	byte TurboCPUSpeedMode; //Turbo CPU Speed mode. 0=Instructions per millisecond, 1=1kHz cycles per second.
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
//Interrupt calls only
#define DEBUGGERLOG_INT 3
//Debug POST Diagnostic codes only
#define DEBUGGERLOG_DIAGNOSTICCODES 4

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

//Architecture possibilities
enum Architectures {
	ARCHITECTURE_XT = 0,
	ARCHITECTURE_AT = 1,
	ARCHITECTURE_PS2 = 2
}; //All possible architectures!

//B/W monitor setting:
//Color mode
#define BWMONITOR_NONE 0
#define BWMONITOR_WHITE 1
#define BWMONITOR_GREEN 2
#define BWMONITOR_AMBER 3

//Default values for new BIOS settings:
#define DEFAULT_BOOT_ORDER 0
#define DEFAULT_CPU CPU_NECV30
#define DEFAULT_DEBUGMODE DEBUGMODE_NONE
#define DEFAULT_EXECUTIONMODE EXECUTIONMODE_BIOS
#define DEFAULT_DEBUGGERLOG DEBUGGERLOG_NONE
#define DEFAULT_ASPECTRATIO 2
#ifdef STATICSCREEN
#define DEFAULT_DIRECTPLOT 0
#else
#define DEFAULT_DIRECTPLOT 2
#endif
#define DEFAULT_BWMONITOR BWMONITOR_NONE
#define DEFAULT_SSOURCEVOL 100
#define DEFAULT_BLASTERVOL 100
#define DEFAULT_FRAMERATE 0
#define DEFAULT_VGASYNCHRONIZATION 2
#define DEFAULT_DIAGNOSTICSPORTOUTPUT_BREAKPOINT -1
#define DEFAULT_DIAGNOSTICSPORTOUTPUT_TIMEOUT 0
#define DEFAULT_CPUSPEEDMODE 0

void BIOS_LoadIO(int showchecksumerrors); //Loads basic I/O drives from BIOS!
void BIOS_ShowBIOS(); //Shows mounted drives etc!
void BIOS_ValidateData(); //Validate all data and eject wrong ones!
void BIOS_LoadDefaults(int tosave); //Load BIOS defaults!

void BIOS_LoadData(); //Loads the data!
int BIOS_SaveData(); //Save BIOS settings!

//Stuff for other units:
uint_32 BIOS_GetMMUSize(); //For MMU!
int boot_system(); //Tries to boot using BIOS Boot Order. TRUE on success, FALSE on error/not booted.

void autoDetectMemorySize(int tosave); //Autodetect memory size! (tosave=To save BIOS?)
void forceBIOSSave(); //Forces BIOS to save!

void BIOSKeyboardInit(); //BIOS part of keyboard initialisation!

//Storage auto-detection, when supported!
void BIOS_DetectStorage(); //Auto-Detect the current storage to use, on start only!

void autoDetectArchitecture(); //Detect the architecture to use!

void BIOS_ejectdisk(int disk); //Eject an ejectable disk?

#endif
