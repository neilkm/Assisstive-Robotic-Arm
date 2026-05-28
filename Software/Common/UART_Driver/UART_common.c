#include "UART_common.h"

#ifndef UART_ENTER_CRITICAL
#define UART_ENTER_CRITICAL() ((void)0)
#endif

#ifndef UART_EXIT_CRITICAL
#define UART_EXIT_CRITICAL() ((void)0)
#endif

#ifndef UART_CONFIGURE
#define UART_CONFIGURE() ((void)0)
#endif

#ifndef UART_READ_RX_BYTE
#define UART_READ_RX_BYTE() ((uint8_t)0u)
#endif

#ifndef UART_WRITE_TX_BYTE
#define UART_WRITE_TX_BYTE(byte_) ((void)(byte_))
#endif

#ifndef UART_ENABLE_TX_INTERRUPT
#define UART_ENABLE_TX_INTERRUPT() ((void)0)
#endif

#ifndef UART_DISABLE_TX_INTERRUPT
#define UART_DISABLE_TX_INTERRUPT() ((void)0)
#endif

#ifndef UART_RX_SEM_INIT
#define UART_RX_SEM_INIT(initial_count_) ((void)(initial_count_))
#endif

#ifndef UART_TX_SEM_INIT
#define UART_TX_SEM_INIT(initial_count_) ((void)(initial_count_))
#endif

#ifndef UART_RX_SEM_WAIT
#define UART_RX_SEM_WAIT(timeout_ticks_) (uart_default_wait_for_rx(timeout_ticks_))
#define UART_USE_DEFAULT_RX_WAIT 1
static uart_status_t uart_default_wait_for_rx(uint32_t timeout_ticks);
#endif

#ifndef UART_TX_SEM_WAIT
#define UART_TX_SEM_WAIT(timeout_ticks_) (uart_default_wait_for_tx(timeout_ticks_))
#define UART_USE_DEFAULT_TX_WAIT 1
static uart_status_t uart_default_wait_for_tx(uint32_t timeout_ticks);
#endif

#ifndef UART_RX_SEM_SIGNAL_FROM_ISR
#define UART_RX_SEM_SIGNAL_FROM_ISR() ((void)0)
#endif

#ifndef UART_TX_SEM_SIGNAL_FROM_ISR
#define UART_TX_SEM_SIGNAL_FROM_ISR() ((void)0)
#endif

static uart_ring_buf_t uart_ring_buffer;

static void uart_advance_rx_in(void)
{
    uart_ring_buffer.rx_in_ptr++;
    if (uart_ring_buffer.rx_in_ptr == &uart_ring_buffer.rx_buffer[UART_RX_STORAGE_SIZE]) {
        uart_ring_buffer.rx_in_ptr = &uart_ring_buffer.rx_buffer[0];
    }
}

static void uart_advance_rx_out(void)
{
    uart_ring_buffer.rx_out_ptr++;
    if (uart_ring_buffer.rx_out_ptr == &uart_ring_buffer.rx_buffer[UART_RX_STORAGE_SIZE]) {
        uart_ring_buffer.rx_out_ptr = &uart_ring_buffer.rx_buffer[0];
    }
}

static void uart_advance_tx_in(void)
{
    uart_ring_buffer.tx_in_ptr++;
    if (uart_ring_buffer.tx_in_ptr == &uart_ring_buffer.tx_buffer[UART_TX_STORAGE_SIZE]) {
        uart_ring_buffer.tx_in_ptr = &uart_ring_buffer.tx_buffer[0];
    }
}

static void uart_advance_tx_out(void)
{
    uart_ring_buffer.tx_out_ptr++;
    if (uart_ring_buffer.tx_out_ptr == &uart_ring_buffer.tx_buffer[UART_TX_STORAGE_SIZE]) {
        uart_ring_buffer.tx_out_ptr = &uart_ring_buffer.tx_buffer[0];
    }
}

void uart_init(void)
{
    UART_CONFIGURE();

    UART_ENTER_CRITICAL();
    uart_ring_buffer.rx_count = 0u;
    uart_ring_buffer.rx_in_ptr = &uart_ring_buffer.rx_buffer[0];
    uart_ring_buffer.rx_out_ptr = &uart_ring_buffer.rx_buffer[0];

    uart_ring_buffer.tx_count = 0u;
    uart_ring_buffer.tx_in_ptr = &uart_ring_buffer.tx_buffer[0];
    uart_ring_buffer.tx_out_ptr = &uart_ring_buffer.tx_buffer[0];
    UART_EXIT_CRITICAL();

    UART_RX_SEM_INIT(0u);
    UART_TX_SEM_INIT(UART_TX_BUF_SIZE);
    UART_DISABLE_TX_INTERRUPT();
}

