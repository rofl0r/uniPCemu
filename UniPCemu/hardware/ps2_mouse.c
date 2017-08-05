#include "headers/hardware/8042.h" //PS/2 Controller support!
#include "headers/hardware/ps2_mouse.h" //Our support!
#include "headers/support/fifobuffer.h"
#include "headers/support/zalloc.h" //Zero free allocation for linked lists!
#include "headers/emu/input.h" //For timing packets!

//To disable the mouse?
#define MOUSE_DISABLED Mouse.disabled

//Are we disabled?
#define __HW_DISABLED 0

extern Controller8042_t Controller8042; //The 8042 controller!

struct
{
	union //All mouse data!
	{
		struct
		{
			byte has_command; //Have command?
			byte command_step; //Command step!
			byte command; //What command has been written?
			byte last_was_error; //Last command was an error?

			/*byte wrap_mode; //Pass data back to the CPU?
			byte stream_mode; //Automatic stream? =Data reporting.
			byte remote_mode; //Same as stream mode, but no IRQ!
			*/
			byte mode; //0=default;1=wrap;2=stream;3=remote!
			byte lastmode; //Last mode!
			byte data_reporting; //Use data reporting?

			byte packetindex; //For helping processing the current byte of a mouse packet!

			byte samplerate; //Sample rate!
			byte resolution; //Counts/mm
			byte scaling21; //1:1=0 or 2:1=1
			byte Resend; //Use last read once?
			byte buttonstatus; //Button status for status bytes!
			byte disabled; //Is the mouse input disabled atm?
		};
		byte data[14]; //All mouse data!
	};
	FIFOBUFFER *buffer; //FIFO buffer for commands etc.

	MOUSE_PACKET *packets; //Contains all packets!
	MOUSE_PACKET *lastpacket; //Last send packet!
	byte supported; //PS/2 mouse supported on this system?
	double timeout; //Timeout for reset commands!
} Mouse; //Ourselves!

OPTINLINE void give_mouse_output(byte data)
{
	if (__HW_DISABLED) return; //Abort!
	writefifobuffer(Mouse.buffer,data); //Write to the buffer, ignore the result!
}

OPTINLINE void input_lastwrite_mouse()
{
	if (__HW_DISABLED) return; //Abort!
	fifobuffer_gotolast(Mouse.buffer); //Goto last!
}

OPTINLINE void next_mousepacket() //Reads the next mouse packet, if any!
{
	if (__HW_DISABLED) return; //Abort!
	MOUSE_PACKET *oldpacket = Mouse.lastpacket; //Save the last packet!
	MOUSE_PACKET *currentpacket = Mouse.packets; //Current packet!
	if (currentpacket) //Gotten a packet?
	{
		Mouse.packets = currentpacket->next; //Set to next packet!
		Mouse.lastpacket = currentpacket; //Move the now old packet to the last packet!
		if (oldpacket!=Mouse.lastpacket) //Original old packet is finished (not a resend)?
		{
			freez((void **)&oldpacket,sizeof(MOUSE_PACKET),"Old MousePacket"); //Release the last packet!
		}
	}
	//Else: nothing to next!
}

OPTINLINE void flushPackets() //Flushes all mouse packets!
{
	if (__HW_DISABLED) return; //Abort!
	while (Mouse.packets)
	{
		next_mousepacket(); //Flush all packets!
	}
	if (Mouse.lastpacket)
	{
		freez((void **)&Mouse.lastpacket,sizeof(MOUSE_PACKET),"Mouse_FlushPacket");
	}
	Mouse.buttonstatus = 0; //No buttons!
	
}

OPTINLINE void resend_lastpacket() //Resends the last packet!
{
	if (__HW_DISABLED) return; //Abort!
	if (Mouse.lastpacket && !Mouse.packetindex) //Gotten a last packet and at the start of a packet?
	{
		Mouse.packets = Mouse.lastpacket; //Resend the last packet!
	}
	//If we don't have a last packet, take the start of the current packet, else the start of the last packet!
	Mouse.packetindex = 0; //Reset packet index always!
}

