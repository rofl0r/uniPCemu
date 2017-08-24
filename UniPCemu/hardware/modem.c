#include "headers/hardware/modem.h" //Our basic definitions!

#include "headers/support/zalloc.h" //Allocation support!
#include "headers/hardware/uart.h" //UART support for the COM port!
#include "headers/support/fifobuffer.h" //FIFO buffer support!
#include "headers/support/locks.h" //Locking support!
#include "headers/bios/bios.h" //BIOS support!

#define MODEM_BUFFERSIZE 256

struct
{
	byte supported; //Are we supported?
	FIFOBUFFER *buffer; //The input buffer!
	byte datamode; //1=Data mode, 0=Command mode!
	byte connected; //Are we connected?
	byte previousATCommand[256]; //Copy of the command for use with "A/" command!
	byte ATcommand[256];
	word ATcommandsize; //The amount of data sent!
	byte escaping; //Are we trying to escape?
	double timer; //A timer for detecting timeout!

	//Various parameters used!
	byte communicationstandard; //What communication standard!
	byte echomode; //Echo everything back to use user?
	byte offhook; //1: Off hook(disconnected), 2=Off hook(connected), otherwise on-hook(disconnected)!
	byte verbosemode; //Verbose mode: 0=Numeric result codes, 1=Text result codes, 2=Quiet mode(no response).
	byte speakervolume; //Speaker volume!
	byte speakercontrol; //0=Always off, 1=On until carrier detected, 2=Always on, 3=On only while answering.
	byte callprogressmethod; //Call progress method:
	byte lastnumber[256]; //Last-dialed number!
	byte currentregister; //What register is selected?
	byte registers[256]; //All possible registers!
	byte flowcontrol;
	/*
	0=Blind dial and no busy detect. CONNECT message when established.
	1=Blind dial and no busy detect. Connection speed in BPS added to CONNECT string.
	2=Dial tone detection, but no busy detection. Connection speed in BPS added to the CONNECT string.
	3=Blind dial, but busy detection. Connection speed in BPS appended to the CONNECT string.
	4=Dial tone detection and busy tone detection. Connection speed in BPS appended to the CONNECT string.
	*/

	//Active status emulated for the modem!
	byte escapecharacter;
	byte carriagereturncharacter;
	byte linefeedcharacter;
	byte backspacecharacter;
	double escapecodeguardtime;
	byte port; //What port are we allocated to?
	byte canrecvdata; //Can we start receiving data to the UART?
	byte linechanges; //For detecting line changes!
} modem;

void modem_sendData(byte value) //Send data to the connected device!
{
	//Handle sent data!
}

byte modem_connect(char *phonenumber)
{
	return 0; //We cannot connect yet!
}

void modem_updateRegister(byte reg)
{
	switch (reg) //What reserved reg to emulate?
	{
		case 2: //Escape character?
			if (modem.escapecharacter!=modem.registers[reg]) //Escape character changed?
			{
				for (;modem.escaping;--modem.escaping) //Process all escaped data!
				{
					modem_sendData(modem.escaping); //Send all escaped data!
				}
			}
			modem.escapecharacter = modem.registers[reg]; //Escape!
			break;
		case 3: //CR character?
			modem.carriagereturncharacter = modem.registers[reg]; //Escape!
			break;
		case 4: //Line feed character?
			modem.linefeedcharacter = modem.registers[reg]; //Escape!
			break;
		case 5: //Backspace character?
			modem.backspacecharacter = modem.registers[reg]; //Escape!
			break;
		case 12: //Escape code guard time?
			modem.escapecodeguardtime = (modem.registers[reg]*20000000.0); //Set the escape code guard time, in nanoseconds!
			break;
		default: //Unknown/unsupported?
			break;
	}
}

byte useSERModem() //Serial mouse enabled?
{
	return modem.supported; //Are we supported?
}

