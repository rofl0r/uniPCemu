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
#define getRPL(segment) (segment&3)
#define setRPL(segment,RPL) segment = ((segment&~3)|(RPL&3))
#define getDescriptorIndex(segmentval) ((segmentval>>3)&0x1FFF)

int CPU_segment_index(byte defaultsegment); //Plain segment to use, direct access!
int get_segment_index(word *location);
void protection_nextOP(); //We're executing the next OPcode?
void segmentWritten(int segment, word value, byte isJMPorCALL); //A segment register has been written to! isJMPorCALL: 1=JMP, 2=CALL.

int CPU_MMU_checklimit(int segment, word segmentval, uint_32 offset, int forreading); //Determines the limit of the segment, forreading=2 when reading an opcode!
byte CPU_MMU_checkrights(int segment, word segmentval, uint_32 offset, int forreading, SEGMENT_DESCRIPTOR *descriptor, byte addrtest); //Check rights for VERR/VERW!

byte CPU_faultraised(); //A fault has been raised (286+)?

//Special support for error handling!
void THROWDESCGP(word segment);
void THROWDESCSP(word segment, byte external);
void THROWDESCSeg(word segment, byte external);

//Internal usage by the protection modules!
int LOADDESCRIPTOR(int whatsegment, word segment, SEGDESCRIPTOR_TYPE *container);
void SAVEDESCRIPTOR(int whatsegment, word segment, SEGDESCRIPTOR_TYPE *container); //Save a loaded descriptor back to memory!

byte checkPortRights(word port); //Are we allowed to not use this port?
byte disallowPOPFI(); //Allow POPF to not change the interrupt flag?
byte checkSTICLI(); //Check STI/CLI rights! 1 when allowed, 0 when to be ignored!

void CPU_ProtectedModeInterrupt(byte intnr, byte is_HW, word returnsegment, uint_32 returnoffset, uint_32 error); //Execute a protected mode interrupt!

#endif