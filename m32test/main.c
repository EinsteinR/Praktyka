#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <avr/interrupt.h>

volatile char * komunikat = "Test!\r\n";
volatile uint8_t i;

#define F_CPU 8000000UL  // 8 MHz
#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

void uartSend(uint8_t);

void USART_Init(void)
{
	UCSRA &= ~(1<<U2X); //Rejest ustawienia preskalera do obliczania BAUD
	UBRRL = (unsigned char)BAUD_PRESCALE; //wsp�czynnik do okreslenia predkosci transmisji 9600 kb/s (UBBR=f_sys/(16*BAUD)-1)
	UBRRH = (BAUD_PRESCALE >> 8);
	UCSRC &= ~(1<<UMSEL); //Draca asynchroniczna
	UCSRB |= ((1<<RXEN) | (1<<TXEN)); //wlaczone moduly nadawcze i odbiorcze
	UCSRB |= (1<<RXCIE); //Umozliwia wyzwolenia przerwan przy odbiorze
}

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
}

ISR(INT0_vect) // Interrupt PD2
{
i=0;
	while(*(komunikat + i) != 0)
	{
		uartSend(*(komunikat + i));
		i++;
	}
}

int main(void)
{

	DDRD &=~(1<<PD2);// Set INT0 pin
	PORTD |=(1<<PD2);// Set pin INT0 high

	cli(); // blokowanie przerwa� CLEAR INTERRUPTS
	GICR |=(1<<INT0); //uaktywnienie INT0 w rejestrz GICR s.46 datasheet
	MCUCR |=(1<<ISC01); //ustawienie INT0 na zbocze opadajace s.65 datasheet
	USART_Init(); //Inicjalizacja komunikacji USART
	sei(); // odblokowanie przerwan globalnych SET INTERRUPTS

	while(1)
	{

	}
	return 0;
}
