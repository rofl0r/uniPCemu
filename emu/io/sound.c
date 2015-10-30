//Our audio includes!
#include "headers/types.h" //Basic types!
#include "headers/emu/sound.h" //Sound comp.
#include "headers/support/zalloc.h" //Zero allocation!
#include "headers/support/log.h" //Logging support!
#include "headers/support/highrestimer.h" //High resolution clock for timing checks.
#include "headers/support/signedness.h" //Signedness support!

//Hardware set sample rate
#define HW_SAMPLERATE 44100

//Are we disabled?
#define __HW_DISABLED 0
//How many samples to process at once? Originally 2048; 64=Optimum
#define SAMPLESIZE 2048
//Maximum samplerate in Hertz (200KHz)
#define MAX_SAMPLERATE 50000.0f
//Enable below if debugging speed is to be enabled.
//#define DEBUG_SOUNDSPEED
//Enable below if debugging buffering is to be enabled.
//#define DEBUG_SOUNDBUFFER
//Same as speed, but for allocations themselves.
//#define DEBUG_SOUNDALLOC

//Use external timing synchronization?
//#define EXTERNAL_TIMING

//#define __USE_EQUALIZER

typedef struct
{
	void *samples; //All samples!
	uint_32 length;		/* size of sound data in bytes */
	uint_32 position;		/* the position in the sound data in bytes */
	uint_32 numsamples; //Ammount of samples in samples.
} sound_t, *sound_p;

/* Structure for a currently playing sound. */
typedef struct
{
	//All our sound data
	sound_t sound;              /* sound data to play from the channel, loaded by the sound handler */
	SOUNDHANDLER soundhandler; //For filling the buffer!

	//Currently executing position
	word position;            /* current position in the sound buffer */
	
	//Rest channel data
	float volume; //The volume!
	float volume_percent; //The volume, in percent (1/volume).
	float samplerate; //The sample rate!
	float convert_samplerate; //Conversion from hardware samplerate to used samplerate!
	uint_32 bufferinc; //The increase in the buffer (how much position decreases during a buffering)
	byte stereo; //Mono stream or stereo stream? 1=Stereo stream, else mono.
	void *extradata; //Extra data for the handler!
	char name[256]; //A short name!
	byte samplemethod; //The method used to decode samples!
	byte bufferflags; //Special flags about the buffer from the soundhandler function!
	//Fill and processbuffer to use!
	void *fillbuffer; //Fillbuffer function to use!
	void *processbuffer; //Channelbuffer function to use (determined by fillbuffer function)!
} playing_t, *playing_p;

byte audioticksready = 0; //Default: not ready yet!
TicksHolder audioticks;

//Our calls for data buffering and processing.
typedef uint_32 (*fillbuffer_call)(playing_p currentchannel, uint_32 *relsample, uint_32 currentpos);
typedef void (*processbuffer_call)(playing_p currentchannel, int_32 *result_l, int_32 *result_r, uint_32 relsample);

uint_32 fillbuffer_new(playing_p currentchannel, uint_32 *relsample, uint_32 currentpos); //New fillbuffer call (for new channels)!

uint_32 soundchannels_used = 0; //Ammount of used sound channels to check, increases as the ammount of entries increase!
playing_t soundchannels[1000]; //All soundchannels!

SDL_AudioSpec audiospecs; //Our requested and obtained audio specifications.

//The sample rate to render at:
#define SW_SAMPLERATE (float)audiospecs.freq

//Sample position precalcs!
uint_32 *samplepos[2]; //Sample positions for mono and stereo channels!
uint_32 samplepos_size; //Size of the sample position precalcs (both of them)

//Default samplerate is HW_SAMPLERATE, Real samplerate is SW_SAMPLERATE

word audiolocklvl = 0; //Audio lock level!

void lockaudio()
{
	if (!audiolocklvl) //Root level?
	{
		if (SDL_WasInit(SDL_INIT_AUDIO)) //Using SDL and audio enabled?
		{
			SDL_LockAudio(); //Lock the audio!
		}
	}
	++audiolocklvl; //Increase the lock level!
}

void unlockaudio()
{
	--audiolocklvl; //Decrease the lock level!
	if (!audiolocklvl) //Root level?
	{
		//We're unlocking!
		if (SDL_WasInit(SDL_INIT_AUDIO)) //SDL loaded and audio enabled?
		{
			SDL_UnlockAudio(); //Unlock the audio!
		}
	}
}

