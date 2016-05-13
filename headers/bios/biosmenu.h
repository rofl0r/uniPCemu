#ifndef BIOSMENU_H
#define BIOSMENU_H

//Cancelled!
#define FILELIST_CANCEL -1
//No files with this extension!
#define FILELIST_NOFILES -2
//Default item!
#define FILELIST_DEFAULT -3

#define ITEMLIST_MAXITEMS 1000

void allocBIOSMenu(); //Stuff that take extra video memory etc. for seperated BIOS allocation (so before MMU, because it may take it all)!
void freeBIOSMenu(); //Free up all BIOS related memory!

int CheckBIOSMenu(uint_32 timeout); //To run the BIOS Menus! Result: to reboot?
byte runBIOS(byte showloadingtext); //Run the BIOS!

typedef void(*list_information)(char *filename); //Displays information about a harddisk to mount!
int ExecuteList(int x, int y, char *defaultentry, int maxlen, list_information information_handler); //Runs the file list!

byte sound_playMIDIfile(byte showinfo); //Play a MIDI file!
void BIOS_SoundStartStopRecording(); //Start/stop sound recording!
#endif