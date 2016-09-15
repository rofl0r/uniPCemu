#ifndef HEADER_DOSBOXMPU_H
#define HEADER_DOSBOXMPU_H

#include "headers/header_dosbox.h" //Basic dosbox patches!
#include "headers/mmu/mmuhandler.h" //MMU handler support!
#include "headers/hardware/midi/midi.h" //MIDI OUT/IN device support!
#include "headers/hardware/pic.h" //Own typedefs etc.
#include "headers/hardware/ports.h" //I/O port support!
#include "headers/emu/timers.h" //Timer support!
//Our own typedefs for easier changing of the dosbox code!
#define MIDI_RawOutByte MIDI_OUT
#define MIDI_Available() 1
#define MPU_IRQ 9

//Remove overflow used in math.h
#undef OVERFLOW

//PIC support!
#define PIC_RemoveEvents(function) removetimer("MPU")
#define PIC_AddEvent(function,timeout) addtimer(1/(timeout/1000000),function,"MPU",1,0,NULL)
#define PIC_ActivateIRQ(irq) raiseirq(irq)
#define PIC_DeActivateIRQ(irq) lowerirq(irq);acnowledgeIRQrequest(irq)

#define IO_RegisterWriteHandler(port,handler,name) register_PORTOUT(handler)
#define IO_RegisterReadHandler(port,handler,name) register_PORTIN(handler)
#endif