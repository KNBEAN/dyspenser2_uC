/*
 author: Mikolaj Arciszewski
 version: 0.01a
 */

/* komendy:
 * - rozpoczecie nalewania
 * MASTER:
 * #a%u\n	- %u - zadana objetosc [ml]
 *
 * SLAVE:
 * #a0\n	- ok
 * #a1\n	- blad
 *
 *
 * - zapytanie o stan nalewania
 * MASTER:
 * #b\n
 *
 * SLAVE:
 * #b%u\n	%u - nalana objetosc [ml]
 *
 * - zatrzymanie nalewania
 * MASTER:
 * #c\n
 *
 * SLAVE:
 * #c0\n	- ok
 * #c1\n	- blad
 *
 *
 * - brak wody
 * SLAVE:
 * #d1\n
 *
 * - po prostu blad
 * SLAVE:
 * #e1\n
 */

#ifndef F_CPU
#define F_CPU 4000000L
#endif
//TODO: ustawic fusy

#define VALVEPORT	PORTD
#define VALVEPIN	2

#define FLOWREADFREQ 0.5 //[s]
#define MINFLOW 17 //[ml/s]
#define MAXVOLUME 300	//przykladowe wartosci
#define MINVOLUME 25

#define FRAMEFIRSTCHAR '#'

#include <stddef.h>
#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <avr/sleep.h>

#define _sbi(port, pin) (port) |= (1 << pin)
#define _cbi(port, pin) (port) &= (~(1 << pin))

volatile unsigned char usartDataRecievedFlag;

volatile uint16_t waterVolumePassed; //[ml]
volatile uint16_t totalWaterVolumePassed;
volatile uint16_t waterVolumeSet;
volatile uint8_t flowrate; // [ml/s]
volatile uint8_t flowsensorTimeoutCounter;

volatile uint8_t lowFlowWarningFlag;
volatile uint8_t ongoingPouringFlag;

volatile unsigned int usartTxBufferInd;
volatile unsigned int usartRxBufferInd;
char usartTxBuffer[30];
char usartRxBuffer[30];

void usartInit(void)
{
	// ustawienie transmisji
#define BAUD 9600        //9600bps standardowa predkosc transmisji modulu HC-05
#include <util/setbaud.h> //linkowanie tego pliku musi byæ
	//po zdefiniowaniu BAUD

	//ustaw obliczone przez #define wartoœci
	UBRRH = UBRRH_VALUE;
	UBRRL = UBRRL_VALUE;
#if USE_2X
	UCSRA |= (1 << U2X);
#else
	UCSRA &= ~(1 << U2X);
#endif

	//Ustawiamy pozostale parametry modul USART

	//standardowe parametry transmisji modulu HC-05
	UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);  //bitów danych: 8
														 //bity stopu:  1
														 //parzystoœæ:  brak
	//wlacz nadajnik i odbiornik oraz ich przerwania odbiornika
	//przerwania nadajnika wlaczamy w funkcji wyslij_wynik()
	UCSRB = (1 << TXEN) | (1 << RXEN) | (1 << RXCIE);
}

inline void flowsensorInit()
{
	_sbi(PORTD, 4); //pull-up wejscia
	//Timer 0 zlicza impulsy, timer 1 wywoluje przerwania co FLOWREADFREQ sekund
	TCCR0 = _BV(CS02) | _BV(CS01); //External clock source on T0 pin. Clock on falling edge.
	TCCR1B = _BV(WGM12) | _BV(CS12);		   //CTC, prescaler /256
	OCR1A = F_CPU * FLOWREADFREQ / 256;
}

inline void flowsensorStart()
{
	TCNT0 = 0;
	_sbi(TCCR1B, CS12);
	TIMSK = _BV(OCIE1A);
}

inline void flowsensorStop()
{
	TIMSK = 0;
	_cbi(TCCR1B, CS12);
}

inline void openValve()
{
	_sbi(VALVEPORT, VALVEPIN);
}

inline void closeValve()
{
	_cbi(VALVEPORT, VALVEPIN);
}

inline void startPouring()
{
	ongoingPouringFlag = 1;
	waterVolumePassed = 0;
	flowsensorStart();
	openValve();
}

inline void stopPouring()
{
	ongoingPouringFlag = 0;
	closeValve();
}

inline void usartTransmit()
//rozpoczyna przesylanie zawartosci usartTxBuffer
{
	usartTxBufferInd = 0;

	//wlacz przerwania pustego bufora UDR, co rozpocznie transmisje
	//aktualnej zawartoœci bufora
	UCSRB |= (1 << UDRIE);
}

inline void usartRxReset()
// ponownie uruchamia odbiornik
{
	//zresetuj bufor
	usartRxBufferInd = 0;
	//wlacza odbiornik
	_sbi(UCSRB, RXEN);
}

void sendMsg(char function, uint8_t data)
// wysylanie ramki do mastera
{
	//Zaczekaj, az bufor nadawania bedzie pusty
	while (!(UCSRA & (1 << UDRE)))
		;

	//zapisz ramke w buforze
	sprintf(usartTxBuffer, "#%c%u", function, data);

	usartTransmit();
}

uint8_t checkIfFilled()
{
	if (waterVolumePassed > waterVolumeSet)
		return 1;
	else
		return 0;
}

