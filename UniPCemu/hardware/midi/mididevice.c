/*

Copyright (C) 2019  Superfury

This file is part of UniPCemu.

UniPCemu is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

UniPCemu is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with UniPCemu.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "headers/types.h" //Basic types!
#include "headers/support/sf2.h" //Soundfont support!
#include "headers/hardware/midi/mididevice.h" //Our own typedefs!
#include "headers/support/zalloc.h" //Zero allocation support!
#include "headers/emu/sound.h" //Sound support!
#include "headers/support/log.h" //Logging support!
#include "headers/support/highrestimer.h" //High resolution timer support!
#include "headers/hardware/midi/adsr.h" //ADSR support!
#include "headers/emu/timers.h" //Use timers for Active Sensing!
#include "headers/support/locks.h" //Locking support!
#include "headers/support/signedness.h"

//Use direct windows MIDI processor if available?

//Our volume to use!
#define MIDI_VOLUME 100.0f

//Effective volume vs samples!
#define VOLUME 1.0f

#ifdef IS_WINDOWS
#include <mmsystem.h>  /* multimedia functions (such as MIDI) for Windows */
#endif

//Are we disabled?
//#define __HW_DISABLED
RIFFHEADER *soundfont; //Our loaded soundfont!

//To log MIDI commands?
//#define MIDI_LOG

byte direct_midi = 0; //Enable direct MIDI synthesis?

//On/off controller bit values!
#define MIDI_CONTROLLER_ON 0x40

//Poly and Omni flags in the Mode Selection.
//Poly: Enable multiple voices per channel. When set to Mono, All Notes Off on the channel when a Note On is received.
#define MIDIDEVICE_POLY 0x1
//Omni: Ignore channel number of the message during note On/Off commands.
#define MIDIDEVICE_OMNI 0x2

//Default mode is Omni Off, Poly
#define MIDIDEVICE_DEFAULTMODE MIDIDEVICE_POLY

//Reverb delay in seconds
#define REVERB_DELAY 0.25f

//Chorus delay in seconds (5ms)
#define CHORUS_DELAY 0.005f

//Chorus LFO Frequency (5Hz)
#define CHORUS_LFO_FREQUENCY 5.0f

//Chorus LFO Strength (cents) sharp
#define CHORUS_LFO_CENTS 10.0f

//16/32 bit quantities from the SoundFont loaded in memory!
#ifndef IS_PSP
#define LE16(x) SDL_SwapLE16(x)
#define LE32(x) SDL_SwapLE32(x)
#else
#define LE16(x) (x)
#define LE32(x) (x)
#endif
#define LE16S(x) = unsigned2signed16(LE16(signed2unsigned16(x)))
#define LE32S(x) = unsigned2signed32(LE32(signed2unsigned32(x)))

float reverb_delay[0x100];
float chorus_delay[0x100];
float choruscents[2];

MIDIDEVICE_CHANNEL MIDI_channels[0x10]; //Stuff for all channels!

MIDIDEVICE_VOICE activevoices[__MIDI_NUMVOICES]; //All active voices!

/* MIDI direct output support*/

#ifdef IS_WINDOWS
int flag;           // monitor the status of returning functions
HMIDIOUT device;    // MIDI device interface for sending MIDI output
#endif

OPTINLINE void lockMPURenderer()
{
	//lockaudio(); //Lock the audio!
}

OPTINLINE void unlockMPURenderer()
{
	//unlockaudio(); //Unlock the audio!
}

/* Reset support */

OPTINLINE void reset_MIDIDEVICE() //Reset the MIDI device for usage!
{
	//First, our variables!
	byte channel,chorusreverbdepth;
	word notes;
	FIFOBUFFER *temp, *chorus_backtrace[CHORUSSIZE];

	lockMPURenderer();
	memset(&MIDI_channels,0,sizeof(MIDI_channels)); //Clear our data!

	for (channel=0;channel<NUMITEMS(activevoices);channel++) //Process all voices!
	{
		temp = activevoices[channel].effect_backtrace_samplespeedup; //Back-up the effect backtrace!
		for (chorusreverbdepth=0;chorusreverbdepth<CHORUSSIZE;++chorusreverbdepth)
		{
			chorus_backtrace[chorusreverbdepth] = activevoices[channel].effect_backtrace_chorus[chorusreverbdepth]; //Back-up!
		}
		memset(&activevoices[channel],0,sizeof(activevoices[channel])); //Clear the entire channel!
		for (chorusreverbdepth=0;chorusreverbdepth<CHORUSSIZE;++chorusreverbdepth)
		{
			activevoices[channel].effect_backtrace_chorus[chorusreverbdepth] = chorus_backtrace[chorusreverbdepth]; //Restore!
		}
		activevoices[channel].effect_backtrace_samplespeedup = temp; //Restore our buffer!
		fifobuffer_clear(temp); //Clear our buffer!
	}

	for (channel=0;channel<0x10;)
	{
		for (notes=0;notes<0x100;)
		{
			MIDI_channels[channel].notes[notes].channel = channel;
			MIDI_channels[channel].notes[notes].note = (byte)notes;

			//Also apply delays while we're at it(also 256 values)!
			reverb_delay[notes] = REVERB_DELAY*(float)notes; //The reverb delay to use for this stream!
			chorus_delay[notes] = CHORUS_DELAY*(float)notes; //The chorus delay to use for this stream!

			++notes; //Next note!
		}
		MIDI_channels[channel].bank = MIDI_channels[channel].activebank = 0; //Reset!
		MIDI_channels[channel].channelrangemin = MIDI_channels[channel].channelrangemax = channel; //We respond to this channel only!
		MIDI_channels[channel].control = 0; //First instrument!
		MIDI_channels[channel].pitch = 0x2000; //Centered pitch = Default pitch!
		MIDI_channels[channel].pressure = 0x40; //Centered pressure!
		MIDI_channels[channel].program = 0; //First program!
		MIDI_channels[channel].sustain = 0; //Disable sustain!
		MIDI_channels[channel].volumeMSB = 0x64; //Default volume as the default volume(100)!
		MIDI_channels[channel].volumeLSB = 0x7F; //Default volume as the default volume(127?)!
		MIDI_channels[channel].expression = 0x7F; //Default volume as the default max expression(127)!
		MIDI_channels[channel].panposition = 0x2000; //Centered pan position as the default pan!
		MIDI_channels[channel].lvolume = MIDI_channels[channel].rvolume = 0.5; //Accompanying the pan position: centered volume!
		MIDI_channels[channel++].mode = MIDIDEVICE_DEFAULTMODE; //Use the default mode!
	}
	MIDI_channels[MIDI_DRUMCHANNEL].bank = MIDI_channels[MIDI_DRUMCHANNEL].activebank = 0x80; //We're locked to a drum set!
	unlockMPURenderer();
}

/*

Cents and DB conversion!

*/

//Low pass filters!

OPTINLINE float modulateLowpass(MIDIDEVICE_VOICE *voice, float Modulation, byte filterindex)
{
	INLINEREGISTER float modulationratio;

	modulationratio = Modulation*voice->lowpassfilter_modenvfactor; //The modulation ratio to use!

	//Now, translate the modulation ratio to samples, optimized!
	modulationratio = floorf(modulationratio); //Round it down to get integer values to optimize!
	if (modulationratio!=voice->lowpass_modulationratio[filterindex]) //Different ratio?
	{
		voice->lowpass_modulationratio[filterindex] = modulationratio; //Update the last ratio!
		modulationratio = voice->lowpass_modulationratiosamples[filterindex] = cents2samplesfactorf(modulationratio)*voice->lowpassfilter_freq; //Calculate the pitch bend and modulation ratio to apply!
		voice->lowpass_dirty[filterindex] = 1; //We're a dirty low-pass filter!
	}
	else
	{
		modulationratio = voice->lowpass_modulationratiosamples[filterindex]; //We're the same as last time!
	}
	return modulationratio; //Give the frequency to use for the low pass filter!
}

OPTINLINE void applyMIDILowpassFilter(MIDIDEVICE_VOICE *voice, float *currentsample, float Modulation, byte filterindex)
{
	float lowpassfilterfreq;
	if (voice->lowpassfilter_freq==0) return; //No filter?
	lowpassfilterfreq = modulateLowpass(voice,Modulation,filterindex); //Load the frequency to use for low-pass filtering!
	if (voice->lowpass_dirty[filterindex]) //Are we dirty? We need to update the low-pass filter, if so!
	{		
		updateSoundFilter(&voice->lowpassfilter[filterindex],0,lowpassfilterfreq,(float)LE32(voice->sample.dwSampleRate)); //Update the low-pass filter, when needed!
		voice->lowpass_dirty[filterindex] = 0; //We're not dirty anymore!
	}
	applySoundFilter(&voice->lowpassfilter[filterindex], currentsample); //Apply a low pass filter!
}

OPTINLINE void applyMIDIReverbFilter(MIDIDEVICE_VOICE *voice, float *currentsample, byte filterindex)
{
	applySoundFilter(&voice->reverbfilter[filterindex], currentsample); //Apply a low pass filter!
}

/*

Voice support

*/

//How many steps to keep!
#define SINUSTABLE_PERCISION 3600
#define SINUSTABLE_PERCISION_FLT 3600.0f
#define SINUSTABLE_PERCISION_REVERSE (1.0f/SINUSTABLE_PERCISION_FLT)

int_32 chorussinustable[SINUSTABLE_PERCISION][2][2]; //10x percision steps of sinus! With 1.0 added always!
float sinustable_percision_reverse = 1.0f; //Reverse lookup!

void MIDIDEVICE_generateSinusTable()
{
	word x;
	byte choruschannel;
	for (x=0;x<NUMITEMS(chorussinustable);++x)
	{
		for (choruschannel=0;choruschannel<2;++choruschannel) //All channels!
		{
			chorussinustable[x][choruschannel][0] = (int_32)((sinf((float)((x/SINUSTABLE_PERCISION_FLT))*360.0f)+1.0f)*choruscents[choruschannel]); //Generate sinus lookup table, negative!
			chorussinustable[x][choruschannel][1] = (int_32)(chorussinustable[x][choruschannel][0]+1200.0f); //Generate sinus lookup table, with cents base added, negative!
		}
	}
	sinustable_percision_reverse = SINUSTABLE_PERCISION_REVERSE; //Our percise value, reverse lookup!
}

