#include "headers/types.h" //Basic types!
#include "headers/support/sf2.h" //Soundfont support!
#include "headers/hardware/midi/mididevice.h" //Our own typedefs!
#include "headers/support/zalloc.h" //Zero allocation support!
#include "headers/emu/sound.h" //Sound support!
#include "headers/support/log.h" //Logging support!
#include "headers/support/highrestimer.h" //High resolution timer support!

//Are we disabled?
#define __HW_DISABLED 1
RIFFHEADER *soundfont; //Our loaded soundfont!

//All MIDI voices that are available!
#define __MIDI_NUMVOICES 64
//How many samples to buffer at once! 42 according to MIDI specs!
#define __MIDI_SAMPLES 84

//All statuses for MIDI voices!
#define MIDISTATUS_OFF 0x00
#define MIDISTATUS_ATTACK 0x01
#define MIDISTATUS_DECAY 0x02
#define MIDISTATUS_SUSTAIN 0x03
#define MIDISTATUS_RELEASE 0x04

//To log MIDI commands?
//#define MIDI_LOG
//To log MIDI rendering timing?
//#define LOG_MIDI_TIMING

//On/off controller values!
#define MIDI_CONTROLLER_ON 0x40
#define MIDI_CONTROLLER_OFF 0x00

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
	byte velocity; //What velocity/volume!
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
	byte active; //Are we active?

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
} MIDIDEVICE_VOICE;

MIDIDEVICE_VOICE activevoices[__MIDI_NUMVOICES]; //All active voices!

OPTINLINE static void MIDIDEVICE_execMIDI(MIDIPTR current); //MIDI device UART mode execution!

MIDICOMMAND *buffer, *last; //MIDI buffer and last item!

/* Buffer support */

void MIDIDEVICE_addbuffer(byte command, MIDIPTR data) //Add a command to the buffer!
{
	if (__HW_DISABLED) return; //We're disabled!
	/*MIDICOMMAND *currentcommand;
	currentcommand = (MIDICOMMAND *)zalloc(sizeof(MIDICOMMAND),"MIDI_COMMAND"); //Allocate a command!
	memcpy(&currentcommand->buffer,&data->buffer,sizeof(currentcommand->buffer)); //Copy params!
	currentcommand->command = command; //The command!
	if (!buffer) //First command?
	{
		buffer = currentcommand; //Load the first item!
	}
	else //Already something there?
	{
		last->next = (void *)currentcommand; //Load the last item new!
	}
	last = currentcommand; //Set the last item to the one added!
	*/
	data->command = command; //Set the command to use!
	MIDIDEVICE_execMIDI(data); //Execute directly!
}

OPTINLINE MIDICOMMAND *MIDIDEVICE_peekbuffer() //Peek at the buffer's first added item!
{
	if (__HW_DISABLED) return NULL; //We're disabled!
	return buffer; //Give the last item, if any!
}

OPTINLINE static int MIDIDEVICE_readbuffer(MIDIPTR result) //Read from the buffer!
{
	if (__HW_DISABLED) return 0; //We're disabled!
	if (buffer) //Gotten an item?
	{
		memcpy(result,buffer,sizeof(result)); //Read from the buffer!
		if (last==buffer) //Final?
		{
			last = NULL; //Unset!
		}
		MIDICOMMAND *old;
		old = buffer; //Load the buffer position!
		buffer = (MIDICOMMAND *)buffer->next; //Goto next item in the buffer!
		freez((void **)&old,sizeof(MIDICOMMAND),"MIDI_COMMAND"); //Release the memory taken by the command!
	}
	return 0; //Nothing to read!
}

