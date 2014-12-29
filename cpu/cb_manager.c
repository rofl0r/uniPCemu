#include "headers/types.h" //Basic types!
#include "headers/cpu/cpu.h" //For handler!
#include "headers/cpu/callback.h" //Typedefs!
#include "headers/cpu/easyregs.h" //Easy register support for DOSBox!

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

void write_BIOSw(uint_32 offset, word value)
{
	EMU_BIOS[offset] = value&0xFF; //Low byte!
	EMU_BIOS[offset+1] = (value>>8); //High byte!
}

#define Bit8u byte
#define Bit16u word

void addCBHandler(byte type, Handler CBhandler, uint_32 intnr) //Add a callback!
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

	switch (type) //Extra handlers?
	{
		case CB_DOSBOX_IRQ0:
		case CB_DOSBOX_IRQ1:
		case CB_DOSBOX_IRQ9:
		case CB_DOSBOX_IRQ12:
		case CB_DOSBOX_IRQ12_RET:
		case CB_DOSBOX_INT16: //Normal operations?
		case CB_DOSBOX_MOUSE: //Mouse?
			CB_datasegment = (intnr>>16);
			CB_dataoffset = (intnr&0xFFFF);
			CB_realoffset = CB_dataoffset; //Offset within the custom bios is this!
			break;
		default: //Original offset method?
			//Real offset is already set
			CB_datasegment = CB_SEG; //Segment of data allocated!
			CB_dataoffset = offset; //Start of data/code!
			break;
	}

	uint_32 dataoffset = CB_realoffset; //Load the real offset for usage by default!

	switch (type)
	{
	case CB_UNASSIGNEDINTERRUPT: //Same as below, but unassigned to an interrupt!
	case CB_INTERRUPT: //Interrupt call?
		//First: add to jmptbl!
		if (type!=CB_UNASSIGNEDINTERRUPT) //Not unassigned?
		{
			CPU_setint(intnr,CB_datasegment,CB_dataoffset); //Set the interrupt!
		}

		
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
		CPU_setint(intnr,CB_datasegment,CB_dataoffset);

		EMU_BIOS[CB_realoffset] = 0xCF; //Simple IRET!
		break;
	case CB_DATA: //Data/custom code only?
		break;

	//Handlers from DOSBox!
	case CB_DOSBOX_IRQ0:	// timer int8
		//if (use_cb) {
			EMU_BIOS[dataoffset+0x00] = (Bit8u)0xFE;	//GRP 4
			EMU_BIOS[dataoffset+0x01] = (Bit8u)0x38;	//Extra Callback instruction
			write_BIOSw(dataoffset+0x02,(Bit16u)curhandler);		//The immediate word
			dataoffset+=4;
		//}
		EMU_BIOS[dataoffset+0x00] =(Bit8u)0x50;		// push ax
		EMU_BIOS[dataoffset+0x01] =(Bit8u)0x52;		// push dx
		EMU_BIOS[dataoffset+0x02] =(Bit8u)0x1e;		// push ds
		write_BIOSw(dataoffset+0x03,(Bit16u)0x1ccd);	// int 1c
		EMU_BIOS[dataoffset+0x05] =(Bit8u)0xfa;		// cli
		EMU_BIOS[dataoffset+0x06] =(Bit8u)0x1f;		// pop ds
		EMU_BIOS[dataoffset+0x07] =(Bit8u)0x5a;		// pop dx
		write_BIOSw(dataoffset+0x08,(Bit16u)0x20b0);	// mov al, 0x20
		write_BIOSw(dataoffset+0x0a,(Bit16u)0x20e6);	// out 0x20, al
		EMU_BIOS[dataoffset+0x0c] =(Bit8u)0x58;		// pop ax
		EMU_BIOS[dataoffset+0x0d] =(Bit8u)0xcf;		//An IRET Instruction
		//return (use_cb?0x12:0x0e);
		break;
	case CB_DOSBOX_IRQ1:	// keyboard int9
		EMU_BIOS[dataoffset+0x00] =(Bit8u)0x50;			// push ax
		write_BIOSw(dataoffset+0x01,(Bit16u)0x60e4);		// in al, 0x60
		write_BIOSw(dataoffset+0x03,(Bit16u)0x4fb4);		// mov ah, 0x4f
		EMU_BIOS[dataoffset+0x05] =(Bit8u)0xf9;			// stc
		write_BIOSw(dataoffset+0x06,(Bit16u)0x15cd);		// int 15
		//if (use_cb) {
			write_BIOSw(dataoffset+0x08,(Bit16u)0x0473);	// jc skip
			EMU_BIOS[dataoffset+0x0a] =(Bit8u)0xFE;		//GRP 4
			EMU_BIOS[dataoffset+0x0b] =(Bit8u)0x38;		//Extra Callback instruction
			write_BIOSw(dataoffset+0x0c,(Bit16u)curhandler);			//The immediate word
			// jump here to (skip):
			dataoffset+=6;
		//}
		EMU_BIOS[dataoffset+0x08] =(Bit8u)0xfa;			// cli
		write_BIOSw(dataoffset+0x09,(Bit16u)0x20b0);		// mov al, 0x20
		write_BIOSw(dataoffset+0x0b,(Bit16u)0x20e6);		// out 0x20, al
		EMU_BIOS[dataoffset+0x0d] =(Bit8u)0x58;			// pop ax
		EMU_BIOS[dataoffset+0x0e] =(Bit8u)0xcf;			//An IRET Instruction
		//return (use_cb?0x15:0x0f);
		break;
	case CB_DOSBOX_IRQ9:	// pic cascade interrupt
		//if (use_cb) {
			EMU_BIOS[dataoffset+0x00] =(Bit8u)0xFE;	//GRP 4
			EMU_BIOS[dataoffset+0x01] =(Bit8u)0x38;	//Extra Callback instruction
			write_BIOSw(dataoffset+0x02,(Bit16u)curhandler);		//The immediate word
			dataoffset+=4;
		//}
		EMU_BIOS[dataoffset+0x00] =(Bit8u)0x50;		// push ax
		write_BIOSw(dataoffset+0x01,(Bit16u)0x61b0);	// mov al, 0x61
		write_BIOSw(dataoffset+0x03,(Bit16u)0xa0e6);	// out 0xa0, al
		write_BIOSw(dataoffset+0x05,(Bit16u)0x0acd);	// int a
		EMU_BIOS[dataoffset+0x07] =(Bit8u)0xfa;		// cli
		EMU_BIOS[dataoffset+0x08] =(Bit8u)0x58;		// pop ax
		EMU_BIOS[dataoffset+0x09] =(Bit8u)0xcf;		//An IRET Instruction
		//return (use_cb?0x0e:0x0a);
		break;
	case CB_DOSBOX_IRQ12:	// ps2 mouse int74
		//if (!use_cb) E_Exit("int74 callback must implement a callback handler!");
		EMU_BIOS[dataoffset+0x00] =(Bit8u)0x1e;		// push ds
		EMU_BIOS[dataoffset+0x01] =(Bit8u)0x06;		// push es
		write_BIOSw(dataoffset+0x02,(Bit16u)0x6066);	// pushad
		EMU_BIOS[dataoffset+0x04] =(Bit8u)0xfc;		// cld
		EMU_BIOS[dataoffset+0x05] =(Bit8u)0xfb;		// sti
		EMU_BIOS[dataoffset+0x06] =(Bit8u)0xFE;		//GRP 4
		EMU_BIOS[dataoffset+0x07] =(Bit8u)0x38;		//Extra Callback instruction
		write_BIOSw(dataoffset+0x08,(Bit16u)curhandler);			//The immediate word
		//return 0x0a;
		break;
	case CB_DOSBOX_IRQ12_RET:	// ps2 mouse int74 return
		//if (use_cb) {
			EMU_BIOS[dataoffset+0x00] =(Bit8u)0xFE;	//GRP 4
			EMU_BIOS[dataoffset+0x01] =(Bit8u)0x38;	//Extra Callback instruction
			write_BIOSw(dataoffset+0x02,(Bit16u)curhandler);		//The immediate word
			dataoffset+=4;
		//}
		EMU_BIOS[dataoffset+0x00] =(Bit8u)0xfa;		// cli
		write_BIOSw(dataoffset+0x01,(Bit16u)0x20b0);	// mov al, 0x20
		write_BIOSw(dataoffset+0x03,(Bit16u)0xa0e6);	// out 0xa0, al
		write_BIOSw(dataoffset+0x05,(Bit16u)0x20e6);	// out 0x20, al
		write_BIOSw(dataoffset+0x07,(Bit16u)0x6166);	// popad
		EMU_BIOS[dataoffset+0x09] =(Bit8u)0x07;		// pop es
		EMU_BIOS[dataoffset+0x0a] =(Bit8u)0x1f;		// pop ds
		EMU_BIOS[dataoffset+0x0b] =(Bit8u)0xcf;		//An IRET Instruction
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
		write_BIOSw(dataoffset+0x00,(Bit16u)0x07eb);		// jmp i33hd
		dataoffset+=9;
		// jump here to (i33hd):
		//if (use_cb) {
			EMU_BIOS[dataoffset+0x00] =(Bit8u)0xFE;	//GRP 4
			EMU_BIOS[dataoffset+0x01] =(Bit8u)0x38;	//Extra Callback instruction
			write_BIOSw(dataoffset+0x02,(Bit16u)curhandler);		//The immediate word
			dataoffset+=4;
		//}
		EMU_BIOS[dataoffset+0x00] =(Bit8u)0xCF;		//An IRET Instruction
		//return (use_cb?0x0e:0x0a);
		break;
	case CB_DOSBOX_INT16:
		EMU_BIOS[dataoffset+0x00] =(Bit8u)0xFB;		//STI
		//if (use_cb) {
			EMU_BIOS[dataoffset+0x01] =(Bit8u)0xFE;	//GRP 4
			EMU_BIOS[dataoffset+0x02] =(Bit8u)0x38;	//Extra Callback instruction
			write_BIOSw(dataoffset+0x03,(Bit16u)curhandler);	//The immediate word
			dataoffset+=4;
		//}
		EMU_BIOS[dataoffset+0x01] =(Bit8u)0xCF;		//An IRET Instruction
		byte i;
		for (i=0;i<=0x0b;i++) EMU_BIOS[dataoffset+0x02+i] =0x90;
		write_BIOSw(dataoffset+0x0e,(Bit16u)0xedeb);	//jmp callback
		//return (use_cb?0x10:0x0c);
		break;


	default: //Default: unsupported!
		break;
	}
}

