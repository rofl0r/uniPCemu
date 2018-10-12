#ifndef PROTECTION_H
#define PROTECTION_H

#include "headers/cpu/cpu.h" //Basic CPU support!

#define getCPL() CPU[activeCPU].CPL
#define getRPL(segment) ((segment)&3)
#define setRPL(segment,RPL) segment = ((segment&~3)|(RPL&3))
#define getDescriptorIndex(segmentval) (((segmentval)>>3)&0x1FFF)

int CPU_segment_index(byte defaultsegment); //Plain segment to use, direct access!
int get_segment_index(word *location);
void protection_nextOP(); //We're executing the next OPcode?
byte segmentWritten(int segment, word value, word isJMPorCALL); //A segment register has been written to! isJMPorCALL: 1=JMP, 2=CALL.

int CPU_MMU_checklimit(int segment, word segmentval, uint_64 offset, byte forreading, byte is_offset16); //Determines the limit of the segment, forreading=2 when reading an opcode!
byte CPU_MMU_checkrights(int segment, word segmentval, uint_64 offset, byte forreading, SEGMENT_DESCRIPTOR *descriptor, byte addrtest, byte is_offset16); //Check rights for VERR/VERW!

byte CPU_faultraised(byte type); //A fault has been raised (286+)?

//Special support for error handling!
void THROWDESCGP(word segmentval, byte external, byte tbl);
void THROWDESCSP(word segmentval, byte external, byte tbl);
void THROWDESCNP(word segmentval, byte external, byte tbl);
void THROWDESCTS(word segmentval, byte external, byte tbl);

//Internal usage by the protection modules! Result: 1=OK, 0=Error out by caller, -1=Already errored out, abort error handling(caused by Paging Unit faulting)!
sbyte LOADDESCRIPTOR(int segment, word segmentval, SEGMENT_DESCRIPTOR *container, word isJMPorCALL); //Bits 8-15 of isJMPorCALL are reserved, only 8-bit can be supplied by others than SAVEDESCRIPTOR!
sbyte SAVEDESCRIPTOR(int segment, word segmentval, SEGMENT_DESCRIPTOR *container, word isJMPorCALL); //Save a loaded descriptor back to memory!

byte checkPortRights(word port); //Are we allowed to not use this port?
byte disallowPOPFI(); //Allow POPF to not change the interrupt flag?
byte checkSTICLI(); //Check STI/CLI rights! 1 when allowed, 0 when to be ignored!

byte CPU_ProtectedModeInterrupt(byte intnr, word returnsegment, uint_32 returnoffset, int_64 errorcode, byte is_interrupt); //Execute a protected mode interrupt!

byte STACK_SEGMENT_DESCRIPTOR_B_BIT(); //80286+: Gives the B-Bit of the DATA DESCRIPTOR TABLE FOR SS-register!
byte CODE_SEGMENT_DESCRIPTOR_D_BIT(); //80286+: Gives the B-Bit of the CODE DESCRIPTOR TABLE FOR CS-register!
uint_32 getstackaddrsizelimiter(); //80286+: Gives the stack address size mask to use!
void CPU_saveFaultData(); //Prepare for a fault by saving all required data!
void CPU_onResettingFault(); //When resetting the current instruction for a fault!

void CPU_AC(int_64 errorcode); //Alignment check fault!

byte switchStacks(byte newCPL); //Returns 1 on error, 0 on success!
void updateCPL(); //Update the CPL to be the currently loaded CPL depending on the mode and descriptors!
void CPU_calcSegmentPrecalcs(SEGMENT_DESCRIPTOR *descriptor);
int getLoadedTYPE(SEGMENT_DESCRIPTOR *loadeddescriptor);
#endif
