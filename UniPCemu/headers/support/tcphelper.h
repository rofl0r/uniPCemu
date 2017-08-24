#ifndef TCPHELPER_H
#define TCPHELPER_H

#include "headers/types.h" //Basic types!
#include "headers/support/fifobuffer.h" //FIFO buffer for transferred data!

//Disable below to disable TCP/IP emulation for the modem.
#define TCP_EMULATION

//Based on http://stephenmeier.net/2015/12/23/sdl-2-0-tutorial-04-networking/

//Packet/socket information! We're sending back and forth byte quantities!
#define MAX_PACKET 1
#define MAX_SOCKETS 1
 
//-----------------------------------------------------------------------------
#define WOOD_WAIT_TIME 5000
 
//-----------------------------------------------------------------------------
//Termination requested from the client!
#define FLAG_QUIT 0x0000

//The server and client keep switching roles. This is done by the client(request to send data, request to receive data).

//Server->Client transactions!
//Client: Request if we have data to (not be able to) be send by the server. This is the very first stage of the two-way transfer. When the NOTSENDDATA is sent, nothing can be received(buffers full).
#define CLIENT_REQUEST_SENDDATA 0x0010
#define CLIENT_REQUEST_NOTSENDDATA 0x0011
//Server: We're sending data back to the client to receive. Reverse client-server roles.
#define SERVER_SENDINGDATA 0x0012
//Server: We're not sending data to the client, nor do we have anything to send. Reverse client-server roles.
#define SERVER_NOTSENDINGDATA 0x0013

//Client->Server transactions, which is switched to by the client and back after each transaction 50-50 up/down transfer!
//Client: Request data to be sent to the server.
#define CLIENT_REQUEST_RECEIVEDATA 0x0013
//Server: The data has been received by the server. Reverse client-server roles.
#define SERVER_RECEIVEDDATA 0x0014
//Server: We're not receiving data from the client(buffers full). Reverse client-server roles.
#define SERVER_NOTRECEIVEDDATA 0x0015


typedef struct {
	int in_use;
	//Other data we store about the client, e.g. received data etc.
	FIFOBUFFER *receiveddata; //Received data from the client!
	FIFOBUFFER *sentdata; //Data to be sent to the client!
} Client;

void initTCP(); //Initialize us!
void doneTCP(); //Finish us!

#endif