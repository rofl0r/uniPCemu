#include "headers/types.h" //Basic types!
#include "headers/support/sounddoublebuffer.h" //Double buffered sound support!
#include "headers/emu/sound.h" //Sound support!
#include "headers/support/signedness.h" //Sign conversion support!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/ports.h" //I/O support!

//Are we disabled?
#define __HW_DISABLED 0

double gameblaster_samplelength = 0.0f;

//Game Blaster sample rate and other audio defines!
#define __GAMEBLASTER_SAMPLERATE 22050.0f
#define __GAMEBLASTER_SAMPLEBUFFERSIZE 2048
#define __GAMEBLASTER_VOLUME 100.0f

typedef struct
{
	byte frequency;
	byte frequency_enable;
	byte noise_enable;
	byte octave; //0-7
	byte amplitude[2]; //0-F
	byte envelope[2]; //0-F, 10=off.

	//Data required for timing the square wave
	double time; //Time
	double freq; //Frequency!
	byte level; //The level!
} SAA1099_CHANNEL;

typedef struct
{
	//Data required for simulating noise generators!
	double time; //Time
	double freq; //Frequency!
	byte laststatus; //The last outputted status for detecting cycles!
	byte level; //The level!
} SAA1099_NOISE;

typedef struct
{
	//Basic storage!
	byte regsel; //The selected register!
	byte registers[0x20]; //All selectable registers!

	//Global Data!
	word noise_params[2];
	word env_enable[2];
	word env_reverse_right[2];
	word env_mode[2];
	word env_bits[2];
	word env_clock[2];
	word env_step[2];
	byte all_ch_enable;
	byte sync_state;
	

	//Information taken from the registers!
	SAA1099_CHANNEL channels[8]; //Our channels!
	SAA1099_NOISE noise[2]; //Noise generators!
} SAA1099; //All data for one SAA-1099 chip!

struct
{
	word baseaddr; //Base address of the Game Blaster!
	word soundblastercompatible; //Do we use sound blaster compatible I/O
	SOUNDDOUBLEBUFFER soundbuffer[2]; //Our two sound buffers for our two chips!
	byte storelatch[2]; //Two store/latch buffers!
	SAA1099 chips[2]; //The two chips for generating output!
} GAMEBLASTER; //Our game blaster information!

OPTINLINE byte SAAEnvelope(byte waveform, byte position)
{
	switch (waveform&7) //What waveform?
	{
		case 0: //Zero amplitude?
			return 0; //Always 0!
		case 1: //Maximum amplitude?
			return 0xF; //Always max!
		case 2: //Single decay?
			if (position<0x10)
				return 0xF-position; //Decay!
			else
				return 0; //Silence!
		case 3: //Repetitive decay?
			return 0xF-(position&0xF); //Repetitive decay!
		case 4: //Single triangular?
			if (position>0x20) //Zero past?
				return 0; //Zero!
			else if (position>0x10) //Decay?
				return 0xF-(position&0xF); //Decay!
			else //Attack?
				return (position&0xF); //Attack!
		case 5: //Repetitive triangular?
			if (position&0x10) //Decay?
				return 0xF-(position&0xF); //Decay!
			else //Attack?
				return (position&0xF); //Attack!
		case 6: //Single attack?
			if (position<0x10)
				return position; //Attack!
			else
				return 0;
		case 7: //Repetitive attack?
			return (position&0xF); //Attack!
	}
	return 0; //Unknown envelope?
}

OPTINLINE word calcAmplitude(byte amplitude)
{
	return ((0x10000>>4)-1)*amplitude; //Simple calculation for our range!
}

