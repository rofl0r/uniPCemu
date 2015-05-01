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

//All MIDI voices that are available!
#define __MIDI_NUMVOICES 64
//How many samples to buffer at once! 42 according to MIDI specs! Set to 84 to work!
#define __MIDI_SAMPLES 1024

//To log MIDI commands?
//#define MIDI_LOG
//To log MIDI rendering timing?
//#define LOG_MIDI_TIMING

//On/off controller bit values!
#define MIDI_CONTROLLER_ON 0x40

//Poly and Omni flags in the Mode Selection.
//Poly: Enable multiple voices per channel. When set to Mono, All Notes Off on the channel when a Note On is received.
#define MIDIDEVICE_POLY 0x1
//Omni: Ignore channel number of the message during note On/Off commands (send to channel 0).
#define MIDIDEVICE_OMNI 0x2

//Default mode is Omni Off, Poly
#define MIDIDEVICE_DEFAULTMODE MIDIDEVICE_POLY

typedef struct
{
	//First, infomation for looking us up!
	byte channel; //What channel!
	byte note; //What note!
	byte noteon_velocity; //What velocity/AD(SR)!
	byte noteoff_velocity; //What velocity/(ADS)R!
	word pressure; //Pressure/volume/aftertouch!
} MIDIDEVICE_NOTE; //Current playing note to process information!

typedef struct
{
	MIDIDEVICE_NOTE notes[0xFF]; //All possible MIDI note statuses!
	//Channel information!
	byte control; //Control/current instrument!
	byte program; //Program/instrument!
	byte pressure; //Channel pressure/volume!
	word bank; //What bank are we?
	sword pitch; //Current pitch (14-bit value)
	uint_32 request_on[4]; //All notes requested on (bitfield)
	uint_32 playing[4]; //All notes that are being played!
	uint_32 request_off[4]; //All notes requested off (bitfield)
	byte sustain; //Enable sustain? Don't process KEY OFF while set!
	byte channelrangemin, channelrangemax; //Ranges of used channels to respond to when in Mono Mode.
	byte mode; //Channel mode: 0=Omni off, Mono; 1=Omni off, Poly; 2=Omni on, Mono; 3=Omni on, Poly;
	//Bit 1=1:Poly/0:Mono; Bit2=1:Omni on/0:Omni off
	/* Omni: respond to all channels (ignore channel part); Poly: Use multiple voices; Mono: Use one voice at the time (end other voices on Note On) */
} MIDIDEVICE_CHANNEL;

struct
{
	byte UARTMode;
	MIDIDEVICE_CHANNEL channels[0x10]; //Stuff for all channels!
} MIDIDEVICE; //Current MIDI device data!

typedef struct
{
	uint_32 status_counter; //Counter used within this status!
	uint_32 play_counter; //Current play position within the soundfont!

	//Our assigned notes/channels for lookup!
	MIDIDEVICE_CHANNEL *channel; //The active channel!
	MIDIDEVICE_NOTE *note; //The active note!
	sfSample sample; //The sample to be played back!
	float initsamplespeedup; //Precalculated speedup of the samples, to be processed into effective speedup when starting the rendering!
	float effectivesamplespeedup; //The speedup of the samples!
	uint_32 loopsize; //The size of a loop!
	//Patches to the sample offsets, calculated before generating sound!
	uint_32 startaddressoffset;
	uint_32 startloopaddressoffset;
	uint_32 endaddressoffset;
	uint_32 endloopaddressoffset;

	float lvolume, rvolume; //Left and right panning!

	byte currentloopflags; //What loopflags are active?
	byte requestnumber; //Number of the request block!
	uint_32 requestbit; //The bit used in the request block!

	uint_64 availablevoicebit; //Our available voice's bit!

	//High pass filter
	float lowpassfilter_freq; //What frequency to filter? 0.0f=No filter!
	byte has_last; //Gotten last?
	float last_sample; //Last retrieved sample!
	float last_result; //Last result of the high pass filter!

	ADSR VolumeEnvelope; //The volume envelope!
	ADSR ModulationEnvelope; //The modulation envelope!
	float CurrentVolumeEnvelope; //Current volume envelope!
	float CurrentModulationEnvelope; //Current modulation envelope!

	//Stuff for voice stealing
	uint_64 starttime; //When have we started our voice?
} MIDIDEVICE_VOICE;

