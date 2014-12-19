#ifndef MMU_H
#define MMU_H

#include "headers/types.h"
#include "headers/cpu/cpu.h" //Pointer support etc.

typedef struct
{
	uint_32 size; //The total size of memory allocated!
	byte *memory; //The memory itself!
	int invaddr; //Invalid adress in memory with MMU_ptr?
	int wraparround; //To wrap arround memory?
} MMU_type;

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

#ifndef IS_MMU
extern MMU_type MMU; //Extern call!
#endif


//Continuing internal stuff

#define MEM_FACTOR 16
//Factor is 16 at 8086, 16/4K at 386.

void *MMU_ptr(int segdesc, word segment, uint_32 offset, int forreading, uint_32 size); //Gives direct memory pointer!
void resetMMU(); //Initialises memory!
void doneMMU(); //Releases memory for closing emulator etc.
uint_32 MEMsize(); //Total size of memory in use?
byte MMU_rb(int segdesc, word segment, uint_32 offset, int opcode); //Get adress!
word MMU_rw(int segdesc, word segment, uint_32 offset, int opcode); //Get adress (word)!
uint_32 MMU_rdw(int segdesc, word segment, uint_32 offset, int opcode); //Get adress (dword)!
void MMU_wb(int segdesc, word segment, uint_32 offset, byte val); //Get adress!
void MMU_ww(int segdesc, word segment, uint_32 offset, word val); //Get adress (word)!
void MMU_wdw(int segdesc, word segment, uint_32 offset, uint_32 val); //Get adress (dword)!
int hasmemory(); //Have memory?
int MMU_invaddr(); //Last MMU call has invalid adressing?

byte MMU_directrb(uint_32 realadress); //Direct read from memory (with real data direct)!
void MMU_directwb(uint_32 realadress, byte value); //Direct write to memory (with real data direct)!

//For DMA controller/paging/system: direct word access!
word MMU_directrw(uint_32 realadress); //Direct read from real memory (with real data direct)!
void MMU_directww(uint_32 realadress, word value); //Direct write to real memory (with real data direct)!

//For paging/system only!
uint_32 MMU_directrdw(uint_32 realaddress);
void MMU_directwdw(uint_32 realaddress, uint_32 value);


void MMU_dumpmemory(char *filename); //Dump the memory to a file!
//uint_32 MMU_realaddr(int segdesc, word segment, uint_32 offset); //Real adress in real (direct) memory?
void MMU_wraparround(int dowrap); //To wrap arround 1/3/5/... MB limit?
#endif