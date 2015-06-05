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

//Maximum DB volume (equalling 100% volume)!
#define __MAX_DB 100.0f

//extern void set_port_write_redirector (uint16_t startport, uint16_t endport, void *callback);
//extern void set_port_read_redirector (uint16_t startport, uint16_t endport, void *callback);

uint16_t baseport = 0x388; //Adlib address(w)/status(r) port, +1=Data port (write only)
float usesamplerate = 14318180.0f/288.0f; //The sample rate to use for output!

uint16_t adlibregmem[0xFF], adlibaddr = 0;

int8_t oplwave[4][256] = {
	{
		0, 1, 3, 4, 6, 7, 9, 11, 12, 14, 15, 17, 18, 20, 22, 23, 24, 26, 27, 29, 30, 31, 33, 34, 36, 37, 38, 40, 40, 42, 43, 44, 46, 46, 48, 49, 50, 51, 51, 53,
		53, 54, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 62, 63, 63, 63, 64, 64, 64, 116, 116, 116, 116, 116, 116, 116, 116, 116, 64, 64, 64, 63, 63, 63, 62, 62, 61, 61, 60,
		59, 59, 58, 57, 57, 56, 55, 54, 53, 53, 51, 51, 50, 49, 48, 46, 46, 44, 43, 42, 40, 40, 38, 37, 36, 34, 33, 31, 30, 29, 27, 26, 24, 23, 22, 20, 18, 17, 15, 14,
		12, 11, 9, 7, 6, 4, 3, 1, 0, -1, -3, -4, -6, -7, -9, -11, -12, -14, -15, -17, -18, -20, -22, -23, -24, -26, -27, -29, -30, -31, -33, -34, -36, -37, -38, -40, -40, -42, -43, -44,
		-46, -46, -48, -49, -50, -51, -51, -53, -53, -54, -55, -56, -57, -57, -58, -59, -59, -60, -61, -61, -62, -62, -63, -63, -63, -64, -64, -64, -116, -116, -116, -116, -116, -116, -116, -116, -116, -64, -64, -64,
		-63, -63, -63, -62, -62, -61, -61, -60, -59, -59, -58, -57, -57, -56, -55, -54, -53, -53, -51, -51, -50, -49, -48, -46, -46, -44, -43, -42, -40, -40, -38, -37, -36, -34, -33, -31, -30, -29, -27, -26,
		-24, -23, -22, -20, -18, -17, -15, -14, -12, -11, -9, -7, -6, -4, -3, -1
	},

	{
		0, 1, 3, 4, 6, 7, 9, 11, 12, 14, 15, 17, 18, 20, 22, 23, 24, 26, 27, 29,30, 31, 33, 34, 36, 37, 38, 40, 40, 42, 43, 44, 46, 46, 48, 49, 50, 51, 51, 53,
		53, 54, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 62, 63, 63, 63, 64, 64, 64, 116, 116, 116, 116, 116, 116, 116, 116, 116, 64, 64, 64, 63, 63, 63, 62, 62, 61, 61, 60,
		59, 59, 58, 57, 57, 56, 55, 54, 53, 53, 51, 51, 50, 49, 48, 46, 46, 44, 43, 42, 40, 40, 38, 37, 36, 34, 33, 31, 30, 29, 27, 26, 24, 23, 22, 20, 18, 17, 15, 14,
		12, 11, 9, 7, 6, 4, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	},


	{
		0, 1, 3, 4, 6, 7, 9, 11, 12, 14, 15, 17, 18, 20, 22, 23, 24, 26, 27, 29, 30, 31, 33, 34, 36, 37, 38, 40, 40, 42, 43, 44, 46, 46, 48, 49, 50, 51, 51, 53,
		53, 54, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 62, 63, 63, 63, 64, 64, 64, 116, 116, 116, 116, 116, 116, 116, 116, 116, 64, 64, 64, 63, 63, 63, 62, 62, 61, 61, 60,
		59, 59, 58, 57, 57, 56, 55, 54, 53, 53, 51, 51, 50, 49, 48, 46, 46, 44, 43, 42, 40, 40, 38, 37, 36, 34, 33, 31, 30, 29, 27, 26, 24, 23, 22, 20, 18, 17, 15, 14,
		12, 11, 9, 7, 6, 4, 3, 1, 0, 1, 3, 4, 6, 7, 9, 11, 12, 14, 15, 17, 18, 20, 22, 23, 24, 26, 27, 29, 30, 31, 33, 34, 36, 37, 38, 40, 40, 42, 43, 44,
		46, 46, 48, 49, 50, 51, 51, 53, 53, 54, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 62, 63, 63, 63, 64, 64, 64, 116, 116, 116, 116, 116, 116, 116, 116, 116, 64, 64, 64,
		63, 63, 63, 62, 62, 61, 61, 60, 59, 59, 58, 57, 57, 56, 55, 54, 53, 53, 51, 51, 50, 49, 48, 46, 46, 44, 43, 42, 40, 40, 38, 37, 36, 34, 33, 31, 30, 29, 27, 26,
		24, 23, 22, 20, 18, 17, 15, 14, 12, 11, 9, 7, 6, 4, 3, 1
	},


	{
		0, 1, 3, 4, 6, 7, 9, 11, 12, 14, 15, 17, 18, 20, 22, 23, 24, 26, 27, 29, 30, 31, 33, 34, 36, 37, 38, 40, 40, 42, 43, 44, 46, 46, 48, 49, 50, 51, 51, 53,
		53, 54, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 62, 63, 63, 63, 64, 64, 64, 116, 116, 116, 116, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 4, 6, 7, 9, 11, 12, 14, 15, 17, 18, 20, 22, 23, 24, 26, 27, 29, 30, 31, 33, 34, 36, 37, 38, 40, 40, 42, 43, 44,
		46, 46, 48, 49, 50, 51, 51, 53, 53, 54, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 62, 63, 63, 63, 64, 64, 64, 116, 116, 116, 116, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	}

};

