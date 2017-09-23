#include "headers/cpu/biu.h" //Our own typedefs!
#include "headers/cpu/cpu.h" //CPU!
#include "headers/support/fifobuffer.h" //FIFO support!
#include "headers/cpu/protection.h" //Protection support!
#include "headers/cpu/mmu.h" //MMU support!
#include "headers/hardware/ports.h" //Hardware port support!
#include "headers/support/signedness.h" //Unsigned and signed support!
#include "headers/cpu/paging.h" //Paging support for paging access!
#include "headers/mmu/mmuhandler.h" //MMU direct access support!
#include "headers/emu/debugger/debugger.h" //Debugger support!
#include "headers/mmu/mmu_internals.h" //Internal MMU call support!
#include "headers/mmu/mmuhandler.h" //MMU handling support!

//16-bits compatibility for reading parameters!
#ifndef IS_PSP
#define LE_16BITS(x) SDL_SwapLE16(x)
#else
#define LE_16BITS(x) (x)
#endif
//32-bits compatibility for reading parameters!
#define LE_32BITS(x) SDL_SwapLE32((LE_16BITS((x)&0xFFFF))|(uint_32)((LE_16BITS(((x)>>16)&0xFFFF))<<16))

//Types of request(low 4 bits)!
#define REQUEST_NONE 0

//Type
#define REQUEST_TYPEMASK 7
#define REQUEST_MMUREAD 1
#define REQUEST_MMUWRITE 2
#define REQUEST_IOREAD 3
#define REQUEST_IOWRITE 4

//Size to access
#define REQUEST_SIZEMASK 0xC
#define REQUEST_16BIT 0x08
#define REQUEST_32BIT 0x10

//Extra extension for 16/32-bit accesses(bitflag) to identify high value to be accessed!
#define REQUEST_SUBMASK 0x60
#define REQUEST_SUBSHIFT 5
#define REQUEST_SUB0 0x00
#define REQUEST_SUB1 0x20
#define REQUEST_SUB2 0x40
#define REQUEST_SUB3 0x60

#define CPU286_WAITSTATE_DELAY 1

BIU_type BIU[MAXCPUS]; //All possible BIUs!

extern byte PIQSizes[2][NUMCPUS]; //The PIQ buffer sizes!

byte CPU_databussize = 0; //0=16/32-bit bus! 1=8-bit bus when possible (8088/80188) or 16-bit when possible(286+)!

extern byte cpudebugger; //To debug the CPU?

void CPU_initBIU()
{
	if (BIU[activeCPU].ready) //Are we ready?
	{
		CPU_doneBIU(); //Finish us first!
	}
	
	if (PIQSizes[CPU_databussize][EMULATED_CPU]) //Gotten any PIQ installed with the CPU?
	{
		BIU[activeCPU].PIQ = allocfifobuffer(PIQSizes[CPU_databussize][EMULATED_CPU],0); //Our PIQ we use!
	}
	BIU[activeCPU].requests = allocfifobuffer(20,0); //Our request buffer to use(1 64-bit entry being 2 32-bit entries, for 2 64-bit entries(payload) and 1 32-bit entry(the request identifier))!
	BIU[activeCPU].responses = allocfifobuffer(sizeof(uint_32)<<1,0); //Our response buffer to use(1 64-bit entry as 2 32-bit entries)!
	BIU[activeCPU].ready = 1; //We're ready to be used!
	CPU_flushPIQ(-1); //Init us to start!
}

void CPU_doneBIU()
{
	free_fifobuffer(&BIU[activeCPU].PIQ); //Release our PIQ!
	free_fifobuffer(&BIU[activeCPU].requests); //Our request buffer to use(1 64-bit entry as 2 32-bit entries)!
	free_fifobuffer(&BIU[activeCPU].responses); //Our response buffer to use(1 64-bit entry as 2 32-bit entries)!
	BIU[activeCPU].ready = 0; //We're not ready anymore!
	memset(&BIU[activeCPU],0,sizeof(BIU)); //Full init!
}

void CPU_flushPIQ(int_64 destaddr)
{
	if (BIU[activeCPU].PIQ) fifobuffer_clear(BIU[activeCPU].PIQ); //Clear the Prefetch Input Queue!
	BIU[activeCPU].PIQ_Address = (destaddr!=-1)?(uint_32)destaddr:CPU[activeCPU].registers->EIP; //Use actual IP!
	//TODO: Paging for the fetching process!
	CPU[activeCPU].repeating = 0; //We're not repeating anymore!
}

//Internal helper functions for requests and responses!
OPTINLINE byte BIU_haveRequest() //BIU: Does the BIU have a request?
{
	uint_32 request1;
	return (peekfifobuffer32(BIU[activeCPU].requests,&request1) && (fifobuffer_freesize(BIU[activeCPU].responses)>=8)); //Do we have a request and enough size for a response?
}

OPTINLINE byte BIU_readRequest(uint_32 *requesttype, uint_64 *payload1, uint_64 *payload2) //BIU: Read a request to process!
{
	uint_32 temppayload1, temppayload2;
	if (BIU[activeCPU].requestready==0) return 0; //Not ready!
	if (readfifobuffer32(BIU[activeCPU].requests,requesttype)==0) //Type?
	{
		return 0; //No request!
	}
	if (readfifobuffer32_2u(BIU[activeCPU].requests,&temppayload1,&temppayload2)) //Read the payload?
	{
		*payload1 = (((uint_64)temppayload2<<32)|(uint_64)temppayload1); //Give the request!
		if (readfifobuffer32_2u(BIU[activeCPU].requests,&temppayload1,&temppayload2)) //Read the payload?
		{
			*payload2 = (((uint_64)temppayload2<<32)|(uint_64)temppayload1); //Give the request!
			return 1; //OK! We're having the request!
		}
	}
	return 0; //Invalid request!
}

