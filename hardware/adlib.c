#include "headers/types.h" //Basic headers!
#include "headers/emu/sound.h" //Basic sound!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/ports.h" //Basic ports!
#include "headers/support/highrestimer.h" //High resoltion timer support!
#include "headers/emu/emu_misc.h" //Random short support for noise generation!
#include "headers/emu/timers.h" //Timer support for attack/decay!
#include "headers/support/locks.h" //Locking support!

#include "headers/support/highrestimer.h" //High resolution timer!
#include "headers/support/sounddoublebuffer.h" //FIFO buffer support!

#include "headers/support/wave.h" //WAV file support!

#define uint8_t byte
#define uint16_t word

//Are we disabled?
#define __HW_DISABLED 0
//Use the adlib sound? If disabled, only run timers for the CPU. Sound will not actually be heard.
#define __SOUND_ADLIB 1
//What volume, in percent!
#define ADLIB_VOLUME 100.0f
//What volume is the minimum volume to be heard!
#define __MIN_VOL (1.0f / SHRT_MAX)
//Generate WAV file output?
//#define WAV_ADLIB
//Generate WAV file output of a single sine wave?
//#define WAVE_ADLIB
//Adlib low-pass filter if enabled!
//#define ADLIB_LOWPASS 15392.0f
//Enable Tremolo&Vibrato if enabled!
#define ADLIB_TREMOLOVIBRATO

//How large is our sample buffer? 1=Real time, 0=Automatically determine by hardware
#define __ADLIB_SAMPLEBUFFERSIZE 4971

#define PI2 ((float)(2.0f * PI))

//Silence value?
#define Silence 0x3F

//Convert cents to samples to increase (instead of 1 sample/sample). Floating point number (between 0.0+ usually?) Use this as a counter for the current samples (1.1+1.1..., so keep the rest value (1,1,1,...,0,1,1,1,...))
//The same applies to absolute and relative timecents (with absolute referring to 1 second intervals (framerate samples) and relative to the absolute value)
#define cents2samplesfactor(cents) pow(2, ((cents) / 1200))
//Convert to samples (not entire numbers, so keep them counted)!

//extern void set_port_write_redirector (uint16_t startport, uint16_t endport, void *callback);
//extern void set_port_read_redirector (uint16_t startport, uint16_t endport, void *callback);

uint16_t baseport = 0x388; //Adlib address(w)/status(r) port, +1=Data port (write only)

word max_input; //Maximum input as a raw signal!

//Sample based information!
const float usesamplerate = 14318180.0f/288.0f; //The sample rate to use for output!
const float adlib_soundtick = 1000000000.0f/(14318180.0f/288.0f); //The length of a sample in ns!
//The length of a sample step:
#define adlib_sampleLength (1.0f / (14318180.0f / 288.0f))

float generalmodulatorfactor = 4084.0f / 1024.0f; //Modulation factor!

//Counter info
float counter80 = 0.0f, counter320 = 0.0f; //Counter ticks!
byte timer80=0, timer320=0; //Timer variables for current timer ticks!

//Registers itself
byte adlibregmem[0xFF], adlibaddr = 0;

word OPL2_ExpTable[0x100], OPL2_LogSinTable[0x100]; //The OPL2 Exponentional and Log-Sin tables!

byte adliboperators[2][0x10] = { //Groupings of 22 registers! (20,40,60,80,E0)
	{ 0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12,255,255,255,255,255,255 },
	{ 0x03, 0x04, 0x05, 0x0B, 0x0C, 0x0D, 0x13, 0x14, 0x15,255,255,255,255,255,255 }
};

byte adliboperatorsreverse[0x20] = { 0, 1, 2, 0, 1, 2, 255, 255, 3, 4, 5, 3, 4, 5, 255, 255, 6, 7, 8, 6, 7, 8,255,255,255,255,255,255,255,255,255,255}; //Channel lookup of adlib operators!
byte adliboperatorsreversekeyon[0x20] = { 1, 1, 1, 2, 2, 2, 255, 255, 1, 1, 1, 2, 2, 2, 255, 255, 1, 1, 1, 2, 2, 2,0,0,0,0,0,0,0,0,0,0}; //Modulator/carrier lookup of adlib operators in the keyon bits!

static const float feedbacklookup[8] = { 0, PI / 16.0, PI / 8.0, PI / 4.0, PI / 2.0, PI, PI*2.0, PI*4.0 }; //The feedback to use from opl3emu! Seems to be half a sinus wave per number!
float feedbacklookup2[8]; //Actual feedback lookup value!

byte wavemask = 0; //Wave select mask!

SOUNDDOUBLEBUFFER adlib_soundbuffer; //Our sound buffer for rendering!

#ifdef WAV_ADLIB
WAVEFILE *adlibout = NULL;
#endif

typedef struct
{
	word m_fnum, m_block; //Our settings!
	float convfreq,effectivefreq;
	uint8_t keyon;
	uint8_t synthmode; //What synthesizer mode (1=Additive synthesis, 0=Frequency modulation)
	float feedback; //The feedback strength of the modulator signal.
} ADLIBCHANNEL; //A channel!

typedef struct {
	//Effects
	word outputlevel; //(RAW) output level!
	byte volenv; //(RAW) volume level!
	byte m_ar, m_dr, m_sl, m_rr; //Four rates and levels!
	word m_counter; //Counter for the volume envelope!
	word m_env;
	word m_ksl, m_kslAdd, m_ksr; //Various key setttings regarding pitch&envelope!
	byte ReleaseImmediately; //Release even when the note is still turned on?

	//Volume envelope
	uint8_t volenvstatus; //Envelope status and raw volume envelope value(0-64)
	word gain; //The gain gotten from the volume envelopes!
	word rawgain; //The gain without main volume control!

	byte vibrato, tremolo; //Vibrato/tremolo setting for this channel. 1=Enabled, 0=Disabled.

	//Signal generation
	byte wavesel;
	float ModulatorFrequencyMultiple; //What harmonic to sound?
	float lastsignal; //The last signal produced!
	float freq0, time; //The frequency and current time of an operator!
	float lastfreq; //Last valid set frequency!
	ADLIBCHANNEL *channel;
} ADLIBOP; //An adlib operator to process!

ADLIBOP adlibop[0x20];

ADLIBCHANNEL adlibch[0x10];

word outputtable[0x40]; //Build using software formulas!

uint8_t adlibpercussion = 0, adlibstatus = 0;

uint_32 OPL2_RNGREG = 0;
uint_32 OPL2_RNG = 0; //The current random generated sample!

uint16_t adlibport = 0x388;


//Tremolo/vibrato support

typedef struct
{
	float time; //Current time(loops every second)!
	float current; //Current value in absolute data!
	float depth; //The depth to apply!
	float active; //Active value, depending on tremolo/vibrato what this is: tremolo: volume to apply. vibrato: speedup to apply.
} TREMOLOVIBRATOSIGNAL; //Tremolo&vibrato signals!

TREMOLOVIBRATOSIGNAL tremolovibrato[2]; //Tremolo&vibrato!