MIDIDEVICE_VOICE activevoices[__MIDI_NUMVOICES]; //All active voices!
uint_64 availablevoices = 0xFFFFFFFFFFFFFFFF; //Available voices!

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
			flag = midiOutShortMsg(device, message.word);
			if (flag != MMSYSERR_NOERROR) {
				printf("Warning: MIDI Output is not open.\n");
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
		MIDIDEVICE.channels[channel++].mode = MIDIDEVICE_DEFAULTMODE; //Use the default mode!
	}
	unlockaudio(1);
}

/*

Cents and DB conversion!

*/

//Convert cents to samples to increase (instead of 1 sample/sample). Floating point number (between 0.0+ usually?) Use this as a counter for the current samples (1.1+1.1..., so keep the rest value (1,1,1,...,0,1,1,1,...))
//The same applies to absolute and relative timecents (with absolute referring to 1 second intervals (framerate samples) and relative to the absolute value)
OPTINLINE double cents2samplesfactor(double cents)
{
	return pow(2, (cents / 1200)); //Convert to samples (not entire numbers, so keep them counted)!
}

//Low&high pass filters!

OPTINLINE float calcLowpassFilter(float cutoff_freq, float samplerate, float currentsample, float previoussample, float previousresult)
{
	float RC = 1.0 / (cutoff_freq * 2 * 3.14);
	float dt = 1.0 / samplerate;
	float alpha = dt / (RC + dt);
	return previousresult + (alpha*(currentsample - previousresult));
}

void applyLowpassFilter(MIDIDEVICE_VOICE *voice, float *currentsample)
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

OPTINLINE void MIDIDEVICE_getsample(sample_stereo_t *sample, MIDIDEVICE_VOICE *voice) //Get a sample from an MIDI note!
{
	//Our current rendering routine:
	register uint_32 temp;
	register uint_32 samplepos;
	float lchannel, rchannel; //Both channels to use!
	sword readsample; //The sample retrieved!
	byte loopflags;

	samplepos = voice->play_counter; //Load the current play counter!
	if (voice->VolumeEnvelope.active) ++voice->play_counter; //Disable increasing the counter when inactive: keep the same position!
	samplepos *= voice->effectivesamplespeedup; //Affect speed through cents and other factors!
	samplepos += voice->startaddressoffset; //The start of the sample!

	//First: apply looping! We don't apply [bit 1=0] (Loop infinite until finished), because we have no ADSR envelope yet!
	loopflags = voice->currentloopflags;
	if (voice->VolumeEnvelope.active) //Active voice?
	{
		if (loopflags & 1) //Currently looping and active?
		{
			if (samplepos >= voice->endloopaddressoffset) //Past/at the end of the loop!
			{
				temp = voice->startloopaddressoffset; //The actual start of the loop!
				//Calculate loop size!
				samplepos -= temp; //Take the ammount past the start of the loop!
				samplepos %= voice->loopsize; //Loop past startloop by endloop!
				samplepos += temp; //The destination position within the loop!
				if ((loopflags & 0xC0) == 0x80) //We're depressed and depress action is allowed (not holding)?
				{
					if (loopflags & 2) //Loop until depress?
					{
						voice->currentloopflags = 0; //Disable loop flags: we're not looping anymore!
						//Loop for the last time!
						uint_32 temppos;
						temppos = samplepos;
						temppos -= voice->startaddressoffset; //Go back to the multiplied offset!
						temppos /= voice->effectivesamplespeedup; //Calculate our play counter to use!
						voice->play_counter = temppos; //Possibly our new position to start at!
					}
				}
			}
		}
	}

	//Next, apply finish!
	loopflags = (samplepos >= voice->endaddressoffset); //Expired?
	loopflags |= !voice->VolumeEnvelope.active; //Inactive?
	if (loopflags) //Sound is finished?
	{
		sample->l = sample->r = 0; //No sample!
		return; //Done!
	}

	if (getSFsample(soundfont, samplepos, &readsample)) //Sample found?
	{
		lchannel = (float)readsample; //Convert to floating point for our calculations!

		//First, apply filters and current envelope!
		applyLowpassFilter(voice, &lchannel); //Low pass filter!
		lchannel *= voice->CurrentVolumeEnvelope; //Apply ADSR Volume envelope!
		//Now the sample is ready for output into the actual final volume!

		rchannel = lchannel; //Load into both channels!
		//Now, apply panning!
		lchannel *= voice->lvolume; //Apply left panning!
		rchannel *= voice->rvolume; //Apply right panning!

		//Give the result!
		sample->l = lchannel; //LChannel!
		sample->r = rchannel; //RChannel!
	}
	else
	{
		sample->l = sample->r = 0; //No sample to be found!
	}
}

