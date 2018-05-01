#include "headers/hardware/modem.h" //Our basic definitions!

#include "headers/support/zalloc.h" //Allocation support!
#include "headers/hardware/uart.h" //UART support for the COM port!
#include "headers/support/fifobuffer.h" //FIFO buffer support!
#include "headers/support/locks.h" //Locking support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/support/tcphelper.h" //TCP support!
#include "headers/support/log.h" //Logging support for errors!

#if defined(PACKETSERVER_ENABLED)
#define HAVE_REMOTE
#ifdef IS_WINDOWS
#define WPCAP
#endif
#include <pcap.h>
#include <stdint.h>
#include <stdlib.h>
#endif

/*

Packet server support!

*/

extern BIOS_Settings_TYPE BIOS_Settings; //Currently used settings!

/* packet.c: functions to interface with libpcap/winpcap for ethernet emulation. */

byte PacketServer_running = 0; //Is the packet server running(disables all emulation but hardware)?
uint8_t maclocal[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; //The MAC address of the modem we're emulating!
uint8_t packetserver_broadcastMAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; //The MAC address of the modem we're emulating!
FIFOBUFFER *packetserver_receivebuffer = NULL; //When receiving anything!
byte *packetserver_transmitbuffer = NULL; //When sending a packet, this contains the currently built decoded data, which is already decoded!
uint_32 packetserver_bytesleft = 0;
uint_32 packetserver_transmitlength = 0; //How much has been built?
uint_32 packetserver_transmitsize = 0; //How much has been allocated so far, allocated in whole chunks?
byte packetserver_transmitstate = 0; //Transmit state for processing escaped values!
byte packetserver_sourceMAC[6]; //Our MAC to send from!
byte packetserver_gatewayMAC[6]; //Gateway MAC to send to!
byte packetserver_staticIP[4] = { 0,0,0,0 }; //Static IP to use?
byte packetserver_broadcastIP[4] = { 0xFF,0xFF,0xFF,0xFF }; //Broadcast IP to use?
byte packetserver_useStaticIP = 0; //Use static IP?
char packetserver_staticIPstr[256] = ""; //Static IP, string format

//Authentication data!
char packetserver_username[256]; //Username(settings must match)
char packetserver_password[256]; //Password(settings must match)
char packetserver_protocol[256]; //Protocol(slip). Hangup when sent with username&password not matching setting.
byte packetserver_stage = 0; //Current login/service/packet(connected and authenticated state).
word packetserver_stage_byte = 0; //Byte of data within the current stage(else, use string length or connected stage(no position; in SLIP mode). 0xFFFF=Init new stage.
byte packetserver_stage_byte_overflown = 0; //Overflown?
char packetserver_stage_str[4096]; //Buffer containing output data for a stage
byte packetserver_credentials_invalid = 0; //Marked invalid by username/password/service credentials?
char packetserver_staticIPstr_information[256] = "";
DOUBLE packetserver_delay = 0.0; //Delay for the packet server until doing something!
//How much to delay before sending a message while authenticating?
#define PACKETSERVER_MESSAGE_DELAY 10000000.0

//Different stages of the auth process:
//Ready stage 
//QueryUsername: Sending username request
#define PACKETSTAGE_REQUESTUSERNAME 1
//EnterUsername: Entering username
#define PACKETSTAGE_ENTERUSERNAME 2
//QueryPassword: Sending password request
#define PACKETSTAGE_REQUESTPASSWORD 3
//EnterPassword: Entering password
#define PACKETSTAGE_ENTERPASSWORD 4
//QueryProtocol: Sending protocol request
#define PACKETSTAGE_REQUESTPROTOCOL 5
//EnterProtocol: Entering protocol
#define PACKETSTAGE_ENTERPROTOCOL 6
//Information: IP&MAC autoconfig. Terminates connection when earlier stages invalidate.
#define PACKETSTAGE_INFORMATION 7
//Ready: Sending ready and entering SLIP mode when finished.
#define PACKETSTAGE_READY 8
//SLIP: Transferring SLIP data
#define PACKETSTAGE_SLIP 9
//Initial packet stage without credentials
#define PACKETSTAGE_INIT PACKETSTAGE_REQUESTPROTOCOL
//Initial packet stage with credentials
#define PACKETSTAGE_INIT_PASSWORD PACKETSTAGE_REQUESTUSERNAME
//Packet stage initializing
#define PACKETSTAGE_INITIALIZING 0xFFFF

//SLIP reserved values
//End of frame byte!
#define SLIP_END 0xC0
//Escape byte!
#define SLIP_ESC 0xDB
//END is being send(send after ESC)
#define SLIP_ESC_END 0xDC
//ESC is being send(send after ESC)
#define SLIP_ESC_ESC 0xDD

#ifdef PACKETSERVER_ENABLED
struct netstruct { //Supported, thus use!
#else
struct {
#endif
	uint16_t pktlen;
	byte *packet; //Current packet received!
} net;

#include "headers/packed.h"
typedef union PACKED
{
	struct
	{
		byte dst[6]; //Destination MAC!
		byte src[6]; //Source MAC!
		word type; //What kind of packet!
	};
	byte data[14]; //The data!
} ETHERNETHEADER;
#include "headers/endpacked.h"

uint_32 packetserver_packetpos; //Current pos of sending said packet!

byte readIPnumber(char **x, byte *number); //Prototype!

//Supported and enabled the packet setver?
#if defined(PACKETSERVER_ENABLED)
#ifndef _WIN32
#define PCAP_OPENFLAG_PROMISCUOUS 1
#endif

uint8_t ethif=255, pcap_enabled = 0;
uint8_t dopktrecv = 0;
uint16_t rcvseg, rcvoff, hdrlen, handpkt;

pcap_if_t *alldevs;
pcap_if_t *d;
pcap_t *adhandle;
const u_char *pktdata;
struct pcap_pkthdr *hdr;
int inum;
uint16_t curhandle = 0;
char errbuf[PCAP_ERRBUF_SIZE];
uint8_t maclocal_default[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x13, 0x37 }; //The MAC address of the modem we're emulating!
byte pcap_verbose = 0;

void initPcap() {
	memset(&net,0,sizeof(net)); //Init!
	int i=0;
	char *p;
	byte IPnumbers[4];

	/*

	Custom by superfury

	*/
	PacketServer_running = 0; //We're not using the packet server emulation, enable normal modem(we don't connect to other systems ourselves)!

	if ((BIOS_Settings.ethernetserver_settings.ethernetcard==-1) || (BIOS_Settings.ethernetserver_settings.ethernetcard<0) || (BIOS_Settings.ethernetserver_settings.ethernetcard>255)) //No ethernet card to emulate?
	{
		return; //Disable ethernet emulation!
	}
	ethif = BIOS_Settings.ethernetserver_settings.ethernetcard; //What ethernet card to use?

	//Load MAC address!
	int values[6];

	if( 6 == sscanf( BIOS_Settings.ethernetserver_settings.MACaddress, "%02x:%02x:%02x:%02x:%02x:%02x%*c",
		&values[0], &values[1], &values[2],
		&values[3], &values[4], &values[5] ) ) //Found a MAC address to emulate?
	{
		/* convert to uint8_t */
		for( i = 0; i < 6; ++i )
			maclocal[i] = (uint8_t) values[i]; //MAC address parts!
	}
	else
	{
		memcpy(&maclocal,&maclocal_default,sizeof(maclocal)); //Copy the default MAC address to use!
	}
	if( 6 == sscanf( BIOS_Settings.ethernetserver_settings.gatewayMACaddress, "%02x:%02x:%02x:%02x:%02x:%02x%*c",
		&values[0], &values[1], &values[2],
		&values[3], &values[4], &values[5] ) ) //Found a MAC address to emulate?
	{
		/* convert to uint8_t */
		for( i = 0; i < 6; ++i )
			packetserver_gatewayMAC[i] = (uint8_t) values[i]; //MAC address parts!
	}
	else
	{
		memset(&packetserver_gatewayMAC,0,sizeof(packetserver_gatewayMAC)); //Nothing!
		//We don't have the required addresses! Log and abort!
		dolog("ethernetcard", "Gateway MAC address is required on this platform! Aborting server installation!");
		return; //Disable ethernet emulation!
	}
	
	memcpy(&packetserver_sourceMAC,&maclocal,sizeof(packetserver_sourceMAC)); //Load sender MAC to become active!

	memset(&packetserver_staticIPstr, 0, sizeof(packetserver_staticIPstr));
	memset(&packetserver_staticIP, 0, sizeof(packetserver_staticIP));
	packetserver_useStaticIP = 0; //Default to unused!

	if (safestrlen(&BIOS_Settings.ethernetserver_settings.IPaddress[0], 256) >= 12) //Valid length to convert IP addresses?
	{
		p = &BIOS_Settings.ethernetserver_settings.IPaddress[0]; //For scanning the IP!
		if (readIPnumber(&p, &IPnumbers[0]))
		{
			if (readIPnumber(&p, &IPnumbers[1]))
			{
				if (readIPnumber(&p, &IPnumbers[2]))
				{
					if (readIPnumber(&p, &IPnumbers[3]))
					{
						if (*p == '\0') //EOS?
						{
							//Automatic port?
							snprintf(packetserver_staticIPstr, sizeof(packetserver_staticIPstr), "%u.%u.%u.%u", IPnumbers[0], IPnumbers[1], IPnumbers[2], IPnumbers[3]); //Formulate the address!
							memcpy(&packetserver_staticIP, &IPnumbers, 4); //Set read IP!
							packetserver_useStaticIP = 1; //Static IP set!
						}
					}
				}
			}
		}
	}


	dolog("ethernetcard","Receiver MAC address: %02x:%02x:%02x:%02x:%02x:%02x",maclocal[0],maclocal[1],maclocal[2],maclocal[3],maclocal[4],maclocal[5]);
	dolog("ethernetcard","Gateway MAC Address: %02x:%02x:%02x:%02x:%02x:%02x",packetserver_gatewayMAC[0],packetserver_gatewayMAC[1],packetserver_gatewayMAC[2],packetserver_gatewayMAC[3],packetserver_gatewayMAC[4],packetserver_gatewayMAC[5]); //Log loaded address!
	if (packetserver_useStaticIP) //Static IP configured?
	{
		dolog("ethernetcard","Static IP configured: %s(%02x%02x%02x%02x)",packetserver_staticIPstr,packetserver_staticIP[0],packetserver_staticIP[1],packetserver_staticIP[2],packetserver_staticIP[3]); //Log it!
	}

	packetserver_receivebuffer = allocfifobuffer(2,0); //Simple receive buffer, the size of a packet byte(when encoded) to be able to buffer any packet(since any byte can be doubled)!
	packetserver_transmitlength = 0; //We're at the start of this buffer, nothing is sent yet!

	/*

	End of custom!

	*/

	i = 0; //Init!

	dolog("ethernetcard","Obtaining NIC list via libpcap...");

	/* Retrieve the device list from the local machine */
#if defined(_WIN32)
	if (pcap_findalldevs_ex (PCAP_SRC_IF_STRING, NULL /* auth is not needed */, &alldevs, errbuf) == -1)
#else
	if (pcap_findalldevs (&alldevs, errbuf) == -1)
#endif
		{
			dolog("ethernetcard","Error in pcap_findalldevs_ex: %s", errbuf);
			exit (1);
		}

	/* Print the list */
	for (d= alldevs; d != NULL; d= d->next) {
			i++;
			if (ethif==255) {
					dolog("ethernetcard","%d. %s", i, d->name);
					if (d->description) {
							dolog("ethernetcard"," (%s)", d->description);
						}
					else {
							dolog("ethernetcard"," (No description available)");
						}
				}
		}

	if (i == 0) {
			dolog("ethernetcard","No interfaces found! Make sure WinPcap is installed.");
			return;
		}

	if (ethif==255) exit (0); //Failed: no ethernet card to use!
	else inum = ethif;
	dolog("ethernetcard","Using network interface %u.", ethif);


	if (inum < 1 || inum > i) {
			dolog("ethernetcard","Interface number out of range.");
			/* Free the device list */
			pcap_freealldevs (alldevs);
			return;
		}

	/* Jump to the selected adapter */
	for (d=alldevs, i=0; ((i< inum-1) && d) ; d=d->next, i++);

	/* Open the device */
#ifdef _WIN32
	if ( (adhandle= pcap_open (d->name, 65536, PCAP_OPENFLAG_PROMISCUOUS, -1, NULL, errbuf) ) == NULL)
#else
	if ( (adhandle= pcap_open_live (d->name, 65535, 1, -1, NULL) ) == NULL)
#endif
		{
			dolog("ethernetcard","Unable to open the adapter. %s is not supported by WinPcap", d->name);
			/* Free the device list */
			pcap_freealldevs (alldevs);
			return;
		}

	dolog("ethernetcard","Ethernet bridge on %s...", d->description);

	if (pcap_datalink(adhandle)!=DLT_EN10MB) //Invalid link layer?
	{
		dolog("ethernetcard","Ethernet card unsupported: Ethernet card is required! %s is unsupported!", d->description);
		/* Free the device list */
		pcap_freealldevs (alldevs);
		pcap_close(adhandle); //Close the handle!
		return;		
	}

	/* At this point, we don't need any more the device list. Free it */
	pcap_freealldevs (alldevs);
	pcap_enabled = 1;
	PacketServer_running = 1; //We're using the packet server emulation, disable normal modem(we don't connect to other systems ourselves)!
}

void fetchpackets_pcap() { //Handle any packets to process!
	if (pcap_enabled) //Enabled?
	{
		//Cannot receive until buffer cleared!
		if (net.packet==NULL) //Can we receive anything?
		{
			if (pcap_next_ex (adhandle, &hdr, &pktdata) <=0) return;
			if (hdr->len==0) return;

			net.packet = zalloc(hdr->len,"MODEM_PACKET",NULL);
			if (net.packet) //Allocated?
			{
				memcpy(net.packet, &pktdata[0], hdr->len);
				net.pktlen = (uint16_t) hdr->len;
				if (pcap_verbose) {
						dolog("ethernetcard","Received packet of %u bytes.", net.pktlen);
				}
				//Packet received!
				return;
			}
		}
	}
}

void sendpkt_pcap (uint8_t *src, uint16_t len) {
	if (pcap_enabled) //Enabled?
	{
		pcap_sendpacket (adhandle, src, len);
	}
}

void termPcap()
{
	if (net.packet)
	{
		freez((void **)&net.packet,net.pktlen,"MODEM_PACKET"); //Cleanup!
	}
	if (packetserver_receivebuffer)
	{
		free_fifobuffer(&packetserver_receivebuffer); //Cleanup!
	}
	if (packetserver_transmitbuffer && packetserver_transmitsize) //Gotten a send buffer allocated?
	{
		freez((void **)&packetserver_transmitbuffer,packetserver_transmitsize,"MODEM_SENDPACKET"); //Clear the transmit buffer!
		if (packetserver_transmitbuffer==NULL) packetserver_transmitsize = 0; //Nothing allocated anymore!
	}
	if (pcap_enabled)
	{
		pcap_close(adhandle); //Close the capture/transmit device!
	}
}
#else
//Not supported?
void initPcap() //Unsupported!
{
	memset(&net,0,sizeof(net)); //Init!
}
void sendpkt_pcap (uint8_t *src, uint16_t len)
{
}
void fetchpackets_pcap() //Handle any packets to process!
{
}
void termPcap()
{
}
#endif

void terminatePacketServer() //Cleanup the packet server after being disconnected!
{
	dolog("ethernetcard","Connection by client has been terminated or initialized!");
	fifobuffer_clear(packetserver_receivebuffer); //Clear the receive buffer!
	freez((void **)&packetserver_transmitbuffer,packetserver_transmitsize,"MODEM_SENDPACKET"); //Clear the transmit buffer!
	if (packetserver_transmitbuffer==NULL) packetserver_transmitsize = 0; //Clear!
}

void initPacketServer() //Initialize the packet server for use when connected to!
{
	terminatePacketServer(); //First, make sure we're terminated properly!
	dolog("ethernetcard","Connection by client has been started!");
	packetserver_transmitsize = 1024; //Initialize transmit buffer!
	packetserver_transmitbuffer = zalloc(packetserver_transmitsize,"MODEM_SENDPACKET",NULL); //Initial transmit buffer!
	packetserver_transmitlength = 0; //Nothing buffered yet!
	packetserver_transmitstate = 0; //Initialize transmitter state to the default state!
	packetserver_stage = PACKETSTAGE_INIT; //Initial state when connected.
#ifdef PACKETSERVER_ENABLED
	if (BIOS_Settings.ethernetserver_settings.username[0]&&BIOS_Settings.ethernetserver_settings.password[0]) //Gotten credentials?
	{
		packetserver_stage = PACKETSTAGE_INIT_PASSWORD; //Initial state when connected: ask for credentials too.
	}
#endif
	packetserver_stage_byte = PACKETSTAGE_INITIALIZING; //Reset stage byte: uninitialized!
}

byte packetserver_authenticate()
{
	if (strcmp(packetserver_protocol,"slip")==0) //Valid protocol?
	{
#ifdef PACKETSERVER_ENABLED
		if (!(BIOS_Settings.ethernetserver_settings.username[0]&&BIOS_Settings.ethernetserver_settings.password[0])) //Gotten no credentials?
		{
			return 1; //Always valid: no credentials required!
		}
		else
		{
			if (!(strcmp(BIOS_Settings.ethernetserver_settings.username,packetserver_username)||strcmp(BIOS_Settings.ethernetserver_settings.password,packetserver_password))) //Gotten no credentials?
			{
				return 1; //Valid credentials!
			}
		}
#endif
	}
	return 0; //Invalid credentials!
}

#define MODEM_BUFFERSIZE 256

#define MODEM_SERVERPOLLFREQUENCY 1000
#define MODEM_DATATRANSFERFREQUENCY 57600

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
	DOUBLE timer; //A timer for detecting timeout!
	DOUBLE ringtimer; //Ringing timer!
	DOUBLE serverpolltimer; //Network connection request timer!
	DOUBLE networkdatatimer; //Network connection request timer!

	DOUBLE serverpolltick; //How long it takes!
	DOUBLE networkpolltick;

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
	DOUBLE escapecodeguardtime;
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
	lengthtosend = (word)safestrlen((char *)s,256); //How long to send!
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
	snprintf(s,size,"%u%u%u",(nr%1000)/100,(nr%100)/10,(nr%10)); //Convert to string!
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
		snprintf(s,sizeof(s),"%04u",x); //Convert to a string!
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
	if (PacketServer_running) return 0; //Not OK to send data this way!
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
		*number = (byte)result; //Give the result!
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
	if (PacketServer_running) return 0; //Never connect the modem emulation when we're running as a packet server!
	if (modem.ringing && (phonenumber==NULL) && (PacketServer_running==0)) //Are we ringing and accepting it?
	{
		modem.ringing = 0; //Not ringing anymore!
		modem.connected = 1; //We're connected!
		return 1; //Accepted!
	}
	else if (phonenumber==NULL) //Not ringing, but accepting?
	{
		return 0; //Not connected!
	}
	if (PacketServer_running) return 0; //Don't accept when the packet server is running instead!
	memset(&ipaddress,0,sizeof(ipaddress)); //Init IP address to translate!
	if (safestrlen(phonenumber,256)>=12) //Valid length to convert IP addresses?
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
							snprintf(ipaddress,sizeof(ipaddress),"%u.%u.%u.%u",a,b,c,d); //Formulate the address!
							port = modem.connectionport; //Use the default port as specified!
						}
						else if (*p==':') //Port might follow?
						{
							++p; //Skip character!
							if (sscanf(p,"%u",&port)==0) //Port incorrectly read?
							{
								return 0; //Fail: invalid port has been specified!
							}
							snprintf(ipaddress,sizeof(ipaddress),"%u.%u.%u.%u",a,b,c,d);
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
			safestrcpy(ipaddress,sizeof(ipaddress),phonenumber); //Raw IP with port!
			ipaddress[(ptrnum)p-(ptrnum)phonenumber] = '\0'; //Cut off the port part!
			++p; //Take the port itself!
			if (sscanf(p,"%u",&port)==0) //Port incorrectly read?
			{
				return 0; //Fail: invalid port has been specified!
			}
		}
		else //Raw address?
		{
			safestrcpy(ipaddress,sizeof(ipaddress),phonenumber); //Use t
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
	modem.connected &= ~1; //Not connected anymore!
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
			#ifdef IS_LONGDOUBLE
			modem.escapecodeguardtime = (modem.registers[reg]*20000000.0L); //Set the escape code guard time, in nanoseconds!
			#else
			modem.escapecodeguardtime = (modem.registers[reg]*20000000.0); //Set the escape code guard time, in nanoseconds!
			#endif
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
		modem_updateRegister((byte)reg); //This register has been updated!
	}

	modem.communicationstandard = 0; //Default communication standard!

	//Result defaults
	modem.echomode = 1; //Default: echo back!
	modem.verbosemode = 1; //Text-mode verbose!

	modem.flowcontrol = 0; //Default flow control!
	memset(&modem.lastnumber,0,sizeof(modem.lastnumber)); //No last number!
	modem.offhook = 0; //On-hook!
	if (modem.connected&1) //Are we still connected?
	{
		modem.connected &= ~1; //Disconnect!
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
				if ((modem.connected&1) || modem.ringing) //Are we connected?
				{
					modem_responseResult(MODEMRESULT_NOCARRIER); //No carrier!
					modem_hangup(); //Hang up!
				}
			case 1: //Goto AT command mode?
				modem.datamode = (byte)(modem.ATcommandsize = 0); //Starting a new command!
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
	return (modem.datamode?(modem.CTSAlwaysActive?1:((modem.linechanges>>1)&1)):1)|(modem.DSRisConnectionEstablished?((modem.connected==1)?2:0):2)|(modem.ringing?4:0)|(((modem.connected==1)||(modem.DCDisCarrier==0))?8:0); //0=CTS(can we receive data to send?), 1=DSR(are we ready for use), 2=Ring, 3=Carrier detect!
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
				memcpy(&number,&modem.lastnumber,(safestrlen((char *)&modem.lastnumber[0],sizeof(modem.lastnumber))+1)); //Set the new number to roll!
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
				safestrcpy((char *)&number[0],sizeof(number),(char *)&modem.ATcommand[pos]); //Set the number to dial!
				memset(&modem.lastnumber,0,sizeof(modem.lastnumber)); //Init last number!
				safestrcpy((char *)&modem.lastnumber,sizeof(modem.lastnumber),(char *)&number[0]); //Set the last number!
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
					if ((((modem.connected&1) || modem.ringing)&&(!modem.offhook)) || (modem.offhook && (!((modem.connected&1)||modem.ringing)))) //Disconnected or still ringing/connected?
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
				if (modem.connected&1) //Connected?
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
			#ifdef IS_LONGDOUBLE
			modem.serverpolltick = (1000000000.0L/(DOUBLE)MODEM_SERVERPOLLFREQUENCY); //Server polling rate of connections!
			modem.networkpolltick = (1000000000.0L/(DOUBLE)MODEM_DATATRANSFERFREQUENCY); //Data transfer polling rate!
			#else
			modem.serverpolltick = (1000000000.0/(DOUBLE)MODEM_SERVERPOLLFREQUENCY); //Server polling rate of connections!
			modem.networkpolltick = (1000000000.0/(DOUBLE)MODEM_DATATRANSFERFREQUENCY); //Data transfer polling rate!
			#endif
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
	terminatePacketServer(); //Stop the packet server, if used!
}

void cleanModem()
{
	//Nothing to do!
}

byte packetServerAddWriteQueue(byte data) //Try to add something to the write queue!
{
	byte *newbuffer;
	if (packetserver_transmitlength>=packetserver_transmitsize) //We need to expand the buffer?
	{
		newbuffer = zalloc(packetserver_transmitsize+1024,"MODEM_SENDPACKET",NULL); //Try to allocate a larger buffer!
		if (newbuffer) //Allocated larger buffer?
		{
			dolog("ethernetcard","extending transmit buffer because of buffer shortage(%u)!",packetserver_transmitsize+1024);
			memcpy(newbuffer,packetserver_transmitbuffer,packetserver_transmitsize); //Copy the new data over to the larger buffer!
			freez((void **)&packetserver_transmitbuffer,packetserver_transmitsize,"MODEM_SENDPACKET"); //Release the old buffer!
			packetserver_transmitbuffer = newbuffer; //The new buffer is the enlarged buffer, ready to have been written more data!
			packetserver_transmitsize += 1024; //We've been increased to this larger buffer!
			packetserver_transmitbuffer[packetserver_transmitlength++] = data; //Add the data to the buffer!
			return 1; //Success!
		}
	}
	else //Normal buffer usage?
	{
		packetserver_transmitbuffer[packetserver_transmitlength++] = data; //Add the data to the buffer!
		return 1; //Success!
	}
	return 0; //Failed!
}

void logpacket(byte send, byte *buffer, uint_32 size)
{
	char outbuffer[0x20001]; //Buffer for storin the data!
	char filename[256]; //For storing the raw packet that's sent!
	uint_32 i;
	char adding[3];
	memset(&filename,0,sizeof(filename));
	memset(&outbuffer,0,sizeof(outbuffer));
	memset(&adding,0,sizeof(adding));
	for (i=0;i<size;++i)
	{
		snprintf(adding,sizeof(adding),"%02X",buffer[i]); //Set and ...
		safestrcat(outbuffer,sizeof(outbuffer),adding); //... Add!
	}
	if (send)
	{
		dolog("ethernetcard","Sending packet:");
	}
	else
	{
		dolog("ethernetcard","Receiving packet:");
	}
	dolog("ethernetcard","%s",outbuffer); //What's received/sent!
}

void updateModem(DOUBLE timepassed) //Sound tick. Executes every instruction.
{
	byte datatotransmit;
	ETHERNETHEADER ethernetheader;
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
			if (((modem.linechanges&1)==0) && (PacketServer_running==0)) //Not able to accept?
			{
				TCPServer_restart(modem.connectionport); //Restart into the TCP server!
			}
			else //Able to accept?
			{
				if (PacketServer_running) //Packet server is running?
				{
					modem.connected = 2; //Connect as packet server instead, we start answering manually instead of the emulated modem!
					modem.ringing = 0; //Never ring!
					initPacketServer(); //Initialize the packet server to be used!
				}
				else //Normal behaviour: start ringing!
				{
					modem.ringing = 1; //We start ringing!
					modem.registers[1] = 0; //Reset ring counter!
					modem.ringtimer = timepassed; //Automatic time timer, start immediately!
				}
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
			#ifdef IS_LONGDOUBLE
			modem.ringtimer += 3000000000.0L; //3s timer for every ring!
			#else
			modem.ringtimer += 3000000000.0; //3s timer for every ring!
			#endif
		}
	}

	modem.networkdatatimer += timepassed;
	if ((modem.networkdatatimer>=modem.networkpolltick) && modem.networkpolltick) //To poll?
	{
		for (;modem.networkdatatimer>=modem.networkpolltick;) //While polling!
		{
			modem.networkdatatimer -= modem.networkpolltick; //Timing this byte by byte!
			if (net.packet && (!((modem.connected&2) && (packetserver_stage==PACKETSTAGE_SLIP)))) //Received a packet while not listening?
			{
				freez((void **)&net.packet,net.pktlen,"MODEM_PACKET"); //Release the packet to receive new packets again!				
			}
			if (modem.connected || modem.ringing) //Are we connected?
			{
				if (modem.connected==2) //Running the packet server?
				{
					if (packetserver_stage!=PACKETSTAGE_SLIP) goto skipSLIP; //Don't handle SLIP!
					//Handle packet server packet data transfers into the inputdatabuffer/outputbuffer to the network!
					if (packetserver_receivebuffer) //Properly allocated?
					{
						if (net.packet) //Packet has been received? Try to start transmit it!
						{
							if (fifobuffer_freesize(packetserver_receivebuffer)>=2) //Valid to produce more data?
							{
								if (packetserver_packetpos==0) //New packet?
								{
									if (net.pktlen>(sizeof(ethernetheader.data)+20)) //Length OK(at least one byte of data and complete IP header)?
									{
										memcpy(&ethernetheader.data,net.packet,sizeof(ethernetheader.data)); //Copy for inspection!
										if (ethernetheader.type!=SDL_SwapBE16(0x0800)) //Invalid type?
										{
											//dolog("ethernetcard","Discarding type: %04X",SDL_SwapBE16(ethernetheader.type)); //Showing why we discard!
											goto invalidpacket; //Invalid packet!
										}
										if ((memcmp(&ethernetheader.dst,&packetserver_sourceMAC,sizeof(ethernetheader.dst))!=0) && (memcmp(&ethernetheader.dst,&packetserver_broadcastMAC,sizeof(ethernetheader.dst))!=0)) //Invalid destination(and not broadcasting)?
										{
											//dolog("ethernetcard","Discarding destination."); //Showing why we discard!
											goto invalidpacket; //Invalid packet!
										}
										if (packetserver_useStaticIP) //IP filter?
										{
											if ((memcmp(&net.packet[sizeof(ethernetheader.data) + 16], packetserver_staticIP, 4) != 0) && (memcmp(&net.packet[sizeof(ethernetheader.data) + 16], packetserver_broadcastIP, 4) != 0)) //Static IP mismatch?
											{
												goto invalidpacket; //Invalid packet!
											}
										}
										//Valid packet! Receive it!
										packetserver_packetpos = sizeof(ethernetheader.data); //Skip the ethernet header and give the raw IP data!
										//dolog("ethernetcard","Skipping %u bytes of header data...",packetserver_packetpos); //Log it!
										packetserver_bytesleft = MIN(net.pktlen - packetserver_packetpos, SDL_SwapBE16(*((word *)&net.packet[sizeof(ethernetheader.data)+2]))); //How much is left to send?
									}
									else //Invalid length?
									{
										//dolog("ethernetcard","Discarding invalid packet size: %u...",net.pktlen); //Log it!
										invalidpacket:
										//Discard the invalid packet!
										freez((void **)&net.packet,net.pktlen,"MODEM_PACKET"); //Release the packet to receive new packets again!
										//dolog("ethernetcard","Discarding invalid packet size or different cause: %u...",net.pktlen); //Log it!
										net.packet = NULL; //No packet!
										packetserver_packetpos = 0; //Reset packet position!
									}
								}
								if (net.packet) //Still a valid packet to send?
								{
									//Convert the buffer into transmittable bytes using the proper encoding!
									if (packetserver_bytesleft) //Not finished yet?
									{
										//Start transmitting data into the buffer, according to the protocol!
										--packetserver_bytesleft;
										datatotransmit = net.packet[packetserver_packetpos++]; //Read the data to construct!
										if (datatotransmit==SLIP_END) //End byte?
										{
											//dolog("ethernetcard","transmitting escaped SLIP END to client");
											writefifobuffer(packetserver_receivebuffer,SLIP_ESC); //Escaped ...
											writefifobuffer(packetserver_receivebuffer,SLIP_ESC_END); //END raw data!
										}
										else if (datatotransmit==SLIP_ESC) //ESC byte?
										{
											//dolog("ethernetcard","transmitting escaped SLIP ESC to client");
											writefifobuffer(packetserver_receivebuffer,SLIP_ESC); //Escaped ...
											writefifobuffer(packetserver_receivebuffer,SLIP_ESC_ESC); //ESC raw data!
										}
										else //Normal data?
										{
											//dolog("ethernetcard","transmitting raw to client: %02X",datatotransmit);
											writefifobuffer(packetserver_receivebuffer,datatotransmit); //Unescaped!
										}
									}
									else //Finished transferring a frame?
									{
										//dolog("ethernetcard","transmitting SLIP END to client and finishing packet buffer(size: %u)",net.pktlen);
										writefifobuffer(packetserver_receivebuffer,SLIP_END); //END of frame!
										logpacket(0,net.packet,net.pktlen); //Log it!
										freez((void **)&net.packet,net.pktlen,"MODEM_PACKET"); //Release the packet to receive new packets again!
										net.packet = NULL; //Discard the packet anyway, no matter what!
										packetserver_packetpos = 0; //Reset packet position!
									}
								}
							}
						}

						//Transmit the encoded packet buffer to the client!
						if (fifobuffer_freesize(modem.outputbuffer) && peekfifobuffer(packetserver_receivebuffer,&datatotransmit)) //Able to transmit something?
						{
							for (;fifobuffer_freesize(modem.outputbuffer) && peekfifobuffer(packetserver_receivebuffer,&datatotransmit);) //Can we still transmit something more?
							{
								if (writefifobuffer(modem.outputbuffer,datatotransmit)) //Transmitted?
								{
									//dolog("ethernetcard","transmitted SLIP data to client: %02X",datatotransmit);
									datatotransmit = readfifobuffer(packetserver_receivebuffer,&datatotransmit); //Discard the data that's being transmitted!
								}
							}
						}
					}

					//Handle transmitting packets(with automatically increasing buffer sizing, as a packet can be received of any size theoretically)!
					if (peekfifobuffer(modem.inputdatabuffer,&datatotransmit)) //Is anything transmitted yet?
					{
						if (packetserver_transmitlength==0) //We might need to create an ethernet header?
						{
							//Build an ethernet header, platform dependent!
							//Use the data provided by the settings!
							byte b;
							for (b=0;b<6;++b) //Process MAC addresses!
							{
								ethernetheader.dst[b] = packetserver_gatewayMAC[b]; //Gateway MAC is the destination!
								ethernetheader.src[b] = packetserver_sourceMAC[b]; //Packet server MAC is the source!
							}
							ethernetheader.type = SDL_SwapBE16(0x0800); //We're an IP packet!
							for (b=0;b<14;++b) //Use the provided ethernet packet header!
							{
								if (!packetServerAddWriteQueue(ethernetheader.data[b])) //Failed to add?
								{
									break; //Stop adding!
								}
							}
							if (packetserver_transmitlength!=14) //Failed to generate header?
							{
								dolog("ethernetcard","Error: Transmit initialization failed. Resetting transmitter!");
								packetserver_transmitlength = 0; //Abort the packet generation!
							}
							else
							{
								//dolog("ethernetcard","Header for transmitting to the server has been setup!");
							}
						}
						if (datatotransmit==SLIP_END) //End-of-frame? Send the frame!
						{
							readfifobuffer(modem.inputdatabuffer,&datatotransmit); //Ignore the data, just discard the packet END!
							//Clean up the packet container!
							if (packetserver_transmitlength>sizeof(ethernetheader.data)) //Anything buffered(the header is required)?
							{
								//Send the frame to the server, if we're able to!
								if (packetserver_transmitlength<=0xFFFF) //Within length range?
								{
									//dolog("ethernetcard","Sending generated packet(size: %u)!",packetserver_transmitlength);
									logpacket(1,packetserver_transmitbuffer,packetserver_transmitlength); //Log it!

									sendpkt_pcap(packetserver_transmitbuffer,packetserver_transmitlength); //Send the packet!
								}
								else
								{
									dolog("ethernetcard","Error: Can't send packet: packet is too large to send(size: %u)!",packetserver_transmitlength);									
								}

								//Now, cleanup the buffered frame!
								freez((void **)&packetserver_transmitbuffer,packetserver_transmitsize,"MODEM_SENDPACKET"); //Free 
								packetserver_transmitsize = 1024; //How large is out transmit buffer!
								packetserver_transmitbuffer = zalloc(1024,"MODEM_SENDPACKET",NULL); //Simple transmit buffer, the size of a packet byte(when encoded) to be able to buffer any packet(since any byte can be doubled)!
							}
							else
							{
								dolog("ethernetcard","Error: Not enough buffered to send to the server(size: %u)!",packetserver_transmitlength);
							}
							packetserver_transmitlength = 0; //We're at the start of this buffer, nothing is sent yet!
							packetserver_transmitstate = 0; //Not escaped anymore!
						}
						else if (datatotransmit==SLIP_ESC) //Escaped something?
						{
							readfifobuffer(modem.inputdatabuffer,&datatotransmit); //Discard, as it's processed!
							packetserver_transmitstate = 1; //We're escaping something! Multiple escapes are ignored and not sent!
						}
						else //Active data?
						{
							if (packetserver_transmitlength) //Gotten a valid packet?
							{
								if (packetserver_transmitstate && (datatotransmit==SLIP_ESC_END)) //END sent?
								{
									if (packetServerAddWriteQueue(SLIP_END)) //Added to the queue?
									{
										readfifobuffer(modem.inputdatabuffer,&datatotransmit); //Ignore the data, just discard the packet byte!
										packetserver_transmitstate = 0; //We're not escaping something anymore!
									}
								}
								else if (packetserver_transmitstate && (datatotransmit==SLIP_ESC_ESC)) //ESC sent?
								{
									if (packetServerAddWriteQueue(SLIP_ESC)) //Added to the queue?
									{
										readfifobuffer(modem.inputdatabuffer,&datatotransmit); //Ignore the data, just discard the packet byte!
										packetserver_transmitstate = 0; //We're not escaping something anymore!
									}
								}
								else //Parse as a raw data when invalidly escaped or sent unescaped! Also terminate escape sequence!
								{
									if (packetServerAddWriteQueue(datatotransmit)) //Added to the queue?
									{
										readfifobuffer(modem.inputdatabuffer,&datatotransmit); //Ignore the data, just discard the packet byte!
										packetserver_transmitstate = 0; //We're not escaping something anymore!
									}
								}
							}
						}
					}
					skipSLIP: //SLIP isn't available?

					//Handle an authentication stage
					if (packetserver_stage==PACKETSTAGE_REQUESTUSERNAME)
					{
						if (packetserver_stage_byte==PACKETSTAGE_INITIALIZING)
						{
							memset(&packetserver_stage_str,0,sizeof(packetserver_stage_str));
							safestrcpy(packetserver_stage_str,sizeof(packetserver_stage_str),"username:");
							packetserver_stage_byte = 0; //Init to start of string!
							packetserver_credentials_invalid = 0; //No invalid field detected yet!
							packetserver_delay = PACKETSERVER_MESSAGE_DELAY; //Delay this until we start transmitting!
						}
						packetserver_delay -= timepassed; //Delaying!
						if ((packetserver_delay<=0.0) || (!packetserver_delay)) //Finished?
						{
							packetserver_delay = (DOUBLE)0; //Finish the delay!
							if (writefifobuffer(modem.outputbuffer,packetserver_stage_str[packetserver_stage_byte])) //Transmitted?
							{
								if (++packetserver_stage_byte==safestrlen(packetserver_stage_str,sizeof(packetserver_stage_str))) //Finished?
								{
									packetserver_stage_byte = PACKETSTAGE_INITIALIZING; //Prepare for next step!
									packetserver_stage = PACKETSTAGE_ENTERUSERNAME;
								}
							}
						}
					}

					byte textinputfield=0;
					if (packetserver_stage==PACKETSTAGE_ENTERUSERNAME)
					{
						if (packetserver_stage_byte==PACKETSTAGE_INITIALIZING)
						{
							memset(&packetserver_username,0,sizeof(packetserver_username));
							packetserver_stage_byte = 0; //Init to start filling!
							packetserver_stage_byte_overflown = 0; //Not yet overflown!
						}
						if (peekfifobuffer(modem.inputdatabuffer,&textinputfield)) //Transmitted?
						{
							if (writefifobuffer(modem.outputbuffer,textinputfield)) //Echo back to user!
							{
								readfifobuffer(modem.inputdatabuffer,&textinputfield); //Discard the input!
								if ((textinputfield=='\r') || (textinputfield=='\n')) //Finished?
								{
									packetserver_username[packetserver_stage_byte] = '\0'; //Finish the string!
									packetserver_credentials_invalid |= packetserver_stage_byte_overflown; //Overflow has occurred?
									packetserver_stage_byte = PACKETSTAGE_INITIALIZING; //Prepare for next step!
									packetserver_stage = PACKETSTAGE_REQUESTPASSWORD;
								}
								else
								{
									if ((textinputfield=='\0') || ((packetserver_stage_byte+1)>=sizeof(packetserver_username)) || packetserver_stage_byte_overflown) //Future overflow, overflow already occurring or invalid input to add?
									{
										packetserver_stage_byte_overflown = 1; //Overflow detected!
									}
									else //Valid content to add?
									{
										packetserver_username[packetserver_stage_byte++] = textinputfield; //Add input!
									}
								}
							}
						}
					}

					if (packetserver_stage==PACKETSTAGE_REQUESTPASSWORD)
					{
						if (packetserver_stage_byte==PACKETSTAGE_INITIALIZING)
						{
							memset(&packetserver_stage_str,0,sizeof(packetserver_stage_str));
							safestrcpy(packetserver_stage_str,sizeof(packetserver_stage_str),"password:");
							packetserver_stage_byte = 0; //Init to start of string!
							packetserver_delay = PACKETSERVER_MESSAGE_DELAY; //Delay this until we start transmitting!
						}
						packetserver_delay -= timepassed; //Delaying!
						if ((packetserver_delay<=0.0) || (!packetserver_delay)) //Finished?
						{
							packetserver_delay = (DOUBLE)0; //Finish the delay!
							if (writefifobuffer(modem.outputbuffer,packetserver_stage_str[packetserver_stage_byte])) //Transmitted?
							{
								if (++packetserver_stage_byte==safestrlen(packetserver_stage_str,sizeof(packetserver_stage_str))) //Finished?
								{
									packetserver_stage_byte = PACKETSTAGE_INITIALIZING; //Prepare for next step!
									packetserver_stage = PACKETSTAGE_ENTERPASSWORD;
								}
							}
						}
					}

					if (packetserver_stage==PACKETSTAGE_ENTERPASSWORD)
					{
						if (packetserver_stage_byte==PACKETSTAGE_INITIALIZING)
						{
							memset(&packetserver_password,0,sizeof(packetserver_password));
							packetserver_stage_byte = 0; //Init to start filling!
							packetserver_stage_byte_overflown = 0; //Not yet overflown!
						}
						if (peekfifobuffer(modem.inputdatabuffer,&textinputfield)) //Transmitted?
						{
							if (writefifobuffer(modem.outputbuffer,textinputfield)) //Echo back to user!
							{
								readfifobuffer(modem.inputdatabuffer,&textinputfield); //Discard the input!
								if ((textinputfield=='\r') || (textinputfield=='\n')) //Finished?
								{
									packetserver_password[packetserver_stage_byte] = '\0'; //Finish the string!
									packetserver_credentials_invalid |= packetserver_stage_byte_overflown; //Overflow has occurred?
									packetserver_stage_byte = PACKETSTAGE_INITIALIZING; //Prepare for next step!
									packetserver_stage = PACKETSTAGE_REQUESTPROTOCOL;
								}
								else
								{
									if ((textinputfield=='\0') || ((packetserver_stage_byte+1)>=sizeof(packetserver_password)) || packetserver_stage_byte_overflown) //Future overflow, overflow already occurring or invalid input to add?
									{
										packetserver_stage_byte_overflown = 1; //Overflow detected!
									}
									else //Valid content to add?
									{
										packetserver_password[packetserver_stage_byte++] = textinputfield; //Add input!
									}
								}
							}
						}
					}

					if (packetserver_stage==PACKETSTAGE_REQUESTPROTOCOL)
					{
						if (packetserver_stage_byte==PACKETSTAGE_INITIALIZING)
						{
							memset(&packetserver_stage_str,0,sizeof(packetserver_stage_str));
							safestrcpy(packetserver_stage_str,sizeof(packetserver_stage_str),"protocol:");
							packetserver_stage_byte = 0; //Init to start of string!
							packetserver_delay = PACKETSERVER_MESSAGE_DELAY; //Delay this until we start transmitting!
						}
						packetserver_delay -= timepassed; //Delaying!
						if ((packetserver_delay<=0.0) || (!packetserver_delay)) //Finished?
						{
							packetserver_delay = (DOUBLE)0; //Finish the delay!
							if (writefifobuffer(modem.outputbuffer,packetserver_stage_str[packetserver_stage_byte])) //Transmitted?
							{
								if (++packetserver_stage_byte==safestrlen(packetserver_stage_str,sizeof(packetserver_stage_str))) //Finished?
								{
									packetserver_stage_byte = PACKETSTAGE_INITIALIZING; //Prepare for next step!
									packetserver_stage = PACKETSTAGE_ENTERPROTOCOL;
								}
							}
						}
					}

					if (packetserver_stage==PACKETSTAGE_ENTERPROTOCOL)
					{
						if (packetserver_stage_byte==PACKETSTAGE_INITIALIZING)
						{
							memset(&packetserver_protocol,0,sizeof(packetserver_protocol));
							packetserver_stage_byte = 0; //Init to start filling!
							packetserver_stage_byte_overflown = 0; //Not yet overflown!
#ifdef PACKETSERVER_ENABLED
							if (!(BIOS_Settings.ethernetserver_settings.username[0]&&BIOS_Settings.ethernetserver_settings.password[0])) //Gotten no credentials?
							{
								packetserver_credentials_invalid = 0; //Init!
							}
#endif
						}
						if (peekfifobuffer(modem.inputdatabuffer,&textinputfield)) //Transmitted?
						{
							if (writefifobuffer(modem.outputbuffer,textinputfield)) //Echo back to user!
							{
								readfifobuffer(modem.inputdatabuffer,&textinputfield); //Discard the input!
								if ((textinputfield=='\r') || (textinputfield=='\n')) //Finished?
								{
									packetserver_protocol[packetserver_stage_byte] = '\0'; //Finish the string!
									packetserver_credentials_invalid |= packetserver_stage_byte_overflown; //Overflow has occurred?
									packetserver_stage_byte = PACKETSTAGE_INITIALIZING; //Prepare for next step!
									if (packetserver_credentials_invalid) goto packetserver_autherror; //Authentication error!
									if (packetserver_authenticate()) //Authenticated?
									{
										packetserver_stage = PACKETSTAGE_INFORMATION; //We're logged in!
									}
									else goto packetserver_autherror; //Authentication error!
								}
								else
								{
									if ((textinputfield=='\0') || ((packetserver_stage_byte+1)>=sizeof(packetserver_protocol)) || packetserver_stage_byte_overflown) //Future overflow, overflow already occurring or invalid input to add?
									{
										packetserver_stage_byte_overflown = 1; //Overflow detected!
									}
									else //Valid content to add?
									{
										packetserver_protocol[packetserver_stage_byte++] = textinputfield; //Add input!
									}
								}
							}
						}
					}

					if (packetserver_stage==PACKETSTAGE_INFORMATION)
					{
						if (packetserver_stage_byte==PACKETSTAGE_INITIALIZING)
						{
							memset(&packetserver_stage_str,0,sizeof(packetserver_stage_str));
							snprintf(packetserver_stage_str,sizeof(packetserver_stage_str),"\r\nMACaddress:%02x:%02x:%02x:%02x:%02x:%02x\r\ngatewayMACaddress:%02x:%02x:%02x:%02x:%02x:%02x\r\n",packetserver_sourceMAC[0],packetserver_sourceMAC[1],packetserver_sourceMAC[2],packetserver_sourceMAC[3],packetserver_sourceMAC[4],packetserver_sourceMAC[5],packetserver_gatewayMAC[0],packetserver_gatewayMAC[1],packetserver_gatewayMAC[2],packetserver_gatewayMAC[3],packetserver_gatewayMAC[4],packetserver_gatewayMAC[5]);
							if (packetserver_useStaticIP) //IP filter?
							{
								memset(&packetserver_staticIPstr_information,0,sizeof(packetserver_staticIPstr_information));
								snprintf(packetserver_staticIPstr_information,sizeof(packetserver_staticIPstr_information),"IPaddress:%s\r\n",packetserver_staticIPstr); //Static IP!
								safestrcat(packetserver_stage_str,sizeof(packetserver_stage_str),packetserver_staticIPstr_information); //Inform about the static IP!
							}
							packetserver_stage_byte = 0; //Init to start of string!
							packetserver_delay = PACKETSERVER_MESSAGE_DELAY; //Delay this until we start transmitting!
						}
						packetserver_delay -= timepassed; //Delaying!
						if ((packetserver_delay<=0.0) || (!packetserver_delay)) //Finished?
						{
							packetserver_delay = (DOUBLE)0; //Finish the delay!
							if (writefifobuffer(modem.outputbuffer,packetserver_stage_str[packetserver_stage_byte])) //Transmitted?
							{
								if (++packetserver_stage_byte==safestrlen(packetserver_stage_str,sizeof(packetserver_stage_str))) //Finished?
								{
									packetserver_stage_byte = PACKETSTAGE_INITIALIZING; //Prepare for next step!
									packetserver_stage = PACKETSTAGE_READY;
								}
							}
						}
					}

					if (packetserver_stage==PACKETSTAGE_READY)
					{
						if (packetserver_stage_byte==PACKETSTAGE_INITIALIZING)
						{
							memset(&packetserver_stage_str,0,sizeof(packetserver_stage_str));
							safestrcpy(packetserver_stage_str,sizeof(packetserver_stage_str),"\rCONNECTED\r");
							packetserver_stage_byte = 0; //Init to start of string!
							packetserver_delay = PACKETSERVER_MESSAGE_DELAY; //Delay this until we start transmitting!
						}
						packetserver_delay -= timepassed; //Delaying!
						if ((packetserver_delay<=0.0) || (!packetserver_delay)) //Finished?
						{
							packetserver_delay = (DOUBLE)0; //Finish the delay!
							if (writefifobuffer(modem.outputbuffer,packetserver_stage_str[packetserver_stage_byte])) //Transmitted?
							{
								if (++packetserver_stage_byte==safestrlen(packetserver_stage_str,sizeof(packetserver_stage_str))) //Finished?
								{
									packetserver_stage_byte = PACKETSTAGE_INITIALIZING; //Prepare for next step!
									packetserver_stage = PACKETSTAGE_SLIP; //Start the SLIP service!
								}
							}
						}
					}
				}

				if (peekfifobuffer(modem.outputbuffer,&datatotransmit)) //Byte available to send?
				{
					switch (TCP_SendData(datatotransmit)) //Send the data?
					{
					case 0: //Failed to send?
						packetserver_autherror: //Packet server authentication error?
						modem.connected = 0; //Not connected anymore!
						TCPServer_restart(modem.connectionport); //Restart the server!
						if (PacketServer_running==0) //Not running a packet server?
						{
							modem_responseResult(MODEMRESULT_NOCARRIER);
							modem.datamode = 0; //Drop out of data mode!
							modem.ringing = 0; //Not ringing anymore!
						}
						else //Disconnect from packet server?
						{
							terminatePacketServer(); //Clean up the packet server!
						}
						goto skiptransfers;
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
						if (PacketServer_running==0) //Not running a packet server?
						{
							modem_responseResult(MODEMRESULT_NOCARRIER);
							modem.datamode = 0; //Drop out of data mode!
							modem.ringing = 0; //Not ringing anymore!
						}
						else //Disconnect from packet server?
						{
							terminatePacketServer(); //Clean up the packet server!
						}
						break;
					default: //Unknown function?
						break;
					}
				}
			} //Connected?

			skiptransfers: //For disconnects!

			fetchpackets_pcap(); //Handle any packets that need fetching!
		} //While polling?
	} //To poll?
}
