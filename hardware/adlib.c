#include "headers/types.h" //Basic headers!
#include "headers/emu/sound.h" //Basic sound!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/ports.h" //Basic ports!
#include "headers/support/highrestimer.h" //High resoltion timer support!
#include "headers/emu/emu_misc.h" //Random short support for noise generation!
#include "headers/emu/timers.h" //Timer support for attack/decay!
#include "headers/support/locks.h" //Locking support!

#include "headers/support/highrestimer.h" //High resolution timer!
#include "headers/support/fifobuffer.h" //FIFO buffer support!

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

//How large is our sample buffer? 1=Real time, 0=Automatically determine by hardware
#define __ADLIB_SAMPLEBUFFERSIZE 4096

//The double buffering threshold!
#define __ADLIBDOUBLE_THRESHOLD __ADLIB_SAMPLEBUFFERSIZE

#define PI2 ((float)(2.0f * PI))

//extern void set_port_write_redirector (uint16_t startport, uint16_t endport, void *callback);
//extern void set_port_read_redirector (uint16_t startport, uint16_t endport, void *callback);

uint16_t baseport = 0x388; //Adlib address(w)/status(r) port, +1=Data port (write only)

//Sample based information!
const float usesamplerate = 14318180.0f/288.0f; //The sample rate to use for output!
const float adlib_soundtick = 1000000000.0f/(14318180.0f/288.0f); //The length of a sample in ns!
//The length of a sample step:
#define adlib_sampleLength (1.0f / (14318180.0f / 288.0f))

const float modulatorfactor = 4084.0f / 1024.0f; //Modulation factor!

//Counter info
float counter80 = 0.0f, counter320 = 0.0f; //Counter ticks!
byte timer80=0, timer320=0; //Timer variables for current timer ticks!

//Registers itself
byte adlibregmem[0xFF], adlibaddr = 0;

float OPL2_ExpTable[0x100], OPL2_LogSinTable[0x100]; //The OPL2 Exponentional and Log-Sin tables!

byte adliboperators[2][0x10] = { //Groupings of 22 registers! (20,40,60,80,E0)
	{ 0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12,255,255,255,255,255,255 },
	{ 0x03, 0x04, 0x05, 0x0B, 0x0C, 0x0D, 0x13, 0x14, 0x15,255,255,255,255,255,255 }
};

byte adliboperatorsreverse[0x16] = { 0, 1, 2, 0, 1, 2, 255, 255, 3, 4, 5, 3, 4, 5, 255, 255, 6, 7, 8, 6, 7, 8}; //Channel lookup of adlib operators!
byte adliboperatorsreversekeyon[0x16] = { 1, 1, 1, 2, 2, 2, 255, 255, 1, 1, 1, 2, 2, 2, 255, 255, 1, 1, 1, 2, 2, 2}; //Modulator/carrier lookup of adlib operators in the keyon bits!

static const double feedbacklookup[8] = { 0, PI / 16.0, PI / 8.0, PI / 4.0, PI / 2.0, PI, PI*2.0, PI*4.0 }; //The feedback to use from opl3emu! Seems to be half a sinus wave per number!
double feedbacklookup2[8]; //Actual feedback lookup value!

byte wavemask = 0; //Wave select mask!

FIFOBUFFER *adlibsound = NULL, *adlibdouble = NULL; //Our sound buffer for rendering!

typedef struct {
	//Effects
	float outputlevel, outputlevelraw; //(RAW) output level!
	byte ReleaseImmediately; //Release even when the note is still turned on?

	//Volume envelope
	float attack, attacklinear, decay, decaylinear, sustain, release, releaselinear, volenv, volenvraw, volenvcalculated; //Volume envelope(and raw version)!
	uint8_t volenvstatus;
	byte m_ar, m_dr, m_sl, m_rr; //Four rates and levels!
	word m_counter; //Counter for the volume envelope!
	word m_env;

	//Signal generation
	uint8_t wavesel;
	float freq0, time; //The frequency and current time of an operator!
	float ModulatorFrequencyMultiple; //What harmonic to sound?
	float lastsignal[2]; //The last signal produced!
	float lastfreq; //Last valid set frequency!
	word m_fnum, m_block, m_ksl, m_kslAdd, m_ksr; //Our settings!
} ADLIBOP; //An adlib operator to process!

ADLIBOP adlibop[0x20];

struct structadlibchan {
	uint16_t freq;
	double convfreq;
	uint8_t keyon;
	uint16_t octave;
	uint8_t synthmode; //What synthesizer mode (1=Additive synthesis, 0=Frequency modulation)
	float feedback; //The feedback strength of the modulator signal.
} adlibch[0x10];