byte MIDIDEVICE_renderer(void* buf, uint_32 length, byte stereo, void *userdata) //Sound output renderer!
{
#ifdef __HW_DISABLED
	return 0; //We're disabled!
#endif

	//Initialisation info
	float pitchcents, currentsamplespeedup;
	byte currenton;
	uint_32 requestbit;
	MIDIDEVICE_CHANNEL *channel;
	//Initialised values!
	MIDIDEVICE_VOICE *voice = (MIDIDEVICE_VOICE *)userdata;
	sample_stereo_t* ubuf = (sample_stereo_t *)buf; //Our sample buffer!
	uint_32 numsamples = length; //How many samples to buffer!

#ifdef LOG_MIDI_TIMING
	static TicksHolder ticks; //Our ticks holder!
	startHiresCounting(&ticks);
	ticksholder_AVG(&ticks); //Enable averaging!
#endif

	if (!voice->VolumeEnvelope.active) //Inactive voice?
	{
		return SOUNDHANDLER_RESULT_NOTFILLED; //Empty buffer: we're unused!
	}

	//Calculate the pitch bend speedup!
	pitchcents = (double)voice->channel->pitch; //Load active pitch bend!
	pitchcents /= 40.96f; //Pitch bend in cents!

	//Now apply to the default speedup!
	currentsamplespeedup = voice->initsamplespeedup; //Load the default sample speedup for our tone!
	currentsamplespeedup *= cents2samplesfactor(pitchcents); //Calculate the sample speedup!; //Apply pitch bend!
	voice->effectivesamplespeedup = currentsamplespeedup; //Load the speedup of the samples we need!

	if (voice->channel->request_off[voice->requestnumber] & voice->requestbit) //Requested turn off?
	{
		voice->currentloopflags |= 0x80; //Request quit looping if needed: finish sound!
		voice->currentloopflags &= ~0x40; //Sustain disabled by default!
		voice->currentloopflags |= (voice->channel->sustain << 6); //Sustaining?
	} //Requested off?

	//Now produce the sound itself!
	for (; numsamples--;) //Produce the samples!
	{
		voice->CurrentVolumeEnvelope = ADSR_tick(&voice->VolumeEnvelope, (voice->channel->sustain) || ((voice->currentloopflags & 0xC0) != 0x80)); //Apply Volume Envelope!
		MIDIDEVICE_getsample(ubuf++, voice); //Get the sample from the MIDI device!
	}

#ifdef LOG_MIDI_TIMING
	stopHiresCounting("MIDIDEV", "MIDIRenderer", &ticks); //Log our active counting!
#endif

	if (!voice->VolumeEnvelope.active) //Inactive voice?
	{
		//Get our data concerning the release!
		availablevoices |= voice->availablevoicebit; //We're available again!
		currenton = voice->requestnumber;
		requestbit = voice->requestbit;
		channel = voice->channel; //Current channel!

		channel->request_off[currenton] &= ~requestbit; //Turn the KEY OFF request off, if any!
		channel->playing[currenton] &= ~requestbit; //Turn the PLAYING flag off: we're not playing anymore!
	}

	return SOUNDHANDLER_RESULT_FILLED; //We're filled!
}

