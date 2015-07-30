#include "headers/hardware/ports.h" //Port support!
#include "headers/hardware/vga.h" //VGA support!
#include "headers/hardware/pcspeaker.h" //PC speaker support!
#include "headers/emu/threads.h" //Thread support!
#include "headers/emu/sound.h" //Sound support for our callback!
#include "headers/emu/timers.h" //Timer support!
#include "headers/support/mid.h" //MIDI file support!
#include "headers/hardware/vga.h" //VGA support!
#include "headers/hardware/midi/mididevice.h" //For the MIDI voices!
#include "headers/emu/gpu/gpu_text.h" //Text surface support!
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/bios/biosmenu.h" //BIOS menu option support!

//Test the speaker?
//#define __DEBUG_SPEAKER
//Test the Adlib?
//#define __DEBUG_ADLIB

//Test MIDI?
#define __DEBUG_MIDI
//Test MIDI using MID file?
#define __DEBUG_MPUMID 1

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

extern uint_64 timing_pos; //Current timing position for MIDI speed playback!

word MID_RUNNING = 0;

HEADER_CHNK header;

SDL_sem *MID_channel_Lock = NULL; //Our channel lock for counting running MIDI!
extern byte MID_TERM; //MIDI termination flag! Locked with MID_timing_pos_Lock!

byte *MID_data[100]; //Tempo and music track!
TRACK_CHNK MID_tracks[100];

void handleMIDIChannel()
{
	word channel;
	channel = (word)getthreadparams(); //Gotten a channel?
	nextchannel: //Play next channel when type 2!
	playMIDIStream(channel,MID_data[channel], &header, &MID_tracks[channel]); //Play the MIDI stream!
	SDL_SemWait(MID_channel_Lock);
	if (byteswap16(header.format) == 2) //Multiple tracks to be played after one another?
	{
		timing_pos = 0; //Reset the timing position!
		++channel; //Process the next channel!
		if (channel >= byteswap16(header.n)) goto finish; //Last channel processed?
		SDL_SemPost(MID_channel_Lock);
		goto nextchannel; //Process the next channel now!
	}
	finish:
	--MID_RUNNING; //Done!
	SDL_SemPost(MID_channel_Lock);
}

extern MIDIDEVICE_VOICE activevoices[__MIDI_NUMVOICES]; //All active voices!
extern GPU_TEXTSURFACE *frameratesurface; //Our framerate surface!

void printMIDIChannelStatus()
{
	int i;
	uint_32 color; //The color to use!
	GPU_text_locksurface(frameratesurface); //Lock the surface!
	for (i = 0; i < __MIDI_NUMVOICES; i++) //Process all voices!
	{
		GPU_textgotoxy(frameratesurface,0, i + 5); //Row 5+!
		if (activevoices[i].VolumeEnvelope.active) //Active voice?
		{
			color = RGB(0x00, 0xFF, 0x00); //The color to use!
			GPU_textprintf(frameratesurface, color, RGB(0xDD, 0xDD, 0xDD),"%02i",activevoices[i].VolumeEnvelope.active);
		}
		else //Inactive voice?
		{
			if (activevoices[i].play_counter) //We have been playing?
			{
				color = RGB(0xFF, 0xAA, 0x00);
				GPU_textprintf(frameratesurface, color, RGB(0xDD, 0xDD, 0xDD), "%02i", i);
			}
			else //Completely unused voice?
			{
				color = RGB(0xFF, 0x00, 0x00);
				GPU_textprintf(frameratesurface, color, RGB(0xDD, 0xDD, 0xDD), "%02i", i);
			}
		}
		if (activevoices[i].channel && activevoices[i].note) //Gotten assigned?
		{
			GPU_textprintf(frameratesurface, color, RGB(0xDD, 0xDD, 0xDD), " %04X %02X %02X", activevoices[i].channel->activebank, activevoices[i].channel->program, activevoices[i].note->note); //Dump information about the voice we're playing!
		}
	}
	GPU_text_releasesurface(frameratesurface); //Unlock the surface!
}

//All used locks!
extern SDL_sem *MIDLock;
extern SDL_sem *MID_timing_pos_Lock;
extern SDL_sem *MID_BPM_Lock;
extern uint_64 timing_pos; //Current timing position!
extern float BPM; //No BPM by default!

