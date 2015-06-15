/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2012 Mike Chambers

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/* adlib.c: very ugly Adlib OPL2 emulation for Fake86. very much a work in progress. :) */

//#include "config.h"
#include "headers/types.h" //Basic headers!
#include "headers/emu/sound.h" //Basic sound!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/ports.h" //Basic ports!
#include "headers/support/highrestimer.h" //High resoltion timer support!
#include "headers/emu/emu_misc.h" //Random short support for noise generation!
#include "headers/emu/timers.h" //Timer support for attack/decay!
//#include <stdint.h>
//#include <stdio.h>

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

#define dB2factor(dB, fMaxLevelDB) pow(10, ((dB - fMaxLevelDB) / 20))

//extern void set_port_write_redirector (uint16_t startport, uint16_t endport, void *callback);
//extern void set_port_read_redirector (uint16_t startport, uint16_t endport, void *callback);

uint16_t baseport = 0x388; //Adlib address(w)/status(r) port, +1=Data port (write only)
float usesamplerate = 14318180.0f/288.0f; //The sample rate to use for output!

//Counter info
float counter80 = 0.0f, counter320 = 0.0f; //Counter ticks!
byte timer80=0, timer320=0; //Timer variables for current timer ticks!

//Registers itself
uint16_t adlibregmem[0xFF], adlibaddr = 0;

byte adliboperators[2][9] = { //Groupings of 22 registers! (20,40,60,80,E0)
	{ 0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12 },
	{ 0x03, 0x04, 0x05, 0x0B, 0x0C, 0x0D, 0x13, 0x14, 0x15 }
};

byte adliboperatorsreverse[0x16] = { 0, 1, 2, 0, 1, 2,
									255, 255,
									3, 4, 5, 3, 4, 5,
									255, 255,
									6, 7, 8, 6, 7, 8}; //Channel lookup of adlib operators!

static const double feedbacklookup[8] = { 0, PI / 16.0, PI / 8.0, PI / 4.0, PI / 2.0, PI, PI*2.0, PI*4.0 }; //The feedback to use from opl3emu! Seems to be half a sinus wave per number!

byte wavemask = 0; //Wave select mask!

struct structadlibop {
	//Effects
	float outputlevel;
	byte ReleaseImmediately; //Release even when the note is still turned on?

	//Volume envelope
	float attack, decay, sustain, release, volenv; //Volume envelope!
	uint8_t volenvstatus;

	//Signal generation
	uint8_t wavesel;
	float freq0, time; //The q0quency and current time of an operator!
	float ModulatorFrequencyMultiple; //What harmonic to sound?
} adlibop[0x16];

struct structadlibchan {
	uint16_t freq;
	double convfreq;
	uint8_t keyon;
	uint16_t octave;
	uint8_t synthmode; //What synthesizer mode (1=Additive synthesis, 0=Frequency modulation)
	float feedback; //The feedback strength of the modulator signal.
} adlibch[0x10];

double attacktable[0x10] = { 1.0003, 1.00025, 1.0002, 1.00015, 1.0001, 1.00009, 1.00008, 1.00007, 1.00006, 1.00005, 1.00004, 1.00003, 1.00002, 1.00001, 1.000005 }; //1.003, 1.05, 1.01, 1.015, 1.02, 1.025, 1.03, 1.035, 1.04, 1.045, 1.05, 1.055, 1.06, 1.065, 1.07, 1.075 };
double decaytable[0x10] = { 0.99999, 0.999985, 0.99998, 0.999975, 0.99997, 0.999965, 0.99996, 0.999955, 0.99995, 0.999945, 0.99994, 0.999935, 0.99994, 0.999925, 0.99992, 0.99991 };
double sustaintable[0x10], outputtable[0x40]; //Build using software formulas!

uint8_t adlibpercussion = 0, adlibstatus = 0;

uint16_t adlibport = 0x388;

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

