//src: http://wiki.osdev.org/Serial_Ports

//UART chip emulation.

#include "headers/hardware/pic.h" //IRQ support!
#include "headers/hardware/uart.h" //UART support (ourselves)!
#include "headers/hardware/ports.h"  //Port support!

//Hardware disabled?
#define __HW_DISABLED 0

struct
{
	byte used; //Are we an used UART port?
	//+0 is data register (transmit or receive data)
	//+1 as well as +0 have alternative
	byte InterruptEnableRegister; //Either this register or Divisor Latch when 
	union
	{
		struct
		{
			union
			{
				struct
				{
					byte InterruptPending : 1;
					byte TransmitterEmpty : 1;
					byte BreadError : 1;
					byte StatusChange : 1;
					byte reserved : 1;
					byte Enable64byteFIFO : 1;
					byte FIFOStatus : 2; //0=No FIFO present, 1=Reserved, 2=FIFO Enabled, but not functioning, 3=FIFO Enabled.
				};
				byte data; //Interrupt information. Cleared by doing specific actions!
			};
		} InterruptIdentificationRegister; //Interrupt information.
		struct
		{
			union
			{
				struct
				{
					byte dummy : 1;
					byte SimpleCause : 2; //Simple cause. 9=Modem Status Interrupt, 1=Transmitter Holding Register Empty Interrupt, 2=Received Data Available Interrrupt, 3=Receiver Line Status Interrupt!
					byte Unused : 5; //Unused by old chipsets without FIFO. Always 0.
				};
			} InterruptCause;
		};
	};
	byte FIFOControlRegister; //FIFO Control register!
	union
	{
		struct
		{
			byte DataBits : 2; //Value = 5+DataBits
			byte StopBits : 1; //0=1, 1=1.5(DataBits=0) or 2(All other cases).
			byte ParityEnabled : 1; //Parity enabled?
			byte ParityType : 2; //0=Odd, 1=Even, 2=Mark, 3=Space.
			byte Unknown : 1;
			byte DLAB : 1; //Enable address 0&1 mapping to divisor?
		}; //Information about the sending structure.
		byte data;
	} LineControlRegister;
	byte ModemControlRegister;
	byte LineStatusRegister;
	byte ModemStatusRegister;
	byte ScratchRegister;
	//Seperate register alternative
	word DLAB; //The speed of transmission, 115200/DLAB=Speed set.
	//This speed is the ammount of bits (data bits), stop bits (0=1, 1=1.5(with 5 bits data)/2(all other cases)) and parity bit when set, that are transferred per second.

	byte havereceiveddata; //Have we received data?
	byte receiveddata; //The data that's received (the buffer for the software to read when filled)!

	//The handlers for the device attached, if any!
	UART_setmodemcontrol setmodemcontrol;
	UART_receivedata receivedata;
	UART_senddata senddata;
	UART_hasdata hasdata;

	byte interrupt_causes[4]; //All possible causes of an interrupt!
	double UART_receivetiming; //UART receive timing!
	double UART_bytereceivetiming; //UART byte received timing!
} UART_port[4]; //All UART ports!

byte numUARTports = 0; //How many ports?

OPTINLINE void launchUARTIRQ(byte COMport, byte cause) //Simple 2-bit cause.
{
	if (!UART_port[COMport].used) return; //Unused COM port!
	switch (cause) //What cause?
	{
	case 0: //Modem status changed?
		if (!(UART_port[COMport].InterruptEnableRegister & 8)) return; //Don't trigger if it's disabled!
		break;
	case 1: //Ready to send?
		if (!(UART_port[COMport].InterruptEnableRegister & 2)) return; //Don't trigger if it's disabled!
		break;
	case 2: //Received data?
		if (!(UART_port[COMport].InterruptEnableRegister & 1)) return; //Don't trigger if it's disabled!
		break;
	case 3: //Receiver line status changed?
		if (!(UART_port[COMport].InterruptEnableRegister & 4)) return; //Don't trigger if it's disabled!
		break;
	}
	//Prepare our info!
	UART_port[COMport].interrupt_causes[cause & 3] = 1; //We're requesting an interrupt for this cause!

	//Finally launch the IRQ!
	if (COMport&1) //COM2&COM4?
	{
		raiseirq(3); //Do IRQ!
	}
	else //COM1&COM3?
	{
		raiseirq(4); //Do IRQ!
	}
}