void modem_setModemControl(byte line) //Set output lines of the Modem!
{
	//Handle modem specifics here!
	//0: Data Terminal Ready(we can are ready to work), 1: Request to Send(UART can receive data)
	modem.canrecvdata = (line&2); //Can we receive data?
	if (((line&1)==0) && ((modem.linechanges^line)&1)) //Became not ready?
	{
		modem.connected = 0; //Disconnect?
		modem.datamode = modem.ATcommandsize = 0; //Starting a new command!
	}
	modem.linechanges = line; //Save for reference!
}

byte modem_cansend()
{
	return 0; //Outgoing isn't supported yet! Buffer always full!
}

byte modem_hasData() //Do we have data for input?
{
	byte temp;
	return (peekfifobuffer(modem.buffer, &temp)&&(modem.canrecvdata||(modem.flowcontrol!=3))); //Do we have data to receive?
}

byte modem_getstatus()
{
	//0: Clear to Send(Can we buffer data to be sent), 1: Data Set Ready(Not hang up, are we ready for use), 2: Ring Indicator, 3: Carrrier detect
	return (modem.datamode?(modem_cansend()?1:0):1)|/*(modem.linechanges&2)*/2|(modem.connected?8:0); //0=CTS(can we receive data to send?), 1=DSR(are we ready for use), 2=Ring, 3=Carrier detect!
}

byte modem_readData()
{
	byte result,emptycheck;
	if (readfifobuffer(modem.buffer, &result))
	{
		if ((modem.datamode==2) && (!peekfifobuffer(modem.buffer,&emptycheck))) //Became ready to transfer data?
		{
			modem.datamode = 1; //Become ready to send!
		}
		return result; //Give the data!
	}
	return 0; //Nothing to give!
}

byte ATresultsString[6][256] = {"ERROR","OK","CONNECT","RING","NO DIALTONE","NO CARRIER"}; //All possible results!
byte ATresultsCode[6] = {4,0,1,2,6,3}; //Code version!
#define MODEMRESULT_ERROR 0
#define MODEMRESULT_OK 1
#define MODEMRESULT_CONNECT 2
#define MODEMRESULT_RING 3
#define MODEMRESULT_NODIALTONE 4
#define MODEMRESULT_NOCARRIER 5

void modem_responseString(byte *s, byte usecarriagereturn)
{
	word i, lengthtosend;
	lengthtosend = strlen(s); //How long to send!
	if (usecarriagereturn&1)
	{
		writefifobuffer(modem.buffer,modem.carriagereturncharacter); //Termination character!
		writefifobuffer(modem.buffer,modem.linefeedcharacter); //Termination character!
	}
	for (i=0;i<lengthtosend;) //Process all data to send!
	{
		writefifobuffer(modem.buffer,s[i]); //Send the character!
	}
	if (usecarriagereturn&2)
	{
		writefifobuffer(modem.buffer,modem.carriagereturncharacter); //Termination character!
		writefifobuffer(modem.buffer,modem.linefeedcharacter); //Termination character!
	}
}
void modem_nrcpy(char *s, word size, word nr)
{
	memset(s,0,size);
	sprintf(s,"%i%i%i%i",(nr%10000)/1000,(nr%1000)/100,(nr%100)/10,(nr%10)); //Convert to string!
}
void modem_responseResult(byte result) //What result to give!
{
	byte s[256];
	if (result>=MIN(NUMITEMS(ATresultsString),NUMITEMS(ATresultsCode))) //Out of range of results to give?
	{
		result = MODEMRESULT_ERROR; //Error!
	}
	if (modem.verbosemode&2) return; //Quiet mode? No response messages!
	if (modem.verbosemode&1) //Code format result?
	{
		modem_responseString(&ATresultsString[result][0],((result!=MODEMRESULT_CONNECT) || (modem.callprogressmethod==0))?3:2); //Send the string to the user!
	}
	else
	{
		modem_nrcpy((char*)&s[0],sizeof(s),ATresultsCode[result]);
		modem_responseString(&s[0],((result!=MODEMRESULT_CONNECT) || (modem.callprogressmethod==0))?3:2);
	}
	if ((result==MODEMRESULT_CONNECT) && modem.callprogressmethod) //Add speed as well?
	{
		modem_responseString(" 12000",1); //End the command properly with a speed indication in bps!
	}
}

