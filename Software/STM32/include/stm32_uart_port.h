#ifndef STM32_UART_PORT_H
#define STM32_UART_PORT_H

#include <stdint.h>

void stm32_uart_port_configure(void);
uint8_t stm32_uart_port_read_rx_byte(void);
void stm32_uart_port_write_tx_byte(uint8_t byte);
void stm32_uart_port_enable_tx_interrupt(void);
void stm32_uart_port_disable_tx_interrupt(void);
void stm32_uart_port_enter_critical(void);
void stm32_uart_port_exit_critical(void);

#endif
