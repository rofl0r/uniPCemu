//Ports compatibility (e.g. for keyboard, mouse and joystick), input/output to hw ports from software!

#include "headers/types.h"
#include "headers/hardware/ports.h" //Full PORTIN/OUT compatibility!

//Log unhandled port IN/OUT?
//#define LOG_UNHANDLED_PORTS

/*

We handle direct input/output to/from hardware ports by the CPU!

*/

/*

//Initialises ports support!

*/

void Ports_Init()
{
	reset_ports(); //Passtrough: reset all ports!
}

extern word CPU_exec_CS; //OPCode CS
extern uint_32 CPU_exec_EIP; //OPCode EIP

byte PORT_IN_B(word port)
{
	byte result;
	if (EXEC_PORTIN(port,&result)) //Passtrough!
	{
#ifdef LOG_UNHANDLED_PORTS
		dolog("emu", "Warning: Unhandled PORT IN from port %04X (src:%04X:%08X)", port,CPU_exec_CS,CPU_exec_EIP);
#endif
	}
	return result; //Give the result!
}

void PORT_OUT_B(word port, byte b)
{
	if (EXEC_PORTOUT(port, b)) //Passtrough and error?
	{
#ifdef LOG_UNHANDLED_PORTS
		dolog("emu", "Warning: Unhandled PORT OUT to port %04X value %02X (src:%04X:%08X)", port, b, CPU_exec_CS, CPU_exec_EIP); //Report unhandled NMI!
#endif
	}
}

word PORT_IN_W(word port) //IN result,port
{
	union
	{
		struct
		{
			byte low;
			byte high;
		};
		word w;
	} splitter;
	if (port & 1) //Not aligned?
	{
		goto bytetransferr; //Force byte transfer!
	}
	if (EXEC_PORTINW(port, &splitter.w)) //Passtrough!
	{
		bytetransferr:
		splitter.low = PORT_IN_B(port); //Low first
		splitter.high = PORT_IN_B(port + 1); //High last!
	}
	return splitter.w; //Give word!
}

void PORT_OUT_W(word port, word w) //OUT port,w
{
	union
	{
		struct
		{
			byte low;
			byte high;
		} byte;
		word w;
	} splitter;
	if (port & 1) //Not aligned?
	{
		goto bytetransferw; //Force byte transfer!
	}
	if (EXEC_PORTOUTW(port, w)) //Passtrough!
	{
		bytetransferw:
		splitter.w = w; //Split!
		PORT_OUT_B(port, splitter.byte.low); //First low byte!
		PORT_OUT_B(port + 1, splitter.byte.high); //Next high byte!
	}
}

uint_32 PORT_IN_DW(word port) //IN result,port
{
	union
	{
		struct
		{
			word low;
			word high;
		};
		uint_32 dw;
	} splitter;
	if (port & 3) //Not aligned?
	{
		goto wordtransferr; //Force word transfer!
	}
	if (EXEC_PORTIND(port, &splitter.dw)) //Passtrough!
	{
		wordtransferr:
		splitter.low = PORT_IN_W(port); //Low first
		splitter.high = PORT_IN_W(port + 2); //High last!
	}
	return splitter.dw; //Give dword!
}

void PORT_OUT_DW(word port, uint_32 dw) //OUT port,w
{
	union
	{
		struct
		{
			word low;
			word high;
		};
		uint_32 dw;
	} splitter;
	if (port & 3) //Not aligned?
	{
		goto wordtransferw; //Force word transfer!
	}
	if (EXEC_PORTOUTD(port, dw)) //Passtrough!
	{
		wordtransferw:
		splitter.dw = dw; //Split!
		PORT_OUT_W(port, splitter.low); //First low byte!
		PORT_OUT_W(port + 2, splitter.high); //Next high byte!
	}
}