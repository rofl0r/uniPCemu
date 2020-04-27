/*

Copyright (C) 2019  Superfury

This file is part of UniPCemu.

UniPCemu is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

UniPCemu is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with UniPCemu.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "headers/hardware/modem.h" //Our basic definitions!

#include "headers/support/zalloc.h" //Allocation support!
#include "headers/hardware/uart.h" //UART support for the COM port!
#include "headers/support/fifobuffer.h" //FIFO buffer support!
#include "headers/support/locks.h" //Locking support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/support/tcphelper.h" //TCP support!
#include "headers/support/log.h" //Logging support for errors!

//Compile without PCAP support, but with server simulation when NOPCAP and PACKERSERVER_ENABLED is defined(essentially a server without login information and PCap support(thus no packets being sent/received))?
/*
#define NOPCAP
#define PACKETSERVER_ENABLED
*/

#if defined(PACKETSERVER_ENABLED)
#define HAVE_REMOTE
#ifdef IS_WINDOWS
#define WPCAP
#endif
#ifndef NOPCAP
#include <pcap.h>
#endif
#include <stdint.h>
#include <stdlib.h>
#endif

/*

Packet server support!

*/

#ifdef IS_PSP
#ifndef SDL_SwapBE16
//PSP doesn't support SDL_SwapBE16
#define SDL_SwapBE16(x) ((((x)>>8)&0xFF)|(((x)&0xFF)<<8))
#endif
#endif

extern BIOS_Settings_TYPE BIOS_Settings; //Currently used settings!

/* packet.c: functions to interface with libpcap/winpcap for ethernet emulation. */

byte PacketServer_running = 0; //Is the packet server running(disables all emulation but hardware)?
uint8_t maclocal[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; //The MAC address of the modem we're emulating!
uint8_t packetserver_broadcastMAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; //The MAC address of the modem we're emulating!
byte packetserver_sourceMAC[6]; //Our MAC to send from!
byte packetserver_gatewayMAC[6]; //Gateway MAC to send to!
byte packetserver_defaultstaticIP[4] = { 0,0,0,0 }; //Static IP to use?
byte packetserver_broadcastIP[4] = { 0xFF,0xFF,0xFF,0xFF }; //Broadcast IP to use?
byte packetserver_usedefaultStaticIP = 0; //Use static IP?
char packetserver_defaultstaticIPstr[256] = ""; //Static IP, string format

typedef struct
{
	byte* buffer;
	uint_32 size;
	uint_32 length;
} PPPOE_PAD_PACKETBUFFER; //Packet buffer for PAD packets!

//Authentication data and user-specific data!
typedef struct
{
	uint16_t pktlen;
	byte *packet; //Current packet received!
	FIFOBUFFER *packetserver_receivebuffer; //When receiving anything!
	byte *packetserver_transmitbuffer; //When sending a packet, this contains the currently built decoded data, which is already decoded!
	uint_32 packetserver_bytesleft;
	uint_32 packetserver_transmitlength; //How much has been built?
	uint_32 packetserver_transmitsize; //How much has been allocated so far, allocated in whole chunks?
	byte packetserver_transmitstate; //Transmit state for processing escaped values!
	char packetserver_username[256]; //Username(settings must match)
	char packetserver_password[256]; //Password(settings must match)
	char packetserver_protocol[256]; //Protocol(slip). Hangup when sent with username&password not matching setting.
	char packetserver_staticIP[4]; //Static IP to assign this user!
	char packetserver_staticIPstr[256]; //Static IP, string format
	byte packetserver_useStaticIP; //Use static IP?
	byte packetserver_slipprotocol; //Are we using the slip protocol?
	byte packetserver_stage; //Current login/service/packet(connected and authenticated state).
	word packetserver_stage_byte; //Byte of data within the current stage(else, use string length or connected stage(no position; in SLIP mode). 0xFFFF=Init new stage.
	byte packetserver_stage_byte_overflown; //Overflown?
	char packetserver_stage_str[4096]; //Buffer containing output data for a stage
	byte packetserver_credentials_invalid; //Marked invalid by username/password/service credentials?
	char packetserver_staticIPstr_information[256];
	DOUBLE packetserver_delay; //Delay for the packet server until doing something!
	uint_32 packetserver_packetpos; //Current pos of sending said packet!
	byte packetserver_packetack;
	sword connectionid; //The used connection!
	byte used; //Used client record?
	//Connection for PPP connections!
	PPPOE_PAD_PACKETBUFFER pppoe_discovery_PADI; //PADI(Sent)!
	PPPOE_PAD_PACKETBUFFER pppoe_discovery_PADO; //PADO(Received)!
	PPPOE_PAD_PACKETBUFFER pppoe_discovery_PADR; //PADR(Sent)!
	PPPOE_PAD_PACKETBUFFER pppoe_discovery_PADS; //PADS(Received)!
	PPPOE_PAD_PACKETBUFFER pppoe_discovery_PADT; //PADT(Send final)!
	//Disconnect clears all of the above packets(frees them if set) when receiving/sending a PADT packet!
	byte pppoe_lastsentbytewasEND; //Last sent byte was END!
	byte pppoe_lastrecvbytewasEND; //Last received byte was END!
} PacketServer_client;

PacketServer_client Packetserver_clients[0x100]; //Up to 100 clients!
word Packetserver_availableClients = 0; //How many clients are available?
word Packetserver_totalClients = 0; //How many clients are available?

//How much to delay before sending a message while authenticating?
#define PACKETSERVER_MESSAGE_DELAY 10000000.0
//How much to delay before starting the SLIP service?
#define PACKETSERVER_SLIP_DELAY 300000000.0

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
//SLIP: Delaying before starting the SLIP mode!
#define PACKETSTAGE_SLIPDELAY 9
//SLIP: Transferring SLIP data
#define PACKETSTAGE_PACKETS 10
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

//PPP reserved values
//End of frame byte
#define PPP_END 0x7E
//Escape
#define PPP_ESC 0x7D
//Escaped value encoding and decoding
#define PPP_ENCODEESC(val) (val^0x20)
#define PPP_DECODEESC(val) (val^0x20)

//Define below to encode/decode the PPP packets sent/received from the user using the PPP_ESC values
//#define PPPOE_ENCODEDECODE

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

//Normal modem operations!
#define MODEM_BUFFERSIZE 256

//Server polling speed
#define MODEM_SERVERPOLLFREQUENCY 1000
//Data tranfer frequency of transferring data
#define MODEM_DATATRANSFERFREQUENCY 57600
//Data transfer frequency of tranferring data, in the numeric result code of the connection numeric result code! Must match the MODEM_DATATRANSFERFREQUENCY
#define MODEM_DATATRANSFERFREQUENCY_NR 18
//Command completion timeout after receiving a carriage return during a command!
#define MODEM_COMMANDCOMPLETIONTIMEOUT (DOUBLE)((1000000000.0/57600.0)*5760.0)

struct
{
	byte supported; //Are we supported?
	FIFOBUFFER *inputbuffer; //The input buffer!
	FIFOBUFFER *inputdatabuffer[0x100]; //The input buffer, data mode only!
	FIFOBUFFER *outputbuffer[0x100]; //The output buffer!
	byte datamode; //1=Data mode, 0=Command mode!
	byte connected; //Are we connected?
	word connectionport; //What port to connect to by default?
	byte previousATCommand[256]; //Copy of the command for use with "A/" command!
	byte ATcommand[256]; //AT command in uppercase when started!
	byte ATcommandoriginalcase[256]; //AT command in original unmodified case!
	word ATcommandsize; //The amount of data sent!
	byte escaping; //Are we trying to escape?
	DOUBLE timer; //A timer for detecting timeout!
	DOUBLE ringtimer; //Ringing timer!
	DOUBLE serverpolltimer; //Network connection request timer!
	DOUBLE networkdatatimer; //Network connection request timer!

	DOUBLE serverpolltick; //How long it takes!
	DOUBLE networkpolltick;
	DOUBLE detectiontimer[2]; //For autodetection!
	DOUBLE RTSlineDelay; //Delay line on the CTS!
	DOUBLE effectiveRTSlineDelay; //Effective CTS line delay to use!
	DOUBLE DTRlineDelay; //Delay line on the DSR!
	DOUBLE effectiveDTRlineDelay; //Effective DSR line delay to use!

	byte TxDisMark; //Is TxD currently in mark state?

	//Various parameters used!
	byte communicationstandard; //What communication standard! B command!
	byte echomode; //Echo everything back to use user? E command!
	byte offhook; //1: Off hook(disconnected), 2=Off hook(connected), otherwise on-hook(disconnected)! H command!
	byte verbosemode; //Verbose mode: 0=Numeric result codes, 1=Text result codes, 2=Quiet mode(no response). Bit 0=V command, Bits 1-2=Q command
	byte speakervolume; //Speaker volume! L command!
	byte speakercontrol; //0=Always off, 1=On until carrier detected, 2=Always on, 3=On only while answering! M command!
	byte callprogressmethod; //Call progress method! X command!
	byte lastnumber[256]; //Last-dialed number!
	byte currentregister; //What register is selected?
	byte registers[256]; //All possible registers!
	byte flowcontrol; //&K command! See below for an explanation!
	/*
	0=Blind dial and no busy detect. CONNECT message when established.
	1=Blind dial and no busy detect. Connection speed in BPS added to CONNECT string.
	2=Dial tone detection, but no busy detection. Connection speed in BPS added to the CONNECT string.
	3=Blind dial, but busy detection. Connection speed in BPS appended to the CONNECT string.
	4=Dial tone detection and busy tone detection. Connection speed in BPS appended to the CONNECT string.
	*/
	byte communicationsmode; //Communications mode, default=5! &Q command!

	//Active status emulated for the modem!
	byte ringing; //Are we ringing?
	byte DTROffResponse; //Default: full reset! &D command!
	byte DSRisConnectionEstablished; //Default: assert high always! &S command!
	byte DCDisCarrier; //&C command!
	byte CTSAlwaysActive; //Default: always active! &R command!

	//Various characters that can be sent, set by the modem's respective registers!
	byte escapecharacter;
	byte carriagereturncharacter;
	byte linefeedcharacter;
	byte backspacecharacter;
	DOUBLE escapecodeguardtime;

	//Allocated UART port
	byte port; //What port are we allocated to?
	
	//Line status for the different modem lines!
	byte canrecvdata; //Can we start receiving data to the UART?
	byte linechanges; //For detecting line changes!
	byte outputline; //Raw line that's output!
	byte outputlinechanges; //For detecting line changes!
	byte effectiveline; //Effective line to actually use!
	byte effectivelinechanges; //For detecting line changes!

	//What is our connection ID, if we're connected?
	sword connectionid; //Normal connection ID for the internal modem!

	//Command completion status!
	byte wascommandcompletionecho; //Was command completion with echo!
	DOUBLE wascommandcompletionechoTimeout; //Timeout for execution anyways!
} modem;

byte readIPnumber(char **x, byte *number); //Prototype!

void initPacketServerClients()
{
	Packetserver_availableClients = Packetserver_totalClients = NUMITEMS(Packetserver_clients); //How many available clients!
}

//Supported and enabled the packet setver?
#if defined(PACKETSERVER_ENABLED)
#ifndef _WIN32
#define PCAP_OPENFLAG_PROMISCUOUS 1
#endif

uint8_t ethif=255, pcap_enabled = 0;
uint8_t dopktrecv = 0;
uint16_t rcvseg, rcvoff, hdrlen, handpkt;

#if defined(PACKETSERVER_ENABLED) && !defined(NOPCAP)
pcap_if_t *alldevs;
pcap_if_t *d;
pcap_t *adhandle;
const u_char *pktdata;
struct pcap_pkthdr *hdr;
int inum;
uint16_t curhandle = 0;
char errbuf[PCAP_ERRBUF_SIZE];
#endif
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
	memset(&Packetserver_clients, 0, sizeof(Packetserver_clients)); //Initialize the clients!
	initPacketServerClients();
	PacketServer_running = 0; //We're not using the packet server emulation, enable normal modem(we don't connect to other systems ourselves)!

#if defined(PACKETSERVER_ENABLED) && !defined(NOPCAP)
	if ((BIOS_Settings.ethernetserver_settings.ethernetcard==-1) || (BIOS_Settings.ethernetserver_settings.ethernetcard<0) || (BIOS_Settings.ethernetserver_settings.ethernetcard>255)) //No ethernet card to emulate?
	{
		return; //Disable ethernet emulation!
	}
	ethif = BIOS_Settings.ethernetserver_settings.ethernetcard; //What ethernet card to use?
#endif

	//Load MAC address!
	int values[6];

#if defined(PACKETSERVER_ENABLED) && !defined(NOPCAP)
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
#endif

	memcpy(&packetserver_sourceMAC,&maclocal,sizeof(packetserver_sourceMAC)); //Load sender MAC to become active!

	memset(&packetserver_defaultstaticIPstr, 0, sizeof(packetserver_defaultstaticIPstr));
	memset(&packetserver_defaultstaticIP, 0, sizeof(packetserver_defaultstaticIP));
	packetserver_usedefaultStaticIP = 0; //Default to unused!

#if defined(PACKETSERVER_ENABLED) && !defined(NOPCAP)
	if (safestrlen(&BIOS_Settings.ethernetserver_settings.users[0].IPaddress[0], 256) >= 12) //Valid length to convert IP addresses?
	{
		p = &BIOS_Settings.ethernetserver_settings.users[0].IPaddress[0]; //For scanning the IP!
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
							snprintf(packetserver_defaultstaticIPstr, sizeof(packetserver_defaultstaticIPstr), "%u.%u.%u.%u", IPnumbers[0], IPnumbers[1], IPnumbers[2], IPnumbers[3]); //Formulate the address!
							memcpy(&packetserver_defaultstaticIP, &IPnumbers, 4); //Set read IP!
							packetserver_usedefaultStaticIP = 1; //Static IP set!
						}
					}
				}
			}
		}
	}
#else
	memset(&maclocal, 0, sizeof(maclocal));
	memset(&packetserver_gatewayMAC, 0, sizeof(packetserver_gatewayMAC));
#endif

	dolog("ethernetcard","Receiver MAC address: %02x:%02x:%02x:%02x:%02x:%02x",maclocal[0],maclocal[1],maclocal[2],maclocal[3],maclocal[4],maclocal[5]);
	dolog("ethernetcard","Gateway MAC Address: %02x:%02x:%02x:%02x:%02x:%02x",packetserver_gatewayMAC[0],packetserver_gatewayMAC[1],packetserver_gatewayMAC[2],packetserver_gatewayMAC[3],packetserver_gatewayMAC[4],packetserver_gatewayMAC[5]); //Log loaded address!
	if (packetserver_usedefaultStaticIP) //Static IP configured?
	{
		dolog("ethernetcard","Static IP configured: %s(%02x%02x%02x%02x)",packetserver_defaultstaticIPstr,packetserver_defaultstaticIP[0],packetserver_defaultstaticIP[1],packetserver_defaultstaticIP[2],packetserver_defaultstaticIP[3]); //Log it!
	}

	for (i = 0; i < NUMITEMS(Packetserver_clients); ++i) //Initialize client data!
	{
		Packetserver_clients[i].packetserver_receivebuffer = allocfifobuffer(3, 0); //Simple receive buffer, the size of a packet byte(when encoded) to be able to buffer any packet(since any byte can be doubled)! This is 2 bytes required for SLIP, while 3 bytes for PPP(for the extra PPP_END character at the start of a first packet)
		Packetserver_clients[i].packetserver_transmitlength = 0; //We're at the start of this buffer, nothing is sent yet!
	}

	/*

	End of custom!

	*/

	i = 0; //Init!

	dolog("ethernetcard","Obtaining NIC list via libpcap...");

#if defined(PACKETSERVER_ENABLED) && !defined(NOPCAP)
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
#endif
	PacketServer_running = 1; //We're using the packet server emulation, disable normal modem(we don't connect to other systems ourselves)!
}

void fetchpackets_pcap() { //Handle any packets to process!
#if defined(PACKETSERVER_ENABLED) && !defined(NOPCAP)
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
#endif
}

void sendpkt_pcap (uint8_t *src, uint16_t len) {
#if defined(PACKETSERVER_ENABLED) && !defined(NOPCAP)
	if (pcap_enabled) //Enabled?
	{
		pcap_sendpacket (adhandle, src, len);
	}
#endif
}

