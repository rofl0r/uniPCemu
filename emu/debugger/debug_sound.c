#include "headers/hardware/ports.h" //Port support!
#include "headers/hardware/vga.h" //VGA support!
#include "headers/hardware/pcspeaker.h" //PC speaker support!
#include "headers/emu/threads.h" //Thread support!
#include "headers/emu/sound.h" //Sound support for our callback!
#include "headers/emu/timers.h" //Timer support!
#include "headers/support/mid.h" //MIDI file support!
#include "headers/hardware/vga.h" //VGA support!
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/bios/biosmenu.h" //BIOS menu option support!

//Test the speaker?
#define __DEBUG_SPEAKER
//Test the Adlib?
#define __DEBUG_ADLIB

//Test MIDI?
#define __DEBUG_MIDI

void adlibsetreg(byte reg,byte val)
{
	PORT_OUT_B(0x388,reg);
	PORT_OUT_B(0x389,val);
}

//Cannot read adlib registers!

byte adlibgetstatus()
{
	return PORT_IN_B(0x388); //Read status byte!
}

int detectadlib()
{
	return 1; //Detected!
	adlibsetreg(4,0x60); //Reset both timers!
	adlibsetreg(4,0x80); //Enable the interrupts!
	byte status = adlibgetstatus(); //Read the status for comparision!
	adlibsetreg(2,0xFF); //FF in register 2!
	adlibsetreg(4,0x21); //Start timer 1!
	delay(80); //Wait at least 80us!
	byte status2 = adlibgetstatus(); //Read the status for comparision!
	adlibsetreg(4,0x60); //Reset both timers and disable the interrupts!
	if (((status&0xE0)==0x00) && ((status2&0xE0)==0xC0)) //Detected?
	{
		return 1; //Detected!
	}
	return 0; //Not detected!
}

byte PCspeakerFinished = 0;

void testtimer()
{
	byte loops = 5;
	static byte count = 0;
	next: //Next speaker callback!
	switch (count++) //What timer?
	{
		case 0: //First timer!
			disableSpeaker(0); //Disable the first speaker!
			break;
		case 1: //Second timer!
			enableSpeaker(0); //Enable the first speaker!
			disableSpeaker(1); //Disable the second speaker!
			break;
		case 2: //Third timer!
			enableSpeaker(1); //Enable the second speaker!
			disableSpeaker(2); //Disable the third speaker!
			break;
		case 3: //Fourth timer!
			enableSpeaker(2); //Enable the third speaker to enable all speakers again!
			disableSpeaker(0); //Disable the first speaker!
			break;
		case 4: //Fifth timer!
			enableSpeaker(0); //Enable all speakers!
			break;
		case 5: //Sixth timer!
		default: //Unknown status?
			count = 0; //Reset!
			if (!--loops)
			{
				disableSpeaker(0); //Disable the final speaker!
				return; //Abort: finished!
			}
			break;
	}
	delay(1000000); //Wait 1 second!
	goto next; //Next loop/item!
}

extern char itemlist[ITEMLIST_MAXITEMS][256]; //Max X files listed!
int MIDI_file = 0; //The file selected!

int BIOS_MIDI_selection() //MIDI selection menu, custom for this purpose!
{
	BIOS_Title("Select MIDI file to play");
	generateFileList("mid|midi", 0, 0); //Generate file list for all .img files!
	EMU_gotoxy(0, 4); //Goto 4th row!
	EMU_textcolor(0x7F); //We're using inactive color for label!
	GPU_EMU_printscreen(0, 4, "MIDI file: "); //Show selection init!

	int file = ExecuteList(12, 4, itemlist[MIDI_file], 256); //Show menu for the disk image!
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

byte sound_playMIDIfile()
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
		//Play the MIDI file!
		playMIDIFile(&itemlist[MIDI_file][0]); //Play the MIDI file!
		delay(1000000); //Wait 1 second before selecting the next file!
	}
	return 1; //Plain finish: just execute whatever you want!
}

