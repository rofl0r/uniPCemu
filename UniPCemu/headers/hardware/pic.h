#ifndef PIC_H
#define PIC_H

#include "headers/types.h" //Basic type support!

/*

List of IRQs:
0: System Timer
1: Keyboard controller
2: <Must not be used: Used to cascade signals from IRQ 8-15>
3: Serial Port Controller for COM2 (Shared with COM4)
4: Serial Port Controller for COM1 (Shared with COM3)
5: LPT Port 2 or sound card
6: Floppy Disk Controller
7: LPT Port 1 or Printers or for any parallel port if a printer isn't present.

Slave PIC:
8: RTC Timer
9: Left open (Or SCSI Host Adapter)
10: Left open (Or SCSI Or NIC)
11: See 10
12: Mouse on PS/2 connector
13: Math Co-Procesor or integrated FPU or inter-processor interrupt (use depends on OS)
14: Primary ATA channel
15: Secondary ATA channel

Info: ATA interface usually serves hard disks and CD drives.

*/

typedef void(*IRQHandler)(byte IRQ);

typedef struct
{
	uint8_t imr[2]; //mask register
	uint8_t irr[2]; //request register to be read by the emulated CPU!
	uint8_t irr2[2][0x10]; //Extended IRR for determining requesting hardware! This is the actual status of an IR line(high and low)!
	uint8_t irr3[2][0x10]; //Extended IRR for determining requesting hardware! This one is actually used to store the status from hardware until it's handled!
	uint8_t isr[2]; //service register
	uint8_t isr2[2][0x10]; //Alternative in-service register, for handling sources!
	uint8_t icwstep[2]; //used during initialization to keep track of which ICW we're at
	uint8_t icw[2][4]; //4 ICW bytes are used!
	uint8_t intoffset[2]; //interrupt vector offset
	uint8_t priority[2]; //which IRQ has highest priority
	uint8_t autoeoi[2]; //automatic EOI mode
	uint8_t readmode[2]; //remember what to return on read register from OCW3
	uint8_t enabled[2];
	byte IROrder[16]; //The order we process!
	IRQHandler acceptirq[0x10][0x10], finishirq[0x10][0x10]; //All IRQ handlers!
} PIC;

void init8259(); //For initialising the 8259 module!
byte in8259(word portnum, byte *result); //In port
byte out8259(word portnum, byte value); //Out port
byte PICInterrupt(); //We have an interrupt ready to process?
byte nextintr(); //Next interrupt to handle

void registerIRQ(byte IRQ, IRQHandler acceptIRQ, IRQHandler finishIRQ); //Register IRQ handler!

void raiseirq(byte irqnum); //Raise IRQ from hardware request!
void lowerirq(byte irqnum); //Lower IRQ from hardware request!

void acnowledgeIRQrequest(byte irqnum); //Acnowledge an IRQ request!
#endif