//Absolute to get the amount of degrees, converted to a -1.0 to 1.0 scale!
#define MIDIDEVICE_chorussinf(value, choruschannel, add1200centsbase) chorussinustable[(uint_32)(value*SINUSTABLE_PERCISION_FLT)][choruschannel][add1200centsbase]

OPTINLINE void MIDIDEVICE_getsample(int_64 play_counter, uint_32 totaldelay, float samplerate, int_32 samplespeedup, MIDIDEVICE_VOICE *voice, float Volume, float Modulation, byte chorus, float chorusvol, byte filterindex, int_32 *lchannelres, int_32 *rchannelres) //Get a sample from an MIDI note!
{
	//Our current rendering routine:
	INLINEREGISTER uint_32 temp;
	INLINEREGISTER int_64 samplepos;
	float lchannel, rchannel; //Both channels to use!
	byte loopflags; //Flags used during looping!
	static sword readsample = 0; //The sample retrieved!
	int_32 modulationratiocents;
	uint_32 speedupbuffer;

	if (filterindex==0) //Main channel? Log the current sample speedup!
	{
		writefifobuffer32(voice->effect_backtrace_samplespeedup,signed2unsigned32(samplespeedup)); //Log a history of this!
	}

	if ((play_counter>=0) && filterindex) //Are we a running channel that needs reading back?
	{
		if (readfifobuffer32_backtrace(voice->effect_backtrace_samplespeedup,&speedupbuffer,totaldelay,voice->isfinalchannel_chorus[filterindex])) //Try to read from history! Only apply the value when not the originating channel!
		{
			samplespeedup = unsigned2signed32(speedupbuffer); //Apply the sample speedup from that point in time! Not for the originating channel!
		}
	}

	modulationratiocents = 0; //Default: none!
	if (chorus) //Chorus extension channel?
	{
		modulationratiocents = MIDIDEVICE_chorussinf(voice->chorussinpos[filterindex],chorus,0); //Pitch bend default!
		voice->chorussinpos[filterindex] += voice->chorussinposstep; //Step by one sample rendered!
		if (voice->chorussinpos[filterindex]>=SINUSTABLE_PERCISION_FLT) voice->chorussinpos[filterindex] -= SINUSTABLE_PERCISION_FLT; //Wrap around when needed(once per second)!
	}

	modulationratiocents += voice->modenv_pitchfactor; //Apply pitch bend as well!
	//Apply pitch bend to the current factor too!
	modulationratiocents += samplespeedup; //Speedup according to pitch bend!

	//Apply the new modulation ratio, if needed!
	if (modulationratiocents!=voice->modulationratiocents[filterindex]) //Different ratio?
	{
		voice->modulationratiocents[filterindex] = modulationratiocents; //Update the last ratio!
		voice->modulationratiosamples[filterindex] = cents2samplesfactord((DOUBLE)modulationratiocents); //Calculate the pitch bend and modulation ratio to apply!
	}

	samplepos = (int_64)((DOUBLE)play_counter*voice->modulationratiosamples[filterindex]); //Apply the pitch bend and other modulation data to the sample to retrieve!

	//Now, calculate the start offset to start looping!
	samplepos += voice->startaddressoffset; //The start of the sample!

	//First: apply looping!
	loopflags = voice->currentloopflags;
	if (voice->has_finallooppos && (play_counter >= voice->finallooppos)) //Executing final loop?
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
				voice->finallooppos_playcounter = samplepos; //The start position within the loop to use at this point in time!
			}
		}
	}

	//Next, apply finish!
	loopflags = (samplepos >= voice->endaddressoffset) || (play_counter<0); //Expired or not started yet?
	if (loopflags) goto finishedsample;

	if (getSFSample16(soundfont, (uint_32)samplepos, &readsample)) //Sample found?
	{
		lchannel = (float)readsample; //Convert to floating point for our calculations!

		//First, apply filters and current envelope!
		applyMIDILowpassFilter(voice, &lchannel, Modulation, filterindex); //Low pass filter!
		lchannel *= Volume; //Apply ADSR Volume envelope!
		lchannel *= voice->initialAttenuation; //The volume of the samples!
		lchannel *= chorusvol; //Apply chorus&reverb volume for this stream!
		lchannel *= VOLUME; //Apply general volume!
		//Now the sample is ready for output into the actual final volume!

		rchannel = lchannel; //Load into both channels!
		//Now, apply panning!
		lchannel *= voice->lvolume; //Apply left panning, also according to the CC!
		rchannel *= voice->rvolume; //Apply right panning, also according to the CC!

		writefifobufferflt_2(voice->effect_backtrace_chorus[filterindex],lchannel,rchannel); //Left/right channel output!

		*lchannelres += (int_32)lchannel; //Apply the immediate left channel!
		*rchannelres += (int_32)rchannel; //Apply the immedaite right channel!
	}
	else
	{
		finishedsample: //loopflags set?
		writefifobufferflt_2(voice->effect_backtrace_chorus[filterindex],0.0f,0.0f); //Left/right channel output!
	}
}

byte MIDIDEVICE_renderer(void* buf, uint_32 length, byte stereo, void *userdata) //Sound output renderer!
{
#ifdef __HW_DISABLED
	return 0; //We're disabled!
#endif
	if (!stereo) return 0; //Can't handle non-stereo output!
	//Initialisation info
	float pitchcents, currentsamplespeedup, lvolume, rvolume, panningtemp;
	float VolumeEnvelope=0; //Current volume envelope data!
	float ModulationEnvelope=0; //Current modulation envelope data!
	//Initialised values!
	MIDIDEVICE_VOICE *voice = (MIDIDEVICE_VOICE *)userdata;
	sample_stereo_t* ubuf = (sample_stereo_t *)buf; //Our sample buffer!
	ADSR *VolumeADSR = &voice->VolumeEnvelope; //Our used volume envelope ADSR!
	ADSR *ModulationADSR = &voice->ModulationEnvelope; //Our used modulation envelope ADSR!
	MIDIDEVICE_CHANNEL *channel = voice->channel; //Get the channel to use!
	uint_32 numsamples = length; //How many samples to buffer!
	byte currentchorusreverb; //Current chorus and reverb levels we're processing!
	int_64 chorusreverbsamplepos;

	#ifdef MIDI_LOCKSTART
	//lock(voice->locknumber); //Lock us!
	#endif

	if (voice->VolumeEnvelope.active==0) //Simple check!
	{
		#ifdef MIDI_LOCKSTART
		//unlock(voice->locknumber); //Lock us!
		#endif
		return SOUNDHANDLER_RESULT_NOTFILLED; //Empty buffer: we're unused!
	}
	if (memprotect(soundfont,sizeof(*soundfont),"RIFF_FILE")!=soundfont)
	{
		#ifdef MIDI_LOCKSTART
		//unlock(voice->locknumber); //Lock us!
		#endif
		return SOUNDHANDLER_RESULT_NOTFILLED; //Empty buffer: we're unable to render anything!
	}
	if (!soundfont)
	{
		#ifdef MIDI_LOCKSTART
		//unlock(voice->locknumber); //Lock us!
		#endif
		return SOUNDHANDLER_RESULT_NOTFILLED; //The same!
	}
	if (!channel) //Unknown channel?
	{
		#ifdef MIDI_LOCKSTART
		//unlock(voice->locknumber); //Lock us!
		#endif
		return SOUNDHANDLER_RESULT_NOTFILLED; //The same!
	}


	#ifdef MIDI_LOCKSTART
	lock(voice->locknumber); //Actually check!
	#endif

	if (voice->VolumeEnvelope.active == 0) //Not active after all?
	{
		#ifdef MIDI_LOCKSTART
		unlock(voice->locknumber);
		#endif
		return SOUNDHANDLER_RESULT_NOTFILLED; //Empty buffer: we're unused!
	}
	//Calculate the pitch bend speedup!
	pitchcents = (float)(channel->pitch&0x3FFF); //Load active pitch bend (unsigned), Only low 14 bits are used!
	pitchcents -= (float)0x2000; //Convert to a signed value!
	pitchcents /= 128.0f; //Create a value between -1 and 1!
	pitchcents *= voice->pitchwheelmod; //Influence by pitch wheel!

	//Now apply to the default speedup!
	currentsamplespeedup = voice->initsamplespeedup; //Load the default sample speedup for our tone!
	currentsamplespeedup += pitchcents; //Apply pitch bend!
	voice->effectivesamplespeedup = (int_32)currentsamplespeedup; //Load the speedup of the samples we need!

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

	VolumeEnvelope = voice->CurrentVolumeEnvelope; //Make sure we don't clear!
	ModulationEnvelope = voice->CurrentModulationEnvelope; //Make sure we don't clear!

	int_32 lchannel, rchannel; //Left&right samples, big enough for all chorus and reverb to be applied!
	float channelsamplel, channelsampler; //A channel sample!

	float samplerate = (float)LE32(voice->sample.dwSampleRate); //The samplerate we use!

	byte chorus,reverb;
	uint_32 totaldelay;
	float tempstorage;
	byte activechannel, currentactivefinalchannel; //Are we an active channel?

	//Now produce the sound itself!
	do //Produce the samples!
	{
		lchannel = 0; //Reset left channel!
		rchannel = 0; //Reset right channel!
		currentchorusreverb=0; //Init to first chorus channel!
		do //Process all chorus used(2 chorus channels)!
		{
			chorusreverbsamplepos = voice->play_counter; //Load the current play counter!
			totaldelay = voice->chorusdelay[currentchorusreverb]; //Load the total delay!
			chorusreverbsamplepos -= (int_64)totaldelay; //Apply specified chorus&reverb delay!
			VolumeEnvelope = ADSR_tick(VolumeADSR,chorusreverbsamplepos,((voice->currentloopflags & 0xC0) != 0x80),voice->note->noteon_velocity, voice->note->noteoff_velocity); //Apply Volume Envelope!
			ModulationEnvelope = ADSR_tick(ModulationADSR,chorusreverbsamplepos,((voice->currentloopflags & 0xC0) != 0x80),voice->note->noteon_velocity, voice->note->noteoff_velocity); //Apply Modulation Envelope!
			MIDIDEVICE_getsample(chorusreverbsamplepos, totaldelay, samplerate, voice->effectivesamplespeedup, voice, VolumeEnvelope, ModulationEnvelope, currentchorusreverb, voice->chorusvol[currentchorusreverb], currentchorusreverb, &lchannel, &rchannel); //Get the sample from the MIDI device, with only the chorus effect!
		} while (++currentchorusreverb<CHORUSSIZE); //Chorus loop.

		//Apply reverb based on chorus history now!
		chorus = 0; //Init chorus number!
		reverb = 1; //First reverberation to apply!
		tempstorage = VolumeEnvelope; //Store for temporary storage!
		activechannel = (chorusreverbsamplepos>=0); //Are we an active channel?
		do //Process all reverb used(2 reverb channels)!
		{
			totaldelay = voice->reverbdelay[reverb]; //Load the total delay!
			currentactivefinalchannel = (voice->isfinalchannel_reverb[reverb]) && activechannel; //Active&final channel?

			if (readfifobufferflt_backtrace_2(voice->effect_backtrace_chorus[chorus],&channelsamplel,&channelsampler,totaldelay,currentactivefinalchannel)) //Are we successfully read back?
			{
				VolumeEnvelope = voice->reverbvol[reverb]; //Load the envelope to apply!
				applyMIDIReverbFilter(voice, &channelsamplel, (currentchorusreverb<<1)); //Low pass filter!
				applyMIDIReverbFilter(voice, &channelsampler, ((currentchorusreverb<<1)|1)); //Low pass filter!
				lchannel += (int_32)(channelsamplel*VolumeEnvelope); //Sound the left channel at reverb level!
				rchannel += (int_32)(channelsampler*VolumeEnvelope); //Sound the right channel at reverb level!
			}
			++chorus; //Next chorus channel to apply!
			chorus &= 1; //Only 2 choruses to apply, so loop around them!
			reverb += (chorus^1); //Next reverb channel when needed!
		} while (++currentchorusreverb<CHORUSREVERBSIZE); //Remaining channel loop.
		VolumeEnvelope = tempstorage; //Restore the volume envelope!

		//Clip the samples to prevent overflow!
		if (lchannel>SHRT_MAX) lchannel = SHRT_MAX;
		if (lchannel<SHRT_MIN) lchannel = SHRT_MIN;
		if (rchannel>SHRT_MAX) rchannel = SHRT_MAX;
		if (rchannel<SHRT_MIN) rchannel = SHRT_MIN;
		ubuf->l = lchannel; //Left sample!
		ubuf->r = rchannel; //Right sample!
		++voice->play_counter; //Next sample!
		++ubuf; //Prepare for the next sample!
	} while (--numsamples); //Repeat while samples are left!

	voice->CurrentVolumeEnvelope = VolumeEnvelope; //Current volume envelope updated!
	voice->CurrentModulationEnvelope = ModulationEnvelope; //Current volume envelope updated!

	#ifdef MIDI_LOCKSTART
	unlock(voice->locknumber); //Lock us!
	#endif
	return SOUNDHANDLER_RESULT_FILLED; //We're filled!
}