OPTINLINE byte add_mouse_packet(MOUSE_PACKET *packet) //Add an allocated mouse packet!
{
	if (__HW_DISABLED) return 1; //Abort!
	if (Mouse.buttonstatus == packet->buttons) //Same button status?
	{
		if (!packet->xmove && !packet->ymove) //Nothing happened?
		{
			freez((void **)&packet, sizeof(MOUSE_PACKET), "Mouse_EmptyPacket"); //Discard the packet!
			return 0; //Discard the packet!
		}
	}
	Mouse.buttonstatus = packet->buttons; //Save the current button status!
	MOUSE_PACKET *currentpacket = Mouse.packets; //Current packet!
	if (Mouse.packets) //Already have one?
	{
		while (currentpacket->next) //Gotten next?
		{
			currentpacket = (MOUSE_PACKET *)currentpacket->next; //Next packet!
		}
		currentpacket->next = packet; //Set next packet to the new packet!
	}
	else
	{
		Mouse.packets = packet; //Set as current packet!
	}
	return 1; //Packet ready!
}

//Handle a mouse packet!
byte PS2mouse_packet_handler(MOUSE_PACKET *packet) //Packet muse be allocated using zalloc!
{
	if (__HW_DISABLED) return 0; //Abort!
	if (!Mouse.supported) return 0; //PS/2 mouse not supported!
	if (!PS2_SECONDPORTDISABLED(Controller8042)) //We're enabled?
	{
		if (add_mouse_packet(packet)) //Add a mouse packet, and according to timing!
		{} //Do nothing when added!
	}
	return 1; //We're supported!
}

OPTINLINE float HWmouse_getsamplerate() //Which repeat rate to use after the repeat delay! (chars/second)
{
	if (__HW_DISABLED) return 1.0f; //Abort!
	float result;
	result = Mouse.samplerate; //Get result!
	return result; //Give the repeat rate!
}

int useMouseTimer()
{
	if (__HW_DISABLED) return 0; //Abort!
	if (MOUSE_DISABLED) return 0; //No usage!
	if (PS2_SECONDPORTDISABLED(Controller8042)) return 0; //No usage!
	return 1; //We're enabled!
}

void update_mouseTimer()
{
	if (__HW_DISABLED) return; //Abort!
	if (MOUSE_DISABLED) return; //Mouse disabled?
	setMouseRate(HWmouse_getsamplerate()); //Start using this samplerate!
}

OPTINLINE void resetPS2Mouse()
{
	if (__HW_DISABLED) return; //Abort!
	flushPackets(); //Flush all packets!
	memset(&Mouse.data,0,sizeof(Mouse.data)); //Reset the mouse!
	//No data reporting!
	Mouse.resolution = 0x02; //4 pixel/mm resolution!
}

void updatePS2Mouse(double timepassed)
{
	if (Mouse.timeout) //Gotten a timeout?
	{
		Mouse.timeout -= timepassed; //Pass some time!
		if (Mouse.timeout <= 0.0) //Done?
		{
			Mouse.timeout = (double)0; //Finished!
			switch (Mouse.command) //What command to execute?
			{
				case 0xFF:
					switch (Mouse.command_step) //What step?
					{
					case 1: //First stage?
						input_lastwrite_keyboard(); //Force 0x00(dummy byte) to user!
						give_mouse_output(0xFA); //Acnowledge!
						resetPS2Mouse(); //Reset the Keyboard Controller! Don't give a result(this will be done in time)!
						Mouse.timeout = MOUSE_DEFAULTTIMEOUT; //A small delay for the result code to appear!
						Mouse.command_step = 2; //Step 2!
						Mouse.command = 0xFF; //Restore the command byte, so that we can continue!
						Mouse.has_command = 1; //We're stil executing a command!
						Mouse.last_was_error = 0; //Last is OK!
						break;
					case 2: //Final stage?
						give_keyboard_output(0xAA); //Give the result code!
						Mouse.timeout = MOUSE_DEFAULTTIMEOUT; //A small delay for the result code to appear!
						Mouse.command_step = 3; //Step 3! Give the ID next!
						break;
					case 3: //ID mode?
						Mouse.timeout = (double)0; //Finished!
						give_keyboard_output(0x00); //Give the ID code: PS/2 mouse!
						Mouse.command_step = 0; //Finished!
						Mouse.has_command = 0; //Finished command!
						break;
					}
					break;
			}
		}
	}
}