OPTINLINE void tickSAAEnvelope(SAA1099 *chip, byte channel)
{
	channel &= 1; //Only two channels available!
	if (chip->env_enable[channel]) //Envelope enabled and running?
	{
		byte step,mode,mask; //Temp data!
		mode = chip->env_mode[channel]; //The mode to use!
		//Step form 0..63 and then loop 32..63
		step = ++chip->env_step[channel];
		step &= 0x3F; //Wrap around!
		step |= (chip->env_step[channel]&0x20); //OR in the current high block to loop the high part!
		chip->env_step[channel] = step; //Save the new step now used!
		mask = 0xF; //Full resolution!
		mask &= ~chip->env_bits[channel]; //Apply the bit resolution we use to mask bits off when needed!
		
		//Now, apply the current envelope!
		chip->channels[channel*3].envelope[0] = chip->channels[(channel*3)+1].envelope[0] = chip->channels[(channel*3)+2].envelope[0] = (SAAEnvelope(mode,step)&mask); //Apply the normal envelope!
		if (chip->env_reverse_right[channel]) //Reverse right envelope?
		{
			chip->channels[channel*3].envelope[1] = chip->channels[(channel*3)+1].envelope[1] = chip->channels[(channel*3)+2].envelope[1] = ((0xF-SAAEnvelope(mode,step))&mask); //Apply the reversed envelope!
		}
		else //Normal right envelope?
		{
			chip->channels[channel*3].envelope[1] = chip->channels[(channel*3)+1].envelope[1] = chip->channels[(channel*3)+2].envelope[1] = (SAAEnvelope(mode,step)&mask); //Apply the normal envelope!
		}
	}
	else //Envelope mode off, set all envelope factors to 16!
	{
		chip->channels[(channel*3)+0].envelope[0] = chip->channels[(channel*3)+0].envelope[1] = 
			chip->channels[(channel*3)+1].envelope[0] = chip->channels[(channel*3)+1].envelope[1] =
			chip->channels[(channel*3)+2].envelope[0] = chip->channels[(channel*3)+2].envelope[1] = 0x10; //We're off!
	}
}

OPTINLINE void writeSAA1099Address(SAA1099 *chip, byte address)
{
	chip->regsel = (address&0x1F); //Select the register!
	switch (chip->regsel) //What register has been selected?
	{
		case 0x18:
		case 0x19:
			if (chip->env_clock[0]) tickSAAEnvelope(chip,0); //Tick channel 0?
			if (chip->env_clock[1]) tickSAAEnvelope(chip,1); //Tick channel 1?
			break;
		default: //Unknown?
			break;
	}
}

OPTINLINE void writeSAA1099Value(SAA1099 *chip, byte value)
{
	INLINEREGISTER byte reg;
	reg = chip->regsel; //The selected register to write to!
	chip->registers[reg] = value; //Save the register data itself!
	switch (reg) //What register is written?
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05: //Channel n amplitude?
			reg &= 7;
			chip->channels[reg].amplitude[0] = calcAmplitude(value&0xF);
			chip->channels[reg].amplitude[1] = calcAmplitude(value>>4);
			break;
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0B:
		case 0x0C:
		case 0x0D: //Channel n frequency?
			reg &= 7;
			chip->channels[reg].frequency = value; //Set the frequency!
			break;
		case 0x10:
		case 0x11:
		case 0x12: //Channel n octave?
			reg &= 3;
			chip->channels[reg<<1].octave = (value&7);
			chip->channels[(reg<<1)|1].octave = ((value>>4)&7);
			break;
		case 0x14: //Channel n frequency enable?
			chip->channels[0].frequency_enable = (value&1);
			value >>= 1;
			chip->channels[1].frequency_enable = (value&1);
			value >>= 1;
			chip->channels[2].frequency_enable = (value&1);
			value >>= 1;
			chip->channels[3].frequency_enable = (value&1);
			value >>= 1;
			chip->channels[4].frequency_enable = (value&1);
			value >>= 1;
			chip->channels[5].frequency_enable = (value&1);
			break;
		case 0x15: //Channel n noise enable?
			reg = value; //Load for processing!
			chip->channels[0].noise_enable = (reg&1);
			reg >>= 1;
			chip->channels[1].noise_enable = (reg&1);
			reg >>= 1;
			chip->channels[2].noise_enable = (reg&1);
			reg >>= 1;
			chip->channels[3].noise_enable = (reg&1);
			reg >>= 1;
			chip->channels[4].noise_enable = (reg&1);
			reg >>= 1;
			chip->channels[5].noise_enable = (reg&1);
			break;
		case 0x16: //Noise generators parameters?
			chip->noise_params[0] = (value&3);
			chip->noise_params[1] = ((value>>4)&3);
			break;
		case 0x18:
		case 0x19: //Envelope generators parameters?
			reg &= 1; //What channel?
			chip->env_reverse_right[reg] = (value&1);
			chip->env_mode[reg] = ((value>>1)&7); //What mode?
			chip->env_bits[reg] = ((value&0x10)>>4);
			chip->env_clock[reg] = ((value&0x20)>>5);
			chip->env_enable[reg] = ((value&0x80)>>7);
			//Reset the envelope!
			chip->env_step[reg] = 0; //Reset the envelope!
			break;
		case 0x1C: //Channels enable and reset generators!
			chip->all_ch_enable = (value&1);
			if ((chip->sync_state = ((value&2)>>1))) //Sync & Reset generators?
			{
				for (reg=0;reg<6;++reg)
				{
					chip->channels[reg].level = 0;
					chip->channels[reg].time = 0.0;
				}
			}
			break;
		default: //Unknown register?
			break; //Silently ignore invalid and unimplemented writes!
	}
}