OPTINLINE void stepTremoloVibrato(TREMOLOVIBRATOSIGNAL *signal, float frequency)
{
	float current;
	current = sinf(2*PI*frequency*signal->time); //Apply the signal!
	current += 1.0f; //Make positive (0.0-2.0)
	current *= 0.5; //Divide by 2 to get the applied range (0-1)
	signal->current = current; //Save the current signal!
	float temp;
	double d;
	signal->time += adlib_sampleLength; //Add 1 sample to the time!

	temp = signal->time*frequency; //Calculate for overflow!
	if (temp >= 1.0f) { //Overflow?
		signal->time = (float)modf(temp, &d) / frequency;
	}
}

OPTINLINE void OPL2_stepTremoloVibrato()
{
	//Step to the next value!
	stepTremoloVibrato(&tremolovibrato[0],3.7f); //Tremolo at 3.7Hz!
	stepTremoloVibrato(&tremolovibrato[1],6.4f); //Vibrato at 6.4Hz!

	//Now the current value of the signal is stored! Apply the active tremolo/vibrato!
	#ifdef ADLIB_TREMOLOVIBRATO
	tremolovibrato[0].active = dB2factor(47.25f-(tremolovibrato[0].depth*tremolovibrato[0].current),47.25f); //Calculate the current tremolo!
	tremolovibrato[1].active = (100.0f+(tremolovibrato[1].depth*tremolovibrato[1].current))*0.01f; //Calculate the current vibrato!
	#else
	tremolovibrato[0].active = tremolovibrato[1].active = 1.0f; //No tremolo/vibrato!
	#endif
}

OPTINLINE float OPL2_Vibrato(float frequency, sbyte operatornumber)
{
	if (adlibop[operatornumber].vibrato) //Vibrato enabled?
	{
		return frequency*tremolovibrato[1].active; //Apply vibrato!
	}
	return frequency; //Unchanged frequency!
}

OPTINLINE float OPL2_Tremolo(byte operator, float f)
{
	if (adlibop[operator].tremolo) //Tremolo enabled?
	{
		return f*tremolovibrato[0].active; //Apply the current tremolo/vibrato!
	}
	return f; //Unchanged!
}

//RNG

OPTINLINE void OPL2_stepRNG() //Runs at the sampling rate!
{
	OPL2_RNG = ( (OPL2_RNGREG) ^ (OPL2_RNGREG>>14) ^ (OPL2_RNGREG>>15) ^ (OPL2_RNGREG>>22) ) & 1; //Get the current RNG!
	OPL2_RNGREG = (OPL2_RNG<<22) | (OPL2_RNGREG>>1);
}

OPTINLINE float calcModulatorFrequencyMultiple(byte data)
{
	switch (data)
	{
	case 0: return 0.5f;
	case 11: return 10.0f;
	case 13: return 12.0f;
	case 14: return 15.0f;
	default: return (float)data; //The same number!
	}
}

//Attenuation setting!
void EnvelopeGenerator_setAttennuation(ADLIBOP *operator); //Prototype!

OPTINLINE float adlibeffectivefrequency(word fnum, word octave)
{
	return (fnum * usesamplerate) / (float)(1<<(20-octave)); //This is the frequency requested!
}

void writeadlibKeyON(byte channel, byte forcekeyon)
{
	byte keyon;
	byte oldkeyon;
	oldkeyon = adlibch[channel].keyon; //Current&old key on!
	keyon = ((adlibregmem[0xB0 + (channel&0xF)] >> 5) & 1)?3:0; //New key on for melodic channels? Affect both operators! This disturbs percussion mode!
	if (adlibpercussion && (channel&0x80)) //Percussion enabled and percussion channel changed?
	{
		keyon = adlibregmem[0xBD]; //Total key status for percussion channels?
		switch (channel&0xF) //What channel?
		{
			//Adjusted according to http://www.4front-tech.com/dmguide/dmfm.html
			case 6: //Bass drum? Uses the channel normally!
				keyon = (keyon&0x10)?3:0; //Bass drum on? Key on/off on both operators!
				channel = 6; //Use channel 6!
				break;
			case 7: //Snare drum(Modulator)/Hi-hat(Carrier)? fmopl.c: High-hat uses modulator, Snare drum uses Carrier signals.
				keyon = ((keyon>>3)&1)|((keyon<<1)&2); //Shift the information to modulator and carrier positions!
				channel = 7; //Use channel 7!
				break;
			case 8: //Tom-tom(Carrier)/Cymbal(Modulator)? fmopl.c:Tom-tom uses Modulator, Cymbal uses Carrier signals.
				keyon = adlibregmem[0xBD]; //Cymbal/hi-hat on? Use the full register for processing!
				keyon = ((keyon>>2)&1)|(keyon&2); //Shift the information to modulator and carrier positions!
				channel = 8; //Use channel 8!
				break;
			default: //Unknown channel?
				//New key on for melodic channels? Don't change anything!
				break;
		}
	}

	adlibch[channel].m_block = (adlibregmem[0xB0 + channel] >> 2) & 7;
	adlibch[channel].m_fnum = (adlibregmem[0xA0 + channel] | ((adlibregmem[0xB0 + channel] & 3) << 8)); //Frequency number!
	adlibch[channel].convfreq = ((double)adlibch[channel].m_fnum * 0.7626459);
	adlibch[channel].effectivefreq = adlibeffectivefrequency(adlibch[channel].m_fnum,adlibch[channel].m_block); //Calculate the effective frequency!

	if ((adliboperators[0][channel]!=0xFF) && (((keyon&1) && ((oldkeyon^keyon)&1)) || (forcekeyon&1))) //Key ON on operator #1?
	{
		adlibop[adliboperators[0][channel]&0x1F].volenvstatus = 1; //Start attacking!
		adlibop[adliboperators[0][channel]&0x1F].volenv = Silence; //No raw level: Start silence!
		adlibop[adliboperators[0][channel]&0x1F].m_env = Silence; //No raw level: Start level!
		adlibop[adliboperators[0][channel]&0x1F].gain = ((Silence-adlibop[adliboperators[0][channel]].volenv)<<3); //Apply the start gain!
		adlibop[adliboperators[0][channel]&0x1F].m_counter = 0; //No raw level: Start counter!
		adlibop[adliboperators[0][channel]&0x1F].freq0 = adlibop[adliboperators[0][channel]&0x1F].time = 0.0f; //Initialise operator signal!
		adlibop[adliboperators[0][channel]&0x1F].lastsignal = 0.0f; //Reset the last signals!
		EnvelopeGenerator_setAttennuation(&adlibop[adliboperators[0][channel]&0x1F]);
	}

	if ((adliboperators[1][channel]!=0xFF) && (((keyon&2) && ((oldkeyon^keyon)&2)) || (forcekeyon&2))) //Key ON on operator #2?
	{
		adlibop[adliboperators[1][channel]&0x1F].volenvstatus = 1; //Start attacking!
		adlibop[adliboperators[1][channel]&0x1F].volenv = Silence; //No raw level: silence!
		adlibop[adliboperators[1][channel]&0x1F].m_env = Silence; //No raw level: Start level!
		adlibop[adliboperators[1][channel]&0x1F].gain = ((Silence-adlibop[adliboperators[1][channel]].volenv)<<3); //Apply the start gain!
		adlibop[adliboperators[1][channel]&0x1F].m_counter = 0; //No raw level: Start counter!
		adlibop[adliboperators[1][channel]&0x1F].freq0 = adlibop[adliboperators[1][channel]&0x1F].time = 0.0f; //Initialise operator signal!
		adlibop[adliboperators[1][channel]&0x1F].lastsignal = 0.0f; //Reset the last signals!
		EnvelopeGenerator_setAttennuation(&adlibop[adliboperators[1][channel]&0x1F]);
	}

	//Update keyon information!
	adlibch[channel].keyon = keyon | forcekeyon; //Key is turned on?
}