#define C_CALCSAMPLEPOS(rchannel,stereo,time) ((time*(1<<stereo)) + (stereo*rchannel))
OPTINLINE void calc_samplePos() //Calculate sample position precalcs!
{
	if (samplepos[0] && samplepos[1]) return; //Don't reallocate!
	uint_32 precalcs_size = ((uint_32)MAX_SAMPLERATE*sizeof(*samplepos[0])<<1); //Size of samplepos precalcs!
	//Now allocate the precalcs!
	samplepos[0] = (uint_32 *)zalloc(precalcs_size,"Sample position precalcs",NULL);
	samplepos[1] = (uint_32 *)zalloc(precalcs_size,"Sample position precalcs",NULL);
	byte abort;
	abort = 0; //Default: no abort!
	if (!samplepos[1])
	{
		abort = 1; //1 abort!
	}
	if (!samplepos[0])
	{
		abort |= 2; //2 abort!
	}
	if (abort) //Aborted?
	{
		if (abort&1)
		{
			freez((void **)&samplepos[1],precalcs_size,"Sample position precalcs");
		}
		if (abort&2)
		{
			freez((void **)&samplepos[0],precalcs_size,"Sample position precalcs");
		}
		//Aborted: ran out of memory!
		return; //Abort!
	}
	
	//Now calculate the samplepos precalcs!
	
	uint_32 time = 0;
	uint_32 time_limit = (uint_32)MAX_SAMPLERATE; //The limit!
	//byte stereo; //Whether stereo or not!
	for (;;)
	{
		samplepos[0][(time<<1)|0] = C_CALCSAMPLEPOS(0,0,time); //Precalculate left channel indexes!
		samplepos[0][(time<<1)|1] = C_CALCSAMPLEPOS(1,0,time); //Precalculate right channel indexes!
		samplepos[1][(time<<1)|0] = C_CALCSAMPLEPOS(0,1,time); //Precalculate left channel indexes!
		samplepos[1][(time<<1)|1] = C_CALCSAMPLEPOS(1,1,time); //Precalculate right channel indexes!
		if (++time>=time_limit) break; //Next time position!
	}
	
	samplepos_size = precalcs_size; //The size of the samplepos precalcs!
}

OPTINLINE void free_samplePos()
{
	byte stereo;
	for (stereo=0;stereo<2;stereo++) //All sample positions!
	{
		if (samplepos[stereo]) //Loaded?
		{
			freez((void **)&samplepos[stereo],samplepos_size,"Sample position precalcs");
		}
	}
	if (!samplepos[0] && !samplepos[1]) //Both freed?
	{
		samplepos_size = 0; //No size anymore: we're freed!
	}
}

byte setStereo(SOUNDHANDLER handler, void *extradata, byte stereo) //Channel&Volume(100.0f=100%)
{
	if (__HW_DISABLED) return 0; //Disabled?
	uint_32 n;
	for (n=0;(n<soundchannels_used);n++) //Check all!
	{
		if ((soundchannels[n].soundhandler==handler) && (soundchannels[n].extradata==extradata)) //Found?
		{
			lockaudio();
			soundchannels[n].stereo = stereo; //Are we a stereo channel?
			unlockaudio(); //Unlock the audio!
			return 1; //Done: check no more!
		}
	}
	return 0; //Not found!
}

/*void releaseSamplerate(word rate) //Release a samplerate precalcs if needed!
{
	lockaudio(); //Lock the audio!
	if (samplerate_used[rate]) //Still used?
	{
		--samplerate_used[rate]; //Decrease the ammount of users for this samplerate!
	}
	if (!samplerate_used[rate] && convert_samplerate[rate]) //Not used anymore, but still allocated?
	{
		freez((void **)&convert_samplerate[rate],convert_sampleratesize[rate],"Samplerate precalcs"); //Release the sample rate!
		if (!convert_samplerate[rate]) //Not allocated anymore?
		{
			convert_sampleratesize[rate] = 0; //Release the size, setting it to unused!
		}
	}
	unlockaudio(); //Unlock the audio: we're done!
}*/

//Conversion of samplerate!
//#define C_CONVERTSAMPLERATE(t,samplerate) (word)(SAFEDIV(t,(float)SW_SAMPLERATE)*samplerate)