//MIDIvolume: converts a value of the range of maxvalue to a linear volume factor using maxdB dB.
float MIDIattenuate(float value)
{
	return (float)powf(10.0f,value/-200.0f); //Generate default attenuation!
}

//calcNegativeUnipolarSource: Calculates the result of a unipolar source, normalized between 0.x and less than 1.0!
float calcNegativeUnipolarSource(byte attenuationsetting, byte maxvalmask)
{
	return (((float)(maxvalmask - (attenuationsetting&maxvalmask)))/(float)(maxvalmask)); //0=Max(127), 127=0(becoming 1) and everything else is in between, linearly!
}

OPTINLINE byte MIDIDEVICE_newvoice(MIDIDEVICE_VOICE *voice, byte request_channel, byte request_note)
{
	const float MIDI_CHORUS_SINUS_BASE = 2.0f*(float)PI*CHORUS_LFO_FREQUENCY; //MIDI Sinus Base for chorus effects!
	word pbag, ibag, chorusreverbdepth, chorusreverbchannel;
	sword rootMIDITone;
	int_32 cents, tonecents; //Relative root MIDI tone, different cents calculations!
	uint_32 preset, startaddressoffset, endaddressoffset, startloopaddressoffset, endloopaddressoffset, loopsize;
	float panningtemp, pitchwheeltemp,attenuation,tempattenuation,attenuationcontrol;
	int_32 addattenuation;

	MIDIDEVICE_CHANNEL *channel;
	MIDIDEVICE_NOTE *note;
	sfPresetHeader currentpreset;
	sfGenList instrumentptr, applygen;
	sfInst currentinstrument;
	sfInstGenList sampleptr, applyigen;
	sfModList applymod;
	FIFOBUFFER *temp, *chorus_backtrace[CHORUSSIZE];
	static uint_64 starttime = 0; //Increasing start time counter (1 each note on)!

	if (memprotect(soundfont,sizeof(*soundfont),"RIFF_FILE")!=soundfont) return 0; //We're unable to render anything!
	if (!soundfont) return 0; //We're unable to render anything!
	lockMPURenderer(); //Lock the audio: we're starting to modify!
	#ifdef MIDI_LOCKSTART
	lock(voice->locknumber); //Lock us!
	#endif
	if (voice->VolumeEnvelope.active)
	{
		#ifdef MIDI_LOCKSTART
		unlock(voice->locknumber); //Lock us!
		#endif
		unlockMPURenderer(); //We're finished!
		return 1; //Active voices can't be allocated!
	}

	//Check for requested voices!
	//First, all our variables!
	temp = voice->effect_backtrace_samplespeedup; //Back-up the effect backtrace!
	for (chorusreverbdepth=0;chorusreverbdepth<CHORUSSIZE;++chorusreverbdepth)
	{
		chorus_backtrace[chorusreverbdepth] = voice->effect_backtrace_chorus[chorusreverbdepth]; //Back-up!
	}
	memset(voice,0,sizeof(*voice)); //Clear the entire channel!
	voice->effect_backtrace_samplespeedup = temp; //Restore our buffer!
	for (chorusreverbdepth=0;chorusreverbdepth<CHORUSSIZE;++chorusreverbdepth)
	{
		voice->effect_backtrace_chorus[chorusreverbdepth] = chorus_backtrace[chorusreverbdepth]; //Restore!
	}
	fifobuffer_clear(voice->effect_backtrace_samplespeedup); //Clear our history buffer!
	for (chorusreverbdepth=0;chorusreverbdepth<CHORUSSIZE;++chorusreverbdepth) //Initialize all chorus histories!
	{
		fifobuffer_clear(voice->effect_backtrace_chorus[chorusreverbdepth]); //Clear our history buffer!
	}
	
	//Now, determine the actual note to be turned on!
	voice->channel = channel = &MIDI_channels[request_channel]; //What channel!
	voice->note = note = &voice->channel->notes[request_note]; //What note!

	voice->play_counter = 0; //Reset play counter!

	//First, our precalcs!

	//Now retrieve our note by specification!

	if (!lookupPresetByInstrument(soundfont, channel->program, channel->activebank, &preset)) //Preset not found?
	{
		#ifdef MIDI_LOCKSTART
		unlock(voice->locknumber); //Lock us!
		#endif
		unlockMPURenderer(); //We're finished!
		return 0; //No samples!
	}

	if (!getSFPreset(soundfont, preset, &currentpreset))
	{
		#ifdef MIDI_LOCKSTART
		unlock(voice->locknumber); //Lock us!
		#endif
		unlockMPURenderer(); //We're finished!
		return 0;
	}

	if (!lookupPBagByMIDIKey(soundfont, preset, note->note, note->noteon_velocity, &pbag)) //Preset bag not found?
	{
		#ifdef MIDI_LOCKSTART
		unlock(voice->locknumber); //Lock us!
		#endif
		unlockMPURenderer(); //We're finished!
		return 0; //No samples!
	}

	if (!lookupSFPresetGen(soundfont, preset, pbag, instrument, &instrumentptr))
	{
		#ifdef MIDI_LOCKSTART
		unlock(voice->locknumber); //Lock us!
		#endif
		unlockMPURenderer(); //We're finished!
		return 0; //No samples!
	}

	if (!getSFInstrument(soundfont, LE16(instrumentptr.genAmount.wAmount), &currentinstrument))
	{
		#ifdef MIDI_LOCKSTART
		unlock(voice->locknumber); //Lock us!
		#endif
		unlockMPURenderer(); //We're finished!
		return 0;
	}

	if (!lookupIBagByMIDIKey(soundfont, LE16(instrumentptr.genAmount.wAmount), note->note, note->noteon_velocity, &ibag, 1))
	{
		#ifdef MIDI_LOCKSTART
		unlock(voice->locknumber); //Lock us!
		#endif
		unlockMPURenderer(); //We're finished!
		return 0; //No samples!
	}

	if (!lookupSFInstrumentGen(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, sampleID, &sampleptr))
	{
		#ifdef MIDI_LOCKSTART
		unlock(voice->locknumber); //Lock us!
		#endif
		unlockMPURenderer(); //We're finished!
		return 0; //No samples!
	}

	if (!getSFSampleInformation(soundfont, LE16(sampleptr.genAmount.wAmount), &voice->sample))
	{
		#ifdef MIDI_LOCKSTART
		unlock(voice->locknumber); //Lock us!
		#endif
		unlockMPURenderer(); //We're finished!
		return 0; //No samples!
	}

	//Determine the adjusting offsets!

	//Fist, init to defaults!
	startaddressoffset = LE32(voice->sample.dwStart);
	endaddressoffset = LE32(voice->sample.dwEnd);
	startloopaddressoffset = LE32(voice->sample.dwStartloop);
	endloopaddressoffset = LE32(voice->sample.dwEndloop);

	//Next, apply generators!
	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, startAddrsOffset, &applyigen))
	{
		startaddressoffset += LE16(applyigen.genAmount.shAmount); //Apply!
	}
	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, startAddrsCoarseOffset, &applyigen))
	{
		startaddressoffset += (LE16(applyigen.genAmount.shAmount) << 15); //Apply!
	}

	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, endAddrsOffset, &applyigen))
	{
		endaddressoffset += LE16(applyigen.genAmount.shAmount); //Apply!
	}
	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, endAddrsCoarseOffset, &applyigen))
	{
		endaddressoffset += (LE16(applyigen.genAmount.shAmount) << 15); //Apply!
	}

	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, startloopAddrsOffset, &applyigen))
	{
		startloopaddressoffset += LE16(applyigen.genAmount.shAmount); //Apply!
	}
	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, startloopAddrsCoarseOffset, &applyigen))
	{
		startloopaddressoffset += (LE16(applyigen.genAmount.shAmount) << 15); //Apply!
	}

	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, endloopAddrsOffset, &applyigen))
	{
		endloopaddressoffset += LE16(applyigen.genAmount.shAmount); //Apply!
	}
	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, endloopAddrsCoarseOffset, &applyigen))
	{
		endloopaddressoffset += (LE16(applyigen.genAmount.shAmount) << 15); //Apply!
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

	//Calculate MIDI difference in notes!
	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, overridingRootKey, &applyigen))
	{
		rootMIDITone = (sword)LE16(applyigen.genAmount.wAmount); //The MIDI tone to apply is different!
		if ((rootMIDITone<0) || (rootMIDITone>127)) //Invalid?
		{
			rootMIDITone = (sword)voice->sample.byOriginalPitch; //Original MIDI tone!
		}
	}
	else
	{
		rootMIDITone = (sword)voice->sample.byOriginalPitch; //Original MIDI tone!
	}

	rootMIDITone = (((sword)note->note)-rootMIDITone); //>positive difference, <negative difference.
	//Ammount of MIDI notes too high is in rootMIDITone.

	cents = 0; //Default: none!
	cents += voice->sample.chPitchCorrection; //Apply pitch correction for the used sample!

	//Coarse tune...
	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, coarseTune, &applyigen))
	{
		cents = (int_32)LE16(applyigen.genAmount.shAmount)*100; //How many semitones! Apply to the cents: 1 semitone = 100 cents!
		if (lookupSFPresetGenGlobal(soundfont, preset, pbag, coarseTune, &applygen))
		{
			cents += (int_32)LE16(applygen.genAmount.shAmount) * 100; //How many semitones! Apply to the cents: 1 semitone = 100 cents!
		}
	}
	else if (lookupSFPresetGenGlobal(soundfont, preset, pbag, coarseTune, &applygen))
	{
		cents = (int_32)LE16(applygen.genAmount.shAmount)*100; //How many semitones! Apply to the cents: 1 semitone = 100 cents!
	}

	//Fine tune...
	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, fineTune, &applyigen))
	{
		cents += (int_32)LE16(applyigen.genAmount.shAmount); //Add the ammount of cents!
		if (lookupSFPresetGenGlobal(soundfont, preset, pbag, fineTune, &applygen))
		{
			cents += (int_32)LE16(applygen.genAmount.shAmount); //Add the ammount of cents!
		}
	}
	else if (lookupSFPresetGenGlobal(soundfont, preset, pbag, fineTune, &applygen))
	{
		cents += (int_32)LE16(applygen.genAmount.shAmount); //Add the ammount of cents!
	}

	//Scale tuning: how the MIDI number affects semitone (percentage of semitones)
	tonecents = 100; //Default: 100 cents(%) scale tuning!
	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, scaleTuning, &applyigen))
	{
		tonecents = (int_32)LE16(applyigen.genAmount.shAmount); //Apply semitone factor in percent for each tone!
		if (lookupSFPresetGenGlobal(soundfont, preset, pbag, scaleTuning, &applygen))
		{
			tonecents += (int_32)LE16(applygen.genAmount.shAmount); //Apply semitone factor in percent for each tone!
		}
	}
	else if (lookupSFPresetGenGlobal(soundfont, preset, pbag, scaleTuning, &applygen))
	{
		tonecents = (int_32)LE16(applygen.genAmount.shAmount); //Apply semitone factor in percent for each tone!
	}

	tonecents *= rootMIDITone; //Difference in tones we use is applied to the ammount of cents!

	cents += tonecents; //Apply the MIDI tone cents for the MIDI tone!

	//Now the cents variable contains the diviation in cents.
	voice->initsamplespeedup = cents; //Load the default speedup we need for our tone!
	
	//Determine the attenuation generator to use!
	attenuation = 0; //Default attenuation!
	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, initialAttenuation, &applyigen))
	{
		attenuation = (float)LE16(applyigen.genAmount.shAmount); //Apply semitone factor in percent for each tone!
		if (attenuation>1440.0f) attenuation = 1440.0f; //Limit to max!
		if (attenuation<0.0f) attenuation = 0.0f; //Limit to min!
	}
	if (lookupSFPresetGenGlobal(soundfont, preset, pbag, initialAttenuation, &applygen))
	{
		tempattenuation = (float)LE16(applygen.genAmount.shAmount); //Apply semitone factor in percent for each tone!
		if (tempattenuation > 1440.0f) tempattenuation = 1440.0f; //Limit to max!
		if (tempattenuation < 0.0f) tempattenuation = 0.0f; //Limit to min!
		attenuation += tempattenuation; //Additive!
	}

	//Apply all settable volume settings!
	//Note on velocity
	attenuationcontrol = calcNegativeUnipolarSource(note->noteon_velocity,0x7F); //The source of the attenuation!
	addattenuation = 960.0f; //How much to use as a factor (default)!
	if (lookupSFInstrumentModGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, noteOnVelocityToInitialAttenuation, &applymod)) //Gotten Note On velocity to Initial Attenuation?
	{
		applymod.modAmount = LE16(applymod.modAmount); //Patch!
		if (applymod.modAmount > 960) applymod.modAmount = 960; //Limit to max value if needed!
		else if (applymod.modAmount < 0) applymod.modAmount = 0; //Limit to min value if needed!
		addattenuation = (float)applymod.modAmount; //What to use!
		if (lookupSFPresetModGlobal(soundfont, preset, pbag, noteOnVelocityToInitialAttenuation, &applymod)) //Gotten Note On velocity to Initial Attenuation?
		{
			applymod.modAmount = LE16(applymod.modAmount); //Patch!
			if (applymod.modAmount > 960) applymod.modAmount = 960; //Limit to max value if needed!
			else if (applymod.modAmount < 0) applymod.modAmount = 0; //Limit to min value if needed!
			addattenuation += (float)applymod.modAmount; //Range is 960cB, so convert and apply(add to the initial attenuation generator)!
		}
	}
	else if (lookupSFPresetModGlobal(soundfont, preset, pbag, noteOnVelocityToInitialAttenuation, &applymod)) //Gotten Note On velocity to Initial Attenuation?
	{
		applymod.modAmount = LE16(applymod.modAmount); //Patch!
		if (applymod.modAmount > 960) applymod.modAmount = 960; //Limit to max value if needed!
		else if (applymod.modAmount < 0) applymod.modAmount = 0; //Limit to min value if needed!
		addattenuation = (float)applymod.modAmount; //Range is 960cB, so convert and apply(add to the initial attenuation generator)!
	}
	tempattenuation = addattenuation * attenuationcontrol; //How much do we want to attenuate?
	if (tempattenuation > 960.0f) tempattenuation = 960.0f; //Limit!
	if (tempattenuation < 0.0f) tempattenuation = 0.0f; //Limit!
	attenuation += tempattenuation; //96dB range volume using a 960cB attenuation!

	//CC7
	addattenuation = 960.0f; //How much to use as a factor (default)!
	attenuationcontrol = calcNegativeUnipolarSource((channel->volumeMSB & 0x7F),0x7F); //The source of the attenuation!
	if (lookupSFInstrumentModGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, continuousController7ToInitialAttenuation, &applymod)) //Gotten MIDI Continuous Controller 7 to Initial Attenuation?
	{
		applymod.modAmount = LE16(applymod.modAmount); //Patch!
		if (applymod.modAmount > 960) applymod.modAmount = 960; //Limit to max value if needed!
		else if (applymod.modAmount < 0) applymod.modAmount = 0; //Limit to min value if needed!
		addattenuation = (float)(applymod.modAmount); //Range is 960cB, so convert and apply(add to the initial attenuation generator)!
		if (lookupSFPresetModGlobal(soundfont, preset, pbag, continuousController7ToInitialAttenuation, &applymod)) //Gotten MIDI Continuous Controller 7 to Initial Attenuation?
		{
			applymod.modAmount = LE16(applymod.modAmount); //Patch!
			if (applymod.modAmount > 960) applymod.modAmount = 960; //Limit to max value if needed!
			else if (applymod.modAmount < 0) applymod.modAmount = 0; //Limit to min value if needed!
			addattenuation += (float)applymod.modAmount; //Range is 960cB, so convert and apply(add to the initial attenuation generator)!
		}
	}
	else if (lookupSFPresetModGlobal(soundfont, preset,pbag, continuousController7ToInitialAttenuation,&applymod)) //Gotten MIDI Continuous Controller 7 to Initial Attenuation?
	{
		applymod.modAmount = LE16(applymod.modAmount); //Patch!
		if (applymod.modAmount>960) applymod.modAmount = 960; //Limit to max value if needed!
		else if (applymod.modAmount<0) applymod.modAmount = 0; //Limit to min value if needed!
		addattenuation = applymod.modAmount; //Range is 960cB, so convert and apply(add to the initial attenuation generator)!
	}
	tempattenuation = addattenuation * attenuationcontrol; //How much do we want to attenuate?
	if (tempattenuation > 960.0f) tempattenuation = 960.0f; //Limit!
	if (tempattenuation < 0.0f) tempattenuation = 0.0f; //Limit!
	attenuation += tempattenuation; //96dB range volume using a 960cB attenuation!

	//CC11
	addattenuation = 960.0f; //How much to use as a factor (default)!
	attenuationcontrol = calcNegativeUnipolarSource((channel->expression & 0x7F),0x7F); //The source of the attenuation!
	if (lookupSFInstrumentModGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, continuousController11ToInitialAttenuation, &applymod)) //Gotten MIDI Continuous Controller 11 to Initial Attenuation?
	{
		applymod.modAmount = LE16(applymod.modAmount); //Patch!
		if (applymod.modAmount > 960) applymod.modAmount = 960; //Limit to max value if needed!
		else if (applymod.modAmount < 0) applymod.modAmount = 0; //Limit to min value if needed!
		addattenuation = (float)(applymod.modAmount); //Range is 960cB, so convert and apply(add to the initial attenuation generator)!
		if (lookupSFPresetModGlobal(soundfont, preset, pbag, continuousController11ToInitialAttenuation, &applymod)) //Gotten MIDI Continuous Controller 11 to Initial Attenuation?
		{
			applymod.modAmount = LE16(applymod.modAmount); //Patch!
			if (applymod.modAmount > 960) applymod.modAmount = 960; //Limit to max value if needed!
			else if (applymod.modAmount < 0) applymod.modAmount = 0; //Limit to min value if needed!
			addattenuation += (float)applymod.modAmount; //Range is 960cB, so convert and apply(add to the initial attenuation generator)!
		}
	}
	else if (lookupSFPresetModGlobal(soundfont, preset, pbag, continuousController11ToInitialAttenuation, &applymod)) //Gotten MIDI Continuous Controller 11 to Initial Attenuation?
	{
		applymod.modAmount = LE16(applymod.modAmount); //Patch!
		if (applymod.modAmount > 960) applymod.modAmount = 960; //Limit to max value if needed!
		else if (applymod.modAmount < 0) applymod.modAmount = 0; //Limit to min value if needed!
		addattenuation = applymod.modAmount; //Range is 960cB, so convert and apply(add to the initial attenuation generator)!
	}
	tempattenuation = addattenuation * attenuationcontrol; //How much do we want to attenuate?
	if (tempattenuation > 960.0f) tempattenuation = 960.0f; //Limit!
	if (tempattenuation < 0.0f) tempattenuation = 0.0f; //Limit!
	attenuation += tempattenuation; //96dB range volume using a 960cB attenuation!

	if (attenuation>1440.0f) attenuation = 1440.0f; //Limit to max!
	if (attenuation<0.0f) attenuation = 0.0f; //Limit to min!

	attenuation = MIDIattenuate(attenuation); //144dB(1440cB) range volume using attenuation!
	
	//Clip final attenuation and set the attenuation to use!
	if (attenuation>1.0f) attenuation = 1.0f; //Limit to max!
	if (attenuation<0.0f) attenuation = 0.0f; //Limit to min!

	#ifdef IS_LONGDOUBLE
	voice->initialAttenuation = attenuation; //We're converted to a rate of 960 cb!
	#else
	voice->initialAttenuation = attenuation; //We're converted to a rate of 960 cb!
	#endif

	//Determine panning!
	panningtemp = 0.0f; //Default: no panning at all: centered!
	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, pan, &applyigen)) //Gotten panning?
	{
		panningtemp = (float)LE16(applyigen.genAmount.shAmount); //Get the panning specified!
	}
	panningtemp *= 0.001f; //Make into a percentage, it's in 0.1% units!
	voice->initpanning = panningtemp; //Set the initial panning, as a factor!

	panningtemp = 1000.0f; //Default to none!
	if (lookupSFInstrumentModGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount),ibag,continuousController10ToPanPosition,&applymod)) //Gotten panning modulator?
	{
		panningtemp = (float)LE16(applymod.modAmount); //Get the amount specified!
		if (lookupSFPresetModGlobal(soundfont, preset, pbag, continuousController10ToPanPosition, &applymod)) //Gotten panning modulator?
		{
			panningtemp += (float)LE16(applymod.modAmount); //Get the amount specified!
		}
	}
	else if (lookupSFPresetModGlobal(soundfont, preset, pbag, continuousController10ToPanPosition, &applymod)) //Gotten panning modulator?
	{
		panningtemp = (float)LE16(applymod.modAmount); //Get the amount specified!
	}
	panningtemp *= 0.001f; //Make into a percentage, it's in 0.1% units!
	voice->panningmod = panningtemp; //Apply the modulator!

	//Chorus percentage
	panningtemp = 200.0f; //Default to none!
	if (lookupSFInstrumentModGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, continuousController93ToChorusEffectsSend, &applymod)) //Gotten panning modulator?
	{
		panningtemp = (float)LE16(applymod.modAmount); //Get the amount specified!
		if (lookupSFPresetModGlobal(soundfont, preset, pbag, continuousController93ToChorusEffectsSend, &applymod)) //Chorus effects send specified?
		{
			panningtemp += (float)LE16(applymod.modAmount); //Chorus effects send, in 0.1% units!
		}
	}
	else if (lookupSFPresetModGlobal(soundfont, preset, pbag, continuousController93ToChorusEffectsSend, &applymod)) //Chorus effects send specified?
	{
		panningtemp = ((float)LE16(applymod.modAmount)); //Chorus effects send, in 0.1% units!
	}
	panningtemp *= 0.001f; //Make into a percentage, it's in 0.1% units!


	panningtemp *= (1.0f/127.0f); //Linear depth!

	for (chorusreverbdepth=1;chorusreverbdepth<0x100;chorusreverbdepth++) //Process all possible chorus depths!
	{
		voice->chorusdepth[chorusreverbdepth] = (panningtemp*(float)chorusreverbdepth); //Apply the volume!
	}
	voice->chorusdepth[0] = 0.0f; //Always none at the original level!

	//Reverb percentage
	panningtemp = 200.0f; //Default to none!
	if (lookupSFInstrumentModGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, continuousController91ToReverbEffectsSend, &applymod)) //Gotten panning modulator?
	{
		panningtemp = (float)LE16(applymod.modAmount); //Get the amount specified!
		if (lookupSFPresetModGlobal(soundfont, preset, pbag, continuousController91ToReverbEffectsSend, &applymod)) //Reverb effects send specified?
		{
			panningtemp += (float)LE16(applymod.modAmount); //Reverb effects send, in 0.1% units!
		}
	}
	else if (lookupSFPresetModGlobal(soundfont, preset, pbag, continuousController91ToReverbEffectsSend, &applymod)) //Reverb effects send specified?
	{
		panningtemp = (float)LE16(applymod.modAmount); //Reverb effects send, in 0.1% units!
	}
	panningtemp *= 0.001f; //Make into a percentage, it's in 0.1% units!

	panningtemp *= (1.0f/127.0f); //Linear depth!

	for (chorusreverbdepth=0;chorusreverbdepth<0x100;chorusreverbdepth++) //Process all possible chorus depths!
	{
		for (chorusreverbchannel=0;chorusreverbchannel<2;chorusreverbchannel++) //Process all channels!
		{
			if (chorusreverbdepth==0)
			{
				voice->reverbdepth[chorusreverbdepth][chorusreverbchannel] = 0.0; //Nothing at the main channel!
			}
			else //Valid depth?
			{
				voice->reverbdepth[chorusreverbdepth][chorusreverbchannel] = (float)dB2factor((panningtemp * chorusreverbdepth),1.0f); //Apply the volume!
			}
		}
	}
	
	voice->currentchorusdepth = channel->choruslevel; //Current chorus depth!
	voice->currentreverbdepth = channel->reverblevel; //Current reverb depth!

	for (chorusreverbchannel=0;chorusreverbchannel<2;++chorusreverbchannel) //Process all reverb&chorus channels, precalculating every used value!
	{
		voice->activechorusdepth[chorusreverbchannel] = voice->chorusdepth[voice->currentchorusdepth]; //The chorus feedback strength for that channel!
	}

	for (chorusreverbchannel=0;chorusreverbchannel<2;++chorusreverbchannel) //Process all reverb&chorus channels, precalculating every used value!
	{
		voice->activereverbdepth[chorusreverbchannel] = voice->reverbdepth[voice->currentreverbdepth][chorusreverbchannel]; //The selected value!
	}

	//Apply low pass filter!
	voice->lowpassfilter_freq = 13500.0f; //Default: no low pass filter!
	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, initialFilterFc, &applyigen)) //Filter enabled?
	{
		voice->lowpassfilter_freq = (8.176f*cents2samplesfactorf((float)LE16(applyigen.genAmount.shAmount))); //Set a low pass filter to it's initial value!
		if (voice->lowpassfilter_freq>20000.0f) voice->lowpassfilter_freq = 20000.0f; //Apply maximum!
	}

	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, modEnvToFilterFc, &applyigen)) //Gotten a filter on the modulation envelope's Frequency cutoff?
	{
		voice->lowpassfilter_modenvfactor = LE16(applyigen.genAmount.shAmount); //Apply the filter for frequency cutoff!
	}
	else
	{
		voice->lowpassfilter_modenvfactor = 0; //Apply no filter for frequency cutoff!
	}

	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, modEnvToPitch, &applyigen)) //Gotten a filter on the modulation envelope's pitch?
	{
		voice->modenv_pitchfactor = LE16(applyigen.genAmount.shAmount); //Apply the filter for frequency cutoff!
	}
	else
	{
		voice->modenv_pitchfactor = 0; //Apply no filter for frequency cutoff!
	}

	//First, set all chorus data and delays!
	for (chorusreverbdepth=0;chorusreverbdepth<CHORUSSIZE;++chorusreverbdepth)
	{
		voice->modulationratiocents[chorusreverbdepth] = 1200; //Default ratio: no modulation!
		voice->modulationratiosamples[chorusreverbdepth] = 1.0f; //Default ratio: no modulation!
		voice->lowpass_modulationratio[chorusreverbdepth] = 1200.0f; //Default ratio: no modulation!
		voice->lowpass_modulationratiosamples[chorusreverbdepth] = voice->lowpassfilter_freq; //Default ratio: no modulation!
		voice->chorusdelay[chorusreverbdepth] = (uint_32)((chorus_delay[chorusreverbdepth])*(float)LE16(voice->sample.dwSampleRate)); //Total delay to apply for this channel!
		voice->chorussinpos[chorusreverbdepth] = fmodf((float)voice->chorusdelay[chorusreverbdepth],360.0f)*sinustable_percision_reverse; //Initialize the starting chorus sin position for the first sample!
		voice->chorussinposstep = MIDI_CHORUS_SINUS_BASE*(1.0f/(float)LE32(voice->sample.dwSampleRate))*sinustable_percision_reverse; //How much time to add to the chorus sinus after each sample
		voice->isfinalchannel_chorus[chorusreverbdepth] = (chorusreverbdepth==(CHORUSSIZE-1)); //Are we the final channel?
		voice->lowpass_dirty[chorusreverbdepth] = 0; //We're not dirty anymore by default: we're loaded!
		voice->last_lowpass[chorusreverbdepth] = modulateLowpass(voice,1.0f,(byte)chorusreverbdepth); //The current low-pass filter to use!
		initSoundFilter(&voice->lowpassfilter[chorusreverbdepth],0,voice->last_lowpass[chorusreverbdepth],(float)LE32(voice->sample.dwSampleRate)); //Apply a default low pass filter to use!
		voice->lowpass_dirty[chorusreverbdepth] = 0; //We're not dirty anymore by default: we're loaded!
	}

	//Now, set all reverb channel information!
	for (chorusreverbdepth=0;chorusreverbdepth<REVERBSIZE;++chorusreverbdepth)
	{
		voice->reverbvol[chorusreverbdepth] = voice->activereverbdepth[chorusreverbdepth]; //Chorus reverb volume!
		voice->reverbdelay[chorusreverbdepth] = (uint_32)((reverb_delay[chorusreverbdepth])*(float)LE16(voice->sample.dwSampleRate)); //Total delay to apply for this channel!
		voice->isfinalchannel_reverb[chorusreverbdepth] = (chorusreverbdepth==(REVERBSIZE-1)); //Are we the final channel?
	}

	for (chorusreverbdepth=0;chorusreverbdepth<CHORUSREVERBSIZE;++chorusreverbdepth)
	{
		initSoundFilter(&voice->reverbfilter[(chorusreverbdepth<<1)],0,voice->lowpassfilter_freq*((chorusreverbdepth)?1.0f:(0.7f*powf(0.9f,(float)(chorusreverbdepth>>1)))),(float)LE32(voice->sample.dwSampleRate)); //Apply a default low pass filter to use!
		initSoundFilter(&voice->reverbfilter[((chorusreverbdepth<<1)|1)],0,voice->lowpassfilter_freq*((chorusreverbdepth)?1.0f:(0.7f*powf(0.9f,(float)(chorusreverbdepth>>1)))),(float)LE32(voice->sample.dwSampleRate)); //Apply a default low pass filter to use!
	}

	//Setup default channel chorus/reverb!
	#ifdef IS_LONGDOUBLE
	voice->activechorusdepth[0] = 1.0L; //Always the same: produce full sound!
	voice->activereverbdepth[0] = 1.0L; //Always the same: produce full sound!
	#else
	voice->activechorusdepth[0] = 1.0; //Always the same: produce full sound!
	voice->activereverbdepth[0] = 1.0; //Always the same: produce full sound!
	#endif
	voice->chorusvol[0] = voice->activechorusdepth[0];
	voice->reverbvol[0] = voice->activereverbdepth[0]; //Chorus reverb volume, fixed!

	//Pitch wheel modulator
	pitchwheeltemp = 12700.0f; //Default to 12700 cents!
	if (lookupSFInstrumentModGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount),ibag,pitchWheeltoInitialPitchControlledByPitchWheelSensitivity,&applymod)) //Gotten panning modulator?
	{
		pitchwheeltemp = (float)LE16(applymod.modAmount); //Get the amount specified!
		if (lookupSFPresetModGlobal(soundfont,preset, pbag, pitchWheeltoInitialPitchControlledByPitchWheelSensitivity, &applymod)) //Gotten panning modulator?
		{
			pitchwheeltemp += (float)LE16(applymod.modAmount); //Get the amount specified!
		}
	}
	else if (lookupSFPresetModGlobal(soundfont, preset, pbag, pitchWheeltoInitialPitchControlledByPitchWheelSensitivity, &applymod)) //Gotten panning modulator?
	{
		pitchwheeltemp = (float)LE16(applymod.modAmount); //Get the amount specified!
	}
	voice->pitchwheelmod = pitchwheeltemp; //Apply the modulator!	

	//Now determine the volume envelope!
	voice->CurrentVolumeEnvelope = 0.0f; //Default: nothing yet, so no volume, Give us full priority Volume-wise!
	voice->CurrentModulationEnvelope = 0.0f; //Default: nothing tet, so no modulation!

	//Apply loop flags!
	voice->currentloopflags = 0; //Default: no looping!
	if (lookupSFInstrumentGenGlobal(soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, sampleModes, &applyigen)) //Gotten looping?
	{
		switch (LE16(applyigen.genAmount.wAmount)) //What loop?
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

	//Save our instrument we're playing!
	voice->instrument = channel->program;
	voice->bank = channel->activebank;

	//Final adjustments and set active!
	ADSR_init((float)voice->sample.dwSampleRate, note->noteon_velocity, &voice->VolumeEnvelope, soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, preset, pbag, delayVolEnv, attackVolEnv, holdVolEnv, decayVolEnv, sustainVolEnv, releaseVolEnv, -rootMIDITone, keynumToVolEnvHold, keynumToVolEnvDecay); //Initialise our Volume Envelope for use!
	ADSR_init((float)voice->sample.dwSampleRate, note->noteon_velocity, &voice->ModulationEnvelope, soundfont, LE16(instrumentptr.genAmount.wAmount), ibag, preset, pbag, delayModEnv, attackModEnv, holdModEnv, decayModEnv, sustainModEnv, releaseModEnv, -rootMIDITone, keynumToModEnvHold, keynumToModEnvDecay); //Initialise our Modulation Envelope for use!
	#ifdef MIDI_LOCKSTART
	unlock(voice->locknumber); //Unlock us!
	#endif
	unlockMPURenderer(); //We're finished!
	setSampleRate(&MIDIDEVICE_renderer, voice, (float)LE16(voice->sample.dwSampleRate)); //Use this new samplerate!
	voice->starttime = starttime++; //Take a new start time!
	return 0; //Run: we're active!
}

