#include "headers/types.h" //Basic typedefs!
#include "headers/hardware/8237A.h" //DMA controller support!
#include "headers/hardware/ports.h" //Port support!
#include "headers/bios/io.h" //Basic I/O functionality!
#include "headers/hardware/pic.h" //PIC support!
#include "headers/bios/dskimage.h" //DSK image support!

#include "headers/debugger/debugger.h" //Debugger support!

//Configuration of the FDC...

//Double logging if FLOPPY_LOGFILE2 is defined!
//#define FLOPPY_LOGFILE "floppy"
//#define FLOPPY_LOGFILE2 "debugger"

//What IRQ is expected of floppy disk I/O
#define FLOPPY_IRQ 6
//What DMA channel is expected of floppy disk I/O
#define FLOPPY_DMA 2

//Automatic setup.
#ifdef FLOPPY_LOGFILE
#ifdef FLOPPY_LOGFILE2
#define FLOPPY_LOG(...) { dolog(FLOPPY_LOGFILE,__VA_ARGS__); dolog(FLOPPY_LOGFILE2,__VA_ARGS__); }
#else
#define FLOPPY_LOG(...) { dolog(FLOPPY_LOGFILE,__VA_ARGS__); }
#endif
#else
#define FLOPPY_LOG(...)
#endif

//Logging with debugger only!
#ifdef FLOPPY_LOGFILE
#define FLOPPY_LOGD(...) if (debugger_logging()) {FLOPPY_LOG(__VA_ARGS__)}
#else
#define FLOPPY_LOGD(...)
#endif

typedef struct
{
	uint_64 KB;
	byte SPT;
	byte sides;
	byte tracks;
} FLOPPY_GEOMETRY; //All floppy geometries!

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
			byte BusyInPositioningMode : 4; //1 if busy in seek mode.
			byte CommandBusy : 1; //Busy: read/write command of FDC in progress. Set when received command byte, cleared at end of result phase
			byte NonDMA : 1; //1 when not in DMA mode, else DMA mode, during execution phase.
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
	union
	{
		byte data; //ST0 register!
		struct
		{
			byte UnitSelect : 2;
			byte CurrentHead : 1;
			byte NotReady : 1;
			byte UnitCheck : 1;
			byte SeekEnd : 1;
			byte InterruptCode : 2;
		};
	} ST0;
	union
	{
		byte data; //ST1 register!
		struct
		{
			byte EndOfCylinder : 1;
			byte Always0 : 1;
			byte DataError : 1;
			byte TimeOut : 1;
			byte NoData : 1;
			byte NotWritableDuringWriteCommand : 1;
			byte NoAddressMark : 1;
		};
	} ST1;
	union
	{
		byte data; //ST2 register!
	} ST2;
	union
	{
		byte data; //ST3 register!
	} ST3;
	union
	{
		byte data[2]; //Both data bytes!
		struct
		{
			byte HeadUnloadTime : 4;
			byte StepRate : 4;
			byte NDM : 1;
			byte HeadLoadTime : 7;
		};
	} DriveData[4]; //Specify for each of the 4 floppy drives!
	byte commandstep; //Current command step!
	byte commandbuffer[0x10000]; //Outgoing command buffer!
	word commandposition; //What position in the command (starts with commandstep=commandposition=0).
	byte databuffer[0x10000]; //Incoming data buffer!
	word databufferposition; //How much data is buffered!
	word databuffersize; //How much data are we to buffer!
	byte resultbuffer[0x10]; //Incoming result buffer!
	byte resultposition; //The position in the result!
	uint_64 disk_startpos; //The start position of the buffered data in the floppy disk!
	byte IRQPending; //Are we waiting for an IRQ?
	byte DMAPending; //Pending DMA transfer?
	byte diskchanged[4]; //Disk changed?
	FLOPPY_GEOMETRY *geometries[4]; //Disk geometries!
	FLOPPY_GEOMETRY customgeometry[4]; //Custom disk geometries!
	byte reset_pending; //Reset pending?
	byte currentcylinder[4], currenthead[4], currentsector[4]; //Current head for all 4 drives!
	byte TC; //Terminal count triggered?
	uint_32 sectorstransferred; //Ammount of sectors transferred!
	byte MT; //MT bit, as set by the command, if any!
} FLOPPY; //Our floppy drive data!


//Normal floppy specific stuff

#define KB(x) (x/1024)




/*
{  KB,SPT,SIDES,TRACKS,  }
{ 160,  8, 1   , 40   , 0},
{ 180,  9, 1   , 40   , 0},
{ 200, 10, 1   , 40   , 0},
{ 320,  8, 2   , 40   , 1},
{ 360,  9, 2   , 40   , 1},
{ 400, 10, 2   , 40   , 1},
{ 720,  9, 2   , 80   , 3},
{1200, 15, 2   , 80   , 2},
{1440, 18, 2   , 80   , 4},
{2880, 36, 2   , 80   , 6},

*/

FLOPPY_GEOMETRY geometries[] = { //Differently formatted disks, and their corresponding geometries
	{ 160, 8, 1, 40 }, //160K 5.25"
	{ 320, 8, 2, 40 }, //320K 5.25"
	{ 180, 9, 1, 40 }, //180K 5.25"
	{ 360, 9, 2, 40 }, //360K 5.25"
	{ 720, 9, 2, 80 }, //720K 3.5"
	{ 200, 10, 1, 40 }, //200K 5.25"
	{ 400, 10, 2, 40 }, //400K 5.25"
	{ 1200, 15, 2, 80 }, //1200K 5.25"
	{ 1440, 18, 2, 80 }, //1.44M 3.5"
	{ 2880, 36, 2, 80 }, //2.88M 3.5"
	{ 1680, 21, 2, 80 }, //1.68M 3.5"
	{ 1722, 21, 2, 82 }, //1.722M 3.5"
	{ 1840, 23, 2, 80 } //1.84M 3.5"
};

//BPS=512 always(except differently programmed)!

//Floppy geometries

byte floppy_spt(uint_64 floppy_size)
{
	int i;
	for (i = 0; i<NUMITEMS(geometries); i++)
	{
		if (geometries[i].KB == KB(floppy_size)) return geometries[i].SPT; //Found?
	}
	return 0; //Unknown!
}

byte floppy_tracks(uint_64 floppy_size)
{
	int i;
	for (i = 0; i<NUMITEMS(geometries); i++)
	{
		if (geometries[i].KB == KB(floppy_size)) return geometries[i].tracks; //Found?
	}
	return 0; //Unknown!
}

byte floppy_sides(uint_64 floppy_size)
{
	int i;
	for (i = 0; i<NUMITEMS(geometries); i++)
	{
		if (geometries[i].KB == KB(floppy_size)) return geometries[i].sides; //Found?
	}
	return 0; //Unknown!
}

OPTINLINE void updateFloppyGeometries(byte floppy, byte side, byte track)
{
	uint_64 floppysize = disksize(floppy); //Retrieve disk size for reference!
	byte i;
	char *DSKImageFile = NULL; //DSK image file to use?
	DISKINFORMATIONBLOCK DSKInformation;
	TRACKINFORMATIONBLOCK DSKTrackInformation;
	FLOPPY.geometries[floppy] = NULL; //Init geometry to unknown!
	for (i = 0; i < NUMITEMS(geometries); i++) //Update the geometry!
	{
		if (geometries[i].KB == KB(floppysize)) //Found?
		{
			FLOPPY.geometries[floppy] = &geometries[i]; //The geometry we use!
			return; //Stop searching!
		}
	}

	//Unknown geometry!
	if (DSKImageFile = getDSKimage(floppy ? FLOPPY1 : FLOPPY0)) //Are we a DSK image file?
	{
		if (readDSKInfo(DSKImageFile, &DSKInformation)) //Gotten information about the DSK image?
		{
			if (readDSKTrackInfo(DSKImageFile, side, track, &DSKTrackInformation))
			{
				FLOPPY.geometries[floppy] = &FLOPPY.customgeometry[floppy]; //Apply custom geometry!
				FLOPPY.customgeometry[floppy].sides = DSKInformation.NumberOfSides; //Number of sides!
				FLOPPY.customgeometry[floppy].tracks = DSKInformation.NumberOfTracks; //Number of tracks!
				FLOPPY.customgeometry[floppy].SPT = DSKTrackInformation.numberofsectors; //Number of sectors in this track!
			}
		}
	}
}

