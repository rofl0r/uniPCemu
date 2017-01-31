#include "headers/types.h" //Basic types!
#include "headers/support/sounddoublebuffer.h" //Double buffered sound support!
#include "headers/emu/sound.h" //Sound support!
#include "headers/support/signedness.h" //Sign conversion support!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/ports.h" //I/O support!
#include "headers/support/filters.h" //Filter support!
#include "headers/support/wave.h" //WAV logging test support!

//Are we disabled?
#define __HW_DISABLED 0

//Define to log a test wave of 440Hz!
//#define TESTWAVE

//Define to enable the Dosbox/MAME synthesis method, instead of the improved one(testing purposes only)!
#define DOSBOXMAMESYNTH

//Game Blaster sample rate and other audio defines!
//Game blaster runs at 14MHz divided by 2 divided by 256 clocks to get our sample rate to play at! Or divided by 4 to get 3.57MHz!
#define MHZ14_BASETICK 4
//#define MHZ14_BASETICK 256
//We render at ~44.1kHz!
#define MHZ14_RENDERTICK 324

//Base rate of the Game Blaster to run at!
#define __GAMEBLASTER_BASERATE (MHZ14/MHZ14_BASETICK) 

//Renderer defines to use!
#define __GAMEBLASTER_SAMPLERATE (MHZ14/MHZ14_RENDERTICK)
#define __GAMEBLASTER_SAMPLEBUFFERSIZE 4096
#define __GAMEBLASTER_VOLUME 100.0f

#define __GAMEBLASTER_AMPLIFIER (1.0/6.0)

typedef struct
{
	byte frequency;
	byte frequency_enable;
	byte noise_enable;
	byte octave; //0-7
	word amplitude[2]; //0-F?
	byte envelope[2]; //0-F, 10=off.

	//Data required for timing the square wave
	float time; //Time
	float freq; //Frequency!
	byte level; //The level!
	int_32 ampenv[8]; //All envelope outputs! Index Bit0=Right channel, Bit1=Channel output index, Bit 2=Noise output!
	byte noisechannel; //Linked noise channel!
} SAA1099_CHANNEL;

typedef struct
{
	//Data required for simulating noise generators!
	float freq; //Frequency!
	byte laststatus; //The last outputted status for detecting cycles!
	uint_32 level; //The level!
	byte levelbit; //Current bit from the current level!
} SAA1099_NOISE;

typedef struct
{
	float freq; //Currently used frequency!
	uint_32 timepoint; //Point that overflows in time!
	uint_32 timeout; //Half-wave timeout!
	byte output; //Flipflop output!
} SAA1099_SQUAREWAVE;

typedef struct
{
	//Basic storage!
	byte regsel; //The selected register!
	byte registers[0x20]; //All selectable registers!

	//Global Data!
	word noise_params[2];
	word env_enable[2];
	word env_reverse_right[2];
	byte env_mode[2];
	word env_bits[2];
	word env_clock[2];
	byte env_step[2];
	byte all_ch_enable;
	byte sync_state;
	
	//Information taken from the registers!
	SAA1099_CHANNEL channels[8]; //Our channels!
	SAA1099_NOISE noise[2]; //Noise generators!
	SAA1099_SQUAREWAVE squarewave[10]; //Everything needed to generate a square wave!
} SAA1099; //All data for one SAA-1099 chip!

struct
{
	word baseaddr; //Base address of the Game Blaster!
	byte soundblastercompatible; //Do we use sound blaster compatible I/O
	SOUNDDOUBLEBUFFER soundbuffer[2]; //Our two sound buffers for our two chips!
	byte storelatch[2]; //Two store/latch buffers!
	SAA1099 chips[2]; //The two chips for generating output!
	HIGHLOWPASSFILTER filter[4]; //Filter for left and right channels, first the low-pass, then the high-pass!
	uint_32 baseclock; //Base clock to render at(up to bus rate of 14.31818MHz)!
} GAMEBLASTER; //Our game blaster information!

