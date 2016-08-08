#include "headers/types.h" //Basic types!
#include "headers/support/sounddoublebuffer.h" //Double buffered sound support!
#include "headers/emu/sound.h" //Sound output support!
#include "headers/support/signedness.h" //Sign support!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/ports.h" //I/O support!
#include "headers/hardware/adlib.h" //Adlib card has more ports with Sound Blasters!
#include "headers/hardware/pic.h" //Interrupt support!
#include "headers/hardware/8237A.h" //DMA support!

#define __SOUNDBLASTER_SAMPLERATE 22050.0f
#define __SOUNDBLASTER_SAMPLEBUFFERSIZE 2048
#define SOUNDBLASTER_VOLUME 100.0f
//Size of the input buffer of the DSP chip!
#define __SOUNDBLASTER_DSPINDATASIZE 16
//Big enough output buffer for all ranges available!
#define __SOUNDBLASTER_DSPOUTDATASIZE 0x10000

//IRQ/DMA assignments! Use secondary IRQ8(to prevent collisions with existing hardware!)
#define __SOUNDBLASTER_IRQ8 0x17
#define __SOUNDBLASTER_DMA8 1

struct
{
	word baseaddr;
	SOUNDDOUBLEBUFFER soundbuffer; //Outputted sound to render!
	FIFOBUFFER *DSPindata; //Data to be read from the DSP!
	FIFOBUFFER *DSPoutdata; //Data to be rendered for the DSP!
	byte resetport;
	sword command; //The current command we're processing (-1 for none)
	byte commandstep; //The step within the command!
	uint_32 dataleft; //The position(in bytes left) within the command during the data phase!
	byte busy; //Are we busy (not able to receive data/commands)?
	byte IRQ8Pending; //Is a 8-bit IRQ pending?
	byte DREQ; //Our current DREQ signal for transferring data!
	int DirectDACOutput; //Direct DAC output enabled when not -1?
	word silenceoutput;
	uint_32 silencesamples; //Silence samples left!
	byte muted; //Is speaker output disabled?
	byte singen; //Sine wave generator enabled?
	double singentime; //Sine wave generator position in time!
} SOUNDBLASTER; //The Sound Blaster data!

double soundblaster_soundtiming = 0.0, soundblaster_soundtick = 1000000000.0 / __SOUNDBLASTER_SAMPLERATE;
double soundblaster_sampletiming = 0.0, soundblaster_sampletick = 0.0;

byte leftsample=0x80, rightsample=0x80; //Two stereo samples, silence by default!

void updateSoundBlaster(double timepassed)
{
	double dummy;
	double temp;
	if (SOUNDBLASTER.baseaddr == 0) return; //No game blaster?

	//First, check for any static sample(Direct Output)
	if ((SOUNDBLASTER.DirectDACOutput != -1) && (SOUNDBLASTER.silencesamples==0)) //Direct DAC output and not silenced?
	{
		leftsample = rightsample = (byte)SOUNDBLASTER.DirectDACOutput; //Mono Direct DAC output!
	}
	else //Timed output?
	{
		soundblaster_sampletiming += timepassed; //Tick time!
		if ((soundblaster_sampletiming>=soundblaster_sampletick) && (soundblaster_sampletick>0.0)) //Expired?
		{
			for (;soundblaster_sampletiming>=soundblaster_sampletick;) //A sample to play?
			{
				if (SOUNDBLASTER.silencesamples) //Silence requested?
				{
					leftsample = rightsample = 0x80; //Silent sample!
					if (--SOUNDBLASTER.silencesamples == 0) //Decrease the sample counter! If expired, fire IRQ!
					{
						doirq(__SOUNDBLASTER_IRQ8); //Fire the IRQ!
					}
				}
				else //Audio playing?
				{
					//Play audio that's ready!
				}
				soundblaster_sampletiming -= soundblaster_sampletick; //A sample has been ticked!
			}
		}
	}

	if (SOUNDBLASTER.singen) //Sine wave generator enabled?
	{
		leftsample = rightsample = 0x80+(byte)(sin(2 *PI*2000.0f*SOUNDBLASTER.singentime) * (float)0x7F); //Give a full wave at the requested speed!
		SOUNDBLASTER.singentime += timepassed; //Tick the samples processed!
		temp = SOUNDBLASTER.singentime*2000.0f; //Calculate for overflow!
		if (temp >= 1.0) { //Overflow?
			SOUNDBLASTER.singentime = modf(temp, &dummy) / 2000.0f; //Protect against overflow by looping!
		}
	}

	if (SOUNDBLASTER.muted) //Muted?
	{
		leftsample = rightsample = 0x80; //Muted output!
	}

	//Finally, render any rendered Sound Blaster output to the renderer at the correct rate!
	//Sound Blaster sound output
	soundblaster_soundtiming += timepassed; //Get the amount of time passed!
	if (soundblaster_soundtiming >= soundblaster_soundtick)
	{
		for (;soundblaster_soundtiming >= soundblaster_soundtick;)
		{
			//Now push the samples to the output!
			writeDoubleBufferedSound32(&SOUNDBLASTER.soundbuffer, (rightsample<<24) | (leftsample<<8)); //Output the sample to the renderer!
			soundblaster_soundtiming -= soundblaster_soundtick; //Decrease timer to get time left!
		}
	}
}