byte outadlib (uint16_t portnum, uint8_t value) {
	byte oldval;
	if (portnum==adlibport) {
		adlibaddr = value;
		return 1;
		}
	if (portnum != (adlibport+1)) return 0; //Don't handle what's not ours!
	portnum = adlibaddr;
	oldval = adlibregmem[portnum]; //Save the old value for reference!
	if (portnum!=4) adlibregmem[portnum] = value; //Timer control applies it itself, depending on the value!
	switch (portnum & 0xF0) //What block to handle?
	{
	case 0x00:
		switch (portnum) //What primary port?
		{
		case 1: //Waveform select enable
			wavemask = (adlibregmem[0] & 0x20) ? 3 : 0; //Apply waveform mask!
			break;
		case 4: //timer control
			if (value & 0x80) { //Special case: don't apply the value!
				adlibstatus &= 0x1F; //Reset status flags needed!
			}
			else //Apply value to register?
			{
				adlibregmem[portnum] = value; //Apply the value set!
				if (value & 1) //Timer1 enabled?
				{
					timer80 = adlibregmem[2]; //Reload timer!
				}
				if (value & 2) //Timer2 enabled?
				{
					timer320 = adlibregmem[3]; //Reload timer!					
				}
			}
			break;
		default: //Unknown?
			break;
		}
	case 0x10: //Unused?
		break;
	case 0x20:
	case 0x30:
		if (portnum <= 0x35) //Various flags
		{
			portnum &= 0x1F;
			adlibop[portnum].ModulatorFrequencyMultiple = calcModulatorFrequencyMultiple(value & 0xF); //Which harmonic to use?
			adlibop[portnum].ReleaseImmediately = (value & 0x20) ? 0 : 1; //Release when not sustain until release!
			adlibop[portnum].m_ksr = (value>>4)&1; //Keyboard scaling rate!
			EnvelopeGenerator_setAttennuation(&adlibop[portnum]); //Apply attenuation settings!			
		}
		break;
	case 0x40:
	case 0x50:
		if (portnum <= 0x55) //KSL/Output level
		{
			portnum &= 0x1F;
			adlibop[portnum].m_ksl = ((value >> 6) & 3); //Apply KSL!
			adlibop[portnum].outputlevel = outputtable[value]; //Apply raw output level!
			EnvelopeGenerator_setAttennuation(&adlibop[portnum]); //Apply attenuation settings!
		}
		break;
	case 0x60:
	case 0x70:
		if (portnum <= 0x75) { //attack/decay
			portnum &= 0x1F;
			adlibop[portnum].m_ar = (value>>4); //Attack rate
			adlibop[portnum].m_dr = (value&0xF); //Decay rate
		}
		break;
	case 0x80:
	case 0x90:
		if (portnum <= 0x95) //sustain/release
		{
			portnum &= 0x1F;
			adlibop[portnum].m_sl = (value>>4); //Sustain level
			adlibop[portnum].m_rr = (value&0xF); //Release rate
		}
		break;
	case 0xA0:
	case 0xB0:
		if (portnum <= 0xB8)
		{ //octave, freq, key on
			if ((portnum & 0xF) > 8) goto unsupporteditem; //Ignore A9-AF!
			portnum &= 0xF; //Only take the lower nibble (the channel)!
			writeadlibKeyON((byte)portnum,0); //Write to this port! Don't force the key on!
		}
		else if (portnum == 0xBD) //Percussion settings etc.
		{
			adlibpercussion = (value & 0x20)?1:0; //Percussion enabled?
			tremolovibrato[0].depth = (value&0x80)?4.8f:1.0f; //Default: 1dB AM depth, else 4.8dB!
			tremolovibrato[1].depth = (value&0x40)?14.0f:7.0f; //Default: 7 cent vibrato depth, else 14 cents!
			if (((oldval^value)&0x1F) && adlibpercussion) //Percussion enabled and changed state?
			{
				writeadlibKeyON(0x86,0); //Write to this port(Bass drum)! Don't force the key on!
				writeadlibKeyON(0x87,0); //Write to this port(Snare drum/Tom-tom)! Don't force the key on!
				writeadlibKeyON(0x88,0); //Write to this port(Cymbal/Hi-hat)! Don't force the key on!
			}
		}
		break;
	case 0xC0:
		if (portnum <= 0xC8)
		{
			portnum &= 0xF;
			adlibch[portnum].synthmode = (adlibregmem[0xC0 + portnum] & 1); //Save the synthesis mode!
			byte feedback;
			feedback = (adlibregmem[0xC0 + portnum] >> 1) & 7; //Get the feedback value used!
			adlibch[portnum].feedback = (float)feedbacklookup2[feedback]; //Convert to a feedback of the modulator signal!
		}
		break;
	case 0xE0:
	case 0xF0:
		if (portnum <= 0xF5) //waveform select
		{
			portnum &= 0x1F;
			adlibop[portnum].wavesel = value & 3;
		}
		break;
	default: //Unsupported port?
		break;
	}
	unsupporteditem:
	return 1; //We're finished and handled, even non-used registers!
}

uint8_t inadlib (uint16_t portnum, byte *result) {
	if (portnum == adlibport) //Status port?
	{
		*result = adlibstatus; //Give the current status!
		return 1; //We're handled!
	}
	return 0; //Not our port!
}

OPTINLINE float adlibfreq (sbyte operatornumber, uint8_t chan) {
	if (chan > 8) return (0); //Invalid channel: we only have 9 channels!
	float tmpfreq;
	tmpfreq = adlibch[chan].effectivefreq; //Effective frequency!
	if (operatornumber != -1) //Apply frequency multiplication factor and other things concerning this?
	{	
		tmpfreq *= adlibop[operatornumber].ModulatorFrequencyMultiple; //Apply the frequency multiplication factor!
		tmpfreq = OPL2_Vibrato(tmpfreq,operatornumber); //Apply vibrato!
	}
	return (tmpfreq);
}

//Sign bit, disable mask and extension to 16-bit value!
#define SIGNBIT 0x8000
#define SIGNMASK 0x7FFF

OPTINLINE sword Wave2Word(word w)
{
	if (w&SIGNBIT) //Negative?
	{
		w &= SIGNMASK; //Only past the sign!
		return -1 * (sword)w; //Give the converted value!
	}
	else //Positive?
	{
		w &= SIGNMASK; //Only past the sign!
		return (sword)w; //Give the converted value!
	}
}

OPTINLINE word Word2Wave(sword s)
{
	word w;
	if (s<0) //Negative?
	{
		s = -s; //Reverse the sign for conversion!
		w = (word)s; //Convert it to unsigned!
		w &= SIGNMASK; //Apply the sign mask!
		w |= SIGNBIT; //Apply the sign bit!
		return w; //Give the converted value!
	}
	else //Positive?
	{
		w = (word)s; //Convert it to unsigned!
		w &= SIGNMASK; //Apply the sign mask!
		return w; //Give the converted value!
	}
}

