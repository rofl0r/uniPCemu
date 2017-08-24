#include "headers/types.h" //Basic type support!
#include "headers/support/log.h" //Logging support!

#define NET_LOGFILE "net"

#if defined(SDL_NET) || defined(SDL2_NET)
#define GOTNET
#endif

//Some server port to use when listening and sending data packets.
word TCP_PORT = 8099; //What port to operate on for our server/client structure?

#ifdef GOTNET
int next_ind = 0;
TCPsocket server_socket;
Client clients;
SDLNet_SocketSet socket_set;
TCPsocket sockets;
#endif

byte NET_READY = 0; //Are we ready to be used?
byte Server_READY = 0; //Server ready for use?
byte Client_READY = 0; //Client ready for use?

void initTCP() //Initialize us!
{
#ifdef GOTNET
	if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_EVENTS) != 0) {
		dolog(NET_LOGFILE, "ER: SDL_Init: %s\n", SDL_GetError());
		NET_READY  = 0; //Not ready!
		return;
	}
 
	if(SDLNet_Init() == -1) {
		dolog(NET_LOGFILE, "ER: SDLNet_Init: %s\n", SDLNet_GetError());
		NET_READY = 0; //Not ready!
		return;
	}
#endif
	//Initialize buffers for the server, as well as for the client, if used!
	/*
	byte client;
	for (client=0;client<;)
	*/
}

byte TCP_ConnectServer()
{
#ifdef GOTNET
	Server_READY = 0; //Not ready by default!
	if (!NET_READY)
	{
		return 0; //Fail automatically!
	}
	IPaddress ip;
	if(SDLNet_ResolveHost(&ip, NULL, SERVER_PORT) == -1) {
		//fprintf(stderr, "ER: SDLNet_ResolveHost: %s\n", SDLNet_GetError());
		return 0; //Failed!
	}
 
	server_socket = SDLNet_TCP_Open(&ip);
	if(server_socket == NULL) {
		//fprintf(stderr, "ER: SDLNet_TCP_Open: %s\n", SDLNet_GetError());
		return 0; //Failed!
	}

	Server_READY = 1; //First step successful!

	socket_set = SDLNet_AllocSocketSet(MAX_SOCKETS+1);
	if(socket_set == NULL) {
		//fprintf(stderr, "ER: SDLNet_AllocSocketSet: %s\n", SDLNet_GetError());
		return 0; //Failed
	}

	Server_READY  = 2; //Second step successful!
 
	if(SDLNet_TCP_AddSocket(socket_set, server_socket) == -1) {
		//fprintf(stderr, "ER: SDLNet_TCP_AddSocket: %s\n", SDLNet_GetError());
		return 0; //Failed
	}
	Server_READY = 3; //We're ready for use!
	//Clear the server I/O buffers!
	return 1; //Connected!
#else
return 0; //Cannot connect!
#endif
}

