#include "headers/hardware/modem.h" //Our basic definitions!

#include "headers/support/zalloc.h" //Allocation support!
#include "headers/hardware/uart.h" //UART support for the COM port!
#include "headers/support/fifobuffer.h" //FIFO buffer support!
#include "headers/support/locks.h" //Locking support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/support/tcphelper.h" //TCP support!

#define MODEM_BUFFERSIZE 256

#define MODEM_SERVERPOLLFREQUENCY 1000
#define MODEM_DATATRANSFERFREQUENCY 12000

struct
{
	byte supported; //Are we supported?
	FIFOBUFFER *inputbuffer; //The input buffer!
	FIFOBUFFER *inputdatabuffer; //The input buffer, data mode only!
	FIFOBUFFER *outputbuffer; //The output buffer!
	byte datamode; //1=Data mode, 0=Command mode!
	byte connected; //Are we connected?
	word connectionport; //What port to connect to by default?
	byte previousATCommand[256]; //Copy of the command for use with "A/" command!
	byte ATcommand[256];
	word ATcommandsize; //The amount of data sent!
	byte escaping; //Are we trying to escape?
	double timer; //A timer for detecting timeout!
	double ringtimer; //Ringing timer!
	double serverpolltimer; //Network connection request timer!
	double networkdatatimer; //Network connection request timer!

	double serverpolltick; //How long it takes!
	double networkpolltick;

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
	byte ringing; //Are we ringing?
	byte DTROffResponse; //Default: full reset!
	byte DSRisConnectionEstablished; //Default: assert high always!
	byte DCDisCarrier;
	byte CTSAlwaysActive; //Default: always active!

	byte escapecharacter;
	byte carriagereturncharacter;
	byte linefeedcharacter;
	byte backspacecharacter;
	double escapecodeguardtime;
	byte port; //What port are we allocated to?
	byte canrecvdata; //Can we start receiving data to the UART?
	byte linechanges; //For detecting line changes!
} modem;

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
	lengthtosend = strlen((char *)s); //How long to send!
	if (usecarriagereturn&1)
	{
		writefifobuffer(modem.inputbuffer,modem.carriagereturncharacter); //Termination character!
		if ((usecarriagereturn&4)==0) writefifobuffer(modem.inputbuffer,modem.linefeedcharacter); //Termination character!
	}
	for (i=0;i<lengthtosend;) //Process all data to send!
	{
		writefifobuffer(modem.inputbuffer,s[i++]); //Send the character!
	}
	if (usecarriagereturn&2)
	{
		writefifobuffer(modem.inputbuffer,modem.carriagereturncharacter); //Termination character!
		if ((usecarriagereturn&4)==0) writefifobuffer(modem.inputbuffer,modem.linefeedcharacter); //Termination character!
	}
}
void modem_nrcpy(char *s, word size, word nr)
{
	memset(s,0,size);
	sprintf(s,"%u%u%u",(nr%1000)/100,(nr%100)/10,(nr%10)); //Convert to string!
}
void modem_responseResult(byte result) //What result to give!
{
	byte s[256];
	byte connectionspeed[256] = " 12000"; //Connection speed!
	if (result>=MIN(NUMITEMS(ATresultsString),NUMITEMS(ATresultsCode))) //Out of range of results to give?
	{
		result = MODEMRESULT_ERROR; //Error!
	}
	if (modem.verbosemode&2) return; //Quiet mode? No response messages!
	if (modem.verbosemode&1) //Code format result?
	{
		modem_responseString(&ATresultsString[result][0],((result!=MODEMRESULT_CONNECT) || (modem.callprogressmethod==0))?3:1); //Send the string to the user!
	}
	else
	{
		modem_nrcpy((char*)&s[0],sizeof(s),ATresultsCode[result]);
		modem_responseString(&s[0],(((result!=MODEMRESULT_CONNECT) || (modem.callprogressmethod==0))?3:1)|4);
	}
	if ((result==MODEMRESULT_CONNECT) && modem.callprogressmethod) //Add speed as well?
	{
		modem_responseString(&connectionspeed[0],2|((modem.verbosemode&1)<<2)); //End the command properly with a speed indication in bps!
	}
}

