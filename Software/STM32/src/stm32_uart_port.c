#include "stm32f4xx_hal.h"

#include "stm32_uart_port.h"

#define UART_BAUD_RATE 115200u
#define UART_GPIO_AF GPIO_AF7_USART2
#define UART_TX_PIN GPIO_PIN_2
#define UART_RX_PIN GPIO_PIN_3
#define UART_GPIO_PORT GPIOA
#define UART_INSTANCE USART2
#define UART_IRQ USART2_IRQn

static uint32_t uart_critical_primask;

void stm32_uart_port_configure(void)
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

    HAL_NVIC_SetPriority(UART_IRQ, 5u, 0u);
    HAL_NVIC_EnableIRQ(UART_IRQ);
}

uint8_t stm32_uart_port_read_rx_byte(void)
{
    return (uint8_t)(UART_INSTANCE->DR & 0xffu);
}

void stm32_uart_port_write_tx_byte(uint8_t byte)
{
    UART_INSTANCE->DR = byte;
}

void stm32_uart_port_enable_tx_interrupt(void)
{
    UART_INSTANCE->CR1 |= USART_CR1_TXEIE;
}

void stm32_uart_port_disable_tx_interrupt(void)
{
    UART_INSTANCE->CR1 &= ~USART_CR1_TXEIE;
}

void stm32_uart_port_enter_critical(void)
{
    uart_critical_primask = __get_PRIMASK();
    __disable_irq();
}

void stm32_uart_port_exit_critical(void)
{
    if (uart_critical_primask == 0u) {
        __enable_irq();
    }
}

#define UART_CONFIGURE() stm32_uart_port_configure()
#define UART_READ_RX_BYTE() stm32_uart_port_read_rx_byte()
#define UART_WRITE_TX_BYTE(byte_) stm32_uart_port_write_tx_byte((byte_))
#define UART_ENABLE_TX_INTERRUPT() stm32_uart_port_enable_tx_interrupt()
#define UART_DISABLE_TX_INTERRUPT() stm32_uart_port_disable_tx_interrupt()
#define UART_ENTER_CRITICAL() stm32_uart_port_enter_critical()
#define UART_EXIT_CRITICAL() stm32_uart_port_exit_critical()

#include "UART_common.c"

void USART2_IRQHandler(void)
{
    const uint32_t sr = UART_INSTANCE->SR;
    const uint32_t cr1 = UART_INSTANCE->CR1;

    if ((sr & USART_SR_RXNE) != 0u) {
        uart_rx_isr();
    }

    if (((sr & USART_SR_ORE) != 0u) || ((sr & USART_SR_FE) != 0u) || ((sr & USART_SR_NE) != 0u)) {
        (void)UART_INSTANCE->DR;
    }

    if (((sr & USART_SR_TXE) != 0u) && ((cr1 & USART_CR1_TXEIE) != 0u)) {
        uart_tx_isr();
    }
}