void MIDIDEVICE_flushBuffers()
{
	if (__HW_DISABLED) return; //We're disabled!
	MIDICOMMAND current;
	for (;MIDIDEVICE_readbuffer(&current);){} //Flush the MIDI buffer!
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

/* Execution flow support */

OPTINLINE static byte MIDIDEVICE_FilterChannelVoice(byte selectedchannel, byte channel)
{
	//return (selectedchannel==channel); //Ignore Omni&Poly/Mono modes!
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

OPTINLINE static void MIDIDEVICE_noteOff(byte selectedchannel, byte channel, byte note, byte note32, byte note32_index, uint_32 note32_value)
{
	if (MIDIDEVICE_FilterChannelVoice(selectedchannel,channel)) //To be applied?
	{
		if ((MIDIDEVICE.channels[channel].playing[note32]&note32_value) || (MIDIDEVICE.channels[channel].request_on[note32]&note32_value)) //Are we playing or requested?
		{
			MIDIDEVICE.channels[channel].request_off[note32] |= note32_value; //Request finish!
		}
	}
}

OPTINLINE static void MIDIDEVICE_calc_notePosition(byte note, byte *note32, byte *note32_index, uint_32 *note32_value)
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

OPTINLINE static void MIDIDEVICE_AllNotesOff(byte selectedchannel, byte channel) //Used with command, mode change and Mono Mode.
{
	word noteoff; //Current note to turn off!
	//Note values
	byte note32, note32_index;
	uint_32 note32_value;
	lockaudio(); //Lock the audio!
	for (noteoff=0;noteoff<0x100;) //Process all notes!
	{
		MIDIDEVICE_calc_notePosition(noteoff,&note32,&note32_index,&note32_value); //Calculate our needs!
		MIDIDEVICE_noteOff(selectedchannel,channel,noteoff++,note32,note32_index,note32_value); //Execute Note Off!
	}
	unlockaudio(1); //Unlock the audio!
	#ifdef MIDI_LOG
	dolog("MPU","MIDIDEVICE: ALL NOTES OFF: %i",selectedchannel); //Log it!
	#endif
}

OPTINLINE static void MIDIDEVICE_noteOn(byte selectedchannel, byte channel, byte note, byte velocity, byte note32, byte note32_index, uint_32 note32_value)
{
	if (MIDIDEVICE_FilterChannelVoice(selectedchannel,channel)) //To be applied?
	{
		if (!(MIDIDEVICE.channels[channel].playing[note32]&note32_value)) //Not already playing?
		{
			if (!MIDIDEVICE.channels[channel].mode&MIDIDEVICE_POLY) //Mono mode?
			{
				MIDIDEVICE_AllNotesOff(selectedchannel,channel); //Turn all notes off first!
			}
			MIDIDEVICE.channels[channel].request_on[note32] |= note32_value; //Request start!
			MIDIDEVICE.channels[channel].notes[note].velocity = velocity; //Add velocity to our lookup!
		}
	}
}

OPTINLINE static void MIDIDEVICE_execMIDI(MIDIPTR current) //Execute the current MIDI command!
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
			lockaudio(); //Lock the audio!
			for (channel=0;channel<0x10;) //Process all channels!
			{
				MIDIDEVICE_noteOff(currentchannel,channel++,firstparam,note32,note32_index,note32_value); //Execute Note Off!
			}
			unlockaudio(1); //Unlock the audio!
			#ifdef MIDI_LOG
			dolog("MPU","MIDIDEVICE: NOTE OFF: %i-%i=%i:%i-%i",channel,firstparam,channel,note32,note32_index); //Log it!
			#endif
			break;
		case 0x90: //Note on?
			MIDIDEVICE_calc_notePosition(firstparam,&note32,&note32_index,&note32_value); //Calculate our needs!
			lockaudio(); //Lock the audio!
			for (channel=0;channel<0x10;) //Process all channels!
			{
				MIDIDEVICE_noteOn(currentchannel,channel++,firstparam,current->buffer[1],note32,note32_index,note32_value); //Execute Note Off!
			}
			unlockaudio(1); //Unlock the audio!
			#ifdef MIDI_LOG
			dolog("MPU","MIDIDEVICE: NOTE ON: %i-%i=%i:%i-%i",channel,firstparam,currentchannel,note32,note32_index); //Log it!
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
					lockaudio(); //Lock the audio!
					MIDIDEVICE.channels[currentchannel].bank &= 0x3F80; //Only keep MSB!
					MIDIDEVICE.channels[currentchannel].bank |= current->buffer[1]; //Set LSB!
					unlockaudio(1); //Unlock the audio!
					break;
				case 0x40: //Hold Pedal (On/Off) = Sustain Pedal
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
							MIDIDEVICE.channels[currentchannel].mode &= ~MIDIDEVICE_OMNI; //Disable Omni mode!
							break;
						case 1: //Omni Mode On
							MIDIDEVICE.channels[currentchannel].mode |= MIDIDEVICE_OMNI; //Enable Omni mode!
							break;
						case 2: //Mono operation
							MIDIDEVICE.channels[currentchannel].mode &= ~MIDIDEVICE_POLY; //Disable Poly mode!
							MIDIDEVICE.channels[currentchannel].mode &= ~MIDIDEVICE_OMNI; //Disable Omni mode!
							if (current->buffer[1]) //Omni Off+Ammount of channels to respond to?
							{
								rangemax = rangemin = currentchannel;
								rangemax += current->buffer[1]; //Maximum range!
								--rangemax;
							}
							else //Omni On?
							{
								MIDIDEVICE.channels[currentchannel].mode |= MIDIDEVICE_OMNI; //Enable Omni mode!
								rangemin = 0; //Respond to...
								rangemax = 0xF; //All channels!
							}
							MIDIDEVICE.channels[currentchannel].channelrangemin = rangemin;
							MIDIDEVICE.channels[currentchannel].channelrangemax = rangemax;
							break;
						case 3: //Poly Operation
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
			dolog("MPU","MIDIDEVICE: Control change: %i=%i",channel,firstparam); //Log it!
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

/* Basic playback support */

//Convert cents to samples to increase (instead of 1 sample/sample). Floating point number (between 0.0+ usually?) Use this as a counter for the current samples (1.1+1.1..., so keep the rest value (1,1,1,...,0,1,1,1,...))
OPTINLINE static double cents2samplesfactor(double cents)
{
	return pow(2,(cents/1200)); //Convert to samples (not entire numbers, so keep them counted)!
}

OPTINLINE static void MIDIDEVICE_getsample(sample_stereo_t *sample, MIDIDEVICE_VOICE *voice) //Get a sample from an MIDI note!
{
	//Our current rendering routine:
	register uint_32 temp;
	register uint_32 samplepos;
	sword lchannel, rchannel; //Both channels to use!
	byte loopflags;
	
	samplepos = voice->play_counter++; //Load the current play counter!
	if (!voice->active) --voice->play_counter; //Disable increasing the counter when inactive: keep the same position!
	samplepos *= voice->effectivesamplespeedup; //Affect speed through cents and other factors!

	samplepos += voice->startaddressoffset; //The start of the sample!
	
	//First: apply looping! We don't apply [bit 1=0] (Loop infinite until finished), because we have no ADSR envelope yet!
	loopflags = voice->currentloopflags;
	if ((loopflags&1) && (voice->active)) //Currently looping and active?
	{
		if (samplepos>=voice->endloopaddressoffset) //Past/at the end of the loop!
		{
			temp = voice->startloopaddressoffset; //The actual start of the loop!
			//Calculate loop size!
			samplepos -= temp; //Take the ammount past the start of the loop!
			samplepos %= voice->loopsize; //Loop past startloop by endloop!
			samplepos += temp; //The destination position within the loop!
			if ((loopflags&0xC0)==0x80) //Exit loop next round?
			{
				voice->currentloopflags = 0; //Disable loop flags: we're not looping anymore!
				if (loopflags&2) //Loop infinite until finished?
				{
					voice->active = 0; //Terminate voice: since we don't support infinite loop until finish, we terminate the entire voice when used.
				}
				else
				{
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

	//Next, apply finish!
	if ((samplepos>=voice->endaddressoffset) || (!voice->active)) //Sound is finished?
	{
		sample->l = sample->r = 0; //No sample!
		voice->active = !voice->channel->sustain; //Disable our voice when not sustaining anymore!
		return; //Done!
	}

	if (getSFsample(soundfont,samplepos,&lchannel)) //Sample found?
	{
		rchannel = lchannel; //Load the sample to be used into both the left and right channels!
		//Now, apply panning!
		lchannel *= voice->lvolume; //Apply left panning!
		rchannel *= voice->rvolume; //Apply right panning!
		
		//Give the result!
		sample->l = lchannel; //LChannel!
		sample->r = rchannel; //RChannel!
		return; //Done!
	}

	sample->l = sample->r = 0; //No sample to be found!
}

byte MIDIDEVICE_renderer(void* buf, uint_32 length, byte stereo, void *userdata) //Sound output renderer!
{
	//Initialisation info
	byte currentchannel, currenton, biton;
	word pbag, ibag;
	sword rootMIDITone; //Relative root MIDI tone!
	uint_32 requestbit, preset, therequeston, notenumber, startaddressoffset, endaddressoffset, startloopaddressoffset, endloopaddressoffset, loopsize;
	float cents, tonecents, lvolume, rvolume, currentsamplespeedup, pitchcents, panningtemp;
	MIDIDEVICE_CHANNEL *channel;
	MIDIDEVICE_NOTE *note;
	sfPresetHeader currentpreset;
	sfGenList instrumentptr;
	sfInst currentinstrument;
	sfInstGenList sampleptr, applygen;
	MIDIDEVICE_VOICE *voice;

	if (__HW_DISABLED) return 0; //We're disabled!

	#ifdef LOG_MIDI_TIMING
	static TicksHolder ticks; //Our ticks holder!
	startHiresCounting(&ticks);
	ticksholder_AVG(&ticks); //Enable averaging!
	#endif

	voice = (MIDIDEVICE_VOICE *)userdata; //Convert to our voice!
	if (!voice->active) //Inactive voice?
	{
		//Check for requested voices!
		//First, all our variables!
		for (currentchannel=0;currentchannel<0x40;) //Process all channels (10 channels, 4 dwords/channel)!
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
		return 0; //Abort: we're an inactive voice!

		handlerequest: //Handles an NOTE ON request!
		currentchannel = biton; //The specified channel!
		//currentchannel=the channel; currenton=the request dword
		for (biton=0;biton<32;)
		{
			if (therequeston&1) break; //Stop searching when found!
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
	
		if (!lookupPresetByInstrument(soundfont,channel->program,channel->bank,&preset)) //Preset not found?
		{
			return 0; //No samples!
		}
	
		if (!getSFPreset(soundfont,preset,&currentpreset))
		{
			return 0;
		}
		
		if (!lookupPBagByMIDIKey(soundfont,preset,note->note,note->velocity,&pbag)) //Preset bag not found?
		{
			return 0; //No samples!
		}
		
		if (!lookupSFPresetGen(soundfont,preset,pbag,GEN_INSTRUMENT,&instrumentptr))
		{
			return 0; //No samples!
		}
		
		if (!getSFInstrument(soundfont,instrumentptr.genAmount.wAmount,&currentinstrument))
		{
			return 0;
		}
	
		if (!lookupIBagByMIDIKey(soundfont,instrumentptr.genAmount.wAmount,note->note,note->velocity,&ibag,1))
		{
			return 0; //No samples!
		}
		
		if (!lookupSFInstrumentGen(soundfont,instrumentptr.genAmount.wAmount,ibag,GEN_SAMPLEID,&sampleptr))
		{
			return 0; //No samples!
		}
		
		if (!getSFSampleInformation(soundfont,sampleptr.genAmount.wAmount,&voice->sample))
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
		if (lookupSFInstrumentGenGlobal(soundfont,instrumentptr.genAmount.wAmount,ibag,GEN_STARTADDRESSOFFSET,&applygen))
		{
			startaddressoffset += applygen.genAmount.shAmount; //Apply!
		}
		if (lookupSFInstrumentGenGlobal(soundfont,instrumentptr.genAmount.wAmount,ibag,GEN_STARTADDRESSCOARSEOFFSET,&applygen))
		{
			startaddressoffset += (applygen.genAmount.shAmount<<15); //Apply!
		}
	
		if (lookupSFInstrumentGenGlobal(soundfont,instrumentptr.genAmount.wAmount,ibag,GEN_ENDADDRESSOFFSET,&applygen))
		{
			endaddressoffset += applygen.genAmount.shAmount; //Apply!
		}
		if (lookupSFInstrumentGenGlobal(soundfont,instrumentptr.genAmount.wAmount,ibag,GEN_ENDADDRESSCOARSEOFFSET,&applygen))
		{
			endaddressoffset += (applygen.genAmount.shAmount<<15); //Apply!
		}
	
		if (lookupSFInstrumentGenGlobal(soundfont,instrumentptr.genAmount.wAmount,ibag,GEN_STARTLOOPADDRESSOFFSET,&applygen))
		{
			startloopaddressoffset += applygen.genAmount.shAmount; //Apply!
		}
		if (lookupSFInstrumentGenGlobal(soundfont,instrumentptr.genAmount.wAmount,ibag,GEN_STARTLOOPADDRESSCOARSEOFFSET,&applygen))
		{
			startloopaddressoffset += (applygen.genAmount.shAmount<<15); //Apply!
		}
	
		if (lookupSFInstrumentGenGlobal(soundfont,instrumentptr.genAmount.wAmount,ibag,GEN_ENDLOOPADDRESSOFFSET,&applygen))
		{
			endloopaddressoffset += applygen.genAmount.shAmount; //Apply!
		}
		if (lookupSFInstrumentGenGlobal(soundfont,instrumentptr.genAmount.wAmount,ibag,GEN_ENDLOOPADDRESSCOARSEOFFSET,&applygen))
		{
			endloopaddressoffset += (applygen.genAmount.shAmount<<15); //Apply!
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
		if (lookupSFInstrumentGenGlobal(soundfont,instrumentptr.genAmount.wAmount,ibag,GEN_OVERRIDINGROOTKEY,&applygen))
		{
			rootMIDITone = applygen.genAmount.wAmount; //The MIDI tone to apply is different!
		}
		else
		{
			rootMIDITone = voice->sample.byOriginalPitch; //Original MIDI tone!
		}

		rootMIDITone -= note->note; //>=positive difference, <=negative difference.
		//Ammount of MIDI notes too high is in rootMIDITone.

		//Coarse tune...
		if (lookupSFInstrumentGenGlobal(soundfont,instrumentptr.genAmount.wAmount,ibag,GEN_COARSETUNE,&applygen))
		{
			cents = (float)applygen.genAmount.shAmount; //How many semitones!
			cents *= 100.0f; //Apply to the cents: 1 semitone = 100 cents!
		}

		//Fine tune...
		if (lookupSFInstrumentGenGlobal(soundfont,instrumentptr.genAmount.wAmount,ibag,GEN_FINETUNE,&applygen))
		{
			cents += (float)applygen.genAmount.shAmount; //Add the ammount of cents!
		}

		//Scale tuning: how the MIDI number affects semitone (percentage of semitones)
		tonecents = 100.0f; //Default: 100 cents(%) scale tuning!
		if (lookupSFInstrumentGenGlobal(soundfont,instrumentptr.genAmount.wAmount,ibag,GEN_SCALETUNING,&applygen))
		{
			tonecents = (float)applygen.genAmount.shAmount; //Apply semitone factor in percent for each tone!
		}
		
		tonecents *= -((float)rootMIDITone); //Difference in tones we use is applied to the ammount of cents reversed (the more negative, the)!
		
		cents += tonecents; //Apply the MIDI tone cents for the MIDI tone!

		//Now the cents variable contains the diviation in cents.
		voice->initsamplespeedup = cents2samplesfactor(cents); //Load the default speedup we need for our tone!

		//Determine panning!
		lvolume = rvolume = 0.5f; //Default to 50% each (center)!
		if (lookupSFInstrumentGenGlobal(soundfont,instrumentptr.genAmount.wAmount,ibag,GEN_PAN,&applygen)) //Gotten panning?
		{
			panningtemp = (float)applygen.genAmount.shAmount; //Get the panning specified!
			panningtemp *= 0.01f; //Make into a percentage!
			lvolume -= panningtemp; //Left percentage!
			rvolume += panningtemp; //Right percentage!
		}
		voice->lvolume = lvolume; //Left panning!
		voice->rvolume = rvolume; //Right panning!

		
		//Apply loop flags!
		voice->currentloopflags = 0; //Default: no looping!
		if (lookupSFInstrumentGenGlobal(soundfont,instrumentptr.genAmount.wAmount,ibag,GEN_SAMPLEMODES,&applygen)) //Gotten looping?
		{
			switch (applygen.genAmount.wAmount) //What loop?
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
		setSampleRate(&MIDIDEVICE_renderer,userdata,voice->sample.dwSampleRate); //Use this new samplerate!
		channel->playing[currenton] |= requestbit; //Playing flag!
		voice->active = 1; //We're an active voice!
	}

	//Calculate the pitch bend speedup!
	pitchcents = (double)voice->channel->pitch; //Load active pitch bend!
	pitchcents /= 40.96f; //Pitch bend in cents!

	//Now apply to the default speedup!
	currentsamplespeedup = voice->initsamplespeedup; //Load the default sample speedup for our tone!
	currentsamplespeedup *= cents2samplesfactor(pitchcents); //Calculate the sample speedup!; //Apply pitch bend!
	voice->effectivesamplespeedup = currentsamplespeedup; //Load the speedup of the samples we need!

	if (voice->channel->request_off[voice->requestnumber]&voice->requestbit) //Requested turn off?
	{
		voice->currentloopflags |= 0x80; //Request quit looping if needed: finish sound!
		voice->currentloopflags &= ~0x40; //Sustain disabled by default!
		voice->currentloopflags |= (voice->channel->sustain<<6); //Sustaining?
	} //Requested off?
	
	//Now produce the sound itself!
	sample_stereo_t* ubuf = (sample_stereo_t *)buf; //Our sample buffer!
	sample_stereo_t* ubufend;
	ubufend = &ubuf[length]; //Final sample!
	for (;;) //Produce the samples!
	{
		MIDIDEVICE_getsample(ubuf++,voice); //Get the sample from the MIDI device!
		if (ubuf==ubufend) break; //Final sample finished!
	}

	#ifdef LOG_MIDI_TIMING
	stopHiresCounting("MIDIDEV","MIDIRenderer",&ticks); //Log our active counting!
	#endif

	if (!voice->active) //Inactive voice?
	{
		//Get our data concerning the release!
		currenton = voice->requestnumber;
		requestbit = voice->requestbit;
		channel = voice->channel; //Current channel!

		channel->request_off[currenton] &= ~requestbit; //Turn the KEY OFF request off, if any!
		channel->playing[currenton] &= ~requestbit; //Turn the PLAYING flag off: we're not playing anymore!
	}

	return SOUNDHANDLER_RESULT_FILLED; //We're filled!
}

/* Init/destroy support */

void done_MIDIDEVICE() //Finish our midi device!
{
	if (__HW_DISABLED) return; //We're disabled!
	lockaudio();
	//Close the soundfont?
	closeSF(&soundfont);
	int i;
	for (i=0;i<NUMITEMS(activevoices);i++) //Assign all voices available!
	{
		removechannel(&MIDIDEVICE_renderer,&activevoices[i],0); //Remove the channel! Delay at 0.96ms for response speed!
	}
	unlockaudio(1);
}

void init_MIDIDEVICE() //Initialise MIDI device for usage!
{
	if (__HW_DISABLED) return; //We're disabled!
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