void outadlib (uint16_t portnum, uint8_t value) {
	if (portnum==adlibport) {
			adlibaddr = value;
			return;
		}
	portnum = adlibaddr;
	adlibregmem[portnum] = value;
	lockaudio(); //Lock the audio: we're going to adjust audio information!
	switch (portnum) {
	case 0: //Waveform select enable
		wavemask = (adlibregmem[0] & 0x20) ? 3 : 0; //Apply waveform mask!
		break;
			case 4: //timer control
				if (value&0x80) {
						adlibstatus &= 0x1F; //Reset status flags needed!
					}
				if (value&1) //Timer1 enabled?
				{
					timer80 = adlibregmem[2]; //Reload timer!
				}
				if (value&2) //Timer2 enabled?
				{
					timer320 = adlibregmem[3]; //Reload timer!					
				}
				break;
			case 0xBD:
				if (value & 0x10) adlibpercussion = 1;
				else adlibpercussion = 0;
				break;
		}
	if ((portnum >= 0x20) && (portnum <= 0x35)) //Various flags
	{
		portnum &= 0x1F;
		adlibop[portnum].ModulatorFrequencyMultiple = calcModulatorFrequencyMultiple(value & 0xF); //Which harmonic to use?
		adlibop[portnum].ReleaseImmediately = (value & 0x20) ? 0 : 1; //Release when not sustain until release!
	}
	else if ((portnum >= 0x40) && (portnum <= 0x55)) //KSL/Output level
	{
		portnum &= 0x1F;
		adlibop[portnum].outputlevel = outputtable[value & 0x2F]; //Apply output level!
	}
	else if ( (portnum >= 0x60) && (portnum <= 0x75) ) { //attack/decay
			portnum &= 0x1F;
			adlibop[portnum].attack = attacktable[15- (value>>4) ]*1.006;
			adlibop[portnum].decay = decaytable[value&15];
		}
	else if ((portnum >= 0x80) && (portnum <= 0x95)) //sustain/release
	{
		portnum &= 0x1F;
		adlibop[portnum].sustain = sustaintable[value >> 4];
		adlibop[portnum].release = decaytable[value & 15];
	}
	else if ( (portnum >= 0xA0) && (portnum <= 0xB8) )
	{ //octave, freq, key on
		if ((portnum & 0xF) < 9) //Ignore A9-AF!
		{
			portnum &= 0xF; //Get the channel to use!
			if (!adlibch[portnum].keyon && ((adlibregmem[0xB0 + portnum] >> 5) & 1))
			{
				adlibop[adliboperators[0][portnum]].volenvstatus = 1; //Start attacking!
				adlibop[adliboperators[1][portnum]].volenvstatus = 1; //Start attacking!
				adlibop[adliboperators[0][portnum]].volenv = 0.0025;
				adlibop[adliboperators[1][portnum]].volenv = 0.0025;
				adlibop[adliboperators[0][portnum]].freq0 = adlibop[adliboperators[0][portnum]].time = 0.0f; //Initialise operator signal!
				adlibop[adliboperators[1][portnum]].freq0 = adlibop[adliboperators[1][portnum]].time = 0.0f; //Initialise operator signal!
			}
			adlibch[portnum].freq = adlibregmem[0xA0 + portnum] | ((adlibregmem[0xB0 + portnum] & 3) << 8);
			adlibch[portnum].convfreq = ((double)adlibch[portnum].freq * 0.7626459);
			adlibch[portnum].keyon = (adlibregmem[0xB0 + portnum] >> 5) & 1;
			adlibch[portnum].octave = (adlibregmem[0xB0 + portnum] >> 2) & 7;
		}
	}
	else if ((portnum >= 0xC0) && (portnum <= 0xC8))
	{
		portnum &= 0xF;
		adlibch[portnum].synthmode = (adlibregmem[0xC0 + portnum] & 1); //Save the synthesis mode!
		byte feedback;
		feedback = (adlibregmem[0xC0 + portnum] >> 1) & 3; //Get the feedback value used!
		adlibch[portnum].feedback = feedbacklookup[feedback]; //Convert to a feedback of the modulator signal!
	}
	else if ( (portnum >= 0xE0) && (portnum <= 0xF5) ) //waveform select
	{
		portnum &= 0x1F;
		adlibop[portnum].wavesel = value&3;
	}
	unlockaudio(1); //Finished with audio update!
}

uint8_t inadlib (uint16_t portnum) {
	return adlibstatus; //Give the current status!
}

uint16_t adlibfreq (sbyte operatornumber, uint8_t chan) {
	//uint8_t downoct[4] = { 3, 2, 1, 0 };
	//uint8_t upoct[3] = { 1, 2, 3 };
	if (chan > 8) return (0); //Invalid channel: we only have 9 channels!
	uint16_t tmpfreq;
	tmpfreq = (uint16_t) adlibch[chan].convfreq;
	//if (adlibch[chan].octave<4) tmpfreq = tmpfreq>>1;
	//if (adlibch[chan].octave>4) tmpfreq = tmpfreq<<1;
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
				//case 4: tmpfreq = tmpfreq >> 1; break;
			case 5:
				tmpfreq = tmpfreq << 1;
				break;
			case 6:
				tmpfreq = tmpfreq << 2;
				break;
			case 7:
				tmpfreq = tmpfreq << 3;
		}
	if (operatornumber != -1) //Apply frequency multiplication factor?
	{
		tmpfreq *= adlibop[operatornumber].ModulatorFrequencyMultiple; //Apply the frequency multiplication factor!
	}
	return (tmpfreq);
}