OPTINLINE byte BIU_request(uint_32 requesttype, uint_64 payload1, uint_64 payload2) //CPU: Request something from the BIU by the CPU!
{
	byte result;
	uint_32 request1, request2;
	if ((BIU[activeCPU].requestready==0) || (fifobuffer_freesize(BIU[activeCPU].responses)==0)) return 0; //Not ready! Don't allow requests while responses are waiting to be handled!
	request1 = (payload1&0xFFFFFFFF); //Low!
	request2 = (payload1>>32); //High!
	if (fifobuffer_freesize(BIU[activeCPU].requests)>=20) //Enough to accept?
	{
		result = writefifobuffer32(BIU[activeCPU].requests,requesttype); //Request type!
		result &= writefifobuffer32_2u(BIU[activeCPU].requests,request1,request2); //Payload!
		request1 = (payload2&0xFFFFFFFF); //Low!
		request2 = (payload2>>32); //High!
		result &= writefifobuffer32_2u(BIU[activeCPU].requests,request1,request2); //Payload!
		return result; //Are we requested?
	}
	return 0; //Not available!
}

OPTINLINE byte BIU_response(uint_64 response) //BIU: Response given from the BIU!
{
	uint_32 response1, response2;
	response1 = (response&0xFFFFFFFF); //Low!
	response2 = (response>>32); //High!
	return (writefifobuffer32_2u(BIU[activeCPU].responses,response1,response2)); //Response!
}

OPTINLINE byte BIU_readResponse(uint_64 *response) //CPU: Read a response from the BIU!
{
	uint_32 response1, response2;
	if (BIU[activeCPU].requestready==0) return 0; //Not ready!
	if (readfifobuffer32_2u(BIU[activeCPU].responses,&response1,&response2)) //Do we have a request and enough size for a response?
	{
		*response = (((uint_64)response2<<32)|(uint_64)response1); //Give the request!
		return 1; //OK!
	}
	return 0; //No request!
}

//Actual requesting something from the BIU, for the CPU module to call!
//MMU accesses

byte BIU_request_Memoryrb(uint_32 address)
{
	return BIU_request(REQUEST_MMUREAD,address,0); //Request a read!
}

byte BIU_request_Memoryrw(uint_32 address)
{
	return BIU_request(REQUEST_MMUREAD|REQUEST_16BIT,address,0); //Request a read!
}

byte BIU_request_Memoryrdw(uint_32 address)
{
	return BIU_request(REQUEST_MMUREAD|REQUEST_32BIT,address,0); //Request a read!
}

byte BIU_request_Memorywb(uint_32 address, byte val)
{
	return BIU_request(REQUEST_MMUWRITE,((uint_64)address|((uint_64)val<<32)),0); //Request a write!
}

byte BIU_request_Memoryww(uint_32 address, word val)
{
	return BIU_request(REQUEST_MMUWRITE|REQUEST_16BIT,((uint_64)address|((uint_64)val<<32)),0); //Request a write!
}

byte BIU_request_Memorywdw(uint_32 address, uint_32 val)
{
	return BIU_request(REQUEST_MMUWRITE|REQUEST_32BIT,((uint_64)address|((uint_64)val<<32)),0); //Request a write!
}

//BUS(I/O address space) accesses for the Execution Unit to make, and their results!
byte BIU_request_BUSrb(uint_32 addr)
{
	return BIU_request(REQUEST_IOREAD,addr,0); //Request a read!
}

byte BIU_request_BUSrw(uint_32 addr)
{
	return BIU_request(REQUEST_IOREAD|REQUEST_16BIT,addr,0); //Request a read!
}

byte BIU_request_BUSrdw(uint_32 addr)
{
	return BIU_request(REQUEST_IOREAD|REQUEST_32BIT,addr,0); //Request a read!
}

byte BIU_request_BUSwb(uint_32 addr, byte value)
{
	return BIU_request(REQUEST_IOWRITE,(uint_64)addr|((uint_64)value<<32),0); //Request a read!
}

byte BIU_request_BUSww(uint_32 addr, word value)
{
	return BIU_request(REQUEST_IOWRITE|REQUEST_16BIT,((uint_64)addr|((uint_64)value<<32)),0); //Request a write!
}

byte BIU_request_BUSwdw(uint_32 addr, uint_32 value)
{
	return BIU_request(REQUEST_IOWRITE|REQUEST_32BIT,((uint_64)addr|((uint_64)value<<32)),0); //Request a write!
}

byte BIU_readResultb(byte *result) //Read the result data of a BUS request!
{
	byte status;
	uint_64 response;
	status = BIU_readResponse(&response); //Read the response for the user!
	if (status) //Read?
	{
		*result = (byte)response; //Give the response!
		return 1; //Read!
	}
	return 0; //Not read!
}

byte BIU_readResultw(word *result) //Read the result data of a BUS request!
{
	byte status;
	uint_64 response;
	status = BIU_readResponse(&response); //Read the response for the user!
	if (status) //Read?
	{
		*result = (word)response; //Give the response!
		return 1; //Read!
	}
	return 0; //Not read!
}

byte BIU_readResultdw(uint_32 *result) //Read the result data of a BUS request!
{
	byte status;
	uint_64 response;
	status = BIU_readResponse(&response); //Read the response for the user!
	if (status) //Read?
	{
		*result = (uint_32)response; //Give the response!
		return 1; //Read!
	}
	return 0; //Not read!
}

byte BIU_access_writeshift[4] = {32,40,48,56}; //Shift to get the result byte to write to memory!
byte BIU_access_readshift[4] = {0,8,16,24}; //Shift to put the result byte in the result!

OPTINLINE byte BIU_isfulltransfer()
{
	INLINEREGISTER byte result;
	result = 0; //Default: byte transfer!
	if ((BIU[activeCPU].currentrequest&REQUEST_16BIT) && ((BIU[activeCPU].currentaddress&1)==0)) //Aligned 16-bit access?
	{
		if ((EMULATED_CPU>=CPU_80386) || ((EMULATED_CPU<=CPU_80286) && (CPU_databussize==0))) //16-bit+ bus available?
		{
			result = 1; //Start a full transfer this very clock!
		}
	}
	else if ((BIU[activeCPU].currentrequest&REQUEST_32BIT) && ((BIU[activeCPU].currentaddress&3)==0)) //Aligned 32-bit access?
	{
		if ((EMULATED_CPU>=CPU_80386) && (CPU_databussize==0)) //32-bit processor with 32-bit bus?
		{
			result = 1; //Start a full transfer this very clock!
		}
		else if (EMULATED_CPU>=CPU_80386) //32-bit processor with 16-bit data bus?
		{
			result = 2; //Start a full transfer, broken in half(two 16-bit accesses)!
		}
	}
	else if ((BIU[activeCPU].currentrequest&REQUEST_32BIT) && ((BIU[activeCPU].currentaddress&1)==0)) //Word-Aligned 32-bit access, but not 32-bit aligned? Break up into word accesses, when possible!
	{
		if (EMULATED_CPU>=CPU_80386) //32-bit processor with 16-bit data bus at least?
		{
			result = 2; //Start a full transfer, broken in half(two 16-bit accesses)!
		}
	}
	return result; //Give the result!
}

