#include "headers/types.h" //Basic types!
#include "headers/hardware/pic.h" //Interrupt support (IRQ 1)
#include "headers/hardware/ports.h" //Port support!
#include "headers/hardware/8042.h" //Our own functions!
#include "headers/mmu/mmu.h" //For wraparround 1/3/5/... MB! (A20 line)
#include "headers/support/log.h" //Logging support!

//Are we disabled?
#define __HW_DISABLED 0

/*

PS/2 Controller chip (8042)

*/

//Keyboard has higher input priority: IRQ1 expects data always, so higher priority!
byte ControllerPriorities[2] = {0,1}; //Port order to check if something's there 1=Second port, 0=First port else no port!

Controller8042_t Controller8042; //The PS/2 Controller chip!

void give_8042_input(byte data) //Give 8042 input (internal!)
{
	writefifobuffer(Controller8042.buffer,data);
}

void input_lastwrite_8042()
{
	fifobuffer_gotolast(Controller8042.buffer); //Goto last!	
}

void fill8042_input_buffer() //Fill input buffer from full buffer!
{
	if (Controller8042.status_buffer&2) //Buffer full?
	{
		//Nothing to do: buffer already full!
	}
	else //Buffer empty?
	{
		Controller8042.input_buffer = 0; //Undefined to start with!

		if (readfifobuffer(Controller8042.buffer,&Controller8042.input_buffer)) //Gotten something from 8042?
		{
			Controller8042.status_buffer &= ~0x20; //Clear AUX bit!
			Controller8042.status_buffer |= 0x2; //Set input buffer full!
			return; //Filled: we have something from the 8042 controller itself!
		}

		byte portorder;
		for (portorder=0;portorder<2;portorder++) //Process all our ports!
		{
			byte whatport = ControllerPriorities[portorder]; //The port to check!
			if (whatport<2) //Port has priority and available?
			{
				if (Controller8042.portread[whatport] && Controller8042.portpeek[whatport]) //Read handlers from the first PS/2 port available?
				{
					if (Controller8042.portpeek[whatport](&Controller8042.input_buffer)) //Got something?
					{
						Controller8042.input_buffer = Controller8042.portread[whatport](); //Execute the handler!
						Controller8042.status_buffer |= 0x2; //Set input buffer full!
						if (whatport) //AUX port?
						{
							Controller8042.status_buffer |= 0x20; //Set AUX bit!
						}
						else //Non-AUX?
						{
							Controller8042.status_buffer &= ~0x20; //Clear AUX bit!
						}
					}
				}
			}
		}
	}
}

void reset8042() //Reset 8042 up till loading BIOS!
{
	FIFOBUFFER *oldbuffer = Controller8042.buffer; //Our fifo buffer?
	memset(&Controller8042,0,sizeof(Controller8042)); //Init to 0!
	Controller8042.PS2ControllerConfigurationByte.FirstPortInterruptEnabled = 1; //Enable first port interrupt by default!
	Controller8042.buffer = oldbuffer; //Restore buffer!
}

