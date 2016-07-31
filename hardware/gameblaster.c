#include "headers/types.h" //Basic types!
#include "headers/support/sounddoublebuffer.h" //Double buffered sound support!
#include "headers/emu/sound.h" //Sound support!
#include "headers/support/signedness.h" //Sign conversion support!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/ports.h" //I/O support!

//Are we disabled?
#define __HW_DISABLED 0

//Game Blaster sample rate and other audio defines!
#define __GAMEBLASTER_SAMPLERATE 44100.0f
#define __GAMEBLASTER_SAMPLEBUFFERSIZE 4096
#define __GAMEBLASTER_VOLUME 100.0f

struct
{
	word baseaddr; //Base address of the Game Blaster!
	word soundblastercompatible; //Do we use sound blaster compatible I/O
	SOUNDDOUBLEBUFFER soundbuffer[2]; //Our two sound buffers for our two chips!
	byte storelatch[2]; //Two store/latch buffers!
} GAMEBLASTER; //Our game blaster information!

double gameblaster_soundtiming=0.0, gameblaster_soundtick=1000000000.0/__GAMEBLASTER_SAMPLERATE;
void updateGameBlaster(double timepassed)
{
	if (GAMEBLASTER.baseaddr==0) return; //No game blaster?
	//Adlib sound output
	gameblaster_soundtiming += timepassed; //Get the amount of time passed!
	if (gameblaster_soundtiming>=gameblaster_soundtick)
	{
		for (;gameblaster_soundtiming>=gameblaster_soundtick;)
		{
			float samples[2][2]; //Two stereo samples!
			//Generate the sample!
			samples[0][0] = samples[0][1] = samples[1][0] = samples[1][1] = 0.0f; //No sample yet!

			//Now push the samples to the output!
			samples[0][0] = LIMITRANGE(samples[0][0], (float)SHRT_MIN, (float)SHRT_MAX); //Clip our data to prevent overflow!
			samples[0][1] = LIMITRANGE(samples[0][1], (float)SHRT_MIN, (float)SHRT_MAX); //Clip our data to prevent overflow!
			samples[1][0] = LIMITRANGE(samples[1][0], (float)SHRT_MIN, (float)SHRT_MAX); //Clip our data to prevent overflow!
			samples[1][1] = LIMITRANGE(samples[1][1], (float)SHRT_MIN, (float)SHRT_MAX); //Clip our data to prevent overflow!
			writeDoubleBufferedSound32(&GAMEBLASTER.soundbuffer[0],(signed2unsigned16(samples[0][1])<<16)|signed2unsigned16(samples[0][0])); //Output the sample to the renderer!
			writeDoubleBufferedSound32(&GAMEBLASTER.soundbuffer[1],(signed2unsigned16(samples[1][1])<<16)|signed2unsigned16(samples[1][0])); //Output the sample to the renderer!
			gameblaster_soundtiming -= gameblaster_soundtick; //Decrease timer to get time left!
		}
	}
}

byte GameBlaster_soundGenerator(void* buf, uint_32 length, byte stereo, void *userdata) //Generate a sample!
{
	uint_32 c;
	c = length; //Init c!
	
	static uint_32 last=0;
	INLINEREGISTER uint_32 buffer;

	SOUNDDOUBLEBUFFER *doublebuffer = (SOUNDDOUBLEBUFFER *)userdata; //Our double buffered sound input to use!
	int_32 mono_converter;
	sample_stereo_p data_stereo;
	sword *data_mono;
	if (stereo) //Stereo processing?
	{
		data_stereo = (sample_stereo_p)buf; //The data in correct samples!
		for (;;) //Fill it!
		{
			//Left and right samples are the same: we're a mono signal!
			readDoubleBufferedSound32(doublebuffer,&last); //Generate a stereo sample if it's available!
			buffer = last; //Load the last sample for processing!
			data_stereo->l = unsigned2signed16((word)buffer); //Load the last generated sample(left)!
			buffer >>= 16; //Shift low!
			data_stereo->r = unsigned2signed16((word)buffer); //Load the last generated sample(right)!
			++data_stereo; //Next stereo sample!
			if (!--c) return SOUNDHANDLER_RESULT_FILLED; //Next item!
		}
	}
	else //Mono processing?
	{
		data_mono = (sword *)buf; //The data in correct samples!
		for (;;) //Fill it!
		{
			//Left and right samples are the same: we're a mono signal!
			readDoubleBufferedSound32(doublebuffer,&last); //Generate a stereo sample if it's available!
			buffer = last; //Load the last sample for processing!
			mono_converter = unsigned2signed16((word)buffer); //Load the last generated sample(left)!
			buffer >>= 16; //Shift low!
			mono_converter += unsigned2signed16((word)buffer); //Load the last generated sample(right)!
			mono_converter = LIMITRANGE(mono_converter, (float)SHRT_MIN, (float)SHRT_MAX); //Clip our data to prevent overflow!
			*data_mono++ = mono_converter; //Save the sample and point to the next mono sample!
			if (!--c) return SOUNDHANDLER_RESULT_FILLED; //Next item!
		}
	}
}