/* Execution flow support */

OPTINLINE byte MIDIDEVICE_FilterChannelVoice(byte selectedchannel, byte channel)
{
	if (!(MIDI_channels[channel].mode&MIDIDEVICE_OMNI)) //No Omni mode?
	{
		if (channel!=selectedchannel) //Different channel selected?
		{
			return 0; //Don't execute!
		}
	}
	if (!(MIDI_channels[channel].mode&MIDIDEVICE_POLY)) //Mono mode?
	{
		if (!((selectedchannel>=MIDI_channels[channel].channelrangemin) &&
			(selectedchannel<=MIDI_channels[channel].channelrangemax))) //Out of range?
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
			#ifdef MIDI_LOCKSTART
			lock(activevoices[i].locknumber); //Lock us!
			#endif
			if (activevoices[i].VolumeEnvelope.active) //Active note?
			{
				if ((activevoices[i].note->channel == channel) && (activevoices[i].note->note == note)) //Note found?
				{
					activevoices[i].request_off = 1; //We're requesting to be turned off!
					activevoices[i].note->noteoff_velocity = velocity; //Note off velocity!
				}
			}
			#ifdef MIDI_LOCKSTART
			unlock(activevoices[i].locknumber); //Unlock us!
			#endif
		}
	}
}

OPTINLINE void MIDIDEVICE_AllNotesOff(byte selectedchannel, byte channel) //Used with command, mode change and Mono Mode.
{
	word noteoff; //Current note to turn off!
	//Note values
	for (noteoff=0;noteoff<0x100;) //Process all notes!
	{
		MIDIDEVICE_noteOff(selectedchannel,channel,(byte)noteoff++,64); //Execute Note Off!
	}
	#ifdef MIDI_LOG
	dolog("MPU","MIDIDEVICE: ALL NOTES OFF: %u",selectedchannel); //Log it!
	#endif
}

