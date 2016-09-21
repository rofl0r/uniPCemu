#include "headers/types.h" //Basic types!
#include "headers/hardware/pic.h" //Interrupt support (IRQ 1&12)
#include "headers/hardware/ports.h" //Port support!
#include "headers/hardware/8042.h" //Our own functions!
#include "headers/cpu/mmu.h" //For wraparround 1/3/5/... MB! (A20 line)
#include "headers/support/log.h" //Logging support!
#include "headers/support/locks.h" //Locking support!
#include "headers/hardware/ps2_keyboard.h" //Keyboard support!

//Are we disabled?
#define __HW_DISABLED 0

byte force8042 = 0; //Force 8042 controller handling?

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

void fill8042_output_buffer(byte flags) //Fill input buffer from full buffer!
{
	if (!(Controller8042.status_buffer&1)) //Buffer empty?
	{
		Controller8042.output_buffer = 0; //Undefined to start with!

		if (readfifobuffer(Controller8042.buffer, &Controller8042.output_buffer)) //Gotten something from 8042?
		{
			Controller8042.status_buffer &= ~0x20; //Clear AUX bit!
			Controller8042.status_buffer |= 0x1; //Set output buffer full!
		}
		else //Input from hardware?
		{
			byte portorder;
			for (portorder = 0;portorder < 2;portorder++) //Process all our ports!
			{
				byte whatport = ControllerPriorities[portorder]; //The port to check!
				if (whatport < 2) //Port has priority and available?
				{
					if (Controller8042.portread[whatport] && Controller8042.portpeek[whatport]) //Read handlers from the first PS/2 port available?
					{
						if (Controller8042.portpeek[whatport](&Controller8042.output_buffer)) //Got something?
						{
							Controller8042.output_buffer = Controller8042.portread[whatport](); //Execute the handler!
							Controller8042.status_buffer |= 0x1; //Set input buffer full!
							if (whatport) //AUX port?
							{
								Controller8042.status_buffer |= 0x20; //Set AUX bit!
								if (Controller8042.PS2ControllerConfigurationByte.SecondPortInterruptEnabled)
								{
									if (flags&1) raiseirq(12); //Raise secondary IRQ!
								}
								else
								{
									if (flags&1)
									{
										lowerirq(12); //Lower secondary IRQ!
										acnowledgeIRQrequest(12); //Acnowledge!
									}
								}
								if (flags&1)
								{
									lowerirq(1); //Lower primary IRQ!
									acnowledgeIRQrequest(1); //Acnowledge!
								}
							}
							else //Non-AUX?
							{
								Controller8042.status_buffer &= ~0x20; //Clear AUX bit!

								if (Controller8042.PS2ControllerConfigurationByte.FirstPortInterruptEnabled)
								{
									if (flags&1) raiseirq(1); //Raise primary IRQ!
								}
								else
								{
									if (flags&1)
									{
										lowerirq(1); //Lower primary IRQ!
										acnowledgeIRQrequest(1); //Acnowledge!
									}
								}
								if (flags&1)
								{
									lowerirq(12); //Lower secondary IRQ!
									acnowledgeIRQrequest(12); //Acnowledge!
								}
							}
							break; //Finished!
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
		Controller8042.writeoutputport = (byte)(Controller8042.inputtingsecurity = 0); //Not anymore!
		break;
	case 0x20: //Read byte 0 from internal RAM!
	case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x2C: case 0x2D: case 0x2E: case 0x2F: //Read "byte X" from internal RAM!
		Controller8042.Read_RAM = Controller8042.command-0x1F; //Read from internal RAM (value 0x01-0x20), so 1 more than our index!
		Controller8042.readoutputport = (byte)(Controller8042.inputtingsecurity = 0); //Not anymore!
		break;
	case 0xA4: //Password Installed Test
		input_lastwrite_8042(); //Force data to user!
		give_8042_input((Controller8042.has_security && (Controller8042.securitychecksum==Controller8042.securitykey))?0xFA:0xF1); //Passed: give result! No password present!
		input_lastwrite_8042(); //Force byte 0 to user!
		break;
	case 0xA5: //Load security
		Controller8042.inputtingsecurity = 1; //We're starting to input security data until a 0 is found!
		Controller8042.securitychecksum = 0; //Init checksum!
		break;
	case 0xA6: //Enable Security
		//Unknown what to do? Simply set the new key?
		Controller8042.inputtingsecurity = 0; //Finished!
		Controller8042.securitykey = Controller8042.securitychecksum; //Set the new security key!
		break;
	case 0xA7: //Disable second PS/2 port! No ACK!
		Controller8042.PS2ControllerConfigurationByte.SecondPortDisabled = 1; //Disabled!
		break;
	case 0xA8: //Enable second PS/2 port! ACK from keyboard!
		Controller8042.PS2ControllerConfigurationByte.SecondPortDisabled = 0; //Enabled!
		input_lastwrite_8042(); //Force 0xFA to user!
		give_keyboard_input(0xFA); //ACK!
		input_lastwrite_8042(); //Force 0xFA to user!
		IRQ8042(1); //Process the input!
		break;
	case 0xA9: //Test second PS/2 port! Give 0x00 if passed (detected). 0x02-0x04=Not detected?
		input_lastwrite_8042(); //Force result to user!
		if (Controller8042.portwrite[1] && Controller8042.portread[1] && Controller8042.portpeek[1]) //Registered?
		{
			give_8042_input(0x00); //Passed: we have one!
		}
		else
		{
			give_8042_input(0x02); //Failed: not detected!
		}
		input_lastwrite_8042(); //Force 0xFA to user!
		break;
	case 0xAA: //Test PS/2 controller! Result: 0x55: Test passed. 0xFC: Test failed.
		input_lastwrite_8042(); //Force 0xFA to user!
		give_8042_input(0xFA); //ACK!
		input_lastwrite_8042(); //Force 0xFA to user!
		give_8042_input(0x55); //Always OK!
		break;
	case 0xAB: //Test first PS/2 port! See Command A9!
		input_lastwrite_8042(); //Force 0xFA to user!
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
		input_lastwrite_8042(); //Force data to user!
		give_8042_input(Controller8042.RAM[0]); //Passed: give result!
		input_lastwrite_8042(); //Force byte 0 to user!
		byte c;
		for (c=1;c<0x20;) //Process all!
		{
			give_8042_input(Controller8042.RAM[c++]); //Give result!
		}
		break;
	case 0xAD: //Disable first PS/2 port! No ACK!
		Controller8042.PS2ControllerConfigurationByte.FirstPortDisabled = 1; //Disabled!
		break;
	case 0xAE: //Enable first PS/2 port! No ACK!
		Controller8042.PS2ControllerConfigurationByte.FirstPortDisabled = 0; //Enabled!
		break;
	case 0xC0: //Read controller input port?
		input_lastwrite_8042(); //Force 0xFA to user!
		give_8042_input(Controller8042.inputport); //Give it fully!
		input_lastwrite_8042(); //Force 0xFA to user!
		break;
	case 0xC1: //Copy bits 0-3 of input port to status bits 4-7. No ACK!
		Controller8042.status_buffer &= ~0xF0; //Clear bits 4-7!
		Controller8042.status_buffer |= ((Controller8042.inputport&0xF)<<4);
		break;
	case 0xC2: //Copy bits 4-7 of input port to status bits 4-7. No ACK!
		Controller8042.status_buffer &= ~0xF0; //Clear bits 4-7!
		Controller8042.status_buffer |= (Controller8042.inputport&0xF0);
		break;
	case 0xD0: //Next byte read from port 0x60 is read from the Controller 8042 output port!
		Controller8042.readoutputport = 1; //Next byte to port 0x60 is placed on the 8042 output port!
		Controller8042.Read_RAM = 0; //Not anymore!
		break;
	case 0xD1: //Next byte written to port 0x60 is placed on the Controller 8042 output port?
		Controller8042.writeoutputport = 1; //Next byte to port 0x60 is placed on the 8042 output port!
		Controller8042.inputtingsecurity = 0; //Not anymore!
		Controller8042.Write_RAM = 0; //Not anymore!
	case 0xD2: //Next byte written to port 0x60 is send as from the First PS/2 port!
		Controller8042.port60toFirstPS2Output = 1;
		Controller8042.inputtingsecurity = 0; //Not anymore!
		Controller8042.Write_RAM = 0; //Not anymore!
		break;
	case 0xD3: //Next byte written to port 0x60 is send to the Second PS/2 port!
		Controller8042.port60toSecondPS2Output = 1;
		Controller8042.inputtingsecurity = 0; //Not anymore!
		Controller8042.Write_RAM = 0; //Not anymore!
		Controller8042.port60toFirstPS2Output = 0;
		break;
	case 0xD4: //Next byte written to port 0x60 is written to the second PS/2 port
		Controller8042.has_port[1] = 1; //Send to second PS/2 port!
		Controller8042.has_port[0] = 0; //Send to second PS/2 port!
		Controller8042.inputtingsecurity = 0; //Not anymore!
		Controller8042.Write_RAM = 0; //Not anymore!
		Controller8042.port60toSecondPS2Output = 0;
		Controller8042.port60toFirstPS2Output = 0;
		break;
	case 0xE0: //Read test inputs?
		input_lastwrite_8042(); //Force data to user!
		give_8042_input(0); //Passed: give result! Bit0=Clock, Bit1=Data
		input_lastwrite_8042(); //Force byte 0 to user!
		break;
	case 0xF0: case 0xF1: case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF7: case 0xF8: case 0xF9: case 0xFA: case 0xFB: case 0xFC: case 0xFD: case 0xFE: case 0xFF: //Pulses!
		if (!(Controller8042.command&0x1)) //CPU reset (pulse line 0)?
		{
			unlock(LOCK_CPU);
			doneCPU();
			resetCPU(); //Process CPU reset!
			lock(LOCK_CPU); //Relock the CPU!
		}
		break;
	case 0xDD: //Disable A20 line?
		Controller8042.outputport = Controller8042.outputport&(~0x2); //Wrap arround: disable A20 line!
		refresh_outputport(); //Handle the new output port!
		break;
	case 0xDF: //Enable A20 line?
		Controller8042.outputport = Controller8042.outputport|(0x2); //Wrap arround: enable A20 line!
		refresh_outputport(); //Handle the new output port!
		break;
	default: //Default: output to the keyboard controller!
		//Unknown device!
		break;
	}
}

void refresh_outputport()
{
	MMU_setA20(0,(Controller8042.outputport&2)); //Enable/disable wrap arround depending on bit 2 (1=Enable, 0=Disable)!
}

void datawritten_8042() //Data has been written?
{
	if (Controller8042.port60toFirstPS2Output || Controller8042.port60toSecondPS2Output) //port 60 to first/second PS2 output?
	{
		Controller8042.output_buffer = Controller8042.input_buffer; //Input to output!
		Controller8042.status_buffer &= ~0x2; //Cleared ougoing command buffer!
		Controller8042.status_buffer |= 0x1; //Set output buffer full!
		if (Controller8042.port60toSecondPS2Output) //AUX port?
		{
			Controller8042.status_buffer |= 0x20; //Set AUX bit!
			if (Controller8042.PS2ControllerConfigurationByte.SecondPortInterruptEnabled)
			{
				lowerirq(1); //Remove the keyboard IRQ!
				acnowledgeIRQrequest(1); //Acnowledge!
				raiseirq(12); //Call the interrupt if neccesary!
			}
		}
		else //Non-AUX?
		{
			Controller8042.status_buffer &= ~0x20; //Clear AUX bit!
			if (Controller8042.PS2ControllerConfigurationByte.FirstPortInterruptEnabled)
			{
				lowerirq(12); //Remove the mouse IRQ!
				acnowledgeIRQrequest(12); //Acnowledge!
				raiseirq(1); //Call the interrupt if neccesary!
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
				Controller8042.status_buffer &= ~0x2; //Cleared input buffer!
				Controller8042.portwrite[c](Controller8042.input_buffer); //Write data!
				Controller8042.has_port[c] = 0; //Reset!
				break; //Stop searching for a device to output!
			}
		}
	}
	fill8042_output_buffer(1); //Update the input buffer!
}

byte write_8042(word port, byte value)
{
	if ((port & 0xFFF8) != 0x60) return 0; //Not our port!
	switch (port) //What port?
	{
	case 0x60: //Data port: write output buffer?
		Controller8042.status_buffer &= ~0x8; //We've last sent a byte to the data port!
		if (Controller8042.inputtingsecurity) //Inputting security string?
		{
			Controller8042.securitychecksum += value; //Add to the value!
			if (value==0) Controller8042.inputtingsecurity = 0; //Finished inputting?
			return 1; //Don't process normally!
		}
		if (Controller8042.writeoutputport) //Write the output port?
		{
			Controller8042.outputport = value; //Write the output port directly!
			Controller8042.status_buffer &= ~0x2; //Cleared output buffer!
			refresh_outputport(); //Handle the new output port!
			Controller8042.writeoutputport = 0; //Not anymore!
			return 1; //Don't process normally!
		}
		if (Controller8042.Write_RAM) //Write to VRAM byte?
		{
			Controller8042.RAM[Controller8042.Write_RAM-1] = value; //Set data in RAM!
			Controller8042.Write_RAM = 0; //Not anymore!
			Controller8042.status_buffer &= ~0x2; //Cleared output buffer!
			return 1; //Don't process normally!
		}

		Controller8042.input_buffer = value; //Write to output buffer to process!

		datawritten_8042(); //Written handler!
		return 1;
		break;
	case 0x61: //PPI keyboard functionality for XT!
		if (EMULATED_CPU<CPU_80286) //XT machine only?
		{
			if (value & 0x80) //Clear interrupt flag and we're a XT system?
			{
				Controller8042.status_buffer &= ~0x21; //Clear input buffer full&AUX bits!
				fill8042_output_buffer(1); //Fill the next byte to use!
			}
			if (((value^0x40)==(Controller8042.PortB&0x40)) && ((value)&0x40)) //Set when unset?
			{
				resetKeyboard_8042(1); //Reset the keyboard manually! Execute an interrupt when reset!
			}
			Controller8042.PortB = (value&0xC0); //Save values for reference!
			return 1;
		}
		break;
	case 0x64: //Command port: send command?
		Controller8042.status_buffer |= 0x8; //We've last sent a byte to the command port!
		Controller8042.command = value; //Set command!
		Controller8042.status_buffer &= ~0x2; //Cleared output buffer!
		commandwritten_8042(); //Written handler!
		return 1;
		break;
	}
	return 0; //We're unhandled!
}

byte read_8042(word port, byte *result)
{
	if ((port & 0xFFF8) != 0x60) return 0; //Not our port!
	switch (port)
	{
	case 0x60: //Data port: Read input buffer?
		if (Controller8042.readoutputport) //Read the output port?
		{
			*result = Controller8042.outputport; //Read the output port directly!
			fill8042_output_buffer(1); //Fill the input buffer if needed!
			return 1; //Don't process normally!
		}
		if (Controller8042.Read_RAM) //Write to VRAM byte?
		{
			*result = Controller8042.RAM[Controller8042.Read_RAM-1]; //Get data in RAM!
			Controller8042.Read_RAM = 0; //Not anymore!
			fill8042_output_buffer(1); //Fill the input buffer if needed!
			return 1; //Don't process normally!
		}
	
		fill8042_output_buffer(1); //Fill the input buffer if needed!
		if (Controller8042.status_buffer&1) //Gotten data?
		{
			*result = Controller8042.output_buffer; //Read output buffer!
			if ((EMULATED_CPU>=CPU_80286) || force8042) //We're an AT system?
			{
				Controller8042.status_buffer &= ~0x21; //Clear output buffer full&AUX bits!
				fill8042_output_buffer(1); //Get the next byte if needed!
			}
		}
		else
		{
			*result = 0; //Nothing to give: input buffer is empty!
		}
		return 1; //We're processed!
		break;
	case 0x61: //PPI keyboard functionality for XT!
		//We don't handle this, ignore us!
		*result = 0;
		return 1; //Force us to 0 by default!
		break;
	case 0x64: //Command port: read status register?
		if ((EMULATED_CPU >= CPU_80286) || force8042) fill8042_output_buffer(1); //Fill the output buffer if needed!
		*result = (Controller8042.status_buffer|0x10)|(Controller8042.PS2ControllerConfigurationByte.SystemPassedPOST<<2); //Read status buffer combined with the BIOS POST flag! We're never inhabited!
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
	Controller8042.buffer = allocfifobuffer(64,1); //Allocate a small buffer for us to use to commands/data!

	//First: initialise all hardware ports for emulating!
	register_PORTOUT(&write_8042);
	register_PORTIN(&read_8042);
	reset8042(); //First 8042 controller reset!
	if (EMULATED_CPU >= CPU_80286) //IBM AT? We're setting up the input port!
	{
		Controller8042.inputport = 0x80|0x20; //Keyboard not inhibited, Manufacturing jumper not installed.
		switch (BIOS_Settings.VGA_Mode) //What VGA mode?
		{
		case 4: //Pure CGA?
			break; //Report CGA1
		case 5: //Pure MDA?
			Controller8042.inputport |= 0x40; //Report MDA adapter!
			break; //Report MDA!
		default: //(S)VGA?
			break; //Report CGA!
		}
	}
}

void BIOS_done8042()
{
	if (__HW_DISABLED) return; //Abort!
	free_fifobuffer(&Controller8042.buffer); //Free the buffer!
}

void IRQ8042(byte flags) //Generates any IRQs needed on the 8042!
{
	fill8042_output_buffer(flags); //Fill our input buffer!
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
