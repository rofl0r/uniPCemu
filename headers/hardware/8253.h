#ifndef HW82C54_H
#define HW82C54_H

#define PIT_MODE_LATCHCOUNT	0
#define PIT_MODE_LOBYTE	1
#define PIT_MODE_HIBYTE	2
#define PIT_MODE_TOGGLE	3

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
void updatePIT0(); //Timer tick Irq
#endif