byte MIDIDEVICE_newvoice(MIDIDEVICE_VOICE *voice)
{
	static uint_64 starttime = 0; //Calculated start time!
	byte currentchannel, currenton, biton;
	word pbag, ibag;
	sword rootMIDITone; //Relative root MIDI tone!
	uint_32 requestbit, preset, therequeston, notenumber, startaddressoffset, endaddressoffset, startloopaddressoffset, endloopaddressoffset, loopsize;
	float cents, tonecents, lvolume, rvolume, panningtemp;

	MIDIDEVICE_CHANNEL *channel;
	MIDIDEVICE_NOTE *note;
	sfPresetHeader currentpreset;
	sfGenList instrumentptr, applypgen;
	sfInst currentinstrument;
	sfInstGenList sampleptr, applyigen;

	//Check for requested voices!
	//First, all our variables!
	for (currentchannel = 0; currentchannel<0x40;) //Process all channels (10 channels, 4 dwords/channel)!
	{
		biton = currenton = currentchannel; //Current on!
		currenton >>= 4; //Take the current dword!
		biton &= 0xF; //Lower 4 bits is the channel!
		if (MIDIDEVICE.channels[biton].request_on[currenton]) //Any request on?
		{
			therequeston = MIDIDEVICE.channels[biton].request_on[currenton];
			goto handlerequest; //Handle request!
		}
		++currentchannel; //Next channel!
	}
	return 1; //Abort: we're an inactive voice!

handlerequest: //Handles an NOTE ON request!
	currentchannel = biton; //The specified channel!
	//currentchannel=the channel; currenton=the request dword
	for (biton = 0; ((biton<32) && (!(therequeston & 1)));) //Not found yet?
	{
		if (therequeston & 1) break; //Stop searching when found!
		therequeston >>= 1; //Next bit check!
		++biton; //Next bit!
	}
	//biton is the requested bit number

	voice->requestnumber = currenton; //The request number!

	requestbit = 1;
	requestbit <<= biton; //The request bit!

	voice->channel = channel = &MIDIDEVICE.channels[currentchannel]; //What channel!

	channel->request_on[currenton] &= ~requestbit; //Turn the request off!
	voice->requestbit = requestbit; //Save the request bit!

	voice->play_counter = 0; //Reset play counter!

	notenumber = currenton; //Current on!
	notenumber <<= 5; //32 notes for each currenton;
	notenumber |= biton; //The actual note that's turned on!
	//Now, notenumber contains the note turned on!

	//Now, determine the actual note to be turned on!
	voice->note = note = &voice->channel->notes[notenumber]; //What note!

	//First, our precalcs!

	//Now retrieve our note by specification!

	if (!lookupPresetByInstrument(soundfont, channel->program, channel->bank, &preset)) //Preset not found?
	{
		return 1; //No samples!
	}

	if (!getSFPreset(soundfont, preset, &currentpreset))
	{
		return 1;
	}

	if (!lookupPBagByMIDIKey(soundfont, preset, note->note, note->noteon_velocity, &pbag)) //Preset bag not found?
	{
		return 1; //No samples!
	}

	if (!lookupSFPresetGen(soundfont, preset, pbag, instrument, &instrumentptr))
	{
		return 1; //No samples!
	}

	if (!getSFInstrument(soundfont, instrumentptr.genAmount.wAmount, &currentinstrument))
	{
		return 1;
	}

	if (!lookupIBagByMIDIKey(soundfont, instrumentptr.genAmount.wAmount, note->note, note->noteon_velocity, &ibag, 1))
	{
		return 1; //No samples!
	}

	if (!lookupSFInstrumentGen(soundfont, instrumentptr.genAmount.wAmount, ibag, sampleID, &sampleptr))
	{
		return 1; //No samples!
	}

	if (!getSFSampleInformation(soundfont, sampleptr.genAmount.wAmount, &voice->sample))
	{
		return 1; //No samples!
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
	lvolume = rvolume = 0.5f; //Default to 50% each (center)!
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, pan, &applyigen)) //Gotten panning?
	{
		panningtemp = (float)applyigen.genAmount.shAmount; //Get the panning specified!
		panningtemp *= 0.01f; //Make into a percentage!
		lvolume -= panningtemp; //Left percentage!
		rvolume += panningtemp; //Right percentage!
	}
	voice->lvolume = lvolume; //Left panning!
	voice->rvolume = rvolume; //Right panning!

	//Now determine the volume envelope!
	voice->CurrentVolumeEnvelope = 1.0f; //Default: nothing yet, so full volume, Give us full priority Volume-wise!
	voice->CurrentModulationEnvelope = 0.0f; //Default: nothing tet, so no modulation!
	
	ADSR_init((float)voice->sample.dwSampleRate, &voice->VolumeEnvelope, soundfont, instrumentptr.genAmount.wAmount, ibag, preset, pbag, delayVolEnv, attackVolEnv, holdVolEnv, decayVolEnv, sustainVolEnv, releaseVolEnv, -rootMIDITone, keynumToVolEnvHold, keynumToVolEnvDecay);	//Initialise our Volume Envelope for use!

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

	//Final adjustments and set active!
	setSampleRate(&MIDIDEVICE_renderer, voice, voice->sample.dwSampleRate); //Use this new samplerate!
	channel->playing[currenton] |= requestbit; //Playing flag!
	voice->starttime = starttime++; //Take a new start time!
	availablevoices &= ~voice->availablevoicebit; //Set us busy!
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
	else if (!(MIDIDEVICE.channels[channel].mode&MIDIDEVICE_POLY)) //Mono&Omni mode?
	{
		if ((selectedchannel<MIDIDEVICE.channels[channel].channelrangemin) ||
			(selectedchannel>MIDIDEVICE.channels[channel].channelrangemax)) //Out of range?
		{
			return 0; //Don't execute!
		}
	}
	//Poly mode and Omni mode: Respond to all on any channel = Ignore the channel with Poly Mode!
	return 1;
}

