#ifndef UART_COMMON_H
#define UART_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef UART_RX_BUF_SIZE
#define UART_RX_BUF_SIZE 64u
#endif

#ifndef UART_TX_BUF_SIZE
#define UART_TX_BUF_SIZE 64u
#endif

#if UART_RX_BUF_SIZE > UINT16_MAX
#error "UART_RX_BUF_SIZE must fit in uint16_t"
#endif

#if UART_TX_BUF_SIZE > UINT16_MAX
#error "UART_TX_BUF_SIZE must fit in uint16_t"
#endif

#if UART_RX_BUF_SIZE == 0
#define UART_RX_STORAGE_SIZE 1u
#else
#define UART_RX_STORAGE_SIZE UART_RX_BUF_SIZE
#endif

#if UART_TX_BUF_SIZE == 0
#define UART_TX_STORAGE_SIZE 1u
#else
#define UART_TX_STORAGE_SIZE UART_TX_BUF_SIZE
#endif

#ifndef UART_TIMEOUT_NONE
#define UART_TIMEOUT_NONE 0u
#endif

#ifndef UART_TIMEOUT_FOREVER
#define UART_TIMEOUT_FOREVER UINT32_MAX
#endif

typedef enum {
    UART_OK = 0,
    UART_ERR_TIMEOUT,
    UART_ERR_FULL,
    UART_ERR_EMPTY,
    UART_ERR_NULL
} uart_status_t;

typedef struct {
    uint16_t rx_count;
    uint8_t *rx_in_ptr;
    uint8_t *rx_out_ptr;
    uint8_t rx_buffer[UART_RX_STORAGE_SIZE];

    uint16_t tx_count;
    uint8_t *tx_in_ptr;
    uint8_t *tx_out_ptr;
    uint8_t tx_buffer[UART_TX_STORAGE_SIZE];
} uart_ring_buf_t;

void uart_init(void);

uart_status_t uart_get_char(uint8_t *out_char, uint32_t timeout_ticks);
uart_status_t uart_put_char(uint8_t c, uint32_t timeout_ticks);
uart_status_t uart_put_string(const char *string, uint32_t timeout_ticks);

bool uart_is_empty(void);
bool uart_is_full(void);
bool uart_tx_is_empty(void);
bool uart_rx_is_full(void);

void uart_rx_isr(void);
void uart_tx_isr(void);

const uart_ring_buf_t *uart_get_ring_buffer(void);

#endif
