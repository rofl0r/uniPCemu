#include "headers/types.h"
#include "headers/support/zalloc.h" //Zalloc support!
#include "headers/hardware/ports.h" //MPU support!
#include "headers/emu/threads.h" //Thread support!
#include "headers/emu/timers.h" //Timer support!
#include "headers/support/mid.h" //Our own typedefs!
#include "headers/emu/gpu/gpu_text.h" //Text surface support!
#include "headers/hardware/midi/mididevice.h" //For the MIDI voices!
#include "headers/support/log.h" //Logging support!

//Enable this define to log all midi commands executed here!
//#define MID_LOG

#define MIDIHEADER_ID 0x6468544d
#define MIDIHEADER_TRACK_ID 0x6b72544d

#include "headers/packed.h"
typedef struct PACKED
{
	uint_32 Header; //MThd
	uint_32 header_length;
	word format; //0=Single track, 1=Multiple track, 2=Multiple song file format (multiple type 0 files)
	word n; //Number of tracks that follow us
	sword division; //Positive: units per beat, negative:  SMPTE-compatible units.
} HEADER_CHNK;
#include "headers/endpacked.h"

#include "headers/packed.h"
typedef struct PACKED
{
	uint_32 Header; //MTrk
	uint_32 length; //Number of bytes in the chunk.
} TRACK_CHNK;
#include "headers/endpacked.h"

byte MID_TERM = 0; //MIDI termination flag!

//The protective semaphore for our flags!
SDL_sem *MID_timing_pos_Lock = NULL; //Timing position lock!
SDL_sem *MID_BPM_Lock = NULL; //BPM/Active tempo lock!
//The protective semaphore for the hardware!
SDL_sem *MIDLock = NULL;
SDL_sem *MID_channel_Lock = NULL; //Our channel lock for counting running MIDI!

uint_64 timing_pos = 0; //Current timing position!
uint_32 activetempo = 500000; //Current tempo!

word MID_RUNNING = 0; //How many channels are still running/current channel running!

//A loaded MIDI file!
HEADER_CHNK header;
byte *MID_data[100]; //Tempo and music track!
TRACK_CHNK MID_tracks[100];
word MID_tracknr[100];
OPTINLINE word byteswap16(word value)
{
	return ((value & 0xFF) << 8) | ((value & 0xFF00) >> 8); //Byteswap!
}

OPTINLINE uint_32 byteswap32(uint_32 value)
{
	return (byteswap16(value & 0xFFFF) << 8) | byteswap16((value & 0xFFFF0000) >> 16); //
}

OPTINLINE float calcfreq(uint_32 tempo, HEADER_CHNK *header)
{
	float speed;
	byte frames;
	byte subframes; //Pulses per quarter note!
	word division;
	division = byteswap16(header->division); //Byte swap!

	if (division & 0x8000) //SMTPE?
	{
		frames = (byte)((division >> 8) & 0x7F); //Frames!
		subframes = (byte)(division & 0xFF); //Subframes!
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
		//tempo=us/quarter note
		speed = (float)tempo; //Length of a quarter note in us!
		//speede is now the amount of quarter notes per second!
		speed /= (float)division; //Divide us by the PPQN(Pulses Per Quarter Note) to get the ammount of us/pulse!
		speed = 1000000.0f / speed; //Convert us/pulse to pulse/second!
		//Speed is now the ammount of pulses per second!
	}

	//We're counting in ticks!
	return speed; //ticks per second!
}

OPTINLINE void updateMIDTimer(HEADER_CHNK *header) //Request an update of our timer!
{
	addtimer(calcfreq(activetempo, header), (Handler)&timing_pos, "MID_tempotimer", 0, 2, MID_timing_pos_Lock); //Add a counter timer!
}

extern MIDIDEVICE_VOICE activevoices[__MIDI_NUMVOICES]; //All active voices!
extern GPU_TEXTSURFACE *frameratesurface; //Our framerate surface!