void termPcap()
{
	if (net.packet)
	{
		freez((void **)&net.packet,net.pktlen,"MODEM_PACKET"); //Cleanup!
	}
	word client;
	for (client = 0; client < NUMITEMS(Packetserver_clients); ++client) //Process all clients!
	{
		if (Packetserver_clients[client].packet)
		{
			freez((void **)&Packetserver_clients[client].packet, Packetserver_clients[client].pktlen, "SERVER_PACKET"); //Cleanup!
		}
		if (Packetserver_clients[client].packetserver_receivebuffer)
		{
			free_fifobuffer(&Packetserver_clients[client].packetserver_receivebuffer); //Cleanup!
		}
		if (Packetserver_clients[client].packetserver_transmitbuffer && Packetserver_clients[client].packetserver_transmitsize) //Gotten a send buffer allocated?
		{
			freez((void **)&Packetserver_clients[client].packetserver_transmitbuffer, Packetserver_clients[client].packetserver_transmitsize, "MODEM_SENDPACKET"); //Clear the transmit buffer!
			if (Packetserver_clients[client].packetserver_transmitbuffer == NULL) Packetserver_clients[client].packetserver_transmitsize = 0; //Nothing allocated anymore!
		}
	}
#if defined(PACKETSERVER_ENABLED) && !defined(NOPCAP)
	if (pcap_enabled)
	{
		pcap_close(adhandle); //Close the capture/transmit device!
	}
#endif
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

sword allocPacketserver_client()
{
	sword i;
	if (Packetserver_availableClients == 0) return -1; //None available!
	--Packetserver_availableClients; //One taken!
	for (i = 0; i < NUMITEMS(Packetserver_clients); ++i) //Find an unused one!
	{
		if (Packetserver_clients[i].used) continue; //Take unused only!
		if (!Packetserver_clients[i].packetserver_receivebuffer) continue; //Required to receive properly!
		Packetserver_clients[i].used = 1; //We're used now!
		return i; //Give the ID!
	}
	++Packetserver_availableClients; //Couldn't allocate, discard!
	return -1; //Failed to allocate!
}

byte freePacketserver_client(sword client)
{
	if (client >= NUMITEMS(Packetserver_clients)) return 0; //Failure: invalid client!
	if (Packetserver_clients[client].used) //Used?
	{
		Packetserver_clients[client].used = 0; //Not used anymore!
		++Packetserver_availableClients; //One client became available!
		return 1; //Success!
	}
	return 0; //Failure!
}

void terminatePacketServer(sword client) //Cleanup the packet server after being disconnected!
{
	fifobuffer_clear(Packetserver_clients[client].packetserver_receivebuffer); //Clear the receive buffer!
	freez((void **)&Packetserver_clients[client].packetserver_transmitbuffer,Packetserver_clients[client].packetserver_transmitsize,"MODEM_SENDPACKET"); //Clear the transmit buffer!
	if (Packetserver_clients[client].packetserver_transmitbuffer==NULL) Packetserver_clients[client].packetserver_transmitsize = 0; //Clear!
}

void initPacketServer(sword client) //Initialize the packet server for use when connected to!
{
#if defined(PACKETSERVER_ENABLED) && !defined(NOPCAP)
	word c;
#endif
	terminatePacketServer(client); //First, make sure we're terminated properly!
	Packetserver_clients[client].packetserver_transmitsize = 1024; //Initialize transmit buffer!
	Packetserver_clients[client].packetserver_transmitbuffer = zalloc(Packetserver_clients[client].packetserver_transmitsize,"MODEM_SENDPACKET",NULL); //Initial transmit buffer!
	Packetserver_clients[client].packetserver_transmitlength = 0; //Nothing buffered yet!
	Packetserver_clients[client].packetserver_transmitstate = 0; //Initialize transmitter state to the default state!
	Packetserver_clients[client].packetserver_stage = PACKETSTAGE_INIT; //Initial state when connected.
#if defined(PACKETSERVER_ENABLED) && !defined(NOPCAP)
	for (c=0;c<NUMITEMS(BIOS_Settings.ethernetserver_settings.users);++c)
	{
		if (BIOS_Settings.ethernetserver_settings.users[c].username[0]&&BIOS_Settings.ethernetserver_settings.users[c].password[0]) //Gotten credentials?
		{
			Packetserver_clients[client].packetserver_stage = PACKETSTAGE_INIT_PASSWORD; //Initial state when connected: ask for credentials too.
			break;
		}
	}
#endif
	Packetserver_clients[client].packetserver_stage_byte = PACKETSTAGE_INITIALIZING; //Reset stage byte: uninitialized!
	if (Packetserver_clients[client].packet)
	{
		freez((void **)&Packetserver_clients[client].packet, Packetserver_clients[client].pktlen, "SERVER_PACKET"); //Release the buffered packet: we're a new client!
		Packetserver_clients[client].packet = NULL; //No packet anymore!
	}
	Packetserver_clients[client].packetserver_packetpos = 0; //No packet buffered anymore! New connections must read a new packet!
	Packetserver_clients[client].packetserver_packetack = 0; //Not acnowledged yet!
	fifobuffer_clear(modem.inputdatabuffer[client]); //Nothing is received yet!
	fifobuffer_clear(modem.outputbuffer[client]); //Nothing is sent yet!
	Packetserver_clients[client].packetserver_slipprotocol = 0; //Initialize the protocol to the default value, which is unused!
}

byte packetserver_authenticate(sword client)
{
#ifdef PACKETSERVER_ENABLED
#ifndef NOPCAP
	byte IPnumbers[4];
	word c;
	char *p;
#endif
#endif
	if ((strcmp(Packetserver_clients[client].packetserver_protocol, "slip") == 0) || (strcmp(Packetserver_clients[client].packetserver_protocol, "ethernetslip") == 0) || (strcmp(Packetserver_clients[client].packetserver_protocol, "ipxslip") == 0)  || (strcmp(Packetserver_clients[client].packetserver_protocol, "ppp") == 0)) //Valid protocol?
	{
#ifdef PACKETSERVER_ENABLED
#ifndef NOPCAP
		if (!(BIOS_Settings.ethernetserver_settings.users[0].username[0] && BIOS_Settings.ethernetserver_settings.users[0].password[0])) //Gotten no default credentials?
		{
			safestrcpy(Packetserver_clients[client].packetserver_staticIPstr, sizeof(Packetserver_clients[client].packetserver_staticIPstr), packetserver_defaultstaticIPstr); //Default!
			memcpy(&Packetserver_clients[client].packetserver_staticIP, &packetserver_defaultstaticIP, 4); //Set read IP!
			Packetserver_clients[client].packetserver_useStaticIP = packetserver_usedefaultStaticIP; //Static IP set!
			return 1; //Always valid: no credentials required!
		}
		else
		{
			for (c = 0; c < NUMITEMS(BIOS_Settings.ethernetserver_settings.users); ++c) //Check all users!
			{
				if (!(BIOS_Settings.ethernetserver_settings.users[c].username[c] && BIOS_Settings.ethernetserver_settings.users[c].password[c])) //Gotten no credentials?
					continue;
				if (!(strcmp(BIOS_Settings.ethernetserver_settings.users[c].username, Packetserver_clients[client].packetserver_username) || strcmp(BIOS_Settings.ethernetserver_settings.users[c].password, Packetserver_clients[client].packetserver_password))) //Gotten no credentials?
				{
					//Determine the IP address!
					memcpy(&Packetserver_clients[client].packetserver_staticIP, &packetserver_defaultstaticIP, sizeof(Packetserver_clients[client].packetserver_staticIP)); //Use the default IP!
					safestrcpy(Packetserver_clients[client].packetserver_staticIPstr, sizeof(Packetserver_clients[client].packetserver_staticIPstr), packetserver_defaultstaticIPstr); //Formulate the address!
					Packetserver_clients[client].packetserver_useStaticIP = 0; //Default: not detected!
					if (safestrlen(&BIOS_Settings.ethernetserver_settings.users[c].IPaddress[0], 256) >= 12) //Valid length to convert IP addresses?
					{
						p = &BIOS_Settings.ethernetserver_settings.users[c].IPaddress[0]; //For scanning the IP!

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
											snprintf(Packetserver_clients[client].packetserver_staticIPstr, sizeof(Packetserver_clients[client].packetserver_staticIPstr), "%u.%u.%u.%u", IPnumbers[0], IPnumbers[1], IPnumbers[2], IPnumbers[3]); //Formulate the address!
											memcpy(&Packetserver_clients[client].packetserver_staticIP, &IPnumbers, 4); //Set read IP!
											Packetserver_clients[client].packetserver_useStaticIP = 1; //Static IP set!
										}
									}
								}
							}
						}
					}
					if (!Packetserver_clients[client].packetserver_useStaticIP) //Not specified? Use default!
					{
						safestrcpy(Packetserver_clients[client].packetserver_staticIPstr, sizeof(Packetserver_clients[client].packetserver_staticIPstr), packetserver_defaultstaticIPstr); //Default!
						memcpy(&Packetserver_clients[client].packetserver_staticIP, &packetserver_defaultstaticIP, 4); //Set read IP!
						Packetserver_clients[client].packetserver_useStaticIP = packetserver_usedefaultStaticIP; //Static IP set!
					}
					return 1; //Valid credentials!
				}
			}
		}
#else
		return 1; //Valid credentials!
#endif
#endif
	}
	return 0; //Invalid credentials!
}

byte ATresultsString[6][256] = {"ERROR","OK","CONNECT","RING","NO DIALTONE","NO CARRIER"}; //All possible results!
byte ATresultsCode[6] = {4,0,1,2,6,3}; //Code version!
#define MODEMRESULT_ERROR 0
#define MODEMRESULT_OK 1
#define MODEMRESULT_CONNECT 2
#define MODEMRESULT_RING 3
#define MODEMRESULT_NODIALTONE 4
#define MODEMRESULT_NOCARRIER 5

//usecarriagereturn: bit0=before, bit1=after, bit2=use linefeed
void modem_responseString(byte *s, byte usecarriagereturn)
{
	word i, lengthtosend;
	lengthtosend = (word)safestrlen((char *)s,256); //How long to send!
	if (usecarriagereturn&1)
	{
		writefifobuffer(modem.inputbuffer,modem.carriagereturncharacter); //Termination character!
		if (usecarriagereturn&4) writefifobuffer(modem.inputbuffer,modem.linefeedcharacter); //Termination character!
	}
	for (i=0;i<lengthtosend;) //Process all data to send!
	{
		writefifobuffer(modem.inputbuffer,s[i++]); //Send the character!
	}
	if (usecarriagereturn&2)
	{
		writefifobuffer(modem.inputbuffer,modem.carriagereturncharacter); //Termination character!
		if (usecarriagereturn&4) writefifobuffer(modem.inputbuffer,modem.linefeedcharacter); //Termination character!
	}
}
void modem_nrcpy(char *s, word size, word nr)
{
	memset(s,0,size);
	snprintf(s,size,"%u",nr); //Convert to string!
}
char connectionspeed[256]; //Connection speed!
void modem_responseResult(byte result) //What result to give!
{
	byte s[256];
	if (result>=MIN(NUMITEMS(ATresultsString),NUMITEMS(ATresultsCode))) //Out of range of results to give?
	{
		result = MODEMRESULT_ERROR; //Error!
	}
	if ((modem.verbosemode & 6)==2) //All off?
	{
		return; //Quiet mode? No response messages!
	}
	if ((modem.verbosemode & 6) == 4) //No ring and connect/carrier?
	{
		if ((result == MODEMRESULT_RING) || (result == MODEMRESULT_CONNECT) || (result == MODEMRESULT_NOCARRIER)) //Don't send these when sending results?
		{
			return; //Don't send these results!
		}
	}

	//Now, the results can have different formats:
	/*
	- V0 information text: text<CR><LF>
	- V0 numeric code: code<CR>
	- V1 information text: <CR><LF>text<CR><LF>
	- V1 numeric code: <CR><LF>verbose code<CR><LF>
	*/

	if (modem.verbosemode&1) //Text format result?
	{
		modem_responseString(&ATresultsString[result][0],(((result!=MODEMRESULT_CONNECT) || (modem.callprogressmethod==0))?3:1)|4); //Send the string to the user!
		if ((result == MODEMRESULT_CONNECT) && modem.callprogressmethod) //Add speed as well?
		{
			memset(&connectionspeed,0,sizeof(connectionspeed)); //Init!
			safestrcpy(connectionspeed, sizeof(connectionspeed), " "); //Init!
			safescatnprintf(connectionspeed, sizeof(connectionspeed), "%u", (uint_32)MODEM_DATATRANSFERFREQUENCY); //Add the data transfer frequency!
			modem_responseString((byte *)&connectionspeed[0], (2 | 4)); //End the command properly with a speed indication in bps!
		}
	}
	else //Numeric format result? This is V0 beign active! So just CR after!
	{
		if ((result == MODEMRESULT_CONNECT) && modem.callprogressmethod) //Add speed as well?
		{
			modem_nrcpy((char*)&s[0], sizeof(s), MODEM_DATATRANSFERFREQUENCY_NR); //Report 57600!
		}
		else //Normal result code?
		{
			modem_nrcpy((char*)&s[0], sizeof(s), ATresultsCode[result]);
		}
		modem_responseString(&s[0],((2)));
	}
}

void modem_responseNumber(byte x)
{
	char s[256];
	/*
	- V0 information text: text<CR><LF>
	-> V0 numeric code: code<CR>
	- V1 information text: <CR><LF>text<CR><LF>
	-> V1 numeric code: <CR><LF>verbose code<CR><LF>
	*/
	if (modem.verbosemode&1) //Text format result?
	{
		memset(&s,0,sizeof(s));
		snprintf(s,sizeof(s),"%u",x); //Convert to a string!
		modem_responseString((byte *)&s,(1|2|4)); //Send the string to the user! CRLF before and after!
	}
	else
	{
		modem_nrcpy((char*)&s[0], sizeof(s), x);
		modem_responseString((byte *)&s[0], (2)); //Send the numeric result to the user! CR after!
	}
}

byte modem_sendData(byte value) //Send data to the connected device!
{
	//Handle sent data!
	if (PacketServer_running) return 0; //Not OK to send data this way!
	return writefifobuffer(modem.outputbuffer[0],value); //Try to write to the output buffer!
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
	sword connectionid;
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
	if (modem.connected == 1) //Connected and dialing out?
	{
		if (TCP_DisconnectClientServer(modem.connectionid)) //Try and disconnect, if possible!
		{
			modem.connectionid = -1; //Not connected anymore if succeeded!
			fifobuffer_clear(modem.inputdatabuffer[0]); //Clear the output buffer for the next client!
			fifobuffer_clear(modem.outputbuffer[0]); //Clear the output buffer for the next client!
			modem.connected = 0; //Not connected anymore!
		}
	}
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
	if ((connectionid = TCP_ConnectClient(ipaddress,port))>=0) //Connected on the port specified(use the server port by default)?
	{
		modem.connectionid = connectionid; //We're connected to this!
		modem.connected = 1; //We're connected!
		return 1; //We're connected!
	}
	return 0; //We've failed to connect!
}

void modem_hangup() //Hang up, if possible!
{
	if (modem.connected == 1) //Connected?
	{
		TCP_DisconnectClientServer(modem.connectionid); //Try and disconnect, if possible!
		modem.connectionid = -1; //Not connected anymore
		fifobuffer_clear(modem.inputdatabuffer[0]); //Clear the output buffer for the next client!
		fifobuffer_clear(modem.outputbuffer[0]); //Clear the output buffer for the next client!
	}
	modem.connected &= ~1; //Not connected anymore!
	modem.ringing = 0; //Not ringing anymore!
	modem.offhook = 0; //We're on-hook!
	fifobuffer_clear(modem.inputdatabuffer[0]); //Clear anything we still received!
	fifobuffer_clear(modem.outputbuffer[0]); //Clear anything we still need to send!
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
					modem_sendData(modem.escapecharacter); //Send all escaped data!
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
		case 25: //DTR to DSR Delay Interval(hundredths of a second)
			#ifdef IS_LONGDOUBLE
			modem.effectiveDTRlineDelay = (modem.registers[reg] * 10000000.0L); //Set the RTC to CTS line delay, in nanoseconds!
			#else
			modem.effectiveDTRlineDelay = (modem.registers[reg] * 10000000.0); //Set the RTC to CTS line delay, in nanoseconds!
			#endif
			break;
		case 26: //RTC to CTS Delay Interval(hundredths of a second)
			#ifdef IS_LONGDOUBLE
			modem.effectiveRTSlineDelay = (modem.registers[reg] * 10000000.0L); //Set the RTC to CTS line delay, in nanoseconds!
			#else
			modem.effectiveRTSlineDelay = (modem.registers[reg] * 10000000.0); //Set the RTC to CTS line delay, in nanoseconds!
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

	/*

	According to , defaults are:
	B0: communicationstandard=0
	E1: echomode=1
	F0
	L3: speakervolume=3
	M1: speakercontrol=1
	N1
	Q0: verbosemode=(value)<<1|(verbosemode&1)
	T
	V1: verboseemode=(value)|verbosemode
	W1
	X4: callprogressmethod=4
	Y0
	&C1: DCDmodeisCarrier=1
	&D2: DTRoffRresponse=2
	&K3: flowcontrol=3
	&Q5: communicatinsmode=5
	&R1: CTSalwaysActive=1
	&S0: DSRisConnectionEstablished=0
	\A1
	\B3
	\K5
	\N3: 
	%C3
	%E2

	*/
	modem.communicationstandard = 0; //Default communication standard!
	modem.echomode = 1; //Default: echo back!
	//Speaker controls
	modem.speakervolume = 3; //Max level speaker volume!
	modem.speakercontrol = 1; //Enabled speaker!
	//Result defaults
	modem.verbosemode = 1; //Text-mode verbose!
	modem.callprogressmethod = 4;
	//Default handling of the Hardware lines is also loaded:
	modem.DCDisCarrier = 1; //Default: DCD=Set Data Carrier Detect (DCD) signal according to remote modem data carrier signal..
	modem.DTROffResponse = 2; //Default: Hang-up and Goto AT command mode?!
	modem.flowcontrol = 3; //Default: Enable RTS/CTS flow control!
	modem.communicationsmode = 5; //Default: communications mode 5 for V-series system products, &Q0 for Smartmodem products! So use &Q5 here!
	modem.CTSAlwaysActive = 1; //Default: CTS controlled by flow control!
	modem.DSRisConnectionEstablished = 0; //Default: DSR always ON!
	//Finish up the default settings!
	modem.datamode = 0; //In command mode!

	memset(&modem.lastnumber,0,sizeof(modem.lastnumber)); //No last number!
	modem.offhook = 0; //On-hook!
	if (modem.connected&1) //Are we still connected?
	{
		modem.connected &= ~1; //Disconnect!
		modem_responseResult(MODEMRESULT_NOCARRIER); //Report no carrier!
		TCP_DisconnectClientServer(modem.connectionid); //Disconnect the client!
		modem.connectionid = -1; //Not connected anymore!
		fifobuffer_clear(modem.inputdatabuffer[0]); //Clear the output buffer for the next client!
		fifobuffer_clear(modem.outputbuffer[0]); //Clear the output buffer for the next client!
	}


	//Misc data
	memset(&modem.previousATCommand,0,sizeof(modem.previousATCommand)); //No previous command!

	if (loadModemProfile(state)) //Loaded?
	{
		return 1; //OK!
	}
	return 0; //Invalid profile!
}

void MODEM_sendAutodetectionPNPmessage()
{
	//return; //We don't know what to send yet, so disable the PNP feature for now!
	//According to https://git.kontron-electronics.de/linux/linux-imx-exceet/blob/115a57c5b31ab560574fe1a09deaba2ae89e77b5/drivers/serial/8250_pnp.c , PNPC10F should be a "Standard Modem".
	//"PNPC10F"=Standard Modem. Order is(in order of escapes: Version(two 5-bits values, divided by 100 is the version number, high 5-bits first, low 5-bits second) ID("PNP"), product ID(the ID), Serial number(00000001), Class name("MODEM", as literally in the Plug and Play Exernal COM Device Specification Version 1.00 February 28, 1995), Device ID("," followed by the ID), User name("Modem", this is what's reported to the user as plain text).
	//The ID used to be "PNPC10F". Use PNPC102 for a safe Standard 28800bps modem.
	char EISA_productID[] = "PNPC107"; //Product ID! Standard modem?
	char DeviceID[] = "\\PNPC107"; //Device ID! Standard modem?
	char PNPHeader[] = "\x28\x01\x24"; //Header until EISA/product ID
	char PNPMid[] = "\\00000001\\MODEM"; //After EISA/product ID until Device ID
	char PNPFooter[] = "\\ModemCC\x29"; //Footer with checksum!
	char message[256]; //Buffer for the message to be modified into!
	memset(&message, 0, sizeof(message)); //Init the message to fill!
	word size;
	byte checksum;
	char *p, *e;
	//Start generating the checksum!
	checksum = 0; //Init checksum!
	//Copy the initial buffer data over(excluding checksum)!
	safestrcat(message,sizeof(message),PNPHeader); //Copy the message over to the actual buffer!
	safestrcat(message,sizeof(message),EISA_productID); //Copy the product ID over!
	safestrcat(message,sizeof(message),PNPMid); //Copy the second part of the message to the actual buffer!
	safestrcat(message,sizeof(message),DeviceID); //Copy the device ID over!
	safestrcat(message,sizeof(message),PNPFooter); //Copy the footer over!
	size = safe_strlen(message,sizeof(message)); //Size to send! Sizeof includes a final NULL byte, which we don't want to include! Taking sizeof's position gives us the byte past the string!
	e = &message[size-1]; //End of the message buffer(when to stop processing the checksum(the end PnP character). This selects from after the start byte until before the end byte, excluding the checksum itself)!
	p = &message[1]; //Init message to calculate the checksum(a ROMmed constant) to the first used byte(the byte after the start of the )!
	message[size - 2] = 0; //Second checksum nibble isn't counted!
	message[size - 3] = 0; //First checksum nibble isn't counted!
	for (;(p!=e);) //Not finished processing the entire checksum?
	{
		checksum += *p++; //Add to the checksum(minus the actual checksum bytes)! Also copy to the active message buffer!
	}
	checksum &= 0xFF; //It's MOD 256 for all but the checksum fields itself to get the actual checksum!
	message[size - 2] = ((checksum & 0xF) > 0xA) ? (((checksum & 0xF) - 0xA) + (byte)'A') : ((checksum & 0xF) + (byte)'0'); //Convert hex digit the low nibble checksum!
	message[size - 3] = (((checksum>>4) & 0xF) > 0xA) ? ((((checksum>>4) & 0xF) - 0xA) + (byte)'A') : (((checksum>>4) & 0xF) + (byte)'0'); //Convert hex digit the high nibble checksum!

	//The PNP message is now ready to be sent to the Data Terminal!

	fifobuffer_clear(modem.inputbuffer); //Clear the input buffer for out message!
	char c;
	p = &message[0]; //Init message!
	e = &message[size]; //End of the message buffer! Don't include the terminating NULL character, so substract one to stop when reaching the NULL byte instead of directly after it!
	for (; (p!=e) && ((fifobuffer_freesize(modem.inputbuffer)>2));) //Process the message, until either finished or not enough size left!
	{
		c = *p++; //Read a character to send!
		writefifobuffer(modem.inputbuffer, c); //Write the character!
	}
	//Finally, the CR/LF combination is sent!
	writefifobuffer(modem.inputbuffer,modem.carriagereturncharacter);
	writefifobuffer(modem.inputbuffer,modem.linefeedcharacter);
}

