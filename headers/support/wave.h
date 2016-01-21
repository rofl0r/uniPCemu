#ifndef __WAVE_H
#define __WAVE_H

#include "headers/types.h" //Basic types!

#include "headers/packed.h" //Packed type!
typedef struct PACKED
{
	uint_32 ChunkID;
	uint_32 ChunkSize;
	uint_32 Format;
	uint_32 Subchunk1ID;
	uint_32 Subchunk1Size;
	word AudioFormat;
	word NumChannels;
	uint_32 SampleRate;
	uint_32 ByteRate;
	word BlockAlign;
	word BitsPerSample;
	uint_32 Subchunk2ID;
	uint_32 Subchunk2Size;
} WAVEHEADER;
#include "headers/endpacked.h" //End of packed type!

typedef struct
{
	FILE *f; //The file itself!
	WAVEHEADER header; //Full version of the WAVE header to be written to the file when closed!
} WAVEFILE;

WAVEFILE *createWAV(char *filename, byte channels, uint_32 samplerate);
byte writeWAVMonoSample(WAVEFILE *f, word sample);
byte writeWAVStereoSample(WAVEFILE *f, word lsample, word rsample);
void closeWAV(WAVEFILE **f);

#endif