OPTINLINE void printMIDIChannelStatus()
{
	int i;
	uint_32 color; //The color to use!
	GPU_text_locksurface(frameratesurface); //Lock the surface!
	for (i = 0; i < __MIDI_NUMVOICES; i++) //Process all voices!
	{
		GPU_textgotoxy(frameratesurface, 0, i + 5); //Row 5+!
		if (activevoices[i].VolumeEnvelope.active) //Active voice?
		{
			color = RGB(0x00, 0xFF, 0x00); //The color to use!
			GPU_textprintf(frameratesurface, color, RGB(0xDD, 0xDD, 0xDD), "%02i", activevoices[i].VolumeEnvelope.active);
		}
		else //Inactive voice?
		{
			if (activevoices[i].play_counter) //We have been playing?
			{
				color = RGB(0xFF, 0xAA, 0x00);
				GPU_textprintf(frameratesurface, color, RGB(0xDD, 0xDD, 0xDD), "%02i", i);
			}
			else //Completely unused voice?
			{
				color = RGB(0xFF, 0x00, 0x00);
				GPU_textprintf(frameratesurface, color, RGB(0xDD, 0xDD, 0xDD), "%02i", i);
			}
		}
		if (activevoices[i].channel && activevoices[i].note) //Gotten assigned?
		{
			GPU_textprintf(frameratesurface, color, RGB(0xDD, 0xDD, 0xDD), " %04X %02X %02X", activevoices[i].channel->activebank, activevoices[i].channel->program, activevoices[i].note->note); //Dump information about the voice we're playing!
		}
	}
	GPU_text_releasesurface(frameratesurface); //Unlock the surface!
}

void resetMID() //Reset our settings for playback of a new file!
{
	WaitSem(MID_BPM_Lock) //Wait for the lock!
	activetempo = 500000; //Default = 120BPM = 500000 microseconds/quarter note!
	PostSem(MID_BPM_Lock) //Finish: we've set the BPM!

	WaitSem(MID_timing_pos_Lock) //Wait for the lock!
	timing_pos = 0; //Reset the timing for the current song!
	PostSem(MID_timing_pos_Lock) //Finish: we've set the timing position!

	MID_TERM = 0; //Reset termination flag!

	updateMIDTimer(&header); //Update the timer!
}

word readMID(char *filename, HEADER_CHNK *header, TRACK_CHNK *tracks, byte **channels, word maxchannels)
{
	FILE *f;
	TRACK_CHNK currenttrack;
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
	if ((currenttrackn<byteswap16(header->n))) //Format 1? Take all tracks!
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

OPTINLINE byte consumeStream(byte *stream, TRACK_CHNK *track, byte *result)
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

OPTINLINE byte peekStream(byte *stream, TRACK_CHNK *track, byte *result)
{
	byte *streamdata = stream + sizeof(uint_32); //Start of the data!
	uint_32 *streampos = (uint_32 *)stream; //Position!
	if (!memprotect(streampos, 4, "MIDI_DATA")) return 0; //Error: Invalid stream!
	if (*streampos >= byteswap32(track->length)) return 0; //End of stream reached!
	if (!memprotect(&streamdata[*streampos], 1, "MIDI_DATA")) return 0; //Error: Invalid data!
	*result = streamdata[*streampos]; //Read the data!
	return 1; //Consumed!
}

OPTINLINE byte read_VLV(byte *midi_stream, TRACK_CHNK *track, uint_32 *result)
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

#define MIDI_ERROR(position) {error = position; goto abortMIDI;}

OPTINLINE void playMIDIStream(word channel, byte *midi_stream, HEADER_CHNK *header, TRACK_CHNK *track)
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
			WaitSem(MID_timing_pos_Lock)
			if (MID_TERM) //Termination requested?
			{
				//Unlock
				PostSem(MID_timing_pos_Lock)
				return; //Stop playback: we're requested to stop playing!
			}
			if (timing_pos >= play_pos)
			{
				//Unlock
				PostSem(MID_timing_pos_Lock)
				break; //Arrived? Play!
			}
			//Unlock
			PostSem(MID_timing_pos_Lock)
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
					WaitSem(MID_BPM_Lock)
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
					PostSem(MID_BPM_Lock)
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
			WaitSem(MIDLock)

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
				PostSem(MIDLock) //Finish up!
				return; //Abort on error!
			}
			PostSem(MIDLock) //Finish: prepare for next command!
		}
	}
}