byte adliboperators[2][9] = { //Groupings of 22 registers! (20,40,60,80,E0)
	{ 0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12 },
	{ 0x03, 0x04, 0x05, 0x0B, 0x0C, 0x0D, 0x13, 0x14, 0x15 }
};

byte adliboperatorsreverse[0x16] = { 0, 1, 2, 0, 1, 2,
									255, 255,
									3, 4, 5, 3, 4, 5,
									255, 255,
									6, 7, 8, 6, 7, 8}; //Channel lookup of adlib operators!

byte wavemask = 0; //Wave select mask!

struct structadlibop {
	uint8_t wavesel;
	uint8_t AM; //Amplitude modulation enabled?
	float ModulatorFrequencyMultiple; //What harmonic to sound?
	byte ReleaseImmediately; //Release even when the note is still turned on?
} adlibop[0x10];

struct structadlibchan {
	uint16_t freq;
	double convfreq;
	uint8_t keyon;
	uint16_t octave;
	uint8_t synthmode; //What synthesizer mode (1=Additive synthesis, 0=Frequency modulation)
} adlibch[0x10];

double attacktable[0x10] = { 1.0003, 1.00025, 1.0002, 1.00015, 1.0001, 1.00009, 1.00008, 1.00007, 1.00006, 1.00005, 1.00004, 1.00003, 1.00002, 1.00001, 1.000005 }; //1.003, 1.05, 1.01, 1.015, 1.02, 1.025, 1.03, 1.035, 1.04, 1.045, 1.05, 1.055, 1.06, 1.065, 1.07, 1.075 };
double decaytable[0x10] = { 0.99999, 0.999985, 0.99998, 0.999975, 0.99997, 0.999965, 0.99996, 0.999955, 0.99995, 0.999945, 0.99994, 0.999935, 0.99994, 0.999925, 0.99992, 0.99991 };
double sustaintable[0x10] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
double adlibenv[0x20], adlibrelease[0x20], adlibsustain[0x20], adlibdecay[0x20], adlibattack[0x20];
uint8_t adlibenvstatus[0x20], adlibpercussion = 0, adlibstatus = 0;

float AMDepth = 0.0f; //AM depth in dB!

uint16_t adlibport = 0x388;