void modem_updatelines(byte lines); //Prototype for modem_setModemControl!

void modem_setModemControl(byte line) //Set output lines of the Modem!
{
	//Handle modem specifics here!
	//0: Data Terminal Ready(we can are ready to work), 1: Request to Send(UART can receive data), 4=Set during mark state of the TxD line.
	if ((line & 0x10) == 0) //Mark not set?
	{
		//TxD isn't mark, the detection timers are stopped, as TxD is required to be mark when using the detection scheme!
		modem.detectiontimer[0] = (DOUBLE)0; //Stop timing!
		modem.detectiontimer[1] = (DOUBLE)0; //Stop timing!
	}
	modem.canrecvdata = (line&2); //Can we receive data?
	modem.TxDisMark = ((line & 0x10) >> 4); //Is TxD set to mark?
	line &= 0xF; //Ignore unused lines!
	modem.outputline = line; //The line that's output!
	if ((modem.linechanges^line)&2) //RTS changed?
	{
		modem.RTSlineDelay = modem.effectiveRTSlineDelay; //Start timing the CTS line delay!
		if (modem.RTSlineDelay) //Gotten a delay?
		{
			modem_updatelines(2 | 4); //Update RTS internally, don't acnowledge RTS to CTS yet!
		}
		else
		{
			modem_updatelines(2); //Update RTS internally, acnowledge RTS to CTS!
		}
	}
	if (((modem.linechanges^line)&1)) //DTR changed?
	{
		modem.DTRlineDelay = modem.effectiveDTRlineDelay; //Start timing the CTS line delay!
		if (modem.DTRlineDelay) //Gotten a delay?
		{
			modem_updatelines(1 | 4); //Update DTR, don't acnowledge yet!
		}
		else
		{
			modem_updatelines(1); //Update DTR, acnowledge!
		}
	}
	modem.linechanges = line; //Save for reference!
}

void modem_updatelines(byte lines)
{
	if ((lines & 4) == 0) //Update effective lines?
	{
		modem.effectiveline = ((modem.effectiveline & ~(lines & 3)) | (modem.outputline & (lines & 3))); //Apply the line(s)!
	}
	if ((((modem.effectiveline&1)==0) && ((modem.effectivelinechanges^modem.effectiveline)&1)) && ((lines&4)==0)) //Became not ready?
	{
		modem.detectiontimer[0] = (DOUBLE)0; //Stop timing!
		modem.detectiontimer[1] = (DOUBLE)0; //Stop timing!
		switch (modem.DTROffResponse) //What reponse?
		{
			case 0: //Ignore the line?
				break;
			case 3: //Reset and Hang-up?
			case 2: //Hang-up and Goto AT command mode?
				if ((modem.connected&1) || modem.ringing) //Are we connected?
				{
					modem_responseResult(MODEMRESULT_NOCARRIER); //No carrier!
					modem_hangup(); //Hang up!
				}
			case 1: //Goto AT command mode?
				modem.datamode = (byte)(modem.ATcommandsize = 0); //Starting a new command!
				if (modem.DTROffResponse==3) //Reset as well?
				{
					resetModem(0); //Reset!
				}
				break;
			default:
				break;
		}
	}
	if (((modem.outputline&1)==1) && ((modem.outputlinechanges^modem.outputline)&1) && (modem.TxDisMark)) //DTR set while TxD is mark?
	{
		modem.detectiontimer[0] = (DOUBLE)150000000.0; //Timer 150ms!
		modem.detectiontimer[1] = (DOUBLE)250000000.0; //Timer 250ms!
		//Run the RTS checks now!
	}
	if ((modem.outputline&2) && (modem.detectiontimer[0])) //RTS and T1 not expired?
	{
		modem_startidling:
		modem.detectiontimer[0] = (DOUBLE)0; //Stop timing!
		modem.detectiontimer[1] = (DOUBLE)0; //Stop timing!
		goto finishupmodemlinechanges; //Finish up!
	}
	if ((modem.outputline&2) && (!modem.detectiontimer[0]) && (modem.detectiontimer[1])) //RTS and T1 expired and T2 not expired?
	{
		//Send serial PNP message!
		MODEM_sendAutodetectionPNPmessage();
		goto modem_startidling; //Start idling again!
	}
	if ((modem.outputline&2) && (!modem.detectiontimer[1])) //RTS and T2 expired?
	{
		goto modem_startidling; //Start idling again!
	}
	finishupmodemlinechanges:
	modem.outputlinechanges = modem.outputline; //Save for reference!
	if ((lines & 4) == 0) //Apply effective line?
	{
		modem.effectivelinechanges = modem.effectiveline; //Save for reference!
	}
}

byte modem_hasData() //Do we have data for input?
{
	byte temp;
	return ((peekfifobuffer(modem.inputbuffer, &temp) || (peekfifobuffer(modem.inputdatabuffer[0],&temp) && (modem.datamode==1)))&&((modem.canrecvdata&&((modem.flowcontrol==1)||(modem.flowcontrol==3))) || ((modem.flowcontrol!=1) && (modem.flowcontrol!=3)))); //Do we have data to receive and flow control allows it?
}

byte modem_getstatus()
{
	byte result = 0;
	result = 0;
	//0: Clear to Send(Can we buffer data to be sent), 1: Data Set Ready(Not hang up, are we ready for use), 2: Ring Indicator, 3: Carrrier detect
	if (modem.communicationsmode && (modem.communicationsmode < 4)) //Synchronous mode? CTS is affected!
	{
		switch (modem.CTSAlwaysActive)
		{
		case 0: //Track RTS? V.25bis handshake!
			result |= ((modem.effectiveline >> 1) & 1); //Track RTS!
			break;
		case 1: //Depends on the buffers! Only drop when required by flow control!
			result |= ((modem.datamode == 1) ? ((modem.connectionid >= 0) ? (fifobuffer_freesize(modem.outputbuffer[modem.connectionid]) ? 1 : 0) : 1) : 1); //Can we send to the modem?
			break;
		case 2: //Always on?
			result |= 1; //Always on!
			break;
		}
	}
	else
	{
		//Hayes documentation says it doesn't control CTS and RTS functions!
		result |= ((modem.effectiveline >> 1) & 1); //Always on! &Rn has no effect according to Hayes docs! But do this anyways!
	}
	//DSRisConnectionEstablished: 0:1, 1:DTR
	if ((modem.communicationsmode) && (modem.communicationsmode < 5)) //Special actions taken?
	{
		//((modem.outputline & 1) << 1)
		switch (modem.DSRisConnectionEstablished) //What state?
		{
		default:
		case 0: //S0?
		case 1: //S1?
			//0 at command state and idle, handshake(connected) turns on, lowered when hanged up.
			if ((modem.connected == 1) && (modem.datamode != 2)) //Handshaked?
			{
				result |= 2; //Raise the line!
			}
			//Otherwise, lower the line!
			break;
		case 2: //S2?
			//0 at command state and idle, prior to handshake turns on, lowered when hanged up.
			if ((modem.connected == 1) && (modem.datamode)) //Handshaked or pending handshake?
			{
				result |= 2; //Raise the line!
			}
			//Otherwise, lower the line!
			break;
		}
	}
	else //Q0/5/6?
	{
		switch (modem.DSRisConnectionEstablished) //What state?
		{
		default:
		case 0: //S0?
			result |= 2; //Always raised!
			break;
		case 1: //S1?
			result |= ((modem.outputline & 1) << 1); //Follow handshake!
			break;
		case 2: //S2?
			result |= ((modem.outputline & 1) << 1); //Follow handshake!
			break;
		}
	}
	result |= (((modem.ringing&1)&((modem.ringing)>>1))?4:0)| //Currently Ringing?
			(((modem.connected==1)||(modem.DCDisCarrier==0))?8:0); //Connected or forced on?
	return result; //Give the resulting line status!
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
		if (readfifobuffer(modem.inputdatabuffer[0], &result))
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
	char firmwareversion[] = "UniPCemu emulated modem V1.00\0"; //Firmware version!
	char hardwareinformation[] = "UniPCemu Hayes - compatible modem\0"; //Hardware information!
	char tempcommand[256]; //Stripped command with spaces removed!
	char tempcommand2[256]; //Stripped original case command with spaces removed!
	int n0;
	char number[256];
	byte dialproperties=0;
	memset(&number,0,sizeof(number)); //Init number!
	byte *temp, *temp2;
	byte verbosemodepending; //Pending verbose mode!

	temp = &modem.ATcommand[0]; //Parse the entire string!
	temp2 = &modem.ATcommandoriginalcase[0]; //Original case backup!
	for (; *temp;)
	{
		*temp2 = *temp; //Original case backup!
		*temp = (byte)toupper((int)*temp); //Convert to upper case!
		++temp; //Next character!
		++temp2; //Next character!
	}
	*temp2 = (char)0; //End of string!

	//Read and execute the AT command, if it's valid!
	if (strcmp((char *)&modem.ATcommand[0],"A/")==0) //Repeat last command?
	{
		memcpy(&modem.ATcommand,modem.previousATCommand,sizeof(modem.ATcommand)); //Last command again!
		//Re-case the command!
		temp = &modem.ATcommand[0]; //Parse the entire string!
		temp2 = &modem.ATcommandoriginalcase[0]; //Original case backup!
		for (; *temp;)
		{
			*temp2 = *temp; //Original case backup!
			*temp = (byte)toupper((int)*temp); //Convert to upper case!
			++temp; //Next character!
			++temp2; //Next character!
		}
		*temp2 = 0; //End of string!
		modem.detectiontimer[0] = (DOUBLE)0; //Stop timing!
		modem.detectiontimer[1] = (DOUBLE)0; //Stop timing!
	}

	//Check for a command to send!
	//Parse the AT command!

	if (modem.ATcommand[0]==0) //Empty line? Stop dialing and perform autoanswer!
	{
		modem.detectiontimer[0] = (DOUBLE)0; //Stop timing!
		modem.detectiontimer[1] = (DOUBLE)0; //Stop timing!
		return;
	}

	if (!((modem.ATcommand[0] == 'A') && (modem.ATcommand[1] == 'T'))) //Invalid header to the command?
	{
		modem_responseResult(MODEMRESULT_ERROR); //Error!
		return; //Abort!
	}

	if (modem.ATcommand[2] == 0) //Empty AT command? Just an "AT\r" command!
	{
		//Stop dialing and perform autoanswer!
		modem.registers[0] = 0; //Stop autoanswer!
		//Dialing doesn't need to stop, as it's instantaneous!
	}

	modem.detectiontimer[0] = (DOUBLE)0; //Stop timing!
	modem.detectiontimer[1] = (DOUBLE)0; //Stop timing!
	memcpy(&modem.previousATCommand,&modem.ATcommandoriginalcase,sizeof(modem.ATcommandoriginalcase)); //Save the command for later use!
	verbosemodepending = modem.verbosemode; //Save the old verbose mode, to detect and apply changes after the command is successfully completed!
	word pos=2,posbackup; //Position to read!
	byte SETGET = 0;
	word dialnumbers = 0;
	word temppos;
	char *c = &BIOS_Settings.phonebook[0][0]; //Phone book support

	memcpy(&tempcommand, &modem.ATcommand, MIN(sizeof(modem.ATcommand),sizeof(tempcommand))); //Make a copy of the current AT command for stripping!
	memcpy(&tempcommand2, &modem.ATcommandoriginalcase, MIN(sizeof(modem.ATcommandoriginalcase), sizeof(tempcommand2))); //Make a copy of the current AT command for stripping!
	memset(&modem.ATcommand, 0, sizeof(modem.ATcommand)); //Clear the command for the stripped version!
	memset(&modem.ATcommandoriginalcase, 0, sizeof(modem.ATcommandoriginalcase)); //Clear the command for the stripped version!
	posbackup = safe_strlen(tempcommand, sizeof(tempcommand)); //Store the length for fast comparison!
	for (pos = 0; pos < posbackup; ++pos) //We're stripping spaces!
	{
		if (tempcommand[pos] != ' ') //Not a space(which is ignored)? Linefeed is taken as is and errors out when encountered!
		{
			safescatnprintf((char *)&modem.ATcommand[0], sizeof(modem.ATcommand), "%c", tempcommand[pos]); //Add the valid character to the command!
		}
		if (tempcommand2[pos] != ' ') //Not a space(which is ignored)? Linefeed is taken as is and errors out when encountered!
		{
			safescatnprintf((char*)&modem.ATcommandoriginalcase[0], sizeof(modem.ATcommandoriginalcase), "%c", tempcommand2[pos]); //Add the valid character to the command!
		}
	}
	pos = 2; //Reset the position to the end of the AT identifier for the processing of the command!
	for (;;) //Parse the command!
	{
		switch (modem.ATcommand[pos++]) //What command?
		{
		case 0: //EOS? OK!
			modem_responseResult(MODEMRESULT_OK); //OK
			modem.verbosemode = verbosemodepending; //New verbose mode, if set!
			return; //Finished processing the command!
		case 'E': //Select local echo?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATE;
			default: //Unknown values are next commands and assume 0!
			case 0:
				--pos; //Next command!
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
			}
			break;
		case 'N': //Automode negotiation?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATN;
			default: //Unknown values are next commands and assume 0!
			case 0:
				--pos; //Next command!
			case '0': //Off?
				n0 = 0;
			doATN:
				if (n0 < 2) //OK?
				{
					//Not handled!
				}
				else
				{
					modem_responseResult(MODEMRESULT_ERROR); //Error!
					return; //Abort!
				}
				break;
			}
			break;
		case 'D': //Dial?
			do_ATD: //Phonebook ATD!
			switch (modem.ATcommandoriginalcase[pos++]) //What dial command?
			{
			case 'L':
				memcpy(&number,&modem.lastnumber,(safestrlen((char *)&modem.lastnumber[0],sizeof(modem.lastnumber))+1)); //Set the new number to roll!
				goto actondial;
			case 'A': //Reverse to answer mode after dialing?
				goto unsupporteddial; //Unsupported for now!
				dialproperties = 1; //Reverse to answer mode!
				goto actondial;
			case ';': //Remain in command mode after dialing
				dialproperties = 2; //Remain in command mode!
				goto dodial_tone;
			case ',': //Pause for the time specified in register S8(usually 2 seconds)
			case '!': //Flash-Switch hook (Hang up for half a second, as in transferring a call)
				goto unsupporteddial;
			case 0: //EOS?
				--pos; //Next command!
			case 'T': //Tone dial?
			case 'P': //Pulse dial?
			case 'W': //Wait for second dial tone?
			case '@': //Wait for up to	30 seconds for one or more ringbacks
			dodial_tone: //Perform a tone dial!
				//Scan for a remain in command mode modifier!
				for (temppos = pos; temppos < safe_strlen((char *)&modem.ATcommand[0], sizeof(modem.ATcommand)); ++temppos) //Scan the string!
				{
					switch (modem.ATcommand[temppos]) //Check for modifiers in the connection string!
					{
					case ';': //Remain in command mode after dialing
						dialproperties = 2; //Remain in command mode!
						break;
					case ',': //Pause for the time specified in register S8(usually 2 seconds)
					case '!': //Flash-Switch hook (Hang up for half a second, as in transferring a call)
						goto unsupporteddial;
					}
				}
				safestrcpy((char *)&number[0],sizeof(number),(char *)&modem.ATcommandoriginalcase[pos]); //Set the number to dial, in the original case!
				if (safestrlen((char *)&number[0],sizeof(number)) < 2 && number[0]) //Maybe a phone book entry? This is for easy compatiblity for quick dial functionality on unsupported software!
				{
					posbackup = pos; //Save the position!
					if (modemcommand_readNumber(&pos, &n0)) //Read a phonebook entry?
					{
						if (modem.ATcommand[pos] == '\0') //End of string? We're a quick dial!
						{
							if (n0 < 10) //Valid quick dial?
							{
								if (dialnumbers&(1<<n0)) goto badnumber; //Prevent looping!
								goto handleQuickDial; //Handle the quick dial number!
							}
							else //Not a valid quick dial?
							{
								badnumber: //Infinite recursive dictionary detected!
								pos = posbackup; //Return to where we were! It's a normal phonenumber!
							}
						}
						else
						{
							pos = posbackup; //Return to where we were! It's a normal phonenumber!
						}
					}
					else
					{
						pos = posbackup; //Return to where we were! It's a normal phonenumber!
					}
				}
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
			case 'S': //Dial phonebook?
				posbackup = pos; //Save for returning later!
				if (modemcommand_readNumber(&pos, &n0)) //Read the number?
				{
					handleQuickDial: //Handle a quick dial!
					pos = posbackup; //Reverse to the dial command!
					--pos; //Return to the dial command!
					if (n0 > NUMITEMS(BIOS_Settings.phonebook)) goto invalidPhonebookNumberDial;
					snprintf((char *)&modem.ATcommand[pos], sizeof(modem.ATcommand) - pos, "%s",(char *)&BIOS_Settings.phonebook[n0]); //Select the phonebook entry based on the number to dial!
					snprintf((char*)&modem.ATcommandoriginalcase[pos], sizeof(modem.ATcommand) - pos, "%s", (char*)&BIOS_Settings.phonebook[n0]); //Select the phonebook entry based on the number to dial!
					if (dialnumbers & (1 << n0)) goto loopingPhonebookNumberDial; //Prevent looping of phonenumbers being quick dialed through the phonebook or through a single-digit phonebook shortcut!
					dialnumbers |= (1 << n0); //Handling noninfinite! Prevent dialing of this entry when quick dialed throuh any method!
					goto do_ATD; //Retry with the new command!
				loopingPhonebookNumberDial: //Loop detected?
					modem_responseResult(MODEMRESULT_NOCARRIER); //No carrier!
					return; //Abort!
				invalidPhonebookNumberDial: //Dialing invalid number?
					modem_responseResult(MODEMRESULT_ERROR);
					return; //Abort!
				}
				else
				{
					modem_responseResult(MODEMRESULT_ERROR);
					return; //Abort!
				}
				break;

			default: //Unsupported?
				--pos; //Retry analyzing!
				goto dodial_tone; //Perform a tone dial on this!
			unsupporteddial: //Unsupported dial function?
				modem_responseResult(MODEMRESULT_ERROR); //Error!
				return; //Abort!
				break;
			}
			break; //Dial?
		case 'A': //Answer?
			switch (modem.ATcommand[pos++]) //What type?
			{
			default: //Unknown values are next commands and assume 0!
			case 0: //EOS?
				--pos; //Next command!
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
			}
			break;
		case 'Q': //Quiet mode?
			switch (modem.ATcommand[pos++]) //What type?
			{
			default: //Unknown values are next commands and assume 0!
			case 0: //Assume 0!
				--pos; //Next command!
			case '0': //Answer? All on!
				n0 = 0;
				goto doATQ;
			case '1': //All off!
				n0 = 1;
				goto doATQ;
			case '2': //No ring and no Connect/Carrier in answer mode?
				n0 = 2;
				doATQ:
				if (n0<3)
				{
					verbosemodepending = (n0<<1)|(verbosemodepending&1); //Quiet mode!
				}
				else
				{
					modem_responseResult(MODEMRESULT_ERROR); //ERROR!
					return; //Abort!
				}
				break;
			}
			break;
		case 'H': //Select communication standard?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATH;
			default: //Unknown values are next commands and assume 0!
			case 0:
				--pos; //Next command!
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
			}
			break;
		case 'B': //Select communication standard?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATB;
			default: //Unknown values are next commands and assume 0!
			case 0:
				--pos; //Next command!
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
			default: //Unknown values are next commands and assume 0!
			case 0:
				--pos; //Next command!
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
			default: //Unknown values are next commands and assume 0!
			case 0:
				--pos; //Next command!
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
			}
			break;
		case 'V': //Verbose mode?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATV;
			default: //Unknown values are next commands and assume 0!
			case 0:
				--pos; //Nerxt command!
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
			default: //Unknown values are next commands and assume 0!
			case 0:
				--pos; //Next command!
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
			}
			break;
		case 'Z': //Reset modem?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATZ;
			default: //Unknown values are next commands and assume 0!
			case 0: //EOS?
				--pos; //Next command!
			case '0':
				n0 = 0;
				doATZ:
				if (n0<2) //OK and supported by our emulation?
				{
					if (resetModem(n0)) //Reset to the given state!
					{
						//Do nothing when succeeded! Give OK if no other errors occur!
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
			}
			break;
		case 'T': //Tone dial?
		case 'P': //Pulse dial?
			break; //Ignore!
		case 'I': //Inquiry, Information, or Interrogation?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATI;
			case '2':
				n0 = 2;
				goto doATI;
			case '3':
				n0 = 3;
				goto doATI;
			case '4':
				n0 = 4;
				goto doATI;
			case '5':
				n0 = 5;
				goto doATI;
			case '6':
				n0 = 6;
				goto doATI;
			case '7':
				n0 = 7;
				goto doATI;
			case '8':
				n0 = 8;
				goto doATI;
			default: //Unknown values are next commands and assume 0!
			case 0:
				--pos; //Next command!
			case '0':
				n0 = 0;
				doATI:
				if (n0<5) //OK?
				{
					switch (n0) //What request?
					{
					case 3: //Firmware version!
						modem_responseString((byte *)&firmwareversion[0], (1 | 2 | 4)); //Full response!
					case 4: //Hardware information!
						modem_responseString((byte *)&hardwareinformation[0], (1 | 2 | 4)); //Full response!
						break;
					default: //Unknown!
						//Just respond with a basic OK!
						break;
					}
				}
				else
				{
					modem_responseResult(MODEMRESULT_ERROR); //Error: line not defined!
					return; //Abort!
				}
				break;
			}
			break;
		case 'O': //Return online?
			switch (modem.ATcommand[pos++]) //What type?
			{
			case '1':
				n0 = 1;
				goto doATO;
			default: //Unknown values are next commands and assume 0!
			case 0:
				--pos; //Next command!
			case '0':
				n0 = 0;
				doATO:
				if (modem.connected & 1) //Connected?
				{
					modem.datamode = 1; //Return to data mode, no result code!
				}
				else
				{
					modem_responseResult(MODEMRESULT_ERROR);
					return; //Abort!
				}
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
				modem_responseResult(MODEMRESULT_ERROR); //Error!
				return; //Abort command parsing!
			case 'Q': //Communications mode option?
				switch (modem.ATcommand[pos++]) //What flow control?
				{
				default: //Unknown values are next commands and assume 0!
				case 0:
					--pos; //Next command!
				case '0':
					n0 = 0; //
					goto setAT_EQ;
				case '1':
					n0 = 1; //
					goto setAT_EQ;
				case '2':
					n0 = 2; //
					goto setAT_EQ;
				case '3':
					n0 = 3; //
					goto setAT_EQ;
				case '4':
					n0 = 4; //
					goto setAT_EQ;
				case '5':
					n0 = 5; //
					goto setAT_EQ;
				case '6':
					n0 = 6; //
				setAT_EQ:
					if (n0 < 7) //Valid?
					{
						modem.communicationsmode = n0; //Set communications mode!
					}
					else
					{
						modem_responseResult(MODEMRESULT_ERROR); //Error!
						return; //Abort!
					}
					break;
				}
				break;
			case 'R': //Force CTS high option?
				switch (modem.ATcommand[pos++]) //What flow control?
				{
				default: //Unknown values are next commands and assume 0!
					--pos;
				case '0':
					n0 = 0; //Modem turns on the Clear To Send signal when it detects the Request To Send (RTS) signal from host.
					goto setAT_R;
				case '1':
					n0 = 1; //Modem ignores the Request To Send signal and turns on its Clear To Send signal when ready to receive data.
					goto setAT_R;
				case '2':
					n0 = 2; // *Clear To Send force on.
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
				default: //Unknown values are next commands and assume 0!
					--pos;
				case '0':
					n0 = 0; // Keep Data Carrier Detect (DCD) signal always ON.
					goto setAT_C;
				case '1':
					n0 = 1; // * Set Data Carrier Detect (DCD) signal according to remote modem data carrier signal.
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
				default: //Unknown values are next commands and assume 0!
					--pos;
				case '0':
					n0 = 0; // * Data Set Ready is forced on
					goto setAT_S;
				case '1':
					n0 = 1; // Data Set Ready to operate according to RS-232 specification(follow DTR)
					goto setAT_S;
				case '2':
					n0 = 2; //
				setAT_S:
					if (n0<3) //Valid?
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
				default: //Unknown values are next commands and assume 0!
					--pos;
				case '0':
					n0 = 0; //Ignore DTR line from computer
					goto setAT_D;
				case '1':
					n0 = 1; //Goto AT command state when DTR On->Off
					goto setAT_D;
				case '2':
					n0 = 2; //Hang-up and Command mode when DTR On->Off
					goto setAT_D;
				case '3':
					n0 = 3; //Full reset when DTR On->Off
					setAT_D:
					if (n0<4) //Valid?
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
				n0 = 0; //Default configuration!
				goto doATZ; //Execute ATZ!
			case 'Z': //Z special?
				n0 = 10; //Default: acnowledge!
				SETGET = 0; //Default: invalid!
				switch (modem.ATcommand[pos++]) //What flow control?
				{
				default:
				case '\0': //EOS?
					goto handlePhoneNumberEntry; //Acnowledge!
					//Ignore!
					break;
				case '0': //Set stored number?
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9': //Might be phonebook?
					n0 = (modem.ATcommand[pos - 1])-(byte)'0'; //Find the number that's to use!
					if (n0 >= NUMITEMS(BIOS_Settings.phonebook))
					{
						n0 = 10; //Invalid entry!
						goto handlePhoneNumberEntry; //Handle it!
					}
					switch (modem.ATcommand[pos++]) //SET/GET detection!
					{
					case '?': //GET?
						SETGET = 1; //GET!
						goto handlePhoneNumberEntry;
						break;
					case '=': //SET?
						SETGET = 2; //SET!
						goto handlePhoneNumberEntry;
						break;
					default: //Invalid command!
						n0 = 10; //Simple acnowledge!
						goto handlePhoneNumberEntry;
						break;
					}
					break;

					handlePhoneNumberEntry: //Handle a phone number dictionary entry!
					if (n0<NUMITEMS(BIOS_Settings.phonebook)) //Valid?
					{
						switch (SETGET) //What kind of set/get?
						{
						case 1: //GET?
							modem_responseString((byte *)&BIOS_Settings.phonebook[n0], 1|2|4); //Give the phonenumber!
							break;
						case 2: //SET?
							memset(&BIOS_Settings.phonebook[n0], 0, sizeof(BIOS_Settings.phonebook[0])); //Init the phonebook entry!
							c = (char *)&modem.ATcommandoriginalcase[pos]; //What phonebook value to set!
							safestrcpy(BIOS_Settings.phonebook[n0], sizeof(BIOS_Settings.phonebook[0]), c); //Set the phonebook entry!
							break;
						default:
							goto ignorePhonebookSETGET;
							break;
						}
					}
					else
					{
						ignorePhonebookSETGET:
						modem_responseResult(MODEMRESULT_ERROR); //Error: invalid phonebook entry or command!
						return; //Abort!
					}
					break;
				}
				break;

			case 'K': //Flow control?
				switch (modem.ATcommand[pos++]) //What flow control?
				{
				default: //Unknown values are next commands and assume 0!
					--pos;
				case '0':
					n0 = 0;
					goto setAT_K;
				case '1':
					goto unsupportedflowcontrol; //Unsupported!
					n0 = 1;
					goto setAT_K;
				case '2':
					goto unsupportedflowcontrol; //Unsupported!
					n0 = 2;
					goto setAT_K;
				case '3':
					n0 = 3;
					goto setAT_K;
				case '4':
					goto unsupportedflowcontrol; //Unsupported!
					n0 = 4;
					setAT_K:
					if (n0<5) //Valid?
					{
						modem.flowcontrol = n0; //Set flow control!
					}
					else
					{
						unsupportedflowcontrol:
						modem_responseResult(MODEMRESULT_ERROR); //Error!
						return; //Abort!
					}
					break;
				}
				break;
			default: //Invalid extension?
				--pos; //Retry analyzing!
				modem_responseResult(MODEMRESULT_ERROR); //Invalid extension!
				return;
				break;
			}
			break;
		case '\\': //Extension 2?
			switch (modem.ATcommand[pos++])
			{
			case 0: //EOS?
				modem_responseResult(MODEMRESULT_ERROR); //Let us handle it!
				return; //Abort processing!
			case 'N': //Flow control?
				switch (modem.ATcommand[pos++]) //What flow control?
				{
				default: //Unknown values are next commands and assume 0!
					--pos; //Next command!
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
					goto setAT_N;
				case '5':
					n0 = 5;
					setAT_N:
					if (n0<6) //Valid?
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
			default: //Invalid extension?
				--pos; //Retry analyzing!
				modem_responseResult(MODEMRESULT_ERROR); //Invalid extension!
				return;
			}
			break;
		default: //Unknown instruction?
			modem_responseResult(MODEMRESULT_ERROR); //Just ERROR unknown commands!
			return; //Abort!
			break;
		} //Switch!
	}
}

