#include "headers/hardware/sermouse.h" //Our typedefs!
#include "headers/support/zalloc.h" //Allocation support!
#include "headers/hardware/uart.h" //UART support for the COM port!
#include "headers/support/fifobuffer.h" //FIFO buffer support!
#include "headers/support/locks.h" //Locking support!
#include "headers/bios/bios.h" //BIOS support!

struct
{
	byte supported; //Are we supported?
	FIFOBUFFER *buffer; //The input buffer!
	byte buttons; //Current button status!
	byte movementmask; //Movement mask to use!
	byte powered; //Are we powered on?
} SERMouse;

byte useSERMouse() //Serial mouse enabled?
{
	return SERMouse.supported; //Are we supported?
}

void SERmouse_packet_handler(MOUSE_PACKET *packet)
{
	if (((((packet->xmove) || (packet->ymove)) && SERMouse.movementmask) || (SERMouse.buttons != packet->buttons)) && SERMouse.powered) //Something to do and powered on?
	{
		//Process the packet into the buffer, if possible!
		if (fifobuffer_freesize(SERMouse.buffer) > 2) //Gotten enough space to process?
		{
			byte buttons = 0;
			SERMouse.buttons = packet->buttons; //Last button status!
			//Convert buttons (packet=1=left, 2=right, 4=middle) to output (1=right, 2=left)!
			buttons = packet->buttons; //Left/right/middle mouse button!
			buttons &= 3; //Only left&right mouse buttons!
			buttons = (buttons >> 1) | ((buttons & 1) << 1);  //Left mouse button and right mouse buttons are switched in the packet vs our mouse handler packet!
			uint8_t highbits = 0;
			union
			{
				struct
				{
					byte xmove;
					byte ymove;
				};
				struct
				{
					sbyte smovex;
					sbyte smovey;
				};
			} movementconverter;
			movementconverter.smovex = packet->xmove; //X movement, 8-bit value!
			movementconverter.smovey = packet->ymove; //Y movement, 8-bit value!
			if (SERMouse.movementmask) //Not gotten movement masked?
			{
				//Bits 0-1 are X6&X7. Bits 2-3 are Y6&Y7. They're signed values.
				highbits |= (movementconverter.xmove>>6); //X6&X7 to bits 0-1!
				highbits |= ((movementconverter.ymove>>4)&0xC); //Y6&7 to bits 2-3!
			}
			writefifobuffer(SERMouse.buffer, 0x40 | (buttons << 4) | highbits); //Give info and buttons!
			writefifobuffer(SERMouse.buffer, (movementconverter.xmove&SERMouse.movementmask)); //X movement!
			writefifobuffer(SERMouse.buffer, (movementconverter.ymove&SERMouse.movementmask)); //Y movement!
		}
	}
	MOUSE_PACKET *temp;
	temp = packet; //Load packet to delete!
	freez((void **)&temp, sizeof(*temp), "SERMouse_FlushPacket");
}

void SERmouse_setModemControl(byte line) //Set output lines of the Serial Mouse!
{
	INLINEREGISTER byte previouspower; //Previous power line detected!
	previouspower = SERMouse.powered; //Previous power present?
	SERMouse.powered = (line & 2); //Are we powered on? This is done by the RTS output!
	if (SERMouse.powered&(previouspower^SERMouse.powered)) //Powered on? We're performing a mouse reset(Repowering the mouse)!
	{
		fifobuffer_clear(SERMouse.buffer); //Flush the FIFO buffer until last input!
		writefifobuffer(SERMouse.buffer, 'M'); //We respond with an ASCII 'M' character on reset.
	}
	SERMouse.movementmask = (line&1)?0x3F:0x00; //Allow movement to be used? Clearing DTR makes it not give Movement Input(it powers the lights that detect movement).
}

byte serMouse_readData()
{
	byte result;
	if (readfifobuffer(SERMouse.buffer, &result))
	{
		return result; //Give the data!
	}
	return 0; //Nothing to give!
}

byte serMouse_hasData() //Do we have data for input?
{
	byte temp;
	return peekfifobuffer(SERMouse.buffer, &temp); //Do we have data to receive?
}

void initSERMouse(byte enabled)
{
	memset(&SERMouse, 0, sizeof(SERMouse));
	SERMouse.supported = enabled; //Use serial mouse?
	if (useSERMouse()) //Is this mouse enabled?
	{
		SERMouse.buffer = allocfifobuffer(16,1); //Small input buffer!
		UART_registerdevice(0,&SERmouse_setModemControl,&serMouse_hasData,&serMouse_readData,NULL); //Register our UART device!
		setMouseRate(40.0f); //We run at 40 packets per second!
	}
	else
	{
		SERMouse.buffer = NULL; //No buffer present!
	}
}

void doneSERMouse()
{
	if (SERMouse.buffer) //Allocated?
	{
		free_fifobuffer(&SERMouse.buffer); //Free our buffer!
	}
}