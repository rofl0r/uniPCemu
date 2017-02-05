#ifndef FILTER_H
#define FILTER_H

#include "headers/types.h" //Basic types!

typedef struct
{
	byte isInit; //New uninitialized filter?
	float sound_last_result; //Last result!
	float sound_last_sample; //Last sample!

	float alpha; //Solid value that doesn't change for the filter, until the filter is updated!

	//General filter information and settings set for the filter!
	byte isHighPass;
	float cutoff_freq;
	float samplerate;
} HIGHLOWPASSFILTER; //High or low pass filter!


#define applySoundLowPassFilterObj(filter,currentsample) currentsample = filter.sound_last_result = filter.sound_last_result+(filter.alpha*(currentsample-filter.sound_last_result))

#define applySoundHighPassFilterObj(filter,currentsample,last_resulttmp) last_resulttmp = filter.sound_last_result; \ //Load the last result to process!
	last_resulttmp = filter.alpha * (last_resulttmp + currentsample - filter.sound_last_sample); \
	filter.sound_last_sample = currentsample; \ //The last sample that was processed!
	currentsample = filter.sound_last_result = last_resulttmp; //Give the new result!

//Global high and low pass filters support!
void initSoundFilter(HIGHLOWPASSFILTER *filter, byte ishighpass, float cutoff_freq, float samplerate); //Initialize the filter!
void updateSoundFilter(HIGHLOWPASSFILTER *filter, byte ishighpass, float cutoff_freq, float samplerate); //Update the filter information/type!
void applySoundHighPassFilter(HIGHLOWPASSFILTER *filter, float *currentsample); //Apply the filter to a sample stream!
void applySoundLowPassFilter(HIGHLOWPASSFILTER *filter, float *currentsample); //Apply the filter to a sample stream!
void applySoundFilter(HIGHLOWPASSFILTER *filter, float *currentsample); //Apply the filter to a sample stream!

#endif