void modem_flushCommandCompletion()
{
	//Perform linefeed-related things!
	modem.wascommandcompletionecho = 0; //Disable the linefeed echo!
	modem.wascommandcompletionechoTimeout = (DOUBLE)0; //Stop the timeout!

	//Start execution of the currently buffered command!
	modem.ATcommand[modem.ATcommandsize] = 0; //Terminal character!
	modem.ATcommandsize = 0; //Start the new command!
	modem_executeCommand();
}

void modem_writeCommandData(byte value)
{
	if (modem.datamode) //Data mode?
	{
		modem.wascommandcompletionecho = 0; //Disable the linefeed echo!
		modem_sendData(value); //Send the data!
	}
	else //Command mode?
	{
		modem.timer = 0.0; //Reset the timer when anything is received!
		if (value == '~') //Pause stream for half a second?
		{
			modem.wascommandcompletionecho = 0; //Disable the linefeed echo!
			//Ignore this time?
			if (modem.echomode) //Echo enabled?
			{
				writefifobuffer(modem.inputbuffer, value); //Echo the value back to the terminal!
			}
		}
		else if (value == modem.backspacecharacter) //Backspace?
		{
			modem.wascommandcompletionecho = 0; //Disable the linefeed echo!
			if (modem.ATcommandsize) //Valid to backspace?
			{
				--modem.ATcommandsize; //Remove last entered value!
			}
			if (modem.echomode) //Echo enabled?
			{
				if (fifobuffer_freesize(modem.inputbuffer) >= 2) //Enough to add the proper backspace?
				{
					writefifobuffer(modem.inputbuffer, ' '); //Space to clear the character followed by...
					writefifobuffer(modem.inputbuffer, value); //Another backspace to clear it, if possible!
				}
			}
		}
		else if (value == modem.carriagereturncharacter) //Carriage return? Execute the command!
		{
			if (modem.echomode) //Echo enabled?
			{
				modem.wascommandcompletionecho = 1; //Was command completion with echo!
				writefifobuffer(modem.inputbuffer, value); //Echo the value back to the terminal!
			}
			else
			{
				modem.wascommandcompletionecho = 2; //Was command completion without echo!
			}
			handlemodemCR:
			modem.wascommandcompletionechoTimeout = MODEM_COMMANDCOMPLETIONTIMEOUT; //Start the timeout on command completion!
		}
		else if (value) //Not NULL-terminator? Command byte!
		{
			if (modem.echomode || ((modem.wascommandcompletionecho==1) && (value==modem.linefeedcharacter))) //Echo enabled and command completion with echo?
			{
				if (modem.echomode || ((modem.wascommandcompletionecho == 1) && (value == modem.linefeedcharacter))) //To echo back?
				{
					writefifobuffer(modem.inputbuffer, value); //Echo the value back to the terminal!
				}
				if ((modem.wascommandcompletionecho && (value == modem.linefeedcharacter))) //Finishing echo and start of command execution?
				{
					modem_flushCommandCompletion(); //Start executing the command now!
					return; //Don't add to the buffer!
				}
			}
			if (modem.wascommandcompletionecho) //Finishing echo and start of command execution?
			{
				modem_flushCommandCompletion(); //Start executing the command now!
			}
			modem.wascommandcompletionecho = 0; //Disable the linefeed echo from now on!
			if (modem.ATcommandsize < (sizeof(modem.ATcommand) - 1)) //Valid to input(leave 1 byte for the terminal character)?
			{
				modem.ATcommand[modem.ATcommandsize++] = value; //Add data to the string!
				if ((modem.ATcommand[0] != 'A') && (modem.ATcommand[0]!='a')) //Not a valid start?
				{
					modem.ATcommand[0] = 0;
					modem.ATcommandsize = 0; //Reset!
				}
				else if ((modem.ATcommandsize == 2) && (modem.ATcommand[1] != '/')) //Invalid repeat or possible attention(AT/at) request!
				{
					if (!( //Not either valid combination of AT or at to get the attention?
						((modem.ATcommand[1] == 'T') && (modem.ATcommand[0] == 'A')) //Same case AT?
						|| ((modem.ATcommand[1] == 't') && (modem.ATcommand[0] == 'a')) //Same case at?
						))
					{
						if ((modem.ATcommand[1] == 'A') || (modem.ATcommand[1] == 'a')) //Another start marker entered?
						{
							modem.ATcommand[0] = modem.ATcommand[1]; //Becomes the new start marker!
							--modem.ATcommandsize; //Just discard to get us back to inputting another one!
						}
						else //Invalid start marker after starting!
						{
							modem.ATcommand[0] = 0;
							modem.ATcommandsize = 0; //Reset!
						}
					}
				}
				else if ((modem.ATcommandsize == 2) && (modem.ATcommand[1] == '/')) //Doesn't need an carriage return?
				{
					if (modem.echomode) //Echo enabled?
					{
						modem.wascommandcompletionecho = 1; //Was command completion with echo!
					}
					else
					{
						modem.wascommandcompletionecho = 0; //Disable the linefeed echo!
					}
					goto handlemodemCR; //Handle the carriage return automatically, because A/ is received!
				}
			}
		}
	}
}

void modem_writeData(byte value)
{
	//Handle the data sent to the modem!
	if ((value==modem.escapecharacter) && (modem.escapecharacter<=0x7F) && ((modem.escaping && (modem.escaping<3)) || ((modem.timer>=modem.escapecodeguardtime) && (modem.escaping==0)))) //Possible escape sequence? Higher values than 127 disables the escape character! Up to 3 escapes after the guard timer is allowed!
	{
		++modem.escaping; //Increase escape info!
	}
	else //Not escaping(anymore)?
	{
		for (;modem.escaping;) //Process escape characters as data!
		{
			--modem.escaping; //Handle one!
			modem_writeCommandData(modem.escapecharacter); //Send it as data/command!
		}
		modem_writeCommandData(value); //Send it as data/command!
	}
	modem.timer = 0.0; //Reset the timer when anything is received!
}

void initModem(byte enabled) //Initialise modem!
{
	word i;
	memset(&modem, 0, sizeof(modem));
	modem.supported = enabled; //Are we to be emulated?
	if (useSERModem()) //Is this modem enabled?
	{
		modem.port = allocUARTport(); //Try to allocate a port to use!
		if (modem.port==0xFF) //Unable to allocate?
		{
			modem.supported = 0; //Unsupported!
			goto unsupportedUARTModem;
		}
		modem.connectionid = -1; //Default: not connected!
		modem.inputbuffer = allocfifobuffer(MODEM_BUFFERSIZE,0); //Small input buffer!
		initPacketServerClients(); //Prepare the clients for use!
		Packetserver_availableClients = 0; //Init: 0 clients available!
		for (i = 0; i < MIN(MIN(NUMITEMS(modem.inputdatabuffer),NUMITEMS(modem.outputbuffer)),(Packetserver_totalClients?Packetserver_totalClients:1)); ++i) //Allocate buffers for server and client purposes!
		{
			modem.inputdatabuffer[i] = allocfifobuffer(MODEM_BUFFERSIZE, 0); //Small input buffer!
			modem.outputbuffer[i] = allocfifobuffer(MODEM_BUFFERSIZE, 0); //Small input buffer!
			if (modem.inputdatabuffer[i] && modem.outputbuffer[i]) //Both allocated?
			{
				if (Packetserver_clients[i].packetserver_receivebuffer) //Packet server buffers allocated?
				{
					++Packetserver_availableClients; //One more client available!
				}
			}
			else break; //Failed to allocate? Not available client anymore!
		}
		Packetserver_totalClients = Packetserver_availableClients; //Init: n clients available in total!
		if (modem.inputbuffer && modem.inputdatabuffer[0] && modem.outputbuffer[0]) //Gotten buffers?
		{
			modem.connectionport = BIOS_Settings.modemlistenport; //Default port to connect to if unspecified!
			if (modem.connectionport==0) //Invalid?
			{
				modem.connectionport = 23; //Telnet port by default!
			}
			TCP_ConnectServer(modem.connectionport,Packetserver_availableClients?Packetserver_availableClients:1); //Connect the server on the default port!
			resetModem(0); //Reset the modem to the default state!
			#ifdef IS_LONGDOUBLE
			modem.serverpolltick = (1000000000.0L/(DOUBLE)MODEM_SERVERPOLLFREQUENCY); //Server polling rate of connections!
			modem.networkpolltick = (1000000000.0L/(DOUBLE)MODEM_DATATRANSFERFREQUENCY); //Data transfer polling rate!
			#else
			modem.serverpolltick = (1000000000.0/(DOUBLE)MODEM_SERVERPOLLFREQUENCY); //Server polling rate of connections!
			modem.networkpolltick = (1000000000.0/(DOUBLE)MODEM_DATATRANSFERFREQUENCY); //Data transfer polling rate!
			#endif
			UART_registerdevice(modem.port, &modem_setModemControl, &modem_getstatus, &modem_hasData, &modem_readData, &modem_writeData); //Register our UART device!
		}
		else
		{
			if (modem.inputbuffer) free_fifobuffer(&modem.inputbuffer);
			for (i = 0; i < NUMITEMS(modem.inputdatabuffer); ++i)
			{
				if (modem.outputbuffer[i]) free_fifobuffer(&modem.outputbuffer[i]);
				if (modem.inputdatabuffer[i]) free_fifobuffer(&modem.inputdatabuffer[i]);
			}
		}
	}
	else
	{
		unsupportedUARTModem: //Unsupported!
		modem.inputbuffer = NULL; //No buffer present!
		memset(&modem.inputdatabuffer,0,sizeof(modem.inputdatabuffer)); //No buffer present!
		memset(&modem.outputbuffer, 0, sizeof(modem.outputbuffer)); //No buffer present!
	}
}