void commandwritten_8042() //A command has been written to the 8042 controller?
{
	//Handle specific PS/2 commands!
	switch (Controller8042.command) //Special command?
	{
	case 0x60: //Write byte 0 from internal RAM!
	case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67: case 0x68: case 0x69: case 0x6A: case 0x6B: case 0x6C: case 0x6D: case 0x6E: case 0x6F: //Read "byte X" from internal RAM!
		Controller8042.Write_RAM = Controller8042.command-0x5F; //Write to internal RAM (value 0x01-0x20), so 1 more than our index!
		Controller8042.writeoutputport = 0; //Not anymore!
		break;
	case 0x20: //Read byte 0 from internal RAM!
	case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x2C: case 0x2D: case 0x2E: case 0x2F: //Read "byte X" from internal RAM!
		Controller8042.Read_RAM = Controller8042.command-0x1F; //Read from internal RAM (value 0x01-0x20), so 1 more than our index!
		Controller8042.readoutputport = 0; //Not anymore!
		break;
	case 0xA7: //Disable second PS/2 port! No ACK!
		Controller8042.PS2ControllerConfigurationByte.SecondPortDisabled = 1; //Disabled!
		break;
	case 0xA8: //Enable second PS/2 port! No ACK!
		Controller8042.PS2ControllerConfigurationByte.SecondPortDisabled = 0; //Enabled!
		break;
	case 0xA9: //Test second PS/2 port! Give 0x00 if passed (detected). 0x02-0x04=Not detected?
		if (Controller8042.portwrite[1] && Controller8042.portread[1] && Controller8042.portpeek[1]) //Registered?
		{
			give_8042_input(0x00); //Passed: we have one!
		}
		else
		{
			give_8042_input(0x01); //Failed: not detected!
		}
		input_lastwrite_8042(); //Force 0xFA to user!
		break;
	case 0xAA: //Test PS/2 controller! Result: 0x55: Test passed. 0xFC: Test failed.
		give_8042_input(0x55); //Always OK!
		input_lastwrite_8042(); //Force 0xFA to user!
		break;
	case 0xAB: //Test first PS/2 port! See Command A9!
		if (Controller8042.portwrite[0] && Controller8042.portread[0] && Controller8042.portpeek[0]) //Registered?
		{
			give_8042_input(0x00); //Passed: we have one!
		}
		else
		{
			give_8042_input(0x01); //Failed: not detected!
		}
		input_lastwrite_8042(); //Force 0xFA to user!
		break;
	case 0xAC: //Diagnostic dump: read all bytes of internal RAM!
		give_8042_input(Controller8042.RAM[0]); //Passed: give result!
		input_lastwrite_8042(); //Force byte 0 to user!
		byte c;
		for (c=1;c<0x20;c++) //Process all!
		{
			give_8042_input(Controller8042.RAM[c]); //Give result!
		}
		break;
	case 0xAD: //Disable first PS/2 port! No ACK!
		Controller8042.PS2ControllerConfigurationByte.FirstPortDisabled = 1; //Disabled!
		break;
	case 0xAE: //Enable first PS/2 port! No ACK!
		Controller8042.PS2ControllerConfigurationByte.FirstPortDisabled = 0; //Enabled!
		break;
	case 0xC0: //Read controller input port?
		if (!(PORT_IN_B(0x64)&2)) //Nothing to give?
		{
			give_8042_input(0xFF); //Just give something!
			break;
		}
		give_8042_input(Controller8042.input_buffer); //Give it fully!
		break;
	case 0xC1: //Copy bits 0-3 of input port to status bits 4-7. No ACK!
		Controller8042.status_buffer &= ~0xF0; //Clear bits 4-7!
		if (!(PORT_IN_B(0x64)&1)) //Nothing to give?
		{
			Controller8042.status_buffer = 0xF<<4; //Just nothing to give!
		}
		else //Something to give?
		{
			Controller8042.status_buffer |= ((Controller8042.input_buffer&0xF)<<4);
		}
		break;
	case 0xC2: //Copy bits 4-7 of input port to status bits 4-7. No ACK!
		Controller8042.status_buffer &= ~0xF0; //Clear bits 4-7!
		if (!(PORT_IN_B(0x64)&1)) //Nothing to give?
		{
			Controller8042.status_buffer = 0xF<<4; //Just nothing to give!
		}
		else //Something to give?
		{
			Controller8042.status_buffer |= (Controller8042.input_buffer&0xF0);
		}
		break;
	case 0xD0: //Next byte read from port 0x60 is read from the Controller 8042 output port!
		Controller8042.readoutputport = 1; //Next byte to port 0x60 is placed on the 8042 output port!
		Controller8042.Read_RAM = 0; //Not anymore!
		break;
	case 0xD1: //Next byte written to port 0x60 is placed on the Controller 8042 output port?
		Controller8042.writeoutputport = 1; //Next byte to port 0x60 is placed on the 8042 output port!
		Controller8042.Write_RAM = 0; //Not anymore!
	case 0xD2: //Next byte written to port 0x60 is send as from the First PS/2 port!
		Controller8042.port60toFirstPS2Input = 1;
		break;
	case 0xD3: //Next byte written to port 0x60 is send to the Second PS/2 port!
		Controller8042.port60toSecondPS2Input = 1;
		break;
	case 0xD4: //Next byte written to port 0x60 is written to the second PS/2 port
		Controller8042.has_port[1] = 1; //Send to second PS/2 port!
		break;
	case 0xF0: case 0xF1: case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF7: case 0xF8: case 0xF9: case 0xFA: case 0xFB: case 0xFC: case 0xFD: case 0xFE: case 0xFF: //Pulses!
		if (!(Controller8042.command&0x1)) //CPU reset?
		{
			if (!(Controller8042.outputport&0x1)) //PC reset triggered?
			{
				sleep(); //The CPU needs to lock up!
			}

			resetCPU(); //Process CPU reset!
		}
		break;
	default: //Default: output to the keyboard controller!
		//Unknown device!
		break;
	}
}

void refresh_outputport()
{
	MMU_wraparround(!((Controller8042.outputport&2)>>1)); //Enable/disable wrap arround depending on bit 2 (1=Disable wrap, 0=Enable wrap(disable A20 line))!
}

