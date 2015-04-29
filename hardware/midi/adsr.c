#include "headers/types.h" //Basic types!
#include "headers/hardware/midi/adsr.h" //Our own typedefs!

void ADSR_release(ADSR *adsr)
{
	if (adsr->release) //Gotten release?
	{
		adsr->ADSREnvelope -= voice->releasefactor; //Apply factor!
		if (adsr->ADSREnvelope>0.0f) return; //Not quiet yet?
	}
	adsr->ADSREnvelope = 0.0f; //Nothing to sound!
	adsr->active = MIDISTATUS_IDLE; //Return to IDLE!
}

void ADSR_sustain(ADSR *voice)
{
	/*if (adsr->sustain) //Gotten sustain?
	{*/
	if ((adsr->channel->sustain) || ((adsr->currentloopflags & 0xC0) != 0x80)) //Disable our voice when not sustaining anymore!
	{
		return; //Sustaining!
	}
	//}
	//Sustain expired?
	adsr->active = MIDISTATUS_RELEASE; //Check next step!
	adsr->releasestart = adsr->play_counter; //When we start to release!
	ADSR_release(adsr); //Passthrough!
}

void ADSR_decay(ADSR *adsr)
{
	if (adsr->decay) //Gotten decay?
	{
		if (adsr->decayend > adsr->play_counter) //Decay busy?
		{
			adsr->ADSREnvelope -= adsr->decayfactor; //Apply factor!
			return; //Hold!
		}
	}
	//Decay expired?
	adsr->active = MIDISTATUS_SUSTAIN; //Check next step!
	adsr->ADSREnvelope = adsr->sustainfactor; //Apply sustain factor!
	ADSR_sustain(adsr); //Passthrough!
}

void ADSR_hold(ADSR *adsr)
{
	if (adsr->hold) //Gotten hold?
	{
		if (adsr->holdend > adsr->play_counter) //Hold busy?
		{
			return; //Hold!
		}
	}
	//Hold expired?
	adsr->active = MIDISTATUS_DECAY; //Check next step!
	ADSR_decay(adsr); //Passthrough!
}

void ADSR_attack(ADSR *adsr)
{
	if (adsr->attack) //Gotten attack?
	{
		if (adsr->attackend > adsr->play_counter) //Attack busy?
		{
			adsr->ADSREnvelope += adsr->attackfactor; //Apply factor!
			return;
		}
	}
	//Attack expired?
	adsr->ADSREnvelope = 1.0f; //Make sure we're at 100%
	adsr->active = MIDISTATUS_HOLD; //Check next step!
	ADSR_hold(adsr); //Passthrough!
}

void ADSR_delay(ADSR *adsr)
{
	if (adsr->delaytime) //Gotten delay?
	{
		if (adsr->delaytime > adsr->play_counter) //Delay busy?
		{
			return; //Normal delay!
		}
	}
	adsr->active = MIDISTATUS_ATTACK; //Check next step!
	ADSR_attack(adsr); //Passthrough!
}

void ADSR_idle(ADSR *adsr)
{
	//Idle does nothing!
}

static MIDI_ADSR ADSR[7] = {
	ADSR_idle, ADSR_delay, //Still quiet!
	ADSR_attack, ADSR_hold, ADSR_decay, //Start
	ADSR_sustain, //Holding/sustain
	ADSR_release //Release
}; //ADSR states!

void ADSR_init(ADSR *adsr) //Initialise an ADSR!
{
}

float ADSR_tick(ADSR *adsr) //Tick and ADSR!
{
}