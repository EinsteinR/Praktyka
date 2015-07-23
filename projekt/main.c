#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

#include "i2c.h"

#define F_CPU 8000000UL  // 1 MHz
#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

void uartSend(uint8_t);

void USART_Init(void)
{
	UCSRA &= ~(1<<U2X); //Rejest ustawienia preskalera do obliczania BAUD
	UBRRL = (unsigned char)BAUD_PRESCALE; //współczynnik do okreslenia predkosci transmisji 9600 kb/s (UBBR=f_sys/(16*BAUD)-1)
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

void uintToA (uint8_t num, char* ptr, uint8_t base)
{
	uint8_t i = 0;
	do
	{
		ptr[i] = 0x30 + (num % base);
		num = num / base;
		i++;
	}while(num != 0);
	ptr[i]='\0';
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
int main(void)
{
	USART_Init();
	i2c_init();
	uint8_t addr = 0x1D;
	uint8_t readAddr = (addr << 1) + 1;
	uint8_t writeAddr = (addr << 1);
	uint16_t cos = 18;
	uint8_t i;
	strSend("BW_RATE---");
	checkRegister(addr,0x2C);
	strSend("---");
	strSend("POWER_CTL---");
		checkRegister(addr,0x2D);
		strSend("---");

	setRegister(addr, 0x2D,  0x08);

	strSend("POWER_CTL- po zmianie");
		checkRegister(addr,0x2D);
		strSend("---");


	strSend("FIFO_CTL---");
	checkRegister(addr,0x38);
	strSend("---");
	char wynik[16];
	while(1)
	{

		i2c_start();
		i2c_write(writeAddr);
		i2c_write(0x32);
		i2c_start();
		i2c_write(readAddr);
		for(i=0;i<2;i++)
		{
			cos = i2c_read(1);
			itoa(cos, wynik, 10);
						strSend(wynik);
						_delay_ms(100);
		}
		i2c_read(0);
		i2c_stop();


		_delay_ms(1000);
	}
	return 0;
}
