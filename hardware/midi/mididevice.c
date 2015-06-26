#include "headers/types.h" //Basic types!
#include "headers/support/sf2.h" //Soundfont support!
#include "headers/hardware/midi/mididevice.h" //Our own typedefs!
#include "headers/support/zalloc.h" //Zero allocation support!
#include "headers/emu/sound.h" //Sound support!
#include "headers/support/log.h" //Logging support!
#include "headers/support/highrestimer.h" //High resolution timer support!
#include "headers/hardware/midi/adsr.h" //ADSR support!

//Use direct windows MIDI processor if available?
//#define DIRECT_MIDI

#ifdef _WIN32
#ifdef DIRECT_MIDI
#include <mmsystem.h>  /* multimedia functions (such as MIDI) for Windows */
#endif
#endif

//Are we disabled?
//#define __HW_DISABLED
RIFFHEADER *soundfont; //Our loaded soundfont!

//To log MIDI commands?
//#define MIDI_LOG
//To log MIDI rendering timing?
//#define LOG_MIDI_TIMING

//On/off controller bit values!
#define MIDI_CONTROLLER_ON 0x40

//Poly and Omni flags in the Mode Selection.
//Poly: Enable multiple voices per channel. When set to Mono, All Notes Off on the channel when a Note On is received.
#define MIDIDEVICE_POLY 0x1
//Omni: Ignore channel number of the message during note On/Off commands.
#define MIDIDEVICE_OMNI 0x2

//Default mode is Omni Off, Poly
#define MIDIDEVICE_DEFAULTMODE MIDIDEVICE_POLY

struct
{
	byte UARTMode;
	MIDIDEVICE_CHANNEL channels[0x10]; //Stuff for all channels!
} MIDIDEVICE; //Current MIDI device data!

MIDIDEVICE_VOICE activevoices[__MIDI_NUMVOICES]; //All active voices!

OPTINLINE void MIDIDEVICE_execMIDI(MIDIPTR current); //MIDI device UART mode execution!

/* MIDI direct output support*/

#ifdef _WIN32
#ifdef DIRECT_MIDI
int flag;           // monitor the status of returning functions
HMIDIOUT device;    // MIDI device interface for sending MIDI output
#endif
#endif

/* Buffer support */

void MIDIDEVICE_addbuffer(byte command, MIDIPTR data) //Add a command to the buffer!
{
	#ifdef __HW_DISABLED
	return; //We're disabled!
	#endif

	#ifdef _WIN32
	#ifdef DIRECT_MIDI
		//We're directly sending MIDI to the output!
		union { unsigned long word; unsigned char data[4]; } message;
		message.data[0] = command; //The command!
		message.data[1] = data->buffer[0];
		message.data[2] = data->buffer[1];
		message.data[3] = 0; //Unused!
		switch (command&0xF0) //What command?
		{
		case 0x80:
		case 0x90:
		case 0xA0:
		case 0xB0:
		case 0xC0:
		case 0xD0:
		case 0xE0:
		case 0xF0:
			if (command != 0xFF) //Not resetting?
			{
				flag = midiOutShortMsg(device, message.word);
				if (flag != MMSYSERR_NOERROR) {
					printf("Warning: MIDI Output is not open.\n");
				}
			}
			else
			{
				// turn any MIDI notes currently playing:
				midiOutReset(device);
			}
			break;
		}
		return; //Stop: ready!
	#endif
	#endif

	data->command = command; //Set the command to use!
	MIDIDEVICE_execMIDI(data); //Execute directly!
}

/* Reset support */

OPTINLINE void reset_MIDIDEVICE() //Reset the MIDI device for usage!
{
	//First, our variables!
	byte channel;
	word notes;

	lockaudio();
	memset(&MIDIDEVICE,0,sizeof(MIDIDEVICE)); //Clear our data!
	memset(&activevoices,0,sizeof(activevoices)); //Clear our active voices!
	for (channel=0;channel<0x10;)
	{
		for (notes=0;notes<0x100;)
		{
			MIDIDEVICE.channels[channel].notes[notes].channel = channel;
			MIDIDEVICE.channels[channel].notes[notes].note = notes;
			++notes; //Next note!
		}
		MIDIDEVICE.channels[channel].bank = MIDIDEVICE.channels[channel].activebank = 0; //Reset!
		MIDIDEVICE.channels[channel].channelrangemin = MIDIDEVICE.channels[channel].channelrangemax = channel; //We respond to this channel only!
		MIDIDEVICE.channels[channel].control = 0; //First instrument!
		MIDIDEVICE.channels[channel].pitch = 0x2000; //Centered pitch = Default pitch!
		MIDIDEVICE.channels[channel].pressure = 0x40; //Centered pressure!
		MIDIDEVICE.channels[channel].program = 0; //First program!
		MIDIDEVICE.channels[channel].sustain = 0; //Disable sustain!
		MIDIDEVICE.channels[channel].volume = 0x2000; //Centered volume as the default volume!
		MIDIDEVICE.channels[channel].panposition = 0x2000; //Centered pan position as the default pan!
		MIDIDEVICE.channels[channel].lvolume = MIDIDEVICE.channels[channel].rvolume = 0.5; //Accompanying the pan position: centered volume!
		MIDIDEVICE.channels[channel++].mode = MIDIDEVICE_DEFAULTMODE; //Use the default mode!
	}
	unlockaudio(1);
}

/*

Cents and DB conversion!

*/

//Low&high pass filters!

