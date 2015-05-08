#ifndef __ADSR_H
#define __ADSR_H

#include "headers/types.h" //Basic types!
#include "headers/support/sf2.h" //SF2 support!

//All statuses for an MIDI ADSR!
#define ADSR_IDLE 0x00
#define ADSR_DELAY 0x01
#define ADSR_ATTACK 0x02
#define ADSR_HOLD 0x03
#define ADSR_DECAY 0x04
#define ADSR_SUSTAIN 0x05
#define ADSR_RELEASE 0x06

//Convert cents to samples to increase (instead of 1 sample/sample). Floating point number (between 0.0+ usually?) Use this as a counter for the current samples (1.1+1.1..., so keep the rest value (1,1,1,...,0,1,1,1,...))
//The same applies to absolute and relative timecents (with absolute referring to 1 second intervals (framerate samples) and relative to the absolute value)
#define cents2samplesfactor(cents) pow(2, ((cents) / 1200))
//Convert to samples (not entire numbers, so keep them counted)!

typedef struct
{
	//ADSR
	uint_32 delay, attack, hold, decay, sustain, release, releasestart; //All lengths!
	uint_32 attackend, holdend, decayend; //End position of each of the phases, precalculated!
	uint_64 play_counter; //Current counter position!
	float attackfactor, decayfactor, sustainfactor, releasefactor;
	float ADSREnvelope; //Current ADSR envelope status!

	byte active; //Are we active?
} ADSR; //An ADSR's data!

void ADSR_init(float sampleRate, byte velocity, ADSR *adsr, RIFFHEADER *soundfont, word instrumentptrAmount, word ibag, uint_32 preset, word pbag, word delayLookup, word attackLookup, word holdLookup, word decayLookup, word sustainLookup, word releaseLookup, sword relKeynum, word keynumToEnvHoldLookup, word keynumToEnvDecayLookup); //Initialise an ADSR!
float ADSR_tick(ADSR *adsr, byte sustain, byte releasevelocity); //Tick an ADSR!
#endif