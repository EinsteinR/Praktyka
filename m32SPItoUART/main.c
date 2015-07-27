#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <inttypes.h>

volatile char * komunikat = "Test!\r\n";
volatile uint8_t i;

#define F_CPU 8000000UL  // 8 MHz
#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

void InitSPI(void)
{
DDRB = (1<<PB4)|(1<<PB5) | (1<<PB7);	 // Set MOSI , SCK , and SS output
SPCR = ( (1<<SPE)|(1<<MSTR) | (1<<SPR1) |(1<<SPR0));	// Enable SPI, Master, set clock rate fck/128
}

void WriteByteSPI(unsigned char byte)
{

SPDR = byte;					//Load byte to Data register
while(!(SPSR & (1<<SPIF))); 	// Wait for transmission complete

}

char ReadByteSPI(char addr)
{
	SPDR = addr;					//Load byte to Data register
	while(!(SPSR & (1<<SPIF))); 	// Wait for transmission complete
	addr=SPDR;
	return addr;
}

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


int main(void){

	DDRD &=~(1<<PD2);// Set INT0 pin
	PORTD |=(1<<PD2);// Set pin INT0 high

	DDRB &=~(1<<PB0);
	PORTB |=(1<<PB0);

	cli(); // blokowanie przerwa� CLEAR INTERRUPTS
	GICR |=(1<<INT0); //uaktywnienie INT0 w rejestrz GICR s.46 datasheet
	MCUCR |=(1<<ISC01); //ustawienie INT0 na zbocze opadajace s.65 datasheet
	USART_Init(); //Inicjalizacja komunikacji USART
	InitSPI();
	sei(); // odblokowanie przerwan globalnych SET INTERRUPTS

	strSend("Rozpoczecie komunikacji:\r\n");

	while(1){
		_delay_ms(1000);

		PORTB &=~(1<<PB0);
		WriteByteSPI(0x0B);
		uartSend(ReadByteSPI(0x08));
		PORTB |=(1<<PB0);

	}
return 0;
}