OPTINLINE float calcLowpassFilter(float cutoff_freq, float samplerate, float currentsample, float previoussample, float previousresult)
{
	float RC = 1.0 / (cutoff_freq * 2 * 3.14);
	float dt = 1.0 / samplerate;
	float alpha = dt / (RC + dt);
	return previousresult + (alpha*(currentsample - previousresult));
}

void applyLowpassFilter(MIDIDEVICE_VOICE *voice, sword *currentsample)
{
	if (!voice->lowpassfilter_freq) //No filter?
	{
		voice->has_last = 0; //No last (anymore)!
		return; //Abort: nothing to filter!
	}
	if (!voice->has_last) //No last?
	{
		voice->last_result = voice->last_sample = *currentsample; //Save the current sample!
		voice->has_last = 1;
		return; //Abort: don't filter the first sample!
	}
	voice->last_result = calcLowpassFilter(voice->lowpassfilter_freq, voice->sample.dwSampleRate, *currentsample, voice->last_sample, voice->last_result);
	voice->last_sample = *currentsample; //The last sample that was processed!
	*currentsample = voice->last_result; //Give the new result!
}

/*

Voice support

*/

OPTINLINE void MIDIDEVICE_getsample(sample_stereo_t *sample, int_64 play_counter, float samplespeedup, MIDIDEVICE_VOICE *voice, float Volume) //Get a sample from an MIDI note!
{
	//Our current rendering routine:
	register uint_32 temp;
	register int_64 temppos;
	register int_64 samplepos;
	sword lchannel, rchannel; //Both channels to use!
	static sword readsample; //The sample retrieved!
	byte loopflags; //Flags used during looping!

	samplepos = play_counter; //Load the current play counter!
	samplepos *= samplespeedup; //Affect speed through cents and other factors!
	samplepos += voice->startaddressoffset; //The start of the sample!

	//First: apply looping!
	loopflags = voice->currentloopflags;
	if (voice->has_finallooppos && (samplepos >= voice->finallooppos)) //Executing final loop?
	{
		samplepos -= voice->finallooppos; //Take the relative offset to the start of the final loop!
		samplepos += voice->finallooppos_playcounter; //Add the relative offset to the start of our data of the final loop!
	}
	else if (loopflags & 1) //Currently looping and active?
	{
		if (samplepos >= voice->endloopaddressoffset) //Past/at the end of the loop!
		{
			if ((loopflags & 0xC2) == 0x82) //We're depressed, depress action is allowed (not holding) and looping until depressed?
			{
				if (!voice->has_finallooppos) //No final loop position set yet?
				{
					voice->currentloopflags &= ~0x80; //Clear depress bit!
					//Loop for the last time!
					voice->finallooppos = samplepos; //Our new position for our final execution till the end!
					voice->has_finallooppos = 1; //We have a final loop position set!
					loopflags |= 0x20; //We're to update our final loop start!
				}
			}

			//Loop according to loop data!
			temp = voice->startloopaddressoffset; //The actual start of the loop!
			//Loop the data!
			samplepos -= temp; //Take the ammount past the start of the loop!
			samplepos %= voice->loopsize; //Loop past startloop by endloop!
			samplepos += temp; //The destination position within the loop!
			//Check for depress special actions!
			if (loopflags&0x20) //Extra information needed for the final loop?
			{
				voice->finallooppos_playcounter = samplepos; //The start position within the loop to use!
			}
		}
	}

	//Next, apply finish!
	loopflags = (samplepos >= voice->endaddressoffset) || (play_counter<0); //Expired or not started yet?
	if (loopflags) //Sound is finished?
	{
		sample->l = sample->r = 0; //No sample!
		return; //Done!
	}

	if (getSFSample16(soundfont, samplepos, &readsample)) //Sample found?
	{
		lchannel = (float)readsample; //Convert to floating point for our calculations!

		//First, apply filters and current envelope!
		applyLowpassFilter(voice, &lchannel); //Low pass filter!
		lchannel *= Volume; //Apply ADSR Volume envelope!
		//Now the sample is ready for output into the actual final volume!

		rchannel = lchannel; //Load into both channels!
		//Now, apply panning!
		lchannel *= voice->lvolume; //Apply left panning, also according to the CC!
		rchannel *= voice->rvolume; //Apply right panning, also according to the CC!

		//Give the result!
		sample->l = lchannel; //LChannel!
		sample->r = rchannel; //RChannel!
		return;
	}
	sample->l = sample->r = 0; //No sample to be found!
}