float adlib_scaleFactor = (SHRT_MAX - 1.0f);

OPTINLINE float adlibWave(byte signal, const float frequencytime) {
	double x;
	float result = sinf(2.0f * PI * frequencytime); //The sinus function!
	float t = modf(frequencytime, &x);

	switch (signal) {
	case 0: //SINE?
		return result; //SINE!
	case 1: // Negative=0?
		if (t > 0.5f) return 0.0f; //Negative!
		return result; //Positive!
	case 3: // Absolute with second half=0?
		if (fmod(t,0.5f) > 0.25) return 0.0f; //Are we the second half of the half period? Clear the signal if so!
	case 2: // Absolute?
		if (result < 0) result = 0 - result; //Make positive!
		return result; //Simply absolute!
	case 4:
		return RandomFloat(-1.0f, 1.0f); //Random noise!	
	default: //Unknown signal?
		return 0.0f;
	}
}

OPTINLINE float calcAdlibSignal(byte wave, float phase, float frequency, float *freq0, float *time) //Calculates a signal for input to the adlib synth!
{
	const float adlib_sampleLength = 1.0f / usesamplerate;
	if (frequency != *freq0) { //Frequency changed?
		*time *= (*freq0 / frequency);
	}

	float result = adlibWave(wave, (frequency * *time)+phase); //Set the channels!
	*time += adlib_sampleLength; //Add 1 sample to the time!

	float temp = *time*frequency; //Calculate!
	if (temp > 1.0f) {
		double d;
		*time = modf(temp, &d) / frequency;
	}

	*freq0 = frequency;
	return result; //Give the generated sample!
}

//Calculate an operator signal!
OPTINLINE float calcOperator(byte curchan, byte operator, float modulator, float feedback)
{
	float frequency, result; //What frequency to generate?

	//Calculate the frequency to use!
	frequency = adlibfreq(operator, curchan); //Effective carrier init!
	if (adlibch[curchan].synthmode) modulator = 0.0f; //Don't FM using the first operator when needed, so no PM!

	//Generate the signal!
	result = calcAdlibSignal(adlibop[operator].wavesel&wavemask,modulator, frequency, &adlibop[operator].freq0, &adlibop[operator].time);
	result *= adlibop[operator].volenv; //Apply current volume of the ADSR envelope!
	result *= adlibop[operator].outputlevel; //Apply the output level to the operator!

	if (feedback!=0.0f) //We're using feedback?
	{
		result *= feedback; //Convert to feedback ratio!
		return calcOperator(curchan, operator,result, 0); //Apply feedback using our own signal as the modulator!
	}

	return result; //Give the result!
}

OPTINLINE short adlibsample (uint8_t curchan) {
	float modulatorresult, result; //The operator result and the final result!

	if (curchan >= NUMITEMS(adlibch)) return 0; //No sample with invalid channel!
	if (adlibpercussion && (curchan >= 6) && (curchan <= 8)) //We're percussion?
	{
		return 0; //Percussion isn't supported yet!
	}

	//Determine the type of modulation!
	//Operator 1!
	modulatorresult = calcOperator(curchan, adliboperators[0][curchan], 0,adlibch[curchan].feedback); //Calculate the modulator!
	result = calcOperator(curchan, adliboperators[1][curchan], modulatorresult,0); //Calculate the carrier with applied modulator if needed!

	//Perform additive synthesis when asked to do so.
	if (adlibch[curchan].synthmode) result += modulatorresult; //Perform additive synthesis when needed!

	result *= adlib_scaleFactor; //Convert to output scale (We're only going from -1.0 to +1.0 up to this point), convert to signed 16-bit scale!
	return (short)result; //Give the result, converted to short!
}