byte setSampleRate(SOUNDHANDLER handler, void *extradata, float rate)
{
	if (__HW_DISABLED) return 0; //Disabled?
	//dolog("soundservice","setSampleRate: %f",rate);
	uint_32 n;
	lockaudio(); //Lock the audio!
	for (n=0;n<soundchannels_used;n++) //Check all!
	{
		if (soundchannels[n].soundhandler && (soundchannels[n].soundhandler==handler) && (soundchannels[n].extradata==extradata)) //Found?
		{
			
			/*uint_32 old_samplerate = soundchannels[n].samplerate; //Old samplerate!
			uint_32 samplerate = rate;
			if (!samplerate) //Rate not specified?
			{
				 samplerate = SW_SAMPLERATE; //Default: hardware samplerate!
			}
			soundchannels[n].samplerate = samplerate; //Save the actual samplerate used!
			
			if (samplerate>MAX_SAMPLERATE) //Overflow?
			{
				soundchannels[n].samplerate = 0; //Disabled!
				dolog("soundservice","Samplerate overflow: %i",samplerate); //Invalid samplerate!
				unlockaudio(); //Unlock the audio: we're done!
				return 0; //Error: invalid samplerate!
			}
			
			if (convert_samplerate[rate]) //Already allocated?
			{
				if (rate!=old_samplerate) //Changed?
				{
					++samplerate_used[rate]; //Increase the ammount of users of this samplerate!
				}
				goto ready; 
			}
			
			//We need to be allocated!
			word realrate = soundchannels[n].samplerate; //Final sample rate!
			convert_samplerate[realrate] = zalloc(realrate*sizeof(*convert_samplerate[realrate]),"Samplerate precalcs"); //Allocate double the maximum sample rate!
			if (!convert_samplerate[rate]) //Error allocating the samplerate precalcs?
			{
				dolog("soundservice","Allocating sample rate failed.");
				soundchannels[n].samplerate = 0; //No samplerate available!
				
				if (old_samplerate) //Old samplerate was set?
				{
					releaseSamplerate(old_samplerate); //Release the old samplerate!
				}
				unlockaudio(); //Unlock the audio!
				return 0; //Error: couldn't allocate the samplerate precalcs!
			}
			
			//Samplerate allocated, so fill the samplerate precalcs!
			
			//First, release the old samplerate!
			if (old_samplerate!=samplerate) //Different old samplerate? We've changed samplerates!
			{
				releaseSamplerate(old_samplerate); //Release the old samplerate first!
			}
			
			word time = 0;
			for (;;)
			{
				convert_samplerate[rate][time] = C_CONVERTSAMPLERATE(time,realrate); //Precalculate rate conversion channel indexes!
				if (++time>=realrate) break; //Next rate position!
			}
			
			samplerate_used[rate] = 1; //We're the only user of this samplerate atm!
			
			ready: //We're ready to execute!
			*/

			if (rate>(float)MAX_SAMPLERATE) //Too much?
			{
				dolog("soundservice","Maximum samplerate passed: %f",rate); //Maximum samplerate passed!
				unlockaudio();
				return 0; //Invalid samplerate!
			}
			
			//Determine the samplerate used first!
			float samplerate = rate;
			if (samplerate==0.0f) //Rate not specified?
			{
				 samplerate = SW_SAMPLERATE; //Default: hardware samplerate!
			}
			
			soundchannels[n].samplerate = samplerate; //Save the actual samplerate used!
			soundchannels[n].convert_samplerate = (1/SW_SAMPLERATE)*samplerate; //The factor for each SW samplerate in destination samplerates!
			soundchannels[n].bufferinc = (uint_32)((float)soundchannels[n].sound.numsamples/(float)soundchannels[n].convert_samplerate); //How much the buffer position decreases during a buffering, in samples!
			
			#ifdef DEBUG_SOUNDALLOC
			dolog("soundservice","Convert samplerate: x%10.5f=SW:%10.5f,Channel:%10.5f; REQ:%10.5f",soundchannels[n].convert_samplerate,SW_SAMPLERATE,samplerate,rate);
			#endif
			
			unlockaudio(); //Unlock the audio!
			return 1; //OK: we're ready to run!
		}
	}
	unlockaudio(); //Unlock the audio!
	return 0; //Not found!
}

byte setVolume(SOUNDHANDLER handler, void *extradata, float p_volume) //Channel&Volume(100.0f=100%)
{
	if (__HW_DISABLED) return 0; //Disabled?
	uint_32 n;
	lockaudio(); //Lock the audio!
	for (n=0;n<soundchannels_used;n++) //Check all!
	{
		if (soundchannels[n].soundhandler && (soundchannels[n].soundhandler==handler) && (soundchannels[n].extradata==extradata)) //Found?
		{
			soundchannels[n].volume = p_volume; //Set the volume of the channel!
			soundchannels[n].volume_percent = p_volume?dB2factor(p_volume*0.01f,1):0; //The volume in linear percent, with 0dB=silence!
			unlockaudio(); //Unlock the audio!
			return 1; //Done: check no more!
		}
	}
	unlockaudio(); //Unlock the audio!
	return 0; //Not found!
}

//add&removal of channels!

OPTINLINE uint_32 samplesize(uint_32 samples, byte method)
{
	switch (method)
	{
		case SMPL16: //16 bit unsigned?
		case SMPL16S: //16 bit signed?
		case SMPL16U: //16 bit unsigned linear?
			return (samples<<1)*sizeof(word);
			break;
		case SMPL8: //8 bit unsigned?
		case SMPL8S: //8 bit signed?
		case SMPL8U: //8 bit unsigned linear?
			return (samples<<1)*sizeof(byte);
			break;
		case SMPLFLT: //Floating point numbers?
			return (samples<<1)*sizeof(float);
			break;
		default:
			break;
	}
	return 0; //No size available: invalid size!
}

