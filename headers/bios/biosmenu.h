#ifndef BIOSMENU_H
#define BIOSMENU_H

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

void allocBIOSMenu(); //Stuff that take extra video memory etc. for seperated BIOS allocation (so before MMU, because it may take it all)!
void freeBIOSMenu(); //Free up all BIOS related memory!
void initBIOSMenu(); //Stuff that take extra video memory etc. for seperated BIOS!

int CheckBIOSMenu(uint_32 timeout); //To run the BIOS Menus! Result: to reboot?
void runBIOS(); //Run the BIOS!
void BIOS_MenuChooser(); //The menu chooser!

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
void BIOS_MemReAlloc(); //Reallocate memory!
void BIOS_DirectPlotSetting(); //Direct Plot Setting!
void BIOS_FontSetting(); //BIOS Font Setting!
void BIOS_KeepAspectRatio(); //Keep aspect ratio?
void BIOS_ConvertStaticDynamicHDD(); //Convert static to dynamic HDD?
void BIOS_ConvertDynamicStaticHDD(); //Generate Static HDD Image from a dynamic one!
void BIOS_DefragmentDynamicHDD(); //Defragment a dynamic HDD Image!
void BIOS_BWMonitor(); //Switch b/w monitor vs color monitor!

void BIOSMenu_LoadDefaults(); //Load the defaults option!

void BIOSClearScreen(); //Resets the BIOS's screen!
void BIOSDoneScreen(); //Cleans up the BIOS's screen!
int psp_inputkey(); //Input key from PSP directly!


#endif