void modem_responseNumber(byte x)
{
	char s[256];
	if (modem.verbosemode&1) //Code format result?
	{
		memset(&s,0,sizeof(s));
		sprintf(s,"%u",x); //Convert to a string!
		modem_responseString((byte *)&s,1); //Send the string to the user!
	}
	else
	{
		writefifobuffer(modem.buffer,x); //Code variant instead!
	}
}

byte loadModemProfile(byte state)
{
	return 0; //Default: no states stored yet!
}

byte resetModem(byte state)
{
	word reg;
	memset(&modem.registers,0,sizeof(modem.registers)); //Initialize the registers!
	//Load general default state!
	modem.registers[0] = 0; //Number of rings before auto-answer
	modem.registers[1] = 0; //Ring counter
	modem.registers[2] = 43; //Escape character(+, ASCII)
	modem.registers[3] = 0xD; //Carriage return character(ASCII)
	modem.registers[4] = 0xA; //Line feed character(ASCII)
	modem.registers[5] = 0x8; //Back space character(ASCII)
	modem.registers[6] = 2; //Wait time before blind dialing(seconds).
	modem.registers[7] = 50; //Wait for carrier after dial(seconds(+1))
	modem.registers[8] = 2; //Pause time for ,(dial delay, seconds)
	modem.registers[9] = 6; //Carrier detect response time(tenths of a seconds(+1)) 
	modem.registers[10] = 14; //Delay between Loss of Carrier and Hang-up(tenths of a second)
	modem.registers[11] = 95; //DTMF Tone duration(50-255 milliseconds)
	modem.registers[12] = 50; //Escape code guard time(fiftieths of a second)
	modem.registers[18] = 0; //Test timer(seconds)
	modem.registers[25] = 5; //Delay to DTR(seconds in synchronous mode, hundredths of a second in all other modes)
	modem.registers[26] = 1; //RTC to CTS Delay Interval(hundredths of a second)
	modem.registers[30] = 0; //Inactivity disconnect timer(tens of seconds). 0=Disabled
	modem.registers[37] = 0; //Desired Telco line speed(0-10. 0=Auto, otherwise, speed)
	modem.registers[38] = 20; //Delay before Force Disconnect(seconds)
	for (reg=0;reg<256;++reg)
	{
		modem_updateRegister(reg); //This register has been updated!
	}

	if (loadModemProfile(state)) //Loaded?
	{
		return 0; //Invalid!
	}
	return 0; //Invalid profile!
}

