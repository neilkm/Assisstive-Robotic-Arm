#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"
#include "test_format.h"
#include "UART_common.h"

#define LINE_BUFFER_SIZE 96u
#define PROMPT_PERIOD_MS 2000u

static void uart_write_char_blocking(char character)
{
    while (uart_put_char((uint8_t)character, UART_TIMEOUT_NONE) != UART_OK) {
        HAL_Delay(1u);
    }
}

static void uart_format_writer(char character, void *context)
{
    (void)context;
    uart_write_char_blocking(character);
}

static void uart_write_string_blocking(const char *string)
{
    if (string == NULL) {
        return;
    }

    while (*string != '\0') {
        uart_write_char_blocking(*string);
        string++;
    }
}

static void print_intro_message(void)
{
    common_test_write_text(uart_format_writer, NULL, "STM32 UART Test Harness, enter text and hear echo.\n");
}

static void echo_line(const char *line)
{
    uart_write_string_blocking("\nReceived: [");
    uart_write_string_blocking(line);
    uart_write_string_blocking("]\n");
}

int main(void)
{
    char line_buffer[LINE_BUFFER_SIZE];
    size_t line_length = 0u;
    uint32_t last_prompt_ms = 0u;

    HAL_Init();
    uart_init();

    print_intro_message();
    last_prompt_ms = HAL_GetTick();

    for (;;) {
        uint8_t byte = 0u;
        const uint32_t now_ms = HAL_GetTick();

        if ((now_ms - last_prompt_ms) >= PROMPT_PERIOD_MS) {
            uart_write_string_blocking("\n> ");
            last_prompt_ms = now_ms;
        }

        if (uart_get_char(&byte, UART_TIMEOUT_NONE) != UART_OK) {
            continue;
        }

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
