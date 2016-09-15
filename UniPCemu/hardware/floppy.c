#include "headers/types.h" //Basic typedefs!
#include "headers/hardware/8237A.h" //DMA controller support!
#include "headers/hardware/ports.h" //Port support!
#include "headers/basicio/io.h" //Basic I/O functionality!
#include "headers/hardware/pic.h" //PIC support!
#include "headers/basicio/dskimage.h" //DSK image support!
#include "headers/cpu/cpu.h" //Emulated CPU support!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/floppy.h" //Our type definitions!
#include "headers/bios/biosrom.h" //ROM support for Turbo XT BIOS detection!
#include "headers/emu/debugger/debugger.h" //For logging extra information when debugging!

//Configuration of the FDC...

//Enable density errors or gap length errors?
#define EMULATE_DENSITY 0
#define EMULATE_GAPLENGTH 0

//Double logging if FLOPPY_LOGFILE2 is defined!
#define FLOPPY_LOGFILE "floppy"
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

//Redirect to direct log always if below is uncommented?
/*
#undef FLOPPY_LOGD
#define FLOPPY_LOGD FLOPPY_LOG
*/

struct
{
	union
	{
		struct
		{
			byte DriveNumber : 2; //What drive to address?
			byte REST : 1; //Enable controller when set!
			byte DMA : 1; //0=IRQ channel, 1=DMA mode
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
		struct
		{
			byte rate : 2; //0=500kbits/s, 1=300kbits/s, 2=250kbits/s, 3=1Mbits/s
			byte unused : 6;
		};
	} CCR; //CCR
	union
	{
		byte data; //DIR data!
		struct
		{
			byte HighDensity : 1; //1 if high density, 0 otherwise.
			byte DataRate : 2; //0=500, 1=300, 2=250, 3=1MBit/s
			byte AlwaysF : 4; //Always 0xF
			byte DiskChange : 1; //1 when disk changed. Executing a command clears this.
		};
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
			byte UnitCheck : 1; //Set with drive fault or cannot find track 0 after 79 pulses!
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
		struct
		{
			byte unused : 1;
			byte DeletedAddressMark : 1;
			byte CRCError : 1;
			byte WrongCyclinder : 1;
			byte SeekEqual : 1;
			byte SeekError : 1;
			byte BadCyclinder : 1;
			byte NoDataAddressMarkDAM : 1;
		};
	} ST2;
	union
	{
		byte data; //ST3 register!
		struct
		{
			byte ErrorSignature : 1;
			byte WriteProtection : 1;
			byte DriveReady : 1;
			byte Track0 : 1;
			byte DoubleSided : 1;
			byte Head1Active : 1;
			byte DriveSelect : 2;
		};
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
	union
	{
		byte data[3]; //All data bytes!
		struct
		{
			byte FirstParameter0; //Set to 0
			struct
			{
				byte Threshold : 4; //Threadhold!
				byte DrivePollingModeDisable : 1; //Disable drive polling mode if set!
				byte FIFODisable : 1; //Disable FIFO if set!
				byte ImpliedSeekEnable : 1; //Enable Implied Seek if set!
				byte unused7 : 1; //Unused 7th bit!
			};
			byte PreComp; //Precompensation value!
		};
	} Configuration; //The data from the Configure command!
	byte Locked; //Are we locked?
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
	byte reset_pending,reset_pended; //Reset pending?
	byte reset_pending_size; //Size of the pending reset max value! A maximum set of 3 with 4 drives reset!
	byte currentcylinder[4], currenthead[4], currentsector[4]; //Current head for all 4 drives!
	byte TC; //Terminal count triggered?
	uint_32 sectorstransferred; //Ammount of sectors transferred!
	byte MT,DoubleDensity,Skip; //MT bit, Double Density bit and Skip bit  as set by the command, if any!
	byte floppy_resetted; //Are we resetted?
	byte ignorecommands; //Locked up by an invalid Sense Interrupt?
} FLOPPY; //Our floppy drive data!

byte density_forced = 0; //Default: don't ignore the density with the CPU!

//Normal floppy specific stuff

#define KB(x) (x/1024)

//Floppy commands from OSDev.
enum FloppyCommands
{
   READ_TRACK =                 2,	// generates IRQ6
   SPECIFY =                    3,      // * set drive parameters
   SENSE_DRIVE_STATUS =         4,
   WRITE_DATA =                 5,      // * write to the disk
   READ_DATA =                  6,      // * read from the disk
   RECALIBRATE =                7,      // * seek to cylinder 0
   SENSE_INTERRUPT =            8,      // * ack IRQ6, get status of last command
   WRITE_DELETED_DATA =         9,
   READ_ID =                    10,	// generates IRQ6
   READ_DELETED_DATA =          12,
   FORMAT_TRACK =               13,     // *
   SEEK =                       15,     // * seek both heads to cylinder X
   VERSION =                    16,	// * used during initialization, once
   SCAN_EQUAL =                 17,
   PERPENDICULAR_MODE =         18,	// * used during initialization, once, maybe
   CONFIGURE =                  19,     // * set controller parameters
   LOCK =                       20,     // * protect controller params from a reset
   VERIFY =                     22,
   SCAN_LOW_OR_EQUAL =          25,
   SCAN_HIGH_OR_EQUAL =         29
};

/*
{  KB,SPT,SIDES,TRACKS,  }
{ 160,  8, 1   , 40   , 0 },
{ 180,  9, 1   , 40   , 0 },
{ 200, 10, 1   , 40   , 0 },
{ 320,  8, 2   , 40   , 1 },
{ 360,  9, 2   , 40   , 1 },
{ 400, 10, 2   , 40   , 1 },
{ 720,  9, 2   , 80   , 3 },
{1200, 15, 2   , 80   , 2 },
{1440, 18, 2   , 80   , 4 },
{2880, 36, 2   , 80   , 6 },

*/

//Allowed transfer rates!
#define TRANSFERRATE_500k 0
#define TRANSFERRATE_300k 1
#define TRANSFERRATE_250k 2
#define TRANSFERRATE_1M 3

//Allowed gap length!
#define GAPLENGTH_IGNORE 0
#define GAPLENGTH_STD 42
#define GAPLENGTH_5_14 32
#define GAPLENGTH_3_5 27

//Flags when executing commands!
#define CMD_EXT_SKIPDELETEDADDRESSMARKS 0x20
#define CMD_EXT_DOUBLEDENSITYMODE 0x40
#define CMD_EXT_MULTITRACKOPERATION 0x80

//Density limits and specification!
#define DENSITY_SINGLE 0
#define DENSITY_DOUBLE 1
#define DENSITY_HD 2
#define DENSITY_ED 4
//Ignore density on specific target BIOS/CPU?
#define DENSITY_IGNORE 8

FLOPPY_GEOMETRY floppygeometries[NUMFLOPPYGEOMETRIES] = { //Differently formatted disks, and their corresponding geometries
	//First, 5"
	{ 160,  8,  1, 40, 0, 0, TRANSFERRATE_250k|(TRANSFERRATE_300k<<2)|(TRANSFERRATE_300k<<4)|(TRANSFERRATE_300k<<6),0xFE,512 , 1, 64 ,DENSITY_SINGLE           ,GAPLENGTH_5_14,0x00   }, //160K 5.25" supports 250kbits, 300kbits SD!
	{ 180,  9,  1, 40, 0, 0, TRANSFERRATE_250k|(TRANSFERRATE_300k<<2)|(TRANSFERRATE_300k<<4)|(TRANSFERRATE_300k<<6),0xFC,512 , 2, 64 ,DENSITY_SINGLE           ,GAPLENGTH_5_14,0x00   }, //180K 5.25" supports 250kbits, 300kbits SD!
	{ 200, 10,  1, 40, 0, 0, TRANSFERRATE_250k|(TRANSFERRATE_300k<<2)|(TRANSFERRATE_300k<<4)|(TRANSFERRATE_300k<<6),0xFC,512 , 2, 64 ,DENSITY_SINGLE           ,GAPLENGTH_5_14,0x00   }, //200K 5.25" supports 250kbits, 300kbits SD!
	{ 320,  8,  2, 40, 0, 0, TRANSFERRATE_250k|(TRANSFERRATE_300k<<2)|(TRANSFERRATE_300k<<4)|(TRANSFERRATE_300k<<6),0xFF,512 , 1, 112,DENSITY_SINGLE           ,GAPLENGTH_5_14,0x00   }, //320K 5.25" supports 250kbits, 300kbits SD!
	{ 360,  9,  2, 40, 0, 0, TRANSFERRATE_250k|(TRANSFERRATE_300k<<2)|(TRANSFERRATE_300k<<4)|(TRANSFERRATE_300k<<6),0xFD,1024, 2, 112,DENSITY_DOUBLE           ,GAPLENGTH_5_14,0x00   }, //360K 5.25" supports 250kbits, 300kbits DD!
	{ 400, 10,  2, 40, 0, 0, TRANSFERRATE_250k|(TRANSFERRATE_300k<<2)|(TRANSFERRATE_300k<<4)|(TRANSFERRATE_300k<<6),0xFD,1024, 2, 112,DENSITY_SINGLE           ,GAPLENGTH_5_14,0x00   }, //400K 5.25" supports 250kbits, 300kbits SD!
	{1200, 15,  2, 80, 0, 0, TRANSFERRATE_250k|(TRANSFERRATE_300k<<2)|(TRANSFERRATE_500k<<4)|(TRANSFERRATE_500k<<6),0xF9,512 , 7, 224,DENSITY_SINGLE           ,GAPLENGTH_5_14,0x00   }, //1200K 5.25" supports 300kbits, 500kbits SD!
	//Now 3.5"
	{ 720,  9,  2, 80, 1, 1, TRANSFERRATE_250k|(TRANSFERRATE_300k<<2)|(TRANSFERRATE_300k<<4)|(TRANSFERRATE_300k<<6),0xF9,1024, 3, 112,DENSITY_DOUBLE           ,GAPLENGTH_3_5,0xC0 }, //720K 3.5" supports 250kbits, 300kbits DD! Disable gap length checking here because we need to work without it on a XT?
	{1440, 18,  2, 80, 3, 1, TRANSFERRATE_250k|(TRANSFERRATE_300k<<2)|(TRANSFERRATE_500k<<4)|(TRANSFERRATE_500k<<6),0xF0,512 , 9, 224,DENSITY_IGNORE|DENSITY_HD,GAPLENGTH_3_5,0x80 }, //1.44M 3.5" supports 250kbits, 500kbits HD! Disable gap length checking here because we need to work without it on a XT?
	{1680, 21,  2, 80, 3, 1, TRANSFERRATE_250k|(TRANSFERRATE_500k<<2)|(TRANSFERRATE_500k<<4)|(TRANSFERRATE_500k<<6),0xF0,512 , 9, 224,DENSITY_IGNORE|DENSITY_HD,GAPLENGTH_3_5,0x80    }, //1.68M 3.5" supports 250kbits, 500kbits HD! Supporting BIOS only!
	{1722, 21,  2, 82, 3, 1, TRANSFERRATE_250k|(TRANSFERRATE_500k<<2)|(TRANSFERRATE_500k<<4)|(TRANSFERRATE_500k<<6),0xF0,512 , 9, 224,DENSITY_IGNORE|DENSITY_HD,GAPLENGTH_3_5,0x80    }, //1.722M 3.5" supports 250kbits, 500kbits HD! Supporting BIOS only!
	{1840, 23,  2, 80, 3, 1, TRANSFERRATE_250k|(TRANSFERRATE_500k<<2)|(TRANSFERRATE_500k<<4)|(TRANSFERRATE_500k<<6),0xF0,512 , 9, 224,DENSITY_IGNORE|DENSITY_HD,GAPLENGTH_3_5,0x80    }, //1.84M 3.5" supports 250kbits, 500kbits HD! Supporting BIOS only!
	{2880, 36,  2, 80, 2, 1, TRANSFERRATE_1M|(TRANSFERRATE_1M<<2)|(TRANSFERRATE_1M<<4)|(TRANSFERRATE_1M<<6),        0xF0,1024, 9, 240,DENSITY_IGNORE|DENSITY_ED,GAPLENGTH_IGNORE,0x40 } //2.88M 3.5" supports 1Mbits ED!
};

//BPS=512 always(except differently programmed)!

//Floppy geometries

byte floppy_spt(uint_64 floppy_size)
{
	int i;
	for (i = 0; i<(int)NUMITEMS(floppygeometries); i++)
	{
		if (floppygeometries[i].KB == KB(floppy_size)) return (byte)floppygeometries[i].SPT; //Found?
	}
	return 0; //Unknown!
}

byte floppy_tracks(uint_64 floppy_size)
{
	int i;
	for (i = 0; i<(int)NUMITEMS(floppygeometries); i++)
	{
		if (floppygeometries[i].KB == KB(floppy_size)) return floppygeometries[i].tracks; //Found?
	}
	return 0; //Unknown!
}

byte floppy_sides(uint_64 floppy_size)
{
	int i;
	for (i = 0; i<(int)NUMITEMS(floppygeometries); i++)
	{
		if (floppygeometries[i].KB == KB(floppy_size)) return (byte)floppygeometries[i].sides; //Found?
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
	for (i = 0; i < NUMITEMS(floppygeometries); i++) //Update the geometry!
	{
		if (floppygeometries[i].KB == KB(floppysize)) //Found?
		{
			FLOPPY.geometries[floppy] = &floppygeometries[i]; //The geometry we use!
			return; //Stop searching!
		}
	}

	//Unknown geometry!
	if ((DSKImageFile = getDSKimage((floppy) ? FLOPPY1 : FLOPPY0))) //Are we a DSK image file?
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

uint_32 floppy_LBA(byte floppy, word side, word track, word sector)
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
	raiseirq(FLOPPY_IRQ); //Execute the IRQ!
}

OPTINLINE void FLOPPY_lowerIRQ()
{
	FLOPPY.IRQPending = 0; //We're not pending anymore!
	lowerirq(FLOPPY_IRQ); //Lower the IRQ!
	acnowledgeIRQrequest(FLOPPY_IRQ); //Acnowledge!
}

OPTINLINE byte FLOPPY_useDMA()
{
	return (FLOPPY.DOR.DMA && (!FLOPPY.DriveData[FLOPPY.DOR.DriveNumber].NDM)); //Are we using DMA?
}

OPTINLINE byte FLOPPY_supportsrate(byte disk)
{
	return 1; //Support all rates officially!
	if (!FLOPPY.geometries[disk]) return 1; //No disk geometry, so supported by default(unknown drive)!
	byte supported = 0, current=0, currentrate;
	supported = FLOPPY.geometries[disk]->supportedrates; //Load the supported rates!
	currentrate = FLOPPY.CCR.rate; //Current rate we use (both CCR and DSR can be used, since they're both updated when either changes)!
	for (;current<4;) //Check all available rates!
	{
		if (currentrate==(supported&3)) return 1; //We're a supported rate!
		supported  >>= 2; //Check next rate!
		++current; //Next supported!
	}
	return 0; //Unsupported rate!
}

OPTINLINE void FLOPPY_handlereset(byte source) //Resets the floppy disk command when needed!
{
	byte pending_size; //Our currently pending size to use!
	if ((!FLOPPY.DOR.REST) || FLOPPY.DSR.SWReset) //We're to reset by either one enabled?
	{
		if (!FLOPPY.floppy_resetted) //Not resetting yet?
		{
			if (source==1)
			{
				FLOPPY_LOGD("FLOPPY: Reset requested by DSR!")
			}
			else
			{
				FLOPPY_LOGD("FLOPPY: Reset requested by DOR!")
			}
			FLOPPY.DOR.MotorControl = 0; //Reset motors!
			FLOPPY.DOR.DriveNumber = 0; //Reset drives!
			FLOPPY.DOR.DMA = 0; //IRQ channel!
			FLOPPY.MSR.data = 0; //Default to no data!
			FLOPPY.commandposition = 0; //No command!
			FLOPPY.commandstep = 0; //Reset step to indicate we're to read the result in ST0!
			FLOPPY.ST0.data = 0xC0; //Reset ST0 to the correct value: drive became not ready!
			FLOPPY.ST1.data = FLOPPY.ST2.data = 0; //Reset the ST data!
			pending_size = 4; //Pending full size with polling mode enabled!
			if (FLOPPY.Configuration.DrivePollingModeDisable) pending_size = 0; //Don't pend when polling mode is off!
			FLOPPY.reset_pending_size = FLOPPY.reset_pending = pending_size; //We have a reset pending for all 4 drives, unless interrupted by an other command!
			FLOPPY.reset_pended = 1; //We're pending a reset! Clear status once we're becoming active!
			memset(FLOPPY.currenthead, 0, sizeof(FLOPPY.currenthead)); //Clear the current heads!
			memset(FLOPPY.currentcylinder, 0, sizeof(FLOPPY.currentcylinder)); //Clear the current heads!
			memset(FLOPPY.currentsector, 1, sizeof(FLOPPY.currentsector)); //Clear the current sectors!
			FLOPPY.TC = 0; //Disable TC identifier!
			if (!FLOPPY.Locked) //Are we not locked? Perform stuff that's not locked during reset!
			{
				FLOPPY.Configuration.Threshold = 0; //Reset threshold!
				FLOPPY.Configuration.FIFODisable = 1; //Disable the FIFO!
			}
			FLOPPY.floppy_resetted = 1; //We're resetted!
			FLOPPY.ignorecommands = 0; //We're enabling commands again!
		}
	}
	else if (FLOPPY.floppy_resetted) //We were resetted and are activated?
	{
		FLOPPY_raiseIRQ(); //Raise the IRQ: We're reset and have been activated!
		FLOPPY.floppy_resetted = 0; //Not resetted anymore!
		if (source==1)
		{
			FLOPPY_LOGD("FLOPPY: Activation requested by DSR!")
		}
		else
		{
			FLOPPY_LOGD("FLOPPY: Activation requested by DOR!")
		}
	}
}

//Execution after command and data phrases!
byte oldMSR = 0; //Old MSR!

OPTINLINE void updateFloppyMSR() //Update the floppy MSR!
{
	switch (FLOPPY.commandstep) //What command step?
	{
	case 0: //Command?
		FLOPPY.sectorstransferred = 0; //There's nothing transferred yet!
		FLOPPY.MSR.CommandBusy = 0; //Not busy: we're waiting for a command!
		FLOPPY.MSR.RQM = !FLOPPY.floppy_resetted; //Ready for data transfer when not being reset!
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
		case WRITE_DATA: //Write sector?
		case WRITE_DELETED_DATA: //Write deleted sector?
		case FORMAT_TRACK: //Format sector?
		case READ_DATA: //Read sector?
		case READ_DELETED_DATA: //Read deleted sector?
			FLOPPY.MSR.RQM = FLOPPY.MSR.NonDMA = !FLOPPY_useDMA(); //Use no DMA? Then transfer data and set NonDMA! Else, clear non DMA and don't transfer!
			break;
		default: //Unknown command?
			FLOPPY.MSR.RQM = FLOPPY.MSR.NonDMA = 1; //Use no DMA by default, for safety!
			break; //Don't process!
		}

		//Check data direction!
		switch (FLOPPY.commandbuffer[0]) //Process input/output to/from controller!
		{
		case WRITE_DATA: //Write sector?
		case WRITE_DELETED_DATA: //Write deleted sector?
		case FORMAT_TRACK: //Format sector?
			FLOPPY.MSR.HaveDataForCPU = 0; //We request data from the CPU!
			break;
		case READ_DATA: //Read sector?
		case READ_DELETED_DATA: //Read deleted sector?
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
	if (FLOPPY.MSR.data != oldMSR) //MSR changed?
	{
		oldMSR = FLOPPY.MSR.data; //Update old!
		FLOPPY_LOGD("FLOPPY: MSR changed: %02x", FLOPPY.MSR.data) //The updated MSR!
	}
}

OPTINLINE void updateFloppyDIR() //Update the floppy DIR!
{
	FLOPPY.DIR.data = 0; //Init to not changed!
	if (FLOPPY.diskchanged[0] && (FLOPPY.DOR.MotorControl&1))
	{
		FLOPPY.DIR.data = 0x80; //Set our bit!
	}
	if (FLOPPY.diskchanged[1] && (FLOPPY.DOR.MotorControl&2))
	{
		FLOPPY.DIR.data = 0x80; //Set our bit!
	}
	if (FLOPPY.diskchanged[2] && (FLOPPY.DOR.MotorControl&4))
	{
		FLOPPY.DIR.data = 0x80; //Set our bit!
	}
	if (FLOPPY.diskchanged[3] && (FLOPPY.DOR.MotorControl&8))
	{
		FLOPPY.DIR.data = 0x80; //Set our bit!
	}
	//Rest of the bits are reserved on an AT!
}

OPTINLINE void clearDiskChanged()
{
	//Reset state for all drives!
	FLOPPY.diskchanged[0] = 0; //Reset!
	FLOPPY.diskchanged[1] = 0; //Reset!
	FLOPPY.diskchanged[2] = 0; //Reset!
	FLOPPY.diskchanged[3] = 0; //Reset!
}

OPTINLINE void updateFloppyTrack0()
{
	FLOPPY.ST3.Track0 = (FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber] == 0); //Are we at track 0?
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
			if (!FLOPPY_useDMA()) //Non-DMA mode?
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

	if (FLOPPY_useDMA()) //DMA mode determines our triggering?
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
		FLOPPY_LOGD("FLOPPY: Start transfer of data (DMA: %i)...",FLOPPY_useDMA())
	}
	FLOPPY.commandstep = 2; //Move to data phrase!
	if (FLOPPY_useDMA()) //DMA mode?
	{
		FLOPPY.DMAPending = 1; //Pending DMA!
	}
	FLOPPY_dataReady(); //We have data to transfer!
}

OPTINLINE void floppy_readsector() //Request a read sector command!
{
	char *DSKImageFile = NULL; //DSK image file to use?
	SECTORINFORMATIONBLOCK sectorinformation; //Information about the sector!

	if (!FLOPPY.geometries[FLOPPY.DOR.DriveNumber]) //Not inserted or valid?
	{
		FLOPPY_LOGD("FLOPPY: Error: Invalid drive!")
		FLOPPY.ST0.data = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;
	}
	if ((FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->DoubleDensity!=(FLOPPY.DoubleDensity&~DENSITY_IGNORE)) && (!(FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->DoubleDensity&DENSITY_IGNORE) || density_forced) && EMULATE_DENSITY) //Wrong density?
	{
		FLOPPY_LOGD("FLOPPY: Error: Invalid density!")
		FLOPPY.ST0.data = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;
	}

	FLOPPY.databuffersize = translateSectorSize(FLOPPY.commandbuffer[5]); //Sector size into data buffer!
	if (!FLOPPY.commandbuffer[5]) //Special case? Use given info!
	{
		FLOPPY.databuffersize = FLOPPY.commandbuffer[8]; //Use data length!
	}
	FLOPPY.disk_startpos = floppy_LBA(FLOPPY.DOR.DriveNumber, FLOPPY.currenthead[FLOPPY.DOR.DriveNumber], FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber], FLOPPY.currentsector[FLOPPY.DOR.DriveNumber]); //The start position, in sectors!
	FLOPPY_LOGD("FLOPPY: Read sector #%i", FLOPPY.disk_startpos) //We're reading this sector!
	if (FLOPPY.commandstep != 2) { FLOPPY_LOGD("FLOPPY: Sector size: %i bytes", FLOPPY.databuffersize) }
	FLOPPY.disk_startpos *= FLOPPY.databuffersize; //Calculate the start sector!
	if (FLOPPY.commandstep != 2) { FLOPPY_LOGD("FLOPPY: Requesting transfer for %i bytes.", FLOPPY.databuffersize) } //Transfer this many sectors!