void modem_executeCommand() //Execute the currently loaded AT command, if it's valid!
{
	int n0, n1;
	char number[256];
	byte dialproperties=0;
	memset(&number,0,sizeof(number)); //Init number!
	//Read and execute the AT command, if it's valid!
	if (strcmp(modem.ATcommand,"A/\xD")==0) //Repeat last command?
	{
		memcpy(&modem.ATcommand,modem.previousATCommand,sizeof(modem.ATcommand)); //Last command again!
	}
	//Check for a command to send!
	if ((modem.ATcommand[0]=='A') && (modem.ATcommand[1]=='T')) //AT command?
	{
		memcpy(&modem.previousATCommand,&modem.ATcommand,sizeof(modem.ATcommand)); //Save the command for later use!
		//Parse the AT command!
		if (sscanf(modem.ATcommand,"ATE%u\xD",&n0)) //Select local echo?
		{
			modem.datamode = 0; //Mode not data!
			if (n0<2) //OK?
			{
				modem.echomode = n0; //Set the communication standard!
				modem_responseResult(MODEMRESULT_OK); //Accept!
			}
			else
			{
				modem_responseResult(MODEMRESULT_ERROR); //Error!
			}
		}
		else
		{
			word pos=3; //Position to read!
			if (modem.echomode) //Echo every command back to the user?
			{
				//NOTE: Are we to send the finishing carriage return character as well?
				modem_responseString(&modem.ATcommand[0],3); //Send the string back!
			}
			else
			{
				if (strcmp(modem.ATcommand,"ATDL\xD")) //Dial last number?
				{
					diallast:
					n0 = 'T'; //Dialing tone!
					memcpy(&number,&modem.lastnumber,(strlen(modem.lastnumber)+1)); //Set the new number to roll!
					goto dial;
				}
				else if (sscanf(modem.ATcommand,"ATD%c%s\xD",(char *)&n0,&number[0])) //Dial?
				{
					dial: //Start dialing?
					modem.datamode = 0; //Mode not data!
					switch (n0) //OK?
					{
						case 'L': //Dial last number
							goto diallast;
						case 'A': //Reverse to answer mode after dialing?
							goto unsupporteddial; //Unsupported for now!
							dialproperties = 1; //Reverse to answer mode!
							goto actondial;
						case ';': //Remain in command mode after dialing
							dialproperties = 2; //Remain in command mode!
							goto actondial;
						case ',': //Pause for the time specified in register S8(usually 2 seconds)
						case '!': //Flash-Switch hook (Hang up for half a second, as in transferring a call)
							goto unsupporteddial;
						case 'T': //Tone dial?
						case 'P': //Pulse dial?
						case 'W': //Wait for second dial tone?
						case '@': //Wait for up to	30 seconds for one or more ringbacks
							actondial: //Start dialing?
							if (modem_connect(&number[0]))
							{
								modem_responseResult(MODEMRESULT_CONNECT); //Accept!
								modem.offhook = 2; //On-hook(connect)!
								if (dialproperties!=2) //Not to remain in command mode?
								{
									modem.datamode = 2; //Enter data mode pending!
								}
							}
							else //Dial failed?
							{
								modem_responseResult(MODEMRESULT_NOCARRIER); //No carrier!
							}
							break;
						default: //Unsupported?
							unsupporteddial: //Unsupported dial function?
							modem_responseResult(MODEMRESULT_ERROR); //Error!
							break;
						}
					}
				else if ((strcmp(modem.ATcommand,"ATA\xD")==0) || strcmp(modem.ATcommand,"ATA0\xD")==0) //ATA?
				{
					if (modem_connect(NULL)) //Answered?
					{
						modem_responseResult(MODEMRESULT_CONNECT); //Connected!
						modem.datamode = 2; //Enter data mode pending!
						modem.offhook = 2; //Off-hook(connect)!
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Not Connected!
					}
				}
				else if (strcmp(modem.ATcommand,"ATQ\xD")==0) //Quiet mode?
				{
					modem_responseResult(MODEMRESULT_OK); //OK!
					modem.verbosemode = 2; //Quiet mode!
				}
				else if (sscanf(modem.ATcommand,"ATH%u\xD",&n0)) //Select communication standard?
				{
					doATH:
					modem.datamode = 0; //Mode not data!
					if (n0<2) //OK?
					{
						//if ((n0==0) && modem.offhook)
						modem.offhook = n0?((modem.offhook==2)?2:1):0; //Set the hook status or hang up!
						modem_responseResult(MODEMRESULT_OK); //Accept!
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
					}
				}
				else if (sscanf(modem.ATcommand,"ATB%u\xD",&n0)) //Select communication standard?
				{
					modem.datamode = 0; //Mode not data!
					if (n0<2) //OK?
					{
						modem.communicationstandard = n0; //Set the communication standard!
						modem_responseResult(MODEMRESULT_OK); //Accept!
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
					}
				}
				else if (sscanf(modem.ATcommand,"ATL%u\xD",&n0)) //Select speaker volume?
				{
					modem.datamode = 0; //Mode not data!
					if (n0<4) //OK?
					{
						modem.speakervolume = n0; //Set the speaker volume!
						modem_responseResult(MODEMRESULT_OK); //Accept!
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
					}
				}
				else if (sscanf(modem.ATcommand,"ATM%u\xD",&n0)) //Speaker control?
				{
					modem.datamode = 0; //Mode not data!
					if (n0<4) //OK?
					{
						modem.speakercontrol = n0; //Set the speaker control!
						modem_responseResult(MODEMRESULT_OK); //Accept!
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
					}
				}
				else if (sscanf(modem.ATcommand,"ATV%u\xD",&n0)) //Verbose mode?
				{
					modem.datamode = 0; //Mode not data!
					if (n0<2) //OK?
					{
						modem_responseResult(MODEMRESULT_OK); //Accept!
						modem.verbosemode = n0; //Set the speaker control!
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
					}
				}
				else if (sscanf(modem.ATcommand,"ATX%u\xD",&n0)) //Select call progress method?
				{
					modem.datamode = 0; //Mode not data!
					if (n0<1) //OK and supported by our emulation?
					{
						modem_responseResult(MODEMRESULT_OK); //Accept!
						modem.callprogressmethod = n0; //Set the speaker control!
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
					}
				}
				else if (sscanf(modem.ATcommand,"ATZ%u\xD",&n0)) //Reset modem?
				{
					modem.datamode = 0; //Mode not data!
					if (n0<2) //OK and supported by our emulation?
					{
						if (resetModem(n0)) //Reset to the given state!
						{
							modem_responseResult(MODEMRESULT_OK); //Accept!
						}
						else
						{
							modem_responseResult(MODEMRESULT_ERROR); //Error!
						}
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
					}
				}
				else if (sscanf(modem.ATcommand,"ATI%u\xD",&n0)) //Inquiry, Information, or Interrogation?
				{
					modem.datamode = 0; //Mode not data!
					/*
					if (n0<2) //OK?
					{
						modem.communicationstandard = n0; //Set the communication standard!
						modem_responseResult(MODEMRESULT_OK); //Accept!
					}
					else
					{
					*/
						modem_responseResult(MODEMRESULT_ERROR); //Error: line not defined!
					//}
				}
				else if (strcmp(modem.ATcommand,"ATO\xD")) //Return online?
				{
					if (modem.connected) //Cpnnected?
						modem.datamode = 1; //Return to data mode!
					else
						modem_responseResult(MODEMRESULT_ERROR);
				}
				else if (sscanf(modem.ATcommand,"ATS%u\xD",&n0)) //Select register n as current register?
				{
					modem.currentregister = n0; //Select the register!
				}
				else if (sscanf(modem.ATcommand,"ATS%u?\xD",&n0)) //Select and query register n as current register?
				{
					modem.currentregister = n0; //Select the register!
					goto queryregister;
				}
				else if (sscanf(modem.ATcommand,"ATS%u=%u?\xD",&n0,&n1)) //Select and query register n as current register?
				{
					modem.currentregister = n0; //Select the register!
					n0 = n1; //The register value to set to n0!
					goto storeregister; //Store n0 in the current register!
				}
				else if (strcmp(modem.ATcommand,"AT?\xD")==0) //Query current register?
				{
					queryregister: //Query a register!
					modem_responseNumber(modem.registers[modem.currentregister]); //Give the register value!
				}
				else if (sscanf(modem.ATcommand,"AT=%u\xD",&n0)) //Set current register?
				{
					storeregister:
					modem.registers[modem.currentregister] = n0; //Set the register!
					modem_updateRegister(modem.currentregister); //Update the register as needed!
				}
				else if (sscanf(modem.ATcommand,"AT&K%i\xD",&n0)) //Flow control?
				{
					if (n0<5) //Valid?
					{
						modem.flowcontrol = n0; //Set flow control!
						modem_responseResult(MODEMRESULT_OK); //OK!
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
					}
				}
				else if (sscanf(modem.ATcommand,"AT\\N%u\xD",&n0)) //Operating mode?
				{
					if (n0<5) //Valid?
					{
						//Unused!
						modem_responseResult(MODEMRESULT_OK); //OK!
					}
					else //Error out?
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
					}
				}
				else //Unknown?
				{
					modem_responseResult(MODEMRESULT_OK); //Just OK unknown commands, according to Dosbox!
				}
			}
		}
	}
}

