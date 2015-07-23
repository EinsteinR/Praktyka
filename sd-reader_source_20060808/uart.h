
#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init();
void uart_connect_stdio();
void uart_putc(uint8_t c);
void uart_putc_hex(uint8_t b);
uint8_t uart_getc();

#endif