byte MIDIDEVICE_renderer(void* buf, uint_32 length, byte stereo, void *userdata) //Sound output renderer!
{
#ifdef __HW_DISABLED
	return 0; //We're disabled!
#endif
	//Initialisation info
	float pitchcents, pitchinfluence, currentsamplespeedup, lvolume, rvolume, panningtemp;
	byte currenton;
	uint_32 requestbit;
	register float VolumeEnvelope; //Current volume envelope data!
	//Initialised values!
	MIDIDEVICE_VOICE *voice = (MIDIDEVICE_VOICE *)userdata;
	sample_stereo_t* ubuf = (sample_stereo_t *)buf; //Our sample buffer!
	ADSR *VolumeADSR = &voice->VolumeEnvelope; //Our used volume envelope ADSR!
	MIDIDEVICE_CHANNEL *channel = voice->channel; //Get the channel to use!
	float velocity_factor; //Current velocity factor!
	uint_32 numsamples = length; //How many samples to buffer!
	++numsamples; //Take one sample more!

#ifdef LOG_MIDI_TIMING
	static TicksHolder ticks; //Our ticks holder!
	startHiresCounting(&ticks);
	ticksholder_AVG(&ticks); //Enable averaging!
#endif
	if (!voice->VolumeEnvelope.active) return SOUNDHANDLER_RESULT_NOTFILLED; //Empty buffer: we're unused!
	if (memprotect(soundfont,sizeof(*soundfont),"RIFF_FILE")!=soundfont) return SOUNDHANDLER_RESULT_NOTFILLED; //Empty buffer: we're unable to render anything!

	//Calculate the pitch bend speedup!
	pitchcents = (double)(channel->pitch%0x1FFF); //Load active pitch bend (unsigned), Only low 14 bits are used!
	pitchcents -= 0x2000; //Convert to a signed value!
	pitchcents /= 128.0f; //Create a value between -1 and 1!
	pitchcents *= cents2samplesfactor(voice->pitchwheelmod*pitchcents); //Influence by pitch wheel!

	//Now apply to the default speedup!
	currentsamplespeedup = voice->initsamplespeedup; //Load the default sample speedup for our tone!
	currentsamplespeedup *= cents2samplesfactor(pitchcents); //Calculate the sample speedup!; //Apply pitch bend!
	voice->effectivesamplespeedup = currentsamplespeedup; //Load the speedup of the samples we need!

	velocity_factor = voice->note->noteon_velocity_factor; //Apply Note On key velocity first!
	//velocity_factor *= ((float)(channel->pressure + 1) / 64.0f); //Adjust velocity, based on channel pressure, which can change during hold!
	//velocity_factor *= ((float)(voice->note->pressure + 1) / 64.0f); //Adjust velocity, based on note pressure, which can change during hold!


	//Determine panning!
	lvolume = rvolume = 0.5f; //Default to 50% each (center)!
	panningtemp = voice->initpanning; //Get the panning specified!
	panningtemp += voice->panningmod*((float)(voice->channel->panposition-0x2000)/128); //Apply panning CC!
	lvolume -= panningtemp; //Left percentage!
	rvolume += panningtemp; //Right percentage!

	voice->lvolume = lvolume; //Left panning!
	voice->rvolume = rvolume; //Right panning!


	if (voice->request_off) //Requested turn off?
	{
		voice->currentloopflags |= 0x80; //Request quit looping if needed: finish sound!
	} //Requested off?

	//Apply sustain
	voice->currentloopflags &= ~0x40; //Sustain disabled by default!
	voice->currentloopflags |= (channel->sustain << 6); //Sustaining?

	//Now produce the sound itself!
	for (; --numsamples;) //Produce the samples!
	{
		VolumeEnvelope = ADSR_tick(VolumeADSR,voice->play_counter,((voice->currentloopflags & 0xC0) != 0x80),velocity_factor, voice->note->noteoff_velocity); //Apply Volume Envelope!
		MIDIDEVICE_getsample(ubuf++, voice->play_counter++, voice->effectivesamplespeedup, voice, VolumeEnvelope); //Get the sample from the MIDI device!
	}

	voice->CurrentVolumeEnvelope = VolumeEnvelope; //Current volume envelope updated!

#ifdef LOG_MIDI_TIMING
	stopHiresCounting("MIDIDEV", "MIDIRenderer", &ticks); //Log our active counting!
#endif

	return SOUNDHANDLER_RESULT_FILLED; //We're filled!
}