OPTINLINE void startUARTIRQ(byte IRQ)
{
	byte cause, port; //What cause are we?
	byte portbase, actualport;
	portbase = (IRQ == 4) ? 0 : 1; //Base port!
	for (port = 0;port < 2;port++) //List ports!
	{
		actualport = portbase + (port << 1); //Take the actual port!
		for (cause = 0;cause < 4;cause++) //Check all causes!
		{
			if (UART_port[actualport].interrupt_causes[cause]) //We're is the cause?
			{
				UART_port[actualport].interrupt_causes[cause] = 0; //Reset the cause!
				UART_port[actualport].InterruptIdentificationRegister.data = 0; //Reset for our cause!
				UART_port[actualport].InterruptCause.SimpleCause = (cause & 3); //Load the simple cause (8250 way)!
				UART_port[actualport].InterruptIdentificationRegister.InterruptPending = 0; //We've activated!
				return; //Stop scanning!
			}
		}
	}
}

byte getCOMport(word port) //What COM port?
{
	byte highnibble = (port>>8); //3 or 2
	byte lownibble = ((port>>3)&0x1F); //F or E
	
	byte result;
	result = 0; //Init COM port!
	switch (lownibble) //Get COM1/3?
	{
		case 0x1F: //COM1/2
			//Base 0
			break;
		case 0x1D: //COM3/4
			result |= 2; //Base 2 (port 3/4)
			break;
		default:
			result = 4; //Illegal!
			break;
	}

	switch (highnibble)
	{
		case 0x3: //Even COM port (COM1/2)
			break;
		case 0x2: //Odd COM port (COM3/4)
			result |= 1; //Add 1!
			break;
		default:
			result = 4; //Illegal!
			break;
	}
	
	return ((result<numUARTports) && (result<4))?result:4; //Invalid by default!; //Give the COM port or 4 for unregistered COM port!
}

/*

Processed until http://en.wikibooks.org/wiki/Serial_Programming/8250_UART_Programming#Modem_Control_Register

*/

//Offset calculator!
#define COMPORT_offset(port) (port&0x7)

void updateUARTSpeed(byte COMport, word DLAB)
{
	UART_port[COMport].UART_bytereceivetiming = (115200.0/(DLAB?0x10000:DLAB)) / (5+UART_port[COMport].LineControlRegister.DataBits+(UART_port[COMport].LineControlRegister.StopBits<<1)); //Every DLAB+1 / Line Control Register-dependant bytes per second! Simple formula instead of full emulation, like the PIT!
}