void startPouringCmdRecieved(int data)
// przetwarzanie komendy rozpoczecia nalewania
{
	if (ongoingPouringFlag)
	{
		//nalewanie juz trwa
		sendMsg('a', 1);
		return;
	}

	waterVolumeSet = data;

	if (waterVolumeSet > MAXVOLUME || waterVolumeSet < MINVOLUME)
	{
		//zla wartosc nastawy, nie nalewamy
		sendMsg('a', 1);
		return;
	}

	startPouring();
	//potwierdzenie rozpoczecia nalewania
	sendMsg('a', 0);
}

void progressQueryRecieved()
// przetwarzanie komendy zapytania o nalana objetosc
{
	sendMsg('b', waterVolumePassed);
}

void stopPouringCmdRecieved()
// przetwarzanie komendy polecenia zatrzymania
{
	if (ongoingPouringFlag)
	{
		stopPouring();
		//potwierdzenie zatrzymania nalewania
		sendMsg('c', 0);
	}
	else
	{
		//nie nalewa sie, wiec nie zatrzymano
		sendMsg('c', 1);
	}
}

uint8_t recievedCmdProcessing()
// 0 - pomyslnie przetworzono komende mastera
// 1 - blad ramki
{
	usartDataRecievedFlag = 0;
	uint8_t err = 0;

	//znajdz znak poczatkowy
	char *firstChar = strchr(usartTxBuffer, FRAMEFIRSTCHAR);
	if (firstChar == NULL)
	{
		usartRxReset();
		return 1;
	}

	//kod funkcji
	char functionId = *(firstChar + sizeof(char));

	//dane
	char *functionDataString = firstChar + 2 * sizeof(char);
	uint16_t data = atoi(functionDataString);

	switch (functionId)
	{
	case 'a':
		startPouringCmdRecieved(data);
		break;

	case 'b':
		progressQueryRecieved();
		break;

	case 'c':
		stopPouringCmdRecieved();
		break;

	default:
		//error
		sendMsg('e', 1);
	}

	usartRxReset();
	return err;
}

ISR(USART_RXC_vect)
{
	//przerwanie generowane po odebraniu bajtu
	usartRxBuffer[usartRxBufferInd] = UDR;
	if (usartRxBuffer[usartRxBufferInd] != '\n')
	{
		if (usartRxBufferInd == 29)
		{
			usartRxBufferInd = 0; //bufor przepelniony, splukuje i czeka na '\n'
		}
		else
		{
			++usartRxBufferInd;
		}
	}
	else
	{
		//koniec danych
		usartRxBuffer[usartRxBufferInd] = '\0'; //zastap \n przez znak konca stringa
		usartDataRecievedFlag = 1; //gotowosc do odczytu bufora
		_cbi(UCSRB, RXEN);	//wylacz odbiornik do momentu odeslania danych
	}

}

ISR(USART_UDRE_vect)
//przerwanie generowane, gdy bufor nadawania jest juz pusty,
//odpowiedzialne za wyslanie wszystkich znaków z tablicy usart_Buffer[]
{
	UDR = usartTxBuffer[usartTxBufferInd];
	//sprawdzamy, wyslany bajt jest znakiem konca ramki, czyli \n
	if (usartTxBuffer[usartTxBufferInd] != '\n')
	{
		++usartTxBufferInd;
	}
	else
	{
		//osiagnieto koniec napisu w tablicy usart_Buffer[]
		UCSRB &= ~(1 << UDRIE); //wylacz przerwania pustego bufora nadawania
	}
}

ISR(TIMER1_COMPA_vect)
//przerwanie generowane w celu pomiaru przeplywu
{
	uint8_t impulseCount = TCNT0;
	TCNT0 = 0;
	waterVolumePassed += impulseCount * 2;
	totalWaterVolumePassed += impulseCount * 2;
	flowrate = impulseCount; // 1 impuls ~ 2 ml / 0.5 s

	if (checkIfFilled() && ongoingPouringFlag)
	{
		//jesli przekroczono docelowa objetosc, zatrzymuje nalewanie
		stopPouring();
	}

	if (flowrate < MINFLOW)
	{
		lowFlowWarningFlag = 1;
		++flowsensorTimeoutCounter;
	}
	else
	{
		lowFlowWarningFlag = 0;
		flowsensorTimeoutCounter = 0;
	}

	if (lowFlowWarningFlag > 10)
	{
		//nie wykryto przeplywu przez 10 * FLOWREADFREQ sekund
		if (ongoingPouringFlag)
		{
			//nie leje sie, chociaz powinno - brak wody
			stopPouring();
			sendMsg('d', 1);
		}
		// else - nie leje sie bo zamknieto zawor
		flowsensorStop();
	}
}

int main(void)
{
	usartInit(); //inicjuj modul USART (RS-232)
	sei();
	//wlacz przerwania globalne
	DDRD = _BV(2);	//wyjscie do sterowania zaworu
	flowsensorInit();
	while (1)
	{

		//czekamy na informacje o odebraniu danych nie blokujac mikrokontrolera
		if (usartDataRecievedFlag)
		{
			//odczytuje ramke z bufora, w przypadku powodzenia wykonuje komende
			recievedCmdProcessing();
		}
	}
}