byte SoundBlaster_soundGenerator(void* buf, uint_32 length, byte stereo, void *userdata) //Generate a sample!
{
	uint_32 c;
	c = length; //Init c!

	static uint_32 last = 0x80008000;
	INLINEREGISTER uint_32 buffer;

	SOUNDDOUBLEBUFFER *doublebuffer = (SOUNDDOUBLEBUFFER *)userdata; //Our double buffered sound input to use!
	int_32 mono_converter;
	sample_stereo_p data_stereo;
	sword *data_mono;
	if (stereo) //Stereo processing?
	{
		data_stereo = (sample_stereo_p)buf; //The data in correct samples!
		for (;;) //Fill it!
		{
			//Left and right samples are the same: we're a mono signal!
			readDoubleBufferedSound32(doublebuffer, &last); //Generate a stereo sample if it's available!
			buffer = last; //Load the last sample for processing!
			data_stereo->l = unsigned2signed16((word)buffer); //Load the last generated sample(left)!
			buffer >>= 16; //Shift low!
			data_stereo->r = unsigned2signed16((word)buffer); //Load the last generated sample(right)!
			++data_stereo; //Next stereo sample!
			if (!--c) return SOUNDHANDLER_RESULT_FILLED; //Next item!
		}
	}
	else //Mono processing?
	{
		data_mono = (sword *)buf; //The data in correct samples!
		for (;;) //Fill it!
		{
			//Left and right samples are the same: we're a mono signal!
			readDoubleBufferedSound32(doublebuffer, &last); //Generate a stereo sample if it's available!
			buffer = last; //Load the last sample for processing!
			mono_converter = unsigned2signed16((word)buffer); //Load the last generated sample(left)!
			buffer >>= 16; //Shift low!
			mono_converter += unsigned2signed16((word)buffer); //Load the last generated sample(right)!
			mono_converter = LIMITRANGE(mono_converter, SHRT_MIN, SHRT_MAX); //Clip our data to prevent overflow!
			*data_mono++ = mono_converter; //Save the sample and point to the next mono sample!
			if (!--c) return SOUNDHANDLER_RESULT_FILLED; //Next item!
		}
	}
}

void DSP_writeData(byte data)
{
	switch (SOUNDBLASTER.command) //What command?
	{
	case 0x10: //Direct DAC output?
		SOUNDBLASTER.DirectDACOutput = (int)data; //Set the direct DAC output!
		SOUNDBLASTER.command = -1; //No command anymore!
		break;
	case 0x40: //Set Time Constant?
		//timer rate: 1000000000.0 / __SOUNDBLASTER_SAMPLERATE
		//TimeConstant = 256 - (1000000 / (SampleChannels * SampleRate)), where SampleChannels is 1 for non-SBPro.
		soundblaster_sampletick = 1000000000.0/((256-data)*1000000); //Tick at the sample rate!
		SOUNDBLASTER.command = -1; //No command anymore!
		break;
	case 0x80: //Silence DAC?
		switch (SOUNDBLASTER.commandstep++)
		{
		case 0: //Length lo byte?
			SOUNDBLASTER.silenceoutput = data; //Set the data (low byte)
			break;
		case 1: //Length hi byte?
			SOUNDBLASTER.silenceoutput |= ((word)data<<8); //Set the samples to be silent!
			SOUNDBLASTER.silencesamples = SOUNDBLASTER.silenceoutput; //How many samples to be silent!
			break;
		}
		break;
	default: //Unknown command?
		break; //Simply ignore anything sent!
	}
}

