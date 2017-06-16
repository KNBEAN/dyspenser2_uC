/*
 * flowMeter.h
 *
 * Created: 16-06-2017 19:53:17
 *  Author: Karol
 */ 


#ifndef FLOWMETER_H_
#define FLOWMETER_H_

#include <avr/io.h>
#include <stdbool.h>

void flowMeter_init(void);
void flowMeter_Start();
void flowMeter_Stop();

bool flowMeter_isNew(void);

uint16_t flowMeter_getVolume(void);


#endif /* FLOWMETER_H_ */