//Linear memory access for the CPU through the Memory Unit!
extern byte MMU_logging; //Are we logging?
extern MMU_type MMU; //MMU support!
extern byte is_Compaq; //Are we emulating a Compaq architecture?
uint_32 wrapaddr[2] = {0xFFFFFFFF,0xFFFFFFFF}; //What wrap to apply!
extern uint_32 effectivecpuaddresspins; //What address pins are supported?
byte BIU_directrb(uint_32 realaddress, byte index)
{
	uint_32 originaladdr;
	byte result;
	//Apply A20!
	wrapaddr[1] = MMU.wraparround; //What wrap to apply when enabled!
	realaddress &= effectivecpuaddresspins; //Only 20-bits address is available on a XT without newer CPU! Only 24-bits is available on a AT!
	originaladdr = realaddress; //Save the address before the A20 is modified!
	realaddress &= wrapaddr[(((MMU.A20LineEnabled==0) && (((realaddress&~0xFFFFF)==0x100000)||(is_Compaq!=1)))&1)]; //Apply A20, when to be applied!

	//Normal memory access!
	result = MMU_INTERNAL_directrb_realaddr(realaddress,index); //Read from MMU/hardware!

	if (unlikely(MMU_logging==1)) //To log?
	{
		debugger_logmemoryaccess(0,originaladdr,result,LOGMEMORYACCESS_PAGED); //Log it!
	}

	return result; //Give the result!
}

void BIU_directwb(uint_32 realaddress, byte val, byte index) //Access physical memory dir
{
	//Apply A20!
	wrapaddr[1] = MMU.wraparround; //What wrap to apply when enabled!
	realaddress &= effectivecpuaddresspins; //Only 20-bits address is available on a XT without newer CPU! Only 24-bits is available on a AT!

	if (unlikely(MMU_logging==1)) //To log?
	{
		debugger_logmemoryaccess(1,realaddress,val,LOGMEMORYACCESS_PAGED); //Log it!
	}

	realaddress &= wrapaddr[(((MMU.A20LineEnabled==0) && (((realaddress&~0xFFFFF)==0x100000)||(is_Compaq!=1)))&1)]; //Apply A20, when to be applied!

	//Normal memory access!
	MMU_INTERNAL_directwb_realaddr(realaddress,val,index); //Set data!
}

extern uint_32 checkMMUaccess_linearaddr; //Saved linear address for the BIU to use!
byte PIQ_block = 0; //Blocking any PIQ access now?
OPTINLINE void CPU_fillPIQ() //Fill the PIQ until it's full!
{
	uint_32 realaddress;
	if (PIQ_block==1) { PIQ_block = 0; return; /* Blocked access: only fetch one byte instead of a full word! */ }
	if (unlikely(BIU[activeCPU].PIQ==0)) return; //Not gotten a PIQ? Abort!
	realaddress = BIU[activeCPU].PIQ_Address; //Next address to fetch!
	checkMMUaccess_linearaddr = (CPU[activeCPU].SEG_base[CPU_SEGMENT_CS]+realaddress); //Default 8086-compatible address to use, otherwise, it's overwritten by checkMMUaccess with the proper linear address!
	if (unlikely(checkMMUaccess(CPU_SEGMENT_CS,CPU[activeCPU].registers->CS,realaddress,0x10|3,getCPL(),0,0))) return; //Abort on fault!
	if (unlikely(is_paging())) //Are we paging?
	{
		checkMMUaccess_linearaddr = mappage(checkMMUaccess_linearaddr,0,getCPL()); //Map it using the paging mechanism!		
	}
	writefifobuffer(BIU[activeCPU].PIQ, BIU_directrb(checkMMUaccess_linearaddr,0)); //Add the next byte from memory into the buffer!
	if (unlikely(checkMMUaccess_linearaddr&1)) //Read an odd address?
	{
		PIQ_block &= 1; //Start blocking when it's 3(byte fetch instead of word fetch). Otherwise, continue as normally!		
	}
	++BIU[activeCPU].PIQ_Address; //Increase the address to the next location!
	//Next data! Take 4 cycles on 8088, 2 on 8086 when loading words/4 on 8086 when loading a single byte.
}

void BIU_dosboxTick()
{
	if (BIU[activeCPU].PIQ) //Prefetching?
	{
		for (;fifobuffer_freesize(BIU[activeCPU].PIQ);)
		{
			CPU_fillPIQ(); //Keep the FIFO fully filled!
		}
	}
}

