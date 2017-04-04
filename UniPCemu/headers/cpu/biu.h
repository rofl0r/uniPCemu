#ifndef BIU_H
#define BIU_H

#include "headers/types.h" //Basic types!
#include "headers/support/fifobuffer.h" //FIFO buffer support for requests/responses!

typedef struct
{
	FIFOBUFFER *requests; //Request FIFO!
	FIFOBUFFER *responses; //Response FIFO!

	//PIQ support!
	FIFOBUFFER *PIQ; //Our Prefetch Input Queue!
	uint_32 PIQ_EIP; //EIP of the current PIQ data!

	uint_32 currentrequest; //Current request!
	uint_64 currentpayload; //Current payload!
	uint_32 currentresult; //Current result!
	uint_32 currentaddress; //Current address!
	byte prefetchclock; //For clocking the BIU to fetch data to/from memory!
} BIU_type;

void CPU_initBIU(); //Initialize the BIU!
void CPU_doneBIU(); //Finish the BIU!
void CPU_tickBIU(); //Tick the BIU!
void BIU_dosboxTick(); //Tick the BIU, dosbox style!

//Opcode read support for ModR/M!
byte CPU_readOP(byte *result); //Reads the operation (byte) at CS:EIP
byte CPU_readOPw(word *result); //Reads the operation (word) at CS:EIP
byte CPU_readOPdw(uint_32 *result); //Reads the operation (32-bit unsigned integer) at CS:EIP

void CPU_flushPIQ(int_64 destaddr); //Flush the PIQ!

//BIU request/responses!
//Requests for memory accesses!
byte BIU_request_MMUrb(uint_32 addr);
byte BIU_request_MMUrw(uint_32 addr);
byte BIU_request_MMUrdw(uint_32 addr);
byte BIU_request_MMUwb(uint_32 addr, byte value);
byte BIU_request_MMUww(uint_32 addr, word value);
byte BIU_request_MMUwdw(uint_32 addr, uint_32 value);
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

#endif