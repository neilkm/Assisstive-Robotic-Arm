#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stddef.h>
#include "stm32f4xx_hal.h"

#define UART_RX_BUFFER_SIZE 256
#define UART_TX_BUFFER_SIZE 256
#define UART_LINE_BUFFER_SIZE 128

void UART_Init(uint32_t baudrate);

size_t UART_Write(const uint8_t *data, size_t len);
size_t UART_WriteString(const char *str);
size_t UART_WriteLine(const char *str);

int UART_ReadByte(uint8_t *byte);
int UART_ReadLine(char *dst, size_t dst_size);

uint16_t UART_RxAvailable(void);
uint16_t UART_TxFree(void);

UART_HandleTypeDef *UART_GetHandle(void);

#endif