OPTINLINE uint_32 floppy_LBA(byte floppy, word side, word track, word sector)
{
	updateFloppyGeometries(floppy,(byte)side,(byte)track); //Update the floppy geometries!
	if (!FLOPPY.geometries[floppy]) return 0; //Unknown floppy geometry!
	return (uint_32)(((track*FLOPPY.geometries[floppy]->sides) + side) * FLOPPY.geometries[floppy]->SPT) + sector - 1; //Give LBA for floppy!
}

//Sector size

OPTINLINE word translateSectorSize(byte size)
{
	return 128<<size; //Give the translated sector size!
}

void FLOPPY_notifyDiskChanged(int disk)
{
	switch (disk)
	{
	case FLOPPY0:
		FLOPPY.diskchanged[0] = 1; //Changed!
		break;
	case FLOPPY1:
		FLOPPY.diskchanged[1] = 1; //Changed!
		break;
	}
}

OPTINLINE void FLOPPY_raiseIRQ() //Execute an IRQ!
{
	FLOPPY.IRQPending = 1; //We're waiting for an IRQ!
	doirq(FLOPPY_IRQ); //Execute the IRQ!
}

OPTINLINE void FLOPPY_lowerIRQ()
{
	FLOPPY.IRQPending = 0;
	removeirq(FLOPPY_IRQ); //Lower the IRQ!
}

OPTINLINE void FLOPPY_reset() //Resets the floppy disk command!
{
	FLOPPY_LOG("FLOPPY: Reset requested!")
	FLOPPY.DOR.MotorControl = 0; //Reset motors!
	FLOPPY.DOR.DriveNumber = 0; //Reset drives!
	FLOPPY.DOR.Mode = 0; //IRQ channel!
	FLOPPY.MSR.data = 0; //Default to no data!
	FLOPPY.commandposition = 0; //No command!
	FLOPPY.commandstep = 0; //Reset step!
	FLOPPY.ST0.data = 0xC0; //Reset ST0 to the correct value: drive became not ready!
	FLOPPY.ST1.data = FLOPPY.ST2.data = 0; //Reset the ST data!
	FLOPPY.reset_pending = 4; //We have a reset pending for all 4 drives!
	memset(FLOPPY.currenthead, 0, sizeof(FLOPPY.currenthead)); //Clear the current heads!
	memset(FLOPPY.currentcylinder, 0, sizeof(FLOPPY.currentcylinder)); //Clear the current heads!
	memset(FLOPPY.currentsector, 0, sizeof(FLOPPY.currentsector)); //Clear the current heads!
	FLOPPY.TC = 0; //Disable TC identifier!
	FLOPPY_raiseIRQ(); //Raise the IRQ flag!
}

//Execution after command and data phrases!

OPTINLINE void updateFloppyMSR() //Update the floppy MSR!
{
	switch (FLOPPY.commandstep) //What command step?
	{
	case 0: //Command?
		FLOPPY.sectorstransferred = 0; //There's nothing transferred yet!
		FLOPPY.MSR.CommandBusy = 0; //Not busy: we're waiting for a command!
		FLOPPY.MSR.RQM = 1; //Ready for data transfer!
		FLOPPY.MSR.HaveDataForCPU = 0; //We don't have data for the CPU!
		break;
	case 1: //Parameters?
		FLOPPY.MSR.CommandBusy = 1; //Default: busy!
		FLOPPY.MSR.RQM = 1; //Ready for data transfer!
		FLOPPY.MSR.HaveDataForCPU = 0; //We don't have data for the CPU!
		break;
	case 2: //Data?
		FLOPPY.MSR.CommandBusy = 1; //Default: busy!
		//Check DMA, RQM and Busy flag!
		switch (FLOPPY.commandbuffer[0]) //What command are we processing?
		{
		case 0x5: //Write sector?
		case 0x9: //Write deleted sector?
		case 0xD: //Format sector?
		case 0x6: //Read sector?
		case 0xC: //Read deleted sector?
			FLOPPY.MSR.NonDMA = (!FLOPPY.DOR.Mode); //Not in DMA mode?
			if (FLOPPY.MSR.NonDMA) //Not in DMA mode?
			{
				FLOPPY.MSR.RQM = 1; //Data transfer!
			}
			else //DMA mode transfer?
			{
				FLOPPY.MSR.RQM = 0; //No transfer!
			}
			break;
		default: //Unknown command?
			break; //Don't process!
		}

		//Check data direction!
		switch (FLOPPY.commandbuffer[0]) //Process input/output to/from controller!
		{
		case 0x5: //Write sector?
		case 0x9: //Write deleted sector?
		case 0xD: //Format sector?
			FLOPPY.MSR.HaveDataForCPU = 0; //We request data from the CPU!
			break;
		case 0x6: //Read sector?
		case 0xC: //Read deleted sector?
			FLOPPY.MSR.HaveDataForCPU = 1; //We have data for the CPU!
			break;
		default: //Unknown direction?
			FLOPPY.MSR.HaveDataForCPU = 0; //Nothing, say output by default!
			break;
		}
		break;
	case 3: //Result?
		FLOPPY.MSR.CommandBusy = 1; //Default: busy!
		FLOPPY.MSR.RQM = 1; //Data transfer!
		FLOPPY.MSR.HaveDataForCPU = 1; //We have data for the CPU!
		break;
	case 0xFF: //Error?
		FLOPPY.MSR.CommandBusy = 1; //Default: busy!
		FLOPPY.MSR.RQM = 1; //Data transfer!
		FLOPPY.MSR.HaveDataForCPU = 1; //We have data for the CPU!
		break;
	default: //Unknown status?
		break; //Unknown?
	}
	static byte oldMSR = 0; //Old MSR!
	if (FLOPPY.MSR.data != oldMSR) //MSR changed?
	{
		oldMSR = FLOPPY.MSR.data; //Update old!
		FLOPPY_LOGD("FLOPPY: MSR: %02x", FLOPPY.MSR.data) //The updated MSR!
	}
}

OPTINLINE void updateFloppyDIR() //Update the floppy DIR!
{
	FLOPPY.DIR.data = (FLOPPY.diskchanged[0] || FLOPPY.diskchanged[1]); //Disk changed?
	FLOPPY.diskchanged[0] = 0; //Reset!
	FLOPPY.diskchanged[1] = 0; //Reset!
}

OPTINLINE void updateFloppyTrack0()
{
	FLOPPY.ST0.SeekEnd = (FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber] == 0); //Are we at cylinder 0?
}

OPTINLINE void updateFloppyWriteProtected(byte iswrite)
{
	FLOPPY.ST1.data = (FLOPPY.ST1.data&~2); //Default: not write protected!
	if (drivereadonly(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0) && iswrite) //Read-only drive and tried to write?
	{
		FLOPPY.ST1.data |= 2; //Write protected!
	}
}

