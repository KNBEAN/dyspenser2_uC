/*
 * uart.h
 *
 * Created: 16-06-2017 16:23:36
 *  Author: Karol
 */ 


#ifndef UART_H_
#define UART_H_

#include <stdbool.h>
#include <avr/io.h>



void usartInit(void);
void setRxBuffer(char* buffer, uint16_t len);

bool uartTxIsBusy(void);

void sendLine(char *text);
void sendData(char *text);

bool recivedNewLine(void);



#endif /* UART_H_ */