#ifndef TCPHELPER_H
#define TCPHELPER_H

#include "headers/types.h" //Basic types!
#include "headers/support/fifobuffer.h" //FIFO buffer for transferred data!

//Based on http://stephenmeier.net/2015/12/23/sdl-2-0-tutorial-04-networking/

//General support for the backend.
void initTCP(); //Initialize us!
void doneTCP(void); //Finish us!

//Server side
byte TCP_ConnectServer(word port); //Try to connect as a server. 1 when successful, 0 otherwise(not ready).
byte TCPServerRunning(); //Is the server running?
byte acceptTCPServer(); //Accept and connect when asked of on the TCP server! 1=Connected to a client, 0=Not connected.
void stopTCPServer(); //Stop the TCP server!

//Client side(also used when the server is connected).
byte TCP_ConnectClient(const char *destination, word port); //Connect as a client!
byte TCP_SendData(byte data); //Send data to the other side(both from server and client).
sbyte TCP_ReceiveData(byte *result); //Receive data, if available. 0=No data, 1=Received data, -1=Not connected anymore!
byte TCP_DisconnectClientServer(); //Disconnect either the client or server, whatever state we're currently using.

void TCPServer_pause(); //Block all incoming connections until done!
void TCPServer_restart(); //Restart the server after being paused!

#endif