OPTINLINE byte floppy_increasesector(byte floppy) //Increase the sector number automatically!
{
	byte result = 1; //Default: read/write more
	if (FLOPPY.geometries[floppy]) //Do we have a valid geometry?
	{
		if (++FLOPPY.currentsector[floppy] > FLOPPY.geometries[floppy]->SPT) //Overflow next sector by parameter?
		{
			if (!FLOPPY.DOR.Mode) //Non-DMA mode?
			{
				if ((FLOPPY.MT && FLOPPY.currenthead[floppy]) || !FLOPPY.MT) //Multi-track and side 1, or not Multi-track?
				{
					result = 0; //SPT finished!
				}
			}
			FLOPPY.currentsector[floppy] = 1; //Reset sector number!
			if (++FLOPPY.currenthead[floppy] >= FLOPPY.geometries[floppy]->sides) //Side overflow?
			{
				FLOPPY.currenthead[floppy] = 0; //Reset side number!
				if (++FLOPPY.currentcylinder[floppy] >= FLOPPY.geometries[floppy]->tracks) //Track overflow?
				{
					FLOPPY.currentcylinder[floppy] = 0; //Reset track number!
				}
			}
		}
	}

	FLOPPY.ST0.CurrentHead = FLOPPY.currenthead[floppy]; //Our idea of the current head!

	if (FLOPPY.DOR.Mode) //DMA mode determines our triggering?
	{
		if (result) //OK to transfer more?
		{
			result = !FLOPPY.TC; //No terminal count triggered? Then we read the next sector!
		}
		else //Error occurred during DMA transfer?
		{
			result = 2; //Abort!
			FLOPPY.ST0.InterruptCode = 1; //Couldn't finish correctly!
			FLOPPY.ST0.SeekEnd = 0; //Failed!
		}
	}

	++FLOPPY.sectorstransferred; //Increase the amount of sectors transferred.

	return result; //Give the result: we've overflown the max sector number!
}

OPTINLINE void FLOPPY_dataReady() //Data transfer ready to transfer!
{
	if (FLOPPY.DriveData[FLOPPY.DOR.DriveNumber].NDM) //Interrupt for each byte transferred?
	{
		FLOPPY_raiseIRQ(); //Raise the floppy IRQ: We have data to transfer!
	}
}

OPTINLINE void FLOPPY_startData() //Start a Data transfer if needed!
{
	FLOPPY.databufferposition = 0; //Start with the new buffer!
	if (FLOPPY.commandstep != 2) //Entering data phase?
	{
		FLOPPY_LOGD("FLOPPY: Start transfer of data...")
	}
	FLOPPY.commandstep = 2; //Move to data phrase!
	if (FLOPPY.DOR.Mode) //DMA mode?
	{
		FLOPPY.DMAPending = 1; //Pending DMA!
	}
	FLOPPY_dataReady(); //We have data to transfer!
}

OPTINLINE void floppy_readsector() //Request a read sector command!
{
	char *DSKImageFile = NULL; //DSK image file to use?
	uint_32 sector;
	SECTORINFORMATIONBLOCK sectorinformation; //Information about the sector!

	FLOPPY.MT = (FLOPPY.commandbuffer[0] >> 7); //Multiple track mode?

	FLOPPY.databuffersize = translateSectorSize(FLOPPY.commandbuffer[5]); //Sector size into data buffer!
	if (!FLOPPY.commandbuffer[5]) //Special case? Use given info!
	{
		FLOPPY.databuffersize = FLOPPY.commandbuffer[8]; //Use data length!
	}
	FLOPPY.disk_startpos = floppy_LBA(FLOPPY.DOR.DriveNumber, FLOPPY.currenthead[FLOPPY.DOR.DriveNumber], FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber], FLOPPY.currentsector[FLOPPY.DOR.DriveNumber]); //The start position, in sectors!
	sector = (uint_32)FLOPPY.disk_startpos; //Our sector!
	FLOPPY_LOG("FLOPPY: Read sector #%i", FLOPPY.disk_startpos) //We're reading this sector!
	if (FLOPPY.commandstep != 2) { FLOPPY_LOGD("FLOPPY: Sector size: %i bytes", FLOPPY.databuffersize) }
	FLOPPY.disk_startpos *= FLOPPY.databuffersize; //Calculate the start sector!
	if (FLOPPY.commandstep != 2) { FLOPPY_LOGD("FLOPPY: Requesting transfer for %i bytes.", FLOPPY.databuffersize) } //Transfer this many sectors!

	if (!(FLOPPY.DOR.MotorControl&(1 << FLOPPY.DOR.DriveNumber))) //Not motor ON?
	{
		FLOPPY_LOG("FLOPPY: Error: drive motor not ON!")
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;
	}

	FLOPPY.ST0.UnitSelect = FLOPPY.DOR.DriveNumber; //Current unit!
	FLOPPY.ST0.CurrentHead = (FLOPPY.commandbuffer[2] & 1); //Current head!
	FLOPPY.ST0.NotReady = 1; //We're not ready yet!
	FLOPPY.ST0.UnitCheck = FLOPPY.ST0.SeekEnd = FLOPPY.ST0.InterruptCode = 0; //Clear unit check and Interrupt code: we're OK. Also clear SE flag: we're still busy!
	if (readdata(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0, &FLOPPY.databuffer, FLOPPY.disk_startpos, FLOPPY.databuffersize)) //Read the data into memory?
	{
		FLOPPY.ST0.SeekEnd = 1; //Successfull read with implicit seek!
		FLOPPY_startData();
	}
	else //DSK or error?
	{
		if (DSKImageFile = getDSKimage(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0)) //Are we a DSK image file?
		{
			if (readDSKSectorData(DSKImageFile,FLOPPY.currenthead[FLOPPY.DOR.DriveNumber], FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber], FLOPPY.currentsector[FLOPPY.DOR.DriveNumber], FLOPPY.commandbuffer[5], &FLOPPY.databuffersize)) //Read the data into memory?
			{
				if (readDSKSectorInfo(DSKImageFile, FLOPPY.commandbuffer[3], FLOPPY.commandbuffer[2], FLOPPY.commandbuffer[4], &sectorinformation)) //Read the sector information too!
				{
					FLOPPY.ST1.data = sectorinformation.ST1; //Load ST1!
					FLOPPY.ST2.data = sectorinformation.ST2; //Load ST2!
				}
				FLOPPY_startData();
				return; //Just execute it!
			}
		}
		//Plain error!
		FLOPPY.ST0.data = 0x80; //Invalid command!
		FLOPPY.commandstep = 0xFF; //Error!
	}
}

