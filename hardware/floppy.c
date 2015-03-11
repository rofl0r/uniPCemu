#include "headers/types.h" //Basic typedefs!
#include "headers/hardware/8237A.h" //DMA controller support!
#include "headers/hardware/ports.h" //Port support!

//What IRQ is expected of floppy disk I/O
#define FLOPPY_IRQ 6
//What DMA channel is expected of floppy disk I/O
#define FLOPPY_DMA 2

struct
{
	union
	{
		struct
		{
			byte DriveNumber : 2; //What drive to address?
			byte REST : 1; //Enable controller when set!
			byte Mode : 1; //0=IRQ channel, 1=DMA mode
			byte MotorControl : 4; //All drive motor statuses!
		};
		byte data; //DOR data!
	} DOR; //DOR
	union
	{
		struct
		{
			byte Busy : 4; //1 if busy in seek mode.
			byte FDCBusy : 1; //Busy: read/write command of FDC in progress.
			byte NonDMA : 1; //1 when not in DMA mode, else DMA mode.
			byte HaveDataForCPU : 1; //1 when has data for CPU, 0 when expecting data.
			byte RQM : 1; //1 when ready for data transfer, 0 when not ready.
		};
		byte data; //MSR data!
	} MSR; //MSR
	union
	{
		byte data; //CCR data!
	} CCR; //CCR
	union
	{
		byte data; //DIR data!
	} DIR; //DIR
	union
	{
		struct
		{
			byte DRATESEL : 2;
			byte PRECOMP : 3;
			byte DSR_0 : 1;
			byte PowerDown : 1;
			byte SWReset : 1;
		};
		byte data; //DSR data!
	} DSR;

} FLOPPY; //Our floppy drive data!

byte PORT_IN_floppy(word port)
{
	switch (port & 0xF) //What port?
	{
	case 0: //SRA?
		return 0; //Not used!
	case 1: //SRB?
		return 0; //Not used!
	case 4: //MSR?
		return FLOPPY.MSR.data; //Give MSR!
	case 5: //Data?
		//Process data!
		return 0; //Default handler!
	case 7: //CCR?
		return FLOPPY.CCR.data; //Give CCR!
	default: //Unknown port?
		return ~0; //Unknown port!
	}
}

void PORT_OUT_floppy(word port, byte value)
{
	switch (port & 0xF) //What port?
	{
	case 2: //DOR?
		FLOPPY.DOR.data = value; //Write to register!
		return; //Finished!
	case 4: //DSR?
		FLOPPY.DSR.data = value; //Write to register!
		return; //Finished!
	case 5: //Data?
		return; //Default handler!
	case 7: //DIR?
		FLOPPY.DIR.data = value; //Write to register!
		return;
	default: //Unknown port?
		return; //Unknown port!
	}
}

//DMA logic

void DMA_floppywrite(byte data)
{
	PORT_OUT_floppy(0x3F5,data); //Send the data to the FDC!
}

byte DMA_floppyread()
{
	return PORT_IN_floppy(0x3F5); //Read from floppy!
}

void initFloppy()
{
	memset(&FLOPPY, 0, sizeof(FLOPPY)); //Initialise floppy!
	registerDMA8(FLOPPY_DMA, &DMA_floppyread, &DMA_floppywrite); //Register our DMA channels!
	register_PORTIN(0x3F0,&PORT_IN_floppy);
	register_PORTIN(0x3F1, &PORT_IN_floppy);
	register_PORTIN(0x3F4, &PORT_IN_floppy);
	register_PORTIN(0x3F5, &PORT_IN_floppy);
	register_PORTIN(0x3F7, &PORT_IN_floppy);
	register_PORTOUT(0x3F2, &PORT_OUT_floppy);
	register_PORTOUT(0x3F4, &PORT_OUT_floppy);
	register_PORTOUT(0x3F5, &PORT_OUT_floppy);
	register_PORTOUT(0x3F7, &PORT_OUT_floppy);
}