float AMPLIFIER = 0.0; //The amplifier, amplifying samples to the full range!

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

OPTINLINE void updateAmpEnv(SAA1099 *chip, byte channel)
{
	INLINEREGISTER byte input, left;
	byte env[2];
	env[0] = (chip->channels[channel].amplitude[0]*chip->channels[channel].envelope[0]) >> 4; //Left envelope!
	env[1] = (chip->channels[channel].amplitude[1]*chip->channels[channel].envelope[1]) >> 4; //Right envelope!
	//bit0=right channel
	//bit1=square wave output
	//bit2=noise output
	//Original algorithm from Dosbox&MAME was:
	input = 0; //First input!
	left = 8; //How many left?
#ifdef DOSBOXMAMESYNTH
	INLINEREGISTER int_32 output;
	INLINEREGISTER byte curenv;
	do {
		output = 0; //Init!
		curenv = env[input&1]; //Current environment!
		if ((input&4) && chip->channels[channel].noise_enable) //Noise on?
		{
			output -= (int_32)(curenv>>1); //Half volume substracted!
		}
		if ((input&2) && chip->channels[channel].frequency_enable) //Frequency on?
		{
			output += (int_32)curenv; //Full volume added!
		}
		chip->channels[channel].ampenv[input] = output; //Give the output!
		++input; //Less left!
	} while (--left); //Next input!

#else
	switch (chip->channels[channel].noise_enable|(chip->channels[channel].frequency_enable<<1)) //Noise/frequency mode?
	{
		case 0: //Both disabled?
			memset(&chip->channels[channel].ampenv,0,sizeof(chip->channels[channel].ampenv)); //No output!
			break;
		case 1: //Noise only?
			do //Check all inputs!
			{
				chip->channels[channel].ampenv[input] = ((input&4)>>2)*env[input&1]; //Noise at max volume!
				++input;
			} while (--left); //Next input!
			break;
		case 2: //Frequency only?
			do //Check all inputs!
			{
				chip->channels[channel].ampenv[input] = ((input&2)>>1)*env[input&1]; //Noise at max volume!
				++input;
			} while (--left); //Next input!
			break;
		case 3: //Noise+Frequency?
			do //Check all inputs!
			{
				if (input&2) //Tone high state?
				{
					chip->channels[channel].ampenv[input] = env[input&1]; //Noise at max volume!
				}
				else if ((input&4)==0) //Tone low and noise is low? Low at full amplitude!
				{
					chip->channels[channel].ampenv[input] = -env[input&1]; //Noise at max volume!
				}
				else //Tone low and noise is high? 50% amplitude!
				{
					chip->channels[channel].ampenv[input] = env[input&1]>>1; //Noise at half volume!
				}
				++input;
			} while (--left); //Next input!
			break;
		default: //Safety check
			break; //Ignore!
	}
#endif
}

OPTINLINE void tickSAAEnvelope(SAA1099 *chip, byte channel)
{
	static byte basechannels[2] = {0,3}; //The base channels!
	byte basechannel;
	channel &= 1; //Only two channels available!
	basechannel = basechannels[channel]; //Base channel!
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
		chip->channels[basechannel].envelope[0] = chip->channels[basechannel+1].envelope[0] = chip->channels[basechannel+2].envelope[0] = (SAAEnvelope(mode,step)&mask); //Apply the normal envelope!
		if (chip->env_reverse_right[channel]) //Reverse right envelope?
		{
			chip->channels[basechannel].envelope[1] = chip->channels[basechannel+1].envelope[1] = chip->channels[basechannel+2].envelope[1] = ((0xF-SAAEnvelope(mode,step))&mask); //Apply the reversed envelope!
		}
		else //Normal right envelope?
		{
			chip->channels[basechannel].envelope[1] = chip->channels[basechannel+1].envelope[1] = chip->channels[basechannel+2].envelope[1] = (SAAEnvelope(mode,step)&mask); //Apply the normal envelope!
		}
	}
	else //Envelope mode off, set all envelope factors to 16!
	{
		chip->channels[basechannel].envelope[0] = chip->channels[basechannel].envelope[1] = 
			chip->channels[basechannel+1].envelope[0] = chip->channels[basechannel+1].envelope[1] =
			chip->channels[basechannel+2].envelope[0] = chip->channels[basechannel+2].envelope[1] = 0x10; //We're off!
	}
	updateAmpEnv(chip,basechannel); //Update the amplitude/envelope!
	updateAmpEnv(chip,basechannel+1); //Update the amplitude/envelope!
	updateAmpEnv(chip,basechannel+2); //Update the amplitude/envelope!
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