OPTINLINE void initPS2Mouse()
{
	if (__HW_DISABLED) return; //Abort!
	flushPackets(); //Flush all packets!
	memset(&Mouse.data,0,sizeof(Mouse.data)); //Reset the mouse!
	//No data reporting!
	Mouse.resolution = 0x02; //4 pixel/mm resolution!
	
	Mouse.command = 0xFF; //Reset!
	Mouse.timeout = 100000.0; //Start timing for our message!
}

OPTINLINE void mouse_handleinvalidcall()
{
	if (__HW_DISABLED) return; //Abort!
	if (!Mouse.last_was_error) //Last wasn't an error?
	{
		give_mouse_output(0xFE); //NACK!
		input_lastwrite_mouse(); //Give byte to the user!		
	}
	else //Error!
	{
		give_mouse_output(0xFC); //Error!
		input_lastwrite_mouse(); //Give byte to the user!
	}
	Mouse.last_was_error = 1; //Last was an error!
}

OPTINLINE void give_mouse_status() //Gives the mouse status buffer!
{
	if (__HW_DISABLED) return; //Abort!
	byte buttonstatus = (
				((Mouse.buttonstatus&1)<<2)| //Left button!
				((Mouse.buttonstatus&4)>>1)| //Middle button!
				((Mouse.buttonstatus&2)>>1) //Right button!
				); //Button status!
	give_mouse_output(
				(
				((Mouse.mode==3)?0x40:0)| //Remove/stream mode?
				(Mouse.data_reporting?0x20:0)| //Data reporting?
				(Mouse.scaling21?0x10:0)| //Scaling 2:1?
				buttonstatus //Apply left-middle-right bits!
				)
				); //Give the info!
	give_mouse_output(Mouse.resolution); //2nd byte is the resolution!
	give_mouse_output(Mouse.samplerate); //3rd byte is the sample rate!
}

OPTINLINE void loadMouseDefaults() //Load the Mouse Defaults!
{
	Mouse.data_reporting = 0;
	Mouse.samplerate = 100; //100 packets/second!
	update_mouseTimer(); //Update the timer!
	Mouse.resolution = 4; //4 Pixels/mm!
	Mouse.scaling21 = 0; //Set the default scaling to 1:1 scaling!
}