OPTINLINE byte getSAA1099SquareWave(float frequencytime)
{
	return (sinf(2*PI*frequencytime)>=0.0f)?1:0; //Give a square wave at the requested speed!
}

OPTINLINE void generateSAA1099channelsample(SAA1099 *chip, byte channel, byte *output_l, byte *output_r)
{
	float temp;
	double dummy;

	channel &= 7;
	chip->channels[channel].freq = (double)((2*15625)<<chip->channels[channel].octave)/(511.0-(double)chip->channels[channel].frequency); //Calculate the current frequency to use!

	chip->channels[channel].level = getSAA1099SquareWave(chip->channels[channel].freq*chip->channels[channel].time); //Current flipflop output of the square wave generator!

	//Now, tick the square wave generator!
	chip->channels[channel].time += gameblaster_samplelength; //New position for the noise generator!

	temp = chip->channels[channel].time*chip->channels[channel].freq; //Calculate for overflow!
	if (temp >= 1.0f) { //Overflow?
		chip->channels[channel].time = modf(temp, &dummy) / chip->channels[channel].freq;
	}

	//Tick the envelopes when needed!
	if ((channel==1) && (chip->env_clock[0]==0))
		tickSAAEnvelope(chip,0);
	if ((channel==4) && (chip->env_clock[1]==0))
		tickSAAEnvelope(chip,1);

	//Check and apply for noise!
	if (chip->channels[channel].noise_enable) //Use noise?
	{
		if (chip->noise[(channel / 3)&1].level & 1) //If the noise level is high (noise 0 for channel 0-2, noise 1 for channel 3-5)
		{
			//Substract to avoid overflows, half amplitude only
			*output_l -= (chip->channels[channel].amplitude[0]*chip->channels[channel].envelope[0]) / 16 / 2; //Noise left!
			*output_r -= (chip->channels[channel].amplitude[1]*chip->channels[channel].envelope[1]) / 16 / 2; //Noise right!
		}
	}

	//Check and apply the square wave!
	if (chip->channels[channel].frequency_enable) //Square wave enabled?
	{
		if (chip->channels[channel].level & 1) //Channel level is high?
		{
			*output_l += chip->channels[channel].amplitude[0]*(chip->channels[channel].envelope[0]) / 16; //Square wave left!
			*output_r += chip->channels[channel].amplitude[1]*(chip->channels[channel].envelope[1]) / 16; //Square wave left!
		}
	}
}

OPTINLINE void tickSAA1099noise(SAA1099 *chip, byte channel)
{
	float temp;
	double dummy;
	byte noise_flipflop;

	channel &= 1; //Only two channels!

	//Check the current noise generators and update them!
	//Noise channel output!
	noise_flipflop = getSAA1099SquareWave(chip->noise[channel].freq*chip->noise[channel].time); //Current flipflop output of the noise timer!
	if (noise_flipflop) //High?
	{
		if (noise_flipflop != chip->noise[channel].laststatus) //Actually risen?
		{
			if (((chip->noise[channel].level & 0x4000) == 0) == ((chip->noise[channel].level & 0x0040) == 0))
				chip->noise[channel].level = (chip->noise[channel].level << 1) | 1;
			else
				chip->noise[channel].level <<= 1;
		}
	}
	chip->noise[channel].laststatus = noise_flipflop; //Save the last status!

	//Now, tick the noise generator!
	chip->noise[channel].time += gameblaster_samplelength; //New position for the noise generator!

	if (chip->noise[channel].freq!=0.0) //Valid frequency?
	{
		temp = chip->noise[channel].time*chip->noise[channel].freq; //Calculate for overflow!
		if (temp >= 1.0f) { //Overflow?
			chip->noise[channel].time = modf(temp, &dummy) / chip->noise[channel].freq;
		}
	}
}

