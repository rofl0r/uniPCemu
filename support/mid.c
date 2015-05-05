#include "headers/types.h"
#include "headers/support/zalloc.h" //Zalloc support!
#include "headers/hardware/ports.h" //MPU support!
#include "headers/emu/timers.h" //Timer support!
#include "headers/support/mid.h" //Our own typedefs!

//Enable this define to log all midi commands executed here!
//#define MID_LOG

#define MIDIHEADER_ID 0x6468544d
#define MIDIHEADER_TRACK_ID 0x6b72544d

word byteswap16(word value)
{
	return ((value & 0xFF) << 8) | ((value & 0xFF00) >> 8); //Byteswap!
}

uint_32 byteswap32(uint_32 value)
{
	return (byteswap16(value & 0xFFFF) << 8) | byteswap16((value & 0xFFFF0000) >> 16); //
}

uint_32 activetempo = 500000; //Current tempo!

word readMID(char *filename, HEADER_CHNK *header, TRACK_CHNK *tracks, byte **channels, word maxchannels)
{
	FILE *f;
	TRACK_CHNK currenttrack;
	TRACK_CHNK *curtracks = tracks;
	word currenttrackn = 0; //Ammount of tracks loaded!
	uint_32 tracklength;

	byte *data;
	f = fopen(filename, "rb"); //Try to open!
	if (!f) return 0; //Error: file not found!
	if (fread(header, 1, sizeof(*header), f) != sizeof(*header))
	{
		fclose(f);
		return 0; //Error reading header!
	}
	if (header->Header != MIDIHEADER_ID)
	{
		fclose(f);
		return 0; //Nothing!
	}
	if (byteswap32(header->header_length) != 6)
	{
		fclose(f);
		return 0; //Nothing!
	}
	if (byteswap16(header->format)>1) //Not single/multiple tracks played simultaneously?
	{
		fclose(f);
		return 0; //Not single track!
	}
	nexttrack: //Read the next track!
	if (fread(&currenttrack, 1, sizeof(currenttrack), f) != sizeof(currenttrack)) //Error in track?
	{
		fclose(f);
		return 0; //Invalid track!
	}
	if (currenttrack.Header != MIDIHEADER_TRACK_ID) //Not a track ID?
	{
		fclose(f);
		return 0; //Invalid track header!
	}
	if (!currenttrack.length) //No length?
	{
		fclose(f);
		return 0; //Invalid track length!
	}
	tracklength = byteswap32(currenttrack.length); //Calculate the length of the track!
	data = zalloc(tracklength+sizeof(uint_32),"MIDI_DATA",NULL); //Allocate data and cursor!
	if (!data) //Ran out of memory?
	{
		fclose(f);
		return 0; //Ran out of memory!
	}
	if (fread(data+sizeof(uint_32), 1, tracklength, f) != tracklength) //Error reading data?
	{
		fclose(f);
		freez((void **)&data, tracklength+sizeof(uint_32), "MIDI_DATA");
		return 0; //Error reading data!
	}

	++currenttrackn; //Increase the number of tracks loaded!
	if (currenttrackn > maxchannels) //Limit broken?
	{
		freez((void **)&data, tracklength+sizeof(uint_32), "MIDI_DATA");
		return 0; //Limit broken: we can't store the file!
	}

	channels[currenttrackn - 1] = data; //Safe the pointer to the data!
	memcpy(tracks, &currenttrack, sizeof(currenttrack)); //Copy track information!
	++tracks; //Next track!
	if (/*(byteswap16(header->format) > 0) &&*/ (currenttrackn<byteswap16(header->n))) //Format 1? Take all tracks!
	{
		goto nexttrack; //Next track to check!
	}

	/*if (!feof(f)) //Not @EOF when required?
	{
		fclose(f);
		freez((void **)data, byteswap32(currenttrack.length), "MIDI_DATA"); //Release current if there!
		return 0; //Incomplete file!
	}*/
	fclose(f);
	activetempo = 500000; //Default = 120BPM = 500000 microseconds/quarter note!
	return currenttrackn; //Give the result: the ammount of tracks loaded!
}