void PPPOE_finishdiscovery(sword connectedclient); //Prototype for doneModem!

void doneModem() //Finish modem!
{
	word i;
	if (modem.inputbuffer) //Allocated?
	{
		free_fifobuffer(&modem.inputbuffer); //Free our buffer!
	}
	if (modem.outputbuffer[0] && modem.inputdatabuffer[0]) //Allocated?
	{
		for (i = 0; i < MIN(NUMITEMS(modem.inputdatabuffer), NUMITEMS(modem.outputbuffer)); ++i) //Allocate buffers for server and client purposes!
		{
			free_fifobuffer(&modem.outputbuffer[i]); //Free our buffer!
			free_fifobuffer(&modem.inputdatabuffer[i]); //Free our buffer!
		}
	}
	for (i = 0; i < NUMITEMS(Packetserver_clients); ++i) //Process all clients!
	{
		if (Packetserver_clients[i].used) //Connected?
		{
			PPPOE_finishdiscovery((sword)i); //Finish discovery, if needed!
			TCP_DisconnectClientServer(Packetserver_clients[i].connectionid); //Stop connecting!
			Packetserver_clients[i].connectionid = -1; //Unused!
			terminatePacketServer(i); //Stop the packet server, if used!
			freePacketserver_client(i); //Free the client!
		}
	}
	if (TCP_DisconnectClientServer(modem.connectionid)) //Disconnect client, if needed!
	{
		modem.connectionid = -1; //Not connected!
		//The buffers are already released!
	}
	stopTCPServer(); //Stop the TCP server!
}

void cleanModem()
{
	//Nothing to do!
}

byte packetServerAddWriteQueue(sword client, byte data) //Try to add something to the write queue!
{
	byte *newbuffer;
	if (Packetserver_clients[client].packetserver_transmitlength>= Packetserver_clients[client].packetserver_transmitsize) //We need to expand the buffer?
	{
		newbuffer = zalloc(Packetserver_clients[client].packetserver_transmitsize+1024,"MODEM_SENDPACKET",NULL); //Try to allocate a larger buffer!
		if (newbuffer) //Allocated larger buffer?
		{
			memcpy(newbuffer, Packetserver_clients[client].packetserver_transmitbuffer, Packetserver_clients[client].packetserver_transmitsize); //Copy the new data over to the larger buffer!
			freez((void **)&Packetserver_clients[client].packetserver_transmitbuffer, Packetserver_clients[client].packetserver_transmitsize,"MODEM_SENDPACKET"); //Release the old buffer!
			Packetserver_clients[client].packetserver_transmitbuffer = newbuffer; //The new buffer is the enlarged buffer, ready to have been written more data!
			Packetserver_clients[client].packetserver_transmitsize += 1024; //We've been increased to this larger buffer!
			Packetserver_clients[client].packetserver_transmitbuffer[Packetserver_clients[client].packetserver_transmitlength++] = data; //Add the data to the buffer!
			return 1; //Success!
		}
	}
	else //Normal buffer usage?
	{
		Packetserver_clients[client].packetserver_transmitbuffer[Packetserver_clients[client].packetserver_transmitlength++] = data; //Add the data to the buffer!
		return 1; //Success!
	}
	return 0; //Failed!
}

byte packetServerAddPPPDiscoveryQueue(PPPOE_PAD_PACKETBUFFER *buffer, byte data) //Try to add something to the discovery queue!
{
	byte* newbuffer;
	if (buffer->length >= buffer->size) //We need to expand the buffer?
	{
		newbuffer = zalloc(buffer->size + 1024, "MODEM_SENDPACKET", NULL); //Try to allocate a larger buffer!
		if (newbuffer) //Allocated larger buffer?
		{
			memcpy(newbuffer, buffer->buffer, buffer->size); //Copy the new data over to the larger buffer!
			freez((void **)&buffer->buffer, buffer->size, "MODEM_SENDPACKET"); //Release the old buffer!
			buffer->buffer = newbuffer; //The new buffer is the enlarged buffer, ready to have been written more data!
			buffer->size += 1024; //We've been increased to this larger buffer!
			buffer->buffer[buffer->length++] = data; //Add the data to the buffer!
			return 1; //Success!
		}
	}
	else //Normal buffer usage?
	{
		buffer->buffer[buffer->length++] = data; //Add the data to the buffer!
		return 1; //Success!
	}
	return 0; //Failed!
}

void packetServerFreePPPDiscoveryQueue(PPPOE_PAD_PACKETBUFFER *buffer)
{
	freez((void **)&buffer->buffer, buffer->size, "MODEM_SENDPACKET"); //Free it!
	buffer->size = buffer->length = 0; //No length anymore!
}

char logpacket_outbuffer[0x20001]; //Buffer for storin the data!
char logpacket_filename[256]; //For storing the raw packet that's sent!
void logpacket(byte send, byte *buffer, uint_32 size)
{
	uint_32 i;
	char adding[3];
	memset(&logpacket_filename,0,sizeof(logpacket_filename));
	memset(&logpacket_outbuffer,0,sizeof(logpacket_outbuffer));
	memset(&adding,0,sizeof(adding));
	for (i=0;i<size;++i)
	{
		snprintf(adding,sizeof(adding),"%02X",buffer[i]); //Set and ...
		safestrcat(logpacket_outbuffer,sizeof(logpacket_outbuffer),adding); //... Add!
	}
	if (send)
	{
		dolog("ethernetcard","Sending packet:");
	}
	else
	{
		dolog("ethernetcard","Receiving packet:");
	}
	dolog("ethernetcard","%s",logpacket_outbuffer); //What's received/sent!
}

void authstage_startrequest(DOUBLE timepassed, sword connectedclient, char *request, byte nextstage)
{
	if (Packetserver_clients[connectedclient].packetserver_stage_byte == PACKETSTAGE_INITIALIZING)
	{
		memset(&Packetserver_clients[connectedclient].packetserver_stage_str, 0, sizeof(Packetserver_clients[connectedclient].packetserver_stage_str));
		safestrcpy(Packetserver_clients[connectedclient].packetserver_stage_str, sizeof(Packetserver_clients[connectedclient].packetserver_stage_str), request);
		Packetserver_clients[connectedclient].packetserver_stage_byte = 0; //Init to start of string!
		Packetserver_clients[connectedclient].packetserver_credentials_invalid = 0; //No invalid field detected yet!
		Packetserver_clients[connectedclient].packetserver_delay = PACKETSERVER_MESSAGE_DELAY; //Delay this until we start transmitting!
	}
	Packetserver_clients[connectedclient].packetserver_delay -= timepassed; //Delaying!
	if ((Packetserver_clients[connectedclient].packetserver_delay <= 0.0) || (!Packetserver_clients[connectedclient].packetserver_delay)) //Finished?
	{
		Packetserver_clients[connectedclient].packetserver_delay = (DOUBLE)0; //Finish the delay!
		if (writefifobuffer(modem.outputbuffer[connectedclient], Packetserver_clients[connectedclient].packetserver_stage_str[Packetserver_clients[connectedclient].packetserver_stage_byte])) //Transmitted?
		{
			if (++Packetserver_clients[connectedclient].packetserver_stage_byte == safestrlen(Packetserver_clients[connectedclient].packetserver_stage_str, sizeof(Packetserver_clients[connectedclient].packetserver_stage_str))) //Finished?
			{
				Packetserver_clients[connectedclient].packetserver_stage_byte = PACKETSTAGE_INITIALIZING; //Prepare for next step!
				Packetserver_clients[connectedclient].packetserver_stage = nextstage;
			}
		}
	}
}

//result: 0: busy, 1: Finished, 2: goto sendoutputbuffer
byte authstage_enterfield(DOUBLE timepassed, sword connectedclient, char* field, uint_32 size, byte specialinit, char charmask)
{
	byte textinputfield = 0;
	byte isbackspace = 0;
	if (Packetserver_clients[connectedclient].packetserver_stage_byte == PACKETSTAGE_INITIALIZING)
	{
		memset(field, 0, size);
		Packetserver_clients[connectedclient].packetserver_stage_byte = 0; //Init to start filling!
		Packetserver_clients[connectedclient].packetserver_stage_byte_overflown = 0; //Not yet overflown!
		if (specialinit==1) //Special init for protocol?
		{
			#if defined(PACKETSERVER_ENABLED) && !defined(NOPCAP)
			if (!(BIOS_Settings.ethernetserver_settings.users[0].username[0] && BIOS_Settings.ethernetserver_settings.users[0].password[0])) //Gotten no credentials?
			{
				Packetserver_clients[connectedclient].packetserver_credentials_invalid = 0; //Init!
			}
			#endif
		}
	}
	if (peekfifobuffer(modem.inputdatabuffer[connectedclient], &textinputfield)) //Transmitted?
	{
		isbackspace = (textinputfield == 8) ? 1 : 0; //Are we backspace?
		if (isbackspace) //Backspace?
		{
			if (Packetserver_clients[connectedclient].packetserver_stage_byte == 0) goto ignorebackspaceoutputfield; //To ignore?
			//We're a valid backspace!
			if (fifobuffer_freesize(modem.outputbuffer[connectedclient]) < 3) //Not enough to contain backspace result?
			{
				return 2; //Not ready to process the writes!
			}
		}
		if (writefifobuffer(modem.outputbuffer[connectedclient], (isbackspace || (textinputfield == '\r') || (textinputfield == '\n') || (!charmask)) ? textinputfield : charmask)) //Echo back to user, encrypted if needed!
		{
			if (isbackspace) //Backspace requires extra data?
			{
				if (!writefifobuffer(modem.outputbuffer[connectedclient], ' ')) return 2; //Clear previous input!
				if (!writefifobuffer(modem.outputbuffer[connectedclient], textinputfield)) return 2; //Another backspace to end up where we need to be!
			}
		ignorebackspaceoutputfield: //Ignore the output part! Don't send back to the user!
			readfifobuffer(modem.inputdatabuffer[connectedclient], &textinputfield); //Discard the input!
			if ((textinputfield == '\r') || (textinputfield == '\n')) //Finished?
			{
				field[Packetserver_clients[connectedclient].packetserver_stage_byte] = '\0'; //Finish the string!
				Packetserver_clients[connectedclient].packetserver_credentials_invalid |= Packetserver_clients[connectedclient].packetserver_stage_byte_overflown; //Overflow has occurred?
				Packetserver_clients[connectedclient].packetserver_stage_byte = PACKETSTAGE_INITIALIZING; //Prepare for next step!
				return 1; //Finished!
			}
			else
			{
				if (isbackspace) //Backspace?
				{
					field[Packetserver_clients[connectedclient].packetserver_stage_byte] = '\0'; //Ending!
					if (Packetserver_clients[connectedclient].packetserver_stage_byte) //Non-empty?
					{
						--Packetserver_clients[connectedclient].packetserver_stage_byte; //Previous character!
						field[Packetserver_clients[connectedclient].packetserver_stage_byte] = '\0'; //Erase last character!
					}
				}
				else if ((textinputfield == '\0') || ((Packetserver_clients[connectedclient].packetserver_stage_byte + 1U) >= size) || Packetserver_clients[connectedclient].packetserver_stage_byte_overflown) //Future overflow, overflow already occurring or invalid input to add?
				{
					Packetserver_clients[connectedclient].packetserver_stage_byte_overflown = 1; //Overflow detected!
				}
				else //Valid content to add?
				{
					field[Packetserver_clients[connectedclient].packetserver_stage_byte++] = textinputfield; //Add input!
				}
			}
		}
	}
	return 0; //Still busy!
}

union
{
	word wval;
	byte bval[2]; //Byte of the word values!
} NETWORKVALSPLITTER;

void PPPOE_finishdiscovery(sword connectedclient)
{
	ETHERNETHEADER ethernetheader, packetheader;
	uint_32 pos; //Our packet buffer location!
	if (!(Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer && Packetserver_clients[connectedclient].pppoe_discovery_PADS.length)) //Already disconnected?
	{
		return; //No discovery to disconnect!
	}
	memcpy(&ethernetheader.data, &Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer, sizeof(ethernetheader.data)); //Make a copy of the PADS ethernet header!

	//Send the PADT packet now!
	memcpy(&packetheader.dst, &ethernetheader.src, sizeof(packetheader.dst)); //Make a copy of the ethernet destination to use!
	memcpy(&packetheader.src, &ethernetheader.dst, sizeof(packetheader.src)); //Make a copy of the ethernet source to use!
	memcpy(&packetheader.type, &ethernetheader.type, sizeof(packetheader.type)); //Make a copy of the ethernet type to use!

	packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADT); //Clear the packet!

	//First, the ethernet header!
	for (pos = 0; pos < sizeof(packetheader.data); ++pos)
	{
		packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADT, packetheader.data[pos]); //Send the header!
	}

	//Now, the PADT packet!
	packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADT); //Clear the packet!
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADT, 0x11); //V/T!
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADT, 0xA7); //PADT!
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR, Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer[sizeof(ethernetheader.data)+2]); //Session_ID first byte!
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR, Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer[sizeof(ethernetheader.data)+3]); //Session_ID second byte!
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR, 0x00); //Length first byte!
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR, 0x00); //Length second byte!
	//Now, the packet is fully ready!
	if (Packetserver_clients[connectedclient].pppoe_discovery_PADR.length != 0x14) //Packet length mismatch?
	{
		packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADT); //PADR not ready to be sent yet!
	}
	else //Send the PADR packet!
	{
		//Send the PADR packet that's buffered!
		sendpkt_pcap(Packetserver_clients[connectedclient].pppoe_discovery_PADT.buffer, Packetserver_clients[connectedclient].pppoe_discovery_PADT.length); //Send the packet to the network!
	}

	//Since we can't be using the buffers after this anyways, free them all!
	packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADI); //No PADI anymore!
	packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADO); //No PADO anymore!
	packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR); //No PADR anymore!
	packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADS); //No PADS anymore!
	packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADT); //No PADT anymore!
}

byte PPPOE_requestdiscovery(sword connectedclient)
{
	byte broadcastmac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; //Broadcast address!
	uint_32 pos; //Our packet buffer location!
	ETHERNETHEADER packetheader;
	//Now, the PADI packet!
	memcpy(&packetheader.dst, broadcastmac, sizeof(packetheader.dst)); //Broadcast it!
	memcpy(&packetheader.src, maclocal, sizeof(packetheader.src)); //Our own MAC address as the source!
	packetheader.type = SDL_SwapBE16(0x8863); //Type!
	packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADI); //Clear the packet!
	for (pos = 0; pos < sizeof(packetheader.data); ++pos)
	{
		packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADI, packetheader.data[pos]); //Send the header!
	}
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADI, 0x11); //V/T!
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADI, 0x09); //PADT!
	//Now, the contents of th packet!
	NETWORKVALSPLITTER.wval = SDL_SwapBE16(0); //Session ID!
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADI, NETWORKVALSPLITTER.bval[0]); //First byte!
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADI, NETWORKVALSPLITTER.bval[1]); //Second byte!
	NETWORKVALSPLITTER.wval = SDL_SwapBE16(0x4); //Length!
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADI, NETWORKVALSPLITTER.bval[0]); //First byte!
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADI, NETWORKVALSPLITTER.bval[1]); //Second byte!
	NETWORKVALSPLITTER.wval = SDL_SwapBE16(0x0101); //Tag type: Service-Name!
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADI, NETWORKVALSPLITTER.bval[0]); //First byte!
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADI, NETWORKVALSPLITTER.bval[1]); //Second byte!
	NETWORKVALSPLITTER.wval = SDL_SwapBE16(0); //Tag length!
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADI, NETWORKVALSPLITTER.bval[0]); //First byte!
	packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADI, NETWORKVALSPLITTER.bval[1]); //Second byte!

	//Now, the packet is fully ready!
	if (Packetserver_clients[connectedclient].pppoe_discovery_PADI.length != 0x18) //Packet length mismatch?
	{
		packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADI); //PADR not ready to be sent yet!
		return 0; //Failure!
	}
	else //Send the PADR packet!
	{
		//Send the PADR packet that's buffered!
		sendpkt_pcap(Packetserver_clients[connectedclient].pppoe_discovery_PADI.buffer, Packetserver_clients[connectedclient].pppoe_discovery_PADI.length); //Send the packet to the network!
	}
	return 1; //Success!
}