byte CPU_readOP(byte *result) //Reads the operation (byte) at CS:EIP
{
	uint_32 instructionEIP = CPU[activeCPU].registers->EIP; //Our current instruction position is increased always!
	if (CPU[activeCPU].resetPending) return 1; //Disable all instruction fetching when we're resetting!
	if (BIU[activeCPU].PIQ) //PIQ present?
	{
		PIQ_retry: //Retry after refilling PIQ!
		//if ((CPU[activeCPU].prefetchclock&(((EMULATED_CPU<=CPU_NECV30)<<1)|1))!=((EMULATED_CPU<=CPU_NECV30)<<1)) return 1; //Stall when not T3(80(1)8X) or T0(286+).
		//Execution can start on any cycle!
		//Protection checks have priority over reading the PIQ! The prefetching stops when errors occur when prefetching, we handle the prefetch error when reading the opcode from the BIU, which has to happen before the BIU is retrieved!
		if (checkMMUaccess(CPU_SEGMENT_CS, CPU[activeCPU].registers->CS, instructionEIP,3,getCPL(),!CODE_SEGMENT_DESCRIPTOR_D_BIT(),0)) //Error accessing memory?
		{
			return 1; //Abort on fault!
		}
		if (readfifobuffer(BIU[activeCPU].PIQ,result)) //Read from PIQ?
		{
			if (cpudebugger) //We're an OPcode retrieval and debugging?
			{
				MMU_addOP(*result); //Add to the opcode cache!
			}
			++CPU[activeCPU].registers->EIP; //Increase EIP to give the correct point to use!
			++CPU[activeCPU].cycles_Prefetch; //Fetching from prefetch takes 1 cycle!
			return 0; //Give the prefetched data!
		}
		//Not enough data in the PIQ? Refill for the next data!
		return 1; //Wait for the PIQ to have new data! Don't change EIP(this is still the same)!
		CPU_fillPIQ(); //Fill instruction cache with next data!
		goto PIQ_retry; //Read again!
	}
	if (checkMMUaccess(CPU_SEGMENT_CS, CPU[activeCPU].registers->CS, instructionEIP,3,getCPL(),!CODE_SEGMENT_DESCRIPTOR_D_BIT(),0)) //Error accessing memory?
	{
		return 1; //Abort on fault!
	}
	*result = MMU_rb(CPU_SEGMENT_CS, CPU[activeCPU].registers->CS, instructionEIP, 3,!CODE_SEGMENT_DESCRIPTOR_D_BIT()); //Read OPcode directly from memory!
	if (cpudebugger) //We're an OPcode retrieval and debugging?
	{
		MMU_addOP(*result); //Add to the opcode cache!
	}
	++CPU[activeCPU].registers->EIP; //Increase EIP, since we don't have to worrt about the prefetch!
	++CPU[activeCPU].cycles_Prefetch; //Fetching from prefetch takes 1 cycle!
	return 0; //Give the result!
}

byte CPU_readOPw(word *result) //Reads the operation (word) at CS:EIP
{
	static byte temp, temp2;
	if ((CPU[activeCPU].instructionfetch.CPU_fetchparameterPos&1)==0) //First opcode half?
	{
		if (CPU_readOP(&temp)) return 1; //Read OPcode!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
		++CPU[activeCPU].instructionfetch.CPU_fetchparameterPos; //Next position!
	}
	if ((CPU[activeCPU].instructionfetch.CPU_fetchparameterPos&1)==1) //First second half?
	{
		if (CPU_readOP(&temp2)) return 1; //Read OPcode!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
		++CPU[activeCPU].instructionfetch.CPU_fetchparameterPos; //Next position!
		*result = LE_16BITS(temp|(temp2<<8)); //Give result!
	}
	return 0; //We're fetched!
}

byte CPU_readOPdw(uint_32 *result) //Reads the operation (32-bit unsigned integer) at CS:EIP
{
	static word resultw1, resultw2;
	if ((CPU[activeCPU].instructionfetch.CPU_fetchparameterPos&2)==0) //First opcode half?
	{
		if (CPU_readOPw(&resultw1)) return 1; //Read OPcode!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
	}
	if ((CPU[activeCPU].instructionfetch.CPU_fetchparameterPos&2)==2) //Second opcode half?
	{
		if (CPU_readOPw(&resultw2)) return 1; //Read OPcode!
		if (CPU[activeCPU].faultraised) return 1; //Abort on fault!
		*result = LE_32BITS((((uint_32)resultw2)<<16)|((uint_32)resultw1)); //Give result!
	}
	return 0; //We're fetched!
}