	if (!(FLOPPY.DOR.MotorControl&(1 << FLOPPY.DOR.DriveNumber))) //Not motor ON?
	{
		FLOPPY_LOGD("FLOPPY: Error: drive motor not ON!")
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		FLOPPY.ST0.data = 0x40; //Abnormal termination!
		return;
	}

	FLOPPY.ST0.UnitSelect = FLOPPY.DOR.DriveNumber; //Current unit!
	FLOPPY.ST0.CurrentHead = (FLOPPY.commandbuffer[2] & 1); //Current head!
	FLOPPY.ST0.NotReady = 1; //We're not ready yet!
	FLOPPY.ST0.UnitCheck = FLOPPY.ST0.SeekEnd = FLOPPY.ST0.InterruptCode = 0; //Clear unit check and Interrupt code: we're OK. Also clear SE flag: we're still busy!

	if (!FLOPPY_supportsrate(FLOPPY.DOR.DriveNumber) || !FLOPPY.geometries[FLOPPY.DOR.DriveNumber]) //We don't support the rate or geometry?
	{
		goto floppy_errorread; //Error out!
	}

	if (readdata(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0, &FLOPPY.databuffer, FLOPPY.disk_startpos, FLOPPY.databuffersize)) //Read the data into memory?
	{
		if ((FLOPPY.commandbuffer[7]!=FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->GAPLength) && (FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->GAPLength!=GAPLENGTH_IGNORE) && EMULATE_GAPLENGTH) //Wrong GAP length?
		{
			FLOPPY.ST0.data = 0x40; //Abnormal termination!
			FLOPPY.commandstep = 0xFF; //Move to error phase!
			return;					
		}
		FLOPPY.ST0.SeekEnd = 1; //Successfull read with implicit seek!
		FLOPPY_startData();
	}
	else //DSK or error?
	{
		if ((DSKImageFile = getDSKimage((FLOPPY.DOR.DriveNumber) ? FLOPPY1 : FLOPPY0))) //Are we a DSK image file?
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

		floppy_errorread: //Error reading data?
		//Plain error reading the sector!
		FLOPPY.ST0.data = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Error!
	}
}

OPTINLINE void FLOPPY_formatsector() //Request a read sector command!
{
	char *DSKImageFile;
	SECTORINFORMATIONBLOCK sectorinfo;
	++FLOPPY.sectorstransferred; //A sector has been transferred!
	if (!FLOPPY_supportsrate(FLOPPY.DOR.DriveNumber) || !FLOPPY.geometries[FLOPPY.DOR.DriveNumber]) //We don't support the rate or geometry?
	{
		goto floppy_errorformat; //Error out!
	}

	if (!FLOPPY_supportsrate(FLOPPY.DOR.DriveNumber) || !FLOPPY.geometries[FLOPPY.DOR.DriveNumber]) //We don't support the rate or geometry?
	{
		FLOPPY.ST0.data = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;
	}

	if ((FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->DoubleDensity!=(FLOPPY.DoubleDensity&~DENSITY_IGNORE)) && (!(FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->DoubleDensity&DENSITY_IGNORE) || density_forced) && EMULATE_DENSITY) //Wrong density?
	{
		FLOPPY_LOGD("FLOPPY: Error: Invalid density!")
		FLOPPY.ST0.data = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;					
	}

	if ((FLOPPY.commandbuffer[5]!=FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->GAPLength) && (FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->GAPLength!=GAPLENGTH_IGNORE) && EMULATE_GAPLENGTH) //Wrong GAP length?
	{
		FLOPPY.ST0.data = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;					
	}


	if (drivereadonly(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0)) //Read only drive?
	{
		FLOPPY_LOGD("FLOPPY: Finished transfer of data (%i sector(s)).", FLOPPY.sectorstransferred) //Log the completion of the sectors written!
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
			floppy_errorformat:
			FLOPPY.ST0.data = 0x40; //Invalid command!
			FLOPPY.commandstep = 0xFF; //Error!
			return; //Error!
		}
		if (FLOPPY.databuffer[1] != FLOPPY.currenthead[FLOPPY.DOR.DriveNumber]) //Not current head?
		{
			goto floppy_errorformat;
			return; //Error!
		}
		if (FLOPPY.databuffer[2] != FLOPPY.currentsector[FLOPPY.DOR.DriveNumber]) //Not current sector?
		{
			goto floppy_errorformat;
			return; //Error!
		}

		//Check disk specific information!
		if ((DSKImageFile = getDSKimage((FLOPPY.DOR.DriveNumber) ? FLOPPY1 : FLOPPY0))) //Are we a DSK image file?
		{
			if (!readDSKSectorInfo(DSKImageFile, FLOPPY.databuffer[1], FLOPPY.databuffer[0], FLOPPY.databuffer[2], &sectorinfo)) //Failed to read sector information block?
			{
				goto floppy_errorformat;
				return; //Error!
			}

			//Verify sector size!
			if (sectorinfo.SectorSize != FLOPPY.databuffer[3]) //Invalid sector size?
			{
				goto floppy_errorformat;
				return; //Error!
			}

			//Fill the sector buffer and write it!
			memset(FLOPPY.databuffer, FLOPPY.commandbuffer[5], (1 << sectorinfo.SectorSize)); //Clear our buffer with the fill byte!
			if (!writeDSKSectorData(DSKImageFile, FLOPPY.currenthead[FLOPPY.DOR.DriveNumber], FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber], FLOPPY.currentsector[FLOPPY.DOR.DriveNumber], sectorinfo.SectorSize, &FLOPPY.databuffer)) //Failed writing the formatted sector?
			{
				goto floppy_errorformat;
				return; //Error!
			}
		}
		else //Are we a normal image file?
		{
			if (FLOPPY.databuffer[3] != 0x2) //Not 512 bytes/sector?
			{
				goto floppy_errorformat;
				return; //Error!
			}
			memset(FLOPPY.databuffer, FLOPPY.commandbuffer[5], 512); //Clear our buffer with the fill byte!
			if (!writedata(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0, &FLOPPY.databuffer, floppy_LBA(FLOPPY.DOR.DriveNumber, FLOPPY.currenthead[FLOPPY.DOR.DriveNumber], FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber], FLOPPY.currentsector[FLOPPY.DOR.DriveNumber]),512)) //Failed writing the formatted sector?
			{
				goto floppy_errorformat;
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
	FLOPPY.databuffersize = translateSectorSize(FLOPPY.commandbuffer[5]); //Sector size into data buffer!
	if (!FLOPPY.commandbuffer[5]) //Special case? Use given info!
	{
		FLOPPY.databuffersize = FLOPPY.commandbuffer[8]; //Use data length!
	}
	FLOPPY.disk_startpos = floppy_LBA(FLOPPY.DOR.DriveNumber, FLOPPY.commandbuffer[3], FLOPPY.commandbuffer[2], FLOPPY.commandbuffer[4]); //The start position, in sectors!
	if (FLOPPY.commandstep != 2) { FLOPPY_LOGD("FLOPPY: Write sector #%i", FLOPPY.disk_startpos) } //We're reading this sector!
	if (FLOPPY.commandstep != 2) { FLOPPY_LOGD("FLOPPY: Sector size: %i bytes", FLOPPY.databuffersize) }
	FLOPPY.disk_startpos *= FLOPPY.databuffersize; //Calculate the start sector!
	if (FLOPPY.commandstep != 2) { FLOPPY_LOGD("FLOPPY: Requesting transfer for %i bytes.", FLOPPY.databuffersize) } //Transfer this many sectors!

	if (FLOPPY.commandstep != 2) { FLOPPY_LOGD("FLOPPY: Write sector: CHS=%i,%i,%i; Params: %02X%02X%02x%02x%02x%02x%02x%02x", FLOPPY.commandbuffer[3], FLOPPY.commandbuffer[2], FLOPPY.commandbuffer[4], FLOPPY.commandbuffer[1], FLOPPY.commandbuffer[2], FLOPPY.commandbuffer[3], FLOPPY.commandbuffer[4], FLOPPY.commandbuffer[5], FLOPPY.commandbuffer[6], FLOPPY.commandbuffer[7], FLOPPY.commandbuffer[8]) } //Log our request!

	if (!(FLOPPY.DOR.MotorControl&(1 << FLOPPY.DOR.DriveNumber))) //Not motor ON?
	{
		FLOPPY_LOGD("FLOPPY: Error: drive motor not ON!")
		FLOPPY.ST0.data = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;
	}

	if (!FLOPPY_supportsrate(FLOPPY.DOR.DriveNumber) || !FLOPPY.geometries[FLOPPY.DOR.DriveNumber]) //We don't support the rate or geometry?
	{
		FLOPPY.ST0.data = 0x40; //Abnormal termination!
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
	switch (FLOPPY.commandbuffer[0]) //What command!
	{
		case WRITE_DATA: //Write sector
		case WRITE_DELETED_DATA: //Write deleted sector
			//Write sector to disk!
			updateFloppyWriteProtected(1); //Try to write with(out) protection!
			if (FLOPPY.databufferposition == FLOPPY.databuffersize) //Fully buffered?
			{
				if (!FLOPPY_supportsrate(FLOPPY.DOR.DriveNumber) || !FLOPPY.geometries[FLOPPY.DOR.DriveNumber]) //We don't support the rate or geometry?
				{
					FLOPPY_LOGD("FLOPPY: Error: Invalid disk rate/geometry!")
					FLOPPY.ST0.data = 0x40; //Abnormal termination!
					FLOPPY.commandstep = 0xFF; //Move to error phase!
					return;
				}
				if ((FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->DoubleDensity!=(FLOPPY.DoubleDensity&~DENSITY_IGNORE)) && (!(FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->DoubleDensity&DENSITY_IGNORE) || density_forced) && EMULATE_DENSITY) //Wrong density?
				{
					FLOPPY_LOGD("FLOPPY: Error: Invalid density!")
					FLOPPY.ST0.data = 0x40; //Abnormal termination!
					FLOPPY.commandstep = 0xFF; //Move to error phase!
					return;					
				}
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
					FLOPPY_LOGD("FLOPPY: Finished transfer of data (%i sector(s)).", FLOPPY.sectorstransferred) //Log the completion of the sectors written!
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
						FLOPPY_LOGD("FLOPPY: Finished transfer of data (readonly).") //Log the completion of the sectors written!
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
						if ((DSKImageFile = getDSKimage((FLOPPY.DOR.DriveNumber) ? FLOPPY1 : FLOPPY0))) //Are we a DSK image file?
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
								FLOPPY_LOGD("FLOPPY: Finished transfer of data (%i sector(s)).", FLOPPY.sectorstransferred) //Log the completion of the sectors written!
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
						FLOPPY.ST0.data = 0x40; //Invalid command!
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
		case READ_TRACK: //Read complete track
		case READ_DATA: //Read sector
		case READ_DELETED_DATA: //Read deleted sector
			//We've finished reading the read data!
			//updateFloppyWriteProtected(0); //Try to read with(out) protection!
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
				FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[5]; //Sector size from the command buffer!
				FLOPPY.commandstep = 3; //Move to result phrase and give the result!
				FLOPPY_LOGD("FLOPPY: Finished transfer of data (%i sectors).", FLOPPY.sectorstransferred) //Log the completion of the sectors written!
				FLOPPY_raiseIRQ(); //Entering result phase!
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
				FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[5]; //Sector size!
				FLOPPY.commandstep = 3; //Move to result phase!
				FLOPPY_raiseIRQ(); //Entering result phase!
			}
			break;
		case FORMAT_TRACK: //Format sector
			updateFloppyWriteProtected(1); //Try to write with(out) protection!
			FLOPPY_formatsector(); //Execute a format sector command!
			break;
		default: //Unknown command?
			FLOPPY.commandstep = 0xFF; //Move to error phrase!
			FLOPPY.ST0.data = 0x80; //Invalid command!
			break;
	}
}

OPTINLINE void updateST3()
{
	updateFloppyTrack0(); //Update track 0 bit!
	if (FLOPPY.geometries[FLOPPY.DOR.DriveNumber]) //Valid drive?
	{
		FLOPPY.ST3.DoubleSided = (FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->sides==2)?1:0; //Are we double sided?
	}
	else //Apply default disk!
	{
		FLOPPY.ST3.DoubleSided = 1; //Are we double sided?
	}
	FLOPPY.ST3.Head1Active = FLOPPY.currenthead[FLOPPY.DOR.DriveNumber]; //Is head 1 active?
	FLOPPY.ST3.DriveSelect = FLOPPY.DOR.DriveNumber; //Our selected drive!
	FLOPPY.ST3.DriveReady = 1; //We're always ready!
	if (FLOPPY.DOR.DriveNumber<2) //Valid drive number?
	{
		FLOPPY.ST3.WriteProtection = drivereadonly(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0); //Read-only drive and tried to write?
	}
	else
	{
		FLOPPY.ST3.WriteProtection = 0; //Drive unsupported? No write protection!
	}
	FLOPPY.ST3.ErrorSignature = 0; //No errors here!
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
	updateFloppyGeometries(FLOPPY.DOR.DriveNumber, FLOPPY.currenthead[FLOPPY.DOR.DriveNumber], FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber]); //Update the floppy geometries!
	switch (FLOPPY.commandbuffer[0]) //What command!
	{
		case WRITE_DATA: //Write sector
			FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber] = FLOPPY.commandbuffer[2]; //Current cylinder!
			FLOPPY.currenthead[FLOPPY.DOR.DriveNumber] = FLOPPY.commandbuffer[3]; //Current head!
			FLOPPY.currentsector[FLOPPY.DOR.DriveNumber] = FLOPPY.commandbuffer[4]; //Current sector!
			floppy_writesector(); //Start writing a sector!
			break;
		case READ_DATA: //Read sector
			FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber] = FLOPPY.commandbuffer[2]; //Current cylinder!
			FLOPPY.currenthead[FLOPPY.DOR.DriveNumber] = FLOPPY.commandbuffer[3]; //Current head!
			FLOPPY.currentsector[FLOPPY.DOR.DriveNumber] = FLOPPY.commandbuffer[4]; //Current sector!
			floppy_readsector(); //Start reading a sector!
			break;
		case SPECIFY: //Fix drive data/specify command
			FLOPPY.DriveData[FLOPPY.DOR.DriveNumber].data[0] = FLOPPY.commandbuffer[1]; //Set setting byte 1/2!
			FLOPPY.DriveData[FLOPPY.DOR.DriveNumber].data[1] = FLOPPY.commandbuffer[2]; //Set setting byte 2/2!
			FLOPPY.commandstep = 0; //Reset controller command status!
			FLOPPY.ST0.data = 0x00; //Correct command!
			updateFloppyWriteProtected(0); //Try to read with(out) protection!
			break;
		case RECALIBRATE: //Calibrate drive
			//Execute interrupt!
			FLOPPY.commandstep = 0; //Reset controller command status!
			FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber] = 0; //Goto cylinder #0!
			FLOPPY.ST0.data = 0x20; //Completed command!
			updateFloppyWriteProtected(0); //Try to read with(out) protection!
			clearDiskChanged(); //Clear the disk changed flag for the new command!
			FLOPPY_raiseIRQ(); //We're finished!
			break;
		case SENSE_INTERRUPT: //Check interrupt status
			//Set result
			updateFloppyWriteProtected(0); //Try to read with(out) protection!
			FLOPPY.commandstep = 3; //Move to result phrase!
			byte datatemp;
			datatemp = FLOPPY.ST0.data; //Save default!
			//Reset IRQ line!
			if (FLOPPY.reset_pending) //Reset is pending?
			{
				byte reset_drive;
				reset_drive = FLOPPY.reset_pending_size - (FLOPPY.reset_pending--); //We're pending this drive!
				FLOPPY_LOGD("FLOPPY: Reset Sense Interrupt, pending drive %i/%i...",reset_drive,FLOPPY.reset_pending_size)
				FLOPPY.ST0.data &= 0xF8; //Clear low 3 bits!
				FLOPPY.ST0.UnitSelect = reset_drive; //What drive are we giving!
				FLOPPY.ST0.CurrentHead = (FLOPPY.currenthead[reset_drive] & 1); //Set the current head of the drive!
				datatemp = FLOPPY.ST0.data; //Use the current data, not the cleared data!
			}
			else if (!FLOPPY.IRQPending) //Not an pending IRQ?
			{
				FLOPPY_LOGD("FLOPPY: Warning: Checking interrupt status without IRQ pending! Locking up controller!")
				FLOPPY.ignorecommands = 1; //Ignore commands until a reset!
				FLOPPY.ST0.data = 0x80; //Error!
				FLOPPY.commandstep = 0xFF; //Error out!
				return; //Error out now!
			}
			FLOPPY_LOGD("FLOPPY: Sense interrupt: ST0=%02X, Currentcylinder=%02X", datatemp, FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber])
			FLOPPY.resultbuffer[0] = datatemp; //Give old ST0 if changed this call!
			FLOPPY.resultbuffer[1] = FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber]; //Our idea of the current cylinder!
			FLOPPY.resultposition = 0; //Start result!
			FLOPPY.commandstep = 3; //Result phase!
			break;
		case SEEK: //Seek/park head
			FLOPPY.commandstep = 0; //Reset controller command status!
			updateFloppyWriteProtected(0); //Try to read with(out) protection!
			if (FLOPPY.DOR.DriveNumber >= 2) //Invalid drive?
			{
				FLOPPY.ST0.data = (FLOPPY.ST0.data & 0x32) | 0x14 | FLOPPY.DOR.DriveNumber; //Error: drive not ready!
				FLOPPY.commandstep = 0; //Reset command!
				clearDiskChanged(); //Clear the disk changed flag for the new command!
				return; //Abort!
			}
			if (!has_drive(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0)) //Floppy not inserted?
			{
				FLOPPY.ST0.data = (FLOPPY.ST0.data & 0x30) | 0x18 | FLOPPY.DOR.DriveNumber; //Error: drive not ready!
				FLOPPY.commandstep = 0; //Reset command!
				clearDiskChanged(); //Clear the disk changed flag for the new command!
				return; //Abort!
			}
			if (FLOPPY.commandbuffer[2] < floppy_tracks(disksize(FLOPPY.DOR.DriveNumber ? FLOPPY1 : FLOPPY0))) //Valid track?
			{
				FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber] = FLOPPY.commandbuffer[2]; //Set the current cylinder!
				FLOPPY.ST0.data = (FLOPPY.ST0.data & 0x30) | 0x20 | FLOPPY.DOR.DriveNumber; //Valid command!
				FLOPPY_raiseIRQ(); //Finished executing phase!
				clearDiskChanged(); //Clear the disk changed flag for the new command!
				return; //Give an error!
			}

			//Invalid track?
			FLOPPY.ST2.data |= 0x4; //Invalid seek!
			FLOPPY.commandstep = (byte)(FLOPPY.commandposition = 0); //Reset command!
			break;
		case SENSE_DRIVE_STATUS: //Check drive status
			updateST3(); //Update ST3 only!
			FLOPPY.resultbuffer[0] = FLOPPY.ST3.data; //Give ST3!
			FLOPPY.resultposition = 0; //Start the result!
			FLOPPY.commandstep = 3; //Result phase!
			break;
		case READ_ID: //Read sector ID
			if (!FLOPPY_supportsrate(FLOPPY.DOR.DriveNumber)) //We don't support the rate?
			{
				goto floppy_errorReadID; //Error out!
			}
			if ((DSKImageFile = getDSKimage((FLOPPY.DOR.DriveNumber) ? FLOPPY1 : FLOPPY0))) //Are we a DSK image file?
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
				if ((int_32)floppy_LBA(FLOPPY.DOR.DriveNumber, FLOPPY.currenthead[FLOPPY.DOR.DriveNumber], FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber], FLOPPY.currentsector[FLOPPY.DOR.DriveNumber]) >= (int_32)(FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->KB * 1024)) //Invalid address within our image!
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
			return; //Correct read!
			floppy_errorReadID:
			FLOPPY.ST0.data = 0x40; //Error!
			FLOPPY.ST1.NoAddressMark = FLOPPY.ST1.NoData = 1; //Invalid sector!
			FLOPPY.resultposition = 0; //Start the result!
			FLOPPY.resultbuffer[0] = FLOPPY.ST0.data; //ST0!
			FLOPPY.resultbuffer[1] = FLOPPY.ST1.data; //ST1!
			FLOPPY.resultbuffer[2] = FLOPPY.ST2.data; //ST2!
			FLOPPY.resultbuffer[3] = FLOPPY.currentcylinder[FLOPPY.DOR.DriveNumber]; //Cylinder!
			FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY.DOR.DriveNumber]; //Head!
			FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY.DOR.DriveNumber]; //Sector!
			return; //Incorrect read!
			break;
		case FORMAT_TRACK: //Format sector
			if (!(FLOPPY.DOR.MotorControl&(1 << FLOPPY.DOR.DriveNumber))) //Not motor ON?
			{
				FLOPPY_LOGD("FLOPPY: Error: drive motor not ON!")
				FLOPPY.ST0.data = 0x40; //Invalid command!
				FLOPPY.commandstep = 0xFF; //Move to error phase!
				return;
			}

			if (!FLOPPY.geometries[FLOPPY.DOR.DriveNumber]) //No geometry?
			{
				FLOPPY.ST0.data = 0x40; //Invalid command!
				FLOPPY.commandstep = 0xFF; //Error!
			}

			if (FLOPPY.commandbuffer[3] != FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->SPT) //Invalid SPT?
			{
				FLOPPY.ST0.data = 0x40; //Invalid command!
				FLOPPY.commandstep = 0xFF; //Error!
			}

			if ((DSKImageFile = getDSKimage((FLOPPY.DOR.DriveNumber) ? FLOPPY1 : FLOPPY0))) //Are we a DSK image file?
			{
				FLOPPY.databuffersize = 4; //We're 4 bytes per sector!
				FLOPPY_startData(); //Start the data transfer!
			}
			else //Normal standard emulated sector?
			{
				if (FLOPPY.commandbuffer[2] != 0x2) //Not 512 bytes/sector?
				{
					FLOPPY.ST0.data = 0x40; //Invalid command!
					FLOPPY.commandstep = 0xFF; //Error!
				}
				else
				{
					FLOPPY.databuffersize = 4; //We're 4 bytes per sector!
					FLOPPY_startData(); //Start the data transfer!
				}
			}
			break;
		case VERSION: //Version command?
			FLOPPY.resultposition = 0; //Start our result phase!
			FLOPPY.resultbuffer[0] = 0x90; //We're a 82077AA!
			FLOPPY.commandstep = 3; //We're starting the result phase!
			break;
		case CONFIGURE: //Configuration command?
			//Load our 3 parameters for usage!
			FLOPPY.Configuration.data[0] = FLOPPY.commandbuffer[1];
			FLOPPY.Configuration.data[1] = FLOPPY.commandbuffer[2];
			FLOPPY.Configuration.data[2] = FLOPPY.commandbuffer[3];
			FLOPPY.commandstep = 0; //Finish silently! No result bytes or interrupt!
			break;
		case LOCK: //Lock command?
			FLOPPY.Locked = FLOPPY.MT; //Set/unset the lock depending on the MT bit!
			FLOPPY.resultposition = 0; //Start our result phase!
			FLOPPY.resultbuffer[0] = (FLOPPY.Locked<<4); //Give the lock bit as a result!
			FLOPPY.commandstep = 3; //We're starting the result phase!
			break;
		case READ_TRACK: //Read complete track!
		case WRITE_DELETED_DATA: //Write deleted sector
		case READ_DELETED_DATA: //Read deleted sector
			invaliddrive: //Invalid drive detected?
			FLOPPY.commandstep = 0xFF; //Move to error phrase!
			FLOPPY.ST0.data = 0x40; //Invalid command!
			break;
	}
}