OPTINLINE void commandwritten_mouse() //Command has been written to the mouse?
{
	if (__HW_DISABLED) return; //Abort!
	Mouse.has_command = 1; //We have a command!
	Mouse.command_step = 0; //Reset command step!
	//Handle mouse!

	switch (Mouse.command) //What command?
	{
		case 0xFF: //Reset?
			Mouse.timeout = MOUSE_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
			break;
		case 0xFE: //Resend?
			Mouse.has_command = 0; //We're not a command anymore!
			resend_lastpacket(); //Resend the last (if possible) or current packet.
			input_lastwrite_mouse(); //Give byte to the user!
			Mouse.last_was_error = 0; //Last is OK!
			break; //Not used?
		case 0xF6: //Set defaults!
			//Set us to: 
			Mouse.mode = 0; //Reset mode!
			loadMouseDefaults(); //Load our defaults!
			Mouse.has_command = 0; //We're not a command anymore!
			give_mouse_output(0xFA); //Acnowledge!
			input_lastwrite_mouse(); //Give byte to the user!
			Mouse.last_was_error = 0; //Last is OK!
			break;
		case 0xF5: //Disable data reporting?
			Mouse.has_command = 0; //We're not a command anymore!
			Mouse.data_reporting = 0; //Disable data reporting!
			give_mouse_output(0xFA); //Acnowledge!
			input_lastwrite_mouse(); //Give byte to the user!
			Mouse.last_was_error = 0; //Last is OK!
			break;
		case 0xF4: //Enable data reporting?
			Mouse.has_command = 0; //We're not a command anymore!
			Mouse.data_reporting = 1; //Enable data reporting!
			give_mouse_output(0xFA); //Acnowledge!
			input_lastwrite_mouse(); //Give byte to the user!
			Mouse.last_was_error = 0; //Last is OK!
			break;
		case 0xF3: //Set sample rate?
			give_mouse_output(0xFA); //Acnowledge!
			input_lastwrite_mouse(); //Give byte to the user!
			//We're expecting parameters!
			Mouse.last_was_error = 0; //Last is OK!
			break;
		case 0xF2: //Get device ID?
			Mouse.has_command = 0; //We're not a command anymore!
			give_mouse_output(0xFA); //Acnowledge!
			input_lastwrite_mouse(); //Give byte to the user!
			give_mouse_output(0x00); //Standard mouse!
			Mouse.last_was_error = 0; //Last is OK!
			break;
		case 0xF0: //Set Remote Mode?
			Mouse.has_command = 0; //We're not a command anymore!
			Mouse.data_reporting = 0; //Disable data reporting!
			Mouse.mode = 3; //Remote mode
			give_mouse_output(0xFA); //Acnowledge!
			input_lastwrite_mouse(); //Give byte to the user!
			flushPackets(); //Flush our packets!
			Mouse.last_was_error = 0; //Last is OK!
			break;
		case 0xEE: //Set Wrap Mode?
			Mouse.has_command = 0; //We're not a command anymore!
			Mouse.lastmode = Mouse.mode; //Save the last mode!
			Mouse.mode = 1; //Enter wrap mode!
			Mouse.data_reporting = 0; //Disable data reporting!
			give_mouse_output(0xFA); //Acnowledge!
			input_lastwrite_mouse(); //Give byte to the user!
			flushPackets(); //Flush our packets!
			Mouse.last_was_error = 0; //Last is OK!
			break;
		case 0xEC: //Reset Wrap Mode?
			Mouse.has_command = 0; //We're not a command anymore!
			Mouse.mode = Mouse.lastmode; //Restore the last mode we were in!
			give_mouse_output(0xFA); //Acnowledge!
			input_lastwrite_mouse(); //Give byte to the user!
			flushPackets(); //Flush our packets!
			Mouse.last_was_error = 0; //Last is OK!			
			break;
		case 0xEB: //Read data?
			Mouse.has_command = 0; //We're not a command anymore!
			give_mouse_output(0xFA); //OK!
			//Already ready for receiving a packet!
			Mouse.last_was_error = 0; //Last is OK!
			break;
		case 0xEA: //Set stream mode?
			Mouse.has_command = 0; //We're not a command anymore!
			Mouse.data_reporting = 1; //Enable data reporting!
			Mouse.mode = 2; //Set stream mode!
			give_mouse_output(0xFA); //Acnowledge!
			input_lastwrite_mouse(); //Give byte to the user!
			flushPackets(); //Flush our packets!
			Mouse.last_was_error = 0; //Last is OK!
			break;
		case 0xE9: //Status request?
			Mouse.has_command = 0; //We're not a command anymore!
			give_mouse_output(0xFA); //Acnowledge!
			input_lastwrite_mouse(); //Give byte to the user!
			give_mouse_status(); //Give the status!
			Mouse.last_was_error = 0; //Last is OK!
			break;
		case 0xE8: //Set resolution?
			give_mouse_output(0xFA); //Acnowledge!
			Mouse.last_was_error = 0; //Last is OK!
			break;
		case 0xE7: //Set Scaling 2:1?
			Mouse.has_command = 0; //We're not a command anymore!
			Mouse.scaling21 = 1; //Set it!
			give_mouse_output(0xFA); //Acnowledge!
			input_lastwrite_mouse(); //Give byte to the user!
			Mouse.last_was_error = 0; //Last is OK!
			break;
		case 0xE6: //Set Scaling 1:1?
			Mouse.has_command = 0; //We're not a command anymore!
			Mouse.scaling21 = 0; //Set it!
			give_mouse_output(0xFA); //Acnowledge!
			input_lastwrite_mouse(); //Give byte to the user!
			Mouse.last_was_error = 0; //Last is OK!
			break;
		default:
			mouse_handleinvalidcall(); //Give an error!
			Mouse.has_command = 0; //We don't have a command anymore: we ignore the mouse?
			break;
		
	}

	if (Mouse.has_command) //Still a command?
	{
		++Mouse.command_step; //Next step (step starts at 1 always)!
	}
}