void DSP_writeCommand(byte command)
{
	switch (command) //What command?
	{
	case 0x10: //Direct DAC, 8-bit
		SOUNDBLASTER.command = 0x10; //Enable direct DAC mode!
		break;
	case 0x14: //DMA DAC, 8-bit
		break;
	case 0x16: //DMA DAC, 2-bit ADPCM
	case 0x17: //DMA DAC, 2-bit ADPCM reference
		break;
	case 0x20: //Direct ADC, 8-bit
		SOUNDBLASTER.command = 0x20; //Enable direct ADC mode!
		writefifobuffer(SOUNDBLASTER.DSPindata,0x80); //Give an empty sample(silence)!
		fifobuffer_gotolast(SOUNDBLASTER.DSPindata); //Give the result!
		break;
	case 0x24: //DMA ADC, 8-bit
		break;
	case 0x30: //MIDI read poll
		break;
	case 0x31: //MIDI read interrupt
		break;
	case 0x38: //MIDI write poll
		break;
	case 0x40: //Set Time Constant
		SOUNDBLASTER.command = 0x40; //Set the time constant!
		break;
	case 0x74: //DMA DAC, 4-bit ADPCM
	case 0x75: //DMA DAC, 4-bit ADPCM Reference
		break;
	case 0x76: //DMA DAC, 2.6-bit ADPCM
	case 0x77: //DMA DAC, 2.6-bit ADPCM Reference
		break;
	case 0x80: //Silence DAC
		SOUNDBLASTER.command = 0x80; //Start the command!
		SOUNDBLASTER.commandstep = 0; //Reset the output step!
		break;
	case 0xD0: //Halt DMA operation, 8-bit
		break;
	case 0xD1: //Enable Speaker
		SOUNDBLASTER.muted = 0; //Not muted anymore!
		break;
	case 0xD3: //Disable Speaker
		SOUNDBLASTER.muted = 1; //Muted!
		SOUNDBLASTER.singen = 0; //Disable the sine wave generator!
		break;
	case 0xD4: //Continue DMA operation, 8-bit
		break;
	case 0xD8: //Speaker Status
		writefifobuffer(SOUNDBLASTER.DSPindata,SOUNDBLASTER.muted?0x00:0xFF); //Give the correct status!
		fifobuffer_gotolast(SOUNDBLASTER.DSPindata); //Give the output!
		break;
	case 0xE1: //DSP version
		writefifobuffer(SOUNDBLASTER.DSPindata, 1); //Give the correct version!
		fifobuffer_gotolast(SOUNDBLASTER.DSPindata); //Give the output!
		writefifobuffer(SOUNDBLASTER.DSPindata, 5); //Give the correct version!
		break;
	case 0xF0: //Sine Generator
		//Generate 2kHz signal!
		SOUNDBLASTER.muted = 0; //Not muted, give the diagnostic signal until muted!
		SOUNDBLASTER.singen = 1; //Enable the sine wave generator!
		SOUNDBLASTER.singentime = 0.0f; //Reset time on the sine wave generator!
		break;
	case 0xF2: //IRQ Request, 8-bit
		doirq(__SOUNDBLASTER_IRQ8); //Trigger the IRQ for 8-bit transfers!
		break;
	default: //Unknown command?
		break;
	}
}

byte DSP_readData()
{
	byte result;
	readfifobuffer(SOUNDBLASTER.DSPindata, &result); //Read the result, if any!
	return result; //Give the data!
}

void DSP_writeDataCommand(byte value)
{
	if (SOUNDBLASTER.busy == 0) //Ready to process?
	{
		if (SOUNDBLASTER.command != -1) //Handling data?
		{
			DSP_writeData(value); //Writing data!
		}
		else //Writing a command?
		{
			DSP_writeCommand(value); //Writing command!
		}
	}
}

byte inSoundBlaster(word port, byte *result)
{
	byte dummy;
	if ((port&~0xF)!=SOUNDBLASTER.baseaddr) return 0; //Not our base address?
	switch (port & 0xF) //What port?
	{
	case 8: //FM Music - Compatible Status port
		*result = readadlibstatus(); //Read the adlib status!
		return 1; //Handled!
		break;
	case 0xA: //DSP - Read data
		*result = DSP_readData(); //Read the result, if any!
		return 1; //Handled!
		break;
	case 0xC: //DSP - Write Buffer Status
		*result = SOUNDBLASTER.busy?0x80:0x00; //Are we ready to write data? 0x80=Not ready to write.
		return 1; //Handled!
	case 0xE: //DSP - Data Available Status, DSP - IRQ Acknowledge, 8-bit
		*result = (peekfifobuffer(SOUNDBLASTER.DSPindata,&dummy)<<7); //Do we have data available?
		SOUNDBLASTER.IRQ8Pending = 0; //Not pending anymore!
		return 1; //We have a result!
	default:
		break;
	}
	return 0; //Not supported yet!
}

