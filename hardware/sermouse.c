#include "headers/hardware/sermouse.h" //Our typedefs!
#include "headers/support/zalloc.h" //Allocation support!
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/hardware/uart.h" //UART support for the COM port!
#include "headers/support/fifobuffer.h" //FIFO buffer support!
#include "headers/support/locks.h" //Locking support!
#include "headers/support/signedness.h" //Sign conversion!
#include "headers/emu/timers.h" //Timing support!
#include "headers/emu/input.h" //Input timing support!

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
		lock("SERMOUSE");
		byte xu, yu;
		//Process the packet into the buffer, if possible!
		if (fifobuffer_freesize(SERMouse.buffer) > 2) //Gotten enough space to process?
		{
			xu = signed2unsigned8(packet->xmove);
			yu = signed2unsigned8(packet->ymove);
			byte infobits = 0;
			byte buttons = 0;
			SERMouse.buttons = packet->buttons; //Last button status!
			buttons |= (packet->buttons & 1) << 5; //Left mouse button!
			buttons |= (packet->buttons & 2) << 3; //Right mouse button!
			infobits |= ((xu & 0xC0) >> 5); //Upper two x bits!
			infobits |= ((yu & 0xC0) >> 3); //Upper two y bits!
			infobits |= (buttons << 4); //Add the buttons pressed!
			infobits |= 0x40; //Set mouse bit!
			xu &= 0x3F; //x value!
			yu &= 0x3F; //y value!
			//Convert buttons (packet=1=left, 2=right, 4=middle)!
			writefifobuffer(SERMouse.buffer, infobits); //Give info and buttons!
			writefifobuffer(SERMouse.buffer, xu); //X movement!
			writefifobuffer(SERMouse.buffer, yu); //Y movement!
			UART_handleInputs(); //Process the input given by the CPU!
		}
		unlock("SERMOUSE");
	}
	MOUSE_PACKET *temp;
	temp = packet; //Load packet to delete!
	freez(&temp, sizeof(*temp), "SERMouse_FlushPacket");
}

void SERmouse_setModemControl(byte line) //Set output lines of the Serial Mouse!
{
	lock("SERMOUSE");
	if ((line & 1) != (SERMouse.modemcontrol & 1)) //Toggled?
	{
		fifobuffer_gotolast(SERMouse.buffer); //Flush the FIFO buffer until last input!
		writefifobuffer(SERMouse.buffer, 'M'); //with a bunch of ASCII 'M' characters.
		fifobuffer_gotolast(SERMouse.buffer); //Flush the FIFO buffer final byte and start our packet!
		writefifobuffer(SERMouse.buffer, 'M'); //this is intended to be a way for
		writefifobuffer(SERMouse.buffer, 'M'); //drivers to verify that there is
		writefifobuffer(SERMouse.buffer, 'M'); //actually a mouse connected to the port.
		writefifobuffer(SERMouse.buffer, 'M');
		writefifobuffer(SERMouse.buffer, 'M');
		UART_handleInputs(); //Update input!
	}
	SERMouse.modemcontrol = line; //Set the modem control lines for reference!
	unlock("SERMOUSE");
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

void initSERMouse()
{
	memset(&SERMouse, 0, sizeof(SERMouse));
	SERMouse.supported = (EMULATED_CPU <= CPU_80186); //Use serial mouse?
	if (useSERMouse()) //Enabled this mouse?
	{
		SERMouse.buffer = allocfifobuffer(16); //Small input buffer!
		UART_registerdevice(0,&SERmouse_setModemControl,&serMouse_hasData,&serMouse_readData,NULL); //Register our UART device!
		setMouseRate(40.0f); //We run at 40 packets per second!
	}
}