/*
 author: Mikolaj Arciszewski
 */

#ifndef F_CPU
#define F_CPU 4000000L
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include "uart.h"
#include "defines.h"
#include "flowMeter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h>


char recivedBuffer[64]; 
char buffor2[64];

//prototypy funkcji statycznych

static void init(void); 
static void startWater(uint16_t volume);

int main(void){
	
	init();
			
	while(1){
			
		if(recivedNewLine() == true){
			int16_t volume = atoi(recivedBuffer);
			if(volume <= 0 || volume > MAX_VOLUME){
				sendLine("Err");
			}else{
				startWater(volume);
			}
		}
		
	}	
}

static void init(void){
	valveInit();
	valveClose();
	usartInit();
	setRxBuffer(recivedBuffer, sizeof recivedBuffer/sizeof recivedBuffer[0]);
	flowMeter_init();
	sei();
}

static void startWater(uint16_t volume){
	uint16_t currentVolume = 0;
	sprintf(buffor2, "%d", currentVolume);
	sendLine(buffor2);
	flowMeter_Start();
	valveOpen();
	while(currentVolume < volume){
	
		if(flowMeter_isNew() == true){
			currentVolume = flowMeter_getVolume();
			sprintf(buffor2, "%d", currentVolume);
			sendLine(buffor2);
		}
		
		if (recivedNewLine() == true)
		{
			if(strcmp(recivedBuffer, "Stop\r\n") == 0){
				break;
			}
		}		
		
	}
	valveClose();
	flowMeter_Stop();
	sendLine("End");
}