OPTINLINE void updateSAA1099RNGfrequency(SAA1099 *chip, byte channel)
{
	byte channel2=channel|8;
	if (chip->noise[channel].freq!=chip->squarewave[channel2].freq) //Frequency changed?
	{
		chip->squarewave[channel2].timeout = (uint_32)(__GAMEBLASTER_BASERATE/(double)(2.0*chip->noise[channel].freq)); //New timeout!
		chip->squarewave[channel2].timepoint = 0; //Reset the timepoint!
		chip->squarewave[channel2].freq = chip->noise[channel].freq; //We're updated!
	}
}

OPTINLINE void updateSAA1099frequency(SAA1099 *chip, byte channel) //on octave/frequency change!
{
	channel &= 7; //Safety on channel!
	chip->channels[channel].freq = (float)((double)((GAMEBLASTER.baseclock/512)<<chip->channels[channel].octave)/(double)(511.0-chip->channels[channel].frequency)); //Calculate the current frequency to use!
	if (chip->channels[channel].freq!=chip->squarewave[channel].freq) //Frequency changed?
	{
		chip->squarewave[channel].timeout = (uint_32)(__GAMEBLASTER_BASERATE/(double)(2.0*chip->channels[channel].freq)); //New timeout!
		chip->squarewave[channel].timepoint = 0; //Reset!
		chip->squarewave[channel].freq = chip->channels[channel].freq; //We're updated!
	}
}

