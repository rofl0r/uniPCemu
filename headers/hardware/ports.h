#ifndef PORTS_H
#define PORTS_H

#include "headers/types.h" //Basic types!

//Undefined result!
#define PORT_UNDEFINED_RESULT ~0

typedef byte (*PORTIN)(word port, byte *result);    /* A pointer to a PORT IN function (byte sized). Result is 1 on success, 0 on not mapped. */
typedef byte (*PORTOUT)(word port, byte value);    /* A pointer to a PORT OUT function (byte sized). Result is 1 on success, 0 on not mapped. */

void Ports_Init(); //Initialises the ports!

//CPU stuff!
byte PORT_IN_B(word port); //Gives result of IN command (byte)
word PORT_IN_W(word port); //Gives result of IN command (word)
void PORT_OUT_B(word port, byte b); //Executes OUT port,b
void PORT_OUT_W(word port, word w); //Executes OUT port,w

//Internal stuff (cpu/ports.c):

void reset_ports(); //Reset ports to none!
void register_PORTOUT(PORTOUT handler); //Set PORT OUT function handler!
void register_PORTIN(PORTIN handler); //Set PORT IN function handler!
byte EXEC_PORTOUT(word port, byte value); //PORT OUT Byte!
byte EXEC_PORTIN(word port, byte *result); //PORT IN Byte!
#endif