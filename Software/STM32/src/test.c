#include "stm32f4xx_hal.h"
#include "packet.h"
#include "uart.h"

static void SystemClock_Config(void);

#ifdef STM_UART_PROTOCOL_TEST

static volatile arm_actual_state_t g_actual_state;
static volatile arm_desired_state_t g_desired_state;

static void ActualState_Init(void)
{
    for (size_t i = 0u; i < ARM_UART_JOINT_COUNT; ++i) {
        g_actual_state.actual_joint_angles[i] = (float)i * 10.0f;
        g_desired_state.desired_joint_angles[i] = 0.0f;
    }
    g_actual_state.force_sensor = 1.0f;
}

static arm_actual_state_t ActualState_Snapshot(void)
{
    arm_actual_state_t snapshot;
    __disable_irq();
    for (size_t i = 0u; i < ARM_UART_JOINT_COUNT; ++i) {
        snapshot.actual_joint_angles[i] = g_actual_state.actual_joint_angles[i];
    }
    snapshot.force_sensor = g_actual_state.force_sensor;
    __enable_irq();
    return snapshot;
}

static void DesiredState_Store(const arm_desired_state_t *desired)
{
    if (desired == NULL) {
        return;
    }

    __disable_irq();
    for (size_t i = 0u; i < ARM_UART_JOINT_COUNT; ++i) {
        g_desired_state.desired_joint_angles[i] = desired->desired_joint_angles[i];
    }
    __enable_irq();
}

static void ActualState_MirrorDesiredForTest(const arm_desired_state_t *desired, uint8_t sequence)
{
    if (desired == NULL) {
        return;
    }

    __disable_irq();
    for (size_t i = 0u; i < ARM_UART_JOINT_COUNT; ++i) {
        g_actual_state.actual_joint_angles[i] = desired->desired_joint_angles[i];
    }
    g_actual_state.force_sensor = (float)sequence;
    __enable_irq();
}

static void ProtocolStatusLed_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    UART_Init(115200);
    ProtocolStatusLed_Init();
    ActualState_Init();

    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    uint8_t tx_frame[ARM_UART_MAX_FRAME_SIZE];
    uint8_t tx_sequence = 0u;
    uint32_t last_tx_tick = 0u;

    while (1) {
        uint8_t rx_byte = 0u;
        while (UART_ReadByte(&rx_byte)) {
            if (arm_uart_parser_feed(&parser, rx_byte)) {
                arm_desired_state_t desired;
                uint8_t rx_sequence = 0u;
                if (arm_uart_decode_desired_state_packet(parser.bytes,
                                                         parser.frame_length,
                                                         &desired,
                                                         &rx_sequence)) {
                    DesiredState_Store(&desired);
                    ActualState_MirrorDesiredForTest(&desired, rx_sequence);
                }
            }
        }

        const uint32_t now = HAL_GetTick();
        if ((now - last_tx_tick) >= 100u) {
            last_tx_tick = now;
            const arm_actual_state_t actual = ActualState_Snapshot();
            const size_t tx_len = arm_uart_build_actual_state_packet(&actual,
                                                                     tx_sequence,
                                                                     tx_frame,
                                                                     sizeof(tx_frame));
            if (tx_len > 0u) {
                UART_Write(tx_frame, tx_len);
                HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
                tx_sequence++;
            }
        }

        HAL_Delay(1u);
    }
}

#else

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
            last_prompt_tick = HAL_GetTick();
            UART_WriteString(">\r\n");
        }
    }
}

#endif

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();

    /*
     * Use HSI 16 MHz.
     * This is simple and stable for a UART test harness.
     */
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

/*
 * This is the important missing piece.
 * Without this, HAL_GetTick() may stay stuck at 0 in PlatformIO STM32Cube.
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