void datawritten_8042() //Data has been written?
{
	Controller8042.status_buffer &= ~0x8; //We have been written to!

	if (Controller8042.port60toFirstPS2Input || Controller8042.port60toSecondPS2Input)
	{
		Controller8042.input_buffer = Controller8042.output_buffer; //Write to input port!
		Controller8042.status_buffer |= 0x2; //Set input buffer full!
		if (Controller8042.port60toSecondPS2Input) //AUX port?
		{
			Controller8042.status_buffer |= 0x20; //Set AUX bit!
			if (Controller8042.PS2ControllerConfigurationByte.SecondPortInterruptEnabled)
			{
				doirq(12); //Call the interrupt if neccesary!
			}
		}
		else //Non-AUX?
		{
			Controller8042.status_buffer &= ~0x20; //Clear AUX bit!
			if (Controller8042.PS2ControllerConfigurationByte.FirstPortInterruptEnabled)
			{
				doirq(1); //Call the interrupt if neccesary!
			}
		}
		return; //Abort normal process!
	}

	if (!(Controller8042.has_port[0] || Controller8042.has_port[1])) //Neither port?
	{
		Controller8042.has_port[0] = 1; //Default to port 0!
	}

	byte c;
	for (c=0;c<2;c++) //Process output!
	{
		if (Controller8042.has_port[c]) //To this port?
		{
			if (Controller8042.portwrite[c]) //Gotten handler?
			{
				Controller8042.status_buffer &= ~0x1; //Cleared output buffer!
				Controller8042.portwrite[c](Controller8042.output_buffer); //Write data!
				Controller8042.has_port[c] = 0; //Reset!
				break; //Stop searching for a device to output!
			}
		}
	}
}

byte write_8042(word port, byte value)
{
switch (port) //What port?
{
case 0x60: //Data port: write output buffer?
	if (Controller8042.writeoutputport) //Write the output port?
	{
		Controller8042.outputport = value; //Write the output port directly!
		refresh_outputport(); //Handle the new output port!
		Controller8042.writeoutputport = 0; //Not anymore!
		Controller8042.status_buffer &= ~0x1; //Cleared output buffer!
		return 1; //Don't process normally!
	}
	if (Controller8042.Write_RAM) //Write to VRAM byte?
	{
		Controller8042.RAM[Controller8042.Write_RAM-1] = value; //Set data in RAM!
		Controller8042.Write_RAM = 0; //Not anymore!
		Controller8042.status_buffer &= ~0x1; //Cleared output buffer!
		return 1; //Don't process normally!
	}

	Controller8042.output_buffer = value; //Write to output buffer to process!

	datawritten_8042(); //Written handler!
	return 1;
	break;
case 0x64: //Command port: send command?
	Controller8042.command = value; //Set command!
	Controller8042.status_buffer &= ~0x1; //Cleared output buffer!
	commandwritten_8042(); //Written handler!
	return 1;
	break;
}
return 0; //We're unhandled!
}

byte read_8042(word port, byte *result)
{
switch (port)
{
case 0x60: //Data port: Read input buffer?
	if (Controller8042.readoutputport) //Read the output port?
	{
		*result = Controller8042.outputport; //Read the output port directly!
		return 1; //Don't process normally!
	}
	if (Controller8042.Read_RAM) //Write to VRAM byte?
	{
		*result = Controller8042.RAM[Controller8042.Read_RAM-1]; //Get data in RAM!
		Controller8042.Read_RAM = 0; //Not anymore!
		return 1; //Don't process normally!
	}
	
	fill8042_input_buffer(); //Fill the input buffer!
	if (Controller8042.status_buffer&2) //Gotten data?
	{
		*result = Controller8042.input_buffer; //Read input buffer!
		Controller8042.status_buffer &= ~0x22; //Clear input buffer full&AUX bits!
	}
	return 1; //We're processed!
	break;

case 0x64: //Command port: read status register?
	fill8042_input_buffer(); //Fill the input buffer if needed!
	*result = Controller8042.status_buffer; //Read status buffer!
	return 1; //We're processed!
	break;
}
return 0; //Undefined!
}

void BIOS_init8042() //Init 8042&Load all BIOS!
{
	if (__HW_DISABLED) return; //Abort!
	if (Controller8042.buffer) //Gotten a buffer?
	{
		free_fifobuffer(&Controller8042.buffer); //Release our buffer, if we have one!
	}
	Controller8042.buffer = allocfifobuffer(64); //Allocate a small buffer for us to use to commands/data!

	//First: initialise all hardware ports for emulating!
	register_PORTOUT(&write_8042);
	register_PORTIN(&read_8042);
	reset8042(); //First 8042 controller reset!
}

void BIOS_done8042()
{
	if (__HW_DISABLED) return; //Abort!
	free_fifobuffer(&Controller8042.buffer); //Free the buffer!
}

//Registration handlers!

void register_PS2PortWrite(byte port, PS2OUT handler)
{
	if (__HW_DISABLED) return; //Abort!
	port &= 1;
	Controller8042.portwrite[port] = handler; //Register!
}

void register_PS2PortRead(byte port, PS2IN handler, PS2PEEK peekhandler)
{
	if (__HW_DISABLED) return; //Abort!
	port &= 1;
	Controller8042.portread[port] = handler; //Register!
	Controller8042.portpeek[port] = peekhandler; //Peek handler!
}