OPTINLINE void FLOPPY_formatsector() //Request a read sector command!
{
	char *DSKImageFile;
	SECTORINFORMATIONBLOCK sectorinfo;
	++FLOPPY.sectorstransferred; //A sector has been transferred!
	if (drivereadonly(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0)) //Read only drive?
	{
		FLOPPY_LOG("FLOPPY: Finished transfer of data (%i sector(s)).", FLOPPY.sectorstransferred) //Log the completion of the sectors written!
		FLOPPY.resultposition = 0;
		FLOPPY.resultbuffer[0] = FLOPPY.ST0.data;
		FLOPPY.resultbuffer[1] = FLOPPY.ST1.data;
		FLOPPY.resultbuffer[2] = FLOPPY.ST2.data;
		FLOPPY.resultbuffer[3] = FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber];
		FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY.DOR.DriveNumber];
		FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY.DOR.DriveNumber];
		FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[2]; //Sector size from the command buffer!
		FLOPPY.commandstep = 3; //Move to result phrase and give the result!
		FLOPPY_raiseIRQ(); //Entering result phase!
		return; //Abort!
	}
	else //Writeable disk?
	{
		//Check normal error conditions that applies to all disk images!
		if (FLOPPY.databuffer[0] != FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber]) //Not current track?
		{
			FLOPPY.ST0.data = 0x80; //Invalid command!
			FLOPPY.commandstep = 0xFF; //Error!
			return; //Error!
		}
		if (FLOPPY.databuffer[1] != FLOPPY.currenthead[FLOPPY.DOR.DriveNumber]) //Not current head?
		{
			FLOPPY.ST0.data = 0x80; //Invalid command!
			FLOPPY.commandstep = 0xFF; //Error!
			return; //Error!
		}
		if (FLOPPY.databuffer[2] != FLOPPY.currentsector[FLOPPY.DOR.DriveNumber]) //Not current sector?
		{
			FLOPPY.ST0.data = 0x80; //Invalid command!
			FLOPPY.commandstep = 0xFF; //Error!
			return; //Error!
		}

		//Check disk specific information!
		if (DSKImageFile = getDSKimage(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0)) //Are we a DSK image file?
		{
			if (!readDSKSectorInfo(DSKImageFile, FLOPPY.databuffer[1], FLOPPY.databuffer[0], FLOPPY.databuffer[2], &sectorinfo)) //Failed to read sector information block?
			{
				FLOPPY.ST0.data = 0x80; //Invalid command!
				FLOPPY.commandstep = 0xFF; //Error!
				return; //Error!
			}

			//Verify sector size!
			if (sectorinfo.SectorSize != FLOPPY.databuffer[3]) //Invalid sector size?
			{
				FLOPPY.ST0.data = 0x80; //Invalid command!
				FLOPPY.commandstep = 0xFF; //Error!
				return; //Error!
			}

			//Fill the sector buffer and write it!
			memset(FLOPPY.databuffer, FLOPPY.commandbuffer[5], (1 << sectorinfo.SectorSize)); //Clear our buffer with the fill byte!
			if (!writeDSKSectorData(DSKImageFile, FLOPPY.currenthead[FLOPPY.DOR.DriveNumber], FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber], FLOPPY.currentsector[FLOPPY.DOR.DriveNumber], sectorinfo.SectorSize, &FLOPPY.databuffer)) //Failed writing the formatted sector?
			{
				FLOPPY.ST0.data = 0x80; //Invalid command!
				FLOPPY.commandstep = 0xFF; //Error!
				return; //Error!
			}
		}
		else //Are we a normal image file?
		{
			if (FLOPPY.databuffer[3] != 0x2) //Not 512 bytes/sector?
			{
				FLOPPY.ST0.data = 0x80; //Invalid command!
				FLOPPY.commandstep = 0xFF; //Error!
				return; //Error!
			}
			memset(FLOPPY.databuffer, FLOPPY.commandbuffer[5], 512); //Clear our buffer with the fill byte!
			if (!writedata(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0, &FLOPPY.databuffer, floppy_LBA(FLOPPY.DOR.DriveNumber, FLOPPY.currenthead[FLOPPY.DOR.DriveNumber], FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber], FLOPPY.currentsector[FLOPPY.DOR.DriveNumber]),512)) //Failed writing the formatted sector?
			{
				FLOPPY.ST0.data = 0x80; //Invalid command!
				FLOPPY.commandstep = 0xFF; //Error!
				return; //Error!
			}
		}
	}

	FLOPPY.ST0.CurrentHead = FLOPPY.currenthead[FLOPPY.DOR.DriveNumber]; //Our idea of the current head!

	if (++FLOPPY.currentsector[FLOPPY.DOR.DriveNumber] > FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->SPT) //SPT passed? We're finished!
	{
		FLOPPY.currentsector[FLOPPY.DOR.DriveNumber] = 1; //Reset sector number!
		//Enter result phase!
		FLOPPY.resultposition = 0; //Reset result position!
		FLOPPY.commandstep = 3; //Enter the result phase!
		FLOPPY.resultbuffer[0] = FLOPPY.ST0.data;
		FLOPPY.resultbuffer[1] = FLOPPY.ST1.data;
		FLOPPY.resultbuffer[2] = FLOPPY.ST2.data;
		FLOPPY.resultbuffer[3] = FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber];
		FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY.DOR.DriveNumber];
		FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY.DOR.DriveNumber];
		FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[2]; //Sector size from the command buffer!
		return; //Finished!
	}

	//Start transfer of the next sector!
	FLOPPY_startData();
}

OPTINLINE void floppy_writesector() //Request a write sector command!
{
	FLOPPY.MT = (FLOPPY.commandbuffer[0] >> 7); //Multiple track mode?
	FLOPPY.databuffersize = translateSectorSize(FLOPPY.commandbuffer[5]); //Sector size into data buffer!
	if (!FLOPPY.commandbuffer[5]) //Special case? Use given info!
	{
		FLOPPY.databuffersize = FLOPPY.commandbuffer[8]; //Use data length!
	}
	FLOPPY.disk_startpos = floppy_LBA(FLOPPY.DOR.DriveNumber, FLOPPY.commandbuffer[3], FLOPPY.commandbuffer[2], FLOPPY.commandbuffer[4]); //The start position, in sectors!
	if (FLOPPY.commandstep != 2) { FLOPPY_LOG("FLOPPY: Write sector #%i", FLOPPY.disk_startpos) } //We're reading this sector!
	if (FLOPPY.commandstep != 2) { FLOPPY_LOG("FLOPPY: Sector size: %i bytes", FLOPPY.databuffersize) }
	FLOPPY.disk_startpos *= FLOPPY.databuffersize; //Calculate the start sector!
	if (FLOPPY.commandstep != 2) { FLOPPY_LOGD("FLOPPY: Requesting transfer for %i bytes.", FLOPPY.databuffersize) } //Transfer this many sectors!

	if (FLOPPY.commandstep != 2) { FLOPPY_LOG("FLOPPY: Write sector: CHS=%i,%i,%i; Params: %02X%02X%02x%02x%02x%02x%02x%02x", FLOPPY.commandbuffer[3], FLOPPY.commandbuffer[2], FLOPPY.commandbuffer[4], FLOPPY.commandbuffer[1], FLOPPY.commandbuffer[2], FLOPPY.commandbuffer[3], FLOPPY.commandbuffer[4], FLOPPY.commandbuffer[5], FLOPPY.commandbuffer[6], FLOPPY.commandbuffer[7], FLOPPY.commandbuffer[8]) } //Log our request!

	if (!(FLOPPY.DOR.MotorControl&(1 << FLOPPY.DOR.DriveNumber))) //Not motor ON?
	{
		FLOPPY_LOG("FLOPPY: Error: drive motor not ON!")
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;
	}

	FLOPPY.ST0.UnitSelect = FLOPPY.DOR.DriveNumber; //Current unit!
	FLOPPY.ST0.CurrentHead = (FLOPPY.currenthead[FLOPPY.DOR.DriveNumber] & 1); //Current head!
	FLOPPY.ST0.NotReady = 1; //We're not ready yet!
	FLOPPY.ST0.UnitCheck = FLOPPY.ST0.SeekEnd = FLOPPY.ST0.InterruptCode = 0; //Clear unit check and Interrupt code: we're OK. Also clear SE flag: we're still busy!

	if (!(FLOPPY.DOR.MotorControl&(1 << FLOPPY.DOR.DriveNumber))) //Not motor ON?
	{
		FLOPPY.ST0.data = ((FLOPPY.ST0.data & 0x3B) | 1) | ((FLOPPY.commandbuffer[3] & 1) << 2); //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;
	}

	FLOPPY_startData(); //Start the DMA transfer if needed!
}