OPTINLINE void writeSAA1099Value(SAA1099 *chip, byte value)
{
	INLINEREGISTER byte reg;
	reg = chip->regsel; //The selected register to write to!
	chip->registers[reg] = value; //Save the register data itself!
	byte oldval,updated; //For detecting updates!
	word oldvalw;
	switch (reg) //What register is written?
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05: //Channel n amplitude?
			reg &= 7;
			oldvalw = chip->channels[reg].amplitude[0];
			chip->channels[reg].amplitude[0] = calcAmplitude(value&0xF);
			updated = (chip->channels[reg].amplitude[0]!=oldvalw); //Changed?

			oldvalw = chip->channels[reg].amplitude[1];
			chip->channels[reg].amplitude[1] = calcAmplitude(value>>4);
			updated |= (chip->channels[reg].amplitude[1]!=oldvalw); //Changed?

			if (updated) updateAmpEnv(chip,reg); //Update amplitude/envelope!
			break;
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0B:
		case 0x0C:
		case 0x0D: //Channel n frequency?
			reg &= 7;
			oldval = chip->channels[reg].frequency;
			chip->channels[reg].frequency = value; //Set the frequency!
			if (oldval!=chip->channels[reg].frequency) updateSAA1099frequency(chip,reg); //Update frequency!
			break;
		case 0x10:
		case 0x11:
		case 0x12: //Channel n octave?
			reg &= 3;
			oldval = chip->channels[reg<<1].octave;
			chip->channels[reg<<1].octave = (value&7);
			if (oldval!=chip->channels[reg<<1].octave) updateSAA1099frequency(chip,(reg<<1)); //Update frequency!
			
			oldval = chip->channels[(reg<<1)|1].octave;
			chip->channels[(reg<<1)|1].octave = ((value>>4)&7);
			if (oldval!=chip->channels[(reg<<1)|1].octave) updateSAA1099frequency(chip,((reg<<1)|1)); //Update frequency!
			break;
		case 0x14: //Channel n frequency enable?
			oldval = chip->channels[0].frequency_enable;
			chip->channels[0].frequency_enable = (value&1);
			if (oldval!=chip->channels[0].frequency_enable) updateAmpEnv(chip,0); //Update AmpEnv!

			value >>= 1;
			oldval = chip->channels[1].frequency_enable;
			chip->channels[1].frequency_enable = (value&1);
			if (oldval!=chip->channels[1].frequency_enable) updateAmpEnv(chip,1); //Update AmpEnv!

			value >>= 1;
			oldval = chip->channels[2].frequency_enable;
			chip->channels[2].frequency_enable = (value&1);
			if (oldval!=chip->channels[2].frequency_enable) updateAmpEnv(chip,2); //Update AmpEnv!

			value >>= 1;
			oldval = chip->channels[3].frequency_enable;
			chip->channels[3].frequency_enable = (value&1);
			if (oldval!=chip->channels[3].frequency_enable) updateAmpEnv(chip,3); //Update AmpEnv!

			value >>= 1;
			oldval = chip->channels[4].frequency_enable;
			chip->channels[4].frequency_enable = (value&1);
			if (oldval!=chip->channels[4].frequency_enable) updateAmpEnv(chip,4); //Update AmpEnv!

			value >>= 1;
			oldval = chip->channels[5].frequency_enable;
			chip->channels[5].frequency_enable = (value&1);
			if (oldval!=chip->channels[5].frequency_enable) updateAmpEnv(chip,5); //Update AmpEnv!
			break;
		case 0x15: //Channel n noise enable?
			reg = value; //Load for processing!
			oldval = chip->channels[0].noise_enable;
			chip->channels[0].noise_enable = (reg&1);
			if (oldval!=chip->channels[0].noise_enable) updateAmpEnv(chip,0); //Update AmpEnv!

			reg >>= 1;
			oldval = chip->channels[1].noise_enable;
			chip->channels[1].noise_enable = (reg&1);
			if (oldval!=chip->channels[1].noise_enable) updateAmpEnv(chip,1); //Update AmpEnv!

			reg >>= 1;
			oldval = chip->channels[2].noise_enable;
			chip->channels[2].noise_enable = (reg&1);
			if (oldval!=chip->channels[2].noise_enable) updateAmpEnv(chip,2); //Update AmpEnv!

			reg >>= 1;
			oldval = chip->channels[3].noise_enable;
			chip->channels[3].noise_enable = (reg&1);
			if (oldval!=chip->channels[3].noise_enable) updateAmpEnv(chip,3); //Update AmpEnv!

			reg >>= 1;
			oldval = chip->channels[4].noise_enable;
			chip->channels[4].noise_enable = (reg&1);
			if (oldval!=chip->channels[4].noise_enable) updateAmpEnv(chip,4); //Update AmpEnv!

			reg >>= 1;
			oldval = chip->channels[5].noise_enable;
			chip->channels[5].noise_enable = (reg&1);
			if (oldval!=chip->channels[5].noise_enable) updateAmpEnv(chip,5); //Update AmpEnv!
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
					chip->squarewave[reg].timepoint = 0;
					chip->squarewave[reg].output = 0; //Reset wave output signal voltage?
				}
			}
			break;
		default: //Unknown register?
			break; //Silently ignore invalid and unimplemented writes!
	}
}

OPTINLINE byte getSAA1099SquareWave(SAA1099 *chip, byte channel)
{
	byte result;
	uint_32 timepoint;
	result = chip->squarewave[channel].output; //Save the current output to give!
	timepoint = chip->squarewave[channel].timepoint; //Next timepoint!
	++timepoint; //Next timepoint!
	if (timepoint>=chip->squarewave[channel].timeout) //Timeout? Flip-flop!
	{
		chip->squarewave[channel].output = result^2; //Flip-flop to produce a square wave! We're bit 1 of the output!
		timepoint = 0; //Reset the timepoint!
	}
	chip->squarewave[channel].timepoint = timepoint; //Save the resulting timepoint to advance the wave!
	return result; //Give the resulting square wave!
}

