#ifndef PORTS_H
#define PORTS_H

#include "headers/types.h" //Basic types!

//Undefined result!
#define PORT_UNDEFINED_RESULT ~0

typedef byte (*PORTIN)(word port, byte *result);    /* A pointer to a PORT IN function (byte sized). Result is 1 on success, 0 on not mapped. */
typedef byte (*PORTOUT)(word port, byte value);    /* A pointer to a PORT OUT function (byte sized). Result is 1 on success, 0 on not mapped. */
typedef byte(*PORTINW)(word port, word *result);    /* A pointer to a PORT IN function (word sized). Result is 1 on success, 0 on not mapped. */
typedef byte(*PORTOUTW)(word port, word value);    /* A pointer to a PORT OUT function (word sized). Result is 1 on success, 0 on not mapped. */
typedef byte(*PORTIND)(word port, uint_32 *result);    /* A pointer to a PORT IN function (word sized). Result is 1 on success, 0 on not mapped. */
typedef byte(*PORTOUTD)(word port, uint_32 value);    /* A pointer to a PORT OUT function (word sized). Result is 1 on success, 0 on not mapped. */

void Ports_Init(); //Initialises the ports!

//CPU stuff!
byte PORT_IN_B(word port); //Gives result of IN command (byte)
word PORT_IN_W(word port); //Gives result of IN command (word)
uint_32 PORT_IN_D(word port); //Gives result of IN command (word)
void PORT_OUT_B(word port, byte b); //Executes OUT port,b
void PORT_OUT_W(word port, word w); //Executes OUT port,w
void PORT_OUT_D(word port, uint_32 w); //Executes OUT port,w

//Internal stuff (cpu/ports.c):

void reset_ports(); //Reset ports to none!
void register_PORTOUT(PORTOUT handler); //Set PORT OUT function handler!
void register_PORTIN(PORTIN handler); //Set PORT IN function handler!
void register_PORTOUTW(PORTOUTW handler); //Set PORT OUT function handler!
void register_PORTINW(PORTINW handler); //Set PORT IN function handler!
void register_PORTOUTD(PORTOUTD handler); //Set PORT OUT function handler!
void register_PORTIND(PORTIND handler); //Set PORT IN function handler!
byte EXEC_PORTOUT(word port, byte value); //PORT OUT Byte!
byte EXEC_PORTIN(word port, byte *result); //PORT IN Byte!
byte EXEC_PORTOUTW(word port, word value); //PORT OUT Byte!
byte EXEC_PORTINW(word port, word *result); //PORT IN Byte!
byte EXEC_PORTOUTD(word port, uint_32 value); //PORT OUT Byte!
byte EXEC_PORTIND(word port, uint_32 *result); //PORT IN Byte!
#endif