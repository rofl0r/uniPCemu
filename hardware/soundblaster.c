#include "headers/types.h" //Basic types!
#include "headers/support/sounddoublebuffer.h" //Double buffered sound support!
#include "headers/emu/sound.h" //Sound output support!
#include "headers/support/signedness.h" //Sign support!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/ports.h" //I/O support!
#include "headers/hardware/adlib.h" //Adlib card has more ports with Sound Blasters!
#include "headers/hardware/pic.h" //Interrupt support!
#include "headers/hardware/8237A.h" //DMA support!

#define __SOUNDBLASTER_SAMPLERATE 22222.0f
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
	word wordparamoutput;
	uint_32 silencesamples; //Silence samples left!
	byte muted; //Is speaker output disabled?
	byte singen; //Sine wave generator enabled?
	double singentime; //Sine wave generator position in time!
	byte DMAEnabled; //DMA not paused?
	word translatetable[0x100]; //8-bit to 16-bit unsigned translation table!
} SOUNDBLASTER; //The Sound Blaster data!

double soundblaster_soundtiming = 0.0, soundblaster_soundtick = 1000000000.0 / __SOUNDBLASTER_SAMPLERATE;
double soundblaster_sampletiming = 0.0, soundblaster_sampletick = 0.0;

byte leftsample=0x80, rightsample=0x80; //Two stereo samples, silence by default!