//DOSBox compatiblity.

void CALLBACK_SZF(byte val) {
	uint_32 flags;
	flags = EFLAGS; //Read flags!
	EFLAGS = MMU_rw(CPU_SEGMENT_SS,SS,SP+4,0); 
	if (val) ZF = 1;
	else ZF = 0; 
	MMU_ww(CPU_SEGMENT_SS,SS,SP+4,EFLAGS); 
	EFLAGS = flags; //Restore!
}

void CALLBACK_SCF(byte val) {
	uint_32 flags;
	flags = EFLAGS; //Read flags!
	EFLAGS = MMU_rw(CPU_SEGMENT_SS,SS,SP+4,0); 
	if (val) CF = 1;
	else CF = 0; 
	MMU_ww(CPU_SEGMENT_SS,SS,SP+4,EFLAGS); 
	EFLAGS = flags; //Restore!
}

void CALLBACK_SIF(byte val) {
	uint_32 flags;
	flags = EFLAGS; //Read flags!
	EFLAGS = MMU_rw(CPU_SEGMENT_SS,SS,SP+4,0); 
	if (val) IF = 1;
	else IF = 0; 
	MMU_ww(CPU_SEGMENT_SS,SS,SP+4,EFLAGS); 
	EFLAGS = flags; //Restore!
}