uart_status_t uart_get_char(uint8_t *out_char, uint32_t timeout_ticks)
{
    uart_status_t status;

    if (out_char == NULL) {
        return UART_ERR_NULL;
    }

    status = UART_RX_SEM_WAIT(timeout_ticks);
    if (status != UART_OK) {
        return status;
    }

    UART_ENTER_CRITICAL();
    if (uart_ring_buffer.rx_count == 0u) {
        UART_EXIT_CRITICAL();
        return UART_ERR_EMPTY;
    }

    *out_char = *uart_ring_buffer.rx_out_ptr;
    uart_advance_rx_out();
    uart_ring_buffer.rx_count--;
    UART_EXIT_CRITICAL();

    return UART_OK;
}

uart_status_t uart_put_char(uint8_t c, uint32_t timeout_ticks)
{
    bool was_empty;
    uart_status_t status = UART_TX_SEM_WAIT(timeout_ticks);

    if (status != UART_OK) {
        return status;
    }

    UART_ENTER_CRITICAL();
    if (uart_ring_buffer.tx_count >= UART_TX_BUF_SIZE) {
        UART_EXIT_CRITICAL();
        return UART_ERR_FULL;
    }

    was_empty = (uart_ring_buffer.tx_count == 0u);
    *uart_ring_buffer.tx_in_ptr = c;
    uart_advance_tx_in();
    uart_ring_buffer.tx_count++;

    if (was_empty) {
        UART_ENABLE_TX_INTERRUPT();
    }
    UART_EXIT_CRITICAL();

    return UART_OK;
}

uart_status_t uart_put_string(const char *string, uint32_t timeout_ticks)
{
    uart_status_t status;

    if (string == NULL) {
        return UART_ERR_NULL;
    }

    while (*string != '\0') {
        status = uart_put_char((uint8_t)*string, timeout_ticks);
        if (status != UART_OK) {
            return status;
        }
        string++;
    }

    return UART_OK;
}

bool uart_is_empty(void)
{
    return uart_ring_buffer.rx_count == 0u;
}

bool uart_is_full(void)
{
    return uart_ring_buffer.tx_count >= UART_TX_BUF_SIZE;
}

bool uart_tx_is_empty(void)
{
    return uart_ring_buffer.tx_count == 0u;
}

bool uart_rx_is_full(void)
{
    return uart_ring_buffer.rx_count >= UART_RX_BUF_SIZE;
}

void uart_rx_isr(void)
{
    uint8_t c = UART_READ_RX_BYTE();

    UART_ENTER_CRITICAL();
    if (uart_ring_buffer.rx_count < UART_RX_BUF_SIZE) {
        *uart_ring_buffer.rx_in_ptr = c;
        uart_advance_rx_in();
        uart_ring_buffer.rx_count++;
        UART_RX_SEM_SIGNAL_FROM_ISR();
    }
    UART_EXIT_CRITICAL();
}

void uart_tx_isr(void)
{
    uint8_t c;

    UART_ENTER_CRITICAL();
    if (uart_ring_buffer.tx_count == 0u) {
        UART_DISABLE_TX_INTERRUPT();
        UART_EXIT_CRITICAL();
        return;
    }

    c = *uart_ring_buffer.tx_out_ptr;
    uart_advance_tx_out();
    uart_ring_buffer.tx_count--;
    UART_TX_SEM_SIGNAL_FROM_ISR();
    UART_EXIT_CRITICAL();

    UART_WRITE_TX_BYTE(c);
}

const uart_ring_buf_t *uart_get_ring_buffer(void)
{
    return &uart_ring_buffer;
}

#ifdef UART_USE_DEFAULT_RX_WAIT
static uart_status_t uart_default_wait_for_rx(uint32_t timeout_ticks)
{
    if (!uart_is_empty()) {
        return UART_OK;
    }

    return (timeout_ticks == UART_TIMEOUT_FOREVER) ? UART_ERR_EMPTY : UART_ERR_TIMEOUT;
}
#endif

#ifdef UART_USE_DEFAULT_TX_WAIT
static uart_status_t uart_default_wait_for_tx(uint32_t timeout_ticks)
{
    if (!uart_is_full()) {
        return UART_OK;
    }

    return (timeout_ticks == UART_TIMEOUT_FOREVER) ? UART_ERR_FULL : UART_ERR_TIMEOUT;
}
#endif