byte addchannel(SOUNDHANDLER handler, void *extradata, char *name, float samplerate, uint_32 samples, byte stereo, byte method) //Adds and gives a 1 on added or 0 on error!
{
	if (__HW_DISABLED) return 0; //Disabled?
	if (!handler) return 0; //Invalid handler!
	if (method>6) //Invalid method?
	{
		return 0; //Nothing: unsupported method!
	}

	#ifdef DEBUG_SOUNDALLOC
	dolog("soundservice","Request: Adding channel at %fHz, buffer every %i samples, Stereo: %i",samplerate,samples,stereo);
	#endif

	if (!samplerate) //Autodetect?
	{
		samplerate = SW_SAMPLERATE; //Automatic samplerate!
		//dolog("soundservice","Autodetect samplerate: %fHz",samplerate); //Autodetect!
	}
	if (!samples) //Autodetect?
	{
		samples = (uint_32)((float)samplerate*((float)(SAMPLESIZE)/(float)SW_SAMPLERATE)); //Calculate samples based on samplesize samples out of hardware samplerate!
		//dolog("soundservice","Autodetect: %i samples buffer!",samples);
	}

	//Check for existant update!
	if (setSampleRate(handler,extradata,samplerate)) //Set?
	{
		dolog("soundservice","Sample rate changed to %f",samplerate);
		if (setStereo(handler,extradata,stereo)) //Set?
		{
			dolog("soundservice","Stereo changed to %i",stereo);
			#ifdef DEBUG_SOUNDALLOC
			dolog("soundservice","Channel changed and ready to run: handler: %p, extra data: %p, samplerate: %i, stereo: %i",handler,extradata,samplerate,stereo);
			dolog("soundservice",""); //Empty row!
			#endif
			return 1; //Already added and updated!
		}
	}

	uint_32 n; //For free allocation finding!
	lockaudio(); //Lock the audio!
	for (n=0;n<NUMITEMS(soundchannels);n++) //Try to find an available one!
	{
		if (!soundchannels[n].soundhandler) //Unused entry?
		{
			#ifdef DEBUG_SOUNDALLOC
			dolog("soundservice","Adding channel %s at %f samples/s, buffer every %i samples, Stereo: %i",name,samplerate,samples,stereo);
			#endif
			soundchannels[n].soundhandler = handler; //Set handler!
			soundchannels[n].fillbuffer = &fillbuffer_new; //Our fillbuffer call to start with!
			soundchannels[n].extradata = extradata; //Extra data to be sent!
			soundchannels[n].sound.numsamples = samples; //Ammount of samples to buffer at a time!
			memset(&soundchannels[n].name,0,sizeof(soundchannels[n].name)); //Init name!
			strcpy(soundchannels[n].name,name); //Set a name to use for easy viewing/debugging!
			
			if (n>=soundchannels_used) //Past the ammount of channels used?
			{
				soundchannels_used = n;
				++soundchannels_used; //Update the ammount of sound channels used!
			}
			
			//Init volume, sample rate and stereo!
			setVolume(handler,extradata,100.0f); //Default volume to 100%
			if (!setSampleRate(handler,extradata,samplerate)) //The sample rate to use!
			{
				removechannel(handler,extradata,0);
				unlockaudio(); //Unlock audio and start playing!
				return 0; //Abort!
			}
			if (!setStereo(handler,extradata,stereo)) //Stereo output?
			{
				removechannel(handler,extradata,0);
				unlockaudio(); //Unlock audio and start playing!
				return 0; //Abort!
			}

			soundchannels[n].samplemethod = method; //The sampling method to use!

			//Finally, the samples themselves!
			soundchannels[n].sound.position = 0; //Make us refresh immediately!
			
			soundchannels[n].sound.length = samplesize(soundchannels[n].sound.numsamples,method); //Ammount of samples in the buffer, stereo quality (even if mono used)!
			soundchannels[n].sound.samples = zalloc(soundchannels[n].sound.length,"SW_Samples",NULL);
			
			#ifdef DEBUG_SOUNDALLOC
			dolog("soundservice","Channel allocated and ready to run: handler: %p, extra data: %p, samplerate: %f, sample buffer size: %i, stereo: %i",soundchannels[n].soundhandler,soundchannels[n].extradata,soundchannels[n].samplerate,soundchannels[n].sound.numsamples,soundchannels[n].stereo);
			dolog("soundservice",""); //Empty row!
			#endif
			unlockaudio(); //Unlock audio and start playing!
			return 1; //Add a channel and give the pointer to the current one!
		}
	}
	
	#ifdef DEBUG_SOUNDALLOC
	dolog("soundservice","Ran out of free channels!");
	#endif
	
	unlockaudio(); //Unlock audio and start playing!
	return 0; //No channel available!
}