OPTINLINE void floppy_executeData() //Execute a floppy command. Data is fully filled!
{
	char *DSKImageFile = NULL; //DSK image file to use?
	switch (FLOPPY.commandbuffer[0] & 0xF) //What command!
	{
		case 0x5: //Write sector
		case 0x9: //Write deleted sector
			//Write sector to disk!
			updateFloppyWriteProtected(1); //Try to write with(out) protection!
			if (FLOPPY.databufferposition == FLOPPY.databuffersize) //Fully buffered?
			{
				if (writedata(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0, &FLOPPY.databuffer, FLOPPY.disk_startpos, FLOPPY.databuffersize)) //Written the data to disk?
				{
					switch (floppy_increasesector(FLOPPY.DOR.DriveNumber)) //Goto next sector!
					{
					case 1: //OK?
						//More to be written?
						floppy_writesector(); //Write another sector!
						return; //Finished!
					case 2: //Error during transfer?
						//Let the floppy_increasesector determine the error!
						break;
					case 0: //OK?
					default: //Unknown?
						FLOPPY.ST0.SeekEnd = 1; //Successfull write with implicit seek!
						FLOPPY.ST0.InterruptCode = 0; //Normal termination!
						FLOPPY.ST0.NotReady = 0; //We're ready!
						break;
					}
					FLOPPY_LOG("FLOPPY: Finished transfer of data (%i sector(s)).", FLOPPY.sectorstransferred) //Log the completion of the sectors written!
					FLOPPY.resultposition = 0;
					FLOPPY.resultbuffer[0] = FLOPPY.ST0.data; //ST0!
					FLOPPY.resultbuffer[1] = FLOPPY.ST1.data; //ST1!
					FLOPPY.resultbuffer[2] = FLOPPY.ST2.data; //ST2!
					FLOPPY.resultbuffer[3] = FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber];
					FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY.DOR.DriveNumber];
					FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY.DOR.DriveNumber];
					FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[5]; //Sector size from the command buffer!
					FLOPPY.commandstep = 3; //Move to result phrase and give the result!
					FLOPPY_raiseIRQ(); //Entering result phase!
				}
				else
				{
					if (drivereadonly(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0)) //Read-only drive?
					{
						FLOPPY_LOG("FLOPPY: Finished transfer of data (readonly).") //Log the completion of the sectors written!
						FLOPPY.resultposition = 0;
						FLOPPY.resultbuffer[0] = FLOPPY.ST0.data = ((FLOPPY.ST0.data & 0x3B) | 1) | ((FLOPPY.commandbuffer[3] & 1) << 2); //Abnormal termination! ST0!
						FLOPPY.resultbuffer[1] = FLOPPY.ST1.data; //Drive write-protected! ST1!
						FLOPPY.resultbuffer[2] = FLOPPY.ST2.data = 0x00; //ST2!
						FLOPPY.resultbuffer[3] = FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber];
						FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY.DOR.DriveNumber];
						FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY.DOR.DriveNumber];
						FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[5]; //Sector size!
						FLOPPY.commandstep = 3; //Move to result phase!
						FLOPPY_raiseIRQ(); //Entering result phase!
					}
					else //DSK or error?
					{
						if (DSKImageFile = getDSKimage(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0)) //Are we a DSK image file?
						{
							if (writeDSKSectorData(DSKImageFile, FLOPPY.commandbuffer[3], FLOPPY.commandbuffer[2], FLOPPY.commandbuffer[4], FLOPPY.commandbuffer[5], &FLOPPY.databuffersize)) //Read the data into memory?
							{
								switch (floppy_increasesector(FLOPPY.DOR.DriveNumber)) //Goto next sector!
								{
								case 1: //OK?
									//More to be written?
									floppy_writesector(); //Write another sector!
									return; //Finished!
								case 2: //Error during transfer?
										//Let the floppy_increasesector determine the error!
									break;
								case 0: //OK?
								default: //Unknown?
									FLOPPY.ST0.SeekEnd = 1; //Successfull write with implicit seek!
									FLOPPY.ST0.InterruptCode = 0; //Normal termination!
									FLOPPY.ST0.NotReady = 0; //We're ready!
									break;
								}
								FLOPPY_LOG("FLOPPY: Finished transfer of data (%i sector(s)).", FLOPPY.sectorstransferred) //Log the completion of the sectors written!
								FLOPPY.ST0.SeekEnd = 1; //Successfull write with implicit seek!
								FLOPPY.resultposition = 0;
								FLOPPY.resultbuffer[0] = FLOPPY.ST0.data = ((FLOPPY.ST0.data & 0x3B) | 1) | ((FLOPPY.commandbuffer[3] & 1) << 2); //Abnormal termination! ST0!
								FLOPPY.resultbuffer[1] = FLOPPY.ST1.data; //Drive write-protected! ST1!
								FLOPPY.resultbuffer[2] = FLOPPY.ST2.data = 0x00; //ST2!
								FLOPPY.resultbuffer[3] = FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber];
								FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY.DOR.DriveNumber];
								FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY.DOR.DriveNumber];
								FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[5]; //Sector size!
								FLOPPY.commandstep = 3; //Move to result phase!
								FLOPPY_raiseIRQ(); //Entering result phase!
								return;
							}
						}
						//Plain error!
						FLOPPY.ST0.data = 0x80; //Invalid command!
						FLOPPY.commandstep = 0xFF; //Error!
					}
				}
			}
			else //Unfinished buffer? Terminate!
			{
				FLOPPY.resultposition = 0;
				FLOPPY.resultbuffer[0] = FLOPPY.ST0.data = ((FLOPPY.ST0.data & 0x3B) | 1) | ((FLOPPY.commandbuffer[3] & 1) << 2); //Abnormal termination! ST0!
				FLOPPY.resultbuffer[1] = FLOPPY.ST1.data; //Drive write-protected! ST1!
				FLOPPY.resultbuffer[2] = FLOPPY.ST2.data; //ST2!
				FLOPPY.resultbuffer[3] = FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber];
				FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY.DOR.DriveNumber];
				FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY.DOR.DriveNumber];
				FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[5]; //Sector size from the command buffer!
				FLOPPY.commandstep = 3; //Move to result phase!
				FLOPPY_raiseIRQ(); //Entering result phase!
			}
			break;
		case 0x2: //Read complete track
		case 0x6: //Read sector
		case 0xC: //Read deleted sector
			//We've finished reading the read data!
			updateFloppyWriteProtected(0); //Try to read with(out) protection!
			if (FLOPPY.databufferposition == FLOPPY.databuffersize) //Fully processed?
			{
				switch (floppy_increasesector(FLOPPY.DOR.DriveNumber)) //Goto next sector!
				{
				case 1: //Read more?
					//More to be written?
					floppy_readsector(); //Read another sector!
					return; //Finished!
				case 2: //Error during transfer?
						//Let the floppy_increasesector determine the error!
					break;
				case 0: //OK?
				default: //Unknown?
					FLOPPY.ST0.SeekEnd = 1; //Successfull write with implicit seek!
					FLOPPY.ST0.InterruptCode = 0; //Normal termination!
					FLOPPY.ST0.NotReady = 0; //We're ready!
					break;
				}
				FLOPPY.resultposition = 0;
				FLOPPY.resultbuffer[0] = FLOPPY.ST0.data;
				FLOPPY.resultbuffer[1] = FLOPPY.ST1.data;
				FLOPPY.resultbuffer[2] = FLOPPY.ST2.data;
				FLOPPY.resultbuffer[3] = FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber];
				FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY.DOR.DriveNumber];
				FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY.DOR.DriveNumber];
				FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[2]; //Sector size from the command buffer!
				FLOPPY.commandstep = 3; //Move to result phrase and give the result!
				FLOPPY_LOG("FLOPPY: Finished transfer of data (%i sectors).", FLOPPY.sectorstransferred) //Log the completion of the sectors written!
				FLOPPY_raiseIRQ(); //Entering result phase!
			}
			else //Unfinished buffer? Terminate!
			{
				FLOPPY.resultposition = 0;
				FLOPPY.resultbuffer[0] = FLOPPY.ST0.data = ((FLOPPY.ST0.data & 0x3B) | 1) | ((FLOPPY.commandbuffer[3] & 1) << 2); //Abnormal termination! ST0!
				FLOPPY.resultbuffer[1] = FLOPPY.ST1.data; //Drive write-protected! ST1!
				FLOPPY.resultbuffer[2] = FLOPPY.ST2.data; //ST2!
				FLOPPY.resultbuffer[3] = FLOPPY.commandbuffer[2]; //Cylinder!
				FLOPPY.resultbuffer[4] = FLOPPY.commandbuffer[3]; //Head!
				FLOPPY.resultbuffer[5] = FLOPPY.commandbuffer[4]; //Sector!
				FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[5]; //Sector size!
				FLOPPY.commandstep = 3; //Move to result phase!
				FLOPPY_raiseIRQ(); //Entering result phase!
			}
			break;
		case 0xD: //Format sector
			updateFloppyWriteProtected(1); //Try to write with(out) protection!
			FLOPPY_formatsector(); //Execute a format sector command!
			break;
		default: //Unknown command?
			FLOPPY.commandstep = 0xFF; //Move to error phrase!
			FLOPPY.ST0.data = 0x80; //Invalid command!
			break;
	}
}

