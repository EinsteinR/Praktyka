#include <avr/io.h>
#include <avr/interrupt.h>
#include <inttypes.h>
#include <avr/pgmspace.h>
#include "fat16.h"
#include "partition.h"
#include "sd_raw.h"
#include "sd_raw_config.h"
//#include "uart.h"

#define F_CPU 8000000UL  // 8 MHz
#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)
#define ENG_SIG_TAB_SIZE 16



void uart_Send(uint8_t u8Data)
{
	while((UCSRA &(1<<UDRE)) == 0);
	UDR = u8Data;
}

void USART_Init(void)
{
	UBRRH = (unsigned char)(BAUD_PRESCALE>>8);
	UBRRL = (unsigned char)BAUD_PRESCALE;
	UCSRB = (1<<RXEN)|(1<<TXEN)|(1<<RXCIE);
	UCSRC = (1<<URSEL)|(1<<USBS)|(3<<UCSZ0);

}

void strSend(char* msg)
{
	uint8_t j=0;
		while(*(msg + j) != 0)
		{
			uart_Send(*(msg + j));
			j++;
		}
}

ISR(USART_RXC_vect)
{
	uint8_t dane = UDR;
	uart_Send(dane);
}

int main(void){

	DDRB |= (1<<PB0)|(1<<PB1);
	PORTB &= ~((1<<PB0)|(1<<PB1));

	cli(); // blokowanie przerwa� CLEAR INTERRUPTS
	USART_Init(); //Inicjalizacja komunikacji USART
	sei(); // odblokowanie przerwan globalnych SET INTERRUPTS

	if(sd_raw_init()) PORTB |= (1<<PB0); //niebieska
	if(sd_raw_available()) PORTB |= (1<<PB1); //czerwona

	while(1){
	}
return 0;
}
