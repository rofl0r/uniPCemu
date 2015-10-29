#ifndef SOUND_H
#define SOUND_H

typedef byte (*SOUNDHANDLER)(void* buf, uint_32 length, byte stereo, void *userdata);    /* A pointer to a handler function */

//PI
#define PI 3.1415926535897932f

typedef short sample_t, *sample_p; //One sample!

typedef struct {
        sample_t l, r; //Stereo sample!
} sample_stereo_t, *sample_stereo_p;

#define SMPL16 0
#define SMPL8 1
#define SMPL16S 2
#define SMPL8S 3
#define SMPLFLT 4
//Same as SMPL16, but linear from -32768 to 32767.
#define SMPL16U 5
//Same as SMPL8, but linear from -256 to 255.
#define SMPL8U 6

//Is the buffer filled or skipped (unused)?
#define SOUNDHANDLER_RESULT_NOTFILLED 0
#define SOUNDHANDLER_RESULT_FILLED 1

void initAudio(); //Initialises audio subsystem!
void doneAudio(); //Finishes audio subsystem!
byte addchannel(SOUNDHANDLER handler, void *extradata, char *name, float samplerate, uint_32 samples, byte stereo, byte method); //Adds and gives a 1 on added or 0 on error!
//is_hw: bit 1 set: do not pause, bit 2 set: do not resume playing.
void removechannel(SOUNDHANDLER handler, void *extradata, byte is_hw); //Removes a sound handler from mixing, use is_hw=0 always, except for init/done of sound.c!
void resetchannels(); //Stop all channels&reset!
byte setVolume(SOUNDHANDLER handler, void *extradata, float p_volume); //Channel&Volume(100.0f=100%)
byte setSampleRate(SOUNDHANDLER handler, void *extradata, float rate); //Set sample rate!

//Audio locking!
void lockaudio();
void unlockaudio(byte startplaying);

#define dB2factor(dB, fMaxLevelDB) pow(10, (((dB) - (fMaxLevelDB)) / 20))
#define factor2dB(factor, fMaxLevelDB) ((fMaxLevelDB) + (20 * log(factor)))

#endif