OPTINLINE word OPL2SinWave(const float r)
{
	const float halfpi = (0.5f*(float)PI); //Half PI!
	const float halfpi1 = (1.0f/halfpi); //Half pi division factor!
	float index;
	word entry; //The entry to convert!
	byte location; //The location in the table to use!
	byte PIpart;
	PIpart = 0; //Default: part 0!
	index = fmod(r,PI2); //Loop the sinus infinitely!
	if (index>=(float)PI) //Second half?
	{
		PIpart = 2; //Second half!
		index -= PI; //Convert to first half!
	}
	if (index>=halfpi) //Past quarter?
	{
		PIpart |= 1; //Second half!
		index -= halfpi; //Convert to first quarter!
	}
	index = (index*halfpi1)*255.0f; //Convert to full range!
	location = (byte)index; //Set the location to use!
	if (PIpart&1) //Reversed quarter(second and fourth quarter)?
	{
		location = ~location; //Reverse us!
	}

	entry = OPL2_LogSinTable[location]; //Take the full load!
	entry = OPL2_LogSinTable[0]-entry; //Reverse the sinus to make it go from 0 to max!
	if (PIpart&2) //Second half is negative?
	{
		entry |= SIGNBIT; //Reverse the negative part!
	}
	return entry; //Give the processed entry!
}

float expfactor = 1.0f;

OPTINLINE float getHiddenBit(word v)
{
	return (v&SIGNBIT)?-1.0f:1.0f; //Significant!
}

OPTINLINE word removeHiddenBit(word v)
{
	return (v&SIGNMASK); //No significant!
}

OPTINLINE float OPL2_Exponential(word v)
{
	//Exponential lookup also reverses the input, since it's a -logSin table!
	//Exponent = x/256
	//Significant = ExpTable[v%256]+1024
	//Output = Significant * (2^Exponent)
	return getHiddenBit(v)*((float)OPL2_ExpTable[v&0xFF]+1024.0f)*powf(2,(removeHiddenBit(v)>>8)); //Lookup normally!
}

OPTINLINE word OPL2_Sin(byte signal, const float frequencytime) {
	double x;
	float t;
	word result;
	switch (signal) {
	case 0: //SINE?
		return OPL2SinWave(frequencytime); //The sinus function!
	default:
		t = (float)modf(frequencytime/PI2, &x); //Calculate rest for special signal information!
		switch (signal) { //What special signal?
		case 1: // Negative=0?
			if (t >= 0.5f) return 0; //Negative!
			result = OPL2SinWave(frequencytime); //The sinus function!
			return result; //Positive!
		case 3: // Absolute with second half=0?
			if (fmod(t, 0.5f) >= 0.25) return 0; //Are we the second half of the half period? Clear the signal if so!
		case 2: // Absolute?
			result = OPL2SinWave(frequencytime); //The sinus function!
			if (result&SIGNBIT) result &= ~SIGNBIT; //Make positive!
			return result; //Simply absolute!
		default: //Unknown signal?
			return 0;
		}
	}
}

OPTINLINE word calcOPL2Signal(byte wave, float phase, float frequency, float *freq0, float *time) //Calculates a signal for input to the adlib synth!
{
	float ftp;
	if (frequency != *freq0) { //Frequency changed?
		*time *= (*freq0 / frequency);
	}

	ftp = frequency; //Frequency!
	ftp *= *time; //Time!
	ftp *= PI2; //Apply frequencytime ratio!
	ftp += phase; //Add phase!
	*freq0 = frequency; //Update new frequency!
	return OPL2_Sin(wave, ftp); //Give the generated sample!
}

OPTINLINE void incop(byte operator, float frequency)
{
	float temp;
	double d;
	adlibop[operator].time += adlib_sampleLength; //Add 1 sample to the time!

	temp = adlibop[operator].time*frequency; //Calculate for overflow!
	if (temp >= 1.0f) { //Overflow?
		adlibop[operator].time = (float)modf(temp, &d) / frequency;
	}
}

OPTINLINE float calcModulator(byte operator, word modulator)
{
	float result;
	result = OPL2_Tremolo(operator,OPL2_Exponential(modulator)); //Apply exponential and tremolo!
	result *= generalmodulatorfactor; //Apply modulation factor!
	result *= PI2; //Calculate current modulation!
	return result;
}

byte logop = 0;

//Calculate an operator signal!
OPTINLINE word calcOperator(byte channel, byte operator, float frequency, float modulator, byte feedback, byte volenvoperator, byte updateoperator)
{
	if (operator==0xFF) return 0; //Invalid operator!
	word result; //Our variables?
	float activemodulation;
	//Generate the signal!
	if (feedback) //Apply channel feedback?
	{
		activemodulation = adlibop[operator].lastsignal;
		activemodulation *= generalmodulatorfactor; //Apply modulation factor!
		activemodulation *= adlibch[channel].feedback; //Calculate current feedback
	}
	else
	{
		activemodulation = modulator; //Use the normal modulator!
	}

	if (logop) //Testing?
	{
		dolog("opl2","Rendering operator %02X wave %i, modulation=%f, Frequency=%f(%f)",operator,adlibop[operator].wavesel&wavemask,activemodulation,frequency,adlibop[operator].lastfreq);
	}
	//Generate the correct signal! Ignore time by setting frequency to 0.0f(effectively disables time, keeping it stuck at 0(frequencytime))!
	result = calcOPL2Signal(adlibop[operator].wavesel&wavemask, activemodulation, (frequency?frequency:adlibop[operator].lastfreq), &adlibop[operator].freq0, &adlibop[operator].time); //Take the last frequency or current frequency!
	if (logop) //Testing?
	{
		dolog("opl2", "Rendering operator %02X gave sample %i", operator,result);
	}
	if (volenvoperator==0xFF) goto skipvolenv;
	sword temp;
	byte negative;

	temp = Wave2Word(result); //Load the result!
	if (temp<0) //Negative?
	{
		negative = 1; //Positive!
		temp = -temp; //Reverse sign to make it positive to look up!
	}
	else negative = 0; //Not negative!

	word gain;
	if (updateoperator & 2) //Special: ignore main volume control!
	{
		gain = outputtable[0]; //Always maximum volume, ignore the volume control!
	}
	else //Normal output level!
	{
		gain = adlibop[operator].outputlevel; //Current gain!
	}
	gain += adlibop[operator].gain;
	temp += (sword)gain; //Apply the gain to the raw signal!

	if (negative) //Negative input?
	{
		temp = -temp; //Reverse sign to make it negative again!
	}

	if (logop) //Testing?
	{
		dolog("opl2", "Rendering operator %02X volume gave sample %i(gain %i)", operator,temp,gain);
	}
	result = Word2Wave(temp); //Apply the gain to the input signal!

	skipvolenv: //Skip vol env operator!
	if (frequency && (updateoperator&1)) //Running operator and allowed to update our signal?
	{
		adlibop[operator].lastsignal = OPL2_Tremolo(operator,OPL2_Exponential(result)); //Set last signal #0 to #1(shift into the older one)!
		adlibop[operator].lastfreq = frequency; //We were last running at this frequency!
		incop(operator,frequency); //Increase time for the operator when allowed to increase (frequency=0 during PCM output)!
	}
	return result; //Give the result!
}

float adlib_scaleFactor = SHRT_MAX / (3000.0f*9.0f); //We're running 9 channels in a 16-bit space, so 1/9 of SHRT_MAX

