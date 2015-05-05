#include "headers/types.h" //Basic types!
#include "headers/hardware/midi/adsr.h" //Our own typedefs!
#include "headers/support/sf2.h" //Soundfont support!

//Helper functions
OPTINLINE double dB2factor(double dB, double fMaxLevelDB)
{
	return pow(10, ((dB - fMaxLevelDB) / 20));
}

OPTINLINE double factor2dB(double factor, double fMaxLevelDB)
{
	return (fMaxLevelDB + (20 * log(factor)));
}

//ADSR itself:

void ADSR_release(ADSR *adsr, byte sustaining, byte release_velocity)
{
	if (adsr->release) //Gotten release?
	{
		adsr->ADSREnvelope -= adsr->releasefactor; //Apply factor!
		if (adsr->ADSREnvelope) return; //Not quiet yet?
	}
	adsr->ADSREnvelope = 0.0f; //Nothing to sound!
	adsr->active = ADSR_IDLE; //Return to IDLE!
}

void ADSR_sustain(ADSR *adsr, byte sustaining, byte release_velocity)
{
	if (sustaining) return; //Disable our voice when not sustaining anymore!
	//Sustain expired?
	adsr->active = ADSR_RELEASE; //Check next step!
	adsr->releasestart = adsr->play_counter; //When we start to release!
	ADSR_release(adsr, sustaining, release_velocity); //Passthrough!
}

void ADSR_decay(ADSR *adsr, byte sustaining, byte release_velocity)
{
	if (adsr->decay) //Gotten decay?
	{
		if (adsr->decayend > adsr->play_counter) //Decay busy?
		{
			adsr->ADSREnvelope -= adsr->decayfactor; //Apply factor!
			return; //Decay busy!
		}
	}
	//Decay expired?
	adsr->active = ADSR_SUSTAIN; //Check next step!
	adsr->ADSREnvelope = adsr->sustainfactor; //Apply sustain factor!
	ADSR_sustain(adsr,sustaining,release_velocity); //Passthrough!
}

void ADSR_hold(ADSR *adsr, byte sustaining, byte release_velocity)
{
	if (adsr->hold) //Gotten hold?
	{
		if (adsr->holdend > adsr->play_counter) return; //Hold busy?
	}
	//Hold expired?
	adsr->active = ADSR_DECAY; //Check next step!
	ADSR_decay(adsr,sustaining,release_velocity); //Passthrough!
}

void ADSR_attack(ADSR *adsr, byte sustaining, byte release_velocity)
{
	if (adsr->attack) //Gotten attack?
	{
		if (adsr->attackend > adsr->play_counter) //Attack busy?
		{
			adsr->ADSREnvelope += adsr->attackfactor; //Apply factor!
			if (adsr->ADSREnvelope < 1.0f) return; //Not full yet?
		}
	}
	//Attack expired?
	adsr->ADSREnvelope = 1.0f; //Make sure we're at 100%
	adsr->active = ADSR_HOLD; //Check next step!
	ADSR_hold(adsr,sustaining,release_velocity); //Passthrough!
}

void ADSR_delay(ADSR *adsr, byte sustaining, byte release_velocity)
{
	if (adsr->delay) //Gotten delay?
	{
		if (adsr->delay > adsr->play_counter) return; //Delay busy?
	}
	adsr->active = ADSR_ATTACK; //Check next step!
	ADSR_attack(adsr,sustaining,release_velocity); //Passthrough!
}

void ADSR_idle(ADSR *adsr, byte sustaining, byte release_velocity)
{
	//Idle does nothing!
}