OPTINLINE byte mouse_is_command(byte data) //Command has been written to the mouse?
{
	if (__HW_DISABLED) return 1; //Abort!
	//Handle mouse!

	switch (data) //What command?
	{
		case 0xFF: //Reset?
		case 0xFE: //Resend?
		case 0xF6: //Set defaults!
		case 0xF5: //Disable data reporting?
		case 0xF4: //Enable data reporting?
		case 0xF3: //Set sample rate?
		case 0xF2: //Get device ID?
		case 0xF0: //Set Remote Mode?
		case 0xEE: //Set Wrap Mode?
		case 0xEC: //Reset Wrap Mode?
		case 0xEB: //Read data?
		case 0xEA: //Set stream mode?
		case 0xE9: //Status request?
		case 0xE8: //Set resolution?
		case 0xE7: //Set Scaling 2:1?
		case 0xE6: //Set Scaling 1:1?
			return 1;
			break;
		default:
			return 0;
			break;
		
	}
	return 0;
}

OPTINLINE void datawritten_mouse(byte data) //Data has been written to the mouse?
{
	if (__HW_DISABLED) return; //Abort!
	switch (Mouse.command) //What command?
	{
		case 0xF3: //Set sample rate?
			Mouse.samplerate = data; //Set the sample rate (in samples/second)!
			update_mouseTimer(); //Update the timer!
			give_mouse_output(0xFA); //Acnowledge!
			input_lastwrite_mouse(); //Give byte to the user!
			break;
		case 0xE8: //Set resolution?
			Mouse.resolution = data; //Set the resolution!
			give_mouse_output(0xFA); //Acnowledge!
			input_lastwrite_mouse(); //Give byte to the user!			
			break;
		default: //Invalid command?
			mouse_handleinvalidcall(); //Give an error!
			Mouse.has_command = 0; //We don't have a command anymore!
			break;
	}
	if (Mouse.has_command) //Still a command?
	{
		++Mouse.command_step; //Next step (step starts at 1 always)!
	}
}

void handle_mousewrite(byte data)
{
	if (__HW_DISABLED) return; //Abort!
	if ((!Mouse.has_command) || mouse_is_command(data)) //Not processing a command or issuing a new command?
	{
		Mouse.command = data; //Becomes a command!
		commandwritten_mouse(); //Process mouse command?
	}
	else //Data?
	{
		datawritten_mouse(data); //Data has been written!
	}
	if (!Mouse.has_command) //No command anymore?
	{
		Mouse.command_step = 0; //Reset command step!
	}	
}

OPTINLINE int apply_resolution(int movement) //Apply movement from joystick -255 - +255!
{
	if (__HW_DISABLED) return 0; //Abort!
	switch (Mouse.resolution) //What resolution?
	{
		case 0x00: case 0x01: case 0x02: case 0x03:
			return movement<<Mouse.resolution; //1/2/4/8 count/mm!
		default: //Unknown?
			return movement; //Unchanged!
	}
}

OPTINLINE int apply_scaling(int movement) //Apply scaling on a mouse packet x/ymove!
{
	if (__HW_DISABLED) return 0; //Abort!
	if (!Mouse.scaling21) return movement; //Unchanged!
	switch (movement)
	{
		case 0:	return 0; //No movement!
		case 1: return 1; //1!
		case -1: return -1; //-1!
		case 2: return 1; //1!
		case -2: return -1; //-1!
		case 3: return 3; //3!
		case -3: return -3; //-3!
		case 4: return 6; //6!
		case -4: return -6; //-6!
		case 5: return 9; //9!
		case -5: return -9; //-9!
		default:
			return 2*movement; //2*Movement counter!
	}	
}

OPTINLINE int applypacketmovement(int movement)
{
	if (__HW_DISABLED) return 0; //Abort!
	return apply_scaling(apply_resolution(movement)); //Apply resolution (in mm), then scaling!
}

