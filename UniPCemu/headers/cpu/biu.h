#ifndef BIU_H
#define BIU_H

#include "headers/types.h" //Basic types!
#include "headers/support/fifobuffer.h" //FIFO buffer support for requests/responses!

typedef struct
{
	byte cycles; //Cycles left pending! 0=Ready to process next step!
	byte prefetchcycles; //Prefetch cycles done
	byte cycles_stallBIU; //How many cycles to stall the BIU when running the BIU?
	byte curcycle; //Current cycle to process?
	byte cycles_stallBUS; //How many cycles to stall the BUS, BIU and EU!
	Handler currentTimingHandler; //What step are we currently executing?
} CPU_CycleTimingInfo;

typedef struct
{
	byte ready; //Ready to use(initialized)?
	FIFOBUFFER *requests; //Request FIFO!
	FIFOBUFFER *responses; //Response FIFO!

	//PIQ support!
	FIFOBUFFER *PIQ; //Our Prefetch Input Queue!
	uint_32 PIQ_Address; //EIP of the current PIQ data!

	uint_32 currentrequest; //Current request!
	uint_64 currentpayload[2]; //Current payload!
	uint_32 currentresult; //Current result!
	uint_32 currentaddress; //Current address!
	byte prefetchclock; //For clocking the BIU to fetch data to/from memory!
	byte waitstateRAMremaining; //Amount of RAM waitstate cycles remaining!
	CPU_CycleTimingInfo cycleinfo; //Current cycle state!
	byte requestready; //Request not ready to retrieve?
	byte TState; //What T-state is the BIU running at?
	byte stallingBUS; //Are we stalling the BUS!
} BIU_type;

void CPU_initBIU(); //Initialize the BIU!
void CPU_doneBIU(); //Finish the BIU!
void CPU_tickBIU(); //Tick the BIU!
void BIU_dosboxTick(); //Tick the BIU, dosbox style!

byte BIU_Ready(); //Are we ready to continue execution?

//Opcode read support for ModR/M!
byte CPU_readOP(byte *result, byte singlefetch); //Reads the operation (byte) at CS:EIP
byte CPU_readOPw(word *result, byte singlefetch); //Reads the operation (word) at CS:EIP
byte CPU_readOPdw(uint_32 *result, byte singlefetch); //Reads the operation (32-bit unsigned integer) at CS:EIP

void CPU_flushPIQ(int_64 destaddr); //Flush the PIQ!

//BIU request/responses!
//Requests for memory accesses, physical memory only!
byte BIU_request_Memoryrb(uint_32 offset);
byte BIU_request_Memoryrw(uint_32 offset);
byte BIU_request_Memoryrdw(uint_32 offset);
byte BIU_request_Memorywb(uint_32 offset, byte val);
byte BIU_request_Memoryww(uint_32 offset, word val);
byte BIU_request_Memorywdw(uint_32 offset, uint_32 val);
//Requests for BUS(I/O address space) accesses!
byte BIU_request_BUSrb(uint_32 addr);
byte BIU_request_BUSrw(uint_32 addr);
byte BIU_request_BUSrdw(uint_32 addr);
byte BIU_request_BUSwb(uint_32 addr, byte value);
byte BIU_request_BUSww(uint_32 addr, word value);
byte BIU_request_BUSwdw(uint_32 addr, uint_32 value);
//Result reading support for all accesses!
byte BIU_readResultb(byte *result); //Read the result data of a BUS request!
byte BIU_readResultw(word *result); //Read the result data of a BUS request!
byte BIU_readResultdw(uint_32 *result); //Read the result data of a BUS request!

//Easy read/write support for non-Paged access.
word BIU_directrw(uint_32 realaddress, byte index); //Direct read from real memory (with real data direct)!
void BIU_directww(uint_32 realaddress, word value, byte index); //Direct write to real memory (with real data direct)!
//Used by paging only!
uint_32 BIU_directrdw(uint_32 realaddress, byte index);
void BIU_directwdw(uint_32 realaddress, uint_32 value, byte index);

byte memory_BIUdirectrb(uint_32 realaddress); //Direct read from real memory (with real data direct)!
word memory_BIUdirectrw(uint_32 realaddress); //Direct read from real memory (with real data direct)!
uint_32 memory_BIUdirectrdw(uint_32 realaddress); //Direct read from real memory (with real data direct)!
void memory_BIUdirectwb(uint_32 realaddress, byte value); //Direct write to real memory (with real data direct)!
void memory_BIUdirectww(uint_32 realaddress, word value); //Direct write to real memory (with real data direct)!
void memory_BIUdirectwdw(uint_32 realaddress, uint_32 value); //Direct write to real memory (with real data direct)!

//MMU support for the above functionality!
byte BIU_directrb(uint_32 realaddress, byte index);
word BIU_directrw(uint_32 realaddress, byte index); //Direct read from real memory (with real data direct)!
uint_32 BIU_directrdw(uint_32 realaddress, byte index);
void BIU_directwb(uint_32 realaddress, byte val, byte index); //Access physical memory dir
void BIU_directww(uint_32 realaddress, word value, byte index); //Direct write to real memory (with real data direct)!
void BIU_directwdw(uint_32 realaddress, uint_32 value, byte index);

#endif