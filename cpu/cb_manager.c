#include "headers/types.h" //Basic types!
#include "headers/cpu/cpu.h" //For handler!
#include "headers/cpu/cb_manager.h" //Typedefs!
#include "headers/cpu/easyregs.h" //Easy register support for DOSBox!

#include "headers/interrupts/interrupt16.h" //For Dosbox compatibility.
#include "headers/interrupts/interrupt10.h" //For Video BIOS compatibility.

extern byte EMU_BIOS[0x10000]; //Full custom BIOS from 0xF0000-0xFFFFF for the emulator itself to use!
extern byte EMU_VGAROM[0x8000]; //Full VGA BIOS from 0xC0000-0xC8000 for the emulator and normal BIOS to use!

extern Int10Data int10; //Our VGA ROM data!

extern byte EMU_RUNNING;

Handler CBHandlers[CB_MAX]; //Handlers!
byte CBTypes[CB_MAX]; //Types!
struct
{
	word handlernr; //Current handle number!
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

void CB_handler(word handlernr) //Call an handler (from CB_Handler)?
{
	currentcallback.hascallback = 1; //Have callback!
	currentcallback.handlernr = handlernr; //The handle to process!
}

byte callbackzero = 0; //Zero callback?

void CB_handleCallbacks() //Handle callbacks after CPU usage!
{
	if (currentcallback.hascallback) //Valid set?
	{
		if (!currentcallback.handlernr) //Special handler?
		{
			callbackzero = 1; //We're a zero callback!
		}
		else
		{
			if (currentcallback.handlernr < NUMITEMS(CBHandlers)) //Do we have a handler in range to execute?
			{
				if (CBHandlers[currentcallback.handlernr]) //Gotten a handler set?
				{
					currentcallback.hascallback = 0; //Reset to not used: we're being handled!
					CB_SetCallback(1); //Callback!
					CBHandlers[currentcallback.handlernr](); //Run the handler!
					CB_SetCallback(0); //Not anymore!
				}
			}
			callbackzero = 0; //Not a zero callback anymore!
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

void write_VGAw(uint_32 offset, word value)
{
	EMU_VGAROM[offset] = value & 0xFF; //Low byte!
	EMU_VGAROM[offset + 1] = (value >> 8); //High byte!
}

void write_BIOSw(uint_32 offset, word value)
{
	EMU_BIOS[offset] = value&0xFF; //Low byte!
	EMU_BIOS[offset+1] = (value>>8); //High byte!
}

#define Bit8u byte
#define Bit16u word

#define incoffset (dataoffset++&0xFFFF)

//In the case of CB_INTERRUPT, how much to add to the address to get the CALL version
#define CB_INTERRUPT_CALLSIZE 10

void CB_createlongjmp(word entrypoint, word segment, word offset) //Create an alias for BIOS compatibility!
{
	EMU_BIOS[entrypoint++] = 0x9A; //CALL!
	offset += CB_INTERRUPT_CALLSIZE; //We're a CALL!
	EMU_BIOS[entrypoint++] = (offset & 0xFF); //Low!
	EMU_BIOS[entrypoint++] = ((offset >> 8) & 0xFF); //High!
	EMU_BIOS[entrypoint++] = (segment & 0xFF); //Low!
	EMU_BIOS[entrypoint++] = ((segment>>8) & 0xFF); //High!
	EMU_BIOS[entrypoint++] = 0xCB; //RETF
}

void CB_updatevectoroffsets(uint_32 intnr, word offset)
{
	word entrypoint = 0xfef3 + (intnr<<1); //Our entry point!
	if (entrypoint < 0xff52) //Within range of the IVT?
	{
		EMU_BIOS[entrypoint++] = (offset & 0xFF); //Low bits!
		offset >>= 8; //Shift to low side!
		EMU_BIOS[entrypoint] = (offset & 0xFF); //High bits!
	}
}

void addCBHandler(byte type, Handler CBhandler, uint_32 intnr) //Add a callback!
{
	if ((CBhandler==NULL || !CBhandler) && (type==CB_INTERRUPT)) return; //Don't accept NULL INTERRUPT!
	word offset;

	word curhandler;
	int found;
	found = 0;
	for (curhandler=1; curhandler<CB_MAX; curhandler++) //Check for new handler! #0 is reserved for a special operation!
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

	switch (type) //Extra handlers?
	{
		case CB_VIDEOINTERRUPT: //Video interrupt, unassigned!
			CB_datasegment = 0xC000;
			CB_dataoffset = int10.rom.used; //Entry point the end of the VGA BIOS!
			CB_realoffset = CB_dataoffset; //Offset within the custom bios is this!
			break;
		case CB_VIDEOENTRY: //Video BIOS entry interrupt, unassigned!
			CB_datasegment = 0xC000;
			CB_dataoffset = 0x0003; //Entry point in the VGA BIOS for the hooking of our interrupts!
			CB_realoffset = CB_dataoffset; //Offset within the custom bios is this!
			break;
		//Dosbox stuff!
		case CB_DOSBOX_IRQ0:
		case CB_DOSBOX_IRQ1:
		case CB_DOSBOX_IRQ9:
		case CB_DOSBOX_IRQ12:
		case CB_DOSBOX_IRQ12_RET:
			CB_datasegment = (intnr>>16);
			CB_dataoffset = (intnr&0xFFFF);
			CB_realoffset = CB_dataoffset; //Offset within the custom bios is this!
			break;
		case CB_IRET: //IRET?
			CB_datasegment = CB_SEG;
			CB_dataoffset = 0xff53;
			CB_realoffset = CB_dataoffset; //We're forced to F000:FF53!
			break;
		default: //Original offset method?
			//Real offset is already set
			CB_datasegment = CB_SEG; //Segment of data allocated!
			CB_dataoffset = offset; //Start of data/code!
			break;
	}

	word dataoffset = CB_realoffset; //Load the real offset for usage by default!

	if ((type == CB_DOSBOX_INT16) || (type == CB_DOSBOX_MOUSE)) //We need to set the interrupt vector too?
	{
		Dosbox_RealSetVec(0x16, (CB_datasegment<<16)|CB_dataoffset); //Use intnr to set the interrupt vector!
	}

	//Now process our compatibility layer for direct calls!
	switch (type) //What type?
	{
	case CB_DATA:
		break; //Data has nothing special!
	case CB_UNASSIGNEDINTERRUPT: //Unassigned interrupt?
	case CB_IRET: //Interrupt return?
	case CB_INTERRUPT: //Normal BIOS interrupt?
	case CB_VIDEOINTERRUPT: //Video (interrupt 10h) interrupt?
		switch (intnr) //What interrupt?
		{
		case 0x15:
			CB_createlongjmp(0xf859, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! INT 15h System Services Entry Point
			break;
		case 0x14: //INT14?
			CB_createlongjmp(0xe739, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! INT 14h Serial Communications Service Entry Point
			break;
		case 0x17: //INT17?
			CB_createlongjmp(0xefd2, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! INT 17h Printer Service Entry Point
			break;
		case 0x13: //INT13?
			CB_createlongjmp(0xe3fe, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! INT 13h Fixed Disk Services Entry Point
			CB_createlongjmp(0xec59, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! INT 13h Diskette Service Entry Point
			break;
		case 0x10: //Video services?
			CB_createlongjmp(0xf045, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! INT 10 Functions 0-Fh Entry Point
			CB_createlongjmp(0xf065, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! INT 10h Video Support Service Entry Point
			break;
		case 0x12: //memory size services?
			CB_createlongjmp(0xf841, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! INT 12h Memory Size Service Entry Point
			break;
		case 0x11: //Equipment list service?
			CB_createlongjmp(0xf84d, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! INT 11h Equipment List Service Entry Point
			break;
		case 0x05:
			CB_createlongjmp(0xff54, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! INT 11h Equipment List Service Entry Point
			break;
		case 0x19:
			CB_createlongjmp(0xe05b, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! POST Entry point
			CB_createlongjmp(0xe6f2, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! INT 19h Boot Load Service Entry Point
			CB_createlongjmp(0xfff0, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! Power-up Entry Point
			break;
		case 0x1A:
			CB_createlongjmp(0xfe6e, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! Power-up Entry Point			
			break;
		default: //Custom interrupt (NON original BIOS)?
			break;
		}
		CB_updatevectoroffsets(intnr, CB_dataoffset); //Update default IVT offsets!
		break;
	case CB_DOSBOX_IRQ0:
		CB_createlongjmp(0xfea5, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! INT 08h System Timer ISR Entry Point
		CB_updatevectoroffsets(intnr, CB_dataoffset); //Update default IVT offsets!
		break;
	case CB_DOSBOX_IRQ1:
		CB_createlongjmp(0xe987, CB_datasegment, CB_dataoffset); //Create a JMP here where software can expect it! INT 09h Keyboard Service Entry Point
		CB_updatevectoroffsets(intnr, CB_dataoffset); //Update default IVT offsets!
		break;
	case CB_DOSBOX_INT16:
		CB_createlongjmp(0xe82e, CB_datasegment, CB_dataoffset); //INT 16h Keyboard Service Entry Point
		CB_updatevectoroffsets(intnr, CB_dataoffset); //Update default IVT offsets!
		break;
	default: //Unknown special code?
		break;
	}

	switch (type)
	{
	case CB_VIDEOINTERRUPT:
	case CB_VIDEOENTRY:
		EMU_VGAROM[incoffset] = 0xFE; //OpCode FE: Special case!
		EMU_VGAROM[incoffset] = 0x38; //Special case: call internal interrupt number!
		EMU_VGAROM[incoffset] = 0; //Call our (interrupt) handler?
		EMU_VGAROM[incoffset] = 0; //We're a zero callback!

		EMU_VGAROM[incoffset] = 0x9A; //CALL ...
		write_VGAw(incoffset, CB_dataoffset + CB_INTERRUPT_CALLSIZE); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		write_VGAw(incoffset, CB_datasegment); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		if (type == CB_VIDEOENTRY) //Video entry point call?
		{
			EMU_VGAROM[incoffset] = 0xCB; //RETF!
		}
		else //Normal interrupt?
		{
			EMU_VGAROM[incoffset] = 0xCF; //RETI: We're an interrupt handler!
		}

		//Next, our handler as a simple FAR CALL function.
		EMU_VGAROM[incoffset] = 0xFE; //OpCode FE: Special case!

		EMU_VGAROM[incoffset] = 0x38; //Special case: call internal interrupt number!
		EMU_VGAROM[incoffset] = curhandler & 0xFF; //Call our (interrupt) handler?
		EMU_VGAROM[incoffset] = (curhandler >> 8) & 0xFF;

		EMU_VGAROM[incoffset] = 0xCB; //RETF!
		break;
	case CB_UNASSIGNEDINTERRUPT: //Same as below, but unassigned to an interrupt!
	case CB_INTERRUPT: //Interrupt call?
		//First: add to jmptbl!
		if (type!=CB_UNASSIGNEDINTERRUPT) //Not unassigned?
		{
			CPU_setint(intnr,CB_datasegment,CB_dataoffset); //Set the interrupt!
		}
		EMU_BIOS[incoffset] = 0xFE; //OpCode FE: Special case!
		EMU_BIOS[incoffset] = 0x38; //Special case: call internal interrupt number!
		EMU_BIOS[incoffset] = 0; //Call our (interrupt) handler?
		EMU_BIOS[incoffset] = 0; //We're a zero callback!

		EMU_BIOS[incoffset] = 0x9A; //CALL ...
		write_BIOSw(incoffset, CB_dataoffset+CB_INTERRUPT_CALLSIZE); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		write_BIOSw(incoffset, CB_datasegment); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		EMU_BIOS[incoffset] = 0xCF; //RETI: We're an interrupt handler!

		//Next, our handler as a simple FAR CALL function.
		EMU_BIOS[incoffset] = 0xFE; //OpCode FE: Special case!

		EMU_BIOS[incoffset] = 0x38; //Special case: call internal interrupt number!
		EMU_BIOS[incoffset] = curhandler&0xFF; //Call our (interrupt) handler?
		EMU_BIOS[incoffset] = (curhandler>>8)&0xFF;

		if (intnr == 0x18) //Special data insertion: interrupt 18h!
		{
			EMU_BIOS[incoffset] = 0xCD;
			EMU_BIOS[incoffset] = 0x19; //INT 19h
		}

		EMU_BIOS[incoffset] = 0xCB; //RETF!

		//Finally, return!
		break;
	case CB_IRET: //Simple IRET handler (empty filler?)
		//First: add to jmptbl!
		CPU_setint(intnr,CB_datasegment,CB_dataoffset);

		EMU_BIOS[CB_realoffset] = 0xCF; //Simple IRET!
		break;
	case CB_DATA: //Data/custom code only?
		break;

	//Handlers from DOSBox!
	case CB_DOSBOX_IRQ0:	// timer int8
		EMU_BIOS[incoffset] = 0xFE; //OpCode FE: Special case!
		EMU_BIOS[incoffset] = 0x38; //Special case: call internal interrupt number!
		EMU_BIOS[incoffset] = 0; //Call our (interrupt) handler?
		EMU_BIOS[incoffset] = 0; //We're a zero callback!

		EMU_BIOS[incoffset] = 0x9A; //CALL ...
		write_BIOSw(incoffset, CB_dataoffset + CB_INTERRUPT_CALLSIZE); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		write_BIOSw(incoffset, CB_datasegment); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		EMU_BIOS[incoffset] = 0xCF; //RETI: We're an interrupt handler!

		//if (use_cb) {
			EMU_BIOS[incoffset] = (Bit8u)0xFE;	//GRP 4
			EMU_BIOS[incoffset] = (Bit8u)0x38;	//Extra Callback instruction
			write_BIOSw(incoffset,(Bit16u)curhandler);		//The immediate word
			dataoffset+=4;
		//}
		EMU_BIOS[incoffset] =(Bit8u)0x50;		// push ax
		EMU_BIOS[incoffset] =(Bit8u)0x52;		// push dx
		EMU_BIOS[incoffset] =(Bit8u)0x1e;		// push ds
		write_BIOSw(incoffset,(Bit16u)0x1ccd);	// int 1c
		++dataoffset;
		EMU_BIOS[incoffset] =(Bit8u)0xfa;		// cli
		EMU_BIOS[incoffset] =(Bit8u)0x1f;		// pop ds
		EMU_BIOS[incoffset] =(Bit8u)0x5a;		// pop dx
		write_BIOSw(incoffset,(Bit16u)0x20b0);	// mov al, 0x20
		++dataoffset;
		write_BIOSw(incoffset,(Bit16u)0x20e6);	// out 0x20, al
		++dataoffset;
		EMU_BIOS[incoffset] =(Bit8u)0x58;		// pop ax
		EMU_BIOS[incoffset] =(Bit8u)0xcb;		//An RETF Instruction
		//return (use_cb?0x12:0x0e);
		break;
	case CB_DOSBOX_IRQ1:	// keyboard int9
		EMU_BIOS[incoffset] = 0xFE; //OpCode FE: Special case!
		EMU_BIOS[incoffset] = 0x38; //Special case: call internal interrupt number!
		EMU_BIOS[incoffset] = 0; //Call our (interrupt) handler?
		EMU_BIOS[incoffset] = 0; //We're a zero callback!

		EMU_BIOS[incoffset] = 0x9A; //CALL ...
		write_BIOSw(incoffset, CB_dataoffset + CB_INTERRUPT_CALLSIZE); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		write_BIOSw(incoffset, CB_datasegment); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		EMU_BIOS[incoffset] = 0xCF; //RETI: We're an interrupt handler!

		EMU_BIOS[incoffset] = (Bit8u)0x50;			// push ax
		write_BIOSw(incoffset,(Bit16u)0x60e4);		// in al, 0x60
		++dataoffset;
		write_BIOSw(incoffset,(Bit16u)0x4fb4);		// mov ah, 0x4f
		++dataoffset;
		EMU_BIOS[incoffset] =(Bit8u)0xf9;			// stc
		write_BIOSw(incoffset,(Bit16u)0x15cd);		// int 15
		++dataoffset;
		//if (use_cb) {
			write_BIOSw(incoffset,(Bit16u)0x0473);	// jc skip
			++dataoffset;
			EMU_BIOS[incoffset] =(Bit8u)0xFE;		//GRP 4
			EMU_BIOS[incoffset] =(Bit8u)0x38;		//Extra Callback instruction
			write_BIOSw(incoffset,(Bit16u)curhandler);			//The immediate word
			++dataoffset;
			// jump here to (skip):
		//}
		EMU_BIOS[incoffset] =(Bit8u)0xfa;			// cli
		write_BIOSw(incoffset,(Bit16u)0x20b0);		// mov al, 0x20
		++dataoffset;
		write_BIOSw(incoffset,(Bit16u)0x20e6);		// out 0x20, al
		++dataoffset;
		EMU_BIOS[incoffset] =(Bit8u)0x58;			// pop ax
		EMU_BIOS[incoffset] =(Bit8u)0xcb;			//An RETF Instruction
		//return (use_cb?0x15:0x0f);
		break;
	case CB_DOSBOX_IRQ9:	// pic cascade interrupt
		EMU_BIOS[incoffset] = 0xFE; //OpCode FE: Special case!
		EMU_BIOS[incoffset] = 0x38; //Special case: call internal interrupt number!
		EMU_BIOS[incoffset] = 0; //Call our (interrupt) handler?
		EMU_BIOS[incoffset] = 0; //We're a zero callback!

		EMU_BIOS[incoffset] = 0x9A; //CALL ...
		write_BIOSw(incoffset, CB_dataoffset + CB_INTERRUPT_CALLSIZE); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		write_BIOSw(incoffset, CB_datasegment); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		EMU_BIOS[incoffset] = 0xCF; //RETI: We're an interrupt handler!

		//if (use_cb) {
			EMU_BIOS[incoffset] =(Bit8u)0xFE;	//GRP 4
			EMU_BIOS[incoffset] =(Bit8u)0x38;	//Extra Callback instruction
			write_BIOSw(incoffset,(Bit16u)curhandler);		//The immediate word
			++dataoffset;
			//dataoffset+=4;
		//}
		EMU_BIOS[incoffset] =(Bit8u)0x50;		// push ax
		write_BIOSw(incoffset,(Bit16u)0x61b0);	// mov al, 0x61
		++dataoffset;
		write_BIOSw(incoffset,(Bit16u)0xa0e6);	// out 0xa0, al
		++dataoffset;
		write_BIOSw(incoffset,(Bit16u)0x0acd);	// int a
		++dataoffset;
		EMU_BIOS[incoffset] =(Bit8u)0xfa;		// cli
		EMU_BIOS[incoffset] =(Bit8u)0x58;		// pop ax
		EMU_BIOS[incoffset] =(Bit8u)0xcb;		//An RETF Instruction
		//return (use_cb?0x0e:0x0a);
		break;
	case CB_DOSBOX_IRQ12:	// ps2 mouse int74
		EMU_BIOS[incoffset] = 0xFE; //OpCode FE: Special case!
		EMU_BIOS[incoffset] = 0x38; //Special case: call internal interrupt number!
		EMU_BIOS[incoffset] = 0; //Call our (interrupt) handler?
		EMU_BIOS[incoffset] = 0; //We're a zero callback!

		EMU_BIOS[incoffset] = 0x9A; //CALL ...
		write_BIOSw(incoffset, CB_dataoffset + CB_INTERRUPT_CALLSIZE); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		write_BIOSw(incoffset, CB_datasegment); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		EMU_BIOS[incoffset] = 0xCF; //RETI: We're an interrupt handler!

		//if (!use_cb) E_Exit("int74 callback must implement a callback handler!");
		EMU_BIOS[incoffset] =(Bit8u)0x1e;		// push ds
		EMU_BIOS[incoffset] =(Bit8u)0x06;		// push es
		write_BIOSw(incoffset,(Bit16u)0x6066);	// pushad
		++dataoffset;
		EMU_BIOS[incoffset] =(Bit8u)0xfc;		// cld
		EMU_BIOS[incoffset] =(Bit8u)0xfb;		// sti
		EMU_BIOS[incoffset] =(Bit8u)0xFE;		//GRP 4
		EMU_BIOS[incoffset] =(Bit8u)0x38;		//Extra Callback instruction
		write_BIOSw(incoffset,(Bit16u)curhandler);			//The immediate word
		++dataoffset;
		EMU_BIOS[incoffset] = (Bit8u)0xcb;		//An RETF Instruction
		//return 0x0a;
		break;
	case CB_DOSBOX_IRQ12_RET:	// ps2 mouse int74 return
		EMU_BIOS[incoffset] = 0xFE; //OpCode FE: Special case!
		EMU_BIOS[incoffset] = 0x38; //Special case: call internal interrupt number!
		EMU_BIOS[incoffset] = 0; //Call our (interrupt) handler?
		EMU_BIOS[incoffset] = 0; //We're a zero callback!

		EMU_BIOS[incoffset] = 0x9A; //CALL ...
		write_BIOSw(incoffset, CB_dataoffset + CB_INTERRUPT_CALLSIZE); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		write_BIOSw(incoffset, CB_datasegment); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		EMU_BIOS[incoffset] = 0xCF; //RETI: We're an interrupt handler!

		//if (use_cb) {
			EMU_BIOS[incoffset] =(Bit8u)0xFE;	//GRP 4
			EMU_BIOS[incoffset] =(Bit8u)0x38;	//Extra Callback instruction
			write_BIOSw(incoffset,(Bit16u)curhandler);		//The immediate word
			++dataoffset;
			//dataoffset+=4;
		//}
		EMU_BIOS[incoffset] =(Bit8u)0xfa;		// cli
		write_BIOSw(incoffset,(Bit16u)0x20b0);	// mov al, 0x20
		++dataoffset;
		write_BIOSw(incoffset,(Bit16u)0xa0e6);	// out 0xa0, al
		++dataoffset;
		write_BIOSw(incoffset,(Bit16u)0x20e6);	// out 0x20, al
		++dataoffset;
		write_BIOSw(incoffset,(Bit16u)0x6166);	// popad
		++dataoffset;
		EMU_BIOS[incoffset] =(Bit8u)0x07;		// pop es
		EMU_BIOS[incoffset] =(Bit8u)0x1f;		// pop ds
		EMU_BIOS[incoffset] =(Bit8u)0xcb;		//An RETF Instruction
		//return (use_cb?0x10:0x0c);
		break;
	/*case CB_DOSBOX_IRQ6_PCJR:	// pcjr keyboard interrupt
		EMU_BIOS[dataoffset+0x00,(Bit8u)0x50);			// push ax
		write_BIOSw(dataoffset+0x01,(Bit16u)0x60e4);		// in al, 0x60
		write_BIOSw(dataoffset+0x03,(Bit16u)0xe03c);		// cmp al, 0xe0
		if (use_cb) {
			write_BIOSw(dataoffset+0x05,(Bit16u)0x0674);	// je skip
			EMU_BIOS[dataoffset+0x07,(Bit8u)0xFE);		//GRP 4
			EMU_BIOS[dataoffset+0x08,(Bit8u)0x38);		//Extra Callback instruction
			write_BIOSw(dataoffset+0x09,(Bit16u)curhandler);			//The immediate word
			physAddress+=4;
		} else {
			write_BIOSw(dataoffset+0x05,(Bit16u)0x0274);	// je skip
		}
		write_BIOSw(dataoffset+0x07,(Bit16u)0x09cd);		// int 9
		// jump here to (skip):
		EMU_BIOS[dataoffset+0x09,(Bit8u)0xfa);			// cli
		write_BIOSw(dataoffset+0x0a,(Bit16u)0x20b0);		// mov al, 0x20
		write_BIOSw(dataoffset+0x0c,(Bit16u)0x20e6);		// out 0x20, al
		EMU_BIOS[dataoffset+0x0e,(Bit8u)0x58);			// pop ax
		EMU_BIOS[dataoffset+0x0f,(Bit8u)0xcf);			//An IRET Instruction
		//return (use_cb?0x14:0x10);
		break;
	*/
	case CB_DOSBOX_MOUSE:
		EMU_BIOS[incoffset] = 0xFE; //OpCode FE: Special case!
		EMU_BIOS[incoffset] = 0x38; //Special case: call internal interrupt number!
		EMU_BIOS[incoffset] = 0; //Call our (interrupt) handler?
		EMU_BIOS[incoffset] = 0; //We're a zero callback!

		EMU_BIOS[incoffset] = 0x9A; //CALL ...
		write_BIOSw(incoffset, CB_dataoffset + CB_INTERRUPT_CALLSIZE); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		write_BIOSw(incoffset, CB_datasegment); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		EMU_BIOS[incoffset] = 0xCF; //RETI: We're an interrupt handler!

		write_BIOSw(incoffset, (Bit16u)0x07eb);		// jmp i33hd
		dataoffset+=7; //9-word=9-2=7.
		// jump here to (i33hd):
		//if (use_cb) {
			EMU_BIOS[incoffset] =(Bit8u)0xFE;	//GRP 4
			EMU_BIOS[incoffset] =(Bit8u)0x38;	//Extra Callback instruction
			write_BIOSw(incoffset,(Bit16u)curhandler);		//The immediate word
			++dataoffset;
			//dataoffset+=4;
		//}
		EMU_BIOS[incoffset] =(Bit8u)0xCB;		//An IRET Instruction
		//return (use_cb?0x0e:0x0a);
		break;
	case CB_DOSBOX_INT16:
		EMU_BIOS[incoffset] = 0xFE; //OpCode FE: Special case!
		EMU_BIOS[incoffset] = 0x38; //Special case: call internal interrupt number!
		EMU_BIOS[incoffset] = 0; //Call our (interrupt) handler?
		EMU_BIOS[incoffset] = 0; //We're a zero callback!

		EMU_BIOS[incoffset] = 0x9A; //CALL ...
		write_BIOSw(incoffset, CB_dataoffset + CB_INTERRUPT_CALLSIZE); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		write_BIOSw(incoffset, CB_datasegment); //... Our interrupt handler, as a function call!
		++dataoffset; //Word address!
		EMU_BIOS[incoffset] = 0xCF; //RETI: We're an interrupt handler!

		EMU_BIOS[incoffset] = (Bit8u)0xFB;		//STI
		//if (use_cb) {
		byte i;
			EMU_BIOS[incoffset] = (Bit8u)0xFE;	//GRP 4
			EMU_BIOS[incoffset] =(Bit8u)0x38;	//Extra Callback instruction
			write_BIOSw(incoffset,(Bit16u)curhandler);	//The immediate word
			++dataoffset;
			//dataoffset+=4;
		//}
			EMU_BIOS[incoffset] = (Bit8u)0xCB;		//An RETF Instruction
			for (i = 0; i <= 0x0b; i++) EMU_BIOS[incoffset] = 0x90;
			write_BIOSw(incoffset, (Bit16u)0xedeb);	//jmp callback
			++dataoffset;
			EMU_BIOS[incoffset] = (Bit8u)0xCF;		//An IRET Instruction
		//return (use_cb?0x10:0x0c);
		break;


	default: //Default: unsupported!
		break;
	}
}

//Flag set/reset for interrupts called by callbacks. Compatible with both callback calls and normal calls.

uint_32 FLAGS_offset()
{
	return REG_SP+4+(callbackzero<<2); //Apply zero callback if needed (call adds 4 bytes to stack)!
}

void CALLBACK_SZF(byte val) {
	uint_32 flags;
	if (EMU_RUNNING == 1)
	{
		flags = REG_EFLAGS; //Read flags!
		REG_EFLAGS = MMU_rw(CPU_SEGMENT_SS, REG_SS, FLAGS_offset(), 0);
	}
	if (val) FLAG_ZF = 1;
	else FLAG_ZF = 0; 
	if (EMU_RUNNING==1)
	{
		MMU_ww(CPU_SEGMENT_SS, REG_SS, FLAGS_offset(), REG_EFLAGS);
		REG_EFLAGS = flags; //Restore!
	}
}

void CALLBACK_SCF(byte val) {
	uint_32 flags;
	if (EMU_RUNNING == 1)
	{
		flags = REG_EFLAGS; //Read flags!
		REG_EFLAGS = MMU_rw(CPU_SEGMENT_SS, REG_SS, FLAGS_offset(), 0);
	}
	if (val) FLAG_CF = 1;
	else FLAG_CF = 0; 
	if (EMU_RUNNING==1)
	{
		MMU_ww(CPU_SEGMENT_SS, REG_SS, FLAGS_offset(), REG_EFLAGS);
		REG_EFLAGS = flags; //Restore!
	}
}

void CALLBACK_SIF(byte val) {
	uint_32 flags;
	if (EMU_RUNNING == 1)
	{
		flags = REG_EFLAGS; //Read flags!
		REG_EFLAGS = MMU_rw(CPU_SEGMENT_SS, REG_SS, FLAGS_offset(), 0);
	}
	if (val) FLAG_IF = 1;
	else FLAG_IF = 0; 
	if (EMU_RUNNING==1)
	{
		MMU_ww(CPU_SEGMENT_SS, REG_SS, FLAGS_offset(), REG_EFLAGS);
		REG_EFLAGS = flags; //Restore!
	}
}