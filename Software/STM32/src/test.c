#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"
#include "stm32_uart_port.h"
#include "test_format.h"

#define LINE_BUFFER_SIZE 96u
#define STATUS_PERIOD_MS 1000u

static void uart_write_char_blocking(char character)
{
    stm32_uart_port_write_tx_byte_blocking((uint8_t)character);
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
    common_test_write_text(uart_format_writer, NULL, "\r\n");
    common_test_write_text(uart_format_writer, NULL, "==============================\r\n");
    common_test_write_text(uart_format_writer, NULL, "   STM32 UART Test Harness\r\n");
    common_test_write_text(uart_format_writer, NULL, "==============================\r\n");
    common_test_write_text(uart_format_writer, NULL, "\r\n");
    common_test_write_text(uart_format_writer, NULL, "Enter text then press Return\r\n");
    common_test_write_text(uart_format_writer, NULL, "STM32 echoes each message\r\n");
    common_test_write_text(uart_format_writer, NULL, "\r\n> ");
}

static void print_prompt(void)
{
    uart_write_string_blocking("\r\n> ");
}

static void echo_line(const char *line)
{
    uart_write_string_blocking("\r\nReceived: [");
    uart_write_string_blocking(line);
    uart_write_string_blocking("]\r\n");
}

int main(void)
{
    char line_buffer[LINE_BUFFER_SIZE];
    size_t line_length = 0u;
    uint32_t last_status_ms = 0u;

    HAL_Init();
    stm32_uart_port_configure_polling();

    print_intro_message();
    last_status_ms = HAL_GetTick();

    for (;;) {
        uint8_t byte = 0u;
        const uint32_t now_ms = HAL_GetTick();

        if ((now_ms - last_status_ms) >= STATUS_PERIOD_MS) {
            print_prompt();
            last_status_ms = now_ms;
        }

        if (stm32_uart_port_read_rx_byte_if_ready(&byte) == 0u) {
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