SDL_sem *activeSenseLock = NULL; //Active Sense lock!

byte MIDIDEVICE_ActiveSensing = 0; //Active Sensing?
word MIDIDEVICE_ActiveSenseCounter = 0; //Counter for Active Sense!

void MIDIDEVICE_activeSense_Timer() //Timeout while Active Sensing!
{
	if (MIDIDEVICE_ActiveSensing) //Are we Active Sensing?
	{
		if (++MIDIDEVICE_ActiveSenseCounter > 300) //300ms passed?
		{
			byte channel, currentchannel;
			PostSem(activeSenseLock); //Release our lock to prevent races on the main thread!
			lock(LOCK_MAINTHREAD); //Make sure we're the only ones!
			for (currentchannel = 0; currentchannel < 0x10;) //Process all active channels!
			{
				for (channel = 0; channel < 0x10;)
				{
					MIDIDEVICE_AllNotesOff(currentchannel, channel++); //Turn all notes off!
				}
				++currentchannel; //Next channel!
			}
			MIDIDEVICE_ActiveSensing = 0; //Reset our flag!
			unlock(LOCK_MAINTHREAD);
			WaitSem(activeSenseLock); //Relock!
		}
	}
}

void MIDIDEVICE_tickActiveSense() //Tick the Active Sense (MIDI) line with any command/data!
{
	WaitSem(activeSenseLock)
	MIDIDEVICE_ActiveSenseCounter = 0; //Reset the counter to count again!
	PostSem(activeSenseLock)
}


