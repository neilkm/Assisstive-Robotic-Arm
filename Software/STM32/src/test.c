#include "stm32f4xx_hal.h"
#include "uart.h"

static void SystemClock_Config(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    UART_Init(115200);

    UART_WriteString("\r\n");
    UART_WriteString("========================================\r\n");
    UART_WriteString(" STM32 Nucleo-F446RE UART Test Harness\r\n");
    UART_WriteString(" Interrupt-driven USART2 circular buffers\r\n");
    UART_WriteString(" TX: PA2, RX: PA3, baud: 115200\r\n");
    UART_WriteString(" Type a message and press Enter.\r\n");
    UART_WriteString(" Echo format: Rx [message]\r\n");
    UART_WriteString("========================================\r\n");
    UART_WriteString("\r\n");

    uint32_t last_prompt_tick = HAL_GetTick();
    char rx_msg[UART_LINE_BUFFER_SIZE];

    while (1) {
        if (UART_ReadLine(rx_msg, sizeof(rx_msg))) {
            UART_WriteString("Rx [");
            UART_WriteString(rx_msg);
            UART_WriteString("]\r\n");
        }

        if ((HAL_GetTick() - last_prompt_tick) >= 5000U) {
            last_prompt_tick += 5000U;
            UART_WriteString(">\r\n");
        }
    }
}

static void SystemClock_Config(void)
{
    /*
     * Simple clock setup using internal HSI at 16 MHz.
     * This keeps the example portable and avoids relying on
     * external oscillator configuration.
     */

    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        while (1) {
        }
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 |
                    RCC_CLOCKTYPE_PCLK2;

    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) {
        while (1) {
        }
    }
}