void updateTCPServer() //Update anything needed on the TCP server!
{
#ifdef GOTNET
	if (request_TCPServer_quit) return; //We're requesting to stop!
	if (!NET_READY || (Server_READY<3)) return; //Abort when not running properly!
	int num_rdy = SDLNet_CheckSockets(socket_set, 0); //Don't block while checking!
 
	if(num_rdy >= 0) {
		// NOTE: some number of the sockets are ready
		int ind;
		for(ind=0; ind<MAX_SOCKETS; ++ind) {
			if(!clients[ind].in_use) continue;
		}

		if(SDLNet_SocketReady(server_socket)) {
			int got_socket = AcceptSocket(next_ind);
			if(!got_socket) {
				num_rdy--;
				continue;
			}
 
			// NOTE: get a new index
			int chk_count;
			for(chk_count=0; chk_count<MAX_SOCKETS; ++chk_count) {
				if(sockets[(next_ind+chk_count)%MAX_SOCKETS] == NULL) break;
			}
 
			next_ind = (next_ind+chk_count)%MAX_SOCKETS;
			//printf("DB: new connection (next_ind = %d)\n", next_ind);
 
			num_rdy--;
		}

		int ind;
		for(ind=0; (ind<MAX_SOCKETS) && num_rdy; ++ind) {
			if(sockets[ind] == NULL) continue;
			if(!SDLNet_SocketReady(sockets[ind])) continue;
 
			uint8_t* data;
			uint16_t flag;
			uint16_t length;
				
			data = TCPServer_RecvData(ind, &length, &flag);
			if(data == NULL) {
				num_rdy--;
				continue;
			}
 
			switch(flag) { //What is requested by the client?
				case FLAG_REQUEST_SENDDATA: { //Client: Request if we have data to be send by the server.
					uint8_t send_data[MAX_PACKET];
					
					if (readfifobuffer(clients[ind].sentdata,&send_data[0]) //Data to be send?
					{
						TCPServer_SendData(ind, send_data, 0, FLAG_SERVER_SENDINGDATA); //We're sending data now!
					}
					else
					{
						TCPServer_SendData(ind, send_data, 0, FLAG_SERVER_NOTSENDINGDATA); //We're not sending data now!
					}
				} break;
				case FLAG_REQUEST_NOTSENDDATA: { //Client: Request if we not have data to be send by the server.
					uint8_t send_data[MAX_PACKET];
					
					TCPServer_SendData(ind, send_data, 0, FLAG_SERVER_NOTSENDINGDATA); //We're not sending data now!
				} break;
 
				case FLAG_QUIT: { //Termination of connection is requested!
					request_TCPServer_quit = 1; //We're requested to terminate the connection!
					//printf("DB: shutdown by client id: %d\n", ind);
				} break;
			}
 
			free(data);
			num_rdy--;
		}
	}
#endif
}

uint8_t* TCPServer_RecvData(int index, uint16_t* length, uint16_t* flag) {
#ifdef GOTNET
	uint8_t temp_data[MAX_PACKET];
	int num_recv = SDLNet_TCP_Recv(sockets[index], temp_data, MAX_PACKET);
 
	if(num_recv <= 0) {
		CloseSocket(index);
		const char* err = SDLNet_GetError();
		if(strlen(err) == 0) {
			printf("DB: client disconnected\n");
		} else {
			fprintf(stderr, "ER: SDLNet_TCP_Recv: %s\n", err);
		}
 
		return NULL;
	} else {
		int offset = 0;
		*flag = *(uint16_t*) &temp_data[offset];
		offset += sizeof(uint16_t);
 
		*length = (num_recv-offset);
 
		uint8_t* data = (uint8_t*) malloc((num_recv-offset)*sizeof(uint8_t));
		memcpy(data, &temp_data[offset], (num_recv-offset));
 
		return data;
	}
#endif
	return NULL; //Nothing to receive!
}

void TCPServer_SendData(int index, uint8_t* data, uint16_t length, uint16_t flag) {
#ifdef GOTNET	
	uint8_t temp_data[MAX_PACKET];
 
	int offset = 0;
	memcpy(temp_data+offset, &flag, sizeof(uint16_t));
	offset += sizeof(uint16_t);
	memcpy(temp_data+offset, data, length);
	offset += length;
 
	int num_sent = SDLNet_TCP_Send(sockets[index], temp_data, offset);
	if(num_sent < offset) {
		//fprintf(stderr, "ER: SDLNet_TCP_Send: %s\n", SDLNet_GetError());
		CloseSocket(index);
	}
#endif
}

int AcceptSocket(int index) {
#ifdef GOTNET
	if(sockets[index]) {
		//fprintf(stderr, "ER: Overriding socket at index %d.\n", index);
		CloseSocket(index);
	}
 
	sockets[index] = SDLNet_TCP_Accept(server_socket);
	if(sockets[index] == NULL) return 0;
 
	clients[index].in_use = 1;
	if(SDLNet_TCP_AddSocket(socket_set, sockets[index]) == -1) {
		//fprintf(stderr, "ER: SDLNet_TCP_AddSocket: %s\n", SDLNet_GetError());
		//exit(-1);
		return 0; //Failed!
	}
 
	return 1;
#endif
	return 0; //Can't accept!
}

void stopTCPServer()
{
#ifdef GOTNET
	if (!NET_READY) return; //Abort when not running properly!
	if (Server_READY>=3) //Fully loaded?
	{
		if(SDLNet_TCP_DelSocket(socket_set, server_socket) == -1) {
			//fprintf(stderr, "ER: SDLNet_TCP_DelSocket: %s\n", SDLNet_GetError());
		}
		--Server_READY; //Layer destroyed!
	}
	if (Server_READY>=2) //Partially loaded?
	{
		SDLNet_TCP_Close(server_socket);
		--Server_READY; //Layer destroyed!
		int i;
		for(i=0; i<MAX_SOCKETS; ++i) {
			if(sockets[i] == NULL) continue;
			CloseSocket(i);
		}
 
		SDLNet_FreeSocketSet(socket_set);
	}
	Server_READY = 0; //Not ready anymore! 
#endif
}

void TCP_CloseSocket(int index) {
#ifdef GOTNET
	if(sockets[index] == NULL) {
		fprintf(stderr, "ER: Attempted to delete a NULL socket.\n");
		return;
	}
 
	if(SDLNet_TCP_DelSocket(socket_set, sockets[index]) == -1) {
		fprintf(stderr, "ER: SDLNet_TCP_DelSocket: %s\n", SDLNet_GetError());
		exit(-1);
	}
 
	memset(&clients[index], 0x00, sizeof(Client));
	SDLNet_TCP_Close(sockets[index]);
	sockets[index] = NULL;
#endif
}

void doneTCP() //Finish us!
{
#ifdef GOTNET
	SDLNet_Quit();
#endif
}