void adlib_soundtick()
{
	const float timer80 = (80/1000000) / (1/usesamplerate); //80us timer tick interval in samples!
	const float timer320 = (320/1000000) / (1/usesamplerate); //320us timer tick interval in samples!
	counter80 += timer80; //Tick he 80us counter!
	counter320 += timer320; //Tick the 320us counter!
	for (;counter80>=1.0f;) //Somthing left?
	{
		adlib_timer80(); //Tick 80us timer!
		counter80 -= 1.0f; //Decrease counter!
	}
	for (;counter320>=1.0f;) //Somthing left?
	{
		adlib_timer320(); //Tick 320us timer!
		counter320 -= 1.0f; //Decrease counter!
	}
}

OPTINLINE short adlibgensample() {
	adlib_soundtick(); //Tick the sound!
	uint8_t curchan;
	short adlibaccum;
	adlibaccum = 0;
	for (curchan = 0; curchan < 9; curchan++)
	{
		if (adlibop[adliboperators[1][curchan]].volenvstatus) //Are we a running envelope?
		{
			adlibaccum += adlibsample(curchan);
		}
	}
	return (adlibaccum);
}

float min_vol = __MIN_VOL;

void tickadlib()
{
	uint8_t curop;
	for (curop = 0; curop < NUMITEMS(adlibop); curop++)
	{
		if (adliboperatorsreverse[curop] == 0xFF) continue; //Skip invalid operators!
		if (adlibop[curop].volenvstatus) //Are we a running envelope?
		{
		volenv_recheck: //Recheck!
			switch (adlibop[curop].volenvstatus)
			{
			case 1: //Attacking?
				adlibop[curop].volenv *= adlibop[curop].attack; //Attack!
				if (adlibop[curop].volenv >= 1.0)
				{
					adlibop[curop].volenv = 1.0; //We're at 1.0 to start decaying!
					++adlibop[curop].volenvstatus; //Enter next phase!
					goto volenv_recheck;
				}
				break;
			case 2: //Decaying?
				adlibop[curop].volenv *= adlibop[curop].decay; //Decay!
				if (adlibop[curop].volenv <= adlibop[curop].sustain) //Sustain level reached?
				{
					adlibop[curop].volenv = adlibop[curop].sustain; //Sustain level!
					++adlibop[curop].volenvstatus; //Enter next phase!
					goto volenv_recheck;
				}
				break;
			case 3: //Sustaining?
				if ((!adlibch[adliboperatorsreverse[curop]].keyon) || adlibop[curop].ReleaseImmediately) //Release entered?
				{
					++adlibop[curop].volenvstatus; //Enter next phase!
					goto volenv_recheck; //Check again!
				}
				break;
			case 4: //Releasing?
				adlibop[curop].volenv *= adlibop[curop].release; //Release!
				if (adlibop[curop].volenv < min_vol) //Less than the format can provide?
				{
					adlibop[curop].volenv = 0.0f; //Clear the sound!
					adlibop[curop].volenvstatus = 0; //Terminate the signal: we're unused!
				}
				break;
			default: //Unknown volume envelope status?
				adlibop[curop].volenvstatus = 0; //Disable this volume envelope!
				break;
			}
			if (adlibop[curop].volenv < min_vol) //Below minimum?
			{
				adlibop[curop].volenv = 0.0f; //No volume below minimum!
			}
		}
	}
}

byte adlib_soundGenerator(void* buf, uint_32 length, byte stereo, void *userdata) //Generate a sample!
{
	if (stereo) return 0; //We don't support stereo!
	
	byte filled,curchan;
	filled = 0; //Default: not filled!
	for (curchan=0; curchan<9; curchan++) { //Check for active channels!
			if (adlibop[adliboperators[1][curchan]].volenvstatus) //Are we a running envelope?
			{
				filled = 1; //We're filled!
				break; //Stop searching!
			}
		}
	
	if (!filled) return SOUNDHANDLER_RESULT_NOTFILLED; //Not filled: nothing to sound!
	
	uint_32 c;
	c = length; //Init c!
	short *data_mono;
	data_mono = (short *)buf; //The data in correct samples!
	for (;;) //Fill it!
	{
		//Left and right are the same!
		*data_mono++ = adlibgensample(); //Generate a mono sample!
		if (!--c) return SOUNDHANDLER_RESULT_FILLED; //Next item!
	}
}

//Timer ticks!

byte ticked80 = 0; //80 ticked?

void tick_adlibtimer()
{
	//We don't have any IRQs assigned!
	if (adlibregmem[8]&0x80) //CSM enabled?
	{
		//Process CSM tick!
	}
}