OPTINLINE void floppy_executeCommand() //Execute a floppy command. Buffers are fully filled!
{
	char *DSKImageFile = NULL; //DSK image file to use?
	SECTORINFORMATIONBLOCK sectorinformation; //Information about the sector!
	FLOPPY.TC = 0; //Reset TC flag!
	FLOPPY.resultposition = 0; //Default: start of the result!
	FLOPPY.databuffersize = 0; //Default: nothing to write/read!
	if (FLOPPY.DOR.DriveNumber & 2) goto invaliddrive;
	FLOPPY_LOGD("FLOPPY: executing command: %02X", FLOPPY.commandbuffer[0]) //Executing this command!
	switch (FLOPPY.commandbuffer[0] & 0xF) //What cFommand!
	{
		case 0x5: //Write sector
			FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber] = FLOPPY.commandbuffer[2]; //Current cylinder!
			FLOPPY.currenthead[FLOPPY.DOR.DriveNumber] = FLOPPY.commandbuffer[3]; //Current head!
			FLOPPY.currentsector[FLOPPY.DOR.DriveNumber] = FLOPPY.commandbuffer[4]; //Current sector!
			floppy_writesector(); //Start writing a sector!
			break;
		case 0x6: //Read sector
			FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber] = FLOPPY.commandbuffer[2]; //Current cylinder!
			FLOPPY.currenthead[FLOPPY.DOR.DriveNumber] = FLOPPY.commandbuffer[3]; //Current head!
			FLOPPY.currentsector[FLOPPY.DOR.DriveNumber] = FLOPPY.commandbuffer[4]; //Current sector!
			floppy_readsector(); //Start reading a sector!
			break;
		case 0x3: //Fix drive data/specify command
			FLOPPY.DriveData[FLOPPY.DOR.DriveNumber].data[0] = FLOPPY.databuffer[0]; //Set setting byte 1/2!
			FLOPPY.DriveData[FLOPPY.DOR.DriveNumber].data[1] = FLOPPY.databuffer[1]; //Set setting byte 2/2!
			FLOPPY.commandstep = 0; //Reset controller command status!
			FLOPPY.ST0.data = 0x00; //Correct command!
			updateFloppyWriteProtected(0); //Try to read with(out) protection!
			break;
		case 0x7: //Calibrate drive
			//Execute interrupt!
			FLOPPY.commandstep = 0; //Reset controller command status!
			FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber] = 0; //Goto cylinder #0!
			FLOPPY.ST0.data = 0x20; //Completed command!
			updateFloppyWriteProtected(0); //Try to read with(out) protection!
			FLOPPY_raiseIRQ(); //We're finished!
			break;
		case 0x8: //Check interrupt status
			//Set result
			updateFloppyWriteProtected(0); //Try to read with(out) protection!
			FLOPPY.commandstep = 3; //Move to result phrase!
			//Reset IRQ line!
			if (FLOPPY.reset_pending) //Reset is pending?
			{
				byte reset_drive = 4 - (FLOPPY.reset_pending--); //We're pending this drive!
				FLOPPY.ST0.data &= 0xF8; //Clear low 3 bits!
				FLOPPY.ST0.UnitSelect = reset_drive; //What drive are we giving!
				FLOPPY.ST0.CurrentHead = (FLOPPY.currenthead[reset_drive] & 1); //Set the current head of the drive!
				FLOPPY.resultbuffer[1] = FLOPPY.currentcylinder[reset_drive]; //Our idea of the current cylinder!
				if (!FLOPPY.reset_pending) //Finished reset?
				{
					FLOPPY.ST0.data = 0x00; //Reset the ST0 register after full reset!
				}
			}
			else if (!FLOPPY.IRQPending) //Not an pending IRQ?
			{
				FLOPPY_LOGD("FLOPPY: Warning: Checking interrupt status without IRQ pending!")
				FLOPPY.ST0.data = 0x80; //Error!
				FLOPPY.resultbuffer[1] = FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber]; //Our idea of the current cylinder!
			}
			else
			{
				FLOPPY.resultbuffer[1] = FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber]; //Our idea of the current cylinder!
			}
			FLOPPY_LOG("FLOPPY: Sense interrupt: ST0=%02X, Currentcylinder=%02X", FLOPPY.ST0.data, FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber])
			FLOPPY.resultbuffer[0] = FLOPPY.ST0.data; //Give ST0!
			FLOPPY.resultposition = 0; //Start result!
			FLOPPY.commandstep = 3; //Result phase!
			break;
		case 0xF: //Seek/park head
			FLOPPY.commandstep = 0; //Reset controller command status!
			updateFloppyWriteProtected(0); //Try to read with(out) protection!
			if (FLOPPY.DOR.DriveNumber >= 2) //Invalid drive?
			{
				FLOPPY.ST0.data = (FLOPPY.ST0.data & 0x32) | 0x14 | FLOPPY.DOR.DriveNumber; //Error: drive not ready!
				FLOPPY.commandstep = 0; //Reset command!
				return; //Abort!
			}
			if (!has_drive(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0)) //Floppy not inserted?
			{
				FLOPPY.ST0.data = (FLOPPY.ST0.data & 0x30) | 0x18 | FLOPPY.DOR.DriveNumber; //Error: drive not ready!
				FLOPPY.commandstep = 0; //Reset command!
				return; //Abort!
			}
			if (FLOPPY.commandbuffer[2] < floppy_tracks(disksize(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0))) //Valid track?
			{
				FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber] = FLOPPY.commandbuffer[2]; //Set the current cylinder!
				FLOPPY.ST0.data = (FLOPPY.ST0.data & 0x30) | 0x20 | FLOPPY.DOR.DriveNumber; //Valid command!
				FLOPPY_raiseIRQ(); //Finished executing phase!
				return; //Give an error!
			}

			//Invalid track?
			FLOPPY.ST2.data |= 0x4; //Invalid seek!
			FLOPPY.commandstep = (byte)(FLOPPY.commandposition = 0); //Reset command!
			break;
		case 0x4: //Check drive status
			FLOPPY.resultbuffer[0] = FLOPPY.ST3.data; //Give ST3!
			FLOPPY.resultposition = 0; //Start the result!
			FLOPPY.commandstep = 3; //Result phase!
			break;
		case 0xA: //Read sector ID
			if (DSKImageFile = getDSKimage(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0)) //Are we a DSK image file?
			{
				if (readDSKSectorInfo(DSKImageFile, FLOPPY.currenthead[FLOPPY.DOR.DriveNumber], FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber], FLOPPY.currentsector[FLOPPY.DOR.DriveNumber], &sectorinformation)) //Read the sector information too!
				{
					FLOPPY.ST1.data = sectorinformation.ST1; //Load ST1!
					FLOPPY.ST2.data = sectorinformation.ST2; //Load ST2!
					FLOPPY.resultbuffer[6] = sectorinformation.SectorSize; //Sector size!
				}
				else
				{
					FLOPPY.ST1.data = 0x00; //Not found!
					FLOPPY.ST2.data = 0x00; //Not found!
					FLOPPY.ST1.NoAddressMark = FLOPPY.ST1.NoData = 1; //We cannot find the ID!
					FLOPPY.resultbuffer[6] = 0; //Unknown sector size!
				}
			}
			else //Normal disk? Generate valid data!
			{
				FLOPPY.ST1.data = 0x00; //Clear ST1!
				FLOPPY.ST2.data = 0x00; //Clear ST2!
				updateFloppyTrack0(); //Update track 0!
				updateFloppyWriteProtected(0); //Update write protected related flags!
				if (floppy_LBA(FLOPPY.DOR.DriveNumber, FLOPPY.currenthead[FLOPPY.DOR.DriveNumber], FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber], FLOPPY.currentsector[FLOPPY.DOR.DriveNumber]) >= (FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->KB * 1024)) //Invalid address within our image!
				{
					FLOPPY.ST1.NoAddressMark = FLOPPY.ST1.NoData = 1; //Invalid sector!
				}
				FLOPPY.resultbuffer[6] = 2; //Always 512 byte sectors!
			}
			FLOPPY.resultposition = 0; //Start the result!
			FLOPPY.resultbuffer[0] = FLOPPY.ST0.data; //ST0!
			FLOPPY.resultbuffer[1] = FLOPPY.ST1.data; //ST1!
			FLOPPY.resultbuffer[2] = FLOPPY.ST2.data; //ST2!
			FLOPPY.resultbuffer[3] = FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber]; //Cylinder!
			FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY.DOR.DriveNumber]; //Head!
			FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY.DOR.DriveNumber]; //Sector!
			break;
		case 0xD: //Format sector
			updateFloppyGeometries(FLOPPY.DOR.DriveNumber, FLOPPY.currenthead[FLOPPY.DOR.DriveNumber], FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber]); //Update the floppy geometries!
			if (!(FLOPPY.DOR.MotorControl&(1 << FLOPPY.DOR.DriveNumber))) //Not motor ON?
			{
				FLOPPY_LOG("FLOPPY: Error: drive motor not ON!")
				FLOPPY.commandstep = 0xFF; //Move to error phase!
				return;
			}

			if (!FLOPPY.geometries[FLOPPY.DOR.DriveNumber]) //No geometry?
			{
				FLOPPY.ST0.data = 0x80; //Invalid command!
				FLOPPY.commandstep = 0xFF; //Error!
			}

			if (FLOPPY.commandbuffer[3] != FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->SPT) //Invalid SPT?
			{
				FLOPPY.ST0.data = 0x80; //Invalid command!
				FLOPPY.commandstep = 0xFF; //Error!
			}

			if (DSKImageFile = getDSKimage(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0)) //Are we a DSK image file?
			{
				FLOPPY.databuffersize = 4; //We're 4 bytes per sector!
				FLOPPY_startData(); //Start the data transfer!
			}
			else //Normal standard emulated sector?
			{
				if (FLOPPY.commandbuffer[2] != 0x2) //Not 512 bytes/sector?
				{
					FLOPPY.ST0.data = 0x80; //Invalid command!
					FLOPPY.commandstep = 0xFF; //Error!
				}
				else
				{
					FLOPPY.databuffersize = 4; //We're 4 bytes per sector!
					FLOPPY_startData(); //Start the data transfer!
				}
			}
			break;
		case 0x2: //Read complete track!
		case 0x9: //Write deleted sector
		case 0xC: //Read deleted sector
			invaliddrive: //Invalid drive detected?
			FLOPPY.commandstep = 0xFF; //Move to error phrase!
			FLOPPY.ST0.data = 0x80; //Invalid command!
			break;
	}
}