void freeMID(TRACK_CHNK *tracks, byte **channels, word numchannels)
{
	uint_32 channelnr;
	for (channelnr = 0; channelnr < numchannels; channelnr++)
	{
		freez((void **)&channels[channelnr], byteswap32(tracks[channelnr].length)+sizeof(uint_32), "MIDI_DATA"); //Try to free!
	}
}

byte consumeStream(byte *stream, TRACK_CHNK *track, byte *result)
{
	byte *streamdata = stream + sizeof(uint_32); //Start of the data!
	uint_32 *streampos = (uint_32 *)stream; //Position!
	if (!memprotect(streampos, 4, "MIDI_DATA")) return 0; //Error: Invalid stream!
	if (*streampos >= byteswap32(track->length)) return 0; //End of stream reached!
	if (!memprotect(&streamdata[*streampos], 1, "MIDI_DATA")) return 0; //Error: Invalid data!
	*result = streamdata[*streampos]; //Read the data!
	++(*streampos); //Increase pointer in the stream!
	return 1; //Consumed!
}

byte peekStream(byte *stream, TRACK_CHNK *track, byte *result)
{
	byte *streamdata = stream + sizeof(uint_32); //Start of the data!
	uint_32 *streampos = (uint_32 *)stream; //Position!
	if (!memprotect(streampos, 4, "MIDI_DATA")) return 0; //Error: Invalid stream!
	if (*streampos >= byteswap32(track->length)) return 0; //End of stream reached!
	if (!memprotect(&streamdata[*streampos], 1, "MIDI_DATA")) return 0; //Error: Invalid data!
	*result = streamdata[*streampos]; //Read the data!
	return 1; //Consumed!
}

byte read_VLV(byte *midi_stream, TRACK_CHNK *track, uint_32 *result)
{
	uint_32 temp = 0;
	byte curdata;
	if (!consumeStream(midi_stream, track, &curdata)) return 0; //Read first VLV failed?
	for (;;) //Process/read the VLV!
	{
		temp |= (curdata & 0x7F); //Add to length!
		if (!(curdata & 0x80)) break; //No byte to follow?
		temp <<= 7; //Make some room for the next byte!
		if (!consumeStream(midi_stream, track, &curdata)) return 0; //Read VLV failed?
	}
	*result = temp; //Give the result!
	return 1; //OK!
}

float calcfreq(uint_32 tempo, HEADER_CHNK *header)
{
	float PPQN, speed;
	byte frames;
	byte subframes; //Pulses per quarter note!
	word division;
	division = byteswap16(header->division); //Byte swap!

	if (division & 0x8000) //SMTPE?
	{
		frames = (float)((division >> 8)&0x7F); //Frames!
		subframes = (float)(division & 0xFF); //Subframes!
		speed = frames; //Default: we're the frames!
		if (subframes)
		{
			//We don't use the tempo: our rate is fixed!
			frames *= subframes; //The result in subframes/second!
		}
		speed = frames; //Use (sub)frames!
	}
	else
	{
		//PPQN method!
		PPQN = (float)division; //Read PPQN!
		speed = (float)tempo; //Convert to speed!
		speed /= 1000000.0f; //Divide by 1 second!
		speed /= PPQN; //Divide to get the ticks per second!
		speed = 1.0f / speed; //Ammount per second!
		speed *= 0.5f;
	}

	//We're counting in ticks!
	return speed; //ticks per second!
}

//The protective semaphore for our flags!
SDL_sem *MID_timing_pos_Lock = NULL; //Timing position lock!
SDL_sem *MID_BPM_Lock = NULL; //BPM/Active tempo lock!