void modem_writeData(byte value)
{
	//Handle the data sent to the modem!
	modem.timer = 0.0; //Reset the timer when anything is received!
	if (modem.datamode) //Data mode?
	{
		//Unhandled yet!
		if (value==modem.escapecharacter) //Possible escape sequence?
		{
			++modem.escaping; //Increase escape info!
		}
		else //Not escaping?
		{
			for (;modem.escaping;) //Process escape characters as data!
			{
				--modem.escaping; //Handle one!
				modem_sendData(modem.escapecharacter); //Send it as data!
			}
			modem_sendData(value); //Send the data!
		}
	}
	else //Command mode?
	{
		if (modem.ATcommandsize<sizeof(modem.ATcommand)-2) //Valid to input(leave 1 byte for the terminal character)?
		{
			if (value=='~') //Pause stream for half a second?
			{
				//Ignore this time?
			}
			else if (value==modem.carriagereturncharacter) //Carriage return? Execute the command!
			{
				modem.ATcommand[modem.ATcommandsize] = 0xD; //Terminal character!
				modem.ATcommand[modem.ATcommandsize+1] = 0; //Terminal character!
				modem.ATcommandsize = 0; //Start the new command!
				modem_executeCommand();
			}
			else if (value!=0x20) //Not space? Command byte!
			{
				modem.ATcommand[modem.ATcommandsize++] = value; //Add data to the string!
			}
		}
	}
}