byte PORT_readUART(word port, byte *result) //Read from the uart!
{
	byte COMport;
	if ((COMport = getCOMport(port))==4) //Unknown?
	{
		return 0; //Error: not our port!
	}
	switch (COMPORT_offset(port)) //What offset?
	{
		case 0: //Receiver buffer OR Low byte of Divisor Value?
			if (UART_port[COMport].LineControlRegister.DLAB) //DLAB?
			{
				*result = (UART_port[COMport].DLAB&0xFF); //Low byte!
			}
			else //Receiver buffer?
			{
				//Read from input buffer!
				if (!UART_port[COMport].InterruptIdentificationRegister.InterruptPending && UART_port[COMport].InterruptCause.SimpleCause==2) //We're to clear?
				{
					UART_port[COMport].InterruptIdentificationRegister.data = 0; //Reset the register!
					UART_port[COMport].InterruptIdentificationRegister.InterruptPending = 1; //Reset interrupt pending!
					switch (COMport) //What port?
					{
					case 0:
					case 2:
						lowerirq(4); //Lower our IRQ if it's raised!
						acnowledgeIRQrequest(4); //Acnowledge!
						break;
					case 1:
					case 3:
						lowerirq(3); //Lower our IRQ if it's raised!
						acnowledgeIRQrequest(3); //Acnowledge!
						break;
					}
				}
				//return value with bits toggled by Line Control Register!
				*result = 0x00; //Invalid input by default!
				if (UART_port[COMport].havereceiveddata)
				{
					*result = UART_port[COMport].receiveddata; //Receive the data!
					UART_port[COMport].receiveddata = 0x00; //Clear the received data in the buffer!
					UART_port[COMport].havereceiveddata = 0; //We don't have any data anymore!
				}
			}
			break;
		case 1: //Interrupt Enable Register?
			if (UART_port[COMport].LineControlRegister.DLAB) //DLAB?
			{
				*result = ((UART_port[COMport].DLAB>>8)&0xFF); //High byte!
			}
			else //Interrupt enable register?
			{
				//bit0 = data available
				//bit1 = transmitter empty
				//bit2 = break/error
				//bit3 = status change
				*result = UART_port[COMport].InterruptEnableRegister; //Give the register!
			}
			break;
		case 2: //Interrupt ID registers?
			*result = UART_port[COMport].InterruptIdentificationRegister.data; //Give the register!
			break;
		case 3: //Line Control Register?
			*result = UART_port[COMport].LineControlRegister.data; //Give the register!
			break;
		case 4:  //Modem Control Register?
			*result = UART_port[COMport].ModemControlRegister; //Give the register!
			break;
		case 5: //Line Status Register?
			if (!UART_port[COMport].InterruptIdentificationRegister.InterruptPending && UART_port[COMport].InterruptCause.SimpleCause == 3) //We're to clear?
			{
				UART_port[COMport].InterruptIdentificationRegister.data = 0; //Reset the register!
				UART_port[COMport].InterruptIdentificationRegister.InterruptPending = 1; //Reset interrupt pending!
				switch (COMport) //What port?
				{
				case 0:
				case 2:
					lowerirq(4); //Lower our IRQ if it's raised!
					acnowledgeIRQrequest(4); //Acnowledge!
					break;
				case 1:
				case 3:
					lowerirq(3); //Lower our IRQ if it's raised!
					acnowledgeIRQrequest(3); //Acnowledge!
					break;
				}
			}
			UART_port[COMport].LineStatusRegister &= ~1; //No data ready!
			if (UART_port[COMport].havereceiveddata) //Data buffer full?
			{
				UART_port[COMport].LineStatusRegister |= 1; //Data is ready!
			}
			*result = UART_port[COMport].LineStatusRegister; //Give the register!
			break;
		case 6: //Modem Status Register?
			if (!UART_port[COMport].InterruptIdentificationRegister.InterruptPending && UART_port[COMport].InterruptCause.SimpleCause == 0) //We're to clear?
			{
				UART_port[COMport].InterruptIdentificationRegister.data = 0; //Reset the register!
				UART_port[COMport].InterruptIdentificationRegister.InterruptPending = 1; //Reset interrupt pending!
				switch (COMport) //What port?
				{
				case 0:
				case 2:
					lowerirq(4); //Lower our IRQ if it's raised!
					acnowledgeIRQrequest(4); //Acnowledge!
					break;
				case 1:
				case 3:
					lowerirq(3); //Lower our IRQ if it's raised!
					acnowledgeIRQrequest(3); //Acnowledge!
					break;
				}
			}
			*result = UART_port[COMport].ModemStatusRegister; //Give the register!
			break;
		case 7: //Scratch register?
			*result = UART_port[COMport].ScratchRegister; //Give the register!
			break; //We do nothing yet!
		default:
			return 0; //Unknown port!
	}
	return 1; //Defined port!
}

