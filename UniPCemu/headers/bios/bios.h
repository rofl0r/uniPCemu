/*

Copyright (C) 2019 - 2020  Superfury

This file is part of UniPCemu.

UniPCemu is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

UniPCemu is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with UniPCemu.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef BIOS_H
#define BIOS_H

#include "headers/emu/input.h" //For INPUT_SETTINGS!
#include "headers/hardware/cmos.h" //CMOS support!
//First, the typedefs:

//What to leave for functions! 2MB for normal operations(1 extra MB for ini files), plus 5 screens for VGA rendering resizing (2 screens for doubled sizing(never x&y together) and 1 screen for the final result)!
#define FREEMEMALLOC ((2*(MBMEMORY))+(5*(PSP_SCREEN_COLUMNS*PSP_SCREEN_ROWS*sizeof(uint_32))))

//Delay between steps!
#define BIOS_INPUTDELAY 250000

//BIOS Version!
#define BIOS_VERSION 1

#ifdef PACKETSERVER_ENABLED
typedef struct
{
	CharacterType IPaddress[256]; //Credentials IP address, otherwise default(the 0th user)!
	CharacterType username[256]; //Credentials username
	CharacterType password[256]; //Credentials password
} ETHERNETSERVER_USER;

typedef struct
{
	int_64 ethernetcard; //What adapter to use? 255=List adapters!
	CharacterType MACaddress[256]; //MAC address, formatted with hexadecimal characters and : only!
	CharacterType gatewayMACaddress[256]; //MAC address, formatted with hexadecimal characters and : only!
	ETHERNETSERVER_USER users[256]; //Up to 256 users!
} ETHERNETSERVER_SETTINGS_TYPE;
#endif

typedef struct
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

	word emulated_CPU; //Original: Emulated CPU?

	byte bootorder; //Boot order?
	byte debugmode; //What debug mode?
	byte debugger_log; //Log when using the debugger?

	INPUT_SETTINGS input_settings; //Settings for input!

	byte GPU_AllowDirectPlot; //Allow VGA Direct Plot: 1 for automatic 1:1 mapping, 0 for always dynamic, 2 for force 1:1 mapping?
	uint_32 VRAM_size; //(S)VGA VRAM size!
	byte bwmonitor; //Are we a b/w monitor?
	byte aspectratio; //The aspect ratio to use?

	byte BIOSmenu_font; //The selected font for the BIOS menu!
	byte firstrun; //Is this the first run of this BIOS?

	CMOSDATA ATCMOS; //The full saved CMOS!
	byte got_ATCMOS; //Gotten an CMOS?

	byte executionmode; //What mode to execute in during runtime?
	byte VGA_Mode; //Enable VGA NMI on precursors?
	byte architecture; //Are we using the XT/AT/PS/2 architecture?
	uint_32 CPUSpeed; //Original: CPU Speed!
	uint_32 SoundSource_Volume; //The sound source volume knob!
	byte ShowFramerate; //Show the frame rate?
	byte DataBusSize; //Original: The size of the emulated BUS. 0=Normal bus, 1=8-bit bus when available for the CPU!
	byte ShowCPUSpeed; //Show the relative CPU speed together with the framerate?
	byte usePCSpeaker; //Emulate PC Speaker sound?
	byte useAdlib; //Emulate Adlib?
	byte useLPTDAC; //Emulate Covox/Disney Sound Source?
	byte VGASynchronization; //VGA synchronization setting. 0=Automatic synchronization based on Host CPU. 1=Tight VGA Synchronization with the CPU.
	byte CGAModel; //What kind of CGA is emulated? Bit0=NTSC, Bit1=New-style CGA
	byte useGameBlaster; //Emulate Game Blaster?
	uint_32 GameBlaster_Volume; //The Game Blaster volume knob!
	byte useSoundBlaster; //Emulate Sound Blaster?
	uint_32 TurboCPUSpeed; //Original: Turbo CPU Speed!
	byte useTurboSpeed; //Original: Are we to use Turbo CPU speed?
	sword diagnosticsportoutput_breakpoint; //Use a diagnostics port breakpoint?
	uint_32 diagnosticsportoutput_timeout; //Breakpoint timeout used!
	byte useDirectMIDI; //Use Direct MIDI synthesis by using a passthrough to the OS?
	uint_64 breakpoint[5]; //The used breakpoint segment:offset and mode!
	byte BIOSROMmode; //BIOS ROM mode.
	byte debugger_logstates; //Are we logging states? 1=Log states, 0=Don't log states!

	//CMOS for Compaq systems!
	CMOSDATA CompaqCMOS; //The full saved CMOS!
	byte got_CompaqCMOS; //Gotten an CMOS?
	byte InboardInitialWaitstates; //Inboard 386 initial delay used?
	word modemlistenport; //What port does the modem need to listen on?
	byte clockingmode; //Original: Are we using the IPS clock instead of cycle-accurate clock?
	byte debugger_logregisters; //Are we to log registers when debugging?
	//CMOS for XT systems!
	CMOSDATA XTCMOS; //The full saved CMOS!
	byte got_XTCMOS; //Gotten an CMOS?
	//CMOS for PS/2 systems!
	CMOSDATA PS2CMOS; //The full saved CMOS!
	byte got_PS2CMOS; //Gotten an CMOS?
	CMOSDATA i430fxCMOS; //The full saved CMOS!
	byte got_i430fxCMOS; //Gotten an CMOS?
	CMOSDATA i440fxCMOS; //The full saved CMOS!
	byte got_i440fxCMOS; //Gotten an CMOS?
#ifdef PACKETSERVER_ENABLED
	ETHERNETSERVER_SETTINGS_TYPE ethernetserver_settings;
#endif
	CharacterType phonebook[10][256]; //A full phonebook for the modem!
	byte backgroundpolicy; //The currently active background policy!
	byte advancedlog; //Use advanced debugger information during common log format logging?
	uint_64 taskBreakpoint; //Task to break on
	uint_64 FSBreakpoint; //FS to break on
	uint_64 CR3breakpoint; //CR3 to break on
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
//Don't show, just run, ignore RTRIGGER buttons.
#define DEBUGMODE_NOSHOW_RUN 4


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
//Always, don't log register state
#define DEBUGGERLOG_ALWAYS_NOREGISTERS 5
//Always, even during skipping?
#define DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP 6
//Always, Single line
#define DEBUGGERLOG_ALWAYS_SINGLELINE 7
//Debugging only, Single line
#define DEBUGGERLOG_DEBUGGING_SINGLELINE 8
//Always, Single line, simplified
#define DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED 9
//Debugging only, Single line, simplified
#define DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED 10
//Universal logging method, always
#define DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT 11
//Always, even during skipping?
#define DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT 12
//Universal logging method, when debugging only.
#define DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT 13

//Debugger state log
//Disabled state log
#define DEBUGGERSTATELOG_DISABLED 0
//Enabled state log
#define DEBUGGERSTATELOG_ENABLED 1

//Debugger registers log
//Disabled registers log
#define DEBUGGERREGISTERSLOG_DISABLED 0
//Enabled registers log
#define DEBUGGERREGISTERSLOG_ENABLED 1


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

//Use cycle-accurate clock
#define CLOCKINGMODE_CYCLEACCURATE 0
//Use IPS clock
#define CLOCKINGMODE_IPSCLOCK 1

//To debug the text mode characters on-screen?
#define DEBUG_VIDEOCARD (BIOS_Settings.executionmode==EXECUTIONMODE_VIDEOCARD)

//Architecture possibilities
enum Architectures {
	ARCHITECTURE_XT = 0,
	ARCHITECTURE_AT = 1,
	ARCHITECTURE_COMPAQ = 2,
	ARCHITECTURE_PS2 = 3,
	ARCHITECTURE_i430fx = 4,
	ARCHITECTURE_i440fx = 5
}; //All possible architectures!

enum BIOSROMMode {
	BIOSROMMODE_NORMAL = 0,
	BIOSROMMODE_DIAGNOSTICS = 1,
	BIOSROMMODE_UROMS = 2
}; //All possible priorities!

//B/W monitor setting:
//Color mode
#define BWMONITOR_NONE 0
#define BWMONITOR_WHITE 1
#define BWMONITOR_GREEN 2
#define BWMONITOR_AMBER 3

//Default values for new BIOS settings:
#define DEFAULT_BOOT_ORDER 0
#define DEFAULT_CPUS 1
#define DEFAULT_CPU CPU_8086
#define DEFAULT_DEBUGMODE DEBUGMODE_NONE
#define DEFAULT_EXECUTIONMODE EXECUTIONMODE_BIOS
#define DEFAULT_DEBUGGERLOG DEBUGGERLOG_NONE
#define DEFAULT_DEBUGGERSTATELOG DEBUGGERSTATELOG_DISABLED
#define DEFAULT_DEBUGGERREGISTERSLOG DEBUGGERREGISTERSLOG_DISABLED

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
#define DEFAULT_DIRECTMIDIMODE 0
#define DEFAULT_BREAKPOINT 0
#define DEFAULT_BIOSROMMODE BIOSROMMODE_NORMAL
#define DEFAULT_INBOARDINITIALWAITSTATES 0
#ifdef ANDROID
#define DEFAULT_MODEMLISTENPORT 65523
#else
#define DEFAULT_MODEMLISTENPORT 23
#endif
#define DEFAULT_CLOCKINGMODE CLOCKINGMODE_IPSCLOCK
#define DEFAULT_CGAMODEL 0
#define DEFAULT_VIDEOCARD 4
#define DEFAULT_SOUNDBLASTER 0

//Breakpoint helper constants
//2-bit mode(0=Disabled, 1=Real, 2=Protected, 3=Virtual 8086)
#define SETTINGS_BREAKPOINT_MODE_SHIFT 60
//Segment to break at!
#define SETTINGS_BREAKPOINT_SEGMENT_SHIFT 32
#define SETTINGS_BREAKPOINT_SEGMENT_MASK 0xFFFFULL
//Offset to break at!
#define SETTINGS_BREAKPOINT_IGNOREEIP_SHIFT 59
#define SETTINGS_BREAKPOINT_IGNOREADDRESS_SHIFT 58
#define SETTINGS_BREAKPOINT_IGNORESEGMENT_SHIFT 57
#define SETTINGS_BREAKPOINT_SINGLESTEP_SHIFT 56
#define SETTINGS_BREAKPOINT_OFFSET_MASK 0xFFFFFFFFULL

//Task breakpoint
#define SETTINGS_TASKBREAKPOINT_ENABLE_SHIFT 60
#define SETTINGS_TASKBREAKPOINT_SEGMENT_SHIFT 32
#define SETTINGS_TASKBREAKPOINT_SEGMENT_MASK 0xFFFFULL
#define SETTINGS_TASKBREAKPOINT_IGNOREBASE_SHIFT 59
#define SETTINGS_TASKBREAKPOINT_IGNORESEGMENT_SHIFT 58
#define SETTINGS_TASKBREAKPOINT_BASE_MASK 0xFFFFFFFF

//FS breakpoint
#define SETTINGS_FSBREAKPOINT_ENABLE_SHIFT 60
#define SETTINGS_FSBREAKPOINT_SEGMENT_SHIFT 32
#define SETTINGS_FSBREAKPOINT_SEGMENT_MASK 0xFFFFULL
#define SETTINGS_FSBREAKPOINT_IGNOREBASE_SHIFT 59
#define SETTINGS_FSBREAKPOINT_IGNORESEGMENT_SHIFT 58
#define SETTINGS_FSBREAKPOINT_BASE_MASK 0xFFFFFFFF

//CR3 breakpoint
#define SETTINGS_CR3BREAKPOINT_ENABLE_SHIFT 60
#define SETTINGS_CR3BREAKPOINT_BASE_MASK 0xFFFFFFFF

#define DEFAULT_BACKGROUNDPOLICY BACKGROUNDPOLICY_FULLHALT
#define DEFAULT_ADVANCEDLOG 0

typedef struct
{
	uint_32 memorybackup;
	byte emulated_CPUbackup; //Emulated CPU?
	byte emulated_CPUsbackup; //Emulated CPUs?
	uint_32 CPUspeedbackup; //CPU speed
	uint_32 TurboCPUspeedbackup; //Turbo CPU speed
	byte useTurboCPUSpeedbackup; //Are we to use Turbo CPU speed?
	byte clockingmodebackup; //Are we using the IPS clock instead of cycle-accurate clock?
	byte DataBusSizebackup; //The size of the emulated BUS. 0=Normal bus, 1=8-bit bus when available for the CPU!
} CMOSGLOBALBACKUPDATA;

void backupCMOSglobalsettings(CMOSDATA *CMOS, CMOSGLOBALBACKUPDATA *backupdata);
void restoreCMOSglobalsettings(CMOSDATA *CMOS, CMOSGLOBALBACKUPDATA *backupdata);

void BIOS_LoadIO(int showchecksumerrors); //Loads basic I/O drives from BIOS!
void BIOS_ShowBIOS(); //Shows mounted drives etc!
void BIOS_ValidateData(); //Validate all data and eject wrong ones!
void BIOS_LoadDefaults(int tosave); //Load BIOS defaults!

void BIOS_LoadData(); //Loads the data!
int BIOS_SaveData(); //Save BIOS settings!

//Stuff for other units:
//Retrieve the current architecture's memory size field for manipulation of it!
uint_32* getarchmemory(); //Get the memory field for the current architecture!
char* getcurrentarchtext(); //Get the current architecture!
byte* getarchemulated_CPU(); //Get the memory field for the current architecture!
byte* getarchemulated_CPUs(); //Get the memory field for the current architecture!
byte* getarchDataBusSize(); //Get the memory field for the current architecture!
uint_32* getarchCPUSpeed(); //Get the memory field for the current architecture!
uint_32* getarchTurboCPUSpeed(); //Get the memory field for the current architecture!
byte* getarchuseTurboCPUSpeed(); //Get the memory field for the current architecture!
byte* getarchclockingmode(); //Get the memory field for the current architecture!

//Retrieve the MMU size to use!
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