void playMIDIFile(char *filename) //Play a MIDI file, CIRCLE to stop playback!
{
	memset(&MID_data, 0, sizeof(MID_data)); //Init data!
	memset(&MID_tracks, 0, sizeof(MID_tracks)); //Init tracks!

	word numchannels = 0;
	if ((numchannels = readMID(filename, &header, &MID_tracks[0], &MID_data[0], 100)) && __DEBUG_MPUMID)
	{
		stopTimers(0); //Stop most timers for max compatiblity and speed!
		//Initialise our device!
		PORT_OUT_B(0x331, 0xFF); //Reset!
		PORT_OUT_B(0x331, 0x3F); //Kick to UART mode!

		//Create the semaphore for the threads!
		MIDLock = SDL_CreateSemaphore(1);
		MID_timing_pos_Lock = SDL_CreateSemaphore(1);
		MID_BPM_Lock = SDL_CreateSemaphore(1);
		MID_channel_Lock = SDL_CreateSemaphore(1);

		SDL_SemWait(MID_BPM_Lock); //Wait for the lock!
		BPM = 0.0f; //Reset BPM!
		SDL_SemPost(MID_BPM_Lock); //Finish: we've set the BPM!

		SDL_SemWait(MID_timing_pos_Lock); //Wait for the lock!
		timing_pos = 0; //Reset the timing for the current song!
		SDL_SemPost(MID_timing_pos_Lock); //Finish: we've set the timing position!

		MID_TERM = 0; //Reset termination flag!

		updateMIDTimer(&header); //Update the timer!

		//Now, start up all timers!

		word i;
		MID_RUNNING = numchannels; //Init to all running!
		for (i = 0; i < numchannels; i++)
		{
			if (!i || (byteswap16(header.format) == 1)) //One channel, or multiple channels with format 2!
			{
				startThread(&handleMIDIChannel, "MIDI_STREAM", (void *)i, DEFAULT_PRIORITY); //Start a thread handling the output of the channel!
			}
		}
		if (byteswap16(header.format) != 1) //One channel only?
		{
			MID_RUNNING = 1; //Only one channel running!
		}

		delay(10000); //Wait a bit to allow for initialisation (position 0) to run!

		startTimers(1);
		startTimers(0); //Start our timers!

		byte running = 1; //Are we running?

		for (;;) //Wait to end!
		{
			delay(50000); //Wait 1sec intervals!
			SDL_SemWait(MID_channel_Lock);
			if (!MID_RUNNING)
			{
				running = 0; //Not running anymore!
			}
			printMIDIChannelStatus(); //Print the MIDI channel status!
			SDL_SemPost(MID_channel_Lock);

			if (psp_keypressed(BUTTON_CIRCLE)) //Circle pressed? Request to stop playback!
			{
				for (; psp_keypressed(BUTTON_CIRCLE);) {
					delay(10000); //Wait for release!
				} //Wait for release!
				SDL_SemWait(MID_timing_pos_Lock); //Wait to update the flag!
				MID_TERM = 1; //Set termination flag to request a termination!
				SDL_SemPost(MID_timing_pos_Lock); //We're updated!
			}

			if (!running) break; //Not running anymore? Start quitting!
		}

		//Destroy semaphore
		SDL_DestroySemaphore(MIDLock);
		SDL_DestroySemaphore(MID_timing_pos_Lock);
		SDL_DestroySemaphore(MID_BPM_Lock);
		SDL_DestroySemaphore(MID_channel_Lock);

		removetimer("MID_tempotimer"); //Clean up!
		freeMID(&MID_tracks[0], &MID_data[0], numchannels); //Free all channels!

		//Clean up the MIDI device for any leftover sound!
		byte channel;
		for (channel = 0; channel < 0x10; channel++) //Process all channels!
		{
			PORT_OUT_B(0x330, 0xB0|(channel&0xF)); //We're requesting a ...
			PORT_OUT_B(0x330, 0x7B); //All Notes off!
			PORT_OUT_B(0x330, 0x00); //On the current channel!!
		}
		PORT_OUT_B(0x330, 0xFF); //Reset MIDI device!
		PORT_OUT_B(0x331, 0xFF); //Reset the MPU!
	}
	//We're finished playing! Return to caller for new input!
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

void dosoundtest()
{
	CPU[activeCPU].registers->AH = 0x00; //Init video mode!
	CPU[activeCPU].registers->AL = VIDEOMODE_EMU; //80x25 16-color TEXT for EMU mode!
	BIOS_int10(); //Switch!

	BIOS_enableCursor(0); //Disable the cursor!
	delay(1000000); //Wait 1 second!

	printmsg(0xF, "\r\nStarting sound test...");
	delay(1000000);
	VGA_waitforVBlank(); //Wait 1 frame!

	//dolog("sound","Soundtest started!");
	//addchannel(&testCallback,NULL,"Test Sinus",__TESTSAMPLERATE,0,0); //Add the speaker at the hardware rate, mono!
	//quitThread(); //Stop the thread: we're debugging!

	//printmsg(0xF,"Beeping PC speaker in 1 second...");
	//speakerOut(18.2f); //Beep low!
	
	//Prepare the PC speaker!
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
				return; //Allow our caller to execute the next step!
			}
		}
		//Play the MIDI file!
		playMIDIFile(&itemlist[MIDI_file][0]); //Play the MIDI file!
		delay(1000000); //Wait 1 second before selecting the next file!
	}

	stopTimers(1); //Stop ALL timers for testing speed!
	//dolog("SF2","Sounding test central C...");
	//printmsg(0xF,"MIDI Piano central C...");
	//Don't worry about timing!
	PORT_OUT_B(0x331,0xFF); //Reset!
	PORT_OUT_B(0x331,0x3F); //Kick to UART mode!
	PORT_OUT_B(0x330,0xC0);
	PORT_OUT_B(0x330,0x00); //Piano
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

	byte notes[10] = {60,62,64,65,67,69,71,72,74,76};
	byte i;
	for (i=0;i<10;)
	{
		PORT_OUT_B(0x330,0x90); //First tone ON!
		PORT_OUT_B(0x330,notes[i]); //This note...
		PORT_OUT_B(0x330,100); //Is sounded at AVG velocity!!
		/*
		PORT_OUT_B(0x330,0xB0); //Controller!
		PORT_OUT_B(0x330,0x40); //Hold pedal!
		PORT_OUT_B(0x330,0x40); //Enabled!
		*/
		delay(10000); //Wait 1 second!
		PORT_OUT_B(0x330,0x80); //Note off!
		PORT_OUT_B(0x330,notes[i]); //Previous note!
		PORT_OUT_B(0x330,100); //Normally off!
		/*
		PORT_OUT_B(0x330,0xB0); //Controller!
		PORT_OUT_B(0x330,0x40); //Hold pedal!
		PORT_OUT_B(0x330,0x00); //Disabled!
		*/
		delay(1000000); //Wait 1 second!
		++i; //Next note!
	}
	delay(10000000);
	#endif
	
	exit(0); //Quit the application!
	sleep(); //Wait forever!
}