void updateSoundBlaster(double timepassed)
{
	double dummy;
	double temp;
	byte monosample; //Mono sample!
	if (SOUNDBLASTER.baseaddr == 0) return; //No game blaster?

	//First, check for any static sample(Direct Output)
	if ((SOUNDBLASTER.DirectDACOutput != -1) && (SOUNDBLASTER.silencesamples==0)) //Direct DAC output and not silenced?
	{
		leftsample = rightsample = (byte)SOUNDBLASTER.DirectDACOutput; //Mono Direct DAC output!
	}

	if (SOUNDBLASTER.DREQ || SOUNDBLASTER.silencesamples) //Transaction busy?
	{
		//Play audio normally using timed output!
		soundblaster_sampletiming += timepassed; //Tick time!
		if ((soundblaster_sampletiming>=soundblaster_sampletick) && (soundblaster_sampletick>0.0)) //Expired?
		{
			for (;soundblaster_sampletiming>=soundblaster_sampletick;) //A sample to play?
			{
				if (SOUNDBLASTER.silencesamples) //Silence requested?
				{
					SOUNDBLASTER.DirectDACOutput = (int)0x80; //Silent sample!
					if (--SOUNDBLASTER.silencesamples == 0) //Decrease the sample counter! If expired, fire IRQ!
					{
						doirq(__SOUNDBLASTER_IRQ8); //Fire the IRQ!
						SOUNDBLASTER.DMAEnabled |= 2; //We're a paused DMA transaction automatically!
					}
				}
				else //Audio playing?
				{
					if (readfifobuffer(SOUNDBLASTER.DSPoutdata, &monosample)) //Mono sample read?
					{
						leftsample = rightsample = monosample; //Render the new mono sample!
					}

					if (fifobuffer_freesize(SOUNDBLASTER.DSPoutdata)==__SOUNDBLASTER_DSPOUTDATASIZE) //Empty buffer? We've finished rendering the samples specified!
					{
						//Time played audio that's ready!
						if (SOUNDBLASTER.DREQ && (SOUNDBLASTER.DREQ & 2)) //Paused until the next sample?
						{
							SOUNDBLASTER.DREQ &= ~2; //Start us up again, if allowed!
						}
					}
				}
				soundblaster_sampletiming -= soundblaster_sampletick; //A sample has been ticked!
			}
		}
	}

	if (SOUNDBLASTER.singen) //Diagnostic Sine wave generator enabled?
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
			writeDoubleBufferedSound32(&SOUNDBLASTER.soundbuffer, (SOUNDBLASTER.translatetable[rightsample]<<16) | (SOUNDBLASTER.translatetable[leftsample])); //Output the sample to the renderer!
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

void SoundBlaster_IRQ8()
{
	SOUNDBLASTER.IRQ8Pending |= 2; //We're actually pending!
	doirq(__SOUNDBLASTER_IRQ8); //Trigger the IRQ for 8-bit transfers!
}

void DSP_writeCommand(byte command)
{
	switch (command) //What command?
	{
	case 0x10: //Direct DAC, 8-bit
		SOUNDBLASTER.command = 0x10; //Enable direct DAC mode!
		break;
	case 0x14: //DMA DAC, 8-bit
		SOUNDBLASTER.commandstep = 0; //We're at the parameter phase!
		SOUNDBLASTER.command = 0x14; //Starting this command!
		SOUNDBLASTER.dataleft = 0; //counter of parameters!
		break;
	case 0x16: //DMA DAC, 2-bit ADPCM
	case 0x17: //DMA DAC, 2-bit ADPCM reference
		break;
	case 0x20: //Direct ADC, 8-bit
		SOUNDBLASTER.command = 0x20; //Enable direct ADC mode!
		writefifobuffer(SOUNDBLASTER.DSPindata, 0x80); //Give an empty sample(silence)!
		fifobuffer_gotolast(SOUNDBLASTER.DSPindata); //Give the result!
		break;
	case 0x24: //DMA ADC, 8-bit
		SOUNDBLASTER.commandstep = 0; //We're at the parameter phase!
		SOUNDBLASTER.command = 0x24; //Starting this command!
		SOUNDBLASTER.dataleft = 0; //counter of parameters!
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
		if (SOUNDBLASTER.DREQ && SOUNDBLASTER.DMAEnabled) //DMA enabled? Busy transaction!
		{
			SOUNDBLASTER.DMAEnabled |= 2; //We're a paused DMA transaction now!
		}
		break;
	case 0xD1: //Enable Speaker
		SOUNDBLASTER.muted = 0; //Not muted anymore!
		break;
	case 0xD3: //Disable Speaker
		SOUNDBLASTER.muted = 1; //Muted!
		SOUNDBLASTER.singen = 0; //Disable the sine wave generator!
		break;
	case 0xD4: //Continue DMA operation, 8-bit
		if (SOUNDBLASTER.DREQ && SOUNDBLASTER.DMAEnabled) //DMA enabled? Busy transaction!
		{
			SOUNDBLASTER.DMAEnabled &= ~2; //We're a continuing DMA transaction now!
		}
		break;
	case 0xD8: //Speaker Status
		writefifobuffer(SOUNDBLASTER.DSPindata, SOUNDBLASTER.muted ? 0x00 : 0xFF); //Give the correct status!
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
		SoundBlaster_IRQ8();
		break;
	case 0xF8: //Undocumented command according to Dosbox
		writefifobuffer(SOUNDBLASTER.DSPindata,0x00); //Give zero bytes!
		fifobuffer_gotolast(SOUNDBLASTER.DSPindata); //Use the given result!
		break;
	default: //Unknown command?
		break;
	}
}

void DSP_writeData(byte data, byte isDMA)
{
	switch (SOUNDBLASTER.command) //What command?
	{
	case 0x10: //Direct DAC output?
		SOUNDBLASTER.DirectDACOutput = (int)data; //Set the direct DAC output!
		SOUNDBLASTER.DMAEnabled = 0; //Disable DMA transaction!
		SOUNDBLASTER.DREQ = 0; //Lower DREQ!
		SOUNDBLASTER.command = -1; //No command anymore!
		fifobuffer_clear(SOUNDBLASTER.DSPoutdata); //Clear the output buffer to use this sample!
		break;
	case 0x40: //Set Time Constant?
		//timer rate: 1000000000.0 / __SOUNDBLASTER_SAMPLERATE
		//TimeConstant = 256 - (1000000(us) / (SampleChannels * SampleRate)), where SampleChannels is 1 for non-SBPro.
		soundblaster_sampletick = 1000000000.0/(1000000.0/(double)(256-data)); //Tick at the sample rate!
		SOUNDBLASTER.command = -1; //No command anymore!
		break;
	case 0x14: //DMA DAC, 8-bit
		if (SOUNDBLASTER.commandstep) //DMA transfer active?
		{
			if (isDMA) //Must be DMA transfer!
			{
				SOUNDBLASTER.DirectDACOutput = -1; //No direct DAC output left until next sample: terminate output for now!
				writefifobuffer(SOUNDBLASTER.DSPoutdata,data); //Send the current sample for rendering!
				if (--SOUNDBLASTER.dataleft==0) //One data used! Finished? Give IRQ!
				{
					doirq(__SOUNDBLASTER_IRQ8); //Raise the 8-bit IRQ!
					SOUNDBLASTER.dataleft = SOUNDBLASTER.wordparamoutput + 1; //Reload the length of the DMA transfer to play back, in bytes!
				}
				SOUNDBLASTER.DREQ |= 2; //Wait for the next sample to be played, according to the sample rate!
			}
			else //Manual override?
			{
				DSP_writeCommand(data); //Override to command instead!
			}
		}
		else //Parameter phase?
		{
			switch (SOUNDBLASTER.dataleft++) //What step?
			{
			case 0: //Length lo byte!
				SOUNDBLASTER.wordparamoutput = (word)data; //The first parameter!
				break;
			case 1: //Length hi byte!
				SOUNDBLASTER.wordparamoutput |= (((word)data)<<8); //The second parameter!
				SOUNDBLASTER.dataleft = SOUNDBLASTER.wordparamoutput+1; //The length of the DMA transfer to play back, in bytes!
				SOUNDBLASTER.DREQ = 1; //Raise: we're outputting data for playback!
				if (SOUNDBLASTER.DMAEnabled==0) //DMA Disabled?
				{
					SOUNDBLASTER.DMAEnabled = 1; //Start the DMA transfer fully itself!
				}
				SOUNDBLASTER.commandstep = 1; //Goto step 1!
				break;
			}
		}
		break;
	case 0x24: //DMA ADC, 8-bit
		if (SOUNDBLASTER.commandstep) //DMA transfer active?
		{
			if (isDMA) //Must be DMA transfer!
			{
				//Writing from DMA during recording??? Must be misconfigured! Ignore the writes!
			}
			else //Manual override?
			{
				DSP_writeCommand(data); //Override to command instead!
			}
		}
		else //Parameter phase?
		{
			switch (SOUNDBLASTER.dataleft++) //What step?
			{
			case 0: //Length lo byte!
				SOUNDBLASTER.wordparamoutput = (word)data; //The first parameter!
				break;
			case 1: //Length hi byte!
				SOUNDBLASTER.wordparamoutput |= (((word)data) << 8); //The second parameter!
				SOUNDBLASTER.dataleft = SOUNDBLASTER.wordparamoutput + 1; //The length of the DMA transfer to play back, in bytes!
				SOUNDBLASTER.DREQ = 1; //Raise: we're outputting data for playback!
				if (SOUNDBLASTER.DMAEnabled == 0) //DMA Disabled?
				{
					SOUNDBLASTER.DMAEnabled = 1; //Start the DMA transfer fully itself!
				}
				SOUNDBLASTER.commandstep = 1; //Goto step 1!
				break;
			}
		}
		break;
	case 0x80: //Silence DAC?
		SOUNDBLASTER.DREQ = 0; //Lower DREQ!
		switch (SOUNDBLASTER.commandstep++)
		{
		case 0: //Length lo byte?
			SOUNDBLASTER.wordparamoutput = data; //Set the data (low byte)
			break;
		case 1: //Length hi byte?
			SOUNDBLASTER.wordparamoutput |= ((word)data<<8); //Set the samples to be silent!
			SOUNDBLASTER.silencesamples = SOUNDBLASTER.wordparamoutput; //How many samples to be silent!
			break;
		}
		break;
	default: //Unknown command?
		break; //Simply ignore anything sent!
	}
}

byte DSP_readData(byte isDMA)
{
	byte result=0x00;
	if ((isDMA == 0) && (SOUNDBLASTER.command == 0x24)) //DMA ADC with normal read?
	{
		return 0x00; //Ignore the input buffer: this is used by the DMA!
	}
	readfifobuffer(SOUNDBLASTER.DSPindata, &result); //Read the result, if any!
	return result; //Give the data!
}

void DSP_writeDataCommand(byte value)
{
	if (SOUNDBLASTER.command != -1) //Handling data?
	{
		DSP_writeData(value,0); //Writing data!
	}
	else //Writing a command?
	{
		DSP_writeCommand(value); //Writing command!
	}
}

byte readDSPData(byte isDMA)
{
	switch (SOUNDBLASTER.command) //What command?
	{
	case 0x24: //DMA ADC, 8-bit
		if (SOUNDBLASTER.commandstep) //DMA transfer active?
		{
			if (isDMA) //Must be DMA transfer!
			{
				SOUNDBLASTER.DirectDACOutput = -1; //No direct DAC output left until next sample: terminate output for now!
				writefifobuffer(SOUNDBLASTER.DSPindata, 0x80); //Send the current sample for rendering!
				if (--SOUNDBLASTER.dataleft == 0) //One data used! Finished? Give IRQ!
				{
					doirq(__SOUNDBLASTER_IRQ8); //Raise the 8-bit IRQ!
					SOUNDBLASTER.dataleft = SOUNDBLASTER.wordparamoutput + 1; //Reload the length of the DMA transfer to play back, in bytes!
				}
				SOUNDBLASTER.DREQ |= 2; //Wait for the next sample to be played, according to the sample rate!			}
				return 1; //Handled!
			}
		}
		break;
	default: //Unknown command?
		break; //Simply ignore anything sent!
	}

	return 0x00; //Unknown!
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
		readDSPData(0); //Check if there's anything to read, ignore the result!
		*result = DSP_readData(0); //Read the result, if any!
		return 1; //Handled!
		break;
	case 0xC: //DSP - Write Buffer Status
		*result = SOUNDBLASTER.busy?0x80:0x00; //Are we ready to write data? 0x80=Not ready to write.
		return 1; //Handled!
	case 0xE: //DSP - Data Available Status, DSP - IRQ Acknowledge, 8-bit
		*result = (peekfifobuffer(SOUNDBLASTER.DSPindata,&dummy)<<7); //Do we have data available?
		if (SOUNDBLASTER.command == 0x24) //Special: no result, as this is buffered by DMA!
		{
			*result = 0x00; //Give no input set!
		}
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
			SOUNDBLASTER.silencesamples = 0; //No silenced samples!
			SOUNDBLASTER.DirectDACOutput = -1; //No direct DAC output!
			SOUNDBLASTER.IRQ8Pending = 0; //No IRQ pending!
			SOUNDBLASTER.singen = 0; //Disable the sine wave generator if it's running!

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
	if (SOUNDBLASTER.IRQ8Pending) //Actually pending?
	{
		SOUNDBLASTER.IRQ8Pending |= 1; //We're starting to execute our IRQ, which has been acnowledged!
	}
}

byte SoundBlaster_readDMA8()
{
	byte result;
	readDSPData(1); //Gotten anything from DMA in the input buffer?
	readfifobuffer(SOUNDBLASTER.DSPindata,&result); //Read anyway, no matter the result!
	return result; //Give the data read!
}

void SoundBlaster_writeDMA8(byte data)
{
	DSP_writeData(data,1); //Write the Data/Command, DAC style!
}

void SoundBlaster_DREQ()
{
	DMA_SetDREQ(__SOUNDBLASTER_DMA8,(SOUNDBLASTER.DREQ==1) && (SOUNDBLASTER.IRQ8Pending==0) && (SOUNDBLASTER.DMAEnabled==1)); //Set the DREQ signal accordingly!
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
	word translatevalue;
	float tmp;
	SOUNDBLASTER.baseaddr = 0; //Default: no sound blaster emulation!
	if (SOUNDBLASTER.DSPindata = allocfifobuffer(__SOUNDBLASTER_DSPINDATASIZE,0)) //DSP read data buffer!
	{
		if (SOUNDBLASTER.DSPoutdata = allocfifobuffer(__SOUNDBLASTER_DSPOUTDATASIZE,0)) //DSP write data buffer!
		{
			if (allocDoubleBufferedSound32(__SOUNDBLASTER_SAMPLEBUFFERSIZE, &SOUNDBLASTER.soundbuffer, 0)) //Valid buffer?
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
	SOUNDBLASTER.DMAEnabled = 0; //Start with disabled DMA(paused)!
	leftsample = rightsample = 0x80; //Default to silence!

	for (translatevalue = 0;translatevalue < 0x100;++translatevalue) //All translatable values!
	{
		tmp = (float)((sword)translatevalue-0x80); //Convert to signed value!
		tmp *= (1.0f/128.0f)*32768.0f; //Convert to destination range!
		tmp += 0x8000; //Destination value!
		tmp = LIMITRANGE(tmp,0.0f,(float)USHRT_MAX); //Limit to our range!
		SOUNDBLASTER.translatetable[translatevalue] = (word)tmp; //Translate values!
	}

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