OPTINLINE word getphase(byte channel, byte operator, float frequency) //Get the current phrase of the operator!
{
	return (word)(((fmod(adlibop[operator].time*frequency*PI2,PI2)/PI2)*((float)0x3D0)))>>6; //Give the 10-bits value
}

float convertphase(word phase)
{
	float p; //Default: multiply into positive!
	if (phase&0x200) //Negative?
	{
		p = -1.0f; //Negative!
		phase &= 0x1FF; //Take the negative part!
	}
	else p = 1.0f; //Positive!
	p *= phase; //Multiply the phase (positive or negative) into it!
	p *= ((float)PI/4.0f); //Convert to destination range(4 sinus waves)!
	return p; //Give the phase to execute!
}

OPTINLINE float adlibsample(uint8_t curchan) {
	byte op6_0, op6_1, op7_0, op7_1, op8_0, op8_1; //The four slots used during Drum samples!
	word tempop_phase; //Current phase of an operator!
	word result;
	float immresult; //The operator result and the final result!
	byte op1,op2; //The two operators to use!
	float op1frequency, op2frequency;
	curchan &= 0xF;
	if (curchan >= NUMITEMS(adlibch)) return 0; //No sample with invalid channel!

	//Determine the modulator and carrier to use!
	op1 = adliboperators[0][curchan]; //First operator number!
	op2 = adliboperators[1][curchan]; //Second operator number!
	op1frequency = adlibfreq(op1, curchan); //Load the modulator frequency!
	op2frequency = adlibfreq(op2, curchan); //Load the carrier frequency!

	if (adlibpercussion && (curchan >= 6) && (curchan <= 8)) //We're percussion?
	{
		INLINEREGISTER word tempphase;
		result = 0; //Initialise the result!
		//Calculations based on http://bisqwit.iki.fi/source/opl3emu.html fmopl.c
		//Load our four operators for processing!
		op6_0 = adliboperators[0][6];
		op6_1 = adliboperators[1][6];
		op7_0 = adliboperators[0][7];
		op7_1 = adliboperators[1][7];
		op8_0 = adliboperators[0][8];
		op8_1 = adliboperators[1][8];
		switch (curchan) //What channel?
		{
			case 6: //Bass drum?
				//Generate Bass drum samples!
				//Special on Bass Drum: Additive synthesis(Operator 1) is ignored.

				//Calculate the frequency to use!
				result = calcOperator(6, op6_0, adlibfreq(op6_0,6), 0.0f,0,op6_0,1); //Calculate the modulator for feedback!

				if (adlibch[6].synthmode) //Additive synthesis?
				{
					result = calcOperator(6, op6_1, adlibfreq(op6_1,6), 0.0f, 0,op6_1,1); //Calculate the carrier without applied modulator additive!
				}
				else //FM synthesis?
				{
					result = calcOperator(6, op6_1, adlibfreq(op6_1,6), calcModulator(op6_0,result), 0,op6_1,1); //Calculate the carrier with applied modulator!
				}

				return OPL2_Tremolo(op6_1,OPL2_Exponential(result)*2.0f); //Apply the exponential! The volume is always doubled!
				break;

				//Comments with information from fmopl.c:
				/* Phase generation is based on: */
				/* HH  (13) channel 7->slot 1 combined with channel 8->slot 2 (same combination as TOP CYMBAL but different output phases) */
				/* SD  (16) channel 7->slot 1 */
				/* TOM (14) channel 8->slot 1 */
				/* TOP (17) channel 7->slot 1 combined with channel 8->slot 2 (same combination as HIGH HAT but different output phases) */

			
				/* Envelope generation based on: */
				/* HH  channel 7->slot1 */
				/* SD  channel 7->slot2 */
				/* TOM channel 8->slot1 */
				/* TOP channel 8->slot2 */
				//So phase modulation is based on the Modulator signal. The volume envelope is in the Modulator signal (Hi-hat/Tom-tom) or Carrier signal ()
			case 7: //Hi-hat(Carrier)/Snare drum(Modulator)? High-hat uses modulator, Snare drum uses Carrier signals.
				immresult = 0; //Initialize immediate result!
				if (adlibop[op7_1].volenvstatus) //Snare drum on modulator?
				{
					//Derive frequency from channel 0.
					tempphase = 0x100<<((getphase(7,0,adlibfreq(op7_0,7))>>8)&1); //Bit8=0(Positive) then 0x100, else 0x200! Based on the phase to generate!
					tempphase ^= (OPL2_RNG<<8); //Noise bits XOR'es phase by 0x100 when set!
					result = calcOperator(7, op7_0, op1frequency, 0,0,op7_0,(!adlibop[op8_1].volenvstatus && !adlibop[op7_0].volenvstatus)?1:0); //Calculate the modulator, but only use the current time(position in the sine wave)!
					result = calcOperator(7, op7_1, adlibfreq(op7_1, 7),convertphase(tempphase), 0,op7_1,1); //Calculate the carrier with applied modulator!
					immresult += OPL2_Tremolo(op7_1,OPL2_Exponential(result)*2.0f); //Apply the exponential!
				}
				if (adlibop[op7_0].volenvstatus) //Hi-hat on carrier?
				{
					//Derive frequency from channel 7(modulator) and 8(carrier).
					tempop_phase = getphase(7,0,adlibfreq(op7_0,7)); //Save the phase!
					tempphase = (tempop_phase>>2);
					tempphase ^= (tempop_phase>>7);
					tempphase |= (tempop_phase>>3);
					tempphase &= 1; //Only 1 bit is used!
					tempphase = tempphase?(0x200|(0xD0>>2)):0xD0;
					tempop_phase = getphase(8,1,adlibfreq(op8_1,8)); //Calculate the phase of channel 8 carrier signal!
					if (((tempop_phase>>3)^(tempop_phase>>5))&1) tempphase = 0x200|(0xD0>>2);
					if (tempphase&0x200)
					{
						if (OPL2_RNG) tempphase = 0x2D0;
					}
					else if (OPL2_RNG) tempphase = (0xD0>>2);
					
					result = calcOperator(8, op8_1, adlibfreq(op8_1, 8), 0,0,op8_1,!adlibop[op8_1].volenvstatus); //Calculate the modulator, but only use the current time(position in the sine wave)!
					result = calcOperator(7, op7_0, adlibfreq(op7_0, 7), convertphase(tempphase), 0,op7_0,((!adlibop[op8_1].volenvstatus)?1:0)|2); //Calculate the carrier with applied modulator!
					immresult += OPL2_Tremolo(op7_0,OPL2_Exponential(result)*2.0f); //Apply the exponential!
				}
				result = immresult; //Load the resulting channel!
				result *= 0.5f; //We only have half(two channels combined)!
				return result; //Give the result, converted to short!
				break;
			case 8: //Tom-tom(Carrier)/Cymbal(Modulator)? Tom-tom uses Modulator, Cymbal uses Carrier signals.
				immresult = 0; //Initialize immediate result!
				if (adlibop[op8_1].volenvstatus) //Cymbal(Modulator)?
				{
					//Derive frequency from channel 7(modulator) and 8(carrier).
					tempop_phase = getphase(7,0,adlibfreq(op7_0,7)); //Save the phase!
					tempphase = (tempop_phase>>2);
					tempphase ^= (tempop_phase>>7);
					tempphase |= (tempop_phase>>3);
					tempphase &= 1; //Only 1 bit is used!
					tempphase <<= 9; //0x200 when 1 makes it become 0x300
					tempphase |= 0x100; //0x100 is always!
					tempop_phase = getphase(8,1,adlibfreq(op8_1,8)); //Calculate the phase of channel 8 carrier signal!
					if (((tempop_phase>>3)^(tempop_phase>>5))&1) tempphase = 0x300;
					
					result = calcOperator(8, op8_1, adlibfreq(op8_1, 8), 0,0,op8_1,1); //Calculate the modulator, but only use the current time(position in the sine wave)!
					result = calcOperator(7, op7_0, adlibfreq(op7_0, 7), convertphase(tempphase), 0,op7_0,1); //Calculate the carrier with applied modulator!
					immresult += OPL2_Tremolo(op8_1,OPL2_Exponential(result)*2.0f); //Apply the exponential!
				}
				if (adlibop[op8_0].volenvstatus) //Tom-tom(Carrier)?
				{
					result = calcOperator(8, op8_0, adlibfreq(op8_0, 8), 0, 0,op8_0,2); //Calculate the carrier without applied modulator additive!
					immresult += OPL2_Tremolo(op8_0,OPL2_Exponential(result)*2.0f); //Apply the exponential!
				}
				result = immresult; //Load the resulting channel!
				result *= 0.5f; //We only have half(two channels combined)!
				return result; //Give the result, converted to short!
				break;
		}
		//Not a percussion channel? Pass through!
	}

	//Operator 1!
	//Calculate the frequency to use!
	result = calcOperator(curchan, op1, op1frequency, 0,1,op1,1); //Calculate the modulator for feedback!

	logop = 1;
	if (adlibch[curchan].synthmode) //Additive synthesis?
	{
		result += calcOperator(curchan, op2, op2frequency, 0.0f, 0,op2,1); //Calculate the carrier without applied modulator additive!
	}
	else //FM synthesis?
	{
		result = calcOperator(curchan, op2, op2frequency, calcModulator(op1,result), 0,op2,1); //Calculate the carrier with applied modulator!
	}
	logop = 0;

	return OPL2_Tremolo(op2,OPL2_Exponential(result)); //Apply the exponential!
}