OPTINLINE double dB2factor(double dB, double fMaxLevelDB)
{
	return pow(10, ((dB - fMaxLevelDB) / 20));
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

void outadlib (uint16_t portnum, uint8_t value) {
	if (portnum==adlibport) {
			adlibaddr = value;
			return;
		}
	portnum = adlibaddr;
	adlibregmem[portnum] = value;
	switch (portnum) {
	case 0: //Waveform select enable
		wavemask = (adlibregmem[0] & 0x20) ? 3 : 0; //Apply waveform mask!
		break;
			case 4: //timer control
				if (value&0x80) {
						adlibstatus = 0;
						adlibregmem[4] = 0;
					}
				break;
			case 0xBD:
				if (value & 0x10) adlibpercussion = 1;
				else adlibpercussion = 0;
				AMDepth = (value & 0x80) ? dB2factor(4.8f, __MAX_DB) : dB2factor(1.0f, __MAX_DB); //AM depth in dB!
				break;
		}
	if ((portnum >= 0x20) && (portnum <= 0x35)) //Various flags
	{
		portnum &= 0x1F;
		adlibop[portnum].AM = (adlibregmem[0x20 + portnum] & 0x80) ? 1 : 0; //Take the AM bit!
		adlibop[portnum].ModulatorFrequencyMultiple = calcModulatorFrequencyMultiple(adlibregmem[0x20 + portnum] & 0xF); //Which harmonic to use?
		adlibop[portnum].ReleaseImmediately = (adlibregmem[0x20 + portnum] & 0x20) ? 0 : 1; //Release when not sustain until release!
	}
	else if ( (portnum >= 0x60) && (portnum <= 0x75) ) { //attack/decay
			portnum &= 0x1F;
			adlibattack[portnum] = attacktable[15- (value>>4) ]*1.006;
			adlibdecay[portnum] = decaytable[value&15];
		}
	else if ((portnum >= 0x80) && (portnum <= 0x95)) //sustain/release
	{
		portnum &= 0x1F;
		adlibsustain[portnum] = sustaintable[15 - (value >> 4)];
		adlibrelease[portnum] = decaytable[value & 15];
	}
	else if ( (portnum >= 0xA0) && (portnum <= 0xB8) ) { //octave, freq, key on
		if (portnum & 0x8) return; //Ignore A8-AF!
			portnum &= 0x7; //Only 9 channels
			if (!adlibch[portnum].keyon && ( (adlibregmem[0xB0+portnum]>>5) &1) ) {
					adlibenvstatus[adliboperators[0][portnum]] = 0;
					adlibenvstatus[adliboperators[1][portnum]] = 0;
					adlibenv[adliboperators[0][portnum]] = 0.0025;
					adlibenv[adliboperators[1][portnum]] = 0.0025;
			}
			adlibch[portnum].freq = adlibregmem[0xA0+portnum] | ( (adlibregmem[0xB0+portnum]&3) <<8);
			adlibch[portnum].convfreq = ( (double) adlibch[portnum].freq * 0.7626459);
			adlibch[portnum].keyon = (adlibregmem[0xB0+portnum]>>5) &1;
			adlibch[portnum].octave = (adlibregmem[0xB0+portnum]>>2) &7;
		}
	else if ((portnum >= 0xC0) && (portnum <= 0xC8))
	{
		portnum &= 7;
		adlibch[portnum].synthmode = (adlibregmem[0xC0 + portnum] & 1); //Save the synthesis mode!
	}
	else if ( (portnum >= 0xE0) && (portnum <= 0xF5) ) { //waveform select
			portnum &= 0x1F;
			adlibop[portnum].wavesel = value&3;
		}
}

uint8_t inadlib (uint16_t portnum) {
	if (!adlibregmem[4]) adlibstatus = 0;
	else adlibstatus = 0x80;
	adlibstatus = adlibstatus + (adlibregmem[4]&1) *0x40 + (adlibregmem[4]&2) *0x10;
	return (adlibstatus);
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

//Calculate an operator signal!
OPTINLINE char calcOperator(byte curchan, byte operator, char modulator)
{
	static uint64_t adlibstep[0x20];
	float fullstep, tempstep, frequency;
	char result;
	frequency = adlibfreq(operator, curchan); //Effective carrier init!
	if (!adlibch[curchan].synthmode) frequency += modulator; //FM using the first operator when needed!
	fullstep = usesamplerate / frequency;
	result = oplwave[adlibop[operator].wavesel&wavemask][(uint8_t)((double)adlibstep[operator]++ / ((double)fullstep / (double)256))];

	//Apply the volume envelope for operator 2!
	tempstep = adlibenv[operator]; //Current volume!
	if (tempstep > 1.0f) tempstep = 1.0f; //Limit volume to 100%!
	result *= tempstep;

	if (adlibstep[operator] > fullstep) adlibstep[operator] = 0; //Make sure we don't overflow!

	return result; //Give the result!
}

OPTINLINE char adlibsample (uint8_t curchan) {
	char modulatorresult, result; //The operator result and the final result!

	if (curchan >= NUMITEMS(adlibch)) return 0; //No sample with invalid channel!
	if (adlibpercussion && (curchan >= 6) && (curchan <= 8)) //We're percussion?
	{
		return 0; //Percussion isn't supported yet!
	}

	//Determine the type of modulation!
	//Operator 1!
	modulatorresult = calcOperator(curchan, adliboperators[0][curchan], 0); //Calculate the modulator!
	//Operator 2!
	result = calcOperator(curchan, adliboperators[1][curchan], modulatorresult); //Calculate the carrier with applied modulator if needed!

	//Perform additive synthesis when asked to do so.
	if (adlibch[curchan].synthmode) result += modulatorresult; //Perform additive synthesis when needed!
	return result; //Give the result!
}

OPTINLINE char adlibgensample() {
	uint8_t curchan;
	char adlibaccum;
	adlibaccum = 0;
	for (curchan = 0; curchan < 9; curchan++)
	{
		if (adlibfreq(-1,curchan) != 0)
		{
			adlibaccum += adlibsample(curchan);
		}
	}
	return (adlibaccum);
}

void tickadlib() {
	uint8_t curchan;
	for (curchan = 0; curchan<0x16; curchan++)
	{
		if (adlibfreq(-1,adliboperatorsreverse[curchan]) != 0)
		{
			if (adlibenvstatus[curchan])
			{
				if (adlibenvstatus[curchan]>=2) //Sustaining/releasing?
				{
					dosustain: //Entered sustain phase!
					if ((!adlibch[adliboperatorsreverse[curchan]].keyon || adlibop[curchan].ReleaseImmediately) && (adlibenvstatus[curchan] == 2)) //Release entered when sustaining?
					{
						adlibenvstatus[curchan] = 3; //Releasing!
					}
					if (adlibenvstatus[curchan] == 3) //Releasing?
					{
						adlibenv[curchan] *= adlibrelease[curchan]; //Release!
					}
				}
				else if (adlibenv[curchan]>adlibsustain[curchan]) //Decay?
				{
					adlibenv[curchan] *= adlibdecay[curchan]; //Decay!
				}
				else
				{
					adlibenv[curchan] = adlibsustain[curchan]; //Sustain level!
					adlibenvstatus[curchan] = 2; //Enter sustain phase!
					goto dosustain; //Do sustain!
				}
			}
			else
			{
				adlibenv[curchan] *= adlibattack[curchan]; //Attack!
				if (adlibenv[curchan] >= 1.0) adlibenvstatus[curchan] = 1; //Enter decay phase!
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
			if (adlibfreq (-1,curchan) !=0) {
				filled = 1; //We're filled!
				break; //Stop searching!
			}
		}
	
	if (!filled) //Not filled?
	{
		return 0; //Unused channel!
	}
	
	uint_32 c;
	c = length; //Init c!
	char *data_mono;
	data_mono = (char *)buf; //The data in correct samples!
	for (;;) //Fill it!
	{
		//Left and right are the same!
		*data_mono++ = adlibgensample(); //Generate a mono sample!
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
		adlibch[i].keyon = 0; //Initialise key on!
		adlibenvstatus[adliboperators[0][i]] = 3; //Initialise to unused!
		adlibenvstatus[adliboperators[1][i]] = 3; //Initialise to unused!
	}

	//All input!
	AMDepth = dB2factor(1.0f, __MAX_DB);

	if (__SOUND_ADLIB)
	{
		if (!addchannel(&adlib_soundGenerator,NULL,"Adlib",usesamplerate,0,0,SMPL8S)) //Start the sound emulation (mono) with automatic samples buffer?
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