byte fulltransfer=0; //Are we to fully finish the transfer in one go?
OPTINLINE byte BIU_processRequests(byte memory_waitstates)
{
	if (BIU[activeCPU].currentrequest) //Do we have a pending request we're handling? This is used for 16-bit and 32-bit requests!
	{
		CPU[activeCPU].BUSactive = 1; //Start memory or BUS cycles!
		switch (BIU[activeCPU].currentrequest&REQUEST_TYPEMASK) //What kind of request?
		{
			//Memory operations!
			case REQUEST_MMUREAD:
				fulltransferMMUread:
				//MMU_generateaddress(segdesc,*CPU[activeCPU].SEGMENT_REGISTERS[segdesc],offset,0,0,is_offset16); //Generate the address on flat memory!
				BIU[activeCPU].currentresult |= (BIU_directrb((BIU[activeCPU].currentaddress),(((BIU[activeCPU].currentrequest&REQUEST_SUBMASK)>>REQUEST_SUBSHIFT)>>8))<<(BIU_access_readshift[((BIU[activeCPU].currentrequest&REQUEST_SUBMASK)>>REQUEST_SUBSHIFT)])); //Read subsequent byte!
				BIU[activeCPU].waitstateRAMremaining += memory_waitstates; //Apply the waitstates for the fetch!
				if ((BIU[activeCPU].currentrequest&REQUEST_SUBMASK)==((BIU[activeCPU].currentrequest&REQUEST_16BIT)?REQUEST_SUB1:REQUEST_SUB3)) //Finished the request?
				{
					if (BIU_response(BIU[activeCPU].currentresult)) //Result given?
					{
						BIU[activeCPU].currentrequest = REQUEST_NONE; //No request anymore! We're finished!
					}
				}
				else
				{
					BIU[activeCPU].currentrequest += REQUEST_SUB1; //Request next 8-bit half next(high byte)!
					++BIU[activeCPU].currentaddress; //Next address!
					if ((fulltransfer==2) && ((BIU[activeCPU].currentaddress&3)==2)) return 1; //Finished 16-bit half of a split 32-bit transfer?
					if (fulltransfer) goto fulltransferMMUread;
				}
				return 1; //Handled!
				break;
			case REQUEST_MMUWRITE:
				fulltransferMMUwrite:
				BIU_directwb((BIU[activeCPU].currentaddress),(BIU[activeCPU].currentpayload[0]>>(BIU_access_writeshift[((BIU[activeCPU].currentrequest&REQUEST_SUBMASK)>>REQUEST_SUBSHIFT)])&0xFF),((BIU[activeCPU].currentrequest&REQUEST_SUBMASK)>>REQUEST_SUBSHIFT)); //Write directly to memory now!
				BIU[activeCPU].waitstateRAMremaining += memory_waitstates; //Apply the waitstates for the fetch!
				if ((BIU[activeCPU].currentrequest&REQUEST_SUBMASK)==((BIU[activeCPU].currentrequest&REQUEST_16BIT)?REQUEST_SUB1:REQUEST_SUB3)) //Finished the request?
				{
					if (BIU_response(1)) //Result given? We're giving OK!
					{
						BIU[activeCPU].currentrequest = REQUEST_NONE; //No request anymore! We're finished!
					}
				}
				else
				{
					BIU[activeCPU].currentrequest += REQUEST_SUB1; //Request next 8-bit half next(high byte)!
					++BIU[activeCPU].currentaddress; //Next address!
					if ((fulltransfer==2) && ((BIU[activeCPU].currentaddress&3)==2)) return 1; //Finished 16-bit half of a split 32-bit transfer?
					if (fulltransfer) goto fulltransferMMUwrite;
				}
				return 1; //Handled!
				break;
			//I/O operations!
			case REQUEST_IOREAD:
				fulltransferIOread:
				if ((BIU[activeCPU].currentrequest&REQUEST_SUBMASK)==((BIU[activeCPU].currentrequest&REQUEST_16BIT)?REQUEST_SUB1:REQUEST_SUB3)) //Finished the request?
				{
					if (BIU_response(BIU[activeCPU].currentresult)) //Result given?
					{
						BIU[activeCPU].currentrequest = REQUEST_NONE; //No request anymore! We're finished!
					}
				}
				else
				{
					BIU[activeCPU].currentrequest += REQUEST_SUB1; //Request next 8-bit half next(high byte)!
					++BIU[activeCPU].currentaddress; //Next address!
					if ((fulltransfer==2) && ((BIU[activeCPU].currentaddress&3)==2)) return 1; //Finished 16-bit half of a split 32-bit transfer?
					if (fulltransfer) goto fulltransferIOread;
				}
				return 1; //Handled!
				break;
			case REQUEST_IOWRITE:
				fulltransferIOwrite:
				if ((BIU[activeCPU].currentrequest&REQUEST_SUBMASK)==((BIU[activeCPU].currentrequest&REQUEST_16BIT)?REQUEST_SUB1:REQUEST_SUB3)) //Finished the request?
				{
					if (BIU_response(1)) //Result given? We're giving OK!
					{
						BIU[activeCPU].currentrequest = REQUEST_NONE; //No request anymore! We're finished!
					}
				}
				else
				{
					BIU[activeCPU].currentrequest += REQUEST_SUB1; //Request next 8-bit half next(high byte)!
					++BIU[activeCPU].currentaddress; //Next address!
					if ((fulltransfer==2) && ((BIU[activeCPU].currentaddress&3)==2)) return 1; //Finished 16-bit half of a split 32-bit transfer?
					if (fulltransfer) goto fulltransferIOwrite;
				}
				return 1; //Handled!
				break;
			default:
			case REQUEST_NONE: //Unknown request?
				BIU[activeCPU].currentrequest = REQUEST_NONE; //No request anymore! We're finished!
				break; //Ignore the entire request!
		}
	}
	else if (BIU_haveRequest()) //Do we have a request to handle first?
	{
		if (BIU_readRequest(&BIU[activeCPU].currentrequest,&BIU[activeCPU].currentpayload[0],&BIU[activeCPU].currentpayload[1])) //Read the request, if available!
		{
			fulltransfer = 0; //Init full transfer flag!
			switch (BIU[activeCPU].currentrequest&REQUEST_TYPEMASK) //What kind of request?
			{
				//Memory operations!
				case REQUEST_MMUREAD:
					CPU[activeCPU].BUSactive = 1; //Start memory or BUS cycles!
					if ((BIU[activeCPU].currentrequest&REQUEST_16BIT) || (BIU[activeCPU].currentrequest&REQUEST_32BIT)) //16/32-bit?
					{
						BIU[activeCPU].currentrequest |= REQUEST_SUB1; //Request 16-bit half next(high byte)!
					}
					BIU[activeCPU].currentaddress = (BIU[activeCPU].currentpayload[0]&0xFFFFFFFF); //Address to use!
					BIU[activeCPU].currentresult = ((BIU_directrb((BIU[activeCPU].currentaddress),0))<<BIU_access_readshift[0]); //Read first byte!
					if ((BIU[activeCPU].currentrequest&REQUEST_SUBMASK)==REQUEST_SUB0) //Finished the request?
					{
						if (BIU_response(BIU[activeCPU].currentresult)) //Result given?
						{
							BIU[activeCPU].currentrequest = REQUEST_NONE; //No request anymore! We're finished!
						}
						else //Response failed?
						{
							BIU[activeCPU].currentrequest &= ~REQUEST_SUB1; //Request low 8-bit half again(low byte)!
						}
					}
					else
					{
						fulltransfer = BIU_isfulltransfer(); //Are we a full transfer?
						++BIU[activeCPU].currentaddress; //Next address!
						if (fulltransfer) goto fulltransferMMUread; //Start Full transfer, when available?
					}
					return 1; //Handled!
					break;
				case REQUEST_MMUWRITE:
					CPU[activeCPU].BUSactive = 1; //Start memory or BUS cycles!
					if ((BIU[activeCPU].currentrequest&REQUEST_16BIT) || (BIU[activeCPU].currentrequest&REQUEST_32BIT)) //16/32-bit?
					{
						BIU[activeCPU].currentrequest |= REQUEST_SUB1; //Request 16-bit half next(high byte)!
					}
					BIU[activeCPU].currentaddress = (BIU[activeCPU].currentpayload[0]&0xFFFFFFFF); //Address to use!
					if ((BIU[activeCPU].currentrequest&REQUEST_SUBMASK)==REQUEST_SUB0) //Finished the request?
					{
						if (BIU_response(1)) //Result given? We're giving OK!
						{
							BIU_directwb((BIU[activeCPU].currentaddress),((BIU[activeCPU].currentpayload[0]>>BIU_access_writeshift[0])&0xFF),0); //Write directly to memory now!
							BIU[activeCPU].currentrequest = REQUEST_NONE; //No request anymore! We're finished!
						}
						else //Response failed? Try again!
						{
							BIU[activeCPU].currentrequest &= ~REQUEST_SUB1; //Request 8-bit half again(low byte)!
						}
					}
					else //Busy request?
					{
						BIU_directwb((BIU[activeCPU].currentpayload[0]&0xFFFFFFFF),(byte)((BIU[activeCPU].currentpayload[0]>>BIU_access_writeshift[0])&0xFF),0); //Write directly to memory now!
						fulltransfer = BIU_isfulltransfer(); //Are we a full transfer?
						++BIU[activeCPU].currentaddress; //Next address!
						if (fulltransfer) goto fulltransferMMUwrite; //Start Full transfer, when available?
					}
					return 1; //Handled!
					break;
				//I/O operations!
				case REQUEST_IOREAD:
					CPU[activeCPU].BUSactive = 1; //Start memory or BUS cycles!
					if ((BIU[activeCPU].currentrequest&REQUEST_16BIT) || (BIU[activeCPU].currentrequest&REQUEST_32BIT)) //16/32-bit?
					{
						BIU[activeCPU].currentrequest |= REQUEST_SUB1; //Request 16-bit half next(high byte)!
					}
					BIU[activeCPU].currentaddress = (BIU[activeCPU].currentpayload[0]&0xFFFFFFFF); //Address to use!
					if (BIU[activeCPU].currentrequest&REQUEST_32BIT) //32-bit?
					{
						BIU[activeCPU].currentresult = PORT_IN_D(BIU[activeCPU].currentaddress&0xFFFF); //Read byte!
					}
					else if (BIU[activeCPU].currentrequest&REQUEST_16BIT) //16-bit?
					{
						BIU[activeCPU].currentresult = PORT_IN_W(BIU[activeCPU].currentaddress&0xFFFF); //Read byte!
					}
					else //8-bit?
					{
						BIU[activeCPU].currentresult = PORT_IN_B(BIU[activeCPU].currentaddress&0xFFFF); //Read byte!
					}
					if ((BIU[activeCPU].currentrequest&REQUEST_SUBMASK)==REQUEST_SUB0) //Finished the request?
					{
						if (BIU_response(BIU[activeCPU].currentresult)) //Result given?
						{
							BIU[activeCPU].currentrequest = REQUEST_NONE; //No request anymore! We're finished!
						}
						else //Response failed?
						{
							BIU[activeCPU].currentrequest &= ~REQUEST_SUB1; //Request low 8-bit half again(low byte)!
						}
					}
					else
					{
						fulltransfer = BIU_isfulltransfer(); //Are we a full transfer?
						++BIU[activeCPU].currentaddress; //Next address!
						if (fulltransfer) goto fulltransferIOread; //Start Full transfer, when available?
					}
					return 1; //Handled!
					break;
				case REQUEST_IOWRITE:
					CPU[activeCPU].BUSactive = 1; //Start memory or BUS cycles!
					if ((BIU[activeCPU].currentrequest&REQUEST_16BIT) || (BIU[activeCPU].currentrequest&REQUEST_32BIT)) //16/32-bit?
					{
						BIU[activeCPU].currentrequest |= REQUEST_SUB1; //Request 16-bit half next(high byte)!
					}
					BIU[activeCPU].currentaddress = (BIU[activeCPU].currentpayload[0]&0xFFFFFFFF); //Address to use!
					if (BIU[activeCPU].currentrequest&REQUEST_32BIT) //32-bit?
					{
						BIU[activeCPU].currentrequest |= REQUEST_SUB1; //Request 16-bit half next(high byte)!
						PORT_OUT_D((word)(BIU[activeCPU].currentpayload[0]&0xFFFF),(uint_32)((BIU[activeCPU].currentpayload[0]>>32)&0xFFFFFFFF)); //Write to memory now!									
					}
					else if (BIU[activeCPU].currentrequest&REQUEST_16BIT) //16-bit?
					{
						BIU[activeCPU].currentrequest |= REQUEST_SUB1; //Request 16-bit half next(high byte)!
						PORT_OUT_W((word)(BIU[activeCPU].currentpayload[0]&0xFFFF),(word)((BIU[activeCPU].currentpayload[0]>>32)&0xFFFFFFFF)); //Write to memory now!									
					}
					else //8-bit?
					{
						PORT_OUT_B((word)(BIU[activeCPU].currentpayload[0]&0xFFFF),(byte)((BIU[activeCPU].currentpayload[0]>>32)&0xFFFFFFFF)); //Write to memory now!									
					}
					if ((BIU[activeCPU].currentrequest&REQUEST_SUBMASK)==REQUEST_SUB0) //Finished the request?
					{
						if (BIU_response(1)) //Result given? We're giving OK!
						{
							BIU[activeCPU].currentrequest = REQUEST_NONE; //No request anymore! We're finished!
						}
						else //Response failed?
						{
							BIU[activeCPU].currentrequest &= ~REQUEST_SUB1; //Request low 8-bit half again(low byte)!
						}
					}
					else
					{
						fulltransfer = BIU_isfulltransfer(); //Are we a full transfer?
						++BIU[activeCPU].currentaddress; //Next address!
						if (fulltransfer) goto fulltransferIOwrite; //Start Full transfer, when available?
					}
					return 1; //Handled!
					break;
				default:
				case REQUEST_NONE: //Unknown request?
					BIU[activeCPU].currentrequest = REQUEST_NONE; //No request anymore! We're finished!
					break; //Ignore the entire request!
			}
		}
	}
	return 0; //No requests left!
}