//Timer ticks!

byte ticked80_320 = 0; //80/320 ticked?

OPTINLINE void tick_adlibtimer()
{
	if (adlibregmem[8] & 0x80) //CSM enabled?
	{
		//Process CSM tick!
		byte channel=0;
		for (;;)
		{
			writeadlibKeyON(channel,3); //Force the key to turn on!
			if (++channel==9) break; //Finished!
		}
	}
}

OPTINLINE void adlib_timer320() //Second timer!
{
	if (adlibregmem[4] & 2) //Timer2 enabled?
	{
		if (++timer320 == 0) //Overflown?
		{
			if ((~adlibregmem[4]) & 0x20) //Update status register?
			{
				adlibstatus |= 0xA0; //Update status register and set the bits!
			}
			timer320 = adlibregmem[3]; //Reload timer!
			ticked80_320 = 1; //We're ticked!
		}
	}
}

byte ticks80 = 0; //How many timer 80 ticks have been done?

OPTINLINE void adlib_timer80() //First timer!
{
	ticked80_320 = 0; //Default: not ticked!
	if (adlibregmem[4] & 1) //Timer1 enabled?
	{
		if (++timer80 == 0) //Overflown?
		{
			timer80 = adlibregmem[2]; //Reload timer!
			if ((~adlibregmem[4]) & 0x40) //Update status?
			{
				adlibstatus |= 0xC0; //Update status register and set the bits!
			}
			ticked80_320 = 1; //Ticked 320 clock!
		}
	}
	if (++ticks80 == 4) //Every 4 timer 80 ticks gets 1 timer 320 tick!
	{
		ticks80 = 0; //Reset counter to count 320us ticks!
		adlib_timer320(); //Execute a timer 320 tick!
	}
	if (ticked80_320) tick_adlibtimer(); //Tick by either timer!
}

float counter80step = 0.0f; //80us timer tick interval in samples!

OPTINLINE byte adlib_channelplaying(byte channel)
{
	switch (channel) //What channel?
	{
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6: //Also drum channel, but it's melodic, do no difference here.
		return adlibop[adliboperators[1][channel]].volenvstatus; //Melodic, so carrier!
	case 7:
		if (adlibpercussion) //Percussion mode? Split channels!
		{
			return (adlibop[adliboperators[1][7]].volenvstatus || adlibop[adliboperators[0][7]].volenvstatus); //Percussion channel?
		}
		else //Melodic?
		{
			return adlibop[adliboperators[1][7]].volenvstatus; //Melodic, so carrier!
		}
	case 8:
		if (adlibpercussion) //Percussion mode? Split channels!
		{
			return (adlibop[adliboperators[1][8]].volenvstatus || adlibop[adliboperators[0][8]].volenvstatus); //Percussion channel?
		}
		else //Melodic?
		{
			return adlibop[adliboperators[1][8]].volenvstatus; //Melodic, so carrier!
		}
		break;
	default:
		return 0; //Unknown channel!
	}
	return 0; //Unknown channel!
}


OPTINLINE short adlibgensample() {
	float adlibaccum = 0.0f;
	byte channel;
	for (channel=0;channel<9;++channel) //Process all channels!
	{
		if (adlib_channelplaying(channel)) adlibaccum += adlibsample(channel); //Sample when playing!
	}
	adlibaccum *= adlib_scaleFactor; //Scale according to volume!
	adlibaccum = LIMITRANGE(adlibaccum,(float)SHRT_MIN,(float)SHRT_MAX); //Clip our data to prevent overflow!
	return (sword)(adlibaccum);
}

void EnvelopeGenerator_setAttennuation(ADLIBOP *operator)
{
	if( operator->m_ksl == 0 ) {
		operator->m_kslAdd = 0;
		return;
	}

	if (!operator->channel) return; //Invalid channel?
	// 1.5 dB att. for base 2 of oct. 7
	// created by: round(8*log2( 10^(dbMax[msb]/10) ))+8;
	// verified from real chip ROM
	static const int kslRom[16] = {
		0, 32, 40, 45, 48, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 64
	};
	// 7 negated is, by one's complement, effectively -8. To compensate this,
	// the ROM's values have an offset of 8.
	int tmp = kslRom[operator->channel->m_fnum >> 6] + 8 * ( operator->channel->m_block - 8 );
	if( tmp <= 0 ) {
	operator->m_kslAdd = 0;
	return;
	}
	operator->m_kslAdd = tmp;
	switch( operator->m_ksl ) {
		case 1:
		// 3 db
		operator->m_kslAdd <<= 1;
		break;
	case 2:
		// no change, 1.5 dB
		break;
        case 3:
		// 6 dB
		operator->m_kslAdd <<= 2;
		break;
	}
}

OPTINLINE byte EnvelopeGenerator_nts(ADLIBOP *operator)
{
	return 0; //TODO!
}

