#include "uart.h"

#include "stm32f4xx_hal.h"

#define UART_BAUD_RATE 115200u
#define UART_GPIO_AF GPIO_AF7_USART2
#define UART_TX_PIN GPIO_PIN_2
#define UART_RX_PIN GPIO_PIN_3
#define UART_GPIO_PORT GPIOA
#define UART_INSTANCE USART2
#define UART_IRQ USART2_IRQn

typedef struct {
    uint8_t data[UART_RX_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
} uart_rx_ring_t;

typedef struct {
    uint8_t data[UART_TX_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
} uart_tx_ring_t;

static uart_rx_ring_t uart_rx_ring;
static uart_tx_ring_t uart_tx_ring;

static void uart_enter_critical(uint32_t *primask)
{
    *primask = __get_PRIMASK();
    __disable_irq();
}

static void uart_exit_critical(uint32_t primask)
{
    if (primask == 0u) {
        __enable_irq();
    }
}

static uint16_t uart_advance_index(uint16_t index, uint16_t size)
{
    index++;
    if (index >= size) {
        index = 0u;
    }

    return index;
}

static void uart_reset_buffers(void)
{
    uint32_t primask;

    uart_enter_critical(&primask);
    uart_rx_ring.head = 0u;
    uart_rx_ring.tail = 0u;
    uart_rx_ring.count = 0u;

    uart_tx_ring.head = 0u;
    uart_tx_ring.tail = 0u;
    uart_tx_ring.count = 0u;
    uart_exit_critical(primask);
}

static void uart_start_tx_locked(void)
{
    if ((uart_tx_ring.count == 0u) || ((UART_INSTANCE->SR & USART_SR_TXE) == 0u)) {
        UART_INSTANCE->CR1 |= USART_CR1_TXEIE;
        return;
    }

    UART_INSTANCE->DR = uart_tx_ring.data[uart_tx_ring.tail];
    uart_tx_ring.tail = uart_advance_index(uart_tx_ring.tail, UART_TX_BUFFER_SIZE);
    uart_tx_ring.count--;

    if (uart_tx_ring.count > 0u) {
        UART_INSTANCE->CR1 |= USART_CR1_TXEIE;
    } else {
        UART_INSTANCE->CR1 &= ~USART_CR1_TXEIE;
    }
}

void uart_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    gpio_init.Pin = UART_TX_PIN | UART_RX_PIN;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.Alternate = UART_GPIO_AF;
    HAL_GPIO_Init(UART_GPIO_PORT, &gpio_init);

    UART_INSTANCE->CR1 = 0u;
    UART_INSTANCE->CR2 = 0u;
    UART_INSTANCE->CR3 = 0u;
    UART_INSTANCE->BRR = HAL_RCC_GetPCLK1Freq() / UART_BAUD_RATE;
    UART_INSTANCE->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    uart_reset_buffers();

    HAL_NVIC_SetPriority(UART_IRQ, 5u, 0u);
    HAL_NVIC_EnableIRQ(UART_IRQ);
}

void uart_write_byte(uint8_t byte)
{
    for (;;) {
        uint32_t primask;
        uint8_t queued = 0u;

        uart_enter_critical(&primask);
        if (uart_tx_ring.count < UART_TX_BUFFER_SIZE) {
            uart_tx_ring.data[uart_tx_ring.head] = byte;
            uart_tx_ring.head = uart_advance_index(uart_tx_ring.head, UART_TX_BUFFER_SIZE);
            uart_tx_ring.count++;
            uart_start_tx_locked();
            queued = 1u;
        }
        uart_exit_critical(primask);

        if (queued != 0u) {
            return;
        }

        HAL_Delay(1u);
    }
}

void uart_write_string(const char *string)
{
    if (string == 0) {
        return;
    }

    while (*string != '\0') {
        uart_write_byte((uint8_t)*string);
        string++;
    }
}

uint8_t uart_read_byte_if_ready(uint8_t *byte)
{
    uint32_t primask;
    uint8_t has_byte = 0u;

    if (byte == 0) {
        return 0u;
    }

    uart_enter_critical(&primask);
    if (uart_rx_ring.count > 0u) {
        *byte = uart_rx_ring.data[uart_rx_ring.tail];
        uart_rx_ring.tail = uart_advance_index(uart_rx_ring.tail, UART_RX_BUFFER_SIZE);
        uart_rx_ring.count--;
        has_byte = 1u;
    }
    uart_exit_critical(primask);

    return has_byte;
}

void USART2_IRQHandler(void)
{
    const uint32_t status = UART_INSTANCE->SR;
    const uint32_t control = UART_INSTANCE->CR1;

    if ((status & USART_SR_RXNE) != 0u) {
        const uint8_t byte = (uint8_t)(UART_INSTANCE->DR & 0xffu);

        if (uart_rx_ring.count < UART_RX_BUFFER_SIZE) {
            uart_rx_ring.data[uart_rx_ring.head] = byte;
            uart_rx_ring.head = uart_advance_index(uart_rx_ring.head, UART_RX_BUFFER_SIZE);
            uart_rx_ring.count++;
        }
    }

    if ((status & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) != 0u) {
        (void)UART_INSTANCE->DR;
    }

    if (((status & USART_SR_TXE) != 0u) && ((control & USART_CR1_TXEIE) != 0u)) {
        if (uart_tx_ring.count > 0u) {
            const uint8_t byte = uart_tx_ring.data[uart_tx_ring.tail];

            uart_tx_ring.tail = uart_advance_index(uart_tx_ring.tail, UART_TX_BUFFER_SIZE);
            uart_tx_ring.count--;
            UART_INSTANCE->DR = byte;
        } else {
            UART_INSTANCE->CR1 &= ~USART_CR1_TXEIE;
        }
    }
}