byte PPPOE_handlePADreceived(sword connectedclient)
{
	uint_32 pos; //Our packet buffer location!
	word length,sessionid,requiredsessionid;
	byte code;
	//Handle a packet that's currently received!
	ETHERNETHEADER ethernetheader, packetheader;
	memcpy(&ethernetheader.data, &Packetserver_clients[connectedclient].packet[0], sizeof(ethernetheader.data)); //Make a copy of the ethernet header to use!
	//Handle the CheckSum after the payload here?
	code = Packetserver_clients[connectedclient].packet[sizeof(ethernetheader.data) + 1]; //The code field!
	if (Packetserver_clients[connectedclient].packet[sizeof(ethernetheader.data)] != 0x11) return 0; //Invalid V/T fields!
	memcpy(&length, &Packetserver_clients[connectedclient].packet[sizeof(ethernetheader.data) + 4],sizeof(length)); //Length field!
	memcpy(&sessionid, &Packetserver_clients[connectedclient].packet[sizeof(ethernetheader.data) + 2], sizeof(sessionid)); //Session_ID field!
	if (Packetserver_clients[connectedclient].pppoe_discovery_PADI.buffer) //PADI sent?
	{
		if(Packetserver_clients[connectedclient].pppoe_discovery_PADO.buffer) //PADO received?
		{
			if (Packetserver_clients[connectedclient].pppoe_discovery_PADR.buffer) //PADR sent?
			{
				if (Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer==NULL) //Waiting for PADS to arrive?
				{
					if (sessionid) return 0; //No session ID yet!
					if (code != 0x65) return 0; //No PADS yet!
					//We've received our PADO!
					//Ignore it's contents for now(unused) and accept always!
					for (pos = 0; pos < Packetserver_clients[connectedclient].pktlen; ++pos) //Add!
					{
						packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADO, Packetserver_clients[connectedclient].packet[pos]); //Add to the buffer!
					}
					return 1; //Handled!
				}
				else //When PADS is received, we're ready for action for normal communication! Handle PADT packets!
				{
					memcpy(&requiredsessionid, &Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer[sizeof(ethernetheader.data) + 2], sizeof(sessionid)); //Session_ID field!
					if (code != 0xA7) return 0; //Not a PADT packet?
					if (sessionid != requiredsessionid) return 0; //Not our session ID?
					//Our session has been terminated. Clear all buffers!
					packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADI); //No PADI anymore!
					packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADO); //No PADO anymore!
					packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR); //No PADR anymore!
					packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADS); //No PADS anymore!
					packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADT); //No PADT anymore!
					return 1; //Handled!
				}
			}
			else //Need PADR to be sent?
			{
				//Send PADR packet now?
				//Ignore the received packet, we can't handle any!
				//Now, the PADR packet again!
				packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR); //Clear the packet!
				//First, the Ethernet header!
				memcpy(&ethernetheader, &Packetserver_clients[connectedclient].pppoe_discovery_PADO.buffer,sizeof(ethernetheader.data)); //The ethernet header that was used to send the PADO packet!
				memcpy(&packetheader.dst, &ethernetheader.src, sizeof(packetheader.dst)); //Make a copy of the ethernet destination to use!
				memcpy(&packetheader.src, &ethernetheader.dst, sizeof(packetheader.src)); //Make a copy of the ethernet source to use!
				memcpy(&packetheader.type, &ethernetheader.type, sizeof(packetheader.type)); //Make a copy of the ethernet type to use!
				for (pos = 0; pos < sizeof(packetheader.data); ++pos)
				{
					packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR, packetheader.data[pos]); //Send the header!
				}
				packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR, 0x11); //V/T!
				packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR, 0x19); //PADR!
				for (pos = sizeof(ethernetheader.data) + 2; pos < Packetserver_clients[connectedclient].pppoe_discovery_PADO.length; ++pos) //Remainder of the PADO packet copied!
				{
					packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR, Packetserver_clients[connectedclient].pppoe_discovery_PADO.buffer[pos]); //Send the remainder of the PADO packet!
				}
				//Now, the packet is fully ready!
				if (Packetserver_clients[connectedclient].pppoe_discovery_PADR.length != Packetserver_clients[connectedclient].pppoe_discovery_PADO.length) //Packet length mismatch?
				{
					packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR); //PADR not ready to be sent yet!
				}
				else //Send the PADR packet!
				{
					//Send the PADR packet that's buffered!
					sendpkt_pcap(Packetserver_clients[connectedclient].pppoe_discovery_PADR.buffer, Packetserver_clients[connectedclient].pppoe_discovery_PADR.length); //Send the packet to the network!
				}
				return 0; //Not handled!
			}
		}
		else //Waiting for PADO packet response? Parse any PADO responses!
		{
			if (sessionid) return 0; //No session ID yet!
			if (code != 7) return 0; //No PADO yet!
			//We've received our PADO!
			//Ignore it's contents for now(unused) and accept always!
			for (pos = 0; pos < Packetserver_clients[connectedclient].pktlen; ++pos) //Add!
			{
				packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADO, Packetserver_clients[connectedclient].packet[pos]); //Add to the buffer!
			}
			//Send the PADR packet now!
			memcpy(&packetheader.dst, &ethernetheader.src, sizeof(packetheader.dst)); //Make a copy of the ethernet destination to use!
			memcpy(&packetheader.src, &ethernetheader.dst, sizeof(packetheader.src)); //Make a copy of the ethernet source to use!
			memcpy(&packetheader.type, &ethernetheader.type, sizeof(packetheader.type)); //Make a copy of the ethernet type to use!

			//First, the ethernet header!
			for (pos = 0; pos < sizeof(packetheader.data); ++pos)
			{
				packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR, packetheader.data[pos]); //Send the header!
			}

			//Now, the PADR packet!
			packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR); //Clear the packet!
			packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR, 0x11); //V/T!
			packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR, 0x19); //PADR!
			for (pos = sizeof(ethernetheader.data)+2; pos < Packetserver_clients[connectedclient].pktlen; ++pos) //Remainder of the PADO packet copied!
			{
				packetServerAddPPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR, Packetserver_clients[connectedclient].packet[pos]); //Send the remainder of the PADO packet!
			}
			//Now, the packet is fully ready!
			if (Packetserver_clients[connectedclient].pppoe_discovery_PADR.length != Packetserver_clients[connectedclient].pktlen) //Packet length mismatch?
			{
				packetServerFreePPPDiscoveryQueue(&Packetserver_clients[connectedclient].pppoe_discovery_PADR); //PADR not ready to be sent yet!
				return 0; //Not handled!
			}
			else //Send the PADR packet!
			{
				//Send the PADR packet that's buffered!
				sendpkt_pcap(Packetserver_clients[connectedclient].pppoe_discovery_PADR.buffer,Packetserver_clients[connectedclient].pppoe_discovery_PADR.length); //Send the packet to the network!
			}
			return 1; //Handled!
		}
	}
	//No PADI sent? Can't handle anything!
	return 0; //Not handled!
}

