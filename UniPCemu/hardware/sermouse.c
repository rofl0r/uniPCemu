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
	byte modemcontrol; //Modem control register!
	byte buttons; //Current button status!
} SERMouse;

byte useSERMouse() //Serial mouse enabled?
{
	return SERMouse.supported; //Are we supported?
}

void SERmouse_packet_handler(MOUSE_PACKET *packet)
{
	if ((packet->xmove) || (packet->ymove) || (SERMouse.buttons != packet->buttons)) //Something to do?
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
			if (packet->xmove < 0) highbits = 3;
			if (packet->ymove < 0) highbits |= 12;
			writefifobuffer(SERMouse.buffer, 0x40 | (buttons << 4) | highbits); //Give info and buttons!
			writefifobuffer(SERMouse.buffer, packet->xmove&63); //X movement!
			writefifobuffer(SERMouse.buffer, packet->ymove&63); //Y movement!
		}
	}
	MOUSE_PACKET *temp;
	temp = packet; //Load packet to delete!
	freez((void **)&temp, sizeof(*temp), "SERMouse_FlushPacket");
}

void SERmouse_setModemControl(byte line) //Set output lines of the Serial Mouse!
{
	if (((line & 3) == 3) && ((SERMouse.modemcontrol&3)!=3)) //DTR&RTS set?
	{
		fifobuffer_clear(SERMouse.buffer); //Flush the FIFO buffer until last input!
		int i;
		for (i = 0;i < 7;i++) //How many bytes to send?
		{
			writefifobuffer(SERMouse.buffer, 'M'); //with a bunch of ASCII 'M' characters.
		}
	}
	SERMouse.modemcontrol = line; //Set the modem control lines for reference!
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