//is_hw: bit 1 set: do not pause, bit 2 set: do not resume playing.
void removechannel(SOUNDHANDLER handler, void *extradata, byte is_hw) //Removes a sound handler from mixing, use is_hw=0 always, except for init/done of sound.c!
{
	if (__HW_DISABLED) return; //Disabled?
	if (!handler) return; //Can't remove no handler!
	uint_32 n; //For free allocation finding!
	lockaudio(); //Lock the audio!
	for (n=0;n<soundchannels_used;n++) //Try to find an available one!
	{
		if (soundchannels[n].soundhandler && (soundchannels[n].soundhandler==handler) && (soundchannels[n].extradata==extradata)) //Found?
		{
			#ifdef DEBUG_SOUNDALLOC
			dolog("soundservice","Releasing channel %s...",soundchannels[n].name);
			dolog("soundservice",""); //Empty row!
			#endif
			//releaseSamplerate(soundchannels[n].samplerate); //Release the sample rate we're using, if needed!
			if (soundchannels[n].sound.samples && soundchannels[n].sound.length) //Samples allocated?
			{
				freez((void **)&soundchannels[n].sound.samples,soundchannels[n].sound.length,"SW_Samples"); //Free samples!
				if (!soundchannels[n].sound.samples) //Freed?
				{
					soundchannels[n].sound.length = 0; //No length anymore!
				}
			}
			
			//Next remove our handler and the channel itself!
			soundchannels[n].soundhandler = NULL; //Stop the handler from availability!
			soundchannels[n].extradata = NULL; //No extra data anymore!

			#ifdef DEBUG_SOUNDALLOC
			dolog("soundservice","Channel %p:%p:%s released and ready to continue.",handler,extradata,soundchannels[n].name);
			dolog("soundservice",""); //Empty row!
			#endif
			
			if (n==(soundchannels_used-1)) //Final sound channel?
			{
				n = soundchannels_used;
				--n; //Final channel in use?
				while (1)
				{
					if (n==0 && !soundchannels[n].soundhandler) //Nothing found?
					{
						soundchannels_used = 0; //Nothing used!
						break; //Stop searching!
					}
					if (soundchannels[n].soundhandler) //Found a channel in use?
					{
						soundchannels_used = n;
						++soundchannels_used; //Found this channel!
						break; //Stop searching!
					}
					--n; //Decrease to the next channel!
				}
				//Now soundchannels_used contains the ammount of actually used sound channels!
			}
			memset(&soundchannels[n].name,0,sizeof(soundchannels[n].name)); //Clear the name!
			
			unlockaudio(); //Unlock the audio and start playing again?
			return; //Done!
		}
	}
	unlockaudio(); //Unlock the audio and start playing again?
}

void resetchannels()
{
	if (__HW_DISABLED) return; //Disabled?
	uint_32 n; //For free allocation finding!
	for (n=0;n<soundchannels_used;n++) //Try to find an available one!
	{
		if (soundchannels[n].soundhandler) //Allocated?
		{
			removechannel(soundchannels[n].soundhandler,soundchannels[n].extradata,3); //Remove the channel and stop all playback!
		}
	}
}

//OUR MIXING!

//Sample retrieval
int_32 getsample_16(playing_p channel, uint_32 position)
{
	word *y = (word *)channel->sound.samples;
	return unsigned2signed16(y[position]);
}

int_32 getsample_16s(playing_p channel, uint_32 position)
{
	sword *ys = (sword *)channel->sound.samples;
	return ys[position];
}

int_32 getsample_16u(playing_p channel, uint_32 position)
{
	word *y = (word *)channel->sound.samples;
	return (int_32)(((int_32)y[position])-0x8000);
}

int_32 getsample_8(playing_p channel, uint_32 position)
{
	byte *x = (byte *)channel->sound.samples;	
	return (int_32)unsigned2signed8(x[position])<<8;
}

int_32 getsample_8s(playing_p channel, uint_32 position)
{
	sbyte *xs = (sbyte *)channel->sound.samples;
	return (int_32)(xs[position]<<8);
}

int_32 getsample_8u(playing_p channel, uint_32 position)
{
	byte *x = (byte *)channel->sound.samples;
	return (int_32)((((int_32)x[position])-0x80)<<8);
}

int_32 getsample_flt(playing_p channel, uint_32 position)
{
	float *z = (float *)channel->sound.samples;
	return (int_32)((z[position]/FLT_MAX)*SHRT_MAX); //Convert to integer value!	
}

typedef int_32 (*SAMPLEHANDLER)(playing_p channel, uint_32 position); //Sample handler!

OPTINLINE int_32 getsample(playing_p channel, uint_32 position) //Get 16-bit sample from sample buffer, convert as needed!
{
	static SAMPLEHANDLER handlers[7] = {getsample_16,getsample_8,getsample_16s,getsample_8s,getsample_flt,getsample_16u,getsample_8u};
	return handlers[channel->samplemethod](channel,position); //Execute handler if available!
}

//Simple macros for checking samples!
//Precalcs handling!
#define C_SAMPLEPOS(channel) (channel->sound.position)
#define C_BUFFERSIZE(channel) (channel->sound.numsamples)
#define C_BUFFERINC(channel) (channel->bufferinc)
//Use precalculated sample positions!
//#define C_SAMPLERATE(channel,position) (convert_samplerate[soundchannels[(channel)].samplerate][(position)])
#define C_SAMPLERATE(channel,position) (uint_32)(channel->convert_samplerate*((float)(position)))
#define C_STEREO(channel) (channel->stereo)
#define C_GETSAMPLEPOS(channel,rchannel,time) (samplepos[C_STEREO(channel)][(((word)time)<<1)|(rchannel)])
#define C_VOLUMEPERCENT(channel) (channel->volume_percent)
#define C_SAMPLE(channel,samplepos) getsample(channel,samplepos)