byte MIDIDEVICE_newvoice(MIDIDEVICE_VOICE *voice, byte request_channel, byte request_note)
{
	static uint_64 starttime = 0; //Calculated start time!
	byte currentchannel, currenton, biton;
	word pbag, ibag;
	sword rootMIDITone; //Relative root MIDI tone!
	uint_32 preset, startaddressoffset, endaddressoffset, startloopaddressoffset, endloopaddressoffset, loopsize;
	float cents, tonecents, panningtemp, pitchwheeltemp;

	MIDIDEVICE_CHANNEL *channel;
	MIDIDEVICE_NOTE *note;
	sfPresetHeader currentpreset;
	sfGenList instrumentptr, applypgen;
	sfInst currentinstrument;
	sfInstGenList sampleptr, applyigen;
	sfModList applymod;

	if (memprotect(soundfont,sizeof(*soundfont),"RIFF_FILE")!=soundfont) return 0; //We're unable to render anything!
	if (voice->VolumeEnvelope.active) return 1; //Active voices can't be allocated!

	memset(voice, 0, sizeof(*voice)); //Clear the voice!

	//Check for requested voices!
	//First, all our variables!
	//Now, determine the actual note to be turned on!
	voice->channel = channel = &MIDIDEVICE.channels[request_channel]; //What channel!
	voice->note = note = &voice->channel->notes[request_note]; //What note!

	voice->play_counter = 0; //Reset play counter!

	//First, our precalcs!

	//Now retrieve our note by specification!

	if (!lookupPresetByInstrument(soundfont, channel->program, channel->activebank, &preset)) //Preset not found?
	{
		return 0; //No samples!
	}

	if (!getSFPreset(soundfont, preset, &currentpreset))
	{
		return 0;
	}

	if (!lookupPBagByMIDIKey(soundfont, preset, note->note, note->noteon_velocity, &pbag)) //Preset bag not found?
	{
		return 0; //No samples!
	}

	if (!lookupSFPresetGen(soundfont, preset, pbag, instrument, &instrumentptr))
	{
		return 0; //No samples!
	}

	if (!getSFInstrument(soundfont, instrumentptr.genAmount.wAmount, &currentinstrument))
	{
		return 0;
	}

	if (!lookupIBagByMIDIKey(soundfont, instrumentptr.genAmount.wAmount, note->note, note->noteon_velocity, &ibag, 1))
	{
		return 0; //No samples!
	}

	if (!lookupSFInstrumentGen(soundfont, instrumentptr.genAmount.wAmount, ibag, sampleID, &sampleptr))
	{
		return 0; //No samples!
	}

	if (!getSFSampleInformation(soundfont, sampleptr.genAmount.wAmount, &voice->sample))
	{
		return 0; //No samples!
	}

	//Determine the adjusting offsets!

	//Fist, init to defaults!
	startaddressoffset = voice->sample.dwStart;
	endaddressoffset = voice->sample.dwEnd;
	startloopaddressoffset = voice->sample.dwStartloop;
	endloopaddressoffset = voice->sample.dwEndloop;

	//Next, apply generators!
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, startAddrsOffset, &applyigen))
	{
		startaddressoffset += applyigen.genAmount.shAmount; //Apply!
	}
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, startAddrsCoarseOffset, &applyigen))
	{
		startaddressoffset += (applyigen.genAmount.shAmount << 15); //Apply!
	}

	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, endAddrsOffset, &applyigen))
	{
		endaddressoffset += applyigen.genAmount.shAmount; //Apply!
	}
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, endAddrsCoarseOffset, &applyigen))
	{
		endaddressoffset += (applyigen.genAmount.shAmount << 15); //Apply!
	}

	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, startloopAddrsOffset, &applyigen))
	{
		startloopaddressoffset += applyigen.genAmount.shAmount; //Apply!
	}
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, startloopAddrsCoarseOffset, &applyigen))
	{
		startloopaddressoffset += (applyigen.genAmount.shAmount << 15); //Apply!
	}

	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, endloopAddrsOffset, &applyigen))
	{
		endloopaddressoffset += applyigen.genAmount.shAmount; //Apply!
	}
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, endloopAddrsCoarseOffset, &applyigen))
	{
		endloopaddressoffset += (applyigen.genAmount.shAmount << 15); //Apply!
	}

	//Save our info calculated!
	voice->startaddressoffset = startaddressoffset;
	voice->endaddressoffset = endaddressoffset;
	voice->startloopaddressoffset = startloopaddressoffset;
	voice->endloopaddressoffset = endloopaddressoffset;

	//Determine the loop size!
	loopsize = endloopaddressoffset; //End of the loop!
	loopsize -= startloopaddressoffset; //Size of the loop!
	voice->loopsize = loopsize; //Save the loop size!

	//Now, calculate the speedup according to the note applied!
	cents = 0.0f; //Default: none!

	//Calculate MIDI difference in notes!
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, overridingRootKey, &applyigen))
	{
		rootMIDITone = applyigen.genAmount.wAmount; //The MIDI tone to apply is different!
	}
	else
	{
		rootMIDITone = voice->sample.byOriginalPitch; //Original MIDI tone!
	}

	rootMIDITone -= note->note; //>=positive difference, <=negative difference.
	//Ammount of MIDI notes too high is in rootMIDITone.

	//Coarse tune...
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, coarseTune, &applyigen))
	{
		cents = (float)applyigen.genAmount.shAmount; //How many semitones!
		cents *= 100.0f; //Apply to the cents: 1 semitone = 100 cents!
	}

	//Fine tune...
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, fineTune, &applyigen))
	{
		cents += (float)applyigen.genAmount.shAmount; //Add the ammount of cents!
	}

	//Scale tuning: how the MIDI number affects semitone (percentage of semitones)
	tonecents = 100.0f; //Default: 100 cents(%) scale tuning!
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, scaleTuning, &applyigen))
	{
		tonecents = (float)applyigen.genAmount.shAmount; //Apply semitone factor in percent for each tone!
	}

	tonecents *= -((float)rootMIDITone); //Difference in tones we use is applied to the ammount of cents reversed (the more negative, the)!

	cents += tonecents; //Apply the MIDI tone cents for the MIDI tone!

	//Now the cents variable contains the diviation in cents.
	voice->initsamplespeedup = cents2samplesfactor(cents); //Load the default speedup we need for our tone!

	//Determine panning!
	panningtemp = 0.0f; //Default: no panning at all: centered!
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, pan, &applyigen)) //Gotten panning?
	{
		panningtemp = (float)applyigen.genAmount.shAmount; //Get the panning specified!
		panningtemp *= 0.001f; //Make into a percentage, it's in 0.1% units!
	}
	voice->initpanning = panningtemp; //Set the initial panning, as a factor!

	panningtemp = -1; //Default to none!
	if (lookupSFInstrumentModGlobal(soundfont, instrumentptr.genAmount.wAmount,ibag,0x028A,&applymod)) //Gotten panning modulator?
	{
		panningtemp = (float)applymod.modAmount; //Get the amount specified!
		panningtemp *= 0.001f; //Make into a percentage, it's in 0.1% units!
	}
	voice->panningmod = panningtemp; //Apply the modulator!
	
	pitchwheeltemp = 12700.0f; //Default to 12700 cents!
	if (lookupSFInstrumentModGlobal(soundfont, instrumentptr.genAmount.wAmount,ibag,0x020E,&applymod)) //Gotten panning modulator?
	{
		pitchwheeltemp = (float)applymod.modAmount; //Get the amount specified!
	}
	voice->pitchwheelmod = pitchwheeltemp; //Apply the modulator!
	

	//Now determine the volume envelope!
	voice->CurrentVolumeEnvelope = 0.0f; //Default: nothing yet, so no volume, Give us full priority Volume-wise!
	voice->CurrentModulationEnvelope = 0.0f; //Default: nothing tet, so no modulation!

	//Apply low pass filter!
	voice->lowpassfilter_freq = 0.0f; //Default: no low pass filter!
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, initialFilterFc, &applyigen)) //Filter enabled?
	{
		voice->lowpassfilter_freq = 8.176*cents2samplesfactor(applyigen.genAmount.shAmount); //Set a low pass filter to it's initial value!
	}

	//Apply loop flags!
	voice->currentloopflags = 0; //Default: no looping!
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, sampleModes, &applyigen)) //Gotten looping?
	{
		switch (applyigen.genAmount.wAmount) //What loop?
		{
		case GEN_SAMPLEMODES_LOOP: //Always loop?
			voice->currentloopflags = 1; //Always loop!
			break;
		case GEN_SAMPLEMODES_LOOPUNTILDEPRESSDONE: //Loop until depressed!
			voice->currentloopflags = 3; //Loop until depressed!
			break;
		case GEN_SAMPLEMODES_NOLOOP: //No loop?
		case GEN_SAMPLEMODES_NOLOOP2: //No loop?
		default:
			//Do nothing!
			break;
		}
	}

	//Calculate the velocity factor!
	float noteon_velocity; //The velocity calculated!
	noteon_velocity = note->noteon_velocity;
	noteon_velocity /= 64.0f; //Adjust velocity, based on the key hit! It's always 1+, since 0 is a release of the key.
	note->noteon_velocity_factor = noteon_velocity; //The velocity calculated!

	//Final adjustments and set active!
	ADSR_init((float)voice->sample.dwSampleRate, note->noteon_velocity, &voice->VolumeEnvelope, soundfont, instrumentptr.genAmount.wAmount, ibag, preset, pbag, delayVolEnv, attackVolEnv, holdVolEnv, decayVolEnv, sustainVolEnv, releaseVolEnv, -rootMIDITone, keynumToVolEnvHold, keynumToVolEnvDecay);	//Initialise our Volume Envelope for use!
	setSampleRate(&MIDIDEVICE_renderer, voice, voice->sample.dwSampleRate); //Use this new samplerate!
	voice->starttime = starttime++; //Take a new start time!
	return 0; //Run: we're active!
}

