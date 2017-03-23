#ifndef MIDIDEVICE_H
#define MIDIDEVICE_H

#include "headers/types.h"
#include "headers/hardware/midi/adsr.h" //ADSR support!
#include "headers/support/fifobuffer.h" //Effect backtrace support for chorus/reverb effects!
#include "headers/support/filters.h" //Filter support!

//MIDI Drum channel number
#define MIDI_DRUMCHANNEL 9

//All MIDI voices that are available! Originally 64! Minimum of 24 according to General MIDI 1!
#define __MIDI_NUMVOICES 24
//Ammount of drum voices to reserve!
#define MIDI_DRUMVOICES 8
//How many samples to buffer at once! 42 according to MIDI specs! Set to 84 to work!
#define __MIDI_SAMPLES 42

//Chorus amount(4 chorus channels) and reverberations including origin(7 reverberations and 1 origin, for a total of 8 copies)
#define CHORUSSIZE 4
#define REVERBSIZE 8
#define CHORUSREVERBSIZE 32

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
	byte choruslevel; //Current chorus depth set!
	byte reverblevel; //Current reverb depth set!
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

	//Stuff for voice stealing
	uint_64 starttime; //When have we started our voice?

	//Our assigned notes/channels for lookup!
	MIDIDEVICE_CHANNEL *channel; //The active channel!
	MIDIDEVICE_NOTE *note; //The active note!
	float pitchwheelmod, initpanning, panningmod; //Precalculated speedup of the samples, to be processed into effective speedup when starting the rendering!
	sword initsamplespeedup; //Initial sample speedup, in cents!
	sword effectivesamplespeedup; //The speedup of the samples, in cents!
	float lvolume, rvolume; //Left and right panning!
	float lowpassfilter_freq; //What frequency to filter? 0.0f=No filter!
	float lowpassfilter_modenvfactor; //How many cents to apply to the frequency of the low pass filter?

	float CurrentVolumeEnvelope; //Current volume envelope!
	float CurrentModulationEnvelope; //Current modulation envelope!

	sword modenv_pitchfactor; //How many cents to apply to the frequency of the sound?

	sfSample sample; //The sample to be played back!
	ADSR VolumeEnvelope; //The volume envelope!
	ADSR ModulationEnvelope; //The modulation envelope!

	byte currentloopflags; //What loopflags are active?
	byte request_off; //Are we to be turned off? Start the release phase when enabled!
	byte has_finallooppos; //Do we have a final loop position?

	byte purpose; //0=Normal voice, 1=Drum channel!
	word bank; //What bank are we playing from?
	byte instrument; //What instrument are we playing?
	byte locknumber; //What lock number do we have? Only valid when actually used(lock defined)!
	float initialAttenuation; //Initial attenuation!

	//Chorus and reverb calculations!
	float chorusdepth[0x100]; //All chorus depths, index 0 is dry sound!
	float reverbdepth[0x100][8]; //All reverb depths, index 0 is dry sound!
	float activechorusdepth[4]; //The chorus depth used for all channels!
	float activereverbdepth[8]; //The reverb depth used for all channels!
	byte currentchorusdepth; //Used chorus depth, set by software when a note is started! 
	byte currentreverbdepth; //Used reverb depth, set by software when a note is started!
	sword modulationratiocents[CHORUSSIZE];
	double modulationratiosamples[CHORUSSIZE]; //Modulation ratio and it's samples rate for faster lookup on boundaries!
	float lowpass_modulationratio[CHORUSSIZE], lowpass_modulationratiosamples[CHORUSSIZE]; //See modulation ratio, but for the low pass filter only!
	FIFOBUFFER *effect_backtrace_samplespeedup; //A backtrace of the sample speedup through time for each sample played in the main stream!
	FIFOBUFFER *effect_backtrace_chorus[CHORUSSIZE]; //Chorus backtrace for reverb purpose, stereo!
	uint_32 chorusdelay[CHORUSSIZE]; //Total delay for the chorus/reverb channel!
	uint_32 reverbdelay[REVERBSIZE]; //Total delay for the chorus/reverb channel!
	float chorusvol[CHORUSSIZE]; //Chorus/reverb volume!
	float reverbvol[REVERBSIZE]; //Reverb volume!
	float chorussinpos[CHORUSSIZE]; //All current chorus sin positions, wrapping around the table limit!
	float chorussinposstep; //The step of one sample in chorussinpos, wrapping around 
	byte isfinalchannel_chorus[CHORUSSIZE]; //Are we the final channel to process for the current sample?
	byte isfinalchannel_reverb[REVERBSIZE]; //Are we the final channel to process for the current sample?
	HIGHLOWPASSFILTER lowpassfilter[CHORUSSIZE]; //Each channel has it's own low-pass filter!
	float last_lowpass[CHORUSSIZE]; //Last lowpass frequency used!
	byte lowpass_dirty[CHORUSSIZE]; //Are we to update the low-pass filter?
} MIDIDEVICE_VOICE;

void MIDIDEVICE_tickActiveSense(); //Tick the Active Sense (MIDI) line with any command/data!
void MIDIDEVICE_addbuffer(byte command, MIDIPTR data); //Add a command to the buffer!
//MIDICOMMAND *MIDIDEVICE_peekbuffer(); //Peek at the buffer!
//int MIDIDEVICE_readbuffer(MIDICOMMAND *result); //Read from the buffer!

byte init_MIDIDEVICE(char *filename, byte use_direct_MIDI); //Initialise MIDI device for usage!
void done_MIDIDEVICE(); //Finish our midi device!

byte directMIDISupported(); //Direct MIDI supported on the compiled platform?

#endif
