#include "headers/types.h" //Basic types!
#include "headers/hardware/midi/adsr.h" //Our own typedefs!

void ADSR_release(ADSR *voice)
{
	if (voice->release) //Gotten release?
	{
		voice->ADSREnvelope -= voice->releasefactor; //Apply factor!
		if (voice->ADSREnvelope>0.0f) return; //Not quiet yet?
	}
	voice->ADSREnvelope = 0.0f; //Nothing to sound!
	voice->active = MIDISTATUS_IDLE; //Return to IDLE!
}

void ADSR_sustain(ADSR *voice)
{
	/*if (voice->sustain) //Gotten sustain?
	{*/
	if ((voice->channel->sustain) || ((voice->currentloopflags & 0xC0) != 0x80)) //Disable our voice when not sustaining anymore!
	{
		return; //Sustaining!
	}
	//}
	//Sustain expired?
	voice->active = MIDISTATUS_RELEASE; //Check next step!
	voice->releasestart = voice->play_counter; //When we start to release!
	ADSR_release(voice); //Passthrough!
}

void ADSR_decay(ADSR *voice)
{
	if (voice->decay) //Gotten decay?
	{
		if (voice->decayend > voice->play_counter) //Decay busy?
		{
			voice->ADSREnvelope -= voice->decayfactor; //Apply factor!
			return; //Hold!
		}
	}
	//Decay expired?
	voice->active = MIDISTATUS_SUSTAIN; //Check next step!
	voice->ADSREnvelope = voice->sustainfactor; //Apply sustain factor!
	ADSR_sustain(voice); //Passthrough!
}

void ADSR_hold(ADSR *voice)
{
	if (voice->hold) //Gotten hold?
	{
		if (voice->holdend > voice->play_counter) //Hold busy?
		{
			return; //Hold!
		}
	}
	//Hold expired?
	voice->active = MIDISTATUS_DECAY; //Check next step!
	ADSR_decay(voice); //Passthrough!
}

void ADSR_attack(ADSR *voice)
{
	if (voice->attack) //Gotten attack?
	{
		if (voice->attackend > voice->play_counter) //Attack busy?
		{
			voice->ADSREnvelope += voice->attackfactor; //Apply factor!
			return;
		}
	}
	//Attack expired?
	voice->ADSREnvelope = 1.0f; //Make sure we're at 100%
	voice->active = MIDISTATUS_HOLD; //Check next step!
	ADSR_hold(voice); //Passthrough!
}

void ADSR_delay(ADSR *voice)
{
	if (voice->delaytime) //Gotten delay?
	{
		if (voice->delaytime > voice->play_counter) //Delay busy?
		{
			return; //Normal delay!
		}
	}
	voice->active = MIDISTATUS_ATTACK; //Check next step!
	ADSR_attack(voice); //Passthrough!
}

void ADSR_idle(ADSR *voice)
{
	//Idle does nothing!
}

static MIDI_ADSR ADSR[7] = {
	ADSR_idle, ADSR_delay, //Still quiet!
	ADSR_attack, ADSR_hold, ADSR_decay, //Start
	ADSR_sustain, //Holding/sustain
	ADSR_release //Release
}; //ADSR states!