/* Execution flow support */

OPTINLINE byte MIDIDEVICE_FilterChannelVoice(byte selectedchannel, byte channel)
{
	if (!(MIDIDEVICE.channels[channel].mode&MIDIDEVICE_OMNI)) //No Omni mode?
	{
		if (channel!=selectedchannel) //Different channel selected?
		{
			return 0; //Don't execute!
		}
	}
	if (!(MIDIDEVICE.channels[channel].mode&MIDIDEVICE_POLY)) //Mono mode?
	{
		if (!((selectedchannel>=MIDIDEVICE.channels[channel].channelrangemin) &&
			(selectedchannel<=MIDIDEVICE.channels[channel].channelrangemax))) //Out of range?
		{
			return 0; //Don't execute!
		}
	}
	//Poly mode and Omni mode: Respond to all on any channel = Ignore the channel with Poly Mode!
	return 1;
}

OPTINLINE void MIDIDEVICE_noteOff(byte selectedchannel, byte channel, byte note, byte velocity)
{
	if (MIDIDEVICE_FilterChannelVoice(selectedchannel,channel)) //To be applied?
	{
		int i;
		for (i = 0; i < __MIDI_NUMVOICES; i++) //Process all voices!
		{
			if (activevoices[i].VolumeEnvelope.active) //Active note?
			{
				if ((activevoices[i].note->channel == channel) && (activevoices[i].note->note == note)) //Note found?
				{
					activevoices[i].request_off = 1; //We're requesting to be turned off!
					activevoices[i].note->noteoff_velocity = velocity; //Note off velocity!
				}
			}
		}
	}
}

OPTINLINE void MIDIDEVICE_AllNotesOff(byte selectedchannel, byte channel) //Used with command, mode change and Mono Mode.
{
	word noteoff; //Current note to turn off!
	//Note values
	byte note32, note32_index;
	uint_32 note32_value;
	lockaudio(); //Lock the audio!
	for (noteoff=0;noteoff<0x100;) //Process all notes!
	{
		MIDIDEVICE_noteOff(selectedchannel,channel,noteoff++,64); //Execute Note Off!
	}
	unlockaudio(1); //Unlock the audio!
	#ifdef MIDI_LOG
	dolog("MPU","MIDIDEVICE: ALL NOTES OFF: %i",selectedchannel); //Log it!
	#endif
}

