#include "headers/hardware/ports.h" //Port support!
#include "headers/hardware/vga/vga.h" //VGA support!
#include "headers/hardware/8253.h" //PC speaker support!
#include "headers/emu/threads.h" //Thread support!
#include "headers/emu/sound.h" //Sound support for our callback!
#include "headers/emu/timers.h" //Timer support!
#include "headers/support/mid.h" //MIDI file support!
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/bios/biosmenu.h" //BIOS menu option support!
#include "headers/interrupts/interrupt10.h" //Interrupt 10h support!
#include "headers/emu/emu_vga_bios.h" //Cursor enable support!
#include "headers/support/dro.h" //DRO file support!

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
	return 1; //Detected: we cannot be detected because the CPU isn't running!
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
	setPITFrequency(2,(word)(1190000.0f/100.0f)); //Low!
	PORT_OUT_B(0x61,(PORT_IN_B(0x61)|3)); //Enable the second speaker!
	delay(1000000);
	
	setPITFrequency(2,(word)(1190000.0f/1000.0f)); //Medium!
	delay(1000000);

	setPITFrequency(2,(word)(1190000.0f/2000.0f)); //High!
	delay(1000000);

	PORT_OUT_B(0x61, (PORT_IN_B(0x61) & 0xFC)); //Disable the speaker!

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
		if (playDROFile("music/ADLIB.DRO",1)) goto skipadlib;
		adlibsetreg(0x20, 0x21); //Modulator multiple to 1!
		adlibsetreg(0x40, 0x3F); //Modulator level about zero to produce a pure tone!
		//adlibsetreg(0x40, 0x10); //Modulator level about 40dB!
		adlibsetreg(0x60, 0xF7); //Modulator attack: quick; decay long!
		adlibsetreg(0x80, 0xFF); //Modulator sustain: medium; release: medium
		adlibsetreg(0xA0, 0x98); //Set voice frequency's LSB (it'll be a D#)!
		adlibsetreg(0x23, 0x21); //Set the carrier's multiple to 1!
		adlibsetreg(0x43, 0x00); //Set the carrier to maximum volume (about 47dB).
		adlibsetreg(0x63, 0xFF); //Carrier attack: quick; decay: long!
		adlibsetreg(0x83, 0x0F); //Carrier sustain: medium; release: medium!
		adlibsetreg(0xB0, 0x31); //Turn the voice on; set the octave and freq MSB!
		adlibsetreg(0xC0, 0x00); //No feedback and use FM synthesis!
		printmsg(0xF, "\r\nYou should only be hearing the Adlib tone now.");
		delay(5000000); //Basic tone!
		int i,j;
		adlibsetreg(0x40, 0x10); //Modulator level about 40dB!
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
	skipadlib: //Skip test playback?
#endif

	#ifdef __DEBUG_MIDI
	printmsg(0xF,"Debugging MIDI...\r\n");
	VGA_waitforVBlank(); //Wait for a VBlank, to allow the screen to be up to date!
	if (!playMIDIFile("music/MPU.MID",1)) //Play the default(we're showing info when playing MIDI files)?
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