void modem_responseNumber(byte x)
{
	char s[256];
	if (modem.verbosemode&1) //Code format result?
	{
		memset(&s,0,sizeof(s));
		sprintf(s,"%04u",x); //Convert to a string!
		modem_responseString((byte *)&s,1); //Send the string to the user!
	}
	else
	{
		writefifobuffer(modem.inputbuffer,x); //Code variant instead!
	}
}

byte modem_sendData(byte value) //Send data to the connected device!
{
	//Handle sent data!
	return writefifobuffer(modem.outputbuffer,value); //Try to write to the output buffer!
}

byte readIPnumber(char **x, byte *number)
{
	byte size=0;
	word result=0;
	for (;(isdigit((int)*(*x)) && (size<3));) //Scan digits!
	{
		result = (result*10)+(*(*x)-'0'); //Convert to a number!
		++(*x); //Next digit!
		++size; //Size has been read!
	}
	if ((size==3) && (result<256)) //Valid IP part?
	{
		*number = result; //Give the result!
		return 1; //Read!
	}
	return 0; //Not a valid IP section!
}

byte modem_connect(char *phonenumber)
{
	char ipaddress[256];
	byte a,b,c,d;
	char *p; //For normal port resolving!
	unsigned int port;
	if (modem.ringing && (phonenumber==NULL)) //Are we ringing and accepting it?
	{
		modem.ringing = 0; //Not ringing anymore!
		modem.connected = 1; //We're connected!
		return 1; //Accepted!
	}
	else if (phonenumber==NULL) //Not ringing, but accepting?
	{
		return 0; //Not connected!
	}
	memset(&ipaddress,0,sizeof(ipaddress)); //Init IP address to translate!
	if (strlen(phonenumber)>=9) //Valid length to convert IP addresses?
	{
		p = phonenumber; //For scanning the phonenumber!
		if (readIPnumber(&p,&a))
		{
			if (readIPnumber(&p,&b))
			{
				if (readIPnumber(&p,&c))
				{
					if (readIPnumber(&p,&d))
					{
						if (*p=='\0') //EOS?
						{
							//Automatic port?
							sprintf(ipaddress,"%u.%u.%u.%u",a,b,c,d); //Formulate the address!
							port = modem.connectionport; //Use the default port as specified!
						}
						else if (*p==':') //Port might follow?
						{
							++p; //Skip character!
							if (sscanf(p,"%u",&port)==0) //Port incorrectly read?
							{
								return 0; //Fail: invalid port has been specified!
							}
							sprintf(ipaddress,"%u.%u.%u.%u",a,b,c,d);
						}
						else //Invalid?
						{
							goto plainaddress; //Plain address inputted?
						}
					}
					else
					{
						goto plainaddress; //Take as plain address!
					}
				}
				else
				{
					goto plainaddress; //Take as plain address!
				}
			}
			else
			{
				goto plainaddress; //Take as plain address!
			}
		}
		else
		{
			goto plainaddress; //Take as plain address!
		}
	}
	else
	{
		plainaddress: //A plain address after all?
		if ((p = strrchr(phonenumber,':'))!=NULL) //Port is specified?
		{
			strcpy(ipaddress,phonenumber); //Raw IP with port!
			ipaddress[(ptrnum)p-(ptrnum)phonenumber] = '\0'; //Cut off the port part!
			++p; //Take the port itself!
			if (sscanf(p,"%u",&port)==0) //Port incorrectly read?
			{
				return 0; //Fail: invalid port has been specified!
			}
		}
		else //Raw address?
		{
			strcpy(ipaddress,phonenumber); //Use t
			port = modem.connectionport; //Use the default port as specified!
		}
	}
	if (TCP_ConnectClient(ipaddress,port)) //Connected on the port specified(use the server port by default)?
	{
		modem.connected = 1; //We're connected!
		return 1; //We're connected!
	}
	return 0; //We've failed to connect!
}

