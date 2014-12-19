#ifndef HW8042_H
#define HW8042_H
#include "headers/types.h" //Basic types!
#include "headers/support/fifobuffer.h" //FIFO buffer support for our data!

typedef void (*PS2OUT)(byte);    /* A pointer to a PS/2 device handler Write function */
typedef byte (*PS2IN)(void);    /* A pointer to a PS/2 device handler Read function */
typedef int (*PS2PEEK)(byte *result);    /* A pointer to a PS/2 device handler Peek (Read without flush) function */

typedef struct
{
	//RAM!
	union
	{
		byte RAM[0x20]; //A little RAM the user can use!
		struct
		{
			struct
			{
				byte FirstPortInterruptEnabled : 1; //First PS/2 port interrupt Enabled
				byte SecondPortInterruptEnabled : 1; //Second PS/2 port interrupt Enabled
				byte SystemPassedPOST : 1; //System passed POST?
				byte ShouldBeZero1 : 1; //Should be 0
				byte FirstPortDisabled : 1; //First PS/2 port disabled?
				byte SecondPortDisabled : 1; //Second PS/2 port disabled?
				byte FirstPortTranslation : 1; //First port translation?
				byte MustBeZero : 1; //Must be zero!
			} PS2ControllerConfigurationByte;
			byte data[0x19]; //Rest of RAM, unused by us!
		};
	};

	//Input and output buffer!
	byte input_buffer; //Contains byte read from device; read only
	byte output_buffer; //Contains byte to-be-written to device; write only

	//Status buffer!
	byte status_buffer; //8 status flags; read-only
	/*

	bit 0: 1=Output buffer full (port 0x60)
	bit 1: 1=Input buffer full (port 0x60). There is something to read.
	bit 2: 0=Power-on reset; 1=Soft reset (by software)
	bit 3: 0=Port 0x60 was last written to; 1=Port 0x64 was last written to
	bit 4: 0=Keyboard communication is inhabited; 1=Not inhabited.
	bit 5: 1=AUX: Data is from the mouse!
	bit 6: 1=Receive timeout: keyboard probably broke.
	bit 7: 1=Parity error.

	*/

	byte Write_RAM; //To write the byte written to 0x60 to RAM index-1, reset to 0 afterwards?
	byte Read_RAM; //See Write_RAM!

	//Command written!
	byte command; //Current command given!

	//PS/2 output devices!
	byte has_port[2]; //First/second PS/2 port command/data when output?
	PS2OUT portwrite[2]; //Port Output handler!
	PS2IN portread[2]; //Port Input has been read?
	PS2PEEK portpeek[2]; //Port has data&peek function!
	
	//Direct feedback support!
	byte port60toFirstPS2Input; //Redirect write to port 0x60 to input of first PS/2 device!
	byte port60toSecondPS2Input; //Redirect write to port 0x60 to input of second PS/2 device!

	//PS/2 output port support!
	byte writeoutputport; //PS/2 Controller Output (to the 8042 output port)? On port 60h!
	byte readoutputport; //PS/2 Controller Input (from the 8042 output port)? On port 60h!
	byte outputport; //The data output for port 60 when read/writeoutputport.
	
	//The output buffer itself!
	FIFOBUFFER *buffer; //The buffer for our data!
	
} Controller8042_t; //The 8042 Controller!

void BIOS_init8042(); //Init 8042&Load all BIOS!
void BIOS_done8042(); //Deallocates the 8042!

//The input buffer!
void give_8042_input(byte value); //Give 8042 input from hardware or 8042!
word free8042buffer(); //Free 8042 buffer size!
void waitforfree8042buffer(byte size); //Wait for size free bytes in the 8042 buffer!

//The output port has changed?
void refresh_outputport(); //Refresh the output port actions!

//Read and write port handlers!
void write_8042(word port, byte value); //Prototype for init port!
byte read_8042(word port); //Prototype for init port!

//Registration of First and Second PS/2 controller!
void register_PS2PortWrite(byte port, PS2OUT handler);
void register_PS2PortRead(byte port, PS2IN handler, PS2PEEK peekhandler);
#endif