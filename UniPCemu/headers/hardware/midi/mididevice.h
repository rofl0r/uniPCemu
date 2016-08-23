#ifndef MIDIDEVICE_H
#define MIDIDEVICE_H

#include "headers/types.h"
#include "headers/hardware/midi/adsr.h" //ADSR support!

//MIDI Drum channel number
#define MIDI_DRUMCHANNEL 9

//All MIDI voices that are available! Originally 64! Minimum of 24 according to General MIDI 1!
#define __MIDI_NUMVOICES 24
//Ammount of drum voices to reserve!
#define MIDI_DRUMVOICES 8
//How many samples to buffer at once! 42 according to MIDI specs! Set to 84 to work!
#define __MIDI_SAMPLES 42

typedef struct
{
byte command; //What command?
byte buffer[2]; //The parameter buffer!
void *next; //Next command, if any!
} MIDICOMMAND, *MIDIPTR;

typedef struct
{
	//First, infomation for looking us up!
	byte channel; //What channel!
	byte note; //What note!
	byte noteon_velocity; //What velocity/AD(SR)!
	float noteon_velocity_factor; //Note on velocity converted to a factor!
	byte noteoff_velocity; //What velocity/(ADS)R!
	byte pressure; //Pressure/volume/aftertouch!
} MIDIDEVICE_NOTE; //Current playing note to process information!

typedef struct
{
	MIDIDEVICE_NOTE notes[0x100]; //All possible MIDI note statuses!
	//Channel information!
	byte control; //Control/current instrument!
	byte program; //Program/instrument!
	byte pressure; //Channel pressure/volume!
	word volume; //Continuous controller volume!
	word panposition; //Continuous controller pan position!
	float lvolume; //Left volume for panning!
	float rvolume; //Right volume for panning!
	word bank; //The bank from a bank select message!
	word activebank; //What bank are we?
	sword pitch; //Current pitch (14-bit value)
	byte sustain; //Enable sustain? Don't process KEY OFF while set!
	byte channelrangemin, channelrangemax; //Ranges of used channels to respond to when in Mono Mode.
	byte mode; //Channel mode: 0=Omni off, Mono; 1=Omni off, Poly; 2=Omni on, Mono; 3=Omni on, Poly;
	//Bit 1=1:Poly/0:Mono; Bit2=1:Omni on/0:Omni off
	/* Omni: respond to all channels (ignore channel part); Poly: Use multiple voices; Mono: Use one voice at the time (end other voices on Note On) */
} MIDIDEVICE_CHANNEL;

typedef struct
{
	int_64 play_counter; //Current play position within the soundfont!
	uint_32 loopsize; //The size of a loop!
	int_64 finallooppos; //Final loop position!
	int_64 finallooppos_playcounter; //Play counter at the final loop position we've calculated!
	//Patches to the sample offsets, calculated before generating sound!
	uint_32 startaddressoffset;
	uint_32 startloopaddressoffset;
	uint_32 endaddressoffset;
	uint_32 endloopaddressoffset;

	float last_sample; //Last retrieved sample!
	float last_result; //Last result of the low pass filter!

	//Stuff for voice stealing
	uint_64 starttime; //When have we started our voice?

	//Our assigned notes/channels for lookup!
	MIDIDEVICE_CHANNEL *channel; //The active channel!
	MIDIDEVICE_NOTE *note; //The active note!
	float initsamplespeedup, pitchwheelmod, initpanning, panningmod; //Precalculated speedup of the samples, to be processed into effective speedup when starting the rendering!
	float effectivesamplespeedup; //The speedup of the samples!
	float lvolume, rvolume; //Left and right panning!
	float lowpassfilter_freq; //What frequency to filter? 0.0f=No filter!
	float lowpassfilter_modenvfactor; //How many cents to apply to the frequency of the low pass filter?

	float CurrentVolumeEnvelope; //Current volume envelope!
	float CurrentModulationEnvelope; //Current modulation envelope!

	float modenv_pitchfactor; //How many cents to apply to the frequency of the sound?

	sfSample sample; //The sample to be played back!
	ADSR VolumeEnvelope; //The volume envelope!
	ADSR ModulationEnvelope; //The modulation envelope!

	byte currentloopflags; //What loopflags are active?
	byte request_off; //Are we to be turned off? Start the release phase when enabled!
	byte lowpass_isfirst; //Are we the first sample to filter?
	byte has_finallooppos; //Do we have a final loop position?

	byte purpose; //0=Normal voice, 1=Drum channel!
	word bank; //What bank are we playing from?
	byte instrument; //What instrument are we playing?
	byte locknumber; //What lock number do we have? Only valid when actually used(lock defined)!
	float initialAttenuation; //Initial attenuation!
} MIDIDEVICE_VOICE;

void MIDIDEVICE_tickActiveSense(); //Tick the Active Sense (MIDI) line with any command/data!
void MIDIDEVICE_addbuffer(byte command, MIDIPTR data); //Add a command to the buffer!
//MIDICOMMAND *MIDIDEVICE_peekbuffer(); //Peek at the buffer!
//int MIDIDEVICE_readbuffer(MIDICOMMAND *result); //Read from the buffer!

byte init_MIDIDEVICE(char *filename); //Initialise MIDI device for usage!
void done_MIDIDEVICE(); //Finish our midi device!

#endif
