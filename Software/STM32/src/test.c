#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"
#include "stm32_uart_port.h"
#include "test_format.h"

#define LINE_BUFFER_SIZE 96u
#define PROMPT_PERIOD_MS 2000u

static void uart_write_char(char character)
{
    while (stm32_uart_port_tx_ready() == 0u) {
    }
    stm32_uart_port_write_tx_byte((uint8_t)character);
}

static void uart_format_writer(char character, void *context)
{
    (void)context;
    uart_write_char(character);
}

static void uart_write_string(const char *string)
{
    if (string == NULL) {
        return;
    }

    while (*string != '\0') {
        uart_write_char(*string);
        string++;
    }
}

static void print_intro_message(void)
{
    common_test_write_text(uart_format_writer, NULL, "STM32 UART Test Harness, enter text and hear echo.\n");
}

static void print_prompt(void)
{
    uart_write_string("\n> ");
}

static void echo_line(const char *line)
{
    uart_write_string("\nReceived: [");
    uart_write_string(line);
    uart_write_string("]\n");
}

int main(void)
{
    char line_buffer[LINE_BUFFER_SIZE];
    size_t line_length = 0u;
    uint32_t last_prompt_ms = 0u;

    HAL_Init();
    stm32_uart_port_configure_polling();

    print_intro_message();
    print_prompt();
    last_prompt_ms = HAL_GetTick();

    for (;;) {
        uint8_t byte = 0u;
        const uint32_t now_ms = HAL_GetTick();

        if ((now_ms - last_prompt_ms) >= PROMPT_PERIOD_MS) {
            print_prompt();
            last_prompt_ms = now_ms;
        }

        if (stm32_uart_port_rx_ready() == 0u) {
            continue;
        }
        byte = stm32_uart_port_read_rx_byte();

        if ((byte == '\r') || (byte == '\n')) {
            if (line_length > 0u) {
                line_buffer[line_length] = '\0';
                echo_line(line_buffer);
                line_length = 0u;
            }
            continue;
        }

        if (line_length < (LINE_BUFFER_SIZE - 1u)) {
            line_buffer[line_length] = (char)byte;
            line_length++;
        } else {
            line_buffer[line_length] = '\0';
            echo_line(line_buffer);
            line_length = 0u;
        }
    }
}