byte outSoundBlaster(word port, byte value)
{
	if ((port&~0xF) != SOUNDBLASTER.baseaddr) return 0; //Not our base address?
	switch (port & 0xF) //What port?
	{
	case 0x6: //DSP - Reset?
		if ((value == 0) && (SOUNDBLASTER.resetport == 1)) //1 going 0? Perform reset!
		{
			SOUNDBLASTER.command = -1; //No command!
			SOUNDBLASTER.busy = 0; //Not busy!

			writefifobuffer(SOUNDBLASTER.DSPindata,0xAA); //We've reset!
			fifobuffer_gotolast(SOUNDBLASTER.DSPindata); //Force the data to the user!
		}
		SOUNDBLASTER.resetport = value; //Save the value for comparision!
		return 1; //Handled!
		break;
	case 0x8: //FM Music - Compatible Register port
		writeadlibaddr(value); //Write to the address port!
		return 1; //Handled!
		break;
	case 0x9: //FM Music - Compatible Data register
		writeadlibdata(value); //Write to the data port!
		return 1; //Handled!
	case 0xC: //DSP - Write Data or Command
		DSP_writeDataCommand(value); //Write data or command!
		return 1; //Handled!
	default:
		break;
	}
	return 0; //Not supported yet!
}

void StartPendingSoundBlasterIRQ(byte IRQ)
{
	SOUNDBLASTER.IRQ8Pending = 1; //We're starting to pend our IRQ!
}

byte SoundBlaster_readDMA8()
{
	byte result;
	readfifobuffer(SOUNDBLASTER.DSPindata,&result); //Read anyway!
	return result; //Give the data read!
}

void SoundBlaster_writeDMA8(byte data)
{
	DSP_writeDataCommand(data); //Write the Data/Command!
}

void SoundBlaster_DREQ()
{
	DMA_SetDREQ(__SOUNDBLASTER_DMA8,SOUNDBLASTER.DREQ && (SOUNDBLASTER.IRQ8Pending==0)); //Set the DREQ signal accordingly!
}

void SoundBlaster_DACK()
{
	//We're transferring something?
	SOUNDBLASTER.busy = 1; //We're busy!
}

void SoundBlaster_TC()
{
	//We're finished?
	SOUNDBLASTER.busy = 0; //We're not busy anymore!
}

void initSoundBlaster(word baseaddr)
{
	SOUNDBLASTER.baseaddr = 0; //Default: no sound blaster emulation!
	if (SOUNDBLASTER.DSPindata = allocfifobuffer(__SOUNDBLASTER_DSPINDATASIZE,0)) //DSP read data buffer!
	{
		if (SOUNDBLASTER.DSPoutdata = allocfifobuffer(__SOUNDBLASTER_DSPOUTDATASIZE,0)) //DSP write data buffer!
		{
			if (allocDoubleBufferedSound16(__SOUNDBLASTER_SAMPLEBUFFERSIZE, &SOUNDBLASTER.soundbuffer, 0)) //Valid buffer?
			{
				if (!addchannel(&SoundBlaster_soundGenerator, &SOUNDBLASTER.soundbuffer, "SoundBlaster", __SOUNDBLASTER_SAMPLERATE, __SOUNDBLASTER_SAMPLEBUFFERSIZE, 0, SMPL16U)) //Start the sound emulation (mono) with automatic samples buffer?
				{
					dolog("adlib", "Error registering sound channel for output!");
				}
				else
				{
					setVolume(&SoundBlaster_soundGenerator, NULL, SOUNDBLASTER_VOLUME);
					SOUNDBLASTER.baseaddr = baseaddr; //The base address to use!
				}
			}
			else
			{
				dolog("adlib", "Error registering double buffer for output!");
			}
		}
	}

	SOUNDBLASTER.resetport = 0xFF; //Reset the reset port!
	SOUNDBLASTER.busy = 0; //Default to not busy!
	SOUNDBLASTER.DREQ = 0; //Not requesting anything!
	SOUNDBLASTER.IRQ8Pending = 0; //Not pending anything!
	SOUNDBLASTER.DirectDACOutput = -1; //Disable Direct DAC output!
	SOUNDBLASTER.muted = 1; //Default: muted!
	leftsample = rightsample = 0x80; //Default to silence!

	register_PORTIN(&inSoundBlaster); //Status port (R)
	//All output!
	register_PORTOUT(&outSoundBlaster); //Address port (W)
	registerIRQ(__SOUNDBLASTER_IRQ8,&StartPendingSoundBlasterIRQ,NULL); //Pending SB IRQ only!
	registerDMA8(__SOUNDBLASTER_DMA8,&SoundBlaster_readDMA8,&SoundBlaster_writeDMA8); //DMA access of the Sound Blaster!
	registerDMATick(__SOUNDBLASTER_DMA8,&SoundBlaster_DREQ,&SoundBlaster_DACK,&SoundBlaster_TC);
}

void doneSoundBlaster()
{
	removechannel(&SoundBlaster_soundGenerator, NULL, 0); //Stop the sound emulation?
	freeDoubleBufferedSound(&SOUNDBLASTER.soundbuffer); //Free our double buffered sound!
	free_fifobuffer(&SOUNDBLASTER.DSPindata); //Release our input buffer!
	free_fifobuffer(&SOUNDBLASTER.DSPoutdata); //Release our output buffer!
}