OPTINLINE void generateSAA1099channelsample(SAA1099 *chip, byte channel, int_32 *output_l, int_32 *output_r)
{
	byte output;
	channel &= 7;
	chip->channels[channel].level = getSAA1099SquareWave(chip,channel); //Current flipflop output of the square wave generator!

	//Tick the envelopes when needed!
	if ((channel==1) && (chip->env_clock[0]==0))
		tickSAAEnvelope(chip,0);
	if ((channel==4) && (chip->env_clock[1]==0))
		tickSAAEnvelope(chip,1);

	output = chip->noise[chip->channels[channel].noisechannel].levelbit; //Use noise? If the noise level is high (noise 0 for channel 0-2, noise 1 for channel 3-5); Level bit 0 taken always to bit 2!
	output |= chip->channels[channel].level; //Level is always 1-bit! Level to bit 2!
	//Check and apply for noise! Substract to avoid overflows, half amplitude only
	*output_l += chip->channels[channel].ampenv[output]; //Output left!
	*output_r += chip->channels[channel].ampenv[output|1]; //Output right!
}

OPTINLINE void tickSAA1099noise(SAA1099 *chip, byte channel)
{
	byte noise_flipflop;

	channel &= 1; //Only two channels!

	//Check the current noise generators and update them!
	//Noise channel output!
	noise_flipflop = getSAA1099SquareWave(chip,channel|8); //Current flipflop output of the noise timer!
	if (noise_flipflop & (noise_flipflop ^ chip->noise[channel].laststatus)) //High and risen?
	{
		if (((chip->noise[channel].level & 0x20000) == 0) == ((chip->noise[channel].level & 0x0400) == 0))
			chip->noise[channel].level = (chip->noise[channel].level << 1) | 1;
		else
			chip->noise[channel].level <<= 1;
		chip->noise[channel].levelbit = ((chip->noise[channel].level&1)<<2); //Current level bit has been updated, preshifted to bit 2 of the output!
	}
	chip->noise[channel].laststatus = noise_flipflop; //Save the last status!
}

float noise_frequencies[3] = {31250.0f*2.0f,15625.0f*2.0f,7812.0f*2.0f}; //Normal frequencies!

OPTINLINE void generateSAA1099sample(SAA1099 *chip, sword *leftsample, sword *rightsample) //Generate a sample on the requested chip!
{
	int_32 output_l, output_r;

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
	updateSAA1099RNGfrequency(chip,0);

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
	updateSAA1099RNGfrequency(chip,1);

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

	output_l = (int_32)(((float)output_l)*AMPLIFIER); //Left channel output!
	output_r = (int_32)(((float)output_r)*AMPLIFIER); //Right channel output!

	output_l = LIMITRANGE(output_l, SHRT_MIN, SHRT_MAX); //Clip our data to prevent overflow!
	output_r = LIMITRANGE(output_r, SHRT_MIN, SHRT_MAX); //Clip our data to prevent overflow!

	*leftsample = (sword)output_l; //Left sample result!
	*rightsample = (sword)output_r; //Right sample result!
}

uint_32 gameblaster_soundtiming=0;
uint_32 gameblaster_rendertiming=0;