OPTINLINE void floppy_abnormalpolling()
{
	FLOPPY.ST0.InterruptCode = 3; //Abnormal termination by polling!
	FLOPPY.ST0.NotReady = 1; //We became not ready!
	FLOPPY.commandstep = 0xFF; //Error!
}

OPTINLINE void floppy_writeData(byte value)
{
	byte commandlength[0x10] = {
		0, //0
		0, //1
		8, //2
		2, //3
		1, //4
		8, //5
		8, //6
		1, //7
		0, //8
		8, //9
		1, //A
		0, //B
		8, //C
		5, //D
		0, //E
		2 //F
		};
	//TODO: handle floppy writes!
	switch (FLOPPY.commandstep) //What step are we at?
	{
		case 0: //Command
			FLOPPY.commandstep = 1; //Start inserting parameters!
			FLOPPY.commandposition = 1; //Start at position 1 with out parameters/data!
			FLOPPY_LOGD("FLOPPY: Command byte sent: %02X", value) //Log our information about the command byte!
			switch (value) //What command?
			{
				case 0x8: //Check interrupt status
					FLOPPY.commandbuffer[0] = value; //Set the command to use!
					floppy_executeCommand(); //Execute the command!
					break;
				case 0x2: //Read complete track
				case 0x22: //same
				case 0x42: //same
				case 0x62: //same
				case 0x5: //Write sector
				case 0x45: //same
				case 0x85: //same
				case 0xC5: //same
				case 0x6: //Read sector
				case 0x26: //same
				case 0x46: //same
				case 0x66: //same
				case 0x86: //same
				case 0xA6: //same
				case 0xC6: //same
				case 0xE6: //same
				case 0x3: //Fix drive data
				case 0x4: //Check drive status
				case 0x7: //Calibrate drive
				case 0xF: //Seek/park head
				case 0xA: //Read sector ID
				case 0xD: //Format sector
					FLOPPY.commandbuffer[0] = value; //Set the command to use!
					break;
				case 0x9: //Write deleted sector
				case 0xC: //Read deleted sector
				default: //Invalid command
					FLOPPY.ST0.data = 0x80; //Invalid command!
					FLOPPY.commandstep = 0xFF; //Error!
					break;
			}
			break;
		case 1: //Parameters
			FLOPPY_LOGD("FLOPPY: Parameter sent: %02X(#%i/%i)", value, FLOPPY.commandposition, commandlength[FLOPPY.commandbuffer[0] & 0xF]) //Log the parameter!
			FLOPPY.commandbuffer[FLOPPY.commandposition++] = value; //Set the command to use!
			if (FLOPPY.commandposition > (commandlength[FLOPPY.commandbuffer[0] & 0xF])) //All parameters have been processed?
			{
				floppy_executeCommand(); //Execute!
				break;
			}
			break;
		case 2: //Data
			switch (FLOPPY.commandbuffer[0]&0xF) //What command?
			{
				case 0x5: //Write sector
				case 0x9: //Write deleted sector
				case 0xD: //Format track
					FLOPPY.databuffer[FLOPPY.databufferposition++] = value; //Set the command to use!
					if (FLOPPY.databufferposition==FLOPPY.databuffersize) //Finished?
					{
						floppy_executeData(); //Execute the command with the given data!
					}
					else if (FLOPPY.DOR.Mode) //DMA mode and not completed?
					{
						FLOPPY_dataReady(); //We have data ready to transfer!
						if (FLOPPY.TC) //Terminal count? We're ending too soon!
						{
							floppy_executeData(); //Execute the command with the given data!
						}
					}
					else //Non-DMA mode and not completed?
					{
						FLOPPY_dataReady(); //We have data ready to transfer!
					}
					break;
				default: //Invalid command
					floppy_abnormalpolling(); //Abnormal polling!
					break;
			}
			break;
		case 3: //Result
			floppy_abnormalpolling();
			break; //We don't write during the result phrase!
		case 0xFF: //Error
			floppy_abnormalpolling();
			//We can't do anything! Ignore any writes now!
			break;
		default:
			break; //Unknown status, hang the controller or do nothing!
	}
}

