/*
 * defines.h
 *
 * Created: 16-06-2017 18:57:01
 *  Author: Karol
 */ 


#ifndef DEFINES_H_
#define DEFINES_H_


#define MAX_VOLUME 300

#define VALVE_PIN	PINC5
#define VALVE_PORT	PORTC
#define VALVE_DDR	PORTC


//macro
#define valveInit() VALVE_DDR |= (1<<VALVE_PIN)
#define valveClose() VALVE_PORT |= (1<<VALVE_PIN)
#define valveOpen() VALVE_PORT &= ~(1<<VALVE_PIN)




#endif /* DEFINES_H_ */