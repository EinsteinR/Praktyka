#define F_CPU 8000000UL  // 8 MHz

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

volatile unsigned char value;

ISR(USART_RXC_vect)
{

   value = UDR;             //read UART register into value
   PORTB = ~value;          // output inverted value on LEDs (0=on)
}

void USART_Init(void){
   // Set baud rate
   UBRRL = BAUD_PRESCALE;// Load lower 8-bits into the low byte of the UBRR register
   UBRRH = (BAUD_PRESCALE >> 8);
   UCSRB = ((1<<TXEN)|(1<<RXEN) | (1<<RXCIE));
}


void USART_SendByte(uint8_t u8Data){

  // Wait until last byte has been transmitted
  while((UCSRA &(1<<UDRE)) == 0);

  // Transmit data
  UDR = u8Data;
}


// not being used but here for completeness
      // Wait until a byte has been received and return received data
uint8_t USART_ReceiveByte(){
  while((UCSRA &(1<<RXC)) == 0);
  return UDR;
}

void Led_init(void){
   //outputs, all off
	 DDRB =0xFF;
   PORTB = 0xFF;
}

int main(void){
   USART_Init();  // Initialise USART
   sei();         // enable all interrupts
   Led_init();    // init LEDs for testing
   value = 'A'; //0x41;
   PORTB = ~value; // 0 = LED on

   for(;;){    // Repeat indefinitely

     USART_SendByte(value);  // send value
     _delay_ms(250);
		         // delay just to stop Hyperterminal screen cluttering up
   }
}