OPTINLINE void MIDIDEVICE_noteOn(byte selectedchannel, byte channel, byte note, byte velocity)
{
	if (MIDIDEVICE_FilterChannelVoice(selectedchannel,channel)) //To be applied?
	{
		if (!(MIDIDEVICE.channels[channel].mode&MIDIDEVICE_POLY)) //Mono mode?
		{
			MIDIDEVICE_AllNotesOff(selectedchannel,channel); //Turn all notes off first!
		}
		MIDIDEVICE.channels[channel].notes[note].noteon_velocity = velocity; //Add velocity to our lookup!

		int voice, foundvoice = -1, voicetosteal = -1;
		int_32 stolenvoiceranking = 0xEFFFFFFF, currentranking; //Stolen voice ranking starts lowest always!
		for (voice = 0; voice < __MIDI_NUMVOICES; voice++) //Find a voice!
		{
			if (MIDIDEVICE_newvoice(&activevoices[voice],channel,note)) //Failed to allocate?
			{
				if (activevoices[voice].VolumeEnvelope.active) //Are we active?
				{
					//Create ranking by scoring the voice!
					currentranking = 0; //Start with no ranking!
					if (activevoices[voice].channel == &MIDIDEVICE.channels[9]) currentranking += 4000; //Drum channel?
					else if (activevoices[voice].VolumeEnvelope.active == ADSR_RELEASE) currentranking -= 2000; //Release gets priority to be stolen!
					if (activevoices[voice].channel->sustain) currentranking -= 1000; //Lower when sustained!
					float volume;
					volume = activevoices[voice].CurrentVolumeEnvelope; //Load the ADSR volume!
					if (activevoices[voice].lvolume > activevoices[voice].rvolume) //More left volume?
					{
						volume *= activevoices[voice].lvolume; //Left volume!
					}
					else
					{
						volume *= activevoices[voice].rvolume; //Right volume!
					}
					currentranking += (int_32)(volume*1000.0f); //Factor in volume!
					if (stolenvoiceranking > currentranking) //We're a lower rank?
					{
						stolenvoiceranking = currentranking; //New voice to steal!
						voicetosteal = voice; //Steal this voice, if needed!
					}
					else if ((currentranking == stolenvoiceranking) && (voicetosteal!=-1)) //Same ranking as the last one found?
					{
						if (activevoices[voice].starttime < activevoices[voicetosteal].starttime) //Earlier start time with same ranking?
						{
							voicetosteal = voice; //Steal this voice, if needed!
						}
					}
				}
				else //Inactive channel, but failed to express when allocating?
				{
					foundvoice = voice; //Found this voice!
					break;
				}
			}
			else //Allocated?
			{
				foundvoice = voice; //Found this voice!
				break; //Stop searching!
			}
		}
		if (foundvoice == -1) //No channels available? We need voice stealing!
		{
			//Perform voice stealing using voicetosteal, if available!
			if (voicetosteal != -1) //Something to steal?
			{
				activevoices[voicetosteal].VolumeEnvelope.active = 0; //Make inactive!
				MIDIDEVICE_newvoice(&activevoices[voicetosteal], channel,note); //Steal the selected voice!
			}
		}
		//Else: allocated!
	}
}