void MIDIDEVICE_ActiveSenseFinished()
{
	removetimer("MIDI Active Sense Timeout"); //Remove the current timeout, if any!
	if (activeSenseLock) //Is Active Sensing used?
	{
		SDL_DestroySemaphore(activeSenseLock); //Destroy our lock!
		activeSenseLock = NULL; //Nothing anymore!
	}
}

void MIDIDEVICE_ActiveSenseInit()
{
	MIDIDEVICE_ActiveSenseFinished(); //Finish old one!
	activeSenseLock = SDL_CreateSemaphore(1); //Create our lock!
	addtimer(300.0f / 1000.0f, &MIDIDEVICE_activeSense_Timer, "MIDI Active Sense Timeout", 1, 1, activeSenseLock); //Add the Active Sense timer!
}

OPTINLINE void MIDIDEVICE_noteOn(byte selectedchannel, byte channel, byte note, byte velocity)
{
	byte purpose;

	if (MIDIDEVICE_FilterChannelVoice(selectedchannel,channel)) //To be applied?
	{
		if (!(MIDI_channels[channel].mode&MIDIDEVICE_POLY)) //Mono mode?
		{
			MIDIDEVICE_AllNotesOff(selectedchannel,channel); //Turn all notes off first!
		}
		MIDI_channels[channel].notes[note].noteon_velocity = velocity; //Add velocity to our lookup!
		purpose = (channel==MIDI_DRUMCHANNEL)?1:0; //Are we a drum channel?
		int voice, foundvoice = -1, voicetosteal = -1;
		int_32 stolenvoiceranking = 0, currentranking; //Stolen voice ranking starts lowest always!
		for (voice = 0; voice < __MIDI_NUMVOICES; voice++) //Find a voice!
		{
			if (activevoices[voice].purpose==purpose) //Our type of channel (drums vs melodic channels)?
			{
				if (MIDIDEVICE_newvoice(&activevoices[voice], channel, note)) //Failed to allocate?
				{
					#ifdef MIDI_LOCKSTART
					lock(activevoices[voice].locknumber); //Lock us!
					#endif
					if (activevoices[voice].VolumeEnvelope.active) //Are we active?
					{
						//Create ranking by scoring the voice!
						currentranking = 0; //Start with no ranking!
						if (activevoices[voice].VolumeEnvelope.active == ADSR_RELEASE) currentranking -= 2000; //Release gets priority to be stolen!
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
						/*if ((activevoices[voice].bank == MIDI_channels[channel].bank) && (activevoices[voice].instrument == MIDI_channels[channel].program) && (activevoices[voice].note->note == note)) //Same note retriggered?
						{
							currentranking -= (int_32)(volume*1000.0f); //We're giving us priority to be stolen, if needed! Take us as if we're having no volume at all!
							++currentranking; //We're taking all but the lowest volume (0)!
						}*/
						if ((stolenvoiceranking > currentranking) || (voicetosteal == -1)) //We're a lower rank or the first ranking?
						{
							stolenvoiceranking = currentranking; //New voice to steal!
							voicetosteal = voice; //Steal this voice, if needed!
						}
						else if ((currentranking == stolenvoiceranking) && (voicetosteal != -1)) //Same ranking as the last one found?
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
					#ifdef MIDI_LOCKSTART
					unlock(activevoices[voice].locknumber); //unlock us!
					#endif
				}
				else //Allocated?
				{
					foundvoice = voice; //Found this voice!
					break; //Stop searching!
				}
			}
		}
		if (foundvoice == -1) //No channels available? We need voice stealing!
		{
			//Perform voice stealing using voicetosteal, if available!
			if (voicetosteal != -1) //Something to steal?
			{
				lockMPURenderer();
				#ifdef MIDI_LOCKSTART
				lock(activevoices[voicetosteal].locknumber); //Lock us!
				#endif
				activevoices[voicetosteal].VolumeEnvelope.active = 0; //Make inactive!
				#ifdef MIDI_LOCKSTART
				unlock(activevoices[voicetosteal].locknumber); //unlock us!
				#endif
				unlockMPURenderer();
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
			for (channel=0;channel<0x10;) //Process all channels!
			{
				MIDIDEVICE_noteOff(currentchannel,channel++,firstparam,current->buffer[1]); //Execute Note Off!
			}
			#ifdef MIDI_LOG
				dolog("MPU","MIDIDEVICE: NOTE OFF: Channel %u Note %u Velocity %u",currentchannel,firstparam,current->buffer[1]); //Log it!
			#endif
			break;
		case 0x90: //Note on?
			if (!current->buffer[1]) goto noteoff; //Actually a note off?
			for (channel=0;channel<0x10;) //Process all channels!
			{
				MIDIDEVICE_noteOn(currentchannel, channel++, firstparam, current->buffer[1]); //Execute Note On!
			}
			#ifdef MIDI_LOG
				dolog("MPU","MIDIDEVICE: NOTE ON: Channel %u Note %u Velocity %u",currentchannel,firstparam,current->buffer[1]); //Log it!
			#endif
			break;
		case 0xA0: //Aftertouch?
			lockMPURenderer(); //Lock the audio!
			MIDI_channels[currentchannel].notes[firstparam].pressure = current->buffer[1];
			unlockMPURenderer(); //Unlock the audio!
			#ifdef MIDI_LOG
				dolog("MPU","MIDIDEVICE: Aftertouch: %u-%u",currentchannel,MIDI_channels[currentchannel].notes[firstparam].pressure); //Log it!
			#endif
			break;
		case 0xB0: //Control change?
			switch (firstparam) //What control?
			{
				case 0x00: //Bank Select (MSB)
					#ifdef MIDI_LOG
						dolog("MPU","MIDIDEVICE: Bank select MSB on channel %u: %02X",currentchannel,current->buffer[1]); //Log it!
					#endif
						if (currentchannel != MIDI_DRUMCHANNEL) //Don't receive on channel 9: it's locked!
						{
							lockMPURenderer(); //Lock the audio!
							MIDI_channels[currentchannel].bank &= 0x3F80; //Only keep MSB!
							MIDI_channels[currentchannel].bank |= current->buffer[1]; //Set LSB!
							unlockMPURenderer(); //Unlock the audio!
						}
					break;
				case 0x20: //Bank Select (LSB) (see cc0)
#ifdef MIDI_LOG
					dolog("MPU", "MIDIDEVICE: Bank select LSB on channel %u: %02X", currentchannel, current->buffer[1]); //Log it!
#endif
					if (currentchannel != MIDI_DRUMCHANNEL) //Don't receive on channel 9: it's locked!
					{
						lockMPURenderer(); //Lock the audio!
						MIDI_channels[currentchannel].bank &= 0x7F; //Only keep LSB!
						MIDI_channels[currentchannel].bank |= (current->buffer[1] << 7); //Set MSB!
						unlockMPURenderer(); //Unlock the audio!
					}
					break;

				case 0x07: //Volume (MSB) CC 07
					#ifdef MIDI_LOG
						dolog("MPU", "MIDIDEVICE: Volume MSB on channel %u: %02X",currentchannel, current->buffer[1]); //Log it!
					#endif
					lockMPURenderer(); //Lock the audio!
					MIDI_channels[currentchannel].volumeMSB = current->buffer[1]; //Set MSB!
					unlockMPURenderer(); //Unlock the audio!
					break;
				case 0x0B: //Expression (MSB) CC 11
					#ifdef MIDI_LOG
						dolog("MPU", "MIDIDEVICE: Volume MSB on channel %u: %02X",currentchannel, current->buffer[1]); //Log it!
					#endif
					lockMPURenderer(); //Lock the audio!
					MIDI_channels[currentchannel].expression = current->buffer[1]; //Set Expression!
					unlockMPURenderer(); //Unlock the audio!
					break;
				case 0x27: //Volume (LSB) CC 39
#ifdef MIDI_LOG
					dolog("MPU", "MIDIDEVICE: Volume LSB on channel %u: %02X", currentchannel, current->buffer[1]); //Log it!
#endif
					lockMPURenderer(); //Lock the audio!
					MIDI_channels[currentchannel].volumeLSB = current->buffer[1]; //Set LSB!
					unlockMPURenderer(); //Unlock the audio!
					break;

				case 0x0A: //Pan position (MSB)
					#ifdef MIDI_LOG
						dolog("MPU", "MIDIDEVICE: Pan position MSB on channel %u: %02X",currentchannel, current->buffer[1]); //Log it!
					#endif
					lockMPURenderer(); //Lock the audio!
					MIDI_channels[currentchannel].panposition &= 0x3F80; //Only keep MSB!
					MIDI_channels[currentchannel].panposition |= current->buffer[1]; //Set LSB!
					unlockMPURenderer(); //Unlock the audio!
					break;
				case 0x2A: //Pan position (LSB)
					#ifdef MIDI_LOG
						dolog("MPU", "MIDIDEVICE: Pan position LSB on channel %u: %02X",currentchannel, current->buffer[1]); //Log it!
					#endif
					lockMPURenderer(); //Lock the audio!
					MIDI_channels[currentchannel].panposition &= 0x7F; //Only keep LSB!
					MIDI_channels[currentchannel].panposition |= (current->buffer[1] << 7); //Set MSB!
					unlockMPURenderer(); //Unlock the audio!
					break;

				//case 0x01: //Modulation wheel (MSB)
					//break;
				//case 0x04: //Foot Pedal (MSB)
					//break;
				//case 0x06: //Data Entry, followed by cc100&101 for the address.
					//break;
				//case 0x21: //Modulation wheel (LSB)
					//break;
				//case 0x24: //Foot Pedal (LSB)
					//break;
				//case 0x26: //Data Entry, followed by cc100&101 for the address.
					//break;
				case 0x40: //Hold Pedal (On/Off) = Sustain Pedal
					#ifdef MIDI_LOG
						dolog("MPU", "MIDIDEVICE:  Channel %u; Hold pedal: %02X=%u", currentchannel, current->buffer[1],(current->buffer[1]&MIDI_CONTROLLER_ON)?1:0); //Log it!
					#endif
					lockMPURenderer(); //Lock the audio!
					MIDI_channels[currentchannel].sustain = (current->buffer[1]&MIDI_CONTROLLER_ON)?1:0; //Sustain?
					unlockMPURenderer(); //Unlock the audio!
					break;
				//case 0x41: //Portamento (On/Off)
					//break;
				//case 0x47: //Resonance a.k.a. Timbre
					//break;
				//case 0x4A: //Frequency Cutoff (a.k.a. Brightness)
					//break;
				case 0x5B: //Reverb Level
					lockMPURenderer(); //Lock the audio!
					MIDI_channels[currentchannel].reverblevel = current->buffer[1]; //Reverb level!
					unlockMPURenderer(); //Unlock the audio!
					break;
				case 0x5D: //Chorus Level
					lockMPURenderer(); //Lock the audio!
					MIDI_channels[currentchannel].choruslevel = current->buffer[1]; //Chorus level!
					unlockMPURenderer(); //Unlock the audio!
					break;
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
					for (channel=0;channel<0x10;)
					{
						MIDIDEVICE_AllNotesOff(currentchannel,channel++); //Turn all notes off!
					}
					if ((firstparam&0x7C)==0x7C) //Mode change command?
					{
						lockMPURenderer(); //Lock the audio!
						switch (firstparam&3) //What mode change?
						{
						case 0: //Omni Mode Off
							#ifdef MIDI_LOG
								dolog("MPU", "MIDIDEVICE: Channel %u, OMNI OFF", currentchannel); //Log it!
							#endif
							MIDI_channels[currentchannel].mode &= ~MIDIDEVICE_OMNI; //Disable Omni mode!
							break;
						case 1: //Omni Mode On
							#ifdef MIDI_LOG
								dolog("MPU", "MIDIDEVICE: Channel %u, OMNI ON", currentchannel); //Log it!
							#endif
							MIDI_channels[currentchannel].mode |= MIDIDEVICE_OMNI; //Enable Omni mode!
							break;
						case 2: //Mono operation
							MIDI_channels[currentchannel].mode &= ~MIDIDEVICE_POLY; //Disable Poly mode!
							MIDI_channels[currentchannel].mode &= ~MIDIDEVICE_OMNI; //Disable Omni mode!
							if (current->buffer[1]) //Omni Off+Ammount of channels to respond to?
							{
								#ifdef MIDI_LOG
									dolog("MPU", "MIDIDEVICE: Channel %u, MONO without OMNI, Channels to respond: %u", currentchannel,current->buffer[1]); //Log it!
								#endif
								rangemax = rangemin = currentchannel;
								rangemax += current->buffer[1]; //Maximum range!
								--rangemax;
							}
							else //Omni On?
							{
								#ifdef MIDI_LOG
									dolog("MPU", "MIDIDEVICE: Channel %u, MONO with OMNI, Respond to all channels.", currentchannel); //Log it!
								#endif
								MIDI_channels[currentchannel].mode |= MIDIDEVICE_OMNI; //Enable Omni mode!
								rangemin = 0; //Respond to...
								rangemax = 0xF; //All channels!
							}
							MIDI_channels[currentchannel].channelrangemin = rangemin;
							MIDI_channels[currentchannel].channelrangemax = rangemax;
							break;
						case 3: //Poly Operation
							#ifdef MIDI_LOG
								dolog("MPU", "MIDIDEVICE: Channel %u, POLY", currentchannel); //Log it!
							#endif
							MIDI_channels[currentchannel].mode |= MIDIDEVICE_POLY; //Enable Poly mode!
							break;
						default:
							break;
						}
						unlockMPURenderer(); //Unlock the audio!
					}
					break;
				default: //Unknown controller?
					#ifdef MIDI_LOG
						dolog("MPU", "MIDIDEVICE: Unknown Continuous Controller change: %u=%u", currentchannel, firstparam); //Log it!
					#endif
					break;
			}
			break;
		case 0xC0: //Program change?
			lockMPURenderer(); //Lock the audio!
			MIDI_channels[currentchannel].program = firstparam; //What program?
			MIDI_channels[currentchannel].activebank = MIDI_channels[currentchannel].bank; //Apply bank from Bank Select Messages!
			unlockMPURenderer(); //Unlock the audio!
			#ifdef MIDI_LOG
				dolog("MPU","MIDIDEVICE: Program change: %u=%u",currentchannel,MIDI_channels[currentchannel].program); //Log it!
			#endif
			break;
		case 0xD0: //Channel pressure?
			lockMPURenderer(); //Lock the audio!
			MIDI_channels[currentchannel].pressure = firstparam;
			unlockMPURenderer(); //Unlock the audio!
			#ifdef MIDI_LOG
				dolog("MPU","MIDIDEVICE: Channel pressure: %u=%u",currentchannel,MIDI_channels[currentchannel].pressure); //Log it!
			#endif
			break;
		case 0xE0: //Pitch wheel?
			lockMPURenderer(); //Lock the audio!
			MIDI_channels[currentchannel].pitch = (sword)((current->buffer[1]<<7)|firstparam); //Actual pitch, converted to signed value!
			unlockMPURenderer(); //Unlock the audio!
			#ifdef MIDI_LOG
				dolog("MPU","MIDIDEVICE: Pitch wheel: %u=%u",currentchannel,MIDI_channels[currentchannel].pitch); //Log it!
			#endif
			break;
		case 0xF0: //System message?
			//We don't handle system messages!
			switch (command)
			{
			case 0xFE: //Active Sense?
				MIDIDEVICE_ActiveSensing = 1; //We're Active Sensing!
				break;
			case 0xFF: //Reset?
				reset_MIDIDEVICE(); //Reset ourselves!
				break;
			default:
				#ifdef MIDI_LOG
					dolog("MPU", "MIDIDEVICE: System messages are unsupported!"); //Log it!
				#endif
				break;
			}
			break;
		default: //Invalid command?
			#ifdef MIDI_LOG
				dolog("MPU","MIDIDEVICE: Unknown command: %02X",command);
			#endif
			break; //Do nothing!
	}
}

/* Buffer support */

void MIDIDEVICE_addbuffer(byte command, MIDIPTR data) //Add a command to the buffer!
{
	#ifdef __HW_DISABLED
	return; //We're disabled!
	#endif

	#ifdef IS_WINDOWS
	if (direct_midi)
	{
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
	}
	#endif

	data->command = command; //Set the command to use!
	MIDIDEVICE_execMIDI(data); //Execute directly!
}

/* Init/destroy support */
extern byte RDPDelta; //RDP toggled?
void done_MIDIDEVICE() //Finish our midi device!
{
	#ifdef __HW_DISABLED
		return; //We're disabled!
	#endif
	#ifdef IS_WINDOWS
	if (direct_midi)
	{
		// turn any MIDI notes currently playing:
		midiOutReset(device);
		lock(LOCK_INPUT);
		if (RDPDelta&1)
		{
			unlock(LOCK_INPUT);
			return;
		}
		unlock(LOCK_INPUT);
		// Remove any data in MIDI device and close the MIDI Output port
		midiOutClose(device);
		//We're directly sending MIDI to the output!
		return; //Stop: ready!
	}
	#endif
	
	lockaudio();
	//Close the soundfont?
	closeSF(&soundfont);
	int i,j;
	for (i=0;i<__MIDI_NUMVOICES;i++) //Assign all voices available!
	{
		removechannel(&MIDIDEVICE_renderer,&activevoices[i],0); //Remove the channel! Delay at 0.96ms for response speed!
		if (activevoices[i].effect_backtrace_samplespeedup) //Used?
		{
			free_fifobuffer(&activevoices[i].effect_backtrace_samplespeedup); //Release the FIFO buffer containing the entire history!
		}
		for (j=0;j<CHORUSSIZE;++j)
		{
			free_fifobuffer(&activevoices[i].effect_backtrace_chorus[j]); //Release the FIFO buffer containing the entire history!
		}
	}
	MIDIDEVICE_ActiveSenseFinished(); //Finish our Active Sense: we're not needed anymore!
	unlockaudio();
}

byte init_MIDIDEVICE(char *filename, byte use_direct_MIDI) //Initialise MIDI device for usage!
{
	float MIDI_CHORUS_SINUS_CENTS;
	MIDI_CHORUS_SINUS_CENTS = (0.5f*CHORUS_LFO_CENTS); //Cents modulation for the outgoing sinus!
	byte result;
	#ifdef __HW_DISABLED
		return 0; //We're disabled!
	#endif
	#ifdef IS_WINDOWS
	direct_midi = use_direct_MIDI; //Use direct MIDI synthesis by the OS, if any?
	if (direct_midi)
	{
		lock(LOCK_INPUT);
		RDPDelta &= ~1; //Clear our RDP delta flag!
		unlock(LOCK_INPUT);
		// Open the MIDI output port
		flag = midiOutOpen(&device, 0, 0, 0, CALLBACK_NULL);
		if (flag != MMSYSERR_NOERROR) {
			printf("Error opening MIDI Output.\n");
			return 0;
		}
		//We're directly sending MIDI to the output!
		return 1; //Stop: ready!
	}
	#endif
	#ifdef MIDI_LOCKSTART
	for (result=0;result<__MIDI_NUMVOICES;result++) //Process all voices!
	{
		if (getLock(result + MIDI_LOCKSTART)) //Our MIDI lock!
		{
			activevoices[result].locknumber = result+MIDI_LOCKSTART; //Our locking number!
		}
		else
		{
			return 0; //We're disabled!
		}
	}
	#endif
	done_MIDIDEVICE(); //Start finished!
	lockaudio();
	memset(&activevoices,0,sizeof(activevoices)); //Clear all voice data!

	reset_MIDIDEVICE(); //Reset our MIDI device!

	int i,j;
	for (i=0;i<2;++i)
	{
		choruscents[i] = (MIDI_CHORUS_SINUS_CENTS*(float)i); //Cents used for this chorus!
	}

	MIDIDEVICE_generateSinusTable(); //Make sure we can generate sinuses required!

	//Load the soundfont?
	soundfont = readSF(filename); //Read the soundfont, if available!
	if (!soundfont) //Unable to load?
	{
		if (filename[0]) //Valid filename?
		{
			dolog("MPU", "No soundfont found or could be loaded!");
		}
		result = 0; //Error!
	}
	else
	{
		result = 1; //OK!
		for (i=0;i<__MIDI_NUMVOICES;i++) //Assign all voices available!
		{
			activevoices[i].purpose = (((__MIDI_NUMVOICES-i)-1) < MIDI_DRUMVOICES) ? 1 : 0; //Drum or melodic voice? Put the drum voices at the far end!
			activevoices[i].effect_backtrace_samplespeedup = allocfifobuffer(((uint_32)((chorus_delay[CHORUSSIZE])*MAX_SAMPLERATE)+1)<<2,0); //Not locked FIFO buffer containing the entire history!
			for (j=0;j<CHORUSSIZE;++j) //All chorus backtrace channels!
			{
				activevoices[i].effect_backtrace_chorus[j] = allocfifobuffer(((uint_32)((reverb_delay[CHORUSSIZE])*MAX_SAMPLERATE)+1)<<3,0); //Not locked FIFO buffer containing the entire history!				
			}
			addchannel(&MIDIDEVICE_renderer,&activevoices[i],"MIDI Voice",44100.0f,__MIDI_SAMPLES,1,SMPL16S); //Add the channel! Delay at 0.96ms for response speed! 44100/(1000000/960)=42.336 samples/response!
			setVolume(&MIDIDEVICE_renderer,&activevoices[i],MIDI_VOLUME); //We're at 40% volume!
		}
	}
	MIDIDEVICE_ActiveSenseInit(); //Initialise Active Sense!
	unlockaudio();
	return result;
}

byte directMIDISupported()
{
	#ifdef IS_WINDOWS
		return 1; //Supported!
	#endif
	return 0; //Default: Unsupported platform!
}
