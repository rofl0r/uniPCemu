#include "headers/support/filters.h" //Our filter definitions!

void updateSoundFilter(HIGHLOWPASSFILTER *filter, byte ishighpass, float cutoff_freq, float samplerate)
{
	filter->isHighPass = ishighpass; //Highpass filter?
	if (filter->isInit || (filter->cutoff_freq!=cutoff_freq) || (filter->samplerate!=samplerate) || (ishighpass!=filter->isHighPass)) //We're to update?
	{
		if (ishighpass) //High-pass filter?
		{
			float RC = (1.0f / (cutoff_freq * (2.0f * (float)PI))); //RC is used multiple times, calculate once!
			filter->solid = (RC / (RC + (1.0f / samplerate))); //Solid value to use!
		}
		else //Low-pass filter?
		{
			float dt = (1.0f / samplerate); //DT is used multiple times, calculate once!
			filter->solid = (dt / ((1.0f / (cutoff_freq * (2.0f * (float)PI))) + dt)); //Solid value to use!
		}
	}
	filter->isHighPass = ishighpass; //Hi-pass filter?
	filter->cutoff_freq = cutoff_freq; //New cutoff frequency!
	filter->samplerate = samplerate; //New samplerate!
}

void initSoundFilter(HIGHLOWPASSFILTER *filter, byte ishighpass, float cutoff_freq, float samplerate)
{
	filter->isInit = 1; //We're an Init!
	filter->isFirstSample = 1; //We're the first sample!
	updateSoundFilter(filter,ishighpass,cutoff_freq,samplerate); //Init our filter!
}

void applySoundFilter(HIGHLOWPASSFILTER *filter, float *currentsample)
{
	INLINEREGISTER float last_result;
	if (filter->isFirstSample) //No last? Only executed once when starting playback!
	{
		filter->sound_last_result = filter->sound_last_sample = *currentsample; //Save the current sample!
		filter->isFirstSample = 0; //Not the first sample anymore!
		return; //Abort: don't filter the first sample!
	}
	last_result = filter->sound_last_result; //Load the last result to process!
	if (filter->isHighPass) //High-pass filter?
	{
		last_result = filter->solid * (last_result + *currentsample - filter->sound_last_sample);
	}
	else //Low-pass filter?
	{
		last_result += (filter->solid*(*currentsample-last_result));
	}
	filter->sound_last_sample = *currentsample; //The last sample that was processed!
	*currentsample = filter->sound_last_result = last_result; //Give the new result!
}