//Processing functions prototypes!
void emptychannelbuffer(playing_p currentchannel, int_32 *result_l, int_32 *result_r, uint_32 relsample); //Empty buffer channel handler!
void filledchannelbuffer(playing_p currentchannel, int_32 *result_l, int_32 *result_r, uint_32 relsample); //Full buffer channel handler!


OPTINLINE void processbufferflags(playing_p currentchannel)
{
	currentchannel->processbuffer = (currentchannel->bufferflags&1)?&filledchannelbuffer:&emptychannelbuffer; //Either the filled or empty channel buffer to use!
}

uint_32 fillbuffer_existing(playing_p currentchannel, uint_32 *relsample, uint_32 currentpos)
{
	uint_32 bufferinc; //Increase in the buffer!
	#ifdef DEBUG_SOUNDBUFFER
	byte buffering = 0; //We're buffered?
	#endif
	bufferinc = C_BUFFERINC(currentchannel); //Load buffer increase rate!
	*relsample = C_SAMPLERATE(currentchannel,currentpos); //Get the sample position of the destination samplerate!
	rebuffer: //Rebuffer check!
	if (*relsample>=C_BUFFERSIZE(currentchannel)) //Expired or empty, we've reached the end of the buffer (sample overflow)?
	{
		#ifdef DEBUG_SOUNDBUFFER
		buffering = 1; //We're buffering!
		dolog("soundservice","Buffering @ %i/%i samples; extra data: %p; name: %s",*relsample,C_BUFFERSIZE(currentchannel),currentchannel->extradata,currentchannel->name);
		#endif
		//Buffer and update buffer position!
		currentchannel->bufferflags = currentchannel->soundhandler(currentchannel->sound.samples,C_BUFFERSIZE(currentchannel),C_STEREO(currentchannel),currentchannel->extradata); // Request next sample for this channel, also give our channel extra information!
		processbufferflags(currentchannel); //Process the buffer flags!
		currentpos -= bufferinc; //Reset position in the next frame!
		*relsample = C_SAMPLERATE(currentchannel,currentpos); //Get the sample rate for the new buffer!
		goto rebuffer; //Rebuffer if needed!
	} //Don't buffer!
	#ifdef DEBUG_SOUNDBUFFER
	if (buffering) //We were buffering?
	{
		buffering = 0;
		dolog("soundservice","Buffer ready. Mixing...");
	}
	#endif
	return currentpos; //Give the new position!
}

uint_32 fillbuffer_new(playing_p currentchannel, uint_32 *relsample, uint_32 currentpos)
{
	//currentpos = 0; //Reset samplepos!
	*relsample = 0; //Reset relative sample!
#ifdef DEBUG_SOUNDBUFFER
	dolog("soundservice", "Initialising sound buffer...");
	dolog("soundservice", "Buffering @ 0/%i samples; extra data: %p; name: %s", C_BUFFERSIZE(currentchannel), currentchannel->extradata, currentchannel->name);
#endif
	//Buffer and update buffer position!
	currentchannel->bufferflags = currentchannel->soundhandler(currentchannel->sound.samples,C_BUFFERSIZE(currentchannel),C_STEREO(currentchannel),currentchannel->extradata); // Request next sample for this channel, also give our channel extra information!
	if (currentchannel->bufferflags&1) //Filled?
	{
		currentchannel->fillbuffer = &fillbuffer_existing; //We're initialised, so call existing buffers from now on!
	}

	processbufferflags(currentchannel); //Process the buffer flags!
	#ifdef DEBUG_SOUNDBUFFER
	dolog("soundservice","Buffer ready. Mixing...");
	#endif
	return 0; //We start at the beginning!
}

void filledchannelbuffer(playing_p currentchannel, int_32 *result_l, int_32 *result_r, uint_32 relsample)
{
	float volume = C_VOLUMEPERCENT(currentchannel); //Retrieve the current volume!
	int_32 sample_l = C_SAMPLE(currentchannel,C_GETSAMPLEPOS(currentchannel,0,relsample)); //The composed sample, based on the relative position!
	int_32 sample_r = C_SAMPLE(currentchannel,C_GETSAMPLEPOS(currentchannel,1,relsample)); //The composed sample, based on the relative position!
	
	//Apply the channel volume!
	sample_l = (int_32)(sample_l*volume);
	sample_r = (int_32)(sample_r*volume);
	
	//Now we have the correct left and right channel data on our native samplerate.
	
	//Next, add the data to the mixer!
	*result_l += sample_l; //Mix the channels equally together based on volume!
	*result_r += sample_r; //See above!
}

void emptychannelbuffer(playing_p currentchannel, int_32 *result_l, int_32 *result_r, uint_32 relsample)
{
	//Do nothing!
}