void updateGameBlaster(uint_32 MHZ14passed)
{
	static sword leftsample[2]={0,0}, rightsample[2]={0,0}; //Two stereo samples!
	if (GAMEBLASTER.baseaddr==0) return; //No game blaster?
	//Game Blaster sound output
	gameblaster_soundtiming += MHZ14passed; //Get the amount of time passed!
	if (gameblaster_soundtiming>=MHZ14_BASETICK)
	{
		for (;gameblaster_soundtiming>=MHZ14_BASETICK;)
		{
			float leftsamplef[2], rightsamplef[2]; //Two stereo samples, floating point format!
			//Generate the sample!

			if (GAMEBLASTER.chips[0].all_ch_enable) //Sound generation of first chip?
			{
				generateSAA1099sample(&GAMEBLASTER.chips[0],&leftsample[0],&rightsample[0]); //Generate a stereo sample on this chip!
			}
			else
			{
				leftsample[0] = rightsample[0] = 0; //No sample!
			}

			if (GAMEBLASTER.chips[1].all_ch_enable) //Sound generation of first chip?
			{
				generateSAA1099sample(&GAMEBLASTER.chips[1], &leftsample[1], &rightsample[1]); //Generate a stereo sample on this chip!
			}
			else
			{
				leftsample[1] = rightsample[1] = 0; //No sample!
			}

			gameblaster_soundtiming -= MHZ14_BASETICK; //Decrease timer to get time left!

			//Convert to floating point to apply filters!
			leftsamplef[0] = (float)leftsample[0];
			rightsamplef[0] = (float)rightsample[0];
			leftsamplef[1] = (float)leftsample[1];
			rightsamplef[1] = (float)rightsample[1];
			//Low-pass filters!
			/*
			applySoundFilter(&GAMEBLASTER.filter[0],&leftsamplef[0]); //Filter low-pass left!
			applySoundFilter(&GAMEBLASTER.filter[1],&leftsamplef[1]); //Filter low-pass left!
			applySoundFilter(&GAMEBLASTER.filter[2],&rightsamplef[0]); //Filter low-pass right!
			applySoundFilter(&GAMEBLASTER.filter[3],&rightsamplef[1]); //Filter low-pass right!
			*/
			//High-pass filters!
			/*
			applySoundFilter(&GAMEBLASTER.filter[4],&leftsamplef[0]); //Filter high-pass left!
			applySoundFilter(&GAMEBLASTER.filter[5],&leftsamplef[1]); //Filter high-pass left!
			applySoundFilter(&GAMEBLASTER.filter[6],&rightsamplef[0]); //Filter high-pass right!
			applySoundFilter(&GAMEBLASTER.filter[7],&rightsamplef[1]); //Filter high-pass right!
			*/
			//Move back to samples!
			leftsample[0] = (sword)leftsamplef[0];
			rightsample[0] = (sword)rightsamplef[0];
			leftsample[1] = (sword)leftsamplef[1];
			rightsample[1] = (sword)rightsamplef[1];
		}
	}

	gameblaster_rendertiming += MHZ14passed; //Tick the base by our passed time!
	if (gameblaster_rendertiming>=MHZ14_RENDERTICK) //To render a sample or more samples?
	{
		for (;gameblaster_rendertiming>=MHZ14_RENDERTICK;)
		{
			//Now push the samples to the output!
			writeDoubleBufferedSound32(&GAMEBLASTER.soundbuffer[0],(signed2unsigned16(rightsample[0])<<16)|signed2unsigned16(leftsample[0])); //Output the sample to the renderer!
			writeDoubleBufferedSound32(&GAMEBLASTER.soundbuffer[1],(signed2unsigned16(rightsample[1])<<16)|signed2unsigned16(leftsample[1])); //Output the sample to the renderer!
			gameblaster_rendertiming -= MHZ14_RENDERTICK; //Tick the renderer by our passed time!
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
			mono_converter = LIMITRANGE(mono_converter, SHRT_MIN, SHRT_MAX); //Clip our data to prevent overflow!
			*data_mono++ = mono_converter; //Save the sample and point to the next mono sample!
			if (!--c) return SOUNDHANDLER_RESULT_FILLED; //Next item!
		}
	}
}

