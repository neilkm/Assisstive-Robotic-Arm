#ifndef STM32_UART_H
#define STM32_UART_H

#include <stdint.h>

void uart_init(void);
void uart_write_byte(uint8_t byte);
void uart_write_string(const char *string);
uint8_t uart_read_byte_if_ready(uint8_t *byte);

#endif
