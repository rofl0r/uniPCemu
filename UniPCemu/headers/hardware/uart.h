#ifndef UART_H
#define UART_H

typedef void (*UART_setmodemcontrol)(byte control);
typedef byte(*UART_hasdata)();
typedef void (*UART_senddata)(byte value);
typedef byte(*UART_receivedata)();

void initUART(byte numports); //Init UART!
void UART_registerdevice(byte portnumber, UART_setmodemcontrol setmodemcontrol, UART_hasdata hasdata, UART_receivedata receivedata, UART_senddata senddata);
void updateUART(double timepassed); //Update UART timing!

#endif