byte outGameBlaster(word port, byte value)
{
	if (__HW_DISABLED) return 0; //We're disabled!
	if ((port&~0xF)!=GAMEBLASTER.baseaddr) return 0; //Not Game Blaster port!
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
			if (GAMEBLASTER.soundblastercompatible>1) return 0; //Ignore all other addresses!
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
	if ((port&~0xF)!=GAMEBLASTER.baseaddr) return 0; //Not Game Blaster port!
	switch (port&0xF)
	{
		case 0: //Left SAA-1099?
		case 1: //Left SAA-1099?
		case 2: //Right SAA-1099?
		case 3: //Right SAA-1099?
			return 0; //Not Handled! The chips cannot be read, only written!
		default: //Other addresses(16 addresses)? CT-1302!
			if (GAMEBLASTER.soundblastercompatible>1) return 0; //Ignore all other addresses!
			switch (port&0xF) //What port?
			{
				case 0x4: //Detection!
					*result = 0x7F; //Give the detection value!
					return 1; //Handled!					
				case 0xA: //Store 1!
					if (GAMEBLASTER.soundblastercompatible) return 0; //Sound blaster compatibility?
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
	GAMEBLASTER.soundblastercompatible = useSoundBlasterIO?1:0; //Sound Blaster compatible I/O? Use 2 chips only with sound blaster, else full 16 ports for detection!
}

void GameBlaster_setVolume(float volume)
{
	if (__HW_DISABLED) return; //We're disabled!
	setVolume(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[0], volume); //Set the volume!
	setVolume(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[1], volume); //Set the volume!
}

void initGameBlaster(word baseaddr)
{
	byte channel;
	if (__HW_DISABLED) return; //We're disabled!
	memset(&GAMEBLASTER,0,sizeof(GAMEBLASTER)); //Full init!
	GAMEBLASTER.baseaddr = baseaddr; //Base address of the Game Blaster!
	setGameBlaster_SoundBlaster(0); //Default to Game Blaster I/O!

	if (allocDoubleBufferedSound32(__GAMEBLASTER_SAMPLEBUFFERSIZE,&GAMEBLASTER.soundbuffer[0],0,__GAMEBLASTER_SAMPLERATE)) //Valid buffer?
	{
		if (allocDoubleBufferedSound32(__GAMEBLASTER_SAMPLEBUFFERSIZE,&GAMEBLASTER.soundbuffer[1],0,__GAMEBLASTER_SAMPLERATE)) //Valid buffer?
		{
			if (!addchannel(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[0],"GameBlaster",(float)__GAMEBLASTER_SAMPLERATE,__GAMEBLASTER_SAMPLEBUFFERSIZE,0,SMPL16S)) //Start the sound emulation (mono) with automatic samples buffer?
			{
				dolog("GameBlaster","Error registering sound channel for output!");
			}
			else
			{
				setVolume(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[0],__GAMEBLASTER_VOLUME);
				if (!addchannel(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[1],"GameBlaster",(float)__GAMEBLASTER_SAMPLERATE,__GAMEBLASTER_SAMPLEBUFFERSIZE,0,SMPL16S)) //Start the sound emulation (mono) with automatic samples buffer?
				{
					dolog("GameBlaster","Error registering sound channel for output!");
				}
				else
				{
					setVolume(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[1],__GAMEBLASTER_VOLUME);
					GAMEBLASTER.storelatch[0] = GAMEBLASTER.storelatch[1] = 0xFF; //Initialise our latches!
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

	AMPLIFIER = (float)__GAMEBLASTER_AMPLIFIER; //Set the amplifier to use!
	GAMEBLASTER.baseclock = (uint_32)(MHZ14/2); //We're currently clocking at the sample rate!
	noise_frequencies[0] = (float)((float)GAMEBLASTER.baseclock/256.0);
	noise_frequencies[1] = (float)((float)GAMEBLASTER.baseclock/512.0);
	noise_frequencies[2] = (float)((float)GAMEBLASTER.baseclock/1024.0);

	initSoundFilter(&GAMEBLASTER.filter[0],0,(float)(__GAMEBLASTER_SAMPLERATE/2.0),(float)__GAMEBLASTER_BASERATE); //Low-pass filter used left at nyquist!
	initSoundFilter(&GAMEBLASTER.filter[1],0,(float)(__GAMEBLASTER_SAMPLERATE/2.0),(float)__GAMEBLASTER_BASERATE); //Low-pass filter used left at nyquist!
	initSoundFilter(&GAMEBLASTER.filter[2],0,(float)(__GAMEBLASTER_SAMPLERATE/2.0),(float)__GAMEBLASTER_BASERATE); //Low-pass filter used right at nyquist!
	initSoundFilter(&GAMEBLASTER.filter[3],0,(float)(__GAMEBLASTER_SAMPLERATE/2.0),(float)__GAMEBLASTER_BASERATE); //Low-pass filter used right at nyquist!
	initSoundFilter(&GAMEBLASTER.filter[4],1,18.2f,(float)__GAMEBLASTER_BASERATE); //High-pass filter used left!
	initSoundFilter(&GAMEBLASTER.filter[5],1,18.2f,(float)__GAMEBLASTER_BASERATE); //High-pass filter used left!
	initSoundFilter(&GAMEBLASTER.filter[6],1,18.2f,(float)__GAMEBLASTER_BASERATE); //High-pass filter used right!
	initSoundFilter(&GAMEBLASTER.filter[7],1,18.2f,(float)__GAMEBLASTER_BASERATE); //High-pass filter used right!

	/*

	Test values!

	*/
#ifdef TESTWAVE
	//Load test wave information for generating samples!
	GAMEBLASTER.chips[0].squarewave[7].timeout = (uint_32)(__GAMEBLASTER_BASERATE/(double)(440.0f*2.0f)); //New timeout!
	GAMEBLASTER.chips[0].squarewave[7].timepoint = 0; //Reset!
	GAMEBLASTER.chips[0].squarewave[7].freq = 440.0f; //We're updated!

	WAVEFILE *testoutput=NULL;

	uint_32 i;
	byte signal;

	testoutput = createWAV("captures/testgameblaster440hz.wav",1,(uint_32)__GAMEBLASTER_BASERATE); //Start the log!

	for (i=0;i<__GAMEBLASTER_BASERATE;++i) //Generate one second of data!
	{
		signal = getSAA1099SquareWave(&GAMEBLASTER.chips[0],7);
		writeWAVMonoSample(testoutput,signed2unsigned16(signal?(sword)32767:(sword)-32768)); //Write a sample!
	}

	closeWAV(&testoutput); //Close the wave file!
#endif
	//End test

	for (channel=0;channel<8;++channel) //Init all channels, when needed!
	{
		updateSAA1099frequency(&GAMEBLASTER.chips[0],channel); //Init frequency!
		updateSAA1099frequency(&GAMEBLASTER.chips[1],channel); //Init frequency!
		GAMEBLASTER.chips[0].channels[channel].noisechannel = (channel/3); //Our noise channel linked to this channel!
	}
	updateSAA1099RNGfrequency(&GAMEBLASTER.chips[0],0); //Init frequency!
	updateSAA1099RNGfrequency(&GAMEBLASTER.chips[1],0); //Init frequency!
	updateSAA1099RNGfrequency(&GAMEBLASTER.chips[0],1); //Init frequency!
	updateSAA1099RNGfrequency(&GAMEBLASTER.chips[1],1); //Init frequency!

	gameblaster_rendertiming = gameblaster_soundtiming = 0; //Reset rendering!
}

void doneGameBlaster()
{
	removechannel(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[0],0); //Stop the sound emulation?
	freeDoubleBufferedSound(&GAMEBLASTER.soundbuffer[0]);
	removechannel(&GameBlaster_soundGenerator,&GAMEBLASTER.soundbuffer[1],0); //Stop the sound emulation?
	freeDoubleBufferedSound(&GAMEBLASTER.soundbuffer[1]);
}