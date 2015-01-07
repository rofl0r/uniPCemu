#include "headers/hardware/ports.h" //Port support!
#include "headers/hardware/vga.h" //VGA support!
#include "headers/hardware/pcspeaker.h" //PC speaker support!
#include "headers/emu/threads.h" //Thread support!
#include "headers/emu/sound.h" //Sound support for our callback!

float currentFunction(byte how, const float time); //For the PC speaker!

//#define __DEBUG_SPEAKER
#define __DEBUG_MIDI
//#define __DEBUG_ADLIB

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

void testtimer()
{
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
		default: //Unknown status?
			count = 0; //Reset!
			goto next; //Reprocess!
			break;
			
	}
}

void dosoundtest()
{
	//dolog("sound","Soundtest started!");
	//addchannel(&testCallback,NULL,"Test Sinus",__TESTSAMPLERATE,0,0); //Add the speaker at the hardware rate, mono!
	//quitThread(); //Stop the thread: we're debugging!

	//printmsg(0xF,"Beeping PC speaker in 1 second...");
	//speakerOut(18.2f); //Beep low!
	
	
	#ifdef __DEBUG_SPEAKER
	//setSpeakerFrequency(0,261.626f);
	setSpeakerFrequency(0,100.0f); //Low!
	enableSpeaker(0); //Enable the second speaker!
	
	
	//Program second speaker manually.
	setSpeakerFrequency(1,1000.0f); //MID!
	enableSpeaker(1); //Enable the second speaker!

	setSpeakerFrequency(2,2000.0f);
	enableSpeaker(2); //Enable the second speaker!
	#endif

	#ifdef __DEBUG_MIDI
	//dolog("SF2","Sounding test central C...");
	//printmsg(0xF,"MIDI Piano central C...");
	//Don't worry about timing!
	PORT_OUT_B(0x331,0xFF); //Reset!
	PORT_OUT_B(0x331,0x3F); //Kick to UART mode!
	PORT_OUT_B(0x330,0xC0);
	PORT_OUT_B(0x330,0x00); //Piano
	//PORT_OUT_B(0x330,0x7F); //Drum kit!
	//PORT_OUT_B(0x330,20); //Organ!
	//PORT_OUT_B(0x330,26); //Jazz guitar
	byte notes[10] = {60,62,64,65,67,69,71,72,74,76};
	byte i;
	for (i=0;i<10;)
	{
		PORT_OUT_B(0x330,0x90); //First tone ON!
		PORT_OUT_B(0x330,notes[i]); //This note...
		PORT_OUT_B(0x330,100); //Is sounded at AVG velocity!!
		PORT_OUT_B(0x330,0xB0); //Controller!
		PORT_OUT_B(0x330,0x40); //Hold pedal!
		PORT_OUT_B(0x330,0x40); //Enabled!
		delay(1000000); //Wait 1 second!
		PORT_OUT_B(0x330,0x80); //Note off!
		PORT_OUT_B(0x330,notes[i]); //Previous note!
		PORT_OUT_B(0x330,100); //Normally off!
		/*PORT_OUT_B(0x330,0xB0); //Controller!
		PORT_OUT_B(0x330,0x40); //Hold pedal!
		PORT_OUT_B(0x330,0x00); //Disabled!
		*/
		++i; //Next note!
	}
	#endif
	
	//printmsg(0xF,"Testing for adlib...");
	
	#ifdef __DEBUG_ADLIB
	if (detectadlib()) //Detected?
	{
		/*
		printmsg(0xF,"\r\nAdlib detected. Starting sound in 1 second...");
		VGA_waitforVBlank(); //Wait 1 frame!
		VGA_waitforVBlank(); //Wait 1 frame!
		*/
		delay(1000000);
		adlibsetreg(0x20,0x01); //Modulator multiple to 1!
		adlibsetreg(0x40,0x10); //Modulator level about 40dB!
		adlibsetreg(0x60,0xF0); //Modulator attack: quick; decay long!
		adlibsetreg(0x80,0x77); //Modulator sustain: medium; release: medium
		adlibsetreg(0xA0,0x98); //Set voice frequency's LSB (it'll be a D#)!
		adlibsetreg(0x23,0x01); //Set the carrier's multiple to 1!
		adlibsetreg(0x43,0x00); //Set the carrier to maximum volume (about 47dB).
		adlibsetreg(0x63,0xF0); //Carrier attack: quick; decay: long!
		adlibsetreg(0x83,0x77); //Carrier sustain: medium; release: medium!
		adlibsetreg(0xB0,0x31); //Turn the voice on; set the octave and freq MSB!
		/*
		delay(2000000); //Wait 2 seconds!
		speakerOut(0.0f); //Speaker off, hearing adlib only!
		printmsg(0xF,"\r\nSpeaker terminated and reset. You should only be hearing Adlib now.\r\n");
		delay(2000000); //Adlib only!
		*/
		/*
		adlibsetreg(0xB0,0x11); //Turn voice off!
		printmsg(0xF,"\r\nAdlib terminated and reset.\r\n");
		*/
	}
	else
	{
		printmsg(0xF,"\r\nNo adlib detected.");
	}
	#endif
	
	startTimers(); //Make sure we're timing (needed for adlib test).
	
	#ifdef __DEBUG_SPEAKER
	int_32 STEP = 1000000; //Wait 1s intervals!
	for (;;) //Still left?
	{
		delay(STEP); //Wait one second!
		testtimer(); //Run test!
	}
	#endif

	printmsg(0xF,"\r\nReady. Waiting 10 seconds...");
	delay(10000000); //Wait 10 seconds!
	sleep(); //Wait forever!
}