OPTINLINE void mixchannel(playing_p currentchannel, int_32 *result_l, int_32 *result_r) //Mixes the channels with each other at a specific time!
{
	//Process multichannel!
	uint_32 relsample; //Current channel and relative sample!
	//Channel specific data
	register uint_32 currentpos; //Current sample pos!
	
	//First, initialise our variables!
	
	//First step: buffering if needed and keep our buffer!
	currentpos = ((fillbuffer_call)currentchannel->fillbuffer)(currentchannel,&relsample,C_SAMPLEPOS(currentchannel)); //Load the current position!

	((processbuffer_call)currentchannel->processbuffer)(currentchannel,result_l,result_r,relsample);

	//Finish up: update the values to be updated!
	++currentpos; //Next position on each channel!
	C_SAMPLEPOS(currentchannel) = currentpos; //Store the current position for next usage!
}

int_32 mixedsamples[SAMPLESIZE*2]; //All mixed samples buffer!

OPTINLINE void mixaudio(sample_stereo_p buffer, uint_32 length) //Mix audio channels to buffer!
{
	//Variables first
	//Current data numbers
	uint_32 currentsample, channelsleft; //The ammount of channels to mix!
	register int_32 result_l, result_r; //Sample buffer!
	//Active data
	playing_p activechannel; //Current channel!
	int_32 *firstactivesample;
	int_32 *activesample;
	
	//Stuff for Master gain
#ifndef __psp__
#ifdef __USE_EQUALIZER
	double RMS_l = 0, RMS_r = 0, gainMaster_l, gainMaster_r; //Everything needed to apply the Master gain (equalizing using Mean value)
#endif
#endif
	
	channelsleft = soundchannels_used; //Load the channels to process!
	if (!length) return; //Abort without length!
	memset(&mixedsamples,0,sizeof(mixedsamples)); //Init mixed samples, stereo!
	if (channelsleft)
	{
		activechannel = &soundchannels[0]; //Lookup the first channel!
		for (;;) //Mix the next channel!
		{
			if (activechannel->soundhandler) //Active?
			{
				if (activechannel->samplerate &&
					memprotect(activechannel->sound.samples,activechannel->sound.length,"SW_Samples")) //Allocated all neccesary channel data?
				{
					currentsample = length; //The ammount of sample to still buffer!
					activesample = &mixedsamples[0]; //Init active sample to the first sample!
					if (!(activechannel->bufferflags & 1)) //Empty channel buffer?
					{
						activechannel->fillbuffer = &fillbuffer_new; //We're not yet initialised, so call check for initialisation from now on!
					}
					for (;;) //Process all samples!
					{
						firstactivesample = activesample++; //First channel sample!
						mixchannel(activechannel,firstactivesample,activesample++); //L&R channel!
						if (!--currentsample) break; //Next sample when still not done!
					}
				}
			}
			if (!--channelsleft) break; //Stop when no channels left!
			++activechannel; //Next channel!
		}
	} //Got channels?


#ifndef __psp__
#ifdef __USE_EQUALIZER
	//Equalize all sound volume!

	//Process all generated samples to output!
	currentsample = length; //Init samples to give!
	activesample = &mixedsamples[0]; //Initialise the mixed samples position!

	//Second step: Apply equalizer using automatic Master gain.
	for (;;)
	{
		RMS_l += (*activesample) * (*activesample);
		++activesample; //Next channel!
		RMS_r += (*activesample) * (*activesample);
		++activesample; //Next sample!
		if (!--currentsample) break;
	}
	
	RMS_l /= length;
	RMS_r /= length;
	RMS_l = sqrt(RMS_l);
	RMS_r = sqrt(RMS_r);
	
	gainMaster_l = SHRT_MAX / (sqrt(2)*RMS_l);
	gainMaster_r = SHRT_MAX / (sqrt(2)*RMS_r);
#endif
#endif
	
	//Final step: apply Master gain and clip to output!
	currentsample = length; //Init samples to give!
	activesample = &mixedsamples[0]; //Initialise the mixed samples position!
	for (;;)
	{
		result_l = *activesample++; //L channel!
		result_r = *activesample++; //R channel!
#ifndef __psp__
#ifdef __USE_EQUALIZER
		result_l *= gainMaster_l; //Apply master gain!
		result_r *= gainMaster_r; //Apply master gain!
#endif
#endif
		if (result_l>SHRT_MAX) result_l = SHRT_MAX;
		if (result_l<SHRT_MIN) result_l = SHRT_MIN;
		if (result_r>SHRT_MAX) result_r = SHRT_MAX;
		if (result_r<SHRT_MIN) result_r = SHRT_MIN;

		buffer->l = (sample_t)result_l; //Left channel!
		buffer->r = (sample_t)result_r; //Right channel!
		if (!--currentsample) return; //Finished!
		++buffer; //Next sample in the result!
	}
}

//Audio callbacks!

//SDL audio callback:

