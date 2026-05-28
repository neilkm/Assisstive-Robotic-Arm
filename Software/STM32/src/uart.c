#include "uart.h"
#include <string.h>

static UART_HandleTypeDef huart2;

/* RX circular buffer */
static volatile uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;
static volatile uint16_t rx_count = 0;

/* TX circular buffer */
static volatile uint8_t tx_buffer[UART_TX_BUFFER_SIZE];
static volatile uint16_t tx_head = 0;
static volatile uint16_t tx_tail = 0;
static volatile uint16_t tx_count = 0;

/* Line parser state */
static char line_buffer[UART_LINE_BUFFER_SIZE];
static uint16_t line_index = 0;

static uint16_t next_index(uint16_t index, uint16_t size)
{
    index++;
    if (index >= size) {
        index = 0;
    }
    return index;
}

static void UART_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    /*
     * Nucleo-F446RE virtual COM port:
     * PA2 = USART2_TX
     * PA3 = USART2_RX
     */
    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;

    HAL_GPIO_Init(GPIOA, &gpio);
}

void UART_Init(uint32_t baudrate)
{
    UART_GPIO_Init();

    huart2.Instance = USART2;
    huart2.Init.BaudRate = baudrate;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK) {
        while (1) {
        }
    }

    __disable_irq();

    rx_head = 0;
    rx_tail = 0;
    rx_count = 0;

    tx_head = 0;
    tx_tail = 0;
    tx_count = 0;

    line_index = 0;

    /*
     * Clear stale USART flags by reading SR then DR.
     */
    volatile uint32_t tmp;
    tmp = USART2->SR;
    tmp = USART2->DR;
    (void)tmp;

    __enable_irq();

    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);

    /*
     * Enable receiver, transmitter, RX interrupt, and UART error interrupt.
     * TXE interrupt is enabled only when bytes are queued for transmit.
     */
    USART2->CR1 |= USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE;
    USART2->CR3 |= USART_CR3_EIE;
}

size_t UART_Write(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) {
        return 0;
    }

    size_t written = 0;

    while (written < len) {
        uint8_t queued = 0;

        __disable_irq();

        if (tx_count < UART_TX_BUFFER_SIZE) {
            tx_buffer[tx_head] = data[written];
            tx_head = next_index(tx_head, UART_TX_BUFFER_SIZE);
            tx_count++;
            written++;
            queued = 1;

            /*
             * Start or continue interrupt-driven transmission.
             */
            USART2->CR1 |= USART_CR1_TXEIE;
        }

        __enable_irq();

        /*
         * If the TX buffer is full, wait until the ISR frees space.
         */
        if (!queued) {
            while (tx_count >= UART_TX_BUFFER_SIZE) {
            }
        }
    }

    return written;
}

size_t UART_WriteString(const char *str)
{
    if (str == NULL) {
        return 0;
    }

    return UART_Write((const uint8_t *)str, strlen(str));
}

size_t UART_WriteLine(const char *str)
{
    size_t n = 0;
    n += UART_WriteString(str);
    n += UART_WriteString("\r\n");
    return n;
}

int UART_ReadByte(uint8_t *byte)
{
    if (byte == NULL) {
        return 0;
    }

    __disable_irq();

    if (rx_count == 0) {
        __enable_irq();
        return 0;
    }

    *byte = rx_buffer[rx_tail];
    rx_tail = next_index(rx_tail, UART_RX_BUFFER_SIZE);
    rx_count--;

    __enable_irq();

    return 1;
}

int UART_ReadLine(char *dst, size_t dst_size)
{
    if (dst == NULL || dst_size == 0) {
        return 0;
    }

    uint8_t byte;

    while (UART_ReadByte(&byte)) {
        char c = (char)byte;

        if (c == '\r' || c == '\n') {
            if (line_index > 0) {
                line_buffer[line_index] = '\0';

                strncpy(dst, line_buffer, dst_size - 1);
                dst[dst_size - 1] = '\0';

                line_index = 0;
                return 1;
            }
        } else {
            if (line_index < UART_LINE_BUFFER_SIZE - 1) {
                line_buffer[line_index++] = c;
            } else {
                /*
                 * Message too long, discard partial line.
                 */
                line_index = 0;
            }
        }
    }

    return 0;
}

uint16_t UART_RxAvailable(void)
{
    return rx_count;
}

uint16_t UART_TxFree(void)
{
    return UART_TX_BUFFER_SIZE - tx_count;
}

void USART2_IRQHandler(void)
{
    uint32_t sr = USART2->SR;

    /*
     * Handle UART errors.
     * Reading SR then DR clears ORE, FE, NE, PE.
     */
    if (sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
        volatile uint32_t tmp;
        tmp = USART2->SR;
        tmp = USART2->DR;
        (void)tmp;
    }

    /*
     * RXNE: byte received.
     */
    if (USART2->SR & USART_SR_RXNE) {
        uint8_t byte = (uint8_t)(USART2->DR & 0xFF);

        if (rx_count < UART_RX_BUFFER_SIZE) {
            rx_buffer[rx_head] = byte;
            rx_head = next_index(rx_head, UART_RX_BUFFER_SIZE);
            rx_count++;
        } else {
            /*
             * RX buffer full, byte dropped.
             */
        }
    }

    /*
     * TXE: transmit data register empty.
     */
    if ((USART2->SR & USART_SR_TXE) && (USART2->CR1 & USART_CR1_TXEIE)) {
        if (tx_count > 0) {
            USART2->DR = tx_buffer[tx_tail];
            tx_tail = next_index(tx_tail, UART_TX_BUFFER_SIZE);
            tx_count--;
        } else {
            USART2->CR1 &= ~USART_CR1_TXEIE;
        }
    }
}