OPTINLINE void generateSAA1099sample(SAA1099 *chip, float *leftsample, float *rightsample) //Generate a sample on the requested chip!
{
	byte output_l, output_r;

	const static double noise_frequencies[3] = {31250.0*2.0,15625.0*2.0,7812.0*2.0}; //Normal frequencies!
	switch (chip->noise_params[0]) //What frequency to use?
	{
	default:
	case 0:
	case 1:
	case 2: //Normal frequencies!
		chip->noise[0].freq = noise_frequencies[chip->noise_params[0]]; //Normal lookup!
		break;
	case 3:
		chip->noise[0].freq = chip->channels[0].freq; //Channel 0 frequency instead!
	}

	switch (chip->noise_params[1]) //What frequency to use?
	{
	default:
	case 0:
	case 1:
	case 2: //Normal frequencies!
		chip->noise[1].freq = noise_frequencies[chip->noise_params[1]]; //Normal lookup!
		break;
	case 3:
		chip->noise[1].freq = chip->channels[3].freq; //Channel 3 frequency instead!
	}

	output_l = output_r = 0; //Reset the output!
	generateSAA1099channelsample(chip,0,&output_l,&output_r); //Channel 0 sample!
	generateSAA1099channelsample(chip,1,&output_l,&output_r); //Channel 1 sample!
	generateSAA1099channelsample(chip,2,&output_l,&output_r); //Channel 2 sample!
	generateSAA1099channelsample(chip,3,&output_l,&output_r); //Channel 3 sample!
	generateSAA1099channelsample(chip,4,&output_l,&output_r); //Channel 4 sample!
	generateSAA1099channelsample(chip,5,&output_l,&output_r); //Channel 5 sample!
	generateSAA1099channelsample(chip,6,&output_l,&output_r); //Channel 6 sample!


	//Finally, write the resultant samples to the result!
	tickSAA1099noise(chip,0); //Tick first noise channel!
	tickSAA1099noise(chip,1); //Tick second noise channel!

	*leftsample = ((float)output_l)*(1.0/6.0); //Left channel output!
	*rightsample = ((float)output_r)*(1.0 / 6.0); //Right channel output!
}

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

			if (GAMEBLASTER.chips[0].all_ch_enable) //Sound generation of first chip?
			{
				generateSAA1099sample(&GAMEBLASTER.chips[0],&samples[0][0],&samples[0][1]); //Generate a stereo sample on this chip!
			}

			if (GAMEBLASTER.chips[1].all_ch_enable) //Sound generation of first chip?
			{
				generateSAA1099sample(&GAMEBLASTER.chips[1], &samples[1][0], &samples[1][1]); //Generate a stereo sample on this chip!
			}

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
			writeSAA1099Value(&GAMEBLASTER.chips[0],value); //Write value!
			return 1; //Handled!
		case 1: //Left SAA-1099?
			writeSAA1099Address(&GAMEBLASTER.chips[0],value); //Write address!
			return 1; //Handled!
		case 2: //Right SAA-1099?
			writeSAA1099Value(&GAMEBLASTER.chips[1],value); //Write value!
			return 1; //Handled!
		case 3: //Right SAA-1099?
			writeSAA1099Address(&GAMEBLASTER.chips[1],value); //Write address!
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
					GAMEBLASTER.storelatch[0] = GAMEBLASTER.storelatch[1] = 0xFF; //Initialise our latches!
					gameblaster_samplelength = 1.0/__GAMEBLASTER_SAMPLERATE; //The partial duration of a sample!
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