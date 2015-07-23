#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
	/*
DDRD |= 1 << PIND6; //Set Direction for output on PINB0
PORTD ^= 1 << PINB6; //Toggling only Pin 0 on port b
DDRB |= 1 << PINB2; //Set Direction for Output on PINB2
DDRB &= ~(1 << PINB1); //Data Direction Register input PINB1
PORTB |= 1 << PINB1; //Set PINB1 to a high reading
	*/
	DDRD |= (1<<PD6); //Wyjsciowy Pin6 portu D
	DDRD |= (1<<PD5);
	DDRD |= (1<<PD4);
	
while (1)
{
	PORTD ^= (1<<PD6);
	_delay_ms(500);
	PORTD ^= (1<<PD5);
	_delay_ms(500);
	PORTD ^= (1<<PD4);
	_delay_ms(500);
}
}
