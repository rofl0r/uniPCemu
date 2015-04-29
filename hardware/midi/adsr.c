#include "headers/types.h" //Basic types!
#include "headers/hardware/midi/adsr.h" //Our own typedefs!
#include "headers/support/sf2.h" //Soundfont support!

//Helper functions
//The same applies to absolute and relative timecents (with absolute referring to 1 second intervals (framerate samples) and relative to the absolute value)
OPTINLINE double cents2samplesfactor(double cents)
{
	return pow(2, (cents / 1200)); //Convert to samples (not entire numbers, so keep them counted)!
}

OPTINLINE double dB2factor(double dB, double fMaxLevelDB)
{
	return pow(10, ((dB - fMaxLevelDB) / 20));
}

OPTINLINE double factor2dB(double factor, double fMaxLevelDB)
{
	return (fMaxLevelDB + (20 * log(factor)));
}

//ADSR itself:

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

void ADSR_init(float sampleRate, ADSR *adsr, RIFFHEADER *soundfont, word instrumentptrAmount, word ibag, uint_32 preset, word pbag, word delayLookup, word attackLookup, word holdLookup, word decayLookup, word sustainLookup, word releaseLookup) //Initialise an ADSR!
{
//Volume envelope information!
	int_32 delaytime, attack, hold, decay, sustain, release; //All lengths!
	float attackfactor = 0.0f, decayfactor = 0.0f, sustainfactor = 0.0f, releasefactor = 0.0f;
	
//Delay
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptrAmount, ibag, delayVolEnv, &applyigen))
	{
		delaytime = applyigen.genAmount.shAmount; //Apply!
	}
	else
	{
		delaytime = -12000; //Default!
	}
	if (lookupSFPresetGenGlobal(soundfont, preset, pbag, delayVolEnv, &applypgen)) //Preset set?
	{
		delaytime += applypgen.genAmount.shAmount; //Apply!
	}

	//Attack
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, attackVolEnv, &applyigen))
	{
		attack = applyigen.genAmount.shAmount; //Apply!
	}
	else
	{
		attack = -12000; //Default!
	}
	if (lookupSFPresetGenGlobal(soundfont, preset, pbag, attackVolEnv, &applypgen)) //Preset set?
	{
		attack = applypgen.genAmount.shAmount; //Apply!
	}

	//Hold
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, holdVolEnv, &applyigen))
	{
		hold = applyigen.genAmount.shAmount; //Apply!
	}
	else
	{
		hold = -12000; //Default!
	}
	if (lookupSFPresetGenGlobal(soundfont, preset, pbag, holdVolEnv, &applypgen)) //Preset set?
	{
		hold += applypgen.genAmount.shAmount; //Apply!
	}

	//Decay
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, decayVolEnv, &applyigen))
	{
		decay = applyigen.genAmount.shAmount; //Apply!
	}
	else
	{
		decay = -12000; //Default!
	}
	if (lookupSFPresetGenGlobal(soundfont, preset, pbag, decayVolEnv, &applypgen)) //Preset set?
	{
		decay += applypgen.genAmount.shAmount; //Apply!
	}

	//Sustain (dB)
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, sustainVolEnv, &applyigen))
	{
		sustain = applyigen.genAmount.shAmount; //Apply!
	}
	else
	{
		sustain = 0; //Default!
	}
	if (lookupSFPresetGenGlobal(soundfont, preset, pbag, sustainVolEnv, &applypgen)) //Preset set?
	{
		sustain += applypgen.genAmount.shAmount; //Apply!
	}

	//Release (disabled!)
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptr.genAmount.wAmount, ibag, releaseVolEnv, &applyigen))
	{
		release = applyigen.genAmount.shAmount; //Apply!
	}
	else
	{
		release = -12000; //Default!
	}
	if (lookupSFPresetGenGlobal(soundfont, preset, pbag, releaseVolEnv, &applypgen)) //Preset set?
	{
		release += applypgen.genAmount.shAmount; //Apply!
	}

	//Now, calculate the length of each interval.
	if (cents2samplesfactor((double)delaytime) < 0.0002f) //0.0001 sec?
	{
		delaytime = 0; //No delay!
	}
	else
	{
		delaytime = sampleRate*cents2samplesfactor((double)delaytime); //Calculate the ammount of samples!
	}
	if (cents2samplesfactor((double)attack) < 0.0002f) //0.0001 sec?
	{
		attack = 0; //No attack!
	}
	else
	{
		attack = sampleRate*cents2samplesfactor((double)attack); //Calculate the ammount of samples!
	}
	if (cents2samplesfactor((double)hold) < 0.0002f) //0.0001 sec?
	{
		hold = 0; //No hold!
	}
	else
	{
		hold = sampleRate*cents2samplesfactor((double)hold); //Calculate the ammount of samples!
	}
	if (cents2samplesfactor((double)decay) < 0.0002f) //0.0001 sec?
	{
		decay = 0; //No decay!
	}
	else
	{
		decay = sampleRate*cents2samplesfactor((double)decay); //Calculate the ammount of samples!
	}
	sustainfactor = dB2factor((double)(1000 - sustain), 1000); //We're on a rate of 1000 cb!
	if (sustainfactor > 1.0f) sustainfactor = 1.0f; //Limit of 100%!
	if (cents2samplesfactor((double)release) < 0.0002f) //0.0001 sec?
	{
		release = 0; //No release!
	}
	else
	{
		release = sampleRate*cents2samplesfactor((double)release); //Calculate the ammount of samples!
	}
	//Now calculate the steps for the envelope!
	//Delay does nothing!
	//Attack!
	if (attack) //Gotten attack?
	{
		attackfactor = 1.0f;
		attackfactor /= attack; //Equal steps from 0 to 1.0f!
		if (!attackfactor)
		{
			attack = 0; //No attack!
		}
	}
	//Hold does nothing!
	//Decay
	if (decay) //Gotten decay?
	{
		decayfactor = 1.0f; //From full!
		decayfactor -= sustainfactor; //We're going to sustain!
		decayfactor /= decay; //Equal steps from 1.0f to sustain!
		if (!decayfactor) //No decay?
		{
			decay = 0; //No decay!
		}
	}
	//Sustain does nothing!
	//Release
	if (release)
	{
		releasefactor = sustainfactor; //From sustain!
		releasefactor /= release; //Equal steps from sustain to 0!
		if (!releasefactor) //No release?
		{
			release = 0; //No release!
		}
	}

	//Apply ADSR to the voice!
	adsr->delaytime = delaytime; //Delay
	adsr->attack = attack; //Attack
	adsr->attackfactor = attackfactor;
	adsr->hold = hold; //Hold
	adsr->decay = decay; //Decay
	adsr->decayfactor = decayfactor;
	adsr->sustain = sustain; //Sustain
	adsr->sustainfactor = sustainfactor; //Sustain %
	adsr->release = release; //Release
	adsr->releasefactor = releasefactor;

	//Finally calculate the actual values needed!
	adsr->attackend = adsr->attack + adsr->delaytime;
	adsr->holdend = adsr->hold + adsr->attack + adsr->delaytime;
	adsr->decayend = adsr->decay + adsr->hold + adsr->attack + adsr->delaytime;
}

float ADSR_tick(ADSR *adsr) //Tick and ADSR!
{
	static MIDI_ADSR ADSR[7] = {
		ADSR_idle, ADSR_delay, //Still quiet!
		ADSR_attack, ADSR_hold, ADSR_decay, //Start
		ADSR_sustain, //Holding/sustain
		ADSR_release //Release
	}; //ADSR states!
	ADSR[adsr->active](adsr); //Execute the current ADSR!
	++adsr->play_counter; //Next position to calculate!
	return adsr->ADSREnvelope; //Give the current envelope!
}