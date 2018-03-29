#ifndef HW82C54_H
#define HW82C54_H

typedef void (*PITTick)(byte output); //A handler for PIT ticks!

typedef struct {
	uint16_t chandata[3];
	uint8_t accessmode[3];
	uint8_t bytetoggle[3];
	uint32_t effectivedata[3];
	float chanfreq[3];
	uint8_t active[3];
	uint16_t counter[3];
} i8253_s;

void init8253(); //Initialisation!
void cleanPIT(); //Timer tick Irq reset timing

//PC speaker!
void setPITFrequency(byte channel, word newfrequency); //Set the new frequency!
void initSpeakers(byte soundspeaker); //Initialises the speaker and sets it up!
void doneSpeakers(); //Finishes the speaker and removes it!
void tickPIT(DOUBLE timepassed, uint_32 MHZ14passed); //Ticks all PIT timers/speakers available!
void setPITMode(byte channel, byte mode); //Set the current rendering mode!
void speakerGateUpdated(); //Gate has been updated?
void registerPIT1Ticker(PITTick ticker); //Register a PIT1 ticker for usage?

#endif