void adlib_timer80() //First timer!
{
	ticked80 = 0; //Default: not ticked!
	if (adlibregmem[4] & 1) //Timer1 enabled?
	{
		if (++timer80 == 0) //Overflown?
		{
			timer80 = adlibregmem[2]; //Reload timer!
			if ((~adlibregmem[4])&0x40) //Update status?
			{
				adlibstatus |= 0xC0; //Update status register and set the bits!
			}
			tick_adlibtimer(); //Tick either timer!
			ticked80 = 1; //Ticked 80 clock!
		}
	}
}

void adlib_timer320() //Second timer!
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
			if (!ticked80) tick_adlibtimer(); //Tick either if not already ticked!
		}
	}
	ticked80 = 0; //Reset 80 tick!
}

//Multicall speedup!
#define ADLIBMULTIPLIER 0

void initAdlib()
{
	if (__HW_DISABLED) return; //Abort!

	int i;
	for (i = 0; i < 9; i++)
	{
		adlibch[i].keyon = 0; //Initialise key on!
		adlibch[i].feedback = 0.0f; //No feedback!
		adlibch[i].synthmode = 0; //Default synth mode (FM Synthesis)!
	}

	//Build the needed tables!
	for (i = 0; i < NUMITEMS(sustaintable); i++)
	{
		if (i==0xF) sustaintable[i] = dB2factor(93, 93); //Full volume exception with all bits set!
		else sustaintable[i] = dB2factor((float)93-(float)(((i & 1) ? 3 : 0) +	((i & 2) ? 6 : 0) +	((i & 4) ? 12 : 0) + ((i & 8) ? 24 : 0)), 93); //Build a sustain table!
	}

	for (i = 0; i < NUMITEMS(outputtable); i++)
	{
		outputtable[i] = dB2factor((float)48 - (float)(
			((i & 1) ? 0.75:0)+
			((i&2)?1.5:0)+
			((i&4)?3:0)+
			((i&8)?6:0)+
			((i&0x10)?12:0)+
			((i&0x20)?24:0)
			),48.0f);
	}

	for (i = 0; i < NUMITEMS(adlibop); i++) //Process all channels!
	{
		adlibop[i].freq0 = adlibop[i].time = 0.0f; //Initialise the signal!

		//Apply default ADSR!
		adlibop[i].attack = attacktable[15] * 1.006;
		adlibop[i].decay = decaytable[0];
		adlibop[i].sustain = sustaintable[15];
		adlibop[i].release = decaytable[0];

		adlibop[i].volenvstatus = 0; //Initialise to unused ADSR!
		adlibop[i].volenv = 0.0f; //Volume envelope to silence!
		adlibop[i].ReleaseImmediately = 1; //Release immediately by default!

		adlibop[i].outputlevel = outputtable[0]; //Apply default output!
	}


	if (__SOUND_ADLIB)
	{
		if (!addchannel(&adlib_soundGenerator,NULL,"Adlib",usesamplerate,0,0,SMPL16S)) //Start the sound emulation (mono) with automatic samples buffer?
		{
			dolog("adlib","Error registering sound channel for output!");
		}
		else
		{
			setVolume(&adlib_soundGenerator,NULL,ADLIB_VOLUME);
		}
	}
	//dolog("adlib","sound channel added. registering ports...");
	//Ignore unregistered channel, we need to be used by software!
	register_PORTIN(baseport,&inadlib); //Status port (R)
	//All output!
	register_PORTOUT(baseport,&outadlib); //Address port (W)
	register_PORTOUT(baseport+1,&outadlib); //Data port (W/O)
	//dolog("adlib","Registering timer...");
	addtimer(usesamplerate,&tickadlib,"AdlibAttackDecay",ADLIBMULTIPLIER,0,NULL); //We run at 49.716Khz, about every 20us.
	addtimer(1.0f / (80.0f / 1000000.0f), &adlib_timer80, "AdlibTimer80", 0, 0, NULL); //80us timer!
	addtimer(1.0f / (320.0f / 1000000.0f), &adlib_timer80, "AdlibTimer320", 0, 0, NULL); //320us timer!
	//dolog("adlib","Ready"); //Ready to run!
}

void doneAdlib()
{
	if (__HW_DISABLED) return; //Abort!
	removetimer("AdlibAttackDecay"); //Stop the audio channel!
	if (__SOUND_ADLIB)
	{
		removechannel(&adlib_soundGenerator,NULL,0); //Stop the sound emulation?
	}
	//Unregister the ports!
	register_PORTIN(baseport,NULL);
	//All output!
	register_PORTOUT(baseport,NULL);
	register_PORTOUT(baseport+1,NULL);		
}