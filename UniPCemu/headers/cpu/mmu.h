#ifndef MMU_H
#define MMU_H

#include "headers/types.h"
#include "headers/cpu/cpu.h" //Pointer support etc.

typedef struct
{
	char unused;
	char sectors_per_track;                         /* AT and later */
	short landing_zone;                             /* AT and later */
	char drive_check_timeout;                       /* XT only */
	char formatting_timeout;                        /* XT only */
	char normal_timeout;                            /* XT only */
	char control;                                   /* AT and later */
	/* bit 7: 1 - do not retry on access error
	   bit 6: 1 - do not retry on ECC error
	   bit 5: 1 - there is a bad block map past the last cylinder
	   bit 4: 0
	   bit 3: 1 - more than 8 heads on the drive
	   bit 2: 0 - no reset
	   bit 1: 0 - disable IRQ
	   bit 0: 0 */
	char max_ECC_data_burst_length;                 /* XT only */
	short pre_compensation_starting_cyl;
	short reduced_write_current_starting_cyl;       /* XT only */
	char heads;             /* XT: 1-8, AT: 1-16, ESDI: 1-32 */
	short cylinders;
} HPPT; //Hard Disk Parameter Table (#0=int41 at 0xF000:0xE401 and #1=int46 at 0x0xF001:0xE401)

//Continuing internal stuff

#define MEM_FACTOR 16
//Factor is 16 at 8086, 16/4K at 386.

void *MMU_ptr(sword segdesc, word segment, uint_32 offset, byte forreading, uint_32 size); //Gives direct memory pointer!
byte MMU_rb(sword segdesc, word segment, uint_32 offset, byte opcode, byte is_offset16); //Get adress!
word MMU_rw(sword segdesc, word segment, uint_32 offset, byte opcode, byte is_offset16); //Get adress (word)!
uint_32 MMU_rdw(sword segdesc, word segment, uint_32 offset, byte opcode, byte is_offset16); //Get adress (dword)!
void MMU_wb(sword segdesc, word segment, uint_32 offset, byte val, byte is_offset16); //Get adress!
void MMU_ww(sword segdesc, word segment, uint_32 offset, word val, byte is_offset16); //Get adress (word)!
void MMU_wdw(sword segdesc, word segment, uint_32 offset, uint_32 val, byte is_offset16); //Get adress (dword)!

//uint_32 MMU_realaddr(int segdesc, word segment, uint_32 offset); //Real adress in real (direct) memory?
void MMU_setA20(byte where, byte enabled); //Set A20 line enabled?
void MMU_clearOP(); //Clear the OPcode cache!
void MMU_addOP(byte data); //Add an opcode to the OPcode cache!

byte checkMMUaccess(sword segdesc, word segment, uint_32 offset, byte readflags, byte CPL, byte is_offset16, byte subbyte); //Check if a byte address is invalid to read/write for a purpose! Used in all CPU modes!

//Direct memory support for the CPU!
byte checkDirectMMUaccess(uint_32 realaddress, byte readflags, byte CPL); //Check direct memory access before applying the writes below!
void Paging_directwb(sword segdesc, uint_32 realaddress, byte val, byte index, byte is_offset16, byte writewordbackup);
byte Paging_directrb(sword segdesc, uint_32 realaddress, byte writewordbackup, byte opcode, byte index);

#endif