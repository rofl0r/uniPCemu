#ifndef PROTECTION_H
#define PROTECTION_H

#include "headers/cpu/cpu.h" //Basic CPU support!

typedef struct
{
	union
	{
		uint_64 DATA64; //Full data for simple set!
		SEGMENT_DESCRIPTOR desc; //The descriptor to be loaded!
		byte descdata[8]; //The data bytes of the descriptor!
	};
} SEGDESCRIPTOR_TYPE;

#define getCPL() CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].DPL

int CPU_segment_index(byte defaultsegment); //Plain segment to use, direct access!
int get_segment_index(word *location);
void protection_nextOP(); //We're executing the next OPcode?
void segmentWritten(int segment, word value, byte isJMPorCALL); //A segment register has been written to! isJMPorCALL: 1=JMP, 2=CALL.

int CPU_MMU_checklimit(int segment, word segmentval, uint_32 offset, int forreading); //Determines the limit of the segment, forreading=2 when reading an opcode!

#endif