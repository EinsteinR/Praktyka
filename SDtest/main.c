#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <compat/twi.h>
#include <inttypes.h>
#include <stdio.h>

#define xtalCpu 8000000L
//
//#define CS (1<<PB2)
//#define MOSI (1<<PB3)
//#define MISO (1<<PB4)
//#define SCK (1<<PB5)
//#define CS_DDR DDRB
//#define CS_ENABLE() (PORTB &= ~CS)
//#define CS_DISABLE() (PORTB |= CS)

//#define F_CPU 8000000UL  // 8 MHz
//#define USART_BAUDRATE 9600
//#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

#include "uart.h"

volatile uint16_t TDelay = 100; //Zmienna obs�ugiwna w przerwaniach
char * komunikat = "Test zapisania na karcie SD";
volatile char  wartosc[4];
volatile uint8_t i = 0;
volatile uint8_t ii = 0;

volatile uint16_t time = 1;
volatile uint16_t speed = 100;



/*
void strSend(char* msg)
{
	uint8_t j=0;
		while(*(msg + j) != 0)
		{
			uartSend(*(msg + j));
			j++;
		}
		uartSend('\r');
		uartSend('\n');
}

void uartSend(uint8_t u8Data)
{
	while((UCSRA &(1<<UDRE)) == 0);
	UDR = u8Data;
}*/

/* ISR(USART_RXC_vect) //Odczyt z UART
{
	//przyjmowanie jednej liczby
	uint8_t dane = UDR;
	if(dane >= '0' && dane <= '9')
			time = dane - 48;
	if (dane == 'A')
		speed = speed - 5;
	if (dane == 'B')
		speed = speed + 1;

	OCR1A = speed;
		itoa(speed, wartosc, 10);
		strSend(wartosc);
}
*/
ISR(INT0_vect) // Interrupt PD2
{
////	i=0;
////		while(*(komunikat + i) != 0)
////		{
////			uartSend(*(komunikat + i));
////			i++;
////		}
////	uartSend('\r');
////	uartSend('\n');
	uart0_puts("nooo!");
	uart0_flush();
}




int main(void){

//	DDRD |= (1<<PD2);
//	PORTD |= (1<<PD2);

	cli(); // blokowanie przerwa� CLEAR INTERRUPTS
//	GICR |=(1<<INT0); //uaktywnienie INT0 w rejestrz GICR s.46 datasheet
//	MCUCR |=(1<<ISC01); //ustawienie INT0 na zbocze opadajace s.65 datasheet
	//USART_Init(); //Inicjalizacja komunikacji USART
	uart0_init(9600);

	sei(); // odblokowanie przerwan globalnych SET INTERRUPTS


	while(1){
		uart0_putc('R');
		uart0_flush();
		_delay_ms(100);
	}
return 0;
}