void initModem(byte enabled) //Initialise modem!
{
	memset(&modem, 0, sizeof(modem));
	modem.supported = enabled; //Are we to be emulated?
	if (useSERModem()) //Is this mouse enabled?
	{
		modem.port = allocUARTport(); //Try to allocate a port to use!
		if (modem.port==0xFF) //Unable to allocate?
		{
			modem.supported = 0; //Unsupported!
			goto unsupportedUARTModem;
		}
		modem.buffer = allocfifobuffer(256,1); //Small input buffer!
		UART_registerdevice(modem.port,&modem_setModemControl,&modem_getstatus,&modem_hasData,&modem_readData,&modem_writeData); //Register our UART device!
		resetModem(0); //Reset the modem to the default state!
	}
	else
	{
		unsupportedUARTModem: //Unsupported!
		modem.buffer = NULL; //No buffer present!
	}
}

void doneModem() //Finish modem!
{
	if (modem.buffer) //Allocated?
	{
		free_fifobuffer(&modem.buffer); //Free our buffer!
	}
}

void cleanModem()
{
	//Nothing to do!
}

void updateModem(uint_32 timepassed) //Sound tick. Executes every instruction.
{
	modem.timer += timepassed; //Add time to the timer!
	if (modem.datamode && modem.escaping) //Data mode and escapes buffered?
	{
		if (modem.timer>=modem.escapecodeguardtime) //Long delay time?
		{
			if (modem.escaping>=3) //At least 3 escapes?
			{
				for (;modem.escaping>3;) //Process pending escapes that's to be sent!
				{
					--modem.escaping;
					modem_sendData(modem.escapecharacter); //Send the escaped data!
				}
				modem.escaping = 0; //Stop escaping!
				modem.datamode = 0; //Return to command mode!
			}
			else //Less than 3 escapes buffered to be sent?
			{
				for (;modem.escaping;) //Send the escaped data after all!
				{
					--modem.escaping;
					modem_sendData(modem.escapecharacter); //Send the escaped data!
				}
			}
		}
	}
}