OPTINLINE void MIDIDEVICE_execMIDI(MIDIPTR current) //Execute the current MIDI command!
{
	//First, our variables!
	byte command, currentchannel, channel, firstparam;
	byte rangemin, rangemax; //Ranges for MONO mode.

	//Process the current command!
	command = current->command; //What command!
	currentchannel = command; //What channel!
	currentchannel &= 0xF; //Make sure we're OK!
	firstparam = current->buffer[0]; //Read the first param: always needed!
	switch (command&0xF0) //What command?
	{
		case 0x80: //Note off?
		noteoff: //Note off!
			#ifdef MIDI_LOG
				if ((command & 0xF0) == 0x90) dolog("MPU", "MIDIDEVICE: NOTE ON: Redirected to NOTE OFF.");
			#endif
			lockaudio(); //Lock the audio!
			for (channel=0;channel<0x10;) //Process all channels!
			{
				MIDIDEVICE_noteOff(currentchannel,channel++,firstparam,current->buffer[1]); //Execute Note Off!
			}
			unlockaudio(1); //Unlock the audio!
			#ifdef MIDI_LOG
				dolog("MPU","MIDIDEVICE: NOTE OFF: Channel %i Note %i Velocity %i",currentchannel,firstparam,current->buffer[1]); //Log it!
			#endif
			break;
		case 0x90: //Note on?
			if (!current->buffer[1]) goto noteoff; //Actually a note off?
			lockaudio(); //Lock the audio!
			for (channel=0;channel<0x10;) //Process all channels!
			{
				MIDIDEVICE_noteOn(currentchannel, channel++, firstparam, current->buffer[1]); //Execute Note On!
			}
			unlockaudio(1); //Unlock the audio!
			#ifdef MIDI_LOG
				dolog("MPU","MIDIDEVICE: NOTE ON: Channel %i Note %i Velocity %i",currentchannel,firstparam,current->buffer[1]); //Log it!
			#endif
			break;
		case 0xA0: //Aftertouch?
			lockaudio(); //Lock the audio!
			MIDIDEVICE.channels[currentchannel].notes[firstparam].pressure = current->buffer[1];
			unlockaudio(1); //Unlock the audio!
			#ifdef MIDI_LOG
				dolog("MPU","MIDIDEVICE: Aftertouch: %i-%i",currentchannel,MIDIDEVICE.channels[currentchannel].notes[firstparam].pressure); //Log it!
			#endif
			break;
		case 0xB0: //Control change?
			switch (firstparam) //What control?
			{
				case 0x00: //Bank Select (MSB)
					#ifdef MIDI_LOG
						dolog("MPU","MIDIDEVICE: Bank select MSB on channel %i: %02X",currentchannel,current->buffer[1]); //Log it!
					#endif
					lockaudio(); //Lock the audio!
					MIDIDEVICE.channels[currentchannel].bank &= 0x7F; //Only keep LSB!
					MIDIDEVICE.channels[currentchannel].bank |= (current->buffer[1]<<7); //Set MSB!
					unlockaudio(1); //Unlock the audio!
					break;
				//case 0x01: //Modulation wheel (MSB)
					//break;
				//case 0x04: //Foot Pedal (MSB)
					//break;
				//case 0x06: //Data Entry, followed by cc100&101 for the address.
					//break;
				case 0x07: //Volume (MSB)
					#ifdef MIDI_LOG
						dolog("MPU", "MIDIDEVICE: Volume MSB on channel %i: %02X",currentchannel, current->buffer[1]); //Log it!
					#endif
					lockaudio(); //Lock the audio!
					MIDIDEVICE.channels[currentchannel].volume &= 0x7F; //Only keep LSB!
					MIDIDEVICE.channels[currentchannel].volume |= (current->buffer[1] << 7); //Set MSB!
					unlockaudio(1); //Unlock the audio!
					break;
				case 0x0A: //Pan position (MSB)
					#ifdef MIDI_LOG
						dolog("MPU", "MIDIDEVICE: Pan position MSB on channel %i: %02X",currentchannel, current->buffer[1]); //Log it!
					#endif
					lockaudio(); //Lock the audio!
					MIDIDEVICE.channels[currentchannel].panposition &= 0x7F; //Only keep LSB!
					MIDIDEVICE.channels[currentchannel].panposition |= (current->buffer[1] << 7); //Set MSB!
					unlockaudio(1); //Unlock the audio!
					break;
				//case 0x0B: //Expression (MSB)
					//break;
				case 0x20: //Bank Select (LSB) (see cc0)
					#ifdef MIDI_LOG
						dolog("MPU", "MIDIDEVICE: Bank select LSB on channel %i: %02X",currentchannel, current->buffer[1]); //Log it!
					#endif
					lockaudio(); //Lock the audio!
					MIDIDEVICE.channels[currentchannel].bank &= 0x3F80; //Only keep MSB!
					MIDIDEVICE.channels[currentchannel].bank |= current->buffer[1]; //Set LSB!
					unlockaudio(1); //Unlock the audio!
					break;
				//case 0x21: //Modulation wheel (LSB)
					//break;
				//case 0x24: //Foot Pedal (LSB)
					//break;
				//case 0x26: //Data Entry, followed by cc100&101 for the address.
					//break;
				case 0x27: //Volume (LSB)
					#ifdef MIDI_LOG
						dolog("MPU", "MIDIDEVICE: Volume LSB on channel %i: %02X",currentchannel, current->buffer[1]); //Log it!
					#endif
					lockaudio(); //Lock the audio!
					MIDIDEVICE.channels[currentchannel].volume &= 0x3F80; //Only keep MSB!
					MIDIDEVICE.channels[currentchannel].volume |= current->buffer[1]; //Set LSB!
					unlockaudio(1); //Unlock the audio!
					break;
				case 0x2A: //Pan position (LSB)
					#ifdef MIDI_LOG
						dolog("MPU", "MIDIDEVICE: Pan position LSB on channel %i: %02X",currentchannel, current->buffer[1]); //Log it!
					#endif
					lockaudio(); //Lock the audio!
					MIDIDEVICE.channels[currentchannel].panposition &= 0x3F80; //Only keep MSB!
					MIDIDEVICE.channels[currentchannel].panposition |= current->buffer[1]; //Set LSB!
					unlockaudio(1); //Unlock the audio!
					break;
				case 0x40: //Hold Pedal (On/Off) = Sustain Pedal
					#ifdef MIDI_LOG
						dolog("MPU", "MIDIDEVICE:  Channel %i; Hold pedal: %02X=%i", currentchannel, current->buffer[1],(current->buffer[1]&MIDI_CONTROLLER_ON)?1:0); //Log it!
					#endif
					lockaudio(); //Lock the audio!
					MIDIDEVICE.channels[currentchannel].sustain = (current->buffer[1]&MIDI_CONTROLLER_ON)?1:0; //Sustain?
					unlockaudio(1); //Unlock the audio!
					break;
				//case 0x41: //Portamento (On/Off)
					//break;
				//case 0x47: //Resonance a.k.a. Timbre
					//break;
				//case 0x4A: //Frequency Cutoff (a.k.a. Brightness)
					//break;
				//case 0x5B: //Reverb Level
					//break;
				//case 0x5D: //Chorus Level
					//break;
					//Sound function On/Off:
				//case 0x78: //All Sound Off
					//break;
				//case 0x79: //All Controllers Off
					//break;
				//case 0x7A: //Local Keyboard On/Off
					//break;
				case 0x7B: //All Notes Off
				case 0x7C: //Omni Mode Off
				case 0x7D: //Omni Mode On
				case 0x7E: //Mono operation
				case 0x7F: //Poly Operation
					lockaudio(); //Lock the audio!
					for (channel=0;channel<0x10;)
					{
						MIDIDEVICE_AllNotesOff(currentchannel,channel++); //Turn all notes off!
					}
					if ((firstparam&0x7C)==0x7C) //Mode change command?
					{
						switch (firstparam&3) //What mode change?
						{
						case 0: //Omni Mode Off
							#ifdef MIDI_LOG
								dolog("MPU", "MIDIDEVICE: Channel %i, OMNI OFF", currentchannel); //Log it!
							#endif
							MIDIDEVICE.channels[currentchannel].mode &= ~MIDIDEVICE_OMNI; //Disable Omni mode!
							break;
						case 1: //Omni Mode On
							#ifdef MIDI_LOG
								dolog("MPU", "MIDIDEVICE: Channel %i, OMNI ON", currentchannel); //Log it!
							#endif
							MIDIDEVICE.channels[currentchannel].mode |= MIDIDEVICE_OMNI; //Enable Omni mode!
							break;
						case 2: //Mono operation
							MIDIDEVICE.channels[currentchannel].mode &= ~MIDIDEVICE_POLY; //Disable Poly mode!
							MIDIDEVICE.channels[currentchannel].mode &= ~MIDIDEVICE_OMNI; //Disable Omni mode!
							if (current->buffer[1]) //Omni Off+Ammount of channels to respond to?
							{
								#ifdef MIDI_LOG
									dolog("MPU", "MIDIDEVICE: Channel %i, MONO without OMNI, Channels to respond: %i", currentchannel,current->buffer[1]); //Log it!
								#endif
								rangemax = rangemin = currentchannel;
								rangemax += current->buffer[1]; //Maximum range!
								--rangemax;
							}
							else //Omni On?
							{
								#ifdef MIDI_LOG
									dolog("MPU", "MIDIDEVICE: Channel %i, MONO with OMNI, Respond to all channels.", currentchannel); //Log it!
								#endif
								MIDIDEVICE.channels[currentchannel].mode |= MIDIDEVICE_OMNI; //Enable Omni mode!
								rangemin = 0; //Respond to...
								rangemax = 0xF; //All channels!
							}
							MIDIDEVICE.channels[currentchannel].channelrangemin = rangemin;
							MIDIDEVICE.channels[currentchannel].channelrangemax = rangemax;
							break;
						case 3: //Poly Operation
							#ifdef MIDI_LOG
								dolog("MPU", "MIDIDEVICE: Channel %i, POLY", currentchannel); //Log it!
							#endif
							MIDIDEVICE.channels[currentchannel].mode |= MIDIDEVICE_POLY; //Enable Poly mode!
							break;
						}
					}
					unlockaudio(1); //Unlock the audio!
					break;
				default: //Unknown controller?
					#ifdef MIDI_LOG
						dolog("MPU", "MIDIDEVICE: Unknown Continuous Controller change: %i=%i", currentchannel, firstparam); //Log it!
					#endif
					break;
			}
			break;
		case 0xC0: //Program change?
			lockaudio(); //Lock the audio!
			MIDIDEVICE.channels[currentchannel].program = firstparam; //What program?
			MIDIDEVICE.channels[currentchannel].activebank = MIDIDEVICE.channels[currentchannel].bank; //Apply bank from Bank Select Messages!
			unlockaudio(1); //Unlock the audio!
			#ifdef MIDI_LOG
				dolog("MPU","MIDIDEVICE: Program change: %i=%i",currentchannel,MIDIDEVICE.channels[currentchannel].program); //Log it!
			#endif
			break;
		case 0xD0: //Channel pressure?
			lockaudio(); //Lock the audio!
			MIDIDEVICE.channels[currentchannel].pressure = firstparam;
			unlockaudio(1); //Unlock the audio!
			#ifdef MIDI_LOG
				dolog("MPU","MIDIDEVICE: Channel pressure: %i=%i",currentchannel,MIDIDEVICE.channels[currentchannel].pressure); //Log it!
			#endif
			break;
		case 0xE0: //Pitch wheel?
			lockaudio(); //Lock the audio!
			MIDIDEVICE.channels[currentchannel].pitch = ((sword)((current->buffer[1]<<7)|firstparam))-0x2000; //Actual pitch, converted to signed value!
			unlockaudio(1); //Unlock the audio!
			#ifdef MIDI_LOG
				dolog("MPU","MIDIDEVICE: Pitch wheel: %i=%i",currentchannel,MIDIDEVICE.channels[currentchannel].pitch); //Log it!
			#endif
			break;
		case 0xF0: //System message?
			//We don't handle system messages!
			if (command==0xFF) //Reset?
			{
				reset_MIDIDEVICE(); //Reset ourselves!
			}
			#ifdef MIDI_LOG
				dolog("MPU","MIDIDEVICE: System messages are unsupported!"); //Log it!
			#endif
			break;
		default: //Invalid command?
			#ifdef MIDI_LOG
				dolog("MPU","MIDIDEVICE: Unknown command: %02X",command);
			#endif
			break; //Do nothing!
	}
}