OPTINLINE void MIDIDEVICE_noteOff(byte selectedchannel, byte channel, byte note, byte velocity, byte note32, byte note32_index, uint_32 note32_value)
{
	if (MIDIDEVICE_FilterChannelVoice(selectedchannel,channel)) //To be applied?
	{
		if ((MIDIDEVICE.channels[channel].playing[note32]&note32_value) || (MIDIDEVICE.channels[channel].request_on[note32]&note32_value)) //Are we playing or requested?
		{
			if ((MIDIDEVICE.channels[channel].request_on[note32] & note32_value) && (~MIDIDEVICE.channels[channel].playing[note32] & note32_value)) //Requested without playing?
			{
				//We're requested, but not playing yet! Remove from queue!
				MIDIDEVICE.channels[channel].request_on[note32] &= ~note32_value; //Don't play this anymore! Discard the note (lost note)!
			}
			else //Normal: we can be shut down!
			{
				MIDIDEVICE.channels[channel].request_off[note32] |= note32_value; //Request finish!
				MIDIDEVICE.channels[channel].notes[note].noteoff_velocity = velocity; //What velocity!
			}
		}
	}
}

OPTINLINE void MIDIDEVICE_calc_notePosition(byte note, byte *note32, byte *note32_index, uint_32 *note32_value)
{
	//Our variables come first!
	//First, calculate our results!
	*note32 = note;
	*note32 >>= 5; //Divide by 32 for the note every dword!
	*note32_index = note;
	*note32_index &= 0x1F; //The index within the search!
	*note32_value = 1; //Load the index!
	*note32_value <<= *note32_index; //Shift to our position!
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
		MIDIDEVICE_calc_notePosition(noteoff,&note32,&note32_index,&note32_value); //Calculate our needs!
		MIDIDEVICE_noteOff(selectedchannel,channel,noteoff++,64,note32,note32_index,note32_value); //Execute Note Off!
	}
	unlockaudio(1); //Unlock the audio!
	#ifdef MIDI_LOG
	dolog("MPU","MIDIDEVICE: ALL NOTES OFF: %i",selectedchannel); //Log it!
	#endif
}