OPTINLINE void floppy_abnormalpolling()
{
	FLOPPY_LOGD("FLOPPY: Abnormal termination because of abnormal polling!")
	FLOPPY.ST0.InterruptCode = 3; //Abnormal termination by polling!
	FLOPPY.ST0.NotReady = 1; //We became not ready!
	FLOPPY.commandstep = 0xFF; //Error!
}

OPTINLINE void floppy_writeData(byte value)
{
	byte commandlength[0x20] = {
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
		,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 //10-1F: Unsupported
		};
	//TODO: handle floppy writes!
	switch (FLOPPY.commandstep) //What step are we at?
	{
		case 0: //Command
			if (FLOPPY.ignorecommands) return; //Ignore commands: we're locked up!
			FLOPPY.commandstep = 1; //Start inserting parameters!
			FLOPPY.commandposition = 1; //Start at position 1 with out parameters/data!
			FLOPPY_LOGD("FLOPPY: Command byte sent: %02X", value) //Log our information about the command byte!
			FLOPPY.MT = (value & CMD_EXT_MULTITRACKOPERATION)?1:0; //Multiple track mode?
			FLOPPY.DoubleDensity = (value & CMD_EXT_DOUBLEDENSITYMODE)?1:0; //Multiple track mode?
			FLOPPY.Skip = (value & CMD_EXT_SKIPDELETEDADDRESSMARKS)?1:0; //Multiple track mode?
			value &= 0x1F; //Make sure that the high data is filtered out!
			switch (value) //What command?
			{
				case SENSE_INTERRUPT: //Check interrupt status
					FLOPPY.commandbuffer[0] = value; //Set the command to use!
					floppy_executeCommand(); //Execute the command!
					break;
				case READ_TRACK: //Read complete track
				case WRITE_DATA: //Write sector
				case READ_DATA: //Read sector
				case SPECIFY: //Fix drive data
				case SENSE_DRIVE_STATUS: //Check drive status
				case RECALIBRATE: //Calibrate drive
				case SEEK: //Seek/park head
				case READ_ID: //Read sector ID
				case FORMAT_TRACK: //Format sector
				case VERSION: //Version
				case CONFIGURE: //Configure
				case LOCK: //Lock
					FLOPPY.reset_pending = 0; //Stop pending reset if we're pending it: we become active!
					if (FLOPPY.reset_pended) //Finished reset?
					{
						FLOPPY_LOGD("FLOPPY: Reset for all drives has been finished!");
	 					FLOPPY.ST0.data = 0x00; //Reset the ST0 register after we've all been read!
						FLOPPY.reset_pended = 0; //Not pending anymore, so don't check for it!
					}
					FLOPPY.commandbuffer[0] = value; //Set the command to use!
					break;
				case WRITE_DELETED_DATA: //Write deleted sector
				case READ_DELETED_DATA: //Read deleted sector
				default: //Invalid command
					FLOPPY_LOGD("FLOPPY: Invalid or unsupported command: %02X",value); //Detection of invalid/unsupported command!
					FLOPPY.ST0.data = 0x80; //Invalid command!
					FLOPPY.commandstep = 0xFF; //Error!
					break;
			}
			break;
		case 1: //Parameters
			FLOPPY_LOGD("FLOPPY: Parameter sent: %02X(#%i/%i)", value, FLOPPY.commandposition, commandlength[FLOPPY.commandbuffer[0]]) //Log the parameter!
			FLOPPY.commandbuffer[FLOPPY.commandposition++] = value; //Set the command to use!
			if (FLOPPY.commandposition > (commandlength[FLOPPY.commandbuffer[0]])) //All parameters have been processed?
			{
				floppy_executeCommand(); //Execute!
				break;
			}
			break;
		case 2: //Data
			switch (FLOPPY.commandbuffer[0]) //What command?
			{
				case WRITE_DATA: //Write sector
				case WRITE_DELETED_DATA: //Write deleted sector
				case FORMAT_TRACK: //Format track
					FLOPPY.databuffer[FLOPPY.databufferposition++] = value; //Set the command to use!
					if (FLOPPY.databufferposition==FLOPPY.databuffersize) //Finished?
					{
						floppy_executeData(); //Execute the command with the given data!
					}
					else //Not completed?
					{
						FLOPPY_dataReady(); //We have data ready to transfer!
						if (FLOPPY_useDMA() && FLOPPY.TC) //DMA mode, Terminal count and not completed? We're ending too soon!
						{
							FLOPPY_LOGD("FLOPPY: Terminal count reached in the middle of a data transfer! Position: %i/%i bytes",FLOPPY.databufferposition,FLOPPY.databuffersize)
							floppy_executeData(); //Execute the command with the given data!
						}
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
	byte resultlength[0x20] = {
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
		7, //f
		1, //10
		0, //11
		0, //12
		0, //13
		0, //14
		0, //15
		7, //16
		0, //17
		0, //18
		0, //19
		0, //1a
		0, //1b
		0, //1c
		0, //1d
		0, //1e
		0 //1f
	};
	byte temp;
	switch (FLOPPY.commandstep) //What step are we at?
	{
		case 0: //Command
			if (FLOPPY.ignorecommands) return 0; //Ignore commands: we're locked up!
			floppy_abnormalpolling(); //Abnormal polling!
			break; //Nothing to read during command phrase!
		case 1: //Parameters
			floppy_abnormalpolling(); //Abnormal polling!
			break; //Nothing to read during parameter phrase!
		case 2: //Data
			switch (FLOPPY.commandbuffer[0]) //What command?
			{
				case READ_TRACK: //Read complete track
				case READ_DATA: //Read sector
				case READ_DELETED_DATA: //Read deleted sector
					temp = FLOPPY.databuffer[FLOPPY.databufferposition++]; //Read data!
					if (FLOPPY.databufferposition==FLOPPY.databuffersize) //Finished?
					{
						floppy_executeData(); //Execute the data finished phrase!
					}
					else //Not completed?
					{
						FLOPPY_dataReady(); //We have data ready to transfer!
						if (FLOPPY_useDMA() && FLOPPY.TC) //DMA mode, Terminal count and not completed? We're ending too soon!
						{
							FLOPPY_LOGD("FLOPPY: Terminal count reached in the middle of a data transfer! Position: %i/%i bytes",FLOPPY.databufferposition,FLOPPY.databuffersize)
							floppy_executeData(); //Execute the command with the given data!
						}
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
			switch (FLOPPY.commandbuffer[0]) //What command?
			{
				case READ_TRACK: //Read complete track
				case WRITE_DATA: //Write sector
				case READ_DATA: //Read sector
				case WRITE_DELETED_DATA: //Write deleted sector
				case READ_DELETED_DATA: //Read deleted sector
				case FORMAT_TRACK: //Format sector
				case SPECIFY: //Fix drive data
				case SENSE_DRIVE_STATUS: //Check drive status
				case RECALIBRATE: //Calibrate drive
				case SENSE_INTERRUPT: //Check interrupt status
				case READ_ID: //Read sector ID
				case SEEK: //Seek/park head
				case VERSION: //Version information!
				case LOCK: //Lock command?
					FLOPPY_LOGD("FLOPPY: Reading result byte %i/%i=%02X",FLOPPY.resultposition,resultlength[FLOPPY.commandbuffer[0]&0x1F],temp)
					if (FLOPPY.resultposition>=resultlength[FLOPPY.commandbuffer[0]]) //Result finished?
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
		case 0xFF: //Error or reset result
			FLOPPY.commandstep = 0; //Reset step!
			return FLOPPY.ST0.data; //Give ST0, containing an error!
			break;
		default:
			break; //Unknown status, hang the controller!
	}
	return ~0; //Not used yet!
}

byte getfloppydisktype(byte floppy)
{
	if (FLOPPY.geometries[floppy]) //Gotten a known geometry?
	{
		return FLOPPY.geometries[floppy]->boardjumpersetting; //Our board jumper settings for this drive!
	}
	return 2; //Default to 2.8MB to fit all!
}

byte PORT_IN_floppy(word port, byte *result)
{
	if ((port&~7) != 0x3F0) return 0; //Not our port range!
	byte temp;
	switch (port & 0x7) //What port?
	{
	case 0: //diskette EHD controller board jumper settings (82072AA)!
		//Officially only on AT systems, but use on XT as well for proper detection!
		//Create floppy flags!
		temp = getfloppydisktype(3); //Floppy #3!
		temp <<= 2;
		temp = getfloppydisktype(2); //Floppy #2!
		temp <<= 2;
		temp = getfloppydisktype(1); //Floppy #1!
		temp <<= 2;
		temp = getfloppydisktype(0); //Floppy #0!
		FLOPPY_LOGD("FLOPPY: Read port Diskette EHD controller board jumper settings=%02X",temp);
		*result = temp; //Give the result!
		return 1; //Used!
		break;
	case 2: //DOR?
		*result = FLOPPY.DOR.data; //Give the DOR!
		return 1; //Used!
		break;
	case 3: //Tape Drive register (82077AA)?
		temp = 0x20; //No drive present here by default!
		if (FLOPPY.geometries[FLOPPY.DOR.DriveNumber]) //Nothing there?
		{
			temp = FLOPPY.geometries[FLOPPY.DOR.DriveNumber]->TapeDriveRegister; //What format are we?
		}
		FLOPPY_LOGD("FLOPPY: Read port Tape Drive Register=%02X",temp);
		*result = temp; //Give the result!
		return 1; //Used!
		break;
	case 4: //MSR?
		updateFloppyMSR(); //Update the MSR with current values!
		FLOPPY_LOGD("FLOPPY: Read MSR=%02X",FLOPPY.MSR.data)
		*result = FLOPPY.MSR.data; //Give MSR!
		return 1;
	case 5: //Data?
		//Process data!
		*result = floppy_readData(); //Read data!
		return 1;
	case 7: //DIR?
		if (EMULATED_CPU>=CPU_80286) //AT?
		{
			updateFloppyDIR(); //Update the DIR register!
			FLOPPY_LOGD("FLOPPY: Read DIR=%02X", FLOPPY.DIR.data)
			*result = FLOPPY.DIR.data; //Give DIR!
			return 1;
		}
		break;
	default: //Unknown port?
		break;
	}
	//Not one of our ports?
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
		FLOPPY_LOGD("FLOPPY: Write DOR=%02X", value)
		FLOPPY.DOR.data = value; //Write to register!
		updateMotorControl(); //Update the motor control!
		FLOPPY_handlereset(0); //Execute a reset by DOR!
		return 1; //Finished!
	case 4: //DSR?
		if (EMULATED_CPU>=CPU_80286) //AT?
		{
			FLOPPY_LOGD("FLOPPY: Write DSR=%02X", value)
			FLOPPY.DSR.data = value; //Write to register to check for reset first!
			FLOPPY_handlereset(1); //Execute a reset by DSR!
			if (FLOPPY.DSR.SWReset) FLOPPY.DSR.SWReset = 0; //Reset requested? Clear the reset bit automatically!
			FLOPPY_handlereset(1); //Execute a reset by DSR if needed!
			FLOPPY.CCR.rate = FLOPPY.DSR.DRATESEL; //Setting one sets the other!
			return 1; //Finished!
		}
	case 5: //Data?
		floppy_writeData(value); //Write data!
		return 1; //Default handler!
	case 7: //CCR?
		if (EMULATED_CPU>=CPU_80286) //AT?
		{
			FLOPPY_LOGD("FLOPPY: Write CCR=%02X", value)
			FLOPPY.CCR.data = value; //Set CCR!
			FLOPPY.DSR.DRATESEL = FLOPPY.CCR.rate; //Setting one sets the other!
			return 1;
		}
		break;
	default: //Unknown port?
		break; //Unknown port!
	}
	//Not one of our ports!
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
	DMA_SetDREQ(FLOPPY_DMA, (FLOPPY.commandstep == 2) && FLOPPY_useDMA()); //Set DREQ from hardware when in the data phase and using DMA transfers and not busy yet(pending)!
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
	density_forced = EMULATED_CPU>=CPU_80286; //Allow force density check if 286+ (non XT)!
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