OPTINLINE uint8_t EnvelopeGenerator_calculateRate(ADLIBOP *operator, uint8_t rateValue )
{
	if (!operator->channel) return 0; //Invalid channel?
	if( rateValue == 0 ) {
		return 0;
	}
	// calculate key scale number (see NTS in the YMF262 manual)
	uint8_t rof = ( operator->channel->m_fnum >> ( EnvelopeGenerator_nts(operator) ? 8 : 9 ) ) & 0x1;
	// ...and KSR (see manual, again)
	rof |= operator->channel->m_block << 1;
	if( !operator->m_ksr ) {
		rof >>= 2;
	}
	// here, rof<=15
	// the limit of 60 results in rof=0 if rateValue=15 below
	return MIN( 60, rof + (rateValue << 2) );
}

OPTINLINE uint8_t EnvelopeGenerator_advanceCounter(ADLIBOP *operator, uint8_t rate )
{
	if (rate >= 16 ) return 0;
	if( rate == 0 ) {
		return 0;
	}
	const uint8_t effectiveRate = EnvelopeGenerator_calculateRate(operator, rate );
	// rateValue <= 15
	const uint8_t rateValue = effectiveRate >> 2;
	// rof <= 3
	const uint8_t rof = effectiveRate & 3;
	// 4 <= Delta <= (7<<15)
	operator->m_counter += ((uint_32)(4 | rof )) << rateValue;
	// overflow <= 7
	uint8_t overflow = operator->m_counter >> 15;
	operator->m_counter &= ( 1 << 15 ) - 1;
	return overflow;
}

OPTINLINE void EnvelopeGenerator_attenuate( ADLIBOP *operator,uint8_t rate )
{
	if( rate >= 64 ) return;
	operator->m_env += EnvelopeGenerator_advanceCounter(operator, rate );
	if( operator->m_env >= Silence ) {
		operator->m_env = Silence;
	}
}

OPTINLINE void EnvelopeGenerator_release(ADLIBOP *operator)
{
	EnvelopeGenerator_attenuate(operator,operator->m_rr);
	if (operator->m_env>=Silence)
	{
		operator->m_env = Silence;
		operator->volenvstatus = 0; //Finished the volume envelope!
	}
}

OPTINLINE void EnvelopeGenerator_decay(ADLIBOP *operator)
{
	if ((operator->m_env>>4)>=operator->m_sl)
	{
		operator->volenvstatus = 3; //Start sustaining!
		return;
	}
	EnvelopeGenerator_attenuate(operator,operator->m_dr);
}

OPTINLINE void EnvelopeGenerator_attack(ADLIBOP *operator)
{
	if (operator->m_env<=0) //Nothin to attack anymore?
	{
		operator->volenvstatus = 2; //Start decaying!
	}
	else if (operator->m_ar==15)
	{
		operator->m_env = 0;
	}
	else //Attack!
	{
		if (operator->m_env<=0) return; //Abort if too high!
		byte overflow = EnvelopeGenerator_advanceCounter(operator,operator->m_ar); //Advance with attack rate!
		if (!overflow) return;
		operator->m_env -= ((operator->m_env*overflow)>>3)+1; //Affect envelope in a curve!
	}
}

OPTINLINE void tickadlib()
{
	const byte maxop = NUMITEMS(adlibop); //Maximum OP count!
	uint8_t curop;
	for (curop = 0; curop < maxop; curop++)
	{
		if (!adlibop[curop].channel) continue; //Skip invalid operators!
		if (adlibop[curop].volenvstatus) //Are we a running envelope?
		{
			switch (adlibop[curop].volenvstatus)
			{
			case 1: //Attacking?
				EnvelopeGenerator_attack(&adlibop[curop]); //New method: Attack!
				adlibop[curop].volenv = LIMITRANGE(adlibop[curop].m_env,0,Silence); //Apply the linear curve
				adlibop[curop].gain = ((Silence-adlibop[curop].volenv)<<3); //Apply the start gain!
				break;
			case 2: //Decaying?
				EnvelopeGenerator_decay(&adlibop[curop]); //New method: Decay!
				if (adlibop[curop].volenvstatus==3)
				{
					goto startsustain; //Start sustaining if needed!
				}
				adlibop[curop].volenv = LIMITRANGE(adlibop[curop].m_env,0,Silence); //Apply the linear curve
				adlibop[curop].gain = ((Silence-adlibop[curop].volenv)<<3); //Apply the start gain!
				break;
			case 3: //Sustaining?
				startsustain:
				if ((!(adlibop[curop].channel->keyon&adliboperatorsreversekeyon[curop])) || adlibop[curop].ReleaseImmediately) //Release entered?
				{
					++adlibop[curop].volenvstatus; //Enter next phase!
					goto startrelease; //Check again!
				}
				adlibop[curop].volenv = LIMITRANGE(adlibop[curop].m_env,0,Silence); //Apply the linear curve
				adlibop[curop].gain = ((Silence-adlibop[curop].volenv)<<3); //Apply the start gain!
				break;
			case 4: //Releasing?
				startrelease:
				EnvelopeGenerator_release(&adlibop[curop]); //Release: new method!
				adlibop[curop].volenv = LIMITRANGE(adlibop[curop].m_env,0,Silence); //Apply the linear curve
				adlibop[curop].gain = ((Silence-adlibop[curop].volenv)<<3); //Apply the start gain!
				break;
			default: //Unknown volume envelope status?
				adlibop[curop].volenvstatus = 0; //Disable this volume envelope!
				break;
			}
		}
	}
}

//Check for timer occurrences.
void cleanAdlib()
{
	//Discard the amount of time passed!
}

void lockEMUOPL()
{
	lock(LOCK_ADLIB); //Lock us!
}
void unlockEMUOPL()
{
	unlock(LOCK_ADLIB); //Unlock us!
}

//Stuff for the low-pass filter!
float opl2_currentsample = 0, opl2_last_result = 0, opl2_last_sample = 0;
byte opl2_first_sample = 1;

double adlib_ticktiming=0.0f,adlib_soundtiming=0.0f;
void updateAdlib(double timepassed)
{
	//Adlib timer!
	adlib_ticktiming += timepassed; //Get the amount of time passed!
	if (adlib_ticktiming >= 80000.0) //Enough time passed?
	{
		lockEMUOPL();
		for (;adlib_ticktiming >= 80000.0;) //All that's left!
		{
			adlib_timer80(); //Tick 80us timer!
			adlib_ticktiming -= 80000.0; //Decrease timer to get time left!
		}
		unlockEMUOPL();
	}
	
	//Adlib sound output
	adlib_soundtiming += timepassed; //Get the amount of time passed!
	if (adlib_soundtiming>=adlib_soundtick)
	{
		lockEMUOPL();
		for (;adlib_soundtiming>=adlib_soundtick;)
		{
			OPL2_stepRNG(); //Tick the RNG!
			OPL2_stepTremoloVibrato(); //Step tremolo/vibrato!
			byte filled;
			word sample;
			filled = 0; //Default: not filled!
			filled |= adlib_channelplaying(0); //Channel 0?
			filled |= adlib_channelplaying(1); //Channel 1?
			filled |= adlib_channelplaying(2); //Channel 2?
			filled |= adlib_channelplaying(3); //Channel 3?
			filled |= adlib_channelplaying(4); //Channel 4?
			filled |= adlib_channelplaying(5); //Channel 5?
			filled |= adlib_channelplaying(6); //Channel 6?
			filled |= adlib_channelplaying(7); //Channel 7?
			filled |= adlib_channelplaying(8); //Channel 8?
			if (filled) sample = (word)adlibgensample();
			else sample = 0;

			#ifdef ADLIB_LOWPASS
				opl2_currentsample = sample;
				//We're applying the low pass filter for the speaker!
				applySoundLowpassFilter(ADLIB_LOWPASS, usesamplerate, &opl2_currentsample, &opl2_last_result, &opl2_last_sample, &opl2_first_sample);
				sample = opl2_currentsample; //Convert us back to our range!
			#endif


			#ifdef WAV_ADLIB
			writeWAVMonoSample(adlibout,sample); //Log the samples!
			#endif
			writeDoubleBufferedSound16(&adlib_soundbuffer,sample); //Output the sample to the renderer!
			tickadlib(); //Tick us to the next timing if needed!
			adlib_soundtiming -= adlib_soundtick; //Decrease timer to get time left!
		}
		unlockEMUOPL();
	}
}

