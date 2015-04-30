#ifndef __ADSR_H
#define __ADSR_H

#include "headers/types.h" //Basic types!

//All statuses for an MIDI ADSR!
#define MIDISTATUS_IDLE 0x00
#define MIDISTATUS_DELAY 0x01
#define MIDISTATUS_ATTACK 0x02
#define MIDISTATUS_HOLD 0x03
#define MIDISTATUS_DECAY 0x04
#define MIDISTATUS_SUSTAIN 0x05
#define MIDISTATUS_RELEASE 0x06

typedef struct
{
	byte active; //Are we active?
	
	//ADSR
	int_32 delaytime, attack, hold, decay, sustain, release, releasestart; //All lengths!
	float attackfactor, decayfactor, sustainfactor, releasefactor;
	float ADSREnvelope; //Current ADSR envelope status!

	uint_32 attackend, holdend, decayend; //End position of each of the phases, precalculated!
	uint_64 play_counter; //Current counter position!
} ADSR; //An ADSR's data!

void ADSR_init(float sampleRate, ADSR *adsr, RIFFHEADER *soundfont, word instrumentptrAmount, word ibag, uint_32 preset, word pbag, word delayLookup, word attackLookup, word holdLookup, word decayLookup, word sustainLookup, word releaseLookup, sword relKeynum, word keynumToEnvHoldLookup, word keynumToEnvDecayLookup); //Initialise an ADSR!
float ADSR_tick(ADSR *adsr, byte sustain); //Tick an ADSR!
#endif