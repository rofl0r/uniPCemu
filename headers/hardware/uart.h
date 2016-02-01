#ifndef UART_H
#define UART_H

typedef void (*UART_setmodemcontrol)(byte control);
typedef byte(*UART_hasdata)();
typedef void (*UART_senddata)();
typedef byte(*UART_receivedata)();

void initUART(byte numports); //Init UART!
void UART_registerdevice(byte portnumber, UART_setmodemcontrol setmodemcontrol, UART_hasdata hasdata, UART_receivedata receivedata, UART_senddata senddata);
void UART_handleInputs(); //Handle all inputs given, if any!

#endif
