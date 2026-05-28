#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"
#include "uart.h"

#define LINE_BUFFER_SIZE 192u
#define STATUS_PERIOD_MS 1000u

static void print_intro(void)
{
    uart_write_string("\r\n");
    uart_write_string("==============================\r\n");
    uart_write_string("   STM32 UART Test Harness\r\n");
    uart_write_string("==============================\r\n");
    uart_write_string("\r\n");
    uart_write_string("Enter text then press Return\r\n");
    uart_write_string("STM32 echoes each message\r\n");
    uart_write_string("\r\n> ");
}

static void print_prompt(void)
{
    uart_write_string("\r\n> ");
}

static void echo_line(const char *line)
{
    uart_write_string("\r\nReceived: [");
    uart_write_string(line);
    uart_write_string("]\r\n");
}

int main(void)
{
    char line_buffer[LINE_BUFFER_SIZE];
    size_t line_length = 0u;
    uint32_t last_status_ms = 0u;

    HAL_Init();
    uart_init();

    print_intro();
    last_status_ms = HAL_GetTick();

    for (;;) {
        uint8_t byte = 0u;
        const uint32_t now_ms = HAL_GetTick();

        if ((now_ms - last_status_ms) >= STATUS_PERIOD_MS) {
            print_prompt();
            last_status_ms = now_ms;
        }

        if (uart_read_byte_if_ready(&byte) == 0u) {
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