void dosoundtest()
{
	CPU[activeCPU].registers->AH = 0x00; //Init video mode!
	CPU[activeCPU].registers->AL = VIDEOMODE_EMU; //80x25 16-color TEXT for EMU mode!
	BIOS_int10(); //Switch!

	BIOS_enableCursor(0); //Disable the cursor!
	delay(1000000); //Wait 1 second!

	printmsg(0xF, "\r\nStarting sound test...\r\n");
	delay(1000000);
	VGA_waitforVBlank(); //Wait 1 frame!

	#ifdef __DEBUG_SPEAKER
	printmsg(0xF,"Debugging PC Speaker...\r\n");
	//setSpeakerFrequency(0,261.626f);
	setSpeakerFrequency(0,100.0f); //Low!
	enableSpeaker(0); //Enable the second speaker!
	
	
	//Program second speaker manually.
	setSpeakerFrequency(1,1000.0f); //MID!
	enableSpeaker(0); //Enable the second speaker!

	setSpeakerFrequency(2,2000.0f);
	enableSpeaker(0); //Enable the second speaker!

	testtimer(); //Run PC speakers test!
	disableSpeaker(0);
	disableSpeaker(1);
	disableSpeaker(2); //Disable all speakers!
	delay(4000000); //Wait 1 second for the next test!
#endif

#ifdef __DEBUG_ADLIB
	startTimers(0); //Make sure we're timing (needed for adlib test).
	printmsg(0xF, "Detecting adlib...");
	if (detectadlib()) //Detected?
	{
		printmsg(0xF, "\r\nAdlib detected. Starting sound in 1 second...");
		VGA_waitforVBlank(); //Wait 1 frame!
		VGA_waitforVBlank(); //Wait 1 frame!
		printmsg(0xF, "\r\nStarting adlib sound...");
		adlibsetreg(0x20, 0x21); //Modulator multiple to 1!
		adlibsetreg(0x40, 0x10); //Modulator level about 40dB!
		adlibsetreg(0x60, 0xF7); //Modulator attack: quick; decay long!
		adlibsetreg(0x80, 0xFF); //Modulator sustain: medium; release: medium
		adlibsetreg(0xA0, 0x98); //Set voice frequency's LSB (it'll be a D#)!
		adlibsetreg(0x23, 0x21); //Set the carrier's multiple to 1!
		adlibsetreg(0x43, 0x00); //Set the carrier to maximum volume (about 47dB).
		adlibsetreg(0x63, 0xFF); //Carrier attack: quick; decay: long!
		adlibsetreg(0x83, 0x0F); //Carrier sustain: medium; release: medium!
		adlibsetreg(0xB0, 0x31); //Turn the voice on; set the octave and freq MSB!
		printmsg(0xF, "\r\nYou should only be hearing the Adlib tone now.");
		delay(5000000); //Basic tone!
		int i,j;
		for (j = 0; j < 2; j++)
		{
			for (i = 0; i <= 7; i++)
			{
				if (j)
				{
					printmsg(0xF, "\r\nSetting feedback level %i, additive synthesis", i);
				}
				else
				{
					printmsg(0xF, "\r\nSetting feedback level %i, fm synthesis", i);
				}
				adlibsetreg(0xC0, ((i << 1) | j));
				delay(3000000); //Wait some time!
			}
		}
		printmsg(0xF, "\r\nResetting synthesis to fm synthesis without feedback...");
		adlibsetreg(0xC0, 0); //Reset synthesis mode and disable feedback!
		delay(10000000); //Adlib only!
		printmsg(0xF, "\r\nSilencing Adlib tone...");
		adlibsetreg(0xB0,0x11); //Turn voice off!
		delay(4000000); //Wait 1 second for the next test!
		printmsg(0xF, "\r\n"); //Finisher!
	}
#endif

	#ifdef __DEBUG_MIDI
	printmsg(0xF,"Debugging MIDI...\r\n");
	VGA_waitforVBlank(); //Wait for a VBlank, to allow the screen to be up to date!
	if (sound_playMIDIfile()) //Play the default?
	{
		stopTimers(1); //Stop ALL timers for testing speed!
		//dolog("SF2","Sounding test central C...");
		//printmsg(0xF,"MIDI Piano central C...");
		//Don't worry about timing!
		PORT_OUT_B(0x331, 0xFF); //Reset!
		PORT_OUT_B(0x331, 0x3F); //Kick to UART mode!
		PORT_OUT_B(0x330, 0xC0);
		PORT_OUT_B(0x330, 0x00); //Piano
		//PORT_OUT_B(0x330,20); //Organ!
		//PORT_OUT_B(0x330,26); //Jazz guitar
		//PORT_OUT_B(0x330, 73); //Flute!

		//Apply drum kit!
		//PORT_OUT_B(0x330, 0x00); //Default drum kit!

		//Switch to the drum kit set!
		PORT_OUT_B(0x330, 0xB0); //Controller change!
		PORT_OUT_B(0x330, 0x00); //Bank high!
		PORT_OUT_B(0x330, 0x00); //0xXX00
		PORT_OUT_B(0x330, 0x20); //Bank low!
		PORT_OUT_B(0x330, 0x00); //0x00XX

		byte notes[10] = { 60, 62, 64, 65, 67, 69, 71, 72, 74, 76 };
		byte i;
		for (i = 0; i < 10;)
		{
			PORT_OUT_B(0x330, 0x90); //First tone ON!
			PORT_OUT_B(0x330, notes[i]); //This note...
			PORT_OUT_B(0x330, 100); //Is sounded at AVG velocity!!
			/*
			PORT_OUT_B(0x330,0xB0); //Controller!
			PORT_OUT_B(0x330,0x40); //Hold pedal!
			PORT_OUT_B(0x330,0x40); //Enabled!
			*/
			delay(10000); //Wait 1 second!
			PORT_OUT_B(0x330, 0x80); //Note off!
			PORT_OUT_B(0x330, notes[i]); //Previous note!
			PORT_OUT_B(0x330, 100); //Normally off!
			/*
			PORT_OUT_B(0x330,0xB0); //Controller!
			PORT_OUT_B(0x330,0x40); //Hold pedal!
			PORT_OUT_B(0x330,0x00); //Disabled!
			*/
			delay(1000000); //Wait 1 second!
			++i; //Next note!
		}
		delay(10000000);
	}
	#endif
	
	exit(0); //Quit the application!
	sleep(); //Wait forever!
}