OPTINLINE void MIDIDEVICE_noteOn(byte selectedchannel, byte channel, byte note, byte velocity, byte note32, byte note32_index, uint_32 note32_value)
{
	if (MIDIDEVICE_FilterChannelVoice(selectedchannel,channel)) //To be applied?
	{
		if (!(MIDIDEVICE.channels[channel].playing[note32]&note32_value)) //Not already playing?
		{
			if (!(MIDIDEVICE.channels[channel].mode&MIDIDEVICE_POLY)) //Mono mode?
			{
				MIDIDEVICE_AllNotesOff(selectedchannel,channel); //Turn all notes off first!
			}
			MIDIDEVICE.channels[channel].request_on[note32] |= note32_value; //Request start!
			MIDIDEVICE.channels[channel].notes[note].noteon_velocity = velocity; //Add velocity to our lookup!

			int voice, foundvoice = -1, voicetosteal = -1;
			int_32 stolenvoiceranking = 0xEFFFFFFF, currentranking; //Stolen voice ranking starts lowest always!
			for (voice = 0; voice < __MIDI_NUMVOICES; voice++) //Find a voice!
			{
				if (MIDIDEVICE_newvoice(&activevoices[voice])) //Failed to allocate?
				{
					if (activevoices[voice].VolumeEnvelope.active) //Are we active?
					{
						//Create ranking by scoring the voice!
						currentranking = 0; //Start with no ranking!
						if (activevoices[voice].channel == &MIDIDEVICE.channels[9]) currentranking += 4000; //Drum channel?
						else if (activevoices[voice].VolumeEnvelope.active == MIDISTATUS_RELEASE) currentranking -= 2000; //Release gets priority to be stolen!
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
					MIDIDEVICE_newvoice(&activevoices[voicetosteal]); //Steal the selected voice!
				}
				else
				{
					//If nothing can be stolen, don't play the note!
					MIDIDEVICE.channels[channel].request_on[note32] &= ~note32_value; //Remove from the requests!
				}
			}
			//Else: allocated!
		}
	}
}

OPTINLINE void MIDIDEVICE_execMIDI(MIDIPTR current) //Execute the current MIDI command!
{
	//First, our variables!
	byte note32, note32_index;
	uint_32 note32_value;
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
			MIDIDEVICE_calc_notePosition(firstparam,&note32,&note32_index,&note32_value); //Calculate our needs!
		noteoff: //Not off!
			#ifdef MIDI_LOG
			if ((command & 0xF0) == 0x90) dolog("MPU", "MIDIDEVICE: NOTE ON: Redirected to NOTE OFF.");
			#endif
			lockaudio(); //Lock the audio!
			for (channel=0;channel<0x10;) //Process all channels!
			{
				MIDIDEVICE_noteOff(currentchannel,channel++,firstparam,current->buffer[1],note32,note32_index,note32_value); //Execute Note Off!
			}
			unlockaudio(1); //Unlock the audio!
			#ifdef MIDI_LOG
			dolog("MPU","MIDIDEVICE: NOTE OFF: Channel %i Note %i Velocity %i = Channel %i: Block %i Note %i",currentchannel,firstparam,current->buffer[1],channel,note32,note32_index); //Log it!
			#endif
			break;
		case 0x90: //Note on?
			MIDIDEVICE_calc_notePosition(firstparam,&note32,&note32_index,&note32_value); //Calculate our needs!
			if (!current->buffer[1]) goto noteoff; //Actually a note off?
			lockaudio(); //Lock the audio!
			for (channel=0;channel<0x10;) //Process all channels!
			{
				MIDIDEVICE_noteOn(currentchannel, channel++, firstparam, current->buffer[1], note32, note32_index, note32_value); //Execute Note Off!
			}
			unlockaudio(1); //Unlock the audio!
			#ifdef MIDI_LOG
			dolog("MPU","MIDIDEVICE: NOTE ON: Channel %i Note %i Velocity %i = Channel %i Block %i Note %i",currentchannel,firstparam,current->buffer[1],currentchannel,note32,note32_index); //Log it!
			#endif
			break;
		case 0xA0: //Aftertouch?
			lockaudio(); //Lock the audio!
			MIDIDEVICE.channels[currentchannel].notes[firstparam].pressure = (firstparam<<7)|current->buffer[1];
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
					dolog("MPU","MIDIDEVICE: Bank select MSB: %02X",current->buffer[1]); //Log it!
					#endif
					lockaudio(); //Lock the audio!
					MIDIDEVICE.channels[currentchannel].bank &= 0x7F; //Only keep LSB!
					MIDIDEVICE.channels[currentchannel].bank |= (current->buffer[1]<<7); //Set MSB!
					unlockaudio(1); //Unlock the audio!
					break;
				case 0x01: //Modulation wheel
					break;
				case 0x04: //Foot Pedal (MSB)
					break;
				case 0x06: //Data Entry, followed by cc100&101 for the address.
					break;
				case 0x07: //Volume (MSB)
					break;
				case 0x0A: //Pan position (MSB)
					break;
				case 0x0B: //Expression (MSB)
					break;
				case 0x20: //Bank Select (LSB) (see cc0)
					#ifdef MIDI_LOG
					dolog("MPU", "MIDIDEVICE: Bank select LSB: %02X", current->buffer[1]); //Log it!
					#endif
					lockaudio(); //Lock the audio!
					MIDIDEVICE.channels[currentchannel].bank &= 0x3F80; //Only keep MSB!
					MIDIDEVICE.channels[currentchannel].bank |= current->buffer[1]; //Set LSB!
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
				case 0x41: //Portamento (On/Off)
					break;
				case 0x47: //Resonance a.k.a. Timbre
					break;
				case 0x4A: //Frequency Cutoff (a.k.a. Brightness)
					break;
				case 0x5B: //Reverb Level
					break;
				case 0x5D: //Chorus Level
					break;
					//Sound function On/Off:
				case 0x78: //All Sound Off
					break;
				case 0x79: //All Controllers Off
					break;
				case 0x7A: //Local Keyboard On/Off
					break;
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
					break;
			}
			#ifdef MIDI_LOG
			dolog("MPU","MIDIDEVICE: Control change: %i=%i",currentchannel,firstparam); //Log it!
			#endif
			break;
		case 0xC0: //Program change?
			lockaudio(); //Lock the audio!
			MIDIDEVICE.channels[currentchannel].program = firstparam; //What program?
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
	for (i=0;i<NUMITEMS(activevoices);i++) //Assign all voices available!
	{
		activevoices[i].availablevoicebit = (1LL << (uint_64)i); //Our bit in available voices!
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
		for (i=0;i<NUMITEMS(activevoices);i++) //Assign all voices available!
		{
			addchannel(&MIDIDEVICE_renderer,&activevoices[i],"MIDI Voice",44100.0f,__MIDI_SAMPLES,1,SMPL16S); //Add the channel! Delay at 0.96ms for response speed! 44100/(1000000/960)=42.336 samples/response!
		}
	}
	unlockaudio(1);
}