void ADSR_init(float sampleRate, byte velocity, ADSR *adsr, RIFFHEADER *soundfont, word instrumentptrAmount, word ibag, uint_32 preset, word pbag, word delayLookup, word attackLookup, word holdLookup, word decayLookup, word sustainLookup, word releaseLookup, sword relKeynum, word keynumToEnvHoldLookup, word keynumToEnvDecayLookup) //Initialise an ADSR!
{
	sfGenList applypgen;
	sfInstGenList applyigen;

//Volume envelope information!
	uint_32 delay, attack, hold, decay, sustain, release; //All lengths!
	float attackfactor, decayfactor, sustainfactor, releasefactor, holdenvfactor, decayenvfactor;
	
//Delay
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptrAmount, ibag, delayLookup, &applyigen))
	{
		delay = applyigen.genAmount.shAmount; //Apply!
	}
	else
	{
		delay = -12000; //Default!
	}
	if (lookupSFPresetGenGlobal(soundfont, preset, pbag, delayLookup, &applypgen)) //Preset set?
	{
		delay += applypgen.genAmount.shAmount; //Apply!
	}

	//Attack
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptrAmount, ibag, attackLookup, &applyigen))
	{
		attack = applyigen.genAmount.shAmount; //Apply!
	}
	else
	{
		attack = -12000; //Default!
	}
	if (lookupSFPresetGenGlobal(soundfont, preset, pbag, attackLookup, &applypgen)) //Preset set?
	{
		attack = applypgen.genAmount.shAmount; //Apply!
	}

	//Hold
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptrAmount, ibag, holdLookup, &applyigen))
	{
		hold = applyigen.genAmount.shAmount; //Apply!
	}
	else
	{
		hold = -12000; //Default!
	}
	if (lookupSFPresetGenGlobal(soundfont, preset, pbag, holdLookup, &applypgen)) //Preset set?
	{
		hold += applypgen.genAmount.shAmount; //Apply!
	}

	//Hold factor
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptrAmount, ibag, keynumToEnvHoldLookup, &applyigen))
	{
		holdenvfactor = applyigen.genAmount.shAmount; //Apply!
	}
	else
	{
		holdenvfactor = 0; //Default!
	}
	if (lookupSFPresetGenGlobal(soundfont, preset, pbag, keynumToEnvHoldLookup, &applypgen)) //Preset set?
	{
		holdenvfactor += applypgen.genAmount.shAmount; //Apply!
	}

	//Decay
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptrAmount, ibag, decayLookup, &applyigen))
	{
		decay = applyigen.genAmount.shAmount; //Apply!
	}
	else
	{
		decay = -12000; //Default!
	}
	if (lookupSFPresetGenGlobal(soundfont, preset, pbag, decayLookup, &applypgen)) //Preset set?
	{
		decay += applypgen.genAmount.shAmount; //Apply!
	}

	//Decay factor
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptrAmount, ibag, keynumToEnvDecayLookup, &applyigen))
	{
		decayenvfactor = applyigen.genAmount.shAmount; //Apply!
	}
	else
	{
		decayenvfactor = 0; //Default!
	}
	if (lookupSFPresetGenGlobal(soundfont, preset, pbag, keynumToEnvDecayLookup, &applypgen)) //Preset set?
	{
		decayenvfactor += applypgen.genAmount.shAmount; //Apply!
	}

	//Sustain (dB)
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptrAmount, ibag, sustainLookup, &applyigen))
	{
		sustain = applyigen.genAmount.shAmount; //Apply!
	}
	else
	{
		sustain = 0; //Default!
	}
	if (lookupSFPresetGenGlobal(soundfont, preset, pbag, sustainLookup, &applypgen)) //Preset set?
	{
		sustain += applypgen.genAmount.shAmount; //Apply!
	}

	//Release (disabled!)
	if (lookupSFInstrumentGenGlobal(soundfont, instrumentptrAmount, ibag, releaseLookup, &applyigen))
	{
		release = applyigen.genAmount.shAmount; //Apply!
	}
	else
	{
		release = -12000; //Default!
	}
	if (lookupSFPresetGenGlobal(soundfont, preset, pbag, releaseLookup, &applypgen)) //Preset set?
	{
		release += applypgen.genAmount.shAmount; //Apply!
	}

	//Now, calculate the length of each interval.
	if (cents2samplesfactor((double)delay) < 0.0002f) //0.0001 sec?
	{
		delay = 0; //No delay!
	}
	else
	{
		delay = sampleRate*cents2samplesfactor((double)delay); //Calculate the ammount of samples!
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
	hold *= cents2samplesfactor((double)(holdenvfactor*relKeynum)); //Apply key number!

	if (cents2samplesfactor((double)decay) < 0.0002f) //0.0001 sec?
	{
		decay = 0; //No decay!
	}
	else
	{
		decay = sampleRate*cents2samplesfactor((double)decay); //Calculate the ammount of samples!
	}
	decay *= cents2samplesfactor((double)(decayenvfactor*relKeynum)); //Apply key number!

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
	else
	{
		attackfactor = 0.0f; //No attack factor!
	}
	//Hold does nothing!
	//Decay
	if (decay) //Gotten decay?
	{
		decayfactor = 1.0f; //From full!
		decayfactor /= decay; //Equal steps from 1.0f to 0.0f!
		if (!decayfactor) //No decay?
		{
			decay = 0; //No decay!
		}
		else
		{
			float temp;
			temp = 1; //Full volume!
			temp -= sustainfactor; //Change to sustain factor difference!
			temp /= decayfactor; //Calculate the new decay time needed to change to the sustain factor!
			decay = temp; //Load the calculated decay time!
		}
	}
	else
	{
		decayfactor = 0.0f; //No decay!
	}
	//Sustain does nothing!
	//Release
	if (release)
	{
		releasefactor = 1.0f; //From full!
		releasefactor /= release; //Equal steps from 1.0f to 0.0f!
		if (!releasefactor) //No release?
		{
			release = 0; //No release!
		}
		else
		{
			float temp;
			temp = sustainfactor; //Full volume!
			temp /= releasefactor; //Calculate the new decay time needed to change to the sustain factor!
			release = temp; //Load the calculated decay time!
		}
	}
	else
	{
		releasefactor = 0.0f; //No release!
	}

	//Apply ADSR to the voice!
	adsr->delay = delay; //Delay
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
	adsr->attackend = adsr->attack + adsr->delay;
	adsr->holdend = adsr->hold + adsr->attackend;
	adsr->decayend = adsr->decay + adsr->holdend;
	adsr->active = ADSR_DELAY; //We're starting with a delay!
	adsr->play_counter = 0; //Initialise our counter!
}

typedef void (*MIDI_STATE)(ADSR *adsr, byte sustaining, byte release_velocity); //ADSR event handlers!

float ADSR_tick(ADSR *adsr, byte sustaining, byte release_velocity) //Tick an ADSR!
{
	static MIDI_STATE ADSR_EXEC[7] = {
		ADSR_idle, ADSR_delay, //Still quiet!
		ADSR_attack, ADSR_hold, ADSR_decay, //Start
		ADSR_sustain, //Holding/sustain
		ADSR_release //Release
	}; //ADSR states!
	ADSR_EXEC[adsr->active](adsr,sustaining,release_velocity); //Execute the current ADSR!
	++adsr->play_counter; //Next position to calculate!
	return dB2factor(adsr->ADSREnvelope,1); //Give the current envelope, convert the linear factor to decibels!
}