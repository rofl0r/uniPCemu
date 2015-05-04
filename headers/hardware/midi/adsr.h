#ifndef __ADSR_H
#define __ADSR_H

#include "headers/types.h" //Basic types!
#include "headers/support/sf2.h" //SF2 support!

//All statuses for an MIDI ADSR!
#define ADSR_IDLE 0x00
#define ADSR_DELAY 0x01
#define ADSR_ATTACK 0x02
#define ADSR_HOLD 0x03
#define ADSR_DECAY 0x05
#define ADSR_SUSTAIN 0x05
#define ADSR_RELEASE 0x00

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