byte adlib_soundGenerator(void* buf, uint_32 length, byte stereo, void *userdata) //Generate a sample!
{
	if (stereo) return 0; //We don't support stereo!
	
	uint_32 c;
	c = length; //Init c!
	
	static short last=0;
	
	short *data_mono;
	data_mono = (short *)buf; //The data in correct samples!
	for (;;) //Fill it!
	{
		//Left and right samples are the same: we're a mono signal!
		readDoubleBufferedSound16(&adlib_soundbuffer,(word *)&last); //Generate a mono sample if it's available!
		*data_mono++ = last; //Load the last generated sample!
		if (!--c) return SOUNDHANDLER_RESULT_FILLED; //Next item!
	}
}

//Multicall speedup!
#define ADLIBMULTIPLIER 0

void initAdlib()
{
	if (__HW_DISABLED) return; //Abort!

	int i;
	for (i = 0; i < 9; i++)
	{
		memset(&adlibch[i],0,sizeof(adlibch[i])); //Initialise all channels!
	}

	//Build the needed tables!
	for (i = 0; i < (int)NUMITEMS(outputtable); i++)
	{
		outputtable[i] = (((word)(0x3F-i))<<5); //Multiply the raw value by 5 to get the actual gain: the curve is applied by the register shifted left!
	}

	for (i = 0; i < (int)NUMITEMS(adlibop); i++) //Process all channels!
	{
		memset(&adlibop[i],0,sizeof(adlibop[i])); //Initialise the channel!

		//Apply default ADSR!
		adlibop[i].volenvstatus = 0; //Initialise to unused ADSR!
		adlibop[i].ReleaseImmediately = 1; //Release immediately by default!

		adlibop[i].outputlevel = outputtable[0]; //Apply default output!
		adlibop[i].ModulatorFrequencyMultiple = calcModulatorFrequencyMultiple(0); //Which harmonic to use?
		adlibop[i].ReleaseImmediately = 1; //We're defaulting to value being 0=>Release immediately.
		adlibop[i].lastsignal = 0.0f; //Reset the last signals!
		if (adliboperatorsreverse[i]!=0xFF) //Valid operator?
		{
			adlibop[i].channel = &adlibch[adliboperatorsreverse[i]&0x1F]; //The channel this operator belongs to!
		}
	}

	//Source of the Exp and LogSin tables: https://docs.google.com/document/d/18IGx18NQY_Q1PJVZ-bHywao9bhsDoAqoIn1rIm42nwo/edit
	for (i = 0;i < 0x100;++i) //Initialise the exponentional and log-sin tables!
	{
		OPL2_ExpTable[i] = round((pow(2, (float)i / 256.0f) - 1.0f) * 1024.0f);
		OPL2_LogSinTable[i] = round(-log(sin((i + 0.5f)*PI / 256.0f / 2.0f)) / log(2.0f) * 256.0f);
	}
	max_input = OPL2_LogSinTable[0]+outputtable[0]+(Silence<<3); //Maximum input value!
	generalmodulatorfactor = (1.0f/OPL2_Exponential(max_input)); //General modulation factor, as applied to both modulation methods!

	adlib_scaleFactor = (float)(1.0f/(OPL2_Exponential(max_input)*9.0f))*SHRT_MAX; //Highest volume conversion Exp table(resulting mix) to SHRT_MAX (9 channels)!

	for (i = 0;i < (int)NUMITEMS(feedbacklookup2);i++) //Process all feedback values!
	{
		feedbacklookup2[i] = feedbacklookup[i]; //Don't convert for now!
	}

	memset(&tremolovibrato,0,sizeof(tremolovibrato)); //Initialise tremolo/vibrato!
	tremolovibrato[0].depth = 1.0f; //Default: 1dB AM depth!
	tremolovibrato[1].depth = 7.0f; //Default: 7 cent vibrato depth!

	//RNG support!
	OPL2_RNGREG = OPL2_RNG = 0; //Initialise the RNG!

	adlib_ticktiming = adlib_soundtiming = 0.0f; //Reset our output timing!

	if (__SOUND_ADLIB)
	{
		if (allocDoubleBufferedSound16(__ADLIB_SAMPLEBUFFERSIZE,&adlib_soundbuffer)) //Valid buffer?
		{
			if (!addchannel(&adlib_soundGenerator,NULL,"Adlib",usesamplerate,__ADLIB_SAMPLEBUFFERSIZE,0,SMPL16S)) //Start the sound emulation (mono) with automatic samples buffer?
			{
				dolog("adlib","Error registering sound channel for output!");
			}
			else
			{
				setVolume(&adlib_soundGenerator,NULL,ADLIB_VOLUME);
			}
		}
		else
		{
			dolog("adlib","Error registering double buffer for output!");
		}
	}
	//dolog("adlib","sound channel added. registering ports...");
	//Ignore unregistered channel, we need to be used by software!
	register_PORTIN(&inadlib); //Status port (R)
	//All output!
	register_PORTOUT(&outadlib); //Address port (W)
	//dolog("adlib","Registering timer...");

	#ifdef WAV_ADLIB
	adlibout = createWAV("captures/adlib.wav",1,usesamplerate); //Start logging!
	#endif

	#ifdef WAVE_ADLIB
	WAVEFILE *w;
	float t,u=0.0f;
	float f,c;
	word s;
	c = (1.0f/OPL2_Exponential(max_input))*SHRT_MAX;
	f = (1/usesamplerate); //Time of a wave sample!
	w = createWAV("captures/adlibwave.wav",1,usesamplerate); //Start logging one wave!
	for (t=0.0f;t<usesamplerate;t+=1.0f) //Process all samples!
	{
		s = OPL2SinWave(2*PI*1.0f*u); //Get the sample(1Hz sine wave)!
		writeWAVMonoSample(w,(word)(OPL2_Exponential(s)*c)); //Log 1 wave, looked up through exponential input!
		u += f; //Add one sample to the time!
	}
	closeWAV(&w); //Close the wave file!
	#endif
}

void doneAdlib()
{
	if (__HW_DISABLED) return; //Abort!
	#ifdef WAV_ADLIB
	closeWAV(&adlibout); //Stop logging!
	#endif
	if (__SOUND_ADLIB)
	{
		removechannel(&adlib_soundGenerator,NULL,0); //Stop the sound emulation?
		freeDoubleBufferedSound(&adlib_soundbuffer); //Free out double buffered sound!
	}
}
