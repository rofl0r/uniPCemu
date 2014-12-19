#include "headers/types.h" //Basic types!
#include "headers/cpu/cpu.h" //For handler!
#include "headers/cpu/callback.h" //Typedefs!

extern byte EMU_BIOS[0x10000]; //Full custom BIOS from 0xF0000-0xFFFFF for the emulator itself to use!

Handler CBHandlers[CB_MAX]; //Handlers!
byte CBTypes[CB_MAX]; //Types!
struct
{
	byte handlernr; //Current handle number!
	byte hascallback;
} currentcallback; //Current callback!

void CB_DataHandler() {} //Data handler dummy for callbacks with Data only!

byte CB_callback = 0; //Default: no callback!
void CB_SetCallback(byte isone)
{
	CB_callback = isone; //Set on/off!
}

byte CB_ISCallback()
{
	return CB_callback?1:0; //Callback or not!
}

void CB_handler(byte handlernr) //Call an handler (from CB_Handler)?
{
	currentcallback.hascallback = 1; //Have callback!
	currentcallback.handlernr = handlernr; //The handle to process!
}

void CB_handleCallbacks() //Handle callbacks after CPU usage!
{
	if (currentcallback.hascallback) //Valid set?
	{
		if (((CBTypes[currentcallback.handlernr]==CB_INTERRUPT) ||( CBTypes[currentcallback.handlernr]==CB_UNASSIGNEDINTERRUPT)) //Any kind of registerd interrupt?
			&& CBHandlers[currentcallback.handlernr]) //Set?
		{
			currentcallback.hascallback = 0; //Reset to not used: we're being handled!
			CB_SetCallback(1); //Callback!
			CBHandlers[currentcallback.handlernr](); //Run the handler!
			CB_SetCallback(0); //Not anymore!
		}
	}
}

void clearCBHandlers() //Reset callbacks!
{
	int curhandler;
	for (curhandler=0; curhandler<CB_MAX; curhandler++) //Process all handlers!
	{
		CBHandlers[curhandler] = NULL; //Reset all handlers!
	}
	memset(CBTypes,0,sizeof(CBTypes)); //Init types to unused!
	currentcallback.hascallback = 0; //Reset use of callbacks by default!
}

word CB_datasegment; //Reserved segment when adding callback!
word CB_dataoffset; //Reserved offset when adding callback!
word CB_realoffset; //Real offset we're loaded at within the custom BIOS!

void addCBHandler(byte type, Handler CBhandler, byte intnr) //Add a callback!
{
	if ((CBhandler==NULL || !CBhandler) && (type==CB_INTERRUPT)) return; //Don't accept NULL INTERRUPT!
	word offset;

	byte curhandler;
	int found;
	found = 0;
	for (curhandler=0; curhandler<CB_MAX; curhandler++) //Check for new handler!
	{
		if (!CBTypes[curhandler]) //Unset?
		{
			found = 1; //Valid!
			break;
		}
	}

	if (!found) //Not empty found?
	{
		return; //Don't add: no handlers left!
	}

	offset = CB_SOFFSET+(curhandler*CB_SIZE); //Start of callback!
	
	CB_realoffset = offset-CB_SOFFSET+CB_BASE; //Real offset within the custom BIOS!

	if (type!=CB_DATA) //Procedure used?
	{
		CBHandlers[curhandler] = CBhandler; //Set our handler!
	}
	else //Data?
	{
		CBHandlers[curhandler] = &CB_DataHandler; //We're data only!
	}
//Now the handler and type!
	CBTypes[curhandler] = type; //Set the type we're using!
	switch (type)
	{
	case CB_UNASSIGNEDINTERRUPT: //Same as below, but unassigned to an interrupt!
	case CB_INTERRUPT: //Interrupt call?
		//First: add to jmptbl!
		CB_datasegment = CB_SEG; //Segment of data allocated!
		CB_dataoffset = offset; //Start of data!
		if (type!=CB_UNASSIGNEDINTERRUPT) //Not unassigned?
		{
			CPU_setint(intnr,CB_datasegment,CB_dataoffset); //Set the interrupt!
		}

		word dataoffset = CB_realoffset; //Load the real offset for usage!
		
		//Next, our interrupt handler:
		EMU_BIOS[dataoffset++] = 0xFE; //OpCode FE: Special case!

		EMU_BIOS[dataoffset++] = 0x38; //Special case: call internal interrupt number!
		EMU_BIOS[dataoffset++] = curhandler&0xFF; //Call our (interrupt) handler?
		EMU_BIOS[dataoffset++] = (curhandler>>8)&0xFF;

		//Finally, RETI!
		EMU_BIOS[dataoffset++] = 0xCF; //RETI!
		break;
	case CB_IRET: //Simple IRET handler (empty filler?)
		//First: add to jmptbl!
		CB_datasegment = CB_SEG; //Segment of data allocated!
		CB_dataoffset = offset; //Start of data!
		CPU_setint(intnr,CB_datasegment,CB_dataoffset);

		EMU_BIOS[CB_realoffset] = 0xCF; //Simple IRET!
		break;
	case CB_DATA: //Data/custom code only?
		CB_datasegment = CB_SEG; //Segment of data allocated!
		CB_dataoffset = offset; //Start of data!
		break;
	default: //Default: unsupported!
		break;
	}
}