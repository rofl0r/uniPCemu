//src: http://wiki.osdev.org/Serial_Ports

//UART chip emulation.

#include "headers/hardware/pic.h" //IRQ support!
//#include "headers/hardware/uart.h" //UART support (ourselves)!
#include "headers/hardware/ports.h"  //Port support!

//Hardware disabled?
#define __HW_DISABLED 1

struct
{
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
} UART_port[4]; //All UART ports!

void launchUARTIRQ(byte COMport, byte cause) //Simple 2-bit cause.
{
	//Prepare our info!
	UART_port[COMport].InterruptIdentificationRegister.data = 0; //Reset for our cause!
	UART_port[COMport].InterruptCause.SimpleCause = (cause&3); //Load the simple cause (8250 way)!
	UART_port[COMport].InterruptIdentificationRegister.InterruptPending = 1; //The interrupt will be pending now, we're the cuase!
	//Finally launch the IRQ!
	if (COMport&1) //COM2&COM4?
	{
		doirq(3); //Do IRQ!
	}
	else //COM1&COM3?
	{
		doirq(3); //Do IRQ!
	}
}

byte getCOMport(word port) //What COM port?
{
	byte highnibble = ((port>>16)&0xF); //3 or 2
	byte lownibble = ((port>>8)&0xF); //F or E
	
	byte COMport;
	COMport = 0; //Init COM port!
	switch (lownibble) //Get COM1/3?
	{
		case 0xF: //COM1/2
			//Base 0
			break;
		case 0xE: //COM3/4
			COMport |= 2; //Base 2 (port 3/4)
			break;
		default:
			return 4; //Illegal!
			break;
	}

	switch (highnibble)
	{
		case 0x3: //Even COM port (COM1/2)
			break;
		case 0x2: //Odd COM port (COM3/4)
			COMport |= 1; //Add 1!
			break;
		default:
			return 4; //Illegal!
			break;
	}
	
	return COMport; //Give the COM port!
}

/*

Gebleven @http://en.wikibooks.org/wiki/Serial_Programming/8250_UART_Programming#Modem_Control_Register

*/

//Offset calculator!
#define COMPORT_offset(port) (port&0x7)

byte PORT_readUART(word port) //Read from the uart!
{
	byte COMport;
	if ((COMport = getCOMport(port))==4) //Unknown?
	{
		return ~0; //Error!
	}
	switch (COMPORT_offset(port)) //What offset?
	{
		case 0: //Receiver buffer OR Low byte of Divisor Value?
			if (UART_port[COMport].LineControlRegister.DLAB) //DLAB?
			{
				return (UART_port[COMport].DLAB&0xFF); //Low byte!
			}
			else //Receiver buffer?
			{
				//Read from input buffer!
				if (UART_port[COMport].InterruptIdentificationRegister.InterruptPending && UART_port[COMport].InterruptCause.SimpleCause==2) //We're to clear?
				{
					UART_port[COMport].InterruptIdentificationRegister.data = 0; //Reset the register!
				}
				//return value with bits toggled by Line Control Register!
			}
			break;
		case 1: //Interrupt Enable Register?
			if (UART_port[COMport].LineControlRegister.DLAB) //DLAB?
			{
				return ((UART_port[COMport].DLAB>>8)&0xFF); //High byte!
			}
			else //Interrupt enable register?
			{
				//bit0 = data available
				//bit1 = transmitter empty
				//bit2 = break/error
				//bit3 = status change
				return UART_port[COMport].InterruptEnableRegister; //Give the register!
			}
			break;
		case 2: //Interrupt ID registers?
			return UART_port[COMport].InterruptIdentificationRegister.data; //Give the register!
			break;
		case 3: //Line Control Register?
			return UART_port[COMport].LineControlRegister.data; //Give the register!
			break;
		case 4:  //Modem Control Register?
			return UART_port[COMport].ModemControlRegister; //Give the register!
			break;
		case 5: //Line Status Register?
			if (UART_port[COMport].InterruptIdentificationRegister.InterruptPending && UART_port[COMport].InterruptCause.SimpleCause==3) //We're to clear?
			{
				UART_port[COMport].InterruptIdentificationRegister.data = 0; //Reset the register!
			}
			return UART_port[COMport].LineStatusRegister; //Give the register!
			break;
		case 6: //Modem Status Register?
			if (UART_port[COMport].InterruptIdentificationRegister.InterruptPending && !UART_port[COMport].InterruptCause.SimpleCause) //We're to clear?
			{
				UART_port[COMport].InterruptIdentificationRegister.data = 0; //Reset the register!
			}
			return UART_port[COMport].ModemStatusRegister; //Give the register!
			break;
		case 7: //Scratch register?
			return UART_port[COMport].ScratchRegister; //Give the register!
			break; //We do nothing yet!
	}
	return ~0; //Undefined!
}

void PORT_writeUART(word port, byte value)
{
	byte COMport;
	byte oldDLAB;
	if ((COMport = getCOMport(port))==4) //Unknown?
	{
		return; //Error!
	}
	switch (COMPORT_offset(port)) //What offset?
	{
		case 0: //Output buffer OR Low byte of Divisor Value?
			if (UART_port[COMport].LineControlRegister.DLAB) //DLAB?
			{
				UART_port[COMport].DLAB &= ~0xFF; //Clear the low byte!
				UART_port[COMport].DLAB |= value; //Low byte!
			}
			else //Output buffer?
			{
				if (UART_port[COMport].InterruptIdentificationRegister.InterruptPending && !UART_port[COMport].InterruptCause.SimpleCause==1) //We're to clear?
				{
					UART_port[COMport].InterruptIdentificationRegister.data = 0; //Reset the register!
				}				
				//Write to output buffer, toggling bits by Line Control Register!
			}
			break;
		case 1: //Interrupt Enable Register?
			if (UART_port[COMport].LineControlRegister.DLAB) //DLAB?
			{
				UART_port[COMport].DLAB &= ~0xFF00; //Clear the high byte!
				UART_port[COMport].DLAB |= (value<<8); //High!
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
			break;
		case 7: //Scratch register?
			UART_port[COMport].ScratchRegister = value; //Set the register!
			break; //We do nothing yet!
		default: //Unknown write register?
			break;
	}
}

void BIOS_initUART() //Init software debugger!
{
	if (__HW_DISABLED) return; //Abort!
	memset(&UART_port,0,sizeof(UART_port)); //Clear memory used!
	word ports[4] = {0,0,0,0};
	byte i,j;
	for (i=0;i<4;i++) //Process all I/O ports!
	{
		//Register all I/O ports for this COM port!
		for (j=0;j<5;j++)
		{
			register_PORTOUT(ports[i]+j,&PORT_writeUART);
		}
		register_PORTOUT(ports[i]+7,&PORT_writeUART);
		
		for (j=0;j<8;j++)
		{
			register_PORTIN(ports[i]+j,&PORT_readUART);
		}
	}
}