/* Init/destroy support */

void done_MIDIDEVICE() //Finish our midi device!
{
	#ifdef __HW_DISABLED
		return; //We're disabled!
	#endif
	#ifdef _WIN32
	#ifdef DIRECT_MIDI
		// turn any MIDI notes currently playing:
		midiOutReset(device);

		// Remove any data in MIDI device and close the MIDI Output port
		midiOutClose(device);
		//We're directly sending MIDI to the output!
		return; //Stop: ready!
	#endif
	#endif
	
	lockaudio();
	//Close the soundfont?
	closeSF(&soundfont);
	int i;
	for (i=0;i<__MIDI_NUMVOICES;i++) //Assign all voices available!
	{
		removechannel(&MIDIDEVICE_renderer,&activevoices[i],0); //Remove the channel! Delay at 0.96ms for response speed!
	}
	unlockaudio(1);
}

void init_MIDIDEVICE() //Initialise MIDI device for usage!
{
	#ifdef __HW_DISABLED
		return; //We're disabled!
	#endif
	#ifdef _WIN32
	#ifdef DIRECT_MIDI
		// Open the MIDI output port
		flag = midiOutOpen(&device, 0, 0, 0, CALLBACK_NULL);
		if (flag != MMSYSERR_NOERROR) {
			printf("Error opening MIDI Output.\n");
			return;
		}
		//We're directly sending MIDI to the output!
		return; //Stop: ready!
	#endif
	#endif
	lockaudio();
	done_MIDIDEVICE(); //Start finished!
	reset_MIDIDEVICE(); //Reset our MIDI device!
	//Load the soundfont?
	soundfont = readSF("MPU.sf2"); //Read the soundfont, if available!
	if (!soundfont) //Unable to load?
	{
		dolog("MPU","No soundfont found or could be loaded!");
	}
	else
	{
		int i;
		for (i=0;i<__MIDI_NUMVOICES;i++) //Assign all voices available!
		{
			addchannel(&MIDIDEVICE_renderer,&activevoices[i],"MIDI Voice",44100.0f,__MIDI_SAMPLES,1,SMPL16S); //Add the channel! Delay at 0.96ms for response speed! 44100/(1000000/960)=42.336 samples/response!
		}
	}
	unlockaudio(1);
}