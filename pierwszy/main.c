#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <compat/twi.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#define F_CPU 8000000UL  // 8 MHz
#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)
#define ENG_SIG_TAB_SIZE 16


volatile uint16_t TDelay = 100; //Zmienna obs�ugiwna w przerwaniach
char * komunikat = "Kokosz Ty BooCIE!";
volatile char  wartosc[4];
volatile uint8_t i = 0;
volatile uint8_t ii = 0;
volatile uint8_t pozycja[ENG_SIG_TAB_SIZE] = {5,0,45,0,40,0,46,0,6,0,54,0,48,0,53,0};
//volatile uint8_t pozycja[ENG_SIG_TAB_SIZE] = {0,0};
//volatile uint8_t pozycja[8] = {5,45,40,46,6,54,48,53};
//volatile uint8_t pozycja[8] = {5,48,6,40,5,48,6,40};
//volatile uint8_t pozycja[8] = {1,9,8,10,2,18,16,17};
//volatile uint8_t pozycja[16] = {1,0,9,0,8,0,10,0,2,0,18,0,16,0,17,0};
//volatile uint8_t pozycja[8] = {1,8,2,16,1,8,2,16};
//volatile uint8_t pozycja[8] = {5,40,6,48,5,40,6,48};
volatile uint16_t time = 1;
volatile uint16_t speed = 100;
volatile bool kierunek =0;
void USART_Init(void)
{
	UCSRA &= ~(1<<U2X); //Rejest ustawienia preskalera do obliczania BAUD
	UBRRL = (unsigned char)BAUD_PRESCALE; //wsp�czynnik do okreslenia predkosci transmisji 9600 kb/s (UBBR=f_sys/(16*BAUD)-1)
	UBRRH = (BAUD_PRESCALE >> 8);
	UCSRC &= ~(1<<UMSEL); //Draca asynchroniczna
	//Odkomentowanie tego nie dzi
	//UCSRC |= (1<<URSEL); //Mozliwosc konfigurowania UCSRC
	//UCSRC |= ((1<<UCSZ1) | (1<<UCSZ0)); //Bity okreslajace wielkosc wysy�anych danych (nie ramki)
	UCSRB |= ((1<<RXEN) | (1<<TXEN)); //wlaczone moduly nadawcze i odbiorcze
	UCSRB |= (1<<RXCIE); //Umozliwia wyzwolenia przerwan przy odbiorze
}

void Timer_Init(void)
{
	TCCR1B |= (1<<CS12)|(1<<WGM12);
	TIMSK |= (1<<OCIE1A);
	OCR1A = speed;
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

ISR(USART_RXC_vect) //Odczyt z UART
{
	//przyjmowanie jednej liczby
	uint8_t dane = UDR;
	if(dane >= '0' && dane <= '9')
			time = dane - 48;
	if (dane == 'A')
		speed = speed - 5;
	if (dane == 'B')
		speed = speed + 1;
	if (dane == 'C')
		kierunek = 0;
	if (dane == 'D')
			kierunek = 1;
		OCR1A = speed;
		itoa(speed, wartosc, 10);
		strSend(wartosc);
		_delay_ms(10);
}

ISR(INT0_vect) // Interrupt PD2
{
i=0;
	while(*(komunikat + i) != 0)
	{
		uartSend(*(komunikat + i));
		i++;
	}
uartSend('\r');
uartSend('\n');
}

ISR(TIMER1_COMPA_vect)
{
	(kierunek)?ii++:ii--;
	ii=ii % ENG_SIG_TAB_SIZE;
	PORTC = pozycja[ii];
}


int main(void){
	DDRD &=~(1<<PD2);// Set INT0 pin
	PORTD |=(1<<PD2);// Set pin INT0 high
	DDRD |= (1 <<PD7)|(1<<PD6); //Set OU0T
	//DDRD &= ~(1<<PD6); //Set IN
	PORTD |= (1<<PD6);
	DDRC = 0b00111111;

	cli(); // blokowanie przerwa� CLEAR INTERRUPTS
	GICR |=(1<<INT0); //uaktywnienie INT0 w rejestrz GICR s.46 datasheet
	MCUCR |=(1<<ISC01); //ustawienie INT0 na zbocze opadajace s.65 datasheet
	USART_Init(); //Inicjalizacja komunikacji USART
	Timer_Init(); //Ustawienia Timera
	sei(); // odblokowanie przerwan globalnych SET INTERRUPTS




	while(1){
		//_delay_ms(100*time);
		//PORTD ^=(1<<PD7);
	}
return 0;
}



