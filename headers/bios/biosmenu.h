#ifndef BIOSMENU_H
#define BIOSMENU_H

//Cancelled!
#define FILELIST_CANCEL -1
//No files with this extension!
#define FILELIST_NOFILES -2
//Default item!
#define FILELIST_DEFAULT -3

#define ITEMLIST_MAXITEMS 0x100

void allocBIOSMenu(); //Stuff that take extra video memory etc. for seperated BIOS allocation (so before MMU, because it may take it all)!
void freeBIOSMenu(); //Free up all BIOS related memory!
void initBIOSMenu(); //Stuff that take extra video memory etc. for seperated BIOS!

int CheckBIOSMenu(uint_32 timeout); //To run the BIOS Menus! Result: to reboot?
byte runBIOS(byte showloadingtext); //Run the BIOS!

int ExecuteList(int x, int y, char *defaultentry, int maxlen); //Runs the file list!

byte sound_playMIDIfile(byte showinfo); //Play a MIDI file!
#endif