uint_64 timing_pos = 0; //Current timing position!
float BPM = 0.0f; //No BPM by default!

void updateMIDTimer(HEADER_CHNK *header) //Request an update of our timer!
{
	addtimer(calcfreq(activetempo, header), (Handler)&timing_pos, "MID_tempotimer", 0, 2, MID_timing_pos_Lock); //Add a counter timer!
}

//The protective semaphore for the hardware!
SDL_sem *MIDLock = NULL;

#define MIDI_ERROR(position) {error = position; goto abortMIDI;}

void playMIDIStream(word channel, byte *midi_stream, HEADER_CHNK *header, TRACK_CHNK *track)
{
	byte curdata;

	//Metadata event!
	byte meta_type;
	uint_32 length, length_counter; //Our metadata variable length!

	uint_64 play_pos = 0; //Current play position!

	uint_32 delta_time; //Delta time!

	byte last_command = 0; //Last executed command!

	int_32 error = 0; //Default: no error!
	for (;;) //Playing?
	{
		//First, timing information and timing itself!
		if (!read_VLV(midi_stream, track, &delta_time)) return; //Read VLV time index!
		play_pos += delta_time; //Add the delta time to the playing position!
		for (;;)
		{
			//Lock
			SDL_SemWait(MID_timing_pos_Lock);
			if (timing_pos >= play_pos)
			{
				//Unlock
				SDL_SemPost(MID_timing_pos_Lock);
				break; //Arrived? Play!
			}
			//Unlock
			SDL_SemPost(MID_timing_pos_Lock);
			delay(0); //Wait for our tick!
		}

		if (!peekStream(midi_stream,track, &curdata))
		{
			return; //Failed to peek!
		}
		if (curdata == 0xFF) //System?
		{
			if (!consumeStream(midi_stream, track, &curdata)) return; //EOS!
			if (!consumeStream(midi_stream, track, &meta_type)) return; //Meta type failed? Give error!
			if (!read_VLV(midi_stream, track, &length)) return; //Error: unexpected EOS!
			switch (meta_type) //What event?
			{
				case 0x2F: //EOT?
					#ifdef MID_LOG
					dolog("MID", "channel %i: EOT!", channel); //EOT reached!
					#endif
					return; //End of track reached: done!
				case 0x51: //Set tempo?
					//Lock
					SDL_SemWait(MID_BPM_Lock);
					removetimer("MID_tempotimer"); //Remove old timer!

					if (!consumeStream(midi_stream, track, &curdata)) return; //Tempo 1/3 failed?
					activetempo = curdata; //Final byte!
					activetempo <<= 8;
					if (!consumeStream(midi_stream, track, &curdata)) return; //Tempo 2/3 failed?
					activetempo |= curdata; //Final byte!
					activetempo <<= 8;
					if (!consumeStream(midi_stream, track, &curdata)) return; //Tempo 3/3 failed?
					activetempo |= curdata; //Final byte!
					//Tempo = us per quarter note!

					updateMIDTimer(header);

					#ifdef MID_LOG
					dolog("MID", "channel %i: Set Tempo:%06X!", channel, activetempo);
					#endif

					//Unlock
					SDL_SemPost(MID_BPM_Lock);
					break;
				default: //Unrecognised meta event? Skip it!
					#ifdef MID_LOG
					dolog("MID", "Unrecognised meta type: %02X@Channel %i; Data length: %i", meta_type, channel, length); //Log the unrecognised metadata type!
					#endif
					for (; length--;) //Process length bytes!
					{
						if (!consumeStream(midi_stream, track, &curdata)) return; //Skip failed?
					}
					break;
			}
		}
		else //Hardware?
		{
			//Lock
			SDL_SemWait(MIDLock);

			if (curdata & 0x80) //Starting a new command?
			{
				#ifdef MID_LOG
				dolog("MID", "Status@Channel %i=%02X", channel, curdata);
				#endif
				if (!consumeStream(midi_stream, track, &curdata)) MIDI_ERROR(1) //EOS!
				last_command = curdata; //Save the last command!
				if (last_command != 0xF7) //Escaped continue isn't sent!
				{
					PORT_OUT_B(0x330, last_command); //Send the command!
				}
			}
			else
			{
				#ifdef MID_LOG
				dolog("MID", "Continued status@Channel %i: %02X=>%02X",channel, last_command, curdata);
				#endif
				if (last_command != 0xF7) //Escaped continue isn't used last?
				{
					PORT_OUT_B(0x330, last_command); //Repeat the status bytes: we don't know what the other channels do!
				}
			}

			//Process the data for the command!
			switch ((last_command >> 4) & 0xF) //What command to send data for?
			{
			case 0xF: //Special?
				switch (last_command & 0xF) //What subcommand are we sending?
				{
				case 0x0: //System exclusive?
				case 0x7: //Escaped continue?
					if (!read_VLV(midi_stream, track, &length)) MIDI_ERROR(2) //Error: unexpected EOS!
					length_counter = 3; //Initialise our position!
					for (; length--;) //Transmit the packet!
					{
						if (!consumeStream(midi_stream, track, &curdata)) MIDI_ERROR(length_counter++) //EOS!
						PORT_OUT_B(0x330, curdata); //Send the byte!
					}
					break;
				case 0x1:
				case 0x3:
					//1 byte follows!
					if (!consumeStream(midi_stream, track, &curdata)) MIDI_ERROR(2) //EOS!
					PORT_OUT_B(0x330, curdata); //Passthrough to MIDI!
					break;
				case 0x2:
					//2 bytes follow!
					if (!consumeStream(midi_stream, track, &curdata)) MIDI_ERROR(2) //EOS!
					PORT_OUT_B(0x330, curdata); //Passthrough to MIDI!
					if (!consumeStream(midi_stream, track, &curdata)) MIDI_ERROR(3) //EOS!
					PORT_OUT_B(0x330, curdata); //Passthrough to MIDI!
					break;
				default: //Unknown special instruction?
					break; //Single byte instruction?
				}
				break;
			case 0x8: //Note off?
			case 0x9: //Note on?
			case 0xA: //Aftertouch?
			case 0xB: //Control change?
			case 0xE: //Pitch bend?
				//2 bytes follow!
				if (!consumeStream(midi_stream, track, &curdata)) MIDI_ERROR(2) //EOS!
				PORT_OUT_B(0x330, curdata); //Passthrough to MIDI!
				if (!consumeStream(midi_stream, track, &curdata)) MIDI_ERROR(3) //EOS!
				PORT_OUT_B(0x330, curdata); //Passthrough to MIDI!
				break;
			case 0xC: //Program change?
			case 0xD: //Channel pressure/aftertouch?
				//1 byte follows
				if (!consumeStream(midi_stream, track, &curdata)) MIDI_ERROR(2) //EOS!
				PORT_OUT_B(0x330, curdata); //Passthrough to MIDI!
				break;
			default: //Unknown data? We're sending directly to the hardware! We shouldn't be here!
				if (!consumeStream(midi_stream, track, &curdata)) MIDI_ERROR(2) //EOS!
				dolog("MID", "Warning: Unknown data detected@channel %i: passthrough to MIDI device: %02X!", channel, curdata);
				//Can't process: ignore the data, since it's invalid!
				break;
			}
		abortMIDI:
			//Unlock
			if (error)
			{
				PORT_OUT_B(0x330, 0xFF); //Reset the synthesizer!
				dolog("MID", "channel %i: Error @position %i during MID processing! Unexpected EOS? Last command: %02X, Current data: %02X", channel, error, last_command, curdata);
				SDL_SemPost(MIDLock); //Finish up!
				return; //Abort on error!
			}
			SDL_SemPost(MIDLock); //Finish: prepare for next command!
		}
	}
}