byte CPU386_WAITSTATE_DELAY = 0; //386+ Waitstate, which is software-programmed?

void CPU_tickBIU()
{
	byte memory_waitstates = 0;
	CPU_CycleTimingInfo *cycleinfo;
	cycleinfo = &BIU[activeCPU].cycleinfo; //Our cycle info to use!

	byte BIU_active = 1; //Are we counted as active cycles?
	//Determine memory waitstate first!
	if (EMULATED_CPU==CPU_80286) //Process normal memory cycles!
	{
		memory_waitstates += CPU286_WAITSTATE_DELAY; //One waitstate RAM!
	}
	else if (EMULATED_CPU==CPU_80386) //Waitstate memory to add?
	{
		memory_waitstates += CPU386_WAITSTATE_DELAY; //One waitstate RAM!
	}

	//Now, normal processing!
	if (!BIU[activeCPU].PIQ) return; //Disable invalid PIQ!
	if ((cycleinfo->cycles==0) && (cycleinfo->cycles_stallBUS==0)) //Are we ready to continue into the next phase?
	{
		cycleinfo->cycles = CPU[activeCPU].cycles; //How many cycles have been spent on the instruction?
		if (cycleinfo->cycles==0) cycleinfo->cycles = 1; //Take 1 cycle at least!

		cycleinfo->prefetchcycles = CPU[activeCPU].cycles_Prefetch; //Prefetch cycles!
		cycleinfo->prefetchcycles += CPU[activeCPU].cycles_EA; //EA cycles!
		cycleinfo->cycles_stallBIU = CPU[activeCPU].cycles_stallBIU; //BIU stall cycles!
		cycleinfo->cycles_stallBUS = CPU[activeCPU].cycles_stallBUS; //BUS stall cycles!
		CPU[activeCPU].cycles_Prefetch = CPU[activeCPU].cycles_EA = CPU[activeCPU].cycles_stallBIU = CPU[activeCPU].cycles_stallBUS = 0; //We don't have any of these after this!
	}
	//Now we have the amount of cycles we're idling.
	if (EMULATED_CPU<=CPU_NECV30) //Old CPU?
	{
		BIU[activeCPU].TState = (BIU[activeCPU].prefetchclock&3); //Currently emulated T-state!
		if (cycleinfo->cycles_stallBUS) //Stall the BUS?
		{
			--cycleinfo->cycles_stallBUS; //Stall!
			BIU[activeCPU].stallingBUS = 1; //Stalling!
		}
		else
		{
			BIU[activeCPU].stallingBUS = 0; //Not stalling BUS!
			if ((CPU[activeCPU].halt & 0xC) && ((BIU[activeCPU].prefetchclock&3)==3)) //CGA wait state is active?
			{
				if ((CPU[activeCPU].halt&0xC) == 8) //Are we to resume execution now?
				{
					CPU[activeCPU].halt &= ~0xC; //We're resuming execution!
					goto resumeFromHLT; //We're resuming from HLT state!
				}
				goto waitstate; //Count cycles normally!
			}
			else //Normal waitstate handling?
			{
				resumeFromHLT:
				if (((BIU[activeCPU].prefetchclock&3)==3) && (BIU[activeCPU].waitstateRAMremaining)) //T4? Check for waitstate RAM first!
				{
					//WaitState RAM busy?
					--BIU[activeCPU].waitstateRAMremaining; //Tick waitstate RAM!
					waitstate:
					BIU[activeCPU].TState = 0xFF; //Waitstate RAM!
					BIU_active = 0; //Count as inactive BIU: don't advance cycles!
				}
				else //No waitstates to apply?
				{
					if (CPU[activeCPU].BUSactive==2) //Handling a DRAM refresh? We're idling on DMA!
					{
						++CPU[activeCPU].cycles_Prefetch_DMA;
						BIU[activeCPU].TState = 0xFE; //DMA cycle special identifier!
						BIU_active = 0; //Count as inactive BIU: don't advance cycles!
					}
					else //Active CPU cycle?
					{
						cycleinfo->curcycle = (BIU[activeCPU].prefetchclock&3); //Current cycle!
						if (cycleinfo->cycles_stallBIU) //To stall?
						{
							--cycleinfo->cycles_stallBIU; //Stall the BIU instead of normal runtime!
							BIU[activeCPU].stallingBUS = 3; //Stalling fetching!
							if (CPU[activeCPU].BUSactive==1) //We're active?
							{
								if ((BIU[activeCPU].prefetchclock&3)!=0) //Not T1 yet?
								{
									if ((++BIU[activeCPU].prefetchclock&3)==0) //From T4 to T1?
									{
										CPU[activeCPU].BUSactive = 0; //Inactive BUS!
									}
								}
							}
							else
							{
								BIU_active = 0; //Count as inactive BIU: don't advance cycles!
							}
						}
						else if ((cycleinfo->curcycle==0) && (CPU[activeCPU].BUSactive==0)) //T1 while not busy? Start transfer, if possible!
						{
							if (cycleinfo->prefetchcycles) {--cycleinfo->prefetchcycles; goto tryprefetch808X;}
							else
							{
								tryprefetch808X:
								if (BIU_processRequests(memory_waitstates)) //Processing a request?
								{
									BIU[activeCPU].requestready = 0; //We're starting a request!
									++BIU[activeCPU].prefetchclock; //Tick!					
								}
								else if (fifobuffer_freesize(BIU[activeCPU].PIQ)>=((uint_32)2>>CPU_databussize)) //Prefetch cycle when not requests are handled? Else, NOP cycle!
								{
									CPU[activeCPU].BUSactive = 1; //Start memory cycles!
									PIQ_block = 0; //We're never blocking(only 1 access)!
									CPU_fillPIQ(); //Add a byte to the prefetch!
									if (CPU_databussize==0) CPU_fillPIQ(); //8086? Fetch words!
									++CPU[activeCPU].cycles_Prefetch_BIU; //Cycles spent on prefetching on BIU idle time!
									BIU[activeCPU].waitstateRAMremaining += memory_waitstates; //Apply the waitstates for the fetch!
									BIU[activeCPU].requestready = 0; //We're pending a request!
									++BIU[activeCPU].prefetchclock; //Tick!
								}
								else //Nothing to do?
								{
									BIU[activeCPU].stallingBUS = 2; //Stalling!
								}
							}
						}
						else if (cycleinfo->curcycle) //Busy transfer?
						{
							++BIU[activeCPU].prefetchclock; //Tick running transfer T-cycle!
						}
						if ((cycleinfo->curcycle==3) && ((BIU[activeCPU].prefetchclock&3)!=3) && (CPU[activeCPU].BUSactive==1)) //Finishing transfer on T4?
						{
							CPU[activeCPU].BUSactive = 0; //Inactive BUS!
							BIU[activeCPU].requestready = 1; //The request is ready to be served!
						}

						if (cycleinfo->cycles && BIU_active) --cycleinfo->cycles; //Decrease the amount of cycles that's left!
					}
				}
			}
		}
	}
	else //286+
	{
		BIU[activeCPU].TState = (BIU[activeCPU].prefetchclock&1); //Currently emulated T-state!
		if (cycleinfo->cycles_stallBUS) //Stall the BUS?
		{
			--cycleinfo->cycles_stallBUS; //Stall!
			BIU[activeCPU].stallingBUS = 1; //Stalling!
			BIU_active = 0; //Count as inactive BIU: don't advance cycles!
		}
		else
		{
			BIU[activeCPU].stallingBUS = 0; //Not stalling BUS!
			if ((CPU[activeCPU].halt & 0xC) && ((BIU[activeCPU].prefetchclock&1)==1)) //CGA wait state is active?
			{
				if ((CPU[activeCPU].halt&0xC) == 8) //Are we to resume execution now?
				{
					CPU[activeCPU].halt &= ~0xC; //We're resuming execution!
					goto resumeFromHLT286; //We're resuming from HLT state!
				}
				goto waitstate286; //Count cycles normally!
			}
			else //Normal waitstate handling?
			{
				resumeFromHLT286:
				if (((BIU[activeCPU].prefetchclock&1)==1) && (BIU[activeCPU].waitstateRAMremaining)) //T2? Check for waitstate RAM first!
				{
					//WaitState RAM busy?
					--BIU[activeCPU].waitstateRAMremaining; //Tick waitstate RAM!
					waitstate286:
					BIU[activeCPU].TState = 0xFF; //Waitstate RAM!
					BIU_active = 0; //Count as inactive BIU: don't advance cycles!
				}
				else //No waitstates to apply?
				{
					if (CPU[activeCPU].BUSactive==2) //Handling a DRAM refresh? We're idling on DMA!
					{
						++CPU[activeCPU].cycles_Prefetch_DMA;
						BIU[activeCPU].TState = 0xFE; //DMA cycle special identifier!
						BIU_active = 0; //Count as inactive BIU: don't advance cycles!
					}
					else //Active CPU cycle?
					{
						cycleinfo->curcycle = (BIU[activeCPU].prefetchclock&1); //Current cycle!
						if (cycleinfo->cycles_stallBIU) //To stall?
						{
							--cycleinfo->cycles_stallBIU; //Stall the BIU instead of normal runtime!
							BIU[activeCPU].stallingBUS = 3; //Stalling fetching!
							if (CPU[activeCPU].BUSactive==1) //We're active?
							{
								if ((BIU[activeCPU].prefetchclock&1)!=0) //Not T1 yet?
								{
									if ((++BIU[activeCPU].prefetchclock&1)==0) //From T2 to T1?
									{
										CPU[activeCPU].BUSactive = 0; //Inactive BUS!
									}
								}
							}
							else
							{
								BIU_active = 0; //Count as inactive BIU: don't advance cycles!
							}
						}
						else if ((cycleinfo->curcycle==0) && (CPU[activeCPU].BUSactive==0)) //T1 while not busy? Start transfer, if possible!
						{
							if (cycleinfo->prefetchcycles) {--cycleinfo->prefetchcycles; goto tryprefetch80286;}
							else
							{
								tryprefetch80286:
								if (BIU_processRequests(memory_waitstates)) //Processing a request?
								{
									BIU[activeCPU].requestready = 0; //We're starting a request!
									++BIU[activeCPU].prefetchclock; //Tick!					
								}
								else if (fifobuffer_freesize(BIU[activeCPU].PIQ)>1) //Prefetch cycle when not requests are handled(2 free spaces only)? Else, NOP cycle!
								{
									CPU[activeCPU].BUSactive = 1; //Start memory cycles!
									PIQ_block = 3; //We're blocking after 1 byte access when at an odd address!
									CPU_fillPIQ(); CPU_fillPIQ(); //Add a word to the prefetch!
									++CPU[activeCPU].cycles_Prefetch_BIU; //Cycles spent on prefetching on BIU idle time!
									BIU[activeCPU].waitstateRAMremaining += memory_waitstates; //Apply the waitstates for the fetch!
									BIU[activeCPU].requestready = 0; //We're starting a request!
									++BIU[activeCPU].prefetchclock; //Tick!					
								}
								else //Nothing to do?
								{
									BIU[activeCPU].stallingBUS = 2; //Stalling!
								}
							}
						}
						else if (cycleinfo->curcycle) //Busy transfer?
						{
							++BIU[activeCPU].prefetchclock; //Tick running transfer T-cycle!
						}
						if ((cycleinfo->curcycle==1) && ((BIU[activeCPU].prefetchclock&1)!=1) && (CPU[activeCPU].BUSactive==1)) //Finishing transfer on T1?
						{
							CPU[activeCPU].BUSactive = 0; //Inactive BUS!
							BIU[activeCPU].requestready = 1; //The request is ready to be served!
						}
						if (cycleinfo->cycles && BIU_active) --cycleinfo->cycles; //Decrease the amount of cycles that's left!
					}
				}
			}
		}
	}
	CPU[activeCPU].cycles = 1; //Only take 1 cycle: we're cycle-accurate emulation of the BIU(and EU by extension, since we handle that part indirectly as well in our timings, resulting in the full CPU timings)!
}

byte BIU_Ready() //Are we ready to continue execution?
{
	return ((BIU[activeCPU].cycleinfo.cycles==0) && (BIU[activeCPU].cycleinfo.cycles_stallBUS==0)); //We're ready to execute the next instruction (or instruction step) when all cycles are handled(no hardware interrupts are busy)!
}

byte BIU_resetRequested()
{
	return (CPU[activeCPU].resetPending && ((BIU_Ready() && (CPU[activeCPU].halt==0))||CPU[activeCPU].halt==1) && (CPU[activeCPU].BUSactive==0)); //Finished executing or halting, and reset is Pending?
}