void handleMIDIChannel()
{
	word *channel;
	channel = (word *)getthreadparams(); //Gotten a channel?
nextchannel: //Play next channel when type 2!
	playMIDIStream(*channel, MID_data[*channel], &header, &MID_tracks[*channel]); //Play the MIDI stream!
	WaitSem(MID_channel_Lock)
	if (byteswap16(header.format) == 2) //Multiple tracks to be played after one another?
	{
		timing_pos = 0; //Reset the timing position!
		++channel; //Process the next channel!
		if (*channel >= byteswap16(header.n)) goto finish; //Last channel processed?
		PostSem(MID_channel_Lock)
		goto nextchannel; //Process the next channel now!
	}
finish:
	--MID_RUNNING; //Done!
	PostSem(MID_channel_Lock)
}

byte playMIDIFile(char *filename, byte showinfo) //Play a MIDI file, CIRCLE to stop playback!
{
	memset(&MID_data, 0, sizeof(MID_data)); //Init data!
	memset(&MID_tracks, 0, sizeof(MID_tracks)); //Init tracks!

	word numchannels;
	if ((numchannels = readMID(filename, &header, &MID_tracks[0], &MID_data[0], 100)))
	{
		stopTimers(0); //Stop most timers for max compatiblity and speed!
		//Initialise our device!
		PORT_OUT_B(0x331, 0xFF); //Reset!
		PORT_OUT_B(0x331, 0x3F); //Kick to UART mode!

		//Create the semaphore for the threads!
		MIDLock = SDL_CreateSemaphore(1);
		MID_timing_pos_Lock = SDL_CreateSemaphore(1);
		MID_BPM_Lock = SDL_CreateSemaphore(1);
		MID_channel_Lock = SDL_CreateSemaphore(1);

		//Now, start up all timers!
		resetMID(); //Reset all our settings!

		word i;
		MID_RUNNING = numchannels; //Init to all running!
		for (i = 0; i < numchannels; i++)
		{
			if (!i || (byteswap16(header.format) == 1)) //One channel, or multiple channels with format 2!
			{
			MID_tracknr[i] = i; //Track number
	startThread(&handleMIDIChannel, "MIDI_STREAM", (void *)&MID_tracknr[i], DEFAULT_PRIORITY); //Start a thread handling the output of the channel!
			}
		}
		if (byteswap16(header.format) != 1) //One channel only?
		{
			MID_RUNNING = 1; //Only one channel running!
		}

		delay(10000); //Wait a bit to allow for initialisation (position 0) to run!

		startTimers(1);
		startTimers(0); //Start our timers!

		byte running; //Are we running?

		running = 1; //We start running!
		for (;;) //Wait to end!
		{
			delay(50000); //Wait little intervals to update status display!
			WaitSem(MID_channel_Lock)
			if (!MID_RUNNING)
			{
				running = 0; //Not running anymore!
			}
			if (showinfo) printMIDIChannelStatus(); //Print the MIDI channel status!
			PostSem(MID_channel_Lock)

			if (psp_keypressed(BUTTON_CIRCLE) || psp_keypressed(BUTTON_STOP)) //Circle/stop pressed? Request to stop playback!
			{
				for (; (psp_keypressed(BUTTON_CIRCLE) || psp_keypressed(BUTTON_STOP));) delay(0); //Wait for release while pressed!
				WaitSem(MID_timing_pos_Lock) //Wait to update the flag!
				MID_TERM = 1; //Set termination flag to request a termination!
				PostSem(MID_timing_pos_Lock) //We're updated!
			}

			if (!running) break; //Not running anymore? Start quitting!
		}

		//Destroy semaphore
		SDL_DestroySemaphore(MIDLock);
		SDL_DestroySemaphore(MID_timing_pos_Lock);
		SDL_DestroySemaphore(MID_BPM_Lock);
		SDL_DestroySemaphore(MID_channel_Lock);

		removetimer("MID_tempotimer"); //Clean up!
		freeMID(&MID_tracks[0], &MID_data[0], numchannels); //Free all channels!

		//Clean up the MIDI device for any leftover sound!
		byte channel;
		for (channel = 0; channel < 0x10; channel++) //Process all channels!
		{
			PORT_OUT_B(0x330, 0xB0 | (channel & 0xF)); //We're requesting a ...
			PORT_OUT_B(0x330, 0x7B); //All Notes off!
			PORT_OUT_B(0x330, 0x00); //On the current channel!!
		}
		PORT_OUT_B(0x330, 0xFF); //Reset MIDI device!
		PORT_OUT_B(0x331, 0xFF); //Reset the MPU!
		return MID_TERM?0:1; //Played without termination?
	}
	return 0; //Invalid file?
}