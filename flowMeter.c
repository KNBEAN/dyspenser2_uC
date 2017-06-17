/*
 * flowMeter.c
 *
 * Created: 16-06-2017 19:53:37
 *  Author: Karol
 */ 


#include "flowMeter.h"
#include <avr/interrupt.h>
#include <util/atomic.h>

#define FLOWMETER_PIN PIND4
#define FLOWMETER_PORT PORTD
#define FLOWMETER_DDR DDRD

#define FLOWREADFREQ 0.5


volatile uint16_t volume = 0;
volatile bool newSample = false;

void flowMeter_init(void){
	FLOWMETER_DDR &= ~(1<<FLOWMETER_PIN);
	//FLOWMETER_PORT |= (1<<FLOWMETER_PIN);	//pull-up wejscia
	
	TIMSK |= (1<<OCIE1A);
	//Timer 0 zlicza impulsy, timer 1 wywoluje przerwania co FLOWREADFREQ sekund
	TCCR0 |= ((1<<CS02)|(1<<CS01));	//External clock source on T0 pin. Clock on falling edge.

	TCCR1B |= (1<<WGM12);// | (1<<CS12);		   //CTC, prescaler /256
	OCR1A = 15625;//7812;//F_CPU * FLOWREADFREQ / 256;
	flowMeter_Stop();
}

void flowMeter_Start()
{
	TCNT0 = 0;
//	TCNT1 = 0;
	volume = 0;
	TCCR1B |= (1<<CS12);
}


void flowMeter_Stop()
{
	TCCR1B &= ~(1<<CS12);
}

uint16_t flowMeter_getVolume(void){
	
	uint16_t v  = 0;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
		newSample = false;
		v = volume;
	}
	return v;
}

bool flowMeter_isNew(void){
	
	bool v = false;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
		v = newSample;
	}
	return v;
}

ISR(TIMER1_COMPA_vect)
//przerwanie generowane w celu pomiaru przeplywu
{
	newSample = true;
	volume += TCNT0*3;
	TCNT0 = 0;
}