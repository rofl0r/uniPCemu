#ifndef MID_H
#define MID_H

#include "headers/types.h" //Basic types!

//MIDI file support!

#include "headers/packed.h"
typedef PACKED struct
{
	uint_32 Header; //MThd
	uint_32 header_length;
	word format; //0=Single track, 1=Multiple track, 2=Multiple song file format (multiple type 0 files)
	word n; //Number of tracks that follow us
	sword division; //Positive: units per beat, negative:  SMPTE-compatible units.
} HEADER_CHNK;
#include "headers/endpacked.h"

#include "headers/packed.h"
typedef PACKED struct
{
	uint_32 Header; //MTrk
	uint_32 length; //Number of bytes in the chunk.
} TRACK_CHNK;
#include "headers/endpacked.h"

word readMID(char *filename, HEADER_CHNK *header, TRACK_CHNK *tracks, byte **channels, word maxchannels);
void freeMID(TRACK_CHNK *tracks, byte **channels, word numchannels);
void playMIDIStream(word channel, byte *midi_stream, HEADER_CHNK *header, TRACK_CHNK *track);

#endif