#include "headers/support/filters.h" //Our filter definitions!

void updateSoundFilter(HIGHLOWPASSFILTER *filter, byte ishighpass, float cutoff_freq, float samplerate)
{
	if (filter->isInit || (filter->cutoff_freq!=cutoff_freq) || (filter->samplerate!=samplerate) || (ishighpass!=filter->isHighPass)) //We're to update?
	{
		if ((ishighpass!=filter->isHighPass) && (filter->isInit==0)) return; //Don't allow changing filter types of running channels!
		if (ishighpass) //High-pass filter?
		{
			float RC = (1.0f / (cutoff_freq * (2.0f * (float)PI))); //RC is used multiple times, calculate once!
			filter->alpha = (RC / (RC + (1.0f / samplerate))); //Alpha value to use!
		}
		else //Low-pass filter?
		{
			float dt = (1.0f / samplerate); //DT is used multiple times, calculate once!
			filter->alpha = (dt / ((1.0f / (cutoff_freq * (2.0f * (float)PI))) + dt)); //Alpha value to use!
		}
	}
	filter->isHighPass = ishighpass; //Hi-pass filter?
	filter->cutoff_freq = cutoff_freq; //New cutoff frequency!
	filter->samplerate = samplerate; //New samplerate!
}

void initSoundFilter(HIGHLOWPASSFILTER *filter, byte ishighpass, float cutoff_freq, float samplerate)
{
	filter->isInit = 1; //We're an Init!
	filter->sound_last_result = filter->sound_last_sample = 0; //Save the first&last sample!
	updateSoundFilter(filter,ishighpass,cutoff_freq,samplerate); //Init our filter!
}

void applySoundFilter(HIGHLOWPASSFILTER *filter, float *currentsample)
{
	INLINEREGISTER float last_result;
	last_result = filter->sound_last_result; //Load the last result to process!
	if (filter->isHighPass) //High-pass filter?
	{
		last_result = filter->alpha * (last_result + *currentsample - filter->sound_last_sample);
	}
	else //Low-pass filter?
	{
		last_result += (filter->alpha*(*currentsample-last_result));
	}
	filter->sound_last_sample = *currentsample; //The last sample that was processed!
	*currentsample = filter->sound_last_result = last_result; //Give the new result!
}

void applySoundHighPassFilter(HIGHLOWPASSFILTER *filter, float *currentsample)
{
	INLINEREGISTER float last_result;
	last_result = filter->sound_last_result; //Load the last result to process!
	last_result = filter->alpha * (last_result + *currentsample - filter->sound_last_sample);
	filter->sound_last_sample = *currentsample; //The last sample that was processed!
	*currentsample = filter->sound_last_result = last_result; //Give the new result!
}

void applySoundLowPassFilter(HIGHLOWPASSFILTER *filter, float *currentsample)
{
	INLINEREGISTER float last_result;
	last_result = filter->sound_last_result; //Load the last result to process!
	last_result += (filter->alpha*(*currentsample-last_result));
	filter->sound_last_sample = *currentsample; //The last sample that was processed!
	*currentsample = filter->sound_last_result = last_result; //Give the new result!
}