void modem_hangup() //Hang up, if possible!
{
	TCPServer_restart(); //Start into the server mode!
	modem.connected = 0; //Not connected anymore!
	modem.ringing = 0; //Not ringing anymore!
	modem.offhook = 0; //We're on-hook!
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

byte loadModemProfile(byte state)
{
	if (state==0) //OK?
	{
		return 1; //OK: loaded state!
	}
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

	modem.communicationstandard = 0; //Default communication standard!

	//Result defaults
	modem.echomode = 1; //Default: echo back!
	modem.verbosemode = 1; //Text-mode verbose!

	modem.flowcontrol = 0; //Default flow control!
	memset(&modem.lastnumber,0,sizeof(modem.lastnumber)); //No last number!
	modem.offhook = 0; //On-hook!
	if (modem.connected) //Are we still connected?
	{
		modem.connected = 0; //Disconnect!
		modem_responseResult(MODEMRESULT_NOCARRIER); //Report no carrier!
		TCPServer_restart(); //Start into the server mode!
	}

	//Default handling of the Hardware lines is also loaded:
	modem.DTROffResponse = 2; //Default: full reset!
	modem.DSRisConnectionEstablished = 0; //Default: assert high always!
	modem.DCDisCarrier = 1; //Default: DCD=Carrier detected.
	modem.CTSAlwaysActive = 1; //Default: always active!

	//Misc data
	memset(&modem.previousATCommand,0,sizeof(modem.previousATCommand)); //No previous command!

	//Speaker controls
	modem.speakercontrol = 0; //Disabled speaker!
	modem.speakervolume = 0; //Muted speaker!

	if (loadModemProfile(state)) //Loaded?
	{
		return 1; //OK!
	}
	return 0; //Invalid profile!
}

void modem_setModemControl(byte line) //Set output lines of the Modem!
{
	//Handle modem specifics here!
	//0: Data Terminal Ready(we can are ready to work), 1: Request to Send(UART can receive data)
	modem.canrecvdata = (line&2); //Can we receive data?
	if (((line&1)==0) && ((modem.linechanges^line)&1)) //Became not ready?
	{
		switch (modem.DTROffResponse) //What reponse?
		{
			case 0: //Ignore the line?
				break;
			case 2: //Full reset, hangup?
				resetModem(0); //Reset!
				if (modem.connected || modem.ringing) //Are we connected?
				{
					modem_responseResult(MODEMRESULT_NOCARRIER); //No carrier!
					modem_hangup(); //Hang up!
				}
			case 1: //Goto AT command mode?
				modem.datamode = modem.ATcommandsize = 0; //Starting a new command!
				break;
		}
	}
	modem.linechanges = line; //Save for reference!
}

byte modem_hasData() //Do we have data for input?
{
	byte temp;
	return ((peekfifobuffer(modem.inputbuffer, &temp) || (peekfifobuffer(modem.inputdatabuffer,&temp) && (modem.datamode==1)))&&(modem.canrecvdata||(modem.flowcontrol!=3))); //Do we have data to receive?
}

byte modem_getstatus()
{
	//0: Clear to Send(Can we buffer data to be sent), 1: Data Set Ready(Not hang up, are we ready for use), 2: Ring Indicator, 3: Carrrier detect
	return (modem.datamode?(modem.CTSAlwaysActive?1:((modem.linechanges>>1)&1)):1)|(modem.DSRisConnectionEstablished?(modem.connected?2:0):2)|(modem.ringing?4:0)|((modem.connected||(modem.DCDisCarrier==0))?8:0); //0=CTS(can we receive data to send?), 1=DSR(are we ready for use), 2=Ring, 3=Carrier detect!
}

byte modem_readData()
{
	byte result,emptycheck;
	if (modem.datamode!=1) //Not data mode?
	{
		if (readfifobuffer(modem.inputbuffer, &result))
		{
			if ((modem.datamode==2) && (!peekfifobuffer(modem.inputbuffer,&emptycheck))) //Became ready to transfer data?
			{
				modem.datamode = 1; //Become ready to send!
			}
			return result; //Give the data!
		}
	}
	if (modem.datamode==1) //Data mode?
	{
		if (readfifobuffer(modem.inputdatabuffer, &result))
		{
			return result; //Give the data!
		}
	}
	return 0; //Nothing to give!
}

byte modemcommand_readNumber(word *pos, int *result)
{
	byte valid = 0;
	*result = 0;
	nextpos:
	switch (modem.ATcommand[*pos]) //What number?
	{
	case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
		*result = (*result*10)+(modem.ATcommand[*pos]-'0'); //Convert to a number!
		++*pos; //Next position to read!
		valid = 1; //We're valid!
		goto nextpos; //Read the next position!
		break;
	default: //Finished?
		break;
	}
	return valid; //Is the result valid?
}

void modem_Answered()
{
	modem_responseResult(MODEMRESULT_CONNECT); //Connected!
	modem.datamode = 2; //Enter data mode pending!
	modem.offhook = 1; //Off-hook(connect)!
}

void modem_executeCommand() //Execute the currently loaded AT command, if it's valid!
{
	int n0;
	char number[256];
	byte dialproperties=0;
	memset(&number,0,sizeof(number)); //Init number!
	byte *temp;
	byte verbosemodepending; //Pending verbose mode!
	temp = &modem.ATcommand[0]; //Parse the entire string!
	for (;*temp;)
	{
		*temp = (byte)toupper((int)*temp); //Convert to upper case!
		++temp; //Next character!
	}
	//Read and execute the AT command, if it's valid!
	if (strcmp((char *)&modem.ATcommand[0],"A/")==0) //Repeat last command?
	{
		memcpy(&modem.ATcommand,modem.previousATCommand,sizeof(modem.ATcommand)); //Last command again!
	}
	//Check for a command to send!
	//Parse the AT command!

	if (modem.ATcommand[0]==0) //Empty line? Stop dialing and autoanswer!
	{
		modem.registers[0] = 0; //Autoanswer off!
		modem_updateRegister(0); //Register has been updated!
		return;
	}

	if ((modem.ATcommand[0] != 'A') || (modem.ATcommand[1] != 'T')) {
		modem_responseResult(MODEMRESULT_ERROR); //Error out!
		return;
	}
	memcpy(&modem.previousATCommand,&modem.ATcommand,sizeof(modem.ATcommand)); //Save the command for later use!
	verbosemodepending = modem.verbosemode; //Save the old verbose mode, to detect and apply changes after the command is successfully completed!
	word pos=2; //Position to read!
	for (;;) //Parse the command!
	{
		switch (modem.ATcommand[pos++]) //What command?
		{
		case 0: //EOS?
			modem_responseResult(MODEMRESULT_OK); //OK
			modem.verbosemode = verbosemodepending; //New verbose mode, if set!
			return; //Finished processing the command!
		case 'E': //Select local echo?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATE;
			case 0:
				--pos; //Retry analyzing!
			case '0': //Off?
				n0 = 0;
				doATE:
				if (n0<2) //OK?
				{
					modem.echomode = n0; //Set the communication standard!
				}
				else
				{
					modem_responseResult(MODEMRESULT_ERROR); //Error!
					return; //Abort!
				}
				break;
			default:
				--pos; //Retry analyzing!
				break;
			}
			break;
		case 'D': //Dial?
			switch (modem.ATcommand[pos++]) //What dial command?
			{
			case 0: //EOS?
				--pos; //Retry analyzing!
				break;
			case 'L':
				memcpy(&number,&modem.lastnumber,(strlen((char *)&modem.lastnumber[0])+1)); //Set the new number to roll!
				goto actondial;
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
				strcpy((char *)&number[0],(char *)&modem.ATcommand[pos]); //Set the number to dial!
				memset(&modem.lastnumber,0,sizeof(modem.lastnumber)); //Init last number!
				strcpy((char *)&modem.lastnumber,(char *)&number[0]); //Set the last number!
				actondial: //Start dialing?
				if (modem_connect(number))
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
				modem.verbosemode = verbosemodepending; //New verbose mode, if set!
				return; //Nothing follows the phone number!
				break;
			default: //Unsupported?
				--pos; //Retry analyzing!
				unsupporteddial: //Unsupported dial function?
				modem_responseResult(MODEMRESULT_ERROR); //Error!
				return; //Abort!
				break;
			}
			break; //Dial?
		case 'A': //Answer?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case 0: //EOS?
				--pos; //Retry analyzing!
			case '0': //Answer?
				if (modem_connect(NULL)) //Answered?
				{
					modem_Answered(); //Answer!
				}
				else
				{
					modem_responseResult(MODEMRESULT_ERROR); //Not Connected!
					return; //Abort!
				}
				break;
			default:
				--pos; //Retry analyzing!
				break;
			}
			break;
		case 'Q': //Quiet mode?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case 0: //EOS?
				--pos; //Retry analyzing!
			case '0': //Answer?
				n0 = 0;
				goto doATQ;
			case '1':
				n0 = 1;
				doATQ:
				if (n0<2)
				{
					verbosemodepending = (n0<<1)|(verbosemodepending&1); //Quiet mode!
				}
				else
				{
					modem_responseResult(MODEMRESULT_ERROR); //ERROR!
					return; //Abort!
				}
				break;
			default:
				--pos; //Retry analyzing!
				break;
			}
		case 'H': //Select communication standard?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATH;
			case 0:
				--pos; //Retry analyzing!
			case '0': //Off hook?
				n0 = 0;
				doATH:
				if (n0<2) //OK?
				{
					//if ((n0==0) && modem.offhook)
					modem.offhook = n0?1:0; //Set the hook status or hang up!
					if (((modem.connected || modem.ringing)&&(!modem.offhook)) || (modem.offhook && (!(modem.connected||modem.ringing)))) //Disconnected or still ringing/connected?
					{
						if (modem.offhook==0) //On-hook?
						{
							modem_hangup(); //Hang up, if required!
						}
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
						return; //Abort!
					}
				}
				else
				{
					modem_responseResult(MODEMRESULT_ERROR); //Error!
					return; //Abort!
				}
				break;
			default:
				--pos; //Retry analyzing!
				break;
			}
			break;
		case 'B': //Select communication standard?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATB;
			case 0:
				--pos; //Retry analyzing!
			case '0':
				n0 = 0;
				doATB:
				if (n0<2) //OK?
				{
					modem.communicationstandard = n0; //Set the communication standard!
				}
				else
				{
					modem_responseResult(MODEMRESULT_ERROR); //Error!
					return; //Abort!
				}
				break;
			default:
				--pos; //Retry analyzing!
				break;
			}
			break;
		case 'L': //Select speaker volume?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATL;
			case '2':
				n0 = 2;
				goto doATL;
			case '3':
				n0 = 3;
				goto doATL;
			case 0:
				--pos; //Retry analyzing!
			case '0':
				n0 = 0;
				doATL:
				if (n0<4) //OK?
				{
					modem.speakervolume = n0; //Set the speaker volume!
				}
				else
				{
					modem_responseResult(MODEMRESULT_ERROR); //Error!
					return; //Abort!
				}
				break;
			default:
				--pos; //Retry analyzing!
				break;
			}
			break;
		case 'M': //Speaker control?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATM;
			case '2':
				n0 = 2;
				goto doATM;
			case '3':
				n0 = 3;
				goto doATM;
			case 0:
				--pos; //Retry analyzing!
			case '0':
				n0 = 0;
				doATM:
				if (n0<4) //OK?
				{
					modem.speakercontrol = n0; //Set the speaker control!
				}
				else
				{
					modem_responseResult(MODEMRESULT_ERROR); //Error!
					return; //Abort!
				}
			default:
				--pos; //Retry analyzing!
				break;
			}
			break;
		case 'V': //Verbose mode?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATV;
			case 0:
				--pos; //Retry analyzing!
			case '0':
				n0 = 0;
				doATV:
				if (n0<2) //OK?
				{
					verbosemodepending = ((verbosemodepending&~1)|n0); //Set the verbose mode to numeric(0) or English(1)!
				}
				else
				{
					modem_responseResult(MODEMRESULT_ERROR); //Error!
					return; //Abort!
				}
				break;
			default:
				--pos; //Retry analyzing!
				break;
			}
			break;
		case 'X': //Select call progress method?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATX;
			case '2':
				n0 = 2;
				goto doATX;
			case '3':
				n0 = 3;
				goto doATX;
			case '4':
				n0 = 4;
				goto doATX;
			case 0:
				--pos; //Retry analyzing!
			case '0':
				n0 = 0;
				doATX:
				modem.datamode = 0; //Mode not data!
				if (n0<5) //OK and supported by our emulation?
				{
					modem.callprogressmethod = n0; //Set the speaker control!
				}
				else
				{
					modem_responseResult(MODEMRESULT_ERROR); //Error!
					return; //Abort!
				}
				break;
			default:
				--pos; //Retry analyzing!
				break;
			}
			break;
		case 'Z': //Reset modem?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATZ;
			case 0:
				--pos; //Retry analyzing!
			case '0':
				n0 = 0;
				doATZ:
				if (n0<2) //OK and supported by our emulation?
				{
					if (resetModem(n0)) //Reset to the given state!
					{
						//Do nothing when succeeded! Give OK!
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
						return; //Abort!
					}
				}
				else
				{
					modem_responseResult(MODEMRESULT_ERROR); //Error!
					return; //Abort!
				}
				break;
			default:
				--pos; //Retry analyzing!
				break;
			}
			break;
		case 'I': //Inquiry, Information, or Interrogation?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATI;
			case 0:
				--pos; //Retry analyzing!
			case '0':
				n0 = 0;
				doATI:
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
					return; //Abort!
				//}
				break;
			default:
				--pos; //Retry analyzing!
				break;
			}
			break;
		case 'O': //Return online?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATO;
			case 0:
				--pos; //Retry analyzing!
			case '0':
				n0 = 0;
				doATO:
				if (modem.connected) //Cpnnected?
					modem.datamode = 2; //Return to data mode!
				else
				{
					modem_responseResult(MODEMRESULT_ERROR);
					return; //Abort!
				}
				break;
			default: //Unknown?
				--pos; //Retry analyzing!
				break;
			}
			break;
		case '?': //Query current register?
			modem_responseNumber(modem.registers[modem.currentregister]); //Give the register value!
			modem.verbosemode = verbosemodepending; //New verbose mode, if set!
			return; //Abort!
			break;
		case '=': //Set current register?
			if (modemcommand_readNumber(&pos,&n0)) //Read the number?
			{
				modem.registers[modem.currentregister] = n0; //Set the register!
				modem_updateRegister(modem.currentregister); //Update the register as needed!
			}
			else
			{
				modem_responseResult(MODEMRESULT_ERROR);
				return; //Abort!
			}
			break;
		case 'S': //Select register n as current register?
			if (modemcommand_readNumber(&pos,&n0)) //Read the number?
			{
				modem.currentregister = n0; //Select the register!
			}
			else
			{
				modem_responseResult(MODEMRESULT_ERROR);
				return; //Abort!
			}
			break;
		case '&': //Extension 1?
			switch (modem.ATcommand[pos++])
			{
			case 0: //EOS?
				--pos; //Let us handle it!
				break;
			case 'R': //Force CTS high option?
				switch (modem.ATcommand[pos++]) //What flow control?
				{
				case '0':
					n0 = 0; //Ignore RTS in command mode, CTS=RTS in data mode.
					goto setAT_R;
				case '1':
					n0 = 1; //Force CTS active
					setAT_R:
					if (n0<2) //Valid?
					{
						modem.CTSAlwaysActive = n0; //Set flow control!
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
						return; //Abort!
					}
					break;
				}
				break;
			case 'C': //Force DCD to be carrier option?
				switch (modem.ATcommand[pos++]) //What flow control?
				{
				case '0':
					n0 = 0; //DCD is always high
					goto setAT_C;
				case '1':
					n0 = 1; //DCD is Carrier detect
					setAT_C:
					if (n0<2) //Valid?
					{
						modem.DCDisCarrier = n0; //Set flow control!
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
						return; //Abort!
					}
					break;
				}
				break;
			case 'S': //Force DSR high option?
				switch (modem.ATcommand[pos++]) //What flow control?
				{
				case '0':
					n0 = 0; //DSR=Always high.
					goto setAT_S;
				case '1':
					n0 = 1; //DSR=Connection established.
					setAT_S:
					if (n0<2) //Valid?
					{
						modem.DSRisConnectionEstablished = n0; //Set flow control!
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
						return; //Abort!
					}
					break;
				}
				break;
			case 'D': //DTR reponse option?
				switch (modem.ATcommand[pos++]) //What flow control?
				{
				case '0':
					n0 = 0; //Ignore DTR line from computer
					goto setAT_D;
				case '1':
					n0 = 1; //Goto AT command state when DTR On->Off
					goto setAT_D;
				case '2':
					n0 = 2; //Full reset when DTR On->Off
					setAT_D:
					if (n0<2) //Valid?
					{
						modem.DTROffResponse = n0; //Set DTR off response!
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
						return; //Abort!
					}
					break;
				}
				break;
			case 'F': //Load defaults?
				n0 = 0; //Defautl configuration!
				goto doATZ; //Execute ATZ!
			case 'K': //Flow control?
				switch (modem.ATcommand[pos++]) //What flow control?
				{
				case '0':
					n0 = 0;
					goto setAT_K;
				case '1':
					n0 = 1;
					goto setAT_K;
				case '2':
					n0 = 2;
					goto setAT_K;
				case '3':
					n0 = 3;
					goto setAT_K;
				case '4':
					n0 = 4;
					setAT_K:
					if (n0<5) //Valid?
					{
						modem.flowcontrol = n0; //Set flow control!
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
						return; //Abort!
					}
					break;
				default:
					--pos; //Retry analyzing!
					break;
				}
				break;
			default:
				--pos; //Retry analyzing!
				break;
			}
			break;
		case '\\': //Extension 2?
			switch (modem.ATcommand[pos++])
			{
			case 0: //EOS?
				--pos; //Let us handle it!
				break;
			case 'N': //Flow control?
				switch (modem.ATcommand[pos++]) //What flow control?
				{
				case '0':
					n0 = 0;
					goto setAT_N;
				case '1':
					n0 = 1;
					goto setAT_N;
				case '2':
					n0 = 2;
					goto setAT_N;
				case '3':
					n0 = 3;
					goto setAT_N;
				case '4':
					n0 = 4;
					setAT_N:
					if (n0<5) //Valid?
					{
						//Unused!
					}
					else //Error out?
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
						return; //Abort!
					}
					break;
				}
				break;
			default:
				--pos; //Retry analyzing!
				break;							
			}
		default: //Unknown?
			modem_responseResult(MODEMRESULT_ERROR); //Just ERROR unknown commands!
			return; //Abort!
			break;
		} //Switch!
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
		if (modem.echomode) //Echo enabled?
		{
			writefifobuffer(modem.inputbuffer,value); //Echo the value back to the terminal!
		}
		if (modem.ATcommandsize<(sizeof(modem.ATcommand)-1)) //Valid to input(leave 1 byte for the terminal character)?
		{
			if (value=='~') //Pause stream for half a second?
			{
				//Ignore this time?
			}
			else if (value==modem.backspacecharacter) //Backspace?
			{
				if (modem.ATcommandsize) //Valid to backspace?
				{
					--modem.ATcommandsize; //Remove last entered value!
				}
			}
			else if (value==modem.carriagereturncharacter) //Carriage return? Execute the command!
			{
				modem.ATcommand[modem.ATcommandsize] = 0; //Terminal character!
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

extern BIOS_Settings_TYPE BIOS_Settings; //Currently used settings!

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
		modem.inputbuffer = allocfifobuffer(MODEM_BUFFERSIZE,1); //Small input buffer!
		modem.inputdatabuffer = allocfifobuffer(MODEM_BUFFERSIZE,1); //Small input buffer!
		modem.outputbuffer = allocfifobuffer(MODEM_BUFFERSIZE,1); //Small input buffer!
		if (modem.inputbuffer && modem.inputdatabuffer && modem.outputbuffer) //Gotten buffers?
		{
			UART_registerdevice(modem.port,&modem_setModemControl,&modem_getstatus,&modem_hasData,&modem_readData,&modem_writeData); //Register our UART device!
			modem.connectionport = BIOS_Settings.modemlistenport; //Default port to connect to if unspecified!
			if (modem.connectionport==0) //Invalid?
			{
				modem.connectionport = 23; //Telnet port by default!
			}
			TCP_ConnectServer(modem.connectionport); //Connect the server on the default port!
			resetModem(0); //Reset the modem to the default state!
			modem.serverpolltick = (1000000000.0/(double)MODEM_SERVERPOLLFREQUENCY); //Server polling rate of connections!
			modem.networkpolltick = (1000000000.0/(double)MODEM_DATATRANSFERFREQUENCY); //Data transfer polling rate!
		}
		else
		{
			if (modem.inputbuffer) free_fifobuffer(&modem.inputbuffer);
			if (modem.outputbuffer) free_fifobuffer(&modem.outputbuffer);
		}
	}
	else
	{
		unsupportedUARTModem: //Unsupported!
		modem.inputbuffer = NULL; //No buffer present!
		modem.outputbuffer = NULL; //No buffer present!
	}
}

