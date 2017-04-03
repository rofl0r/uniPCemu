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

#endif