byte outGameBlaster(word port, byte value)
{
	if (__HW_DISABLED) return 0; //We're disabled!
	if ((port&GAMEBLASTER.soundblastercompatible)!=GAMEBLASTER.baseaddr) return 0; //Not Game Blaster port!
	switch (port&0xF)
	{
		case 0: //Left SAA-1099?
		case 1: //Left SAA-1099?
		case 2: //Right SAA-1099?
		case 3: //Right SAA-1099?
			//Address Phillips chips!
			return 1; //Handled!
		default: //Other addresses(16 addresses)? CT-1302!
			switch (port&0xF) //What port?
			{
				case 6: //Store 1!
				case 7: //Store 2!
					GAMEBLASTER.storelatch[port&1] = value; //Store/latch!
					return 1; //Handled!
				default:
					break;
			}
			return 0; //Not handled yet!
			break;
	}
	return 0; //Not handled!
}

byte inGameBlaster(word port, byte *result)
{
	if (__HW_DISABLED) return 0; //We're disabled!
	if ((port&GAMEBLASTER.soundblastercompatible)!=GAMEBLASTER.baseaddr) return 0; //Not Game Blaster port!
	switch (port&0xF)
	{
		case 0: //Left SAA-1099?
		case 1: //Left SAA-1099?
		case 2: //Right SAA-1099?
		case 3: //Right SAA-1099?
			return 0; //Not Handled! The chips cannot be read, only written!
		default: //Other addresses(16 addresses)? CT-1302!
			switch (port&0xF) //What port?
			{
				case 0x4: //Detection!
					*result = 0x7F; //Give the detection value!
					return 1; //Handled!					
				case 0xA: //Store 1!
				case 0xB: //Store 2!
					*result = GAMEBLASTER.storelatch[port&1]; //Give the store/latch!
					return 1; //Handled!
				default:
					break;
			}
			return 0; //Not handled yet!
			break;
	}
}

void setGameBlaster_SoundBlaster(byte useSoundBlasterIO)
{
	if (__HW_DISABLED) return; //We're disabled!
	GAMEBLASTER.soundblastercompatible = useSoundBlasterIO?0xFFFC:0xFFF0; //Sound Blaster compatible I/O? Use 2 chips only with sound blaster, else full 16 ports for detection!
}

void GameBlaster_setVolume(float volume)
{
	if (__HW_DISABLED) return; //We're disabled!
	setVolume(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[0], volume); //Set the volume!
	setVolume(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[1], volume); //Set the volume!
}

void initGameBlaster(word baseaddr)
{
	if (__HW_DISABLED) return; //We're disabled!
	memset(&GAMEBLASTER,0,sizeof(GAMEBLASTER)); //Full init!
	GAMEBLASTER.baseaddr = baseaddr; //Base address of the Game Blaster!
	setGameBlaster_SoundBlaster(0); //Default to Game Blaster I/O!

	if (allocDoubleBufferedSound32(__GAMEBLASTER_SAMPLEBUFFERSIZE,&GAMEBLASTER.soundbuffer[0])) //Valid buffer?
	{
		if (allocDoubleBufferedSound32(__GAMEBLASTER_SAMPLEBUFFERSIZE,&GAMEBLASTER.soundbuffer[1])) //Valid buffer?
		{
			if (!addchannel(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[0],"GameBlaster",__GAMEBLASTER_SAMPLERATE,__GAMEBLASTER_SAMPLEBUFFERSIZE,0,SMPL16S)) //Start the sound emulation (mono) with automatic samples buffer?
			{
				dolog("GameBlaster","Error registering sound channel for output!");
			}
			else
			{
				setVolume(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[0],__GAMEBLASTER_VOLUME);
				if (!addchannel(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[1],"GameBlaster",__GAMEBLASTER_SAMPLERATE,__GAMEBLASTER_SAMPLEBUFFERSIZE,0,SMPL16S)) //Start the sound emulation (mono) with automatic samples buffer?
				{
					dolog("GameBlaster","Error registering sound channel for output!");
				}
				else
				{
					setVolume(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[1],__GAMEBLASTER_VOLUME);
				}
			}
		}
		else
		{
			dolog("GameBlaster","Error registering second double buffer for output!");
		}
	}
	else
	{
		dolog("GameBlaster","Error registering first double buffer for output!");
	}
	//dolog("adlib","sound channel added. registering ports...");
	//Ignore unregistered channel, we need to be used by software!
	register_PORTIN(&inGameBlaster); //Input ports!
	//All output!
	register_PORTOUT(&outGameBlaster); //Output ports!
}

void doneGameBlaster()
{
	removechannel(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[0],0); //Stop the sound emulation?
	freeDoubleBufferedSound(&GAMEBLASTER.soundbuffer[0]);
	removechannel(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[1],0); //Stop the sound emulation?
	freeDoubleBufferedSound(&GAMEBLASTER.soundbuffer[1]);
}