void doneModem() //Finish modem!
{
	if (modem.inputbuffer) //Allocated?
	{
		free_fifobuffer(&modem.inputbuffer); //Free our buffer!
	}
	if (modem.outputbuffer) //Allocated?
	{
		free_fifobuffer(&modem.outputbuffer); //Free our buffer!
	}
	TCP_DisconnectClientServer(); //Disconnect, if needed!
	stopTCPServer(); //Stop the TCP server!
}

void cleanModem()
{
	//Nothing to do!
}

void updateModem(double timepassed) //Sound tick. Executes every instruction.
{
	byte datatotransmit;
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
				modem_responseResult(MODEMRESULT_OK); //OK message to escape!
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

	modem.serverpolltimer += timepassed;
	if ((modem.serverpolltimer>=modem.serverpolltick) && modem.serverpolltick) //To poll?
	{
		modem.serverpolltimer = fmod(modem.serverpolltimer,modem.serverpolltick); //Polling once every turn!
		if ((!TCPServerRunning()) && (!modem.connected)) //Server isn't running when need to be?
		{
			TCPServer_restart(modem.connectionport); //Try to restart the TCP server!
		}
		if (acceptTCPServer()) //Are we connected to?
		{
			if ((modem.linechanges&1)==0) //Not able to accept?
			{
				TCPServer_restart(modem.connectionport); //Restart into the TCP server!
			}
			else //Able to accept?
			{
				modem.ringing = 1; //We start ringing!
				modem.registers[1] = 0; //Reset ring counter!
				modem.ringtimer = timepassed; //Automatic time timer, start immediately!
			}
		}
	}

	if (modem.ringing) //Are we ringing?
	{
		modem.ringtimer -= timepassed; //Time!
		if (modem.ringtimer<=0.0) //Timed out?
		{
			++modem.registers[1]; //Increase numbr of rings!
			if ((modem.registers[0]>0) && (modem.registers[1]>=modem.registers[0])) //Autoanswer?
			{
				if (modem_connect(NULL)) //Accept incoming call?
				{
					modem_Answered(); //We've answered!
					return; //Abort: not ringing anymore!
				}
			}
			modem_responseResult(MODEMRESULT_RING); //We're ringing!
			modem.ringtimer += 3000000000.0; //3s timer for every ring!
		}
	}

	modem.networkdatatimer += timepassed;
	if ((modem.networkdatatimer>=modem.networkpolltick) && modem.networkpolltick) //To poll?
	{
		for (;modem.networkdatatimer>=modem.networkpolltick;) //While polling!
		{
			modem.networkdatatimer -= modem.networkpolltick; //Timing this byte by byte!
			if (modem.connected || modem.ringing) //Are we connected?
			{
				if (peekfifobuffer(modem.outputbuffer,&datatotransmit)) //Byte available to send?
				{
					switch (TCP_SendData(datatotransmit)) //Send the data?
					{
					case 0: //Failed to send?
						modem.connected = 0; //Not connected anymore!
						TCPServer_restart(modem.connectionport); //Restart the server!
						modem_responseResult(MODEMRESULT_NOCARRIER);
						modem.datamode = 0; //Drop out of data mode!
						modem.ringing = 0; //Not ringing anymore!
						break; //Abort!
					case 1: //Sent?
						readfifobuffer(modem.outputbuffer,&datatotransmit); //We're send!
						break;
					default: //Unknown function?
						break;
					}
				}
				if (fifobuffer_freesize(modem.inputdatabuffer)) //Free to receive?
				{
					switch (TCP_ReceiveData(&datatotransmit))
					{
					case 0: //Nothing received?
						break;
					case 1: //Something received?
						writefifobuffer(modem.inputdatabuffer,datatotransmit); //Add the transmitted data to the input buffer!
						break;
					case -1: //Disconnected?
						modem.connected = 0; //Not connected anymore!
						TCPServer_restart(modem.connectionport); //Restart server!
						modem_responseResult(MODEMRESULT_NOCARRIER);
						modem.datamode = 0; //Drop out of data mode!
						modem.ringing = 0; //Not ringing anymore!
						break;
					default: //Unknown function?
						break;
					}
				}
			}
		}
	}
}