void updateModem(DOUBLE timepassed) //Sound tick. Executes every instruction.
{
	sword connectedclient;
	sword connectionid;
	byte datatotransmit;
	ETHERNETHEADER ethernetheader, ppptransmitheader;
	memset(&ppptransmitheader, 0, sizeof(ppptransmitheader));
	word headertype; //What header type are we?
	modem.timer += timepassed; //Add time to the timer!
	if (modem.escaping) //Escapes buffered and escaping?
	{
		if (modem.timer>=modem.escapecodeguardtime) //Long delay time?
		{
			if (modem.escaping==3) //3 escapes?
			{
				modem.escaping = 0; //Stop escaping!
				modem.datamode = 0; //Return to command mode!
				modem.ATcommandsize = 0; //Start a new command!
				modem_responseResult(MODEMRESULT_OK); //OK message to escape!
			}
			else //Not 3 escapes buffered to be sent?
			{
				for (;modem.escaping;) //Send the escaped data after all!
				{
					--modem.escaping;
					modem_writeCommandData(modem.escapecharacter); //Send the escaped data!
				}
			}
		}
	}

	if (modem.wascommandcompletionechoTimeout) //Timer running?
	{
		modem.wascommandcompletionechoTimeout -= timepassed;
		if (modem.wascommandcompletionechoTimeout <= (DOUBLE)0.0f) //Expired?
		{
			modem.wascommandcompletionecho = 0; //Disable the linefeed echo!
			modem.wascommandcompletionechoTimeout = (DOUBLE)0; //Stop the timeout!
			modem_flushCommandCompletion(); //Execute the command immediately!
		}
	}

	if (modem.detectiontimer[0]) //Timer running?
	{
		modem.detectiontimer[0] -= timepassed;
		if (modem.detectiontimer[0]<=(DOUBLE)0.0f) //Expired?
			modem.detectiontimer[0] = (DOUBLE)0; //Stop timer!
	}
	if (modem.detectiontimer[1]) //Timer running?
	{
		modem.detectiontimer[1] -= timepassed;
		if (modem.detectiontimer[1]<=(DOUBLE)0.0f) //Expired?
			modem.detectiontimer[1] = (DOUBLE)0; //Stop timer!
	}
	if (modem.RTSlineDelay) //Timer running?
	{
		modem.RTSlineDelay -= timepassed;
	}
	if (modem.DTRlineDelay) //Timer running?
	{
		modem.DTRlineDelay -= timepassed;
	}
	if (modem.RTSlineDelay && modem.DTRlineDelay) //Both timing?
	{
		if ((modem.RTSlineDelay<=(DOUBLE)0.0f) && (modem.DTRlineDelay<=(DOUBLE)0.0f)) //Both expired?
		{
			modem.RTSlineDelay = (DOUBLE)0; //Stop timer!
			modem.DTRlineDelay = (DOUBLE)0; //Stop timer!
			modem_updatelines(3); //Update both lines at the same time!
		}
	}
	if (modem.RTSlineDelay) //Timer running?
	{
		if (modem.RTSlineDelay<=(DOUBLE)0.0f) //Expired?
		{
			modem.RTSlineDelay = (DOUBLE)0; //Stop timer!
			modem_updatelines(2); //Update line!
		}
	}
	if (modem.DTRlineDelay) //Timer running?
	{
		if (modem.DTRlineDelay<=(DOUBLE)0.0f) //Expired?
		{
			modem.DTRlineDelay = (DOUBLE)0; //Stop timer!
			modem_updatelines(1); //Update line!
		}
	}

	modem.serverpolltimer += timepassed;
	if ((modem.serverpolltimer>=modem.serverpolltick) && modem.serverpolltick) //To poll?
	{
		modem.serverpolltimer = fmod(modem.serverpolltimer,modem.serverpolltick); //Polling once every turn!
		if (!(((modem.linechanges & 1) == 0) && (PacketServer_running == 0))) //Able to accept?
		{
			if ((connectionid = acceptTCPServer()) >= 0) //Are we connected to?
			{
				if (PacketServer_running) //Packet server is running?
				{
					connectedclient = allocPacketserver_client(); //Try to allocate!
					if (connectedclient >= 0) //Allocated?
					{
						Packetserver_clients[connectedclient].connectionid = connectionid; //We're connected like this!
						modem.connected = 2; //Connect as packet server instead, we start answering manually instead of the emulated modem!
						modem.ringing = 0; //Never ring!
						initPacketServer(connectedclient); //Initialize the packet server to be used!
					}
					else //Failed to allocate?
					{
						TCP_DisconnectClientServer(connectionid); //Try and disconnect, if possible!
					}
				}
				else if (connectionid == 0) //Normal behaviour: start ringing!
				{
					modem.connectionid = connectionid; //We're connected like this!
					modem.ringing = 1; //We start ringing!
					modem.registers[1] = 0; //Reset ring counter!
					modem.ringtimer = timepassed; //Automatic time timer, start immediately!
				}
				else //Invalid ID to handle right now(single host only atm)?
				{
					TCP_DisconnectClientServer(connectionid); //Try and disconnect, if possible!
				}
			}
		}
		else //We can't be connected to, stop the server if so!
		{
			TCPServer_Unavailable(); //We're unavailable to connect to!
			if ((modem.connected==1) || modem.ringing) //We're connected as a modem?
			{
				TCP_DisconnectClientServer(modem.connectionid);
				modem.connectionid = -1; //Not connected anymore!
				fifobuffer_clear(modem.inputdatabuffer[0]); //Clear the output buffer for the next client!
				fifobuffer_clear(modem.outputbuffer[0]); //Clear the output buffer for the next client!
			}
		}
	}

	if (modem.ringing) //Are we ringing?
	{
		modem.ringtimer -= timepassed; //Time!
		if (modem.ringtimer<=0.0) //Timed out?
		{
			if (modem.ringing & 2) //Ring completed?
			{
				++modem.registers[1]; //Increase numbr of rings!
				if ((modem.registers[0] > 0) && (modem.registers[1] >= modem.registers[0])) //Autoanswer?
				{
					modem.registers[1] = 0; //When connected, clear the register!
					if (modem_connect(NULL)) //Accept incoming call?
					{
						modem_Answered(); //We've answered!
						return; //Abort: not ringing anymore!
					}
				}
				//Wait for the next ring to start!
				modem.ringing &= ~2; //Wait to start a new ring!
				#ifdef IS_LONGDOUBLE
					modem.ringtimer += 3000000000.0L; //3s timer for every ring!
				#else
					modem.ringtimer += 3000000000.0; //3s timer for every ring!
				#endif
			}
			else //Starting a ring?
			{
				modem_responseResult(MODEMRESULT_RING); //We're ringing!
				//Wait for the next ring to start!
				modem.ringing |= 2; //Wait to start a new ring!
				#ifdef IS_LONGDOUBLE
					modem.ringtimer += 3000000000.0L; //3s timer for every ring!
				#else
					modem.ringtimer += 3000000000.0; //3s timer for every ring!
				#endif
			}
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
				if (modem.connected == 2) //Running the packet server?
				{
					for (connectedclient = 0; connectedclient < Packetserver_totalClients; ++connectedclient) //Check all connected clients!
					{
						if (Packetserver_clients[connectedclient].used == 0) continue; //Skip unused clients!
						//Handle packet server packet data transfers into the inputdatabuffer/outputbuffer to the network!
						if (Packetserver_clients[connectedclient].packetserver_receivebuffer) //Properly allocated?
						{
							if (net.packet || Packetserver_clients[connectedclient].packet) //Packet has been received or processing? Try to start transmit it!
							{
								if (Packetserver_clients[connectedclient].packet == NULL) //Ready to receive?
								{
									Packetserver_clients[connectedclient].packet = zalloc(net.pktlen,"SERVER_PACKET",NULL); //Allocate a packet to receive!
									if (Packetserver_clients[connectedclient].packet) //Allocated?
									{
										Packetserver_clients[connectedclient].pktlen = net.pktlen; //Save the length of the packet!
										memcpy(Packetserver_clients[connectedclient].packet, net.packet, net.pktlen); //Copy the packet to the active buffer!
									}
								}
								if (fifobuffer_freesize(Packetserver_clients[connectedclient].packetserver_receivebuffer) >= 2) //Valid to produce more data?
								{
									if ((Packetserver_clients[connectedclient].packetserver_packetpos == 0) && (Packetserver_clients[connectedclient].packetserver_packetack == 0)) //New packet?
									{
										if (Packetserver_clients[connectedclient].pktlen > (sizeof(ethernetheader.data) + ((Packetserver_clients[connectedclient].packetserver_slipprotocol!=3)?20:7))) //Length OK(at least one byte of data and complete IP header) or the PPP packet size?
										{
											memcpy(&ethernetheader.data, Packetserver_clients[connectedclient].packet, sizeof(ethernetheader.data)); //Copy for inspection!
											if ((memcmp(&ethernetheader.dst, &packetserver_sourceMAC, sizeof(ethernetheader.dst)) != 0) && (memcmp(&ethernetheader.dst, &packetserver_broadcastMAC, sizeof(ethernetheader.dst)) != 0)) //Invalid destination(and not broadcasting)?
											{
												//dolog("ethernetcard","Discarding destination."); //Showing why we discard!
												goto invalidpacket; //Invalid packet!
											}
											if (Packetserver_clients[connectedclient].packetserver_slipprotocol == 3) //PPP protocol used?
											{
												if (ethernetheader.type == SDL_SwapBE16(0x8863)) //Are we a discovery packet?
												{
													if (PPPOE_handlePADreceived(connectedclient)) //Handle the received PAD packet!
													{
														//Discard the received packet, so nobody else handles it too!
														freez((void**)&net.packet, net.pktlen, "MODEM_PACKET");
														net.packet = NULL; //Discard if failed to deallocate!
														net.pktlen = 0; //Not allocated!
													}
												}
												headertype = SDL_SwapBE16(0x8864); //Receiving uses normal PPP packets to transfer/receive on the receiver line only!
											}
											else if (Packetserver_clients[connectedclient].packetserver_slipprotocol==2) //IPX protocol used?
											{
												headertype = SDL_SwapBE16(0x8137); //We're an IPX packet!
											}
											else //IPv4?
											{
												headertype = SDL_SwapBE16(0x0800); //We're an IP packet!
											}
											if (Packetserver_clients[connectedclient].packetserver_stage != PACKETSTAGE_PACKETS) goto invalidpacket; //Don't handle SLIP/PPP/IPX yet!
											if (ethernetheader.type != headertype) //Invalid type?
											{
												//dolog("ethernetcard","Discarding type: %04X",SDL_SwapBE16(ethernetheader.type)); //Showing why we discard!
												goto invalidpacket; //Invalid packet!
											}
											if (Packetserver_clients[connectedclient].packetserver_useStaticIP && (headertype==SDL_SwapBE16(0x0800))) //IP filter to apply?
											{
												if ((memcmp(&Packetserver_clients[connectedclient].packet[sizeof(ethernetheader.data) + 16], Packetserver_clients[connectedclient].packetserver_staticIP, 4) != 0) && (memcmp(&Packetserver_clients[connectedclient].packet[sizeof(ethernetheader.data) + 16], packetserver_broadcastIP, 4) != 0)) //Static IP mismatch?
												{
													goto invalidpacket; //Invalid packet!
												}
											}
											//Valid packet! Receive it!
											if (Packetserver_clients[connectedclient].packetserver_slipprotocol) //Using slip or PPP protocol?
											{
												if (Packetserver_clients[connectedclient].packetserver_slipprotocol == 3) //PPP?
												{
													if (Packetserver_clients[connectedclient].pppoe_discovery_PADS.length == 0) //No PADS received yet? Invalid packet!
													{
														goto invalidpacket; //Invalid packet: not ready yet!
													}
													if (Packetserver_clients[connectedclient].packet[sizeof(ethernetheader.data) + 0] != 0x11) //Invalid VER/type?
													{
														goto invalidpacket; //Invalid packet!
													}
													if (Packetserver_clients[connectedclient].packet[sizeof(ethernetheader.data) + 1] != 0) //Invalid Type?
													{
														goto invalidpacket; //Invalid packet!
													}
													word length,sessionid,requiredsessionid,pppoe_protocol;
													memcpy(&length, &Packetserver_clients[connectedclient].packet[sizeof(ethernetheader.data) + 4], sizeof(length)); //The length field!
													memcpy(&sessionid, &Packetserver_clients[connectedclient].packet[sizeof(ethernetheader.data) + 2], sizeof(sessionid)); //The length field!
													memcpy(&pppoe_protocol, &Packetserver_clients[connectedclient].packet[sizeof(ethernetheader.data) + 6], sizeof(sessionid)); //The length field!
													memcpy(&requiredsessionid, &Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer[sizeof(ethernetheader.data) + 4], sizeof(requiredsessionid)); //The required session id field!
													if (SDL_SwapBE16(length) < 4) //Invalid Length?
													{
														goto invalidpacket; //Invalid packet!
													}
													if (sessionid != requiredsessionid) //Invalid required session id(other client)?
													{
														goto invalidpacket; //Invalid packet!
													}
													if (SDL_SwapBE16(pppoe_protocol) != 0xC021) //Invalid packet type?
													{
														goto invalidpacket; //Invalid packet!
													}
													Packetserver_clients[connectedclient].packetserver_packetpos = sizeof(ethernetheader.data)+0x8; //Skip the ethernet header and give the raw IP data!
													Packetserver_clients[connectedclient].packetserver_bytesleft = Packetserver_clients[connectedclient].pktlen - Packetserver_clients[connectedclient].packetserver_packetpos; //How much is left to send?
												}
												else //SLIP?
												{
													Packetserver_clients[connectedclient].packetserver_packetpos = sizeof(ethernetheader.data); //Skip the ethernet header and give the raw IP data!
													Packetserver_clients[connectedclient].packetserver_bytesleft = MIN(Packetserver_clients[connectedclient].pktlen - Packetserver_clients[connectedclient].packetserver_packetpos, SDL_SwapBE16(*((word*)&Packetserver_clients[connectedclient].packet[sizeof(ethernetheader.data) + 2]))); //How much is left to send?
												}
											}
											else //We're using the ethernet header protocol?
											{
												//else, we're using ethernet header protocol, so take the
												Packetserver_clients[connectedclient].packetserver_packetack = 1; //We're acnowledging the packet, so start transferring it!
												Packetserver_clients[connectedclient].packetserver_bytesleft = Packetserver_clients[connectedclient].pktlen; //Use the entire packet, unpatched!
											}
											//dolog("ethernetcard","Skipping %u bytes of header data...",packetserver_packetpos); //Log it!
										}
										else //Invalid length?
										{
											//dolog("ethernetcard","Discarding invalid packet size: %u...",net.pktlen); //Log it!
										invalidpacket:
											//Discard the invalid packet!
											freez((void **)&Packetserver_clients[connectedclient].packet, Packetserver_clients[connectedclient].pktlen, "SERVER_PACKET"); //Release the packet to receive new packets again!
											//dolog("ethernetcard","Discarding invalid packet size or different cause: %u...",net.pktlen); //Log it!
											Packetserver_clients[connectedclient].packet = NULL; //No packet!
											Packetserver_clients[connectedclient].packetserver_packetpos = 0; //Reset packet position!
											Packetserver_clients[connectedclient].packetserver_packetack = 0; //Not acnowledged yet!
										}
									}
									if (Packetserver_clients[connectedclient].packetserver_stage != PACKETSTAGE_PACKETS)
									{
										if (Packetserver_clients[connectedclient].packet) //Still have a packet allocated to discard?
										{
											goto invalidpacket; //Discard the received packet!
										}
										goto skipSLIP_PPP; //Don't handle SLIP/PPP because we're not ready yet!
									}
									if (Packetserver_clients[connectedclient].packet) //Still a valid packet to send?
									{
										//Convert the buffer into transmittable bytes using the proper encoding!
										if (Packetserver_clients[connectedclient].packetserver_bytesleft) //Not finished yet?
										{
											//Start transmitting data into the buffer, according to the protocol!
											--Packetserver_clients[connectedclient].packetserver_bytesleft;
											datatotransmit = Packetserver_clients[connectedclient].packet[Packetserver_clients[connectedclient].packetserver_packetpos++]; //Read the data to construct!
											if (Packetserver_clients[connectedclient].packetserver_slipprotocol==3) //PPP?
											{
												if (Packetserver_clients[connectedclient].packetserver_packetpos == (sizeof(ethernetheader.data) + 0x8 + 1)) //Starting new packet?
												{
													if (!(Packetserver_clients[connectedclient].pppoe_lastrecvbytewasEND)) //Not doubled END?
													{
														writefifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, PPP_END); //END of frame!
														Packetserver_clients[connectedclient].pppoe_lastrecvbytewasEND = 1; //Last was END!
													}
												}

												#ifdef PPPOE_ENCODEDECODE
												if (datatotransmit == PPP_END) //End byte?
												{
													//dolog("ethernetcard","transmitting escaped SLIP END to client");
													writefifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, PPP_ESC); //Escaped ...
													writefifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, PPP_ENCODEESC(PPP_ESC)); //END raw data!
												}
												else if (datatotransmit == PPP_ESC) //ESC byte?
												{
													//dolog("ethernetcard","transmitting escaped SLIP ESC to client");
													writefifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, PPP_ESC); //Escaped ...
													writefifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, PPP_ENCODEESC(PPP_ESC)); //ESC raw data!
												}
												else //Normal data?
												{
													//dolog("ethernetcard","transmitting raw to client: %02X",datatotransmit);
													writefifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, datatotransmit); //Unescaped!
												}
												Packetserver_clients[connectedclient].pppoe_lastrecvbytewasEND = 0; //Last wasn't END!
												#else
												if (!((Packetserver_clients[connectedclient].pppoe_lastrecvbytewasEND) && (datatotransmit == PPP_END))) //Not doubled END?
												{
													writefifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, datatotransmit); //Raw!
												}
												Packetserver_clients[connectedclient].pppoe_lastrecvbytewasEND = (datatotransmit == PPP_END); //Last was END?
												#endif
											}
											else //SLIP?
											{
												if (datatotransmit == SLIP_END) //End byte?
												{
													//dolog("ethernetcard","transmitting escaped SLIP END to client");
													writefifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, SLIP_ESC); //Escaped ...
													writefifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, SLIP_ESC_END); //END raw data!
												}
												else if (datatotransmit == SLIP_ESC) //ESC byte?
												{
													//dolog("ethernetcard","transmitting escaped SLIP ESC to client");
													writefifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, SLIP_ESC); //Escaped ...
													writefifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, SLIP_ESC_ESC); //ESC raw data!
												}
												else //Normal data?
												{
													//dolog("ethernetcard","transmitting raw to client: %02X",datatotransmit);
													writefifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, datatotransmit); //Unescaped!
												}
											}
										}
										else //Finished transferring a frame?
										{
											if (Packetserver_clients[connectedclient].packetserver_slipprotocol==3) //PPP?
											{
												//dolog("ethernetcard","transmitting PPP END to client and finishing packet buffer(size: %u)",net.pktlen);
												if (!(Packetserver_clients[connectedclient].pppoe_lastrecvbytewasEND)) //Not doubled END?
												{
													writefifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, PPP_END); //END of frame!
													Packetserver_clients[connectedclient].pppoe_lastrecvbytewasEND = 1; //Last was END!
												}
											}
											else //SLIP?
											{
												//dolog("ethernetcard","transmitting SLIP END to client and finishing packet buffer(size: %u)",net.pktlen);
												writefifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, SLIP_END); //END of frame!
											}
											//logpacket(0,Packetserver_clients[connectedclient].packet,Packetserver_clients[connectedclient].pktlen); //Log it!
											freez((void **)&Packetserver_clients[connectedclient].packet, Packetserver_clients[connectedclient].pktlen, "SERVER_PACKET"); //Release the packet to receive new packets again!
											Packetserver_clients[connectedclient].packet = NULL; //Discard the packet anyway, no matter what!
											Packetserver_clients[connectedclient].packetserver_packetpos = 0; //Reset packet position!
											Packetserver_clients[connectedclient].packetserver_packetack = 0; //Not acnowledged yet!
										}
									}
								}
							}

							//Transmit the encoded packet buffer to the client!
							if (fifobuffer_freesize(modem.outputbuffer[connectedclient]) && peekfifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, &datatotransmit)) //Able to transmit something?
							{
								for (; fifobuffer_freesize(modem.outputbuffer[connectedclient]) && peekfifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, &datatotransmit);) //Can we still transmit something more?
								{
									if (writefifobuffer(modem.outputbuffer[connectedclient], datatotransmit)) //Transmitted?
									{
										//dolog("ethernetcard","transmitted SLIP data to client: %02X",datatotransmit);
										datatotransmit = readfifobuffer(Packetserver_clients[connectedclient].packetserver_receivebuffer, &datatotransmit); //Discard the data that's being transmitted!
									}
								}
							}
						}

						if (Packetserver_clients[connectedclient].packetserver_stage != PACKETSTAGE_PACKETS)
						{
							goto skipSLIP_PPP; //Don't handle SLIP/PPP because we're not ready yet!
						}

						//Handle transmitting packets(with automatically increasing buffer sizing, as a packet can be received of any size theoretically)!
						if (peekfifobuffer(modem.inputdatabuffer[connectedclient], &datatotransmit)) //Is anything transmitted yet?
						{
							if ((Packetserver_clients[connectedclient].packetserver_transmitlength == 0) && (Packetserver_clients[connectedclient].packetserver_slipprotocol)) //We might need to create an ethernet header?
							{
								//Build an ethernet header, platform dependent!
								//Use the data provided by the settings!
								byte b;
								if ((Packetserver_clients[connectedclient].packetserver_slipprotocol == 3) && Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer && Packetserver_clients[connectedclient].pppoe_discovery_PADS.length) //PPP?
								{
									memcpy(&ppptransmitheader.data, &Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer,sizeof(ppptransmitheader.data)); //Make a local copy for usage!
								}
								for (b = 0; b < 6; ++b) //Process MAC addresses!
								{
									if ((Packetserver_clients[connectedclient].packetserver_slipprotocol == 3) && Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer && Packetserver_clients[connectedclient].pppoe_discovery_PADS.length) //PPP?
									{
										ethernetheader.dst[b] = ppptransmitheader.src[b]; //The used server MAC is the destination!
										ethernetheader.src[b] = ppptransmitheader.dst[b]; //The Packet server MAC is the source!
									}
									else //SLIP
									{
										ethernetheader.dst[b] = packetserver_gatewayMAC[b]; //Gateway MAC is the destination!
										ethernetheader.src[b] = packetserver_sourceMAC[b]; //Packet server MAC is the source!
									}
								}
								if (Packetserver_clients[connectedclient].packetserver_slipprotocol==3) //PPP?
								{
									if (Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer && Packetserver_clients[connectedclient].pppoe_discovery_PADS.length) //Valid to send?
									{
										ethernetheader.type = SDL_SwapBE16(0x8864); //Our packet type!
									}
									else goto noPPPtransmit; //Ignore the transmitter for now!
								}
								else if (Packetserver_clients[connectedclient].packetserver_slipprotocol==2) //IPX?
								{
									ethernetheader.type = SDL_SwapBE16(0x8137); //We're an IPX packet!
								}
								else //IPv4?
								{
									ethernetheader.type = SDL_SwapBE16(0x0800); //We're an IP packet!
								}
								for (b = 0; b < 14; ++b) //Use the provided ethernet packet header!
								{
									if (!packetServerAddWriteQueue(connectedclient,ethernetheader.data[b])) //Failed to add?
									{
										break; //Stop adding!
									}
								}
								if ((Packetserver_clients[connectedclient].packetserver_slipprotocol == 3) && Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer && Packetserver_clients[connectedclient].pppoe_discovery_PADS.length) //PPP?
								{
									if (!packetServerAddWriteQueue(connectedclient, 0x11)) //V/T?
									{
										goto noPPPtransmit; //Stop adding!
									}
									if (!packetServerAddWriteQueue(connectedclient, 0x00)) //Code?
									{
										goto noPPPtransmit; //Stop adding!
									}
									NETWORKVALSPLITTER.bval[0] = Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer[0x10]; //Session_ID!
									NETWORKVALSPLITTER.bval[1] = Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer[0x11]; //Session_ID!
									if (!packetServerAddWriteQueue(connectedclient, NETWORKVALSPLITTER.bval[0])) //First byte?
									{
										goto noPPPtransmit; //Stop adding!
									}
									if (!packetServerAddWriteQueue(connectedclient, NETWORKVALSPLITTER.bval[1])) //Second byte?
									{
										goto noPPPtransmit; //Stop adding!
									}
									NETWORKVALSPLITTER.wval = SDL_SwapBE16(0); //Length: to be filled in later!
									if (!packetServerAddWriteQueue(connectedclient, NETWORKVALSPLITTER.bval[0])) //First byte?
									{
										goto noPPPtransmit; //Stop adding!
									}
									if (!packetServerAddWriteQueue(connectedclient, NETWORKVALSPLITTER.bval[1])) //Second byte?
									{
										goto noPPPtransmit; //Stop adding!
									}
									NETWORKVALSPLITTER.wval = SDL_SwapBE16(0xC021); //Protocol!
									if (!packetServerAddWriteQueue(connectedclient, NETWORKVALSPLITTER.bval[0])) //First byte?
									{
										goto noPPPtransmit; //Stop adding!
									}
									if (!packetServerAddWriteQueue(connectedclient, NETWORKVALSPLITTER.bval[1])) //Second byte?
									{
										goto noPPPtransmit; //Stop adding!
									}
								}
								if (
									((Packetserver_clients[connectedclient].packetserver_transmitlength != 14) && (Packetserver_clients[connectedclient].packetserver_slipprotocol!=3)) || 
									((Packetserver_clients[connectedclient].packetserver_transmitlength != 22) && (Packetserver_clients[connectedclient].packetserver_slipprotocol == 3))
									) //Failed to generate header?
								{
									dolog("ethernetcard", "Error: Transmit initialization failed. Resetting transmitter!");
									noPPPtransmit:
									if (!(Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer && Packetserver_clients[connectedclient].pppoe_discovery_PADS.length)) //Not ready to send?
									{
										if (!(Packetserver_clients[connectedclient].pppoe_discovery_PADI.buffer && Packetserver_clients[connectedclient].pppoe_discovery_PADI.length)) //No PADI sent yet? Start sending one now to restore the connection!
										{
											PPPOE_requestdiscovery(connectedclient); //Try to request a new discovery for transmitting PPP packets!
										}
										goto skipSLIP_PPP; //Don't handle the sent data yet, prepare for sending by reconnecting to the PPPOE server!
									}
									Packetserver_clients[connectedclient].packetserver_transmitlength = 0; //Abort the packet generation!
								}
								else
								{
									//dolog("ethernetcard","Header for transmitting to the server has been setup!");
								}
							}
							if (((datatotransmit == SLIP_END) && (Packetserver_clients[connectedclient].packetserver_slipprotocol!=3))
									|| ((datatotransmit==PPP_END) && (Packetserver_clients[connectedclient].packetserver_slipprotocol==3))) //End-of-frame? Send the frame!
							{
								if (Packetserver_clients[connectedclient].packetserver_transmitstate && (Packetserver_clients[connectedclient].packetserver_slipprotocol!=3)) //Were we already escaping?
								{
									if (packetServerAddWriteQueue(connectedclient, SLIP_ESC)) //Ignore the escaped sequence: it's invalid, thus parsed raw!
									{
										Packetserver_clients[connectedclient].packetserver_transmitstate = 0; //We're not escaping something anymore!
									}
								}
								else if (Packetserver_clients[connectedclient].packetserver_transmitstate) //Escaped with  PPP?
								{
									Packetserver_clients[connectedclient].packetserver_transmitstate = 0; //Stopmescaping!
								}
								if (Packetserver_clients[connectedclient].packetserver_transmitstate == 0) //Ready to send the packet(not waiting for the buffer to free)?
								{
									//Clean up the packet container!
									if (
										((Packetserver_clients[connectedclient].packetserver_transmitlength > sizeof(ethernetheader.data)) && (Packetserver_clients[connectedclient].packetserver_slipprotocol!=3)) || //Anything buffered(the header is required)?
										((Packetserver_clients[connectedclient].packetserver_transmitlength > 0x22) && (Packetserver_clients[connectedclient].packetserver_slipprotocol == 3)) //Anything buffered(the header is required)?
										)
									{
										//Send the frame to the server, if we're able to!
										if ((Packetserver_clients[connectedclient].packetserver_transmitlength <= 0xFFFF) || (Packetserver_clients[connectedclient].packetserver_slipprotocol == 3)) //Within length range?
										{
											//dolog("ethernetcard","Sending generated packet(size: %u)!",packetserver_transmitlength);
											//logpacket(1,packetserver_transmitbuffer,packetserver_transmitlength); //Log it!
											if (Packetserver_clients[connectedclient].packetserver_slipprotocol == 3) //PPP?
											{
												if (!((Packetserver_clients[connectedclient].pppoe_lastsentbytewasEND))) //Not doubled END?
												{
													if (!packetServerAddWriteQueue(connectedclient, PPP_END))
													{
														goto skipSLIP_PPP; //Don't handle the sending of the packet yet: not ready!
													}
													Packetserver_clients[connectedclient].pppoe_lastsentbytewasEND = 1; //Last was END!
												}
											}
											if (Packetserver_clients[connectedclient].packetserver_slipprotocol == 3) //Length field needs fixing up?
											{
												NETWORKVALSPLITTER.wval = SDL_SwapBE16(Packetserver_clients[connectedclient].packetserver_transmitlength-0x22); //The length of the PPP packet itself!
												Packetserver_clients[connectedclient].packetserver_transmitbuffer[0x12] = NETWORKVALSPLITTER.bval[0]; //First byte!
												Packetserver_clients[connectedclient].packetserver_transmitbuffer[0x13] = NETWORKVALSPLITTER.bval[1]; //Second byte!
											}
											sendpkt_pcap(Packetserver_clients[connectedclient].packetserver_transmitbuffer, Packetserver_clients[connectedclient].packetserver_transmitlength); //Send the packet!
										}
										else
										{
											dolog("ethernetcard", "Error: Can't send packet: packet is too large to send(size: %u)!", Packetserver_clients[connectedclient].packetserver_transmitlength);
										}
										//Now, cleanup the buffered frame!
										freez((void**)&Packetserver_clients[connectedclient].packetserver_transmitbuffer, Packetserver_clients[connectedclient].packetserver_transmitsize, "MODEM_SENDPACKET"); //Free 
										Packetserver_clients[connectedclient].packetserver_transmitsize = 1024; //How large is out transmit buffer!
										Packetserver_clients[connectedclient].packetserver_transmitbuffer = zalloc(1024, "MODEM_SENDPACKET", NULL); //Simple transmit buffer, the size of a packet byte(when encoded) to be able to buffer any packet(since any byte can be doubled)!
									}
									else
									{
										dolog("ethernetcard", "Error: Not enough buffered to send to the server(size: %u)!", Packetserver_clients[connectedclient].packetserver_transmitlength);
									}
									Packetserver_clients[connectedclient].packetserver_transmitlength = 0; //We're at the start of this buffer, nothing is sent yet!
									Packetserver_clients[connectedclient].packetserver_transmitstate = 0; //Not escaped anymore!
									readfifobuffer(modem.inputdatabuffer[connectedclient], &datatotransmit); //Ignore the data, just discard the packet END!
								}
							}
							#ifdef PPPOE_ENCODEDECODE
							else if ((Packetserver_clients[connectedclient].packetserver_transmitstate) && (Packetserver_clients[connectedclient].packetserver_slipprotocol==3)) //PPP ESCaped value?
							{
								if (Packetserver_clients[connectedclient].packetserver_transmitlength) //Gotten a valid packet?
								{
									if (packetServerAddWriteQueue(connectedclient, PPP_DECODEESC(datatotransmit))) //Added to the queue?
									{
										Packetserver_clients[connectedclient].pppoe_lastsentbytewasEND = 0; //Last was not END!
										readfifobuffer(modem.inputdatabuffer[connectedclient], &datatotransmit); //Ignore the data, just discard the packet byte!
										Packetserver_clients[connectedclient].packetserver_transmitstate = 0; //We're not escaping something anymore!
									}
								}
								else //Unable to parse into the buffer? Discard!
								{
									readfifobuffer(modem.inputdatabuffer[connectedclient], &datatotransmit); //Ignore the data, just discard the packet byte!
									Packetserver_clients[connectedclient].packetserver_transmitstate = 0; //We're not escaping something anymore!
								}
							}
							else if ((datatotransmit==PPP_ESC) && (Packetserver_clients[connectedclient].packetserver_slipprotocol==3)) //PPP ESC?
							{
								readfifobuffer(modem.inputdatabuffer[connectedclient], &datatotransmit); //Discard, as it's processed!
								Packetserver_clients[connectedclient].packetserver_transmitstate = 1; //We're escaping something! Multiple escapes are ignored and not sent!
							}
							#endif
							else if ((datatotransmit == SLIP_ESC) && (Packetserver_clients[connectedclient].packetserver_slipprotocol!=3)) //Escaped something?
							{
								if (Packetserver_clients[connectedclient].packetserver_transmitstate) //Were we already escaping?
								{
									if (packetServerAddWriteQueue(connectedclient, SLIP_ESC)) //Ignore the escaped sequence: it's invalid, thus parsed raw!
									{
										Packetserver_clients[connectedclient].packetserver_transmitstate = 0; //We're not escaping something anymore!
									}
								}
								if (Packetserver_clients[connectedclient].packetserver_transmitstate == 0) //Can we start a new escape?
								{
									readfifobuffer(modem.inputdatabuffer[connectedclient], &datatotransmit); //Discard, as it's processed!
									Packetserver_clients[connectedclient].packetserver_transmitstate = 1; //We're escaping something! Multiple escapes are ignored and not sent!
								}
							}
							else if (Packetserver_clients[connectedclient].packetserver_slipprotocol==3) //Active PPP data?
							{
								if (Packetserver_clients[connectedclient].packetserver_transmitlength) //Gotten a valid packet?
								{
									goto addUnescapedValue; //Process an unescaped PPP value!
								}
							}
							else if (Packetserver_clients[connectedclient].packetserver_slipprotocol!=3) //Active SLIP data?
							{
								if (Packetserver_clients[connectedclient].packetserver_transmitlength) //Gotten a valid packet?
								{
									if (Packetserver_clients[connectedclient].packetserver_transmitstate && (datatotransmit == SLIP_ESC_END)) //Transposed END sent?
									{
										if (packetServerAddWriteQueue(connectedclient,SLIP_END)) //Added to the queue?
										{
											readfifobuffer(modem.inputdatabuffer[connectedclient], &datatotransmit); //Ignore the data, just discard the packet byte!
											Packetserver_clients[connectedclient].packetserver_transmitstate = 0; //We're not escaping something anymore!
										}
									}
									else if (Packetserver_clients[connectedclient].packetserver_transmitstate && (datatotransmit == SLIP_ESC_ESC)) //Transposed ESC sent?
									{
										if (packetServerAddWriteQueue(connectedclient,SLIP_ESC)) //Added to the queue?
										{
											readfifobuffer(modem.inputdatabuffer[connectedclient], &datatotransmit); //Ignore the data, just discard the packet byte!
											Packetserver_clients[connectedclient].packetserver_transmitstate = 0; //We're not escaping something anymore!
										}
									}
									else //Parse as a raw data when invalidly escaped or sent unescaped! Also terminate escape sequence as required!
									{
										if (Packetserver_clients[connectedclient].packetserver_transmitstate) //Were we escaping?
										{
											if (packetServerAddWriteQueue(connectedclient, SLIP_ESC)) //Ignore the escaped sequence: it's invalid, thus parsed unescaped!
											{
												Packetserver_clients[connectedclient].packetserver_transmitstate = 0; //We're not escaping something anymore!
											}
										}
										addUnescapedValue:
										if (Packetserver_clients[connectedclient].packetserver_transmitstate==0) //Can we parse the raw data?
										{
											if (packetServerAddWriteQueue(connectedclient, datatotransmit)) //Added to the queue?
											{
												Packetserver_clients[connectedclient].pppoe_lastsentbytewasEND = 0; //Last was not PPP_END!
												readfifobuffer(modem.inputdatabuffer[connectedclient], &datatotransmit); //Ignore the data, just discard the packet byte!
												Packetserver_clients[connectedclient].packetserver_transmitstate = 0; //We're not escaping something anymore!
											}
										}
									}
								}
							}
						}
					skipSLIP_PPP: //SLIP isn't available?

					//Handle an authentication stage
						if (Packetserver_clients[connectedclient].packetserver_stage == PACKETSTAGE_REQUESTUSERNAME)
						{
							authstage_startrequest(timepassed,connectedclient,"username:",PACKETSTAGE_ENTERUSERNAME);
						}

						if (Packetserver_clients[connectedclient].packetserver_stage == PACKETSTAGE_ENTERUSERNAME)
						{
							switch (authstage_enterfield(timepassed, connectedclient, &Packetserver_clients[connectedclient].packetserver_username[0], sizeof(Packetserver_clients[connectedclient].packetserver_username),0,(char)0))
							{
							case 0: //Do nothing!
								break;
							case 1: //Finished stage!
								Packetserver_clients[connectedclient].packetserver_stage = PACKETSTAGE_REQUESTPASSWORD;
								break;
							case 2: //Send the output buffer!
								goto sendoutputbuffer;
								break;
							}
						}

						if (Packetserver_clients[connectedclient].packetserver_stage == PACKETSTAGE_REQUESTPASSWORD)
						{
							authstage_startrequest(timepassed,connectedclient,"password:",PACKETSTAGE_ENTERPASSWORD);
						}

						if (Packetserver_clients[connectedclient].packetserver_stage == PACKETSTAGE_ENTERPASSWORD)
						{
							switch (authstage_enterfield(timepassed, connectedclient, &Packetserver_clients[connectedclient].packetserver_password[0], sizeof(Packetserver_clients[connectedclient].packetserver_password), 0, '*'))
							{
							case 0: //Do nothing!
								break;
							case 1: //Finished stage!
								Packetserver_clients[connectedclient].packetserver_stage = PACKETSTAGE_REQUESTPROTOCOL;
								break;
							case 2: //Send the output buffer!
								goto sendoutputbuffer;
								break;
							}
						}

						if (Packetserver_clients[connectedclient].packetserver_stage == PACKETSTAGE_REQUESTPROTOCOL)
						{
							authstage_startrequest(timepassed,connectedclient,"protocol:",PACKETSTAGE_ENTERPROTOCOL);
						}

						if (Packetserver_clients[connectedclient].packetserver_stage == PACKETSTAGE_ENTERPROTOCOL)
						{
							switch (authstage_enterfield(timepassed, connectedclient, &Packetserver_clients[connectedclient].packetserver_protocol[0], sizeof(Packetserver_clients[connectedclient].packetserver_protocol),1,(char)0))
							{
							case 0: //Do nothing!
								break;
							case 1: //Finished stage!
								if (Packetserver_clients[connectedclient].packetserver_credentials_invalid) goto packetserver_autherror; //Authentication error!
								if (packetserver_authenticate(connectedclient)) //Authenticated?
								{
									Packetserver_clients[connectedclient].packetserver_slipprotocol = (strcmp(Packetserver_clients[connectedclient].packetserver_protocol, "ppp") == 0)?3:((strcmp(Packetserver_clients[connectedclient].packetserver_protocol, "ipxslip") == 0)?2:((strcmp(Packetserver_clients[connectedclient].packetserver_protocol, "slip") == 0) ? 1 : 0)); //Are we using the slip protocol?
									Packetserver_clients[connectedclient].packetserver_stage = PACKETSTAGE_INFORMATION; //We're logged in!
									PPPOE_requestdiscovery(connectedclient); //Start the discovery phase of the connected client!
								}
								else goto packetserver_autherror; //Authentication error!
								break;
							case 2: //Send the output buffer!
								goto sendoutputbuffer;
								break;
							}
						}

						if (Packetserver_clients[connectedclient].packetserver_stage == PACKETSTAGE_INFORMATION)
						{
							if (Packetserver_clients[connectedclient].packetserver_stage_byte == PACKETSTAGE_INITIALIZING)
							{
								memset(&Packetserver_clients[connectedclient].packetserver_stage_str, 0, sizeof(Packetserver_clients[connectedclient].packetserver_stage_str));
								snprintf(Packetserver_clients[connectedclient].packetserver_stage_str, sizeof(Packetserver_clients[connectedclient].packetserver_stage_str), "\r\nMACaddress:%02x:%02x:%02x:%02x:%02x:%02x\r\ngatewayMACaddress:%02x:%02x:%02x:%02x:%02x:%02x\r\n", packetserver_sourceMAC[0], packetserver_sourceMAC[1], packetserver_sourceMAC[2], packetserver_sourceMAC[3], packetserver_sourceMAC[4], packetserver_sourceMAC[5], packetserver_gatewayMAC[0], packetserver_gatewayMAC[1], packetserver_gatewayMAC[2], packetserver_gatewayMAC[3], packetserver_gatewayMAC[4], packetserver_gatewayMAC[5]);
								if (Packetserver_clients[connectedclient].packetserver_useStaticIP && (Packetserver_clients[connectedclient].packetserver_slipprotocol!=3)) //IP filter?
								{
									memset(&Packetserver_clients[connectedclient].packetserver_staticIPstr_information, 0, sizeof(Packetserver_clients[connectedclient].packetserver_staticIPstr_information));
									snprintf(Packetserver_clients[connectedclient].packetserver_staticIPstr_information, sizeof(Packetserver_clients[connectedclient].packetserver_staticIPstr_information), "IPaddress:%s\r\n", Packetserver_clients[connectedclient].packetserver_staticIPstr); //Static IP!
									safestrcat(Packetserver_clients[connectedclient].packetserver_stage_str, sizeof(Packetserver_clients[connectedclient].packetserver_stage_str), Packetserver_clients[connectedclient].packetserver_staticIPstr_information); //Inform about the static IP!
								}
								Packetserver_clients[connectedclient].packetserver_stage_byte = 0; //Init to start of string!
								Packetserver_clients[connectedclient].packetserver_delay = PACKETSERVER_MESSAGE_DELAY; //Delay this until we start transmitting!
							}
							Packetserver_clients[connectedclient].packetserver_delay -= timepassed; //Delaying!
							if ((Packetserver_clients[connectedclient].packetserver_delay <= 0.0) || (!Packetserver_clients[connectedclient].packetserver_delay)) //Finished?
							{
								Packetserver_clients[connectedclient].packetserver_delay = (DOUBLE)0; //Finish the delay!
								if (writefifobuffer(modem.outputbuffer[connectedclient], Packetserver_clients[connectedclient].packetserver_stage_str[Packetserver_clients[connectedclient].packetserver_stage_byte])) //Transmitted?
								{
									if (++Packetserver_clients[connectedclient].packetserver_stage_byte == safestrlen(Packetserver_clients[connectedclient].packetserver_stage_str, sizeof(Packetserver_clients[connectedclient].packetserver_stage_str))) //Finished?
									{
										Packetserver_clients[connectedclient].packetserver_stage_byte = PACKETSTAGE_INITIALIZING; //Prepare for next step!
										Packetserver_clients[connectedclient].packetserver_stage = PACKETSTAGE_READY;
									}
								}
							}
						}

						if (Packetserver_clients[connectedclient].packetserver_stage == PACKETSTAGE_READY)
						{
							if (Packetserver_clients[connectedclient].packetserver_stage_byte == PACKETSTAGE_INITIALIZING)
							{
								memset(&Packetserver_clients[connectedclient].packetserver_stage_str, 0, sizeof(Packetserver_clients[connectedclient].packetserver_stage_str));
								safestrcpy(Packetserver_clients[connectedclient].packetserver_stage_str, sizeof(Packetserver_clients[connectedclient].packetserver_stage_str), "\rCONNECTED\r");
								Packetserver_clients[connectedclient].packetserver_stage_byte = 0; //Init to start of string!
								Packetserver_clients[connectedclient].packetserver_delay = PACKETSERVER_MESSAGE_DELAY; //Delay this until we start transmitting!
							}
							Packetserver_clients[connectedclient].packetserver_delay -= timepassed; //Delaying!
							if ((Packetserver_clients[connectedclient].packetserver_delay <= 0.0) || (!Packetserver_clients[connectedclient].packetserver_delay)) //Finished?
							{
								if (Packetserver_clients[connectedclient].packetserver_slipprotocol == 3) //Requires PAD connection!
								{
									if ((Packetserver_clients[connectedclient].pppoe_discovery_PADS.length && Packetserver_clients[connectedclient].pppoe_discovery_PADS.buffer) == 0) goto sendoutputbuffer; //Don't finish connecting yet! We're requiring an active PADS packet to have been received(PPPOE connection setup)!
								}
								Packetserver_clients[connectedclient].packetserver_delay = (DOUBLE)0; //Finish the delay!
								if (writefifobuffer(modem.outputbuffer[connectedclient], Packetserver_clients[connectedclient].packetserver_stage_str[Packetserver_clients[connectedclient].packetserver_stage_byte])) //Transmitted?
								{
									if (++Packetserver_clients[connectedclient].packetserver_stage_byte == safestrlen(Packetserver_clients[connectedclient].packetserver_stage_str, sizeof(Packetserver_clients[connectedclient].packetserver_stage_str))) //Finished?
									{
										Packetserver_clients[connectedclient].packetserver_stage = PACKETSTAGE_SLIPDELAY;
										Packetserver_clients[connectedclient].packetserver_delay = PACKETSERVER_SLIP_DELAY; //Delay this much!
										Packetserver_clients[connectedclient].packetserver_stage_byte = PACKETSTAGE_INITIALIZING; //Prepare for next step!
									}
								}
							}
						}

						if (Packetserver_clients[connectedclient].packetserver_stage == PACKETSTAGE_SLIPDELAY) //Delay before starting SLIP communications?
						{
							Packetserver_clients[connectedclient].packetserver_delay -= timepassed; //Delaying!
							if ((Packetserver_clients[connectedclient].packetserver_delay <= 0.0) || (!Packetserver_clients[connectedclient].packetserver_delay)) //Finished?
							{
								Packetserver_clients[connectedclient].packetserver_delay = (DOUBLE)0; //Finish the delay!
								Packetserver_clients[connectedclient].packetserver_stage_byte = PACKETSTAGE_INITIALIZING; //Prepare for next step!
								Packetserver_clients[connectedclient].packetserver_stage = PACKETSTAGE_PACKETS; //Start the SLIP service!
							}
						}
					}
				}

			sendoutputbuffer:
				if ((modem.connected == 1) && (modem.connectionid>=0)) //Normal connection?
				{
					if (peekfifobuffer(modem.outputbuffer[0], &datatotransmit)) //Byte available to send?
					{
						switch (TCP_SendData(modem.connectionid, datatotransmit)) //Send the data?
						{
						case 0: //Failed to send?
							modem.connected = 0; //Not connected anymore!
							if (PacketServer_running == 0) //Not running a packet server?
							{
								TCP_DisconnectClientServer(modem.connectionid); //Disconnect!
								modem.connectionid = -1;
								fifobuffer_clear(modem.inputdatabuffer[0]); //Clear the output buffer for the next client!
								fifobuffer_clear(modem.outputbuffer[0]); //Clear the output buffer for the next client!
								modem.connected = 0; //Not connected anymore!
								modem_responseResult(MODEMRESULT_NOCARRIER);
								modem.datamode = 0; //Drop out of data mode!
								modem.ringing = 0; //Not ringing anymore!
							}
							else //Disconnect from packet server?
							{
								terminatePacketServer(modem.connectionid); //Clean up the packet server!
								fifobuffer_clear(modem.inputdatabuffer[0]); //Clear the output buffer for the next client!
								fifobuffer_clear(modem.outputbuffer[0]); //Clear the output buffer for the next client!
							}
							//goto skiptransfers;
							break; //Abort!
						case 1: //Sent?
							readfifobuffer(modem.outputbuffer[0], &datatotransmit); //We're send!
							break;
						default: //Unknown function?
							break;
						}
					}
					if (fifobuffer_freesize(modem.inputdatabuffer[0])) //Free to receive?
					{
						switch (TCP_ReceiveData(modem.connectionid, &datatotransmit))
						{
						case 0: //Nothing received?
							break;
						case 1: //Something received?
							writefifobuffer(modem.inputdatabuffer[0], datatotransmit); //Add the transmitted data to the input buffer!
							break;
						case -1: //Disconnected?
							modem.connected = 0; //Not connected anymore!
							if (PacketServer_running == 0) //Not running a packet server?
							{
								TCP_DisconnectClientServer(modem.connectionid); //Disconnect!
								modem.connectionid = -1;
								fifobuffer_clear(modem.inputdatabuffer[0]); //Clear the output buffer for the next client!
								fifobuffer_clear(modem.outputbuffer[0]); //Clear the output buffer for the next client!
								modem.connected = 0; //Not connected anymore!
								modem_responseResult(MODEMRESULT_NOCARRIER);
								modem.datamode = 0; //Drop out of data mode!
								modem.ringing = 0; //Not ringing anymore!
							}
							else //Disconnect from packet server?
							{
								terminatePacketServer(modem.connectionid); //Clean up the packet server!
								fifobuffer_clear(modem.inputdatabuffer[0]); //Clear the output buffer for the next client!
								fifobuffer_clear(modem.outputbuffer[0]); //Clear the output buffer for the next client!
							}
							break;
						default: //Unknown function?
							break;
						}
					}
				}
				//Next, process the connected clients!
				else if (modem.connected == 2) //SLIP server connection is active?
				{
					for (connectedclient = 0; connectedclient < Packetserver_totalClients; ++connectedclient) //Check all connected clients!
					{
						if (Packetserver_clients[connectedclient].used == 0) continue; //Skip unused clients!
						if (peekfifobuffer(modem.outputbuffer[connectedclient], &datatotransmit)) //Byte available to send?
						{
							switch (TCP_SendData(Packetserver_clients[connectedclient].connectionid, datatotransmit)) //Send the data?
							{
							case 0: //Failed to send?
							packetserver_autherror: //Packet server authentication error?
								if (PacketServer_running == 0) //Not running a packet server?
								{
									PPPOE_finishdiscovery(connectedclient); //Finish discovery, if needed!
									TCP_DisconnectClientServer(modem.connectionid); //Disconnect!
									modem.connectionid = -1;
									fifobuffer_clear(modem.inputdatabuffer[connectedclient]); //Clear the output buffer for the next client!
									fifobuffer_clear(modem.outputbuffer[connectedclient]); //Clear the output buffer for the next client!
									modem.connected = 0; //Not connected anymore!
									modem_responseResult(MODEMRESULT_NOCARRIER);
									modem.datamode = 0; //Drop out of data mode!
									modem.ringing = 0; //Not ringing anymore!
								}
								else //Disconnect from packet server?
								{
									PPPOE_finishdiscovery(connectedclient); //Finish discovery, if needed!
									TCP_DisconnectClientServer(Packetserver_clients[connectedclient].connectionid); //Clean up the packet server!
									Packetserver_clients[connectedclient].connectionid = -1; //Not connected!
									terminatePacketServer(connectedclient); //Stop the packet server, if used!
									freePacketserver_client(connectedclient); //Free the client list item!
									fifobuffer_clear(modem.inputdatabuffer[connectedclient]); //Clear the output buffer for the next client!
									fifobuffer_clear(modem.outputbuffer[connectedclient]); //Clear the output buffer for the next client!
									if (Packetserver_availableClients == Packetserver_totalClients) //All cleared?
									{
										modem.connected = 0; //Not connected anymore!
									}
								}
								//goto skiptransfers;
								break; //Abort!
							case 1: //Sent?
								readfifobuffer(modem.outputbuffer[connectedclient], &datatotransmit); //We're send!
								break;
							default: //Unknown function?
								break;
							}
						}
						if (fifobuffer_freesize(modem.inputdatabuffer[connectedclient])) //Free to receive?
						{
							switch (TCP_ReceiveData(Packetserver_clients[connectedclient].connectionid, &datatotransmit))
							{
							case 0: //Nothing received?
								break;
							case 1: //Something received?
								writefifobuffer(modem.inputdatabuffer[connectedclient], datatotransmit); //Add the transmitted data to the input buffer!
								break;
							case -1: //Disconnected?
								if (PacketServer_running == 0) //Not running a packet server?
								{
									TCP_DisconnectClientServer(modem.connectionid); //Disconnect!
									modem.connectionid = -1;
									fifobuffer_clear(modem.inputdatabuffer[connectedclient]); //Clear the output buffer for the next client!
									fifobuffer_clear(modem.outputbuffer[connectedclient]); //Clear the output buffer for the next client!
									modem.connected = 0; //Not connected anymore!
									modem_responseResult(MODEMRESULT_NOCARRIER);
									modem.datamode = 0; //Drop out of data mode!
									modem.ringing = 0; //Not ringing anymore!
								}
								else //Disconnect from packet server?
								{
									PPPOE_finishdiscovery(connectedclient); //Finish discovery, if needed!
									terminatePacketServer(Packetserver_clients[connectedclient].connectionid); //Clean up the packet server!
									freePacketserver_client(connectedclient); //Free the client list item!
									fifobuffer_clear(modem.inputdatabuffer[connectedclient]); //Clear the output buffer for the next client!
									fifobuffer_clear(modem.outputbuffer[connectedclient]); //Clear the output buffer for the next client!
									if (Packetserver_availableClients == Packetserver_totalClients) //All cleared?
									{
										modem.connected = 0; //Not connected anymore!
									}
								}
								break;
							default: //Unknown function?
								break;
							}
						}
					}
				}
			} //Connected?

			//skiptransfers: //For disconnects!
			if (net.packet) //Packet received? Discard anything we receive now for other users!
			{
				freez((void **)&net.packet, net.pktlen, "MODEM_PACKET");
				net.packet = NULL; //Discard if failed to deallocate!
				net.pktlen = 0; //Not allocated!
			}

			fetchpackets_pcap(); //Handle any packets that need fetching!
		} //While polling?
	} //To poll?
}