/* This function is called by SDL whenever the sound card
   needs more samples to play. It might be called from a
   separate thread, so we should be careful what we touch. */
uint_64 totaltime_audio = 0; //Total time!
uint_64 totaltimes_audio = 0; //Total times!
uint_32 totaltime_audio_avg = 1; //Total time of an average audio thread. Use this for synchronization with other time-taking hardware threads.
void Sound_AudioCallback(void *user_data, Uint8 *audio, int length)
{
	if (__HW_DISABLED) return; //Disabled?
	/* Clear the audio buffer so we can mix samples into it. */

	//Now, mix all channels!
	sample_stereo_p ubuf = (sample_stereo_p) audio; //Buffer!
	#ifdef EXTERNAL_TIMING
	getuspassed(&audioticks); //Init!
	#endif
	uint_32 reallength = length/sizeof(*ubuf); //Total length!
	mixaudio(ubuf,reallength); //Mix the audio!
	#ifdef EXTERNAL_TIMING
	uint_64 mspassed = getuspassed(&audioticks); //Load the time passed!
	totaltime_audio += mspassed; //Total time!
	++totaltimes_audio; //Total times increase!
	totaltime_audio_avg = (uint_32)SAFEDIV(totaltime_audio,totaltimes_audio); //Recalculate AVG audio time!
	#endif
	#ifdef DEBUG_SOUNDSPEED
	char time1[20];
	char time2[20];
	convertTime(mspassed,&time1[0]); //Ms passed!
	convertTime(totaltime_audio_avg,&time2[0]); //Total time passed!
	if (soundchannels_used) //Any channels out there?
	{
		dolog("soundservice","Mixing %i samples took: %s, average: %s",length/sizeof(ubuf[0]),time1,time2); //Log it!
	}
	#endif
}



byte SDLAudio_Loaded = 0; //Are we loaded (kept forever till quitting)

//Audio initialisation!
void initAudio() //Initialises audio subsystem!
{
	if (__HW_DISABLED) return; //Abort?

	if (SDL_WasInit(SDL_INIT_AUDIO)) //SDL rendering?
	{
		if (!SDLAudio_Loaded) //Not loaded yet?
		{
			if (!audioticksready) //Not ready yet?
			{
				initTicksHolder(&audioticks); //Init!
				audioticksready = 1; //Ready!
			}
			//dolog("soundservice","Use SDL rendering...");
			SDL_PauseAudio(1); //Disable the thread!
			
			//dolog("soundservice","Setting desired audio device...");
			/* Open the audio device. The sound driver will try to give us
			the requested format, but it might not succeed. The 'obtained'
			structure will be filled in with the actual format data. */
			audiospecs.freq = HW_SAMPLERATE;	/* desired output sample rate */
			audiospecs.format = AUDIO_S16SYS;	/* request signed 16-bit samples */
			audiospecs.channels = 2;	/* ask for stereo */
			audiospecs.samples = SAMPLESIZE;	/* this is more or less discretionary */
			audiospecs.size = audiospecs.samples * audiospecs.channels * sizeof(sample_t);
			audiospecs.callback = &Sound_AudioCallback;
			audiospecs.userdata = NULL;	/* we don't need this */
			//dolog("soundservice","Opening audio device...");
			if (SDL_OpenAudio(&audiospecs, NULL) < 0)
			{
				//dolog("soundservice","Unable to open audio device: %s",SDL_GetError());
				raiseError("sound service","Unable to open audio device: %s", SDL_GetError());
				return; //Just to be safe!
			}
			//dolog("soundservice","Initialising channels...");
			memset(&soundchannels,0,sizeof(soundchannels)); //Initialise/reset all sound channels!
			SDLAudio_Loaded = 1; //We're loaded!
			//dolog("soundservice","Channels initialised@%fHz.",SW_SAMPLERATE);
		}
		else //Already loaded, needs reset?
		{
			//dolog("soundservice","Resetting channels...");
			SDL_PauseAudio(1); //Disable the thread!
			resetchannels(); //Reset the channels!
			//dolog("soundservice","Channels reset.");
		}
		//dolog("soundservice","Starting audio...");
		//Finish up to start playing!
		//dolog("soundservice","Calculating samplepos precalcs...");
		calc_samplePos(); //Initialise sample position precalcs!
		//dolog("soundservice","Starting audio...");
		SDL_PauseAudio(0); //Start playing!
		//dolog("soundservice","Device ready.");
	}
}

void doneAudio()
{
	if (__HW_DISABLED) return; //Abort?
	resetchannels(); //Stop all channels!
	//dolog("soundservice","Resetting channels for terminating...");
	//dolog("soundservice","Channels have been reset.");
	if (SDL_WasInit(SDL_INIT_AUDIO)) //Audio loaded?
	{
		//dolog("soundservice","Closing audio.");
		SDL_CloseAudio(); //Close the audio system!
		//dolog("soundservice","Audio closed.");
		SDLAudio_Loaded = 0; //Not loaded anymore!
	}
	free_samplePos(); //Free the sample position precalcs!
}