#include "uart.h"

#include "stm32f4xx_hal.h"

#define UART_BAUD_RATE 115200u
#define UART_GPIO_AF GPIO_AF7_USART2
#define UART_TX_PIN GPIO_PIN_2
#define UART_RX_PIN GPIO_PIN_3
#define UART_GPIO_PORT GPIOA
#define UART_INSTANCE USART2

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
    UART_INSTANCE->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void uart_write_byte(uint8_t byte)
{
    while ((UART_INSTANCE->SR & USART_SR_TXE) == 0u) {
    }

    UART_INSTANCE->DR = byte;

    while ((UART_INSTANCE->SR & USART_SR_TC) == 0u) {
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
    const uint32_t status = UART_INSTANCE->SR;

    if ((status & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) != 0u) {
        (void)UART_INSTANCE->DR;
        return 0u;
    }

    if ((status & USART_SR_RXNE) == 0u) {
        return 0u;
    }

    if (byte != 0) {
        *byte = (uint8_t)(UART_INSTANCE->DR & 0xffu);
    } else {
        (void)UART_INSTANCE->DR;
    }

    return 1u;
}