OPTINLINE byte floppy_readData()
{
	byte resultlength[0x10] = {
		0, //0
		0, //1
		7, //2
		0, //3
		1, //4
		7, //5
		7, //6
		0, //7
		2, //8
		7, //9
		7, //a
		0, //b
		7, //c
		7, //d
		0, //e
		0 //f
		};
	byte temp;
	switch (FLOPPY.commandstep) //What step are we at?
	{
		case 0: //Command
			floppy_abnormalpolling(); //Abnormal polling!
			break; //Nothing to read during command phrase!
		case 1: //Parameters
			floppy_abnormalpolling(); //Abnormal polling!
			break; //Nothing to read during parameter phrase!
		case 2: //Data
			switch (FLOPPY.commandbuffer[0]&0xF) //What command?
			{
				case 0x2: //Read complete track
				case 0x6: //Read sector
				case 0xC: //Read deleted sector
					temp = FLOPPY.databuffer[FLOPPY.databufferposition++]; //Read data!
					if (FLOPPY.databufferposition==FLOPPY.databuffersize) //Finished?
					{
						floppy_executeData(); //Execute the data finished phrase!
					}
					else if (FLOPPY.DOR.Mode) //DMA mode and not completed?
					{
						FLOPPY_dataReady(); //We have data ready to transfer!
						if (FLOPPY.TC) //Terminal count? We're ending too soon!
						{
							floppy_executeData(); //Execute the command with the given data!
						}
					}
					else //Non-DMA mode and not completed?
					{
						FLOPPY_dataReady(); //We have data ready to transfer!
					}
					return temp; //Give the result!
					break;
				default: //Invalid command: we have no data to be READ!
					floppy_abnormalpolling(); //Abnormal polling!
					break;
			}
			break;
		case 3: //Result
			temp = FLOPPY.resultbuffer[FLOPPY.resultposition++]; //Read a result byte!
			switch (FLOPPY.commandbuffer[0]&0xF) //What command?
			{
				case 0x2: //Read complete track
				case 0x5: //Write sector
				case 0x6: //Read sector
				case 0x9: //Write deleted sector
				case 0xC: //Read deleted sector
				case 0xD: //Format sector
				case 0x3: //Fix drive data
				case 0x4: //Check drive status
				case 0x7: //Calibrate drive
				case 0x8: //Check interrupt status
				case 0xA: //Read sector ID
				case 0xF: //Seek/park head
					FLOPPY_LOGD("FLOPPY: Reading result byte %i/%i",FLOPPY.resultposition,resultlength[FLOPPY.commandbuffer[0]&0xF])
					if (FLOPPY.resultposition>=resultlength[FLOPPY.commandbuffer[0]&0xF]) //Result finished?
					{
						FLOPPY.commandstep = 0; //Reset step!
					}
					return temp; //Give result value!
					break;
				default: //Invalid command to read!
					floppy_abnormalpolling(); //Abnormal polling!
					break;
			}
			break;
		case 0xFF: //Error
			FLOPPY.commandstep = 0; //Reset step!
			return FLOPPY.ST0.data; //Give ST0, containing an error!
			break;
		default:
			break; //Unknown status, hang the controller!
	}
	return ~0; //Not used yet!
}

byte PORT_IN_floppy(word port, byte *result)
{
	if ((port&~7) != 0x3F0) return 0; //Not our port range!
	byte temp;
	switch (port & 0x7) //What port?
	{
	case 0: //SRA?
		temp = 0;
		if (FLOPPY.IRQPending) temp |= 0x80; //Pending interrupt!
		*result = temp; //Give the result!
		return 1; //Not used!
	case 1: //SRB?
		*result = 0;
		return 1; //Not used!
	case 4: //MSR?
		updateFloppyMSR(); //Update the MSR with current values!
		*result = FLOPPY.MSR.data; //Give MSR!
		return 1;
	case 5: //Data?
		//Process data!
		*result = floppy_readData(); //Read data!
		return 1;
	case 7: //DIR?
		updateFloppyDIR(); //Update the DIR register!
		*result = FLOPPY.DIR.data; //Give DIR!
		return 1;
	default: //Unknown port?
		break;
	}
	return 0; //Unknown port!
}

OPTINLINE void updateMotorControl()
{
	EMU_setDiskBusy(FLOPPY0, FLOPPY.DOR.MotorControl & 1); //Are we busy?
	EMU_setDiskBusy(FLOPPY1, (FLOPPY.DOR.MotorControl & 2) >> 1); //Are we busy?
}

byte PORT_OUT_floppy(word port, byte value)
{
	if ((port&~7) != 0x3F0) return 0; //Not our address range!
	switch (port & 0x7) //What port?
	{
	case 2: //DOR?
		FLOPPY.DOR.data = value; //Write to register!
		updateMotorControl(); //Update the motor control!
		if (!FLOPPY.DOR.REST) //Reset requested?
		{
			FLOPPY.DOR.REST = 1; //We're finished resetting!
			FLOPPY_reset(); //Execute a reset!
		}
		return 1; //Finished!
	case 4: //DSR?
		if (value & 0x80) //Reset requested?
		{
			value &= 0x7F; //Clear the reset bit automatically!
			FLOPPY_reset(); //Execute a reset!
		}
		FLOPPY.DSR.data = value; //Write to register!
		return 1; //Finished!
	case 5: //Data?
		floppy_writeData(value); //Write data!
		return 1; //Default handler!
	case 7: //CCR?
		FLOPPY.CCR.data = value; //Set CCR!
		return 1;
	default: //Unknown port?
		break; //Unknown port!
	}
	return 0; //Unknown port!
}

//DMA logic

void DMA_floppywrite(byte data)
{
	floppy_writeData(data); //Send the data to the FDC!
}

byte DMA_floppyread()
{
	return floppy_readData(); //Read data!
}

void FLOPPY_DMADREQ() //For checking any new DREQ signals!
{
	DMA_SetDREQ(FLOPPY_DMA, (FLOPPY.commandstep == 2) && FLOPPY.DOR.Mode); //Set DREQ from hardware when in the data phase and using DMA transfers and not busy yet(pending)!
}

void FLOPPY_DMADACK() //For processing DACK signal!
{
	FLOPPY.DMAPending = 0; //We're not pending anymore!
}

void FLOPPY_DMATC() //Terminal count triggered?
{
	FLOPPY.TC = 1; //Terminal count triggered!
}

void initFDC()
{
	memset(&FLOPPY, 0, sizeof(FLOPPY)); //Initialise floppy!
	//Initialise DMA controller settings for the FDC!
	DMA_SetDREQ(FLOPPY_DMA,0); //No DREQ!
	registerDMA8(FLOPPY_DMA, &DMA_floppyread, &DMA_floppywrite); //Register our DMA channels!
	registerDMATick(FLOPPY_DMA, &FLOPPY_DMADREQ, &FLOPPY_DMADACK, &FLOPPY_DMATC); //Our handlers for DREQ, DACK and TC!

	//Set basic I/O ports
	register_PORTIN(&PORT_IN_floppy);
	register_PORTOUT(&PORT_OUT_floppy);
	register_DISKCHANGE(FLOPPY0, &FLOPPY_notifyDiskChanged);
	register_DISKCHANGE(FLOPPY1, &FLOPPY_notifyDiskChanged);
}