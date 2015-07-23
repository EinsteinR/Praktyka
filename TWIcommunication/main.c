#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

//#include "i2c.h"

#define F_CPU 8000000UL  // 8 MHz
#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

void USART_Init(void)
{
	UCSRA &= ~(1<<U2X); //Rejest ustawienia preskalera do obliczania BAUD
	UBRRL = (unsigned char)BAUD_PRESCALE; //wspó³czynnik do okreslenia predkosci transmisji 9600 kb/s (UBBR=f_sys/(16*BAUD)-1)
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
		//uartSend('\n');
}
void i2c_init(void)
{
  TWSR = 0;
  TWBR = (F_CPU / 100000UL - 16)/2;
  TWCR |= (1<<TWINT);
}

void uartSend(uint8_t u8Data)
{
	while((UCSRA &(1<<UDRE)) == 0);
	UDR = u8Data;
}



int main(void)
{
	USART_Init();
	i2c_init();
	while(1)
	{

		_delay_ms(500);
	}
	return 0;
}
