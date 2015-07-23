#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

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

#define CS (1<<PB2)
#define MOSI (1<<PB3)
#define MISO (1<<PB4)
#define SCK (1<<PB5)
#define CS_DDR DDRB
#define CS_ENABLE() (PORTB &= ~CS)
#define CS_DISABLE() (PORTB |= CS)

void SPI_init() {
	CS_DDR |= CS; // SD card circuit select as output
	DDRB |= MOSI + SCK; // MOSI and SCK as outputs
	PORTB |= MISO; // pullup in MISO, might not be needed

	// Enable SPI, master, set clock rate fck/128
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0) | (1<<SPR1);
}

unsigned char SPI_write(unsigned char ch) {
	SPDR = ch;
	while(!(SPSR & (1<<SPIF))) {}
	return SPDR;
}

void SD_command(unsigned char cmd, unsigned long arg, unsigned char crc, unsigned char read) {
	unsigned char i, buffer[8];

	uwrite_str("CMD ");
	uwrite_hex(cmd);

	CS_ENABLE();
	SPI_write(cmd);
	SPI_write(arg>>24);
	SPI_write(arg>>16);
	SPI_write(arg>>8);
	SPI_write(arg);
	SPI_write(crc);

	for(i=0; i<read; i++)
		buffer[i] = SPI_write(0xFF);

	CS_DISABLE();

	for(i=0; i<read; i++) {
		USARTWriteChar(' ');
		uwrite_hex(buffer[i]);
	}

	uwrite_str("\r\n");
}

int main(int argc, char *argv[]) {
	char i;

	USART_Init();
	SPI_init();

	// ]r:10
	CS_DISABLE();
	for(i=0; i<10; i++) // idle for 1 bytes / 80 clocks
		SPI_write(0xFF);

	while(1) {
		switch() {
		case '1':
			SD_command(0x40, 0x00000000, 0x95, 8);
			break;
		case '2':
			SD_command(0x41, 0x00000000, 0xFF, 8);
			break;
		case '3':
			SD_command(0x50, 0x00000200, 0xFF, 8);
			break;
		}
	}

	return 0;
}
