
#include <stdio.h>
#include <avr/io.h>
#include <avr/sfr_defs.h>

#include "uart.h"

/* some mcus have multiple uarts */
#ifdef UDR0
#define UBRRH UBRR0H
#define UBRRL UBRR0L
#define UDR UDR0

#define UCSRA UCSR0A
#define UDRE UDRE0
#define RXC RXC0

#define UCSRB UCSR0B
#define RXEN RXEN0
#define TXEN TXEN0

#define UCSRC UCSR0C
#define URSEL 
#define UCSZ0 UCSZ00
#define UCSZ1 UCSZ01
#define UCSRC_SELECT 0
#else
#define UCSRC_SELECT (1 << URSEL)
#endif

#define BAUD 9600UL
#define UBRRVAL (F_CPU/(BAUD*16)-1)

static int _uart_putc(char c, FILE* stream);
static int _uart_getc(FILE* stream);

static FILE stdio = FDEV_SETUP_STREAM(_uart_putc, _uart_getc, _FDEV_SETUP_RW);

void uart_init()
{
    /* set baud rate */
    UBRRH = UBRRVAL >> 8;
    UBRRL = UBRRVAL & 0xff;
    /* set frame format: 8 bit, no parity, 1 bit */
    UCSRC = UCSRC_SELECT | (1 << UCSZ1) | (1 << UCSZ0);
    /* enable serial receiver and transmitter */
    UCSRB = (1 << RXEN) | (1 << TXEN);
}

void uart_connect_stdio()
{
    stdin = stdout = stderr = &stdio;
}

void uart_putc(uint8_t c)
{
    if(c == '\n')
        uart_putc('\r');
    /* wait until transmit buffer is empty */
    loop_until_bit_is_set(UCSRA, UDRE);
    /* send next byte */
    UDR = c;
}

int _uart_putc(char c, FILE* stream)
{
    uart_putc(c);
    return 0;
}

void uart_putc_hex(uint8_t b)
{
    /* upper nibble */
    if((b >> 4) < 0x0a)
        uart_putc((b >> 4) + '0');
    else
        uart_putc((b >> 4) - 0x0a + 'a');

    /* lower nibble */
    if((b & 0x0f) < 0x0a)
        uart_putc((b & 0x0f) + '0');
    else
        uart_putc((b & 0x0f) - 0x0a + 'a');
}

uint8_t uart_getc()
{
    /* wait until receive buffer is full */
    loop_until_bit_is_set(UCSRA, RXC);

    uint8_t b = UDR;
    if(b == '\r')
        b = '\n';

    return b;
}

int _uart_getc(FILE* stream)
{
    return uart_getc();
}


