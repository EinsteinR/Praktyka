#include <avr/io.h>
#include <avr/interrupt.h>
#include <inttypes.h>

int main(void){

	DDRB |= (1<<PB0)|(1<<PB1);
	PORTB &= ~((1<<PB0)|(1<<PB1));

	while(1){
		if(__AVR_ATmega8__) PORTB |= (1<<PB1);

	}
return 0;
}