byte PORT_writeUART(word port, byte value)
{
	byte COMport;
	byte oldDLAB;
	if ((COMport = getCOMport(port))==4) //Unknown?
	{
		return 0; //Error!
	}
	switch (COMPORT_offset(port)) //What offset?
	{
		case 0: //Output buffer OR Low byte of Divisor Value?
			if (UART_port[COMport].LineControlRegister.DLAB) //DLAB?
			{
				UART_port[COMport].DLAB &= ~0xFF; //Clear the low byte!
				UART_port[COMport].DLAB |= value; //Low byte!
				updateUARTSpeed(COMport,UART_port[COMport].DLAB); //We're updated!
			}
			else //Output buffer?
			{
				if (!UART_port[COMport].InterruptIdentificationRegister.InterruptPending && UART_port[COMport].InterruptCause.SimpleCause == 1) //We're to clear?
				{
					UART_port[COMport].InterruptIdentificationRegister.data = 0; //Reset the register!
					UART_port[COMport].InterruptIdentificationRegister.InterruptPending = 1; //Reset interrupt pending!
				}
				//Write to output buffer, toggling bits by Line Control Register!
				if (UART_port[COMport].senddata)
				{
					UART_port[COMport].senddata(value); //Send the data!
				}

			}
			break;
		case 1: //Interrupt Enable Register?
			if (UART_port[COMport].LineControlRegister.DLAB) //DLAB?
			{
				UART_port[COMport].DLAB &= ~0xFF00; //Clear the high byte!
				UART_port[COMport].DLAB |= (value<<8); //High!
				updateUARTSpeed(COMport, UART_port[COMport].DLAB); //We're updated!
			}
			else //Interrupt enable register?
			{
				//bit0 = data available
				//bit1 = transmitter empty
				//bit2 = break/error
				//bit3 = status change
				UART_port[COMport].InterruptEnableRegister = value; //Set the register!
			}
			break;
		case 2: //FIFO control register?
			UART_port[COMport].FIFOControlRegister = value; //Set the register!
			//Not used in the original 8250 UART.
			break;
		case 3: //Line Control Register?
			oldDLAB = UART_port[COMport].LineControlRegister.DLAB; //DLAB old?
			UART_port[COMport].LineControlRegister.data = value; //Set the register!
			if (!UART_port[COMport].LineControlRegister.DLAB && oldDLAB) //DLAB toggled off? Update the speed?
			{
				//Update the transmit speed with our new values and DLAB!
			}
			break;
		case 4:  //Modem Control Register?
			UART_port[COMport].ModemControlRegister = value; //Set the register!
			//Handle anything concerning this?
			if (UART_port[COMport].setmodemcontrol) //Line handler added?
			{
				UART_port[COMport].setmodemcontrol(value); //Update the output lines!
			}
			break;
		case 7: //Scratch register?
			UART_port[COMport].ScratchRegister = value; //Set the register!
			break; //We do nothing yet!
		default: //Unknown write register?
			return 0;
			break;
	}
	return 1; //We're supported!
}

void UART_handleInputs() //Handle any input to the UART!
{
	int i;

	//Raise the IRQ for the first device to give input!
	for (i = 0;i < 4;i++) //Process all ports!
	{
		if (UART_port[i].havereceiveddata) //Have we received data?
		{
			launchUARTIRQ(i, 2); //We've received data!
		}
	}
}

void updateUART(double timepassed)
{
	byte UART; //Check all UARTs!
	//Check all UART received data!
	for (UART=0;UART<4;++UART) //Check all UARTs!
	{
		if (UART_port[UART].hasdata) //Data receiver enabled for this port?
		{
			UART_port[UART].UART_receivetiming += timepassed; //Time our counter!
			if ((UART_port[UART].UART_receivetiming>=UART_port[UART].UART_bytereceivetiming) && UART_port[UART].UART_bytereceivetiming) //A byte has been received, timed?
			{
				UART_port[UART].UART_receivetiming -= UART_port[UART].UART_bytereceivetiming; //We've received a byte, if available!
				if (UART_port[UART].hasdata()) //Do we have data?
				{
					if (UART_port[UART].havereceiveddata==0) //No data received yet?
					{
						UART_port[UART].receiveddata = UART_port[UART].receivedata(); //Read the data to receive!
						UART_port[UART].havereceiveddata = 1; //We've received data!
					}
				}
			}
		}
	}
	UART_handleInputs(); //Handle the input received, when needed, as well as other conditions required!
}

void UART_registerdevice(byte portnumber, UART_setmodemcontrol setmodemcontrol, UART_hasdata hasdata, UART_receivedata receivedata, UART_senddata senddata)
{
	if (portnumber > 3) return; //Invalid port!
	//Register the handlers!
	UART_port[portnumber].used = 1; //We're an used UART port!
	UART_port[portnumber].setmodemcontrol = setmodemcontrol;
	UART_port[portnumber].hasdata = hasdata;
	UART_port[portnumber].receivedata = receivedata;
	UART_port[portnumber].senddata = senddata;
}

void initUART(byte numports) //Init software debugger!
{
	if (__HW_DISABLED) return; //Abort!
	memset(&UART_port,0,sizeof(UART_port)); //Clear memory used!
	numUARTports = numports;
	register_PORTOUT(&PORT_writeUART);
	register_PORTIN(&PORT_readUART);
	registerIRQ(3, &startUARTIRQ, NULL); //Register our IRQ finish!
	int i;
	for (i = 0;i < 4;i++)
	{
		UART_port[i >> 4].InterruptIdentificationRegister.InterruptPending = 1; //We're not executing!
	}
}
