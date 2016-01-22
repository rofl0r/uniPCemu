#include "headers/types.h"
#include "headers/support/wave.h" //Wave file structures etc.
#include "headers/support/zalloc.h" //Allocation support!

#define RIFF_RIFF 0x46464952
#define RIFF_WAVE 0x45564157
#define RIFF_FMT 0x20746d66
#define RIFF_DATA 0x61746164

byte writeWord(FILE *f, word w) //INTERNAL: Channels are interleaved, channel 0 left, channel 0 right, channel 1 left, channel 1 right etc.
{
	return (fwrite(&w, 1, sizeof(w), f) == sizeof(w)); //Written?
}

byte writeDWord(FILE *f, uint_32 d) //INTERNAL
{
	return (fwrite(&d, 1, sizeof(d), f) == sizeof(d)); //Written?
}

byte writeWAVMonoSample(WAVEFILE *f, word sample)
{
	return writeWord(f->f, sample); //Write the sample!
}

byte writeWAVStereoSample(WAVEFILE *f, word lsample, word rsample)
{
	byte result;
	result = writeWord(f->f, lsample); //Write the left sample!
	if (!result) return 0; //Error!
	return writeWord(f->f, rsample); //Write the right sample!
}

void WAVdealloc(void **ptr, uint_32 size, SDL_sem *lock)
{
	if (lock) WaitSem(lock)
	WAVEFILE **f;
	WAVEFILE *f2;
	uint_32 finalposition; //Final data position!
	f = (WAVEFILE **)ptr; //The wave file pointer
	if (f) //valid?
	{
		f2 = *f; //Get the pointer value!
		if (f2) //Valid pointer?
		{
			if (f2->f) //Valid file?
			{
				//Update WAVE file data!
				finalposition = ftell(f2->f); //Final position!
				if (finalposition == sizeof(f2->header)) //Empty file?
				{
					if (f2->header.NumChannels == 2) //Stereo?
					{
						writeWAVStereoSample(f2,0,0); //Stereo empty sample!
					}
					else
					{
						writeWAVMonoSample(f2, 0); //Mono empty sample!
					}
					finalposition = ftell(f2->f); //Update final position!
				}
				f2->header.Subchunk2Size = finalposition - sizeof(f2->header); //Update data size!
				f2->header.ChunkSize = finalposition - 8; //Update WaveFmt chunk size
				fseek(f2->f,0,SEEK_SET); //Goto BOF to update the header!
				fwrite(&f2->header,1,sizeof(f2->header),f2->f); //Overwrite the file's header!
				//Finally, close the file!
				fclose(f2->f); //Close the file!
				if (finalposition == sizeof(f2->header)) //Empty file?
				{
					remove(f2->filename); //Remove the file: it's invalid!
				}
				f2->f = NULL; //Not allocated anymore!
			}
		}
	}
	DEALLOCFUNC defaultdealloc = getdefaultdealloc(); //Default deallocation function!
	defaultdealloc(ptr,size,NULL); //Release the pointer normally by direct deallocation!
	if (lock) PostSem(lock)
}

WAVEFILE *createWAV(char *filename, byte channels, uint_32 samplerate)
{
	WAVEFILE *f;
	f = zalloc(sizeof(WAVEFILE),"WAVEFILE",NULL); //Allocate the structure for processing!
	if (!f) return NULL; //Cannot allocate structure!
	if (!changedealloc(f, sizeof(WAVEFILE), &WAVdealloc)) //Failed to register our deallocation function?
	{
		freez((void **)&f, sizeof(WAVEFILE), "WAVEFILE"); //Free the file!
		return NULL; //Failed to unregister!
	}
	//Create basic header!
	f->header.ChunkID = RIFF_RIFF; //RIFF chunk start!
	f->header.ChunkSize = sizeof(f->header)-12; //Still empty!
	f->header.Format = RIFF_WAVE; //We're a WAVE file!

	f->header.Subchunk1ID = RIFF_FMT; //Format chunk start!
	f->header.Subchunk1Size = 16; //We're a basic format chunk with 16 bytes of data!
	f->header.AudioFormat = 1; //We're PCM format, uncompressed!
	f->header.NumChannels = channels; //Our number of channels!
	f->header.SampleRate = samplerate; //Our sample rate!
	f->header.ByteRate = (samplerate * channels * 16) / 8; //Byte rate per second
	f->header.BlockAlign = (channels*16)/8; //Size of one sample!
	f->header.BitsPerSample = 16; //Bits per sample!

	f->header.Subchunk2ID = RIFF_DATA; //DATA chunk start!
	f->header.Subchunk2Size = 0; //We don't have any data recorded yet, so 0 bytes atm!
	strcpy(f->filename,filename); //Set the filename to be removed if empty!
	f->f = fopen(filename, "wb+"); //Open the WAV file!
	if (fwrite(&f->header, 1, sizeof(f->header), f->f) != sizeof(f->header)) //Failed to write the header?
	{
		freez((void **)&f, sizeof(WAVEFILE), "WAVEFILE"); //Free the file!
		return NULL; //Failed to unregister!
	}

	return f; //Give the started file!
}

void closeWAV(WAVEFILE **f)
{
	freez((void **)f,sizeof(WAVEFILE),"closeWAV"); //Save and close the file, taking advantage of the auto-cleanup feature of zalloc!
}