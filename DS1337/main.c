#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <compat/twi.h>
#include <inttypes.h>
#include <stdio.h>

#include "i2c.h"

#define F_CPU 8000000UL  // 8 MHz
#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)
#define ENG_SIG_TAB_SIZE 16

char * komunikat = "Test!";
volatile uint8_t i = 0;


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

ISR(INT0_vect) // Interrupt PD2
{
i=0;
	while(*(komunikat + i) != 0)
	{
		uart_Send(*(komunikat + i));
		i++;
	}
}

void beginTransmission(uint8_t devAddr, uint8_t isWriting)
{
	uint8_t readAddr = (devAddr << 1) + 1;
	uint8_t writeAddr = (devAddr << 1);
	i2c_start();
	if (isWriting)
		i2c_write(writeAddr);
	else
		i2c_write(readAddr);
}

void checkRegister(uint8_t devAddr, uint8_t regAddr)
{
	uint8_t buff;
	char ptr [16]="0xwwww";
	beginTransmission(devAddr, 1);
	i2c_write(regAddr);
	beginTransmission(devAddr, 0);
	buff = i2c_read(0);
	itoa(buff, ptr+2, 16);
	strSend(ptr);
	i2c_stop();
}
void setRegister(uint8_t devAddr, uint8_t regAddr, uint8_t data)
{
	beginTransmission(devAddr, 1);
	i2c_write(regAddr);
	i2c_write(data);
	i2c_stop();
}

int main(void){

	DDRD &=~(1<<PD2);// Set INT0 pin
	PORTD |=(1<<PD2);// Set pin INT0 high

	DDRB |= (1<<PB1);
	PORTB |=(1<<PB1);

	cli(); // blokowanie przerwa� CLEAR INTERRUPTS
	GICR |=(1<<INT0); //uaktywnienie INT0 w rejestrz GICR s.46 datasheet
	MCUCR |=(1<<ISC01); //ustawienie INT0 na zbocze opadajace s.65 datasheet
	i2c_init();
	USART_Init(); //Inicjalizacja komunikacji USART
	sei(); // odblokowanie przerwan globalnych SET INTERRUPTS

	uint8_t addr = 0x1D;
	uint8_t readAddr = (addr << 1) + 1;
	uint8_t writeAddr = (addr << 1);

	uint16_t cos = 18;
	char wynik[16];

	while(1){

		i2c_start();
		i2c_write(writeAddr);
		i2c_write(0x32);
		i2c_start();
		i2c_write(readAddr);
		for(i=0;i<5;i++)
		{
			cos = i2c_read(1);
			//cos =i;
			itoa(cos, wynik, 10);
			switch(i)
			{
			case 0: strSend("\rPozycja: X-"); strSend(wynik); break;
			case 2: strSend(" Y-"); strSend(wynik); break;
			case 4: strSend(" Z-"); strSend(wynik); break;
			}
		}
		i2c_read(0);
		i2c_stop();
		strSend("     \r");
		_delay_ms(10);
	}
return 0;
}
