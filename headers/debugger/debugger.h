#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "headers/bios/bios.h" //BIOS support!
#include "headers/cpu/modrm.h" //Modrm support!

//BIOS Settings
#ifndef BIOS_Settings
extern BIOS_Settings_TYPE BIOS_Settings; //BIOS Settings!
#endif

//Debugger enabled?
#define DEBUGGER_ENABLED (((BIOS_Settings.debugmode>0) && (BIOS_Settings.debugmode<4)) || (BIOS_Settings.debugmode==5) || (BIOS_Settings.debugmode==DEBUGMODE_TEST))
//Always debugger on (ignore LTRIGGER?)
#define DEBUGGER_ALWAYS_DEBUG (BIOS_Settings.debugmode>1)

//Always stepwise check, also ignoring RTRIGGER for STEP?
#define DEBUGGER_ALWAYS_STEP ((BIOS_Settings.debugmode==DEBUGMODE_STEP) || (BIOS_Settings.debugmode==DEBUGMODE_TEST_STEP))
//Always keep running, ignoring X for continuing step?
#define DEBUGGER_KEEP_RUNNING (BIOS_Settings.debugmode==DEBUGMODE_TEST)

//Base row of register dump on-screen!
#define DEBUGGER_REGISTERS_BASEROW 1

//Log with debugger?
#define DEBUGGER_LOG debugging()

void debugger_step(); //Debugging, if debugging (see below), after the CPU changes it''s registers!
int debugging(); //Debugging?

//For CPU:
void debugger_beforeCPU(); //Action before the CPU changes it's registers!

void debugger_setcommand(char *text, ...); //Set current command (Opcode only!)
void debugger_setprefix(char *text); //Set prefix (CPU only!)

void modrm_debugger8(MODRM_PARAMS *params, byte whichregister1, byte whichregister2); //8-bit handler!
void modrm_debugger16(MODRM_PARAMS *params, byte whichregister1, byte whichregister2); //16-bit handler!

void debugger_screen(); //On-screen dump of registers etc.

void debugger_logregisters(CPU_registers *registers);
#endif