OPTINLINE byte processMousePacket(MOUSE_PACKET *packet, byte index)
{
	if (__HW_DISABLED) return 0; //Abort!
	if (!packet) return 0; //Nothing to process!
	//First process all movement info!
	int packetmovementx = applypacketmovement(packet->xmove); //Apply x movement!
	int packetmovementy = applypacketmovement(packet->ymove); //Apply y movement!
	
	switch (index) //What index?
	{
		case 0:
			return (
				((packetmovementy<-255 || packetmovementy>255)?0x80:0)| //Y overflow?
				((packetmovementx<-255 || packetmovementx>255)?0x40:0)| //X overflow?
				((packetmovementy<0)?0x20:0)| //Y negative?
				((packetmovementx<0)?0x10:0)| //X negative?
				0x08| //Always 1!
				((packet->buttons&4)?0x04:0)| //Middle button?
				((packet->buttons&2)?0x02:0)| //Right button?
				((packet->buttons&1)?0x01:0) //Left button?
				); //Give the packet byte!
			break;
		case 1:
			return (packetmovementx<0)?((-packetmovementx)%256):(packetmovementx%256); //X movement, lower 8 bits!
			break;
		case 2:
			return (packetmovementy<0)?((-packetmovementy)%256):(packetmovementy%256); //X movement, lower 8 bits!
			break;
		default: //Unknown index?
			break;
	}
	return 0; //Nothing to process!
}

byte handle_mouseread() //Read from the mouse!
{
	if (__HW_DISABLED) return 0; //Abort!
	byte result;
	if (readfifobuffer(Mouse.buffer,&result)) //Try to read?
	{
		return result; //Read successful!
	}
	else if (Mouse.packets) //Gotten a packet?
	{
		result = processMousePacket(Mouse.packets,Mouse.packetindex); //Process it!
		++Mouse.packetindex; //Next index!
		if (Mouse.packetindex>3) //Over our limit?
		{
			Mouse.packetindex = 0; //Reset packet index!
			next_mousepacket(); //Next mouse packet!
		}
		return result; //Give the result!
	}
	else //Nothing to read?
	{
		return 0x00; //NULL!
	}
}

int handle_mousepeek(byte *result) //Peek at the mouse!
{
	if (__HW_DISABLED) return 0; //Abort!
	if (peekfifobuffer(Mouse.buffer,result)) //Peek at the buffer!
	{
		return 1; //Buffered!
	}
	else if (Mouse.packets) //Gotten a packet?
	{
		*result = processMousePacket(Mouse.packets,Mouse.packetindex); //Process it!
		return 1; //Read!
	}
	return 0; //No packet!
}

void EMU_enablemouse(byte enabled) //Enable mouse input (disable during EMU, enable during CPU emulation (not paused))?
{
	if (__HW_DISABLED) return; //Abort!
	Mouse.disabled = !enabled; //Are we enabled?
}

void PS2_initMouse(byte enabled) //Initialise the mouse to reset mode?
{
	if (__HW_DISABLED) return; //Abort!
	memset(&Mouse.data, 0, sizeof(Mouse)); //Clear the mouse information!
	Mouse.supported = enabled; //Are we enabled!
	if (Mouse.supported) //Are we enabled?
	{
		Controller8042.RAM[0] |= 0x22; //Disable the mouse by default!
		//Register ourselves!
		register_PS2PortWrite(1, &handle_mousewrite); //Write functionnality!
		register_PS2PortRead(1, &handle_mouseread, &handle_mousepeek); //Read functionality!

		Mouse.buffer = allocfifobuffer(16,1); //Allocate a small mouse buffer!

		initPS2Mouse(); //Reset the mouse to power-on defaults!

		update_mouseTimer(); //(Re)set mouse timer!
		Mouse.disabled = 1; //Default: disabled!
	}
}

void BIOS_doneMouse()
{
	if (__HW_DISABLED) return; //Abort!
	free_fifobuffer(&Mouse.buffer); //Free the keyboard buffer!
	flushPackets(); //Flush all mouse packets!
}