float attacktable[0x10] = { 1.0003f, 1.00025f, 1.0002f, 1.00015f, 1.0001f, 1.00009f, 1.00008f, 1.00007f, 1.00006f, 1.00005f, 1.00004f, 1.00003f, 1.00002f, 1.00001f, 1.000005f }; //1.003, 1.05, 1.01, 1.015, 1.02, 1.025, 1.03, 1.035, 1.04, 1.045, 1.05, 1.055, 1.06, 1.065, 1.07, 1.075 };
float decaytable[0x10] = { 0.99999f, 0.999985f, 0.99998f, 0.999975f, 0.99997f, 0.999965f, 0.99996f, 0.999955f, 0.99995f, 0.999945f, 0.99994f, 0.999935f, 0.99994f, 0.999925f, 0.99992f, 0.99991f };
float sustaintable[0x10], outputtable[0x40]; //Build using software formulas!
word outputtableraw[0x40]; //Build using software formulas!

uint8_t adlibpercussion = 0, adlibstatus = 0;

uint_32 OPL2_RNGREG = 0;
uint_32 OPL2_RNG = 0; //The current random generated sample!

uint16_t adlibport = 0x388;

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
				keyon = ((keyon>>1)&3); //Shift the information to modulator and carrier positions!
				channel = 8; //Use channel 8!
				break;
			default: //Unknown channel?
				//New key on for melodic channels? Don't change anything!
				break;
		}
	}
	if ((adliboperators[0][channel]!=0xFF) && (((keyon&1) && ((oldkeyon^keyon)&1)) || (forcekeyon&1))) //Key ON on operator #1?
	{
		adlibop[adliboperators[0][channel]&0x1F].volenvstatus = 1; //Start attacking!
		adlibop[adliboperators[0][channel]&0x1F].volenvcalculated = adlibop[adliboperators[0][channel]&0x1F].volenv = 0.0025f;
		adlibop[adliboperators[0][channel]&0x1F].volenvraw = 0; //No raw level!
		adlibop[adliboperators[0][channel]&0x1F].freq0 = adlibop[adliboperators[0][channel]&0x1F].time = 0.0f; //Initialise operator signal!
		memset(&adlibop[adliboperators[0][channel]&0x1F].lastsignal, 0, sizeof(adlibop[0].lastsignal)); //Reset the last signals!
	}
	if ((adliboperators[1][channel]!=0xFF) && (((keyon&2) && ((oldkeyon^keyon)&2)) || (forcekeyon&2))) //Key ON on operator #2?
	{
		adlibop[adliboperators[1][channel]&0x1F].volenvstatus = 1; //Start attacking!
		adlibop[adliboperators[1][channel]&0x1F].volenvcalculated = adlibop[adliboperators[0][channel]&0x1F].volenv = 0.0025f;
		adlibop[adliboperators[1][channel]&0x1F].volenvraw = 0; //No raw level!
		adlibop[adliboperators[1][channel]&0x1F].freq0 = adlibop[adliboperators[1][channel]&0x1F].time = 0.0f; //Initialise operator signal!
		memset(&adlibop[adliboperators[1][channel]&0x1F].lastsignal, 0, sizeof(adlibop[1].lastsignal)); //Reset the last signals!
	}
	adlibch[channel].freq = adlibregmem[0xA0 + channel] | ((adlibregmem[0xB0 + channel] & 3) << 8);
	adlibch[channel].convfreq = ((double)adlibch[channel].freq * 0.7626459);
	adlibch[channel].keyon = keyon | forcekeyon; //Key is turned on?
	adlibch[channel].octave = (adlibregmem[0xB0 + channel] >> 2) & 7;
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
			//wavemask = (adlibregmem[0] & 0x20) ? 3 : 0; //Apply waveform mask! Ignore for debugging waves!
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
		}
		break;
	case 0x40:
	case 0x50:
		if (portnum <= 0x55) //KSL/Output level
		{
			portnum &= 0x1F;
			adlibop[portnum].outputlevel = (float)outputtable[value & 0x2F]; //Apply output level!
			adlibop[portnum].outputlevelraw = (float)outputtableraw[value&0x2F]; //Apply raw output level!
		}
		break;
	case 0x60:
	case 0x70:
		if (portnum <= 0x75) { //attack/decay
			portnum &= 0x1F;
			adlibop[portnum].attack = (float)attacktable[15 - (value >> 4)] * 1.006f;
			adlibop[portnum].decay = (float)decaytable[value & 15];
			adlibop[portnum].m_ar = (value>>4); //Attack rate
			adlibop[portnum].m_dr = (value&0xF); //Decay rate
		}
		break;
	case 0x80:
	case 0x90:
		if (portnum <= 0x95) //sustain/release
		{
			portnum &= 0x1F;
			adlibop[portnum].sustain = (float)sustaintable[value >> 4];
			adlibop[portnum].release = (float)decaytable[value & 15];
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

OPTINLINE uint16_t adlibfreq (sbyte operatornumber, uint8_t chan) {
	if (chan > 8) return (0); //Invalid channel: we only have 9 channels!
	uint16_t tmpfreq;
	tmpfreq = (uint16_t) adlibch[chan].convfreq;
	switch (adlibch[chan].octave) {
			case 0:
				tmpfreq = tmpfreq >> 4;
				break;
			case 1:
				tmpfreq = tmpfreq >> 3;
				break;
			case 2:
				tmpfreq = tmpfreq >> 2;
				break;
			case 3:
				tmpfreq = tmpfreq >> 1;
				break;
			case 5:
				tmpfreq = tmpfreq << 1;
				break;
			case 6:
				tmpfreq = tmpfreq << 2;
				break;
			case 7:
				tmpfreq = tmpfreq << 3;
			default:
				break;
		}
	if (operatornumber != -1) //Apply frequency multiplication factor?
	{
		tmpfreq = (word)(tmpfreq*adlibop[operatornumber].ModulatorFrequencyMultiple); //Apply the frequency multiplication factor!
	}
	return (tmpfreq);
}

OPTINLINE float OPL2SinWave(const float r)
{
	float index;
	float entry; //The entry to convert!
	byte location; //The location in the table to use!
	byte PIpart = 0; //Default: part 0!
	index = fmod(r,PI2); //Loop the sinus infinitely!
	if (index>=(float)PI) //Second half?
	{
		PIpart = 2; //Second half!
	}
	if (fmod(index,(float)PI)>=(0.5f*(float)PI)) //Past quarter?
	{
		PIpart |= 1; //Second half!
	}
	index = (fmod(index,(0.5f*(float)PI))/(0.5f*(float)PI))*256.0f; //Convert to full range!
	location = (byte)index; //Set the location to use!
	if (PIpart&1) //Reversed quarter(first and third quarter)?
	{
		location = 255-location; //Reverse us!
	}

	entry = OPL2_LogSinTable[location]; //Take the full load!
	entry = OPL2_LogSinTable[0] - OPL2_LogSinTable[location]; //First quarter lookup! Reverse the signal (it goes from Umax to Umin instead of a normal sin(0PI to 0.5PI))
	if (PIpart & 2) //Second half is negative?
	{
		entry = -entry; //Make us negative!
	}
	return entry; //Give the processed entry!
}

float expfactor = 1.0f;
float explookup = 1.0f;

OPTINLINE float OPL2_Exponential(float v)
{
	if (v>=0.0f) //Positive?
		return OPL2_ExpTable[(int)(v*explookup)]*expfactor; //Convert to exponent!
	else //Negative?
		return (-OPL2_ExpTable[(int)((-v)*explookup)])*expfactor; //Convert to negative exponent!
}

OPTINLINE float adlibWave(byte signal, const float frequencytime) {
	double x;
	float result,t;
	result = PI2; //Load PI2!
	result *= frequencytime; //Apply freqtime!
	switch (signal) {
	case 0: //SINE?
		return OPL2SinWave(result); //The sinus function!
	case 0xFF: //Random signal?
		return RandomFloat(-1.0f, 1.0f); //Random noise!	
	default:
		t = (float)modf(frequencytime, &x); //Calculate rest for special signal information!
		switch (signal) { //What special signal?
		case 1: // Negative=0?
			if (t >= 0.5f) return 0.0f; //Negative!
			result = OPL2SinWave(result); //The sinus function!
			return result; //Positive!
		case 3: // Absolute with second half=0?
			if (fmod(t, 0.5f) >= 0.25) return 0.0f; //Are we the second half of the half period? Clear the signal if so!
		case 2: // Absolute?
			result = OPL2SinWave(result); //The sinus function!
			if (result < 0) result = 0 - result; //Make positive!
			return result; //Simply absolute!
		default: //Unknown signal?
			return 0.0f;
		}
	}
}

OPTINLINE float calcAdlibSignal(byte wave, float phase, float frequency, float *freq0, float *time) //Calculates a signal for input to the adlib synth!
{
	float ftp;
	if (frequency != *freq0) { //Frequency changed?
		*time *= (*freq0 / frequency);
	}

	ftp = frequency; //Frequency!
	ftp *= *time; //Time!
	ftp += phase; //Add phase!
	*freq0 = frequency; //Update new frequency!
	return adlibWave(wave, ftp); //Give the generated sample!
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

//Calculate an operator signal!
OPTINLINE float calcOperator(byte curchan, byte operator, float frequency, float modulator, byte feedback, byte volenvoperator, byte updateoperator)
{
	if (operator==0xFF) return 0.0f; //Invalid operator!
	float result,feedbackresult; //Our variables?
	//Generate the signal!
	if (feedback) //Apply channel feedback?
	{
		modulator = adlibop[operator].lastsignal[0]; //Take the previous last signal!
		modulator += adlibop[operator].lastsignal[1]; //Take the last signal!
		modulator *= adlibch[curchan].feedback; //Calculate current feedback!
		modulator = OPL2_Exponential(modulator); //Apply the exponential conversion!
	}

	//Generate the correct signal!
	result = calcAdlibSignal(adlibop[operator].wavesel&wavemask, modulator, frequency?frequency:adlibop[operator].lastfreq, &adlibop[operator].freq0, &adlibop[operator].time); //Take the last frequency or current frequency!
	if (volenvoperator==0xFF) goto skipvolenv;
	//result += adlibop[volenvoperator].outputlevelraw; //Apply the output level to the operator!
	//result += adlibop[volenvoperator].m_env; //Apply current volume of the ADSR envelope!
	result *= adlibop[volenvoperator].outputlevel; //Apply old output level directly!
	result *= adlibop[volenvoperator].volenv; //Apply volume envelope directly!
	skipvolenv: //Skip vol env operator!
	if (frequency && updateoperator) //Running operator and allowed to update our signal?
	{
		feedbackresult = result; //Load the current feedback value!
		feedbackresult *= 0.5f; //Prevent overflow (we're adding two values together, so take half the value calculated)!
		adlibop[operator].lastsignal[0] = adlibop[operator].lastsignal[1]; //Set last signal #0 to #1(shift into the older one)!
		adlibop[operator].lastsignal[1] = feedbackresult; //Set the feedback result!
		adlibop[operator].lastfreq = frequency; //We were last running at this frequency!
		incop(operator,frequency); //Increase time for the operator when allowed to increase (frequency=0 during PCM output)!
	}
	return result; //Give the result!
}

float adlib_scaleFactor = 65535.0f / 1018.0f; //We're running 8 channels in a 16-bit space, so 1/8 of SHRT_MAX

OPTINLINE uint_32 getphase(byte operator) //Get the current phrase of the operator!
{
	return (word)((fmod(adlibop[operator].time,PI2)/PI2)*511.0f); //512 points (9-bits value?)
}

OPTINLINE short adlibsample(uint8_t curchan) {
	byte op7_0, op7_1, op8_0, op8_1; //The four slots used during Drum samples!
	byte tempop_phase; //Current phase of an operator!
	float result,immresult; //The operator result and the final result!
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
		register uint_32 tempphase;
		result = 0.0f; //Initialise the result!
		//Calculations based on http://bisqwit.iki.fi/source/opl3emu.html fmopl.c
		//Load our four operators for processing!
		op7_0 = adliboperators[0][7];
		op7_1 = adliboperators[1][7];
		op8_0 = adliboperators[0][8];
		op8_1 = adliboperators[1][8];
		return 0; //Disabled!
		switch (curchan) //What channel?
		{
			case 6: //Bass drum?
				//Generate Bass drum samples!
				//Special on Bass Drum: Additive synthesis(Operator 1) is ignored.

				//Calculate the frequency to use!
				result = calcOperator(curchan, op1, op1frequency, 0.0f,0,op1,1); //Calculate the modulator for feedback!

				if (adlibch[curchan].synthmode) //Additive synthesis?
				{
					result = calcOperator(curchan, op2, op2frequency, 0.0f, 0,op2,1); //Calculate the carrier without applied modulator additive!
				}
				else //FM synthesis?
				{
					result = calcOperator(curchan, op2, op2frequency, result, 0,op2,1); //Calculate the carrier with applied modulator!
				}

				result = OPL2_Exponential(result); //Apply the exponential! The volume is always doubled!
				result *= adlib_scaleFactor; //Convert to output scale (We're only going from -1.0 to +1.0 up to this point), convert to signed 16-bit scale!
				return (short)result; //Give the result, converted to short!
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
				immresult = 0.0f; //Initialize immediate result!
				if (adlibop[op7_1].volenvstatus) //Snare drum on modulator?
				{
					//Derive frequency from channel 0.
					tempphase = 0x100<<((getphase(op7_0)>>8)&1); //Bit8=0(Positive) then 0x100, else 0x200! Based on the phase to generate!
					tempphase ^= (OPL2_RNG<<8); //Noise bits XOR'es phase by 0x100 when set!
					result = calcOperator(curchan, op7_0, op1frequency, 0.0f,1,op7_0,(!adlibop[op8_1].volenvstatus && !adlibop[op7_0].volenvstatus)); //Calculate the modulator, but only use the current time(position in the sine wave)!
					result = calcOperator(curchan, op7_1, adlibfreq(op7_1, curchan),(((float)tempphase)/(float)0x300)*OPL2_LogSinTable[0], 0,op7_1,1); //Calculate the carrier with applied modulator!
					immresult += OPL2_Exponential(result); //Apply the exponential!
				}
				if (adlibop[op7_0].volenvstatus) //Hi-hat on carrier?
				{
					//Derive frequency from channel 7(modulator) and 8(carrier).~
					tempop_phase = getphase(op7_0); //Save the phase!
					tempphase = (tempop_phase>>2);
					tempphase ^= (tempop_phase>>7);
					tempphase |= (tempop_phase>>3);
					tempphase &= 1; //Only 1 bit is used!
					tempphase = tempphase?(0x200|(0xD0>>2)):0xD0;
					tempop_phase = getphase(op8_1); //Calculate the phase of channel 8 carrier signal!
					if (((tempop_phase>>3)^(tempop_phase>>5))&1) tempphase = 0x200|(0xD0>>2);
					if (tempphase&0x200)
					{
						if (OPL2_RNG) tempphase = 0x2D0;
					}
					else if (OPL2_RNG) tempphase = (0xD0>>2);
					
					result = calcOperator(curchan, op7_0, adlibfreq(op7_0, curchan), 0.0f,0,op7_0,!adlibop[op8_1].volenvstatus); //Calculate the modulator, but only use the current time(position in the sine wave)!
					result = calcOperator(curchan, op7_1, adlibfreq(op7_1, curchan), (((float)tempphase)/(float)0x300)*OPL2_LogSinTable[0], 0,op7_0,1); //Calculate the carrier with applied modulator!
					immresult += OPL2_Exponential(result); //Apply the exponential!
				}
				result = immresult; //Load the resulting channel!
				result *= 0.5f; //We only have half(two channels combined)!
				result *= adlib_scaleFactor; //Convert to output scale (We're only going from -1.0 to +1.0 up to this point), convert to signed 16-bit scale!
				return (short)result; //Give the result, converted to short!
				break;
			case 8: //Tom-tom(Carrier)/Cymbal(Modulator)? Tom-tom uses Modulator, Cymbal uses Carrier signals.
				immresult = 0.0f; //Initialize immediate result!
				if (adlibop[op8_1].volenvstatus) //Cymbal(Modulator)?
				{
					//Derive frequency from channel 7(modulator) and 8(carrier).
					tempop_phase = getphase(op7_0); //Save the phase!
					tempphase = (tempop_phase>>2);
					tempphase ^= (tempop_phase>>7);
					tempphase |= (tempop_phase>>3);
					tempphase &= 1; //Only 1 bit is used!
					tempphase <<= 9; //0x200 when 1 makes it become 0x300
					tempphase |= 0x100; //0x100 is always!
					tempop_phase = getphase(op8_1); //Calculate the phase of channel 8 carrier signal!
					if (((tempop_phase>>3)^(tempop_phase>>5))&1) tempphase = 0x300;
					
					result = calcOperator(curchan, op7_0, adlibfreq(op7_0, curchan), 0.0f,0,op7_0,1); //Calculate the modulator, but only use the current time(position in the sine wave)!
					result = calcOperator(curchan, op7_1, adlibfreq(op7_1, curchan), (((float)tempphase)/(float)0x300)*OPL2_LogSinTable[0], 0,op8_1,1); //Calculate the carrier with applied modulator!
					immresult += OPL2_Exponential(result); //Apply the exponential!
				}
				if (adlibop[op8_0].volenvstatus) //Tom-tom(Carrier)?
				{
					result = calcOperator(curchan, op8_0, adlibfreq(op8_0, curchan), 0.0f, 0,op8_0,1); //Calculate the carrier with applied modulator!
					immresult += OPL2_Exponential(result); //Apply the exponential!
				}
				result = immresult; //Load the resulting channel!
				result *= 0.5f; //We only have half(two channels combined)!
				result *= adlib_scaleFactor; //Convert to output scale (We're only going from -1.0 to +1.0 up to this point), convert to signed 16-bit scale!
				return (short)result; //Give the result, converted to short!
				break;
		}
		return 0; //Percussion isn't supported yet for this channel!
	}

	//Operator 1!
	//Calculate the frequency to use!
	result = calcOperator(curchan, op1, op1frequency, 0.0f,1,op1,1); //Calculate the modulator for feedback!

	if (adlibch[curchan].synthmode) //Additive synthesis?
	{
		result += calcOperator(curchan, op2, op2frequency, 0.0f, 0,op2,1); //Calculate the carrier without applied modulator additive!
	}
	else //FM synthesis?
	{
		result = calcOperator(curchan, op2, op2frequency, OPL2_Exponential(result), 0,op2,1); //Calculate the carrier with applied modulator!
	}

	result = OPL2_Exponential(result); //Apply the exponential!

	result *= adlib_scaleFactor; //Convert to output scale (We're only going from -1.0 to +1.0 up to this point), convert to signed 16-bit scale!
	return (short)result; //Give the result, converted to short!
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

OPTINLINE short adlibgensample() {
	int_32 adlibaccum;
	adlibaccum = 0;
	if (adlibop[adliboperators[1][0]].volenvstatus) //Are we a running envelope?
	{
		adlibaccum += adlibsample(0);
	}
	if (adlibop[adliboperators[1][1]].volenvstatus) //Are we a running envelope?
	{
		adlibaccum += adlibsample(1);
	}
	if (adlibop[adliboperators[1][2]].volenvstatus) //Are we a running envelope?
	{
		adlibaccum += adlibsample(2);
	}
	if (adlibop[adliboperators[1][3]].volenvstatus) //Are we a running envelope?
	{
		adlibaccum += adlibsample(3);
	}
	if (adlibop[adliboperators[1][4]].volenvstatus) //Are we a running envelope?
	{
		adlibaccum += adlibsample(4);
	}
	if (adlibop[adliboperators[1][5]].volenvstatus) //Are we a running envelope?
	{
		adlibaccum += adlibsample(5);
	}
	if (adlibop[adliboperators[1][6]].volenvstatus) //Are we a running envelope?
	{
		adlibaccum += adlibsample(6);
	}
	if (adlibpercussion) //Percussion mode? Split channels!
	{
		if (adlibop[adliboperators[1][7]].volenvstatus || adlibop[adliboperators[0][7]].volenvstatus) //Are we a running envelope?
		{
			adlibaccum += adlibsample(7);
		}
		if (adlibop[adliboperators[1][8]].volenvstatus || adlibop[adliboperators[0][8]].volenvstatus) //Are we a running envelope?
		{
			adlibaccum += adlibsample(8);
		}
	}
	else //Melodic mode? Normal channels!
	{
		if (adlibop[adliboperators[1][7]].volenvstatus) //Are we a running envelope?
		{
			adlibaccum += adlibsample(7);
		}
		if (adlibop[adliboperators[1][8]].volenvstatus) //Are we a running envelope?
		{
			adlibaccum += adlibsample(8);
		}
	}
	return (short)(adlibaccum);
}

void EnvelopeGenerator_setAttennuation(ADLIBOP *operator, uint16_t f_number, uint8_t block, uint8_t ksl )
{
	operator->m_fnum = f_number & 0x3ff;
	operator->m_block = block & 0x07;
	operator->m_ksl = ksl & 0x03;

	if( operator->m_ksl == 0 ) {
		operator->m_kslAdd = 0;
		return;
	}

	// 1.5 dB att. for base 2 of oct. 7
	// created by: round(8*log2( 10^(dbMax[msb]/10) ))+8;
	// verified from real chip ROM
	static const int kslRom[16] = {
		0, 32, 40, 45, 48, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 64
	};
	// 7 negated is, by one's complement, effectively -8. To compensate this,
	// the ROM's values have an offset of 8.
	int tmp = kslRom[operator->m_fnum >> 6] + 8 * ( operator->m_block - 8 );
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

byte EnvelopeGenerator_nts(ADLIBOP *operator)
{
	return 0; //TODO!
}

uint8_t EnvelopeGenerator_calculateRate(ADLIBOP *operator, uint8_t rateValue )
{
	if( rateValue == 0 ) {
		return 0;
	}
	// calculate key scale number (see NTS in the YMF262 manual)
	uint8_t rof = ( operator->m_fnum >> ( EnvelopeGenerator_nts(operator) ? 8 : 9 ) ) & 0x1;
	// ...and KSR (see manual, again)
	rof |= operator->m_block << 1;
	if( !operator->m_ksr ) {
		rof >>= 2;
	}
	// here, rof<=15
	// the limit of 60 results in rof=0 if rateValue=15 below
	return MIN( 60, rof + (rateValue << 2) );
}

inline uint8_t EnvelopeGenerator_advanceCounter(ADLIBOP *operator, uint8_t rate )
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

//Silence value?
#define Silence 64

void EnvelopeGenerator_attenuate( ADLIBOP *operator,uint8_t rate )
{
	if( rate >= 64 ) return;
	operator->m_env += EnvelopeGenerator_advanceCounter(operator, rate );
	if( operator->m_env > Silence ) {
		operator->m_env = Silence;
	}
}

void EnvelopeGenerator_release(ADLIBOP *operator)
{
	EnvelopeGenerator_attenuate(operator,operator->m_rr);
	if (operator->m_env>Silence)
	{
		operator->m_env = Silence;
		operator->volenvstatus = 0; //Finished the volume envelope!
	}
}

void EnvelopeGenerator_decay(ADLIBOP *operator)
{
	if ((operator->m_env>>4)>=operator->m_sl)
	{
		operator->volenvstatus = 3; //Start sustaining!
		return;
	}
	EnvelopeGenerator_attenuate(operator,operator->m_dr);
}

void EnvelopeGenerator_attack(ADLIBOP *operator)
{
	if (!operator->m_env)
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
		operator->volenvraw -= ((operator->m_env*overflow)>>3)+1; //Affect envelope!
	}
}

float min_vol = __MIN_VOL; //Original volume limiter!

OPTINLINE void tickadlib()
{
	const byte maxop = MIN(NUMITEMS(adlibop), NUMITEMS(adliboperatorsreverse)); //Maximum OP count!
	uint8_t curop;
	for (curop = 0; curop < maxop; curop++)
	{
		if (adliboperatorsreverse[curop] == 0xFF) continue; //Skip invalid operators!
		if (adlibop[curop].volenvstatus) //Are we a running envelope?
		{
			switch (adlibop[curop].volenvstatus)
			{
			case 1: //Attacking?
				adlibop[curop].volenv = (adlibop[curop].volenvcalculated *= adlibop[curop].attack); //Attack!
				if (adlibop[curop].volenvcalculated >= 1.0)
				{
					adlibop[curop].volenvcalculated = 1.0; //We're at 1.0 to start decaying!
					++adlibop[curop].volenvstatus; //Enter next phase!
					goto startdecay;
				} //Old method!
				//EnvelopeGenerator_attack(&adlibop[curop]); //New method: Attack!
				break;
			case 2: //Decaying?
				startdecay:
				adlibop[curop].volenv = (adlibop[curop].volenvcalculated *= adlibop[curop].decay); //Decay!
				if (adlibop[curop].volenvcalculated <= adlibop[curop].sustain) //Sustain level reached?
				{
					adlibop[curop].volenvcalculated = adlibop[curop].sustain; //Sustain level!
					++adlibop[curop].volenvstatus; //Enter next phase!
					goto startsustain;
				}
				//EnvelopeGenerator_decay(&adlibop[curop]); //New method: Decay!

				if (adlibop[curop].volenvstatus==3) goto startsustain; //Start sustaining if needed!
				break;
			case 3: //Sustaining?
				startsustain:
				if ((!(adlibch[adliboperatorsreverse[curop]].keyon&adliboperatorsreversekeyon[curop])) || adlibop[curop].ReleaseImmediately) //Release entered?
				{
					++adlibop[curop].volenvstatus; //Enter next phase!
					goto startrelease; //Check again!
				}
				break;
			case 4: //Releasing?
				startrelease:
				adlibop[curop].volenv = (adlibop[curop].volenvcalculated *= adlibop[curop].release); //Release!
				if (adlibop[curop].volenvcalculated < min_vol) //Less than the format can provide?
				{
					adlibop[curop].volenv = adlibop[curop].volenvcalculated = 0.0f; //Clear the sound!
					adlibop[curop].volenvstatus = 0; //Terminate the signal: we're unused!
				}
				
				//EnvelopeGenerator_release(&adlibop[curop]); //Release: new method!
				break;
			default: //Unknown volume envelope status?
				adlibop[curop].volenvstatus = 0; //Disable this volume envelope!
				break;
			}
			if (adlibop[curop].volenvcalculated < min_vol) //Below minimum?
			{
				adlibop[curop].volenv = 0.0f; //No volume below minimum!
			}
			adlibop[curop].volenvraw = adlibop[curop].volenv*256.0f; //256 levels of raw volume!
		}
	}
}

//Check for timer occurrences.
void cleanAdlib()
{
	//Discard the amount of time passed!
}

double adlib_ticktiming=0.0f,adlib_soundtiming=0.0f;
void updateAdlib(double timepassed)
{
	//Adlib timer!
	adlib_ticktiming += timepassed; //Get the amount of time passed!
	if (adlib_ticktiming >= 80000.0) //Enough time passed?
	{
		for (;adlib_ticktiming >= 80000.0;) //All that's left!
		{
			adlib_timer80(); //Tick 80us timer!
			adlib_ticktiming -= 80000.0; //Decrease timer to get time left!
		}
	}
	
	//Adlib sound output
	adlib_soundtiming += timepassed; //Get the amount of time passed!
	if (adlib_soundtiming>=adlib_soundtick)
	{
		for (;adlib_soundtiming>=adlib_soundtick;)
		{
			OPL2_stepRNG(); //Tick the RNG!
			byte filled;
			filled = 0; //Default: not filled!
			filled |= adlibop[adliboperators[1][0]].volenvstatus; //Channel 0?
			filled |= adlibop[adliboperators[1][1]].volenvstatus; //Channel 1?
			filled |= adlibop[adliboperators[1][2]].volenvstatus; //Channel 2?
			filled |= adlibop[adliboperators[1][3]].volenvstatus; //Channel 3?
			filled |= adlibop[adliboperators[1][4]].volenvstatus; //Channel 4?
			filled |= adlibop[adliboperators[1][5]].volenvstatus; //Channel 5?
			filled |= adlibop[adliboperators[1][6]].volenvstatus; //Channel 6?
			filled |= adlibop[adliboperators[1][7]].volenvstatus; //Channel 7?
			filled |= adlibop[adliboperators[1][8]].volenvstatus; //Channel 8?
			if (!filled) writefifobuffer16(adlibdouble,0); //Not filled: nothing to sound!
			else writefifobuffer16(adlibdouble,(word)adlibgensample()); //Add the sample to our sound buffer!
			movefifobuffer16(adlibdouble,adlibsound,__ADLIBDOUBLE_THRESHOLD); //Move any data to the destination once filled!
			tickadlib(); //Tick us to the next timing if needed!
			adlib_soundtiming -= adlib_soundtick; //Decrease timer to get time left!
		}
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
		readfifobuffer16(adlibsound,(word *)&last); //Generate a mono sample if it's available!
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
	for (i = 0; i < (int)NUMITEMS(sustaintable); i++)
	{
		if (i==0xF) sustaintable[i] = (float)dB2factor(93, 93); //Full volume exception with all bits set!
		else sustaintable[i] = (float)dB2factor((float)93-(float)(((i & 1) ? 3 : 0) +	((i & 2) ? 6 : 0) +	((i & 4) ? 12 : 0) + ((i & 8) ? 24 : 0)), 93); //Build a sustain table!
	}

	for (i = 0; i < (int)NUMITEMS(outputtable); i++)
	{
		outputtableraw[i] = (float)48 - (float)(
			((i & 1) ? 0.75:0)+
			((i&2)?1.5:0)+
			((i&4)?3:0)+
			((i&8)?6:0)+
			((i&0x10)?12:0)+
			((i&0x20)?24:0)
			); //Raw output level number!
		outputtable[i] = (float)dB2factor(outputtableraw[i],48.0f); //Generate curve!
		outputtableraw[i] <<= 5; //Multiply the raw value by 5 to get the actual gain!
	}

	for (i = 0; i < (int)NUMITEMS(adlibop); i++) //Process all channels!
	{
		memset(&adlibop[i],0,sizeof(adlibop[i])); //Initialise the channel!
		adlibop[i].freq0 = adlibop[i].time = 0.0f; //Initialise the signal!

		//Apply default ADSR!
		adlibop[i].attack = attacktable[15] * 1.006f;
		adlibop[i].decay = decaytable[0];
		adlibop[i].sustain = sustaintable[15];
		adlibop[i].release = decaytable[0];

		adlibop[i].volenvstatus = 0; //Initialise to unused ADSR!
		adlibop[i].volenvcalculated = 0.0f; //Volume envelope to silence!
		adlibop[i].ReleaseImmediately = 1; //Release immediately by default!

		adlibop[i].outputlevel = outputtable[0]; //Apply default output!
		adlibop[i].outputlevelraw = outputtableraw[0]; //Apply default output!
		adlibop[i].ModulatorFrequencyMultiple = calcModulatorFrequencyMultiple(0); //Which harmonic to use?
		adlibop[i].ReleaseImmediately = 1; //We're defaulting to value being 0=>Release immediately.
		memset(&adlibop[i].lastsignal,0,sizeof(adlibop[i].lastsignal)); //Reset the last signals!
	}

	//Source of the Exp and LogSin tables: https://docs.google.com/document/d/18IGx18NQY_Q1PJVZ-bHywao9bhsDoAqoIn1rIm42nwo/edit
	for (i = 0;i < 0x100;++i) //Initialise the exponentional and log-sin tables!
	{
		OPL2_ExpTable[i] = round((pow(2, (float)i / 256.0f) - 1.0f) * 1024.0f);
		OPL2_LogSinTable[i] = round(-log(sin((i + 0.5)*PI / 256.0f / 2.0f)) / log(2.0f) * 256.0f);
		dolog("OPL2_LogSin","%i=%f",i,OPL2_LogSinTable[i]); //Log our table!
	}
	explookup = (1.0f/OPL2_LogSinTable[0])*255.0f; //Exp lookup factor for LogSin values!
	expfactor = (1.0f/OPL2_ExpTable[255]); //The highest volume conversion to apply with our exponential table!
	adlib_scaleFactor = (float)((1.0f/expfactor)*((1.0f/OPL2_ExpTable[255])*3639.0f)); //Highest volume conversion to SHRT_MAX (9 channels)!

	for (i = 0;i < (int)NUMITEMS(feedbacklookup2);i++) //Process all feedback values!
	{
		feedbacklookup2[i] = feedbacklookup[i] * (1.0f / (4.0f * PI)) * (1.0f/PI2); //Convert to a range of 0-1!
	}

	//RNG support!
	OPL2_RNGREG = OPL2_RNG = 0; //Initialise the RNG!

	adlib_ticktiming = adlib_soundtiming = 0.0f; //Reset our output timing!

	if (__SOUND_ADLIB)
	{
		adlibsound = allocfifobuffer(__ADLIB_SAMPLEBUFFERSIZE<<1,1); //Generate our buffer!
		adlibdouble = allocfifobuffer((__ADLIBDOUBLE_THRESHOLD+1)<<1, 0); //Generate our buffer!
		if (adlibsound) //Valid buffer?
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
	}
	//dolog("adlib","sound channel added. registering ports...");
	//Ignore unregistered channel, we need to be used by software!
	register_PORTIN(&inadlib); //Status port (R)
	//All output!
	register_PORTOUT(&outadlib); //Address port (W)
	//dolog("adlib","Registering timer...");
}

void doneAdlib()
{
	if (__HW_DISABLED) return; //Abort!
	if (__SOUND_ADLIB)
	{
		removechannel(&adlib_soundGenerator,NULL,0); //Stop the sound emulation?
		if (adlibsound) //Valid buffer?
		{
			free_fifobuffer(&adlibsound); //Free our sound buffer!
		}
		if (adlibdouble) //Valid buffer?
		{
			free_fifobuffer(&adlibdouble); //Free our sound buffer!
		}
	}
}
