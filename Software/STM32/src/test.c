#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"
#include "uart.h"

#define LINE_BUFFER_SIZE 192u

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

    HAL_Init();
    uart_init();

    print_intro();

    while (1) {
        uint8_t byte = 0u;

        if (uart_read_byte_if_ready(&byte) == 0u) {
            continue;
        }

        uart_write_string("\r\nRX\r\n");

        // if ((byte == '\r') || (byte == '\n')) {
        //     if (line_length > 0u) {
        //         line_buffer[line_length] = '\0';
        //         echo_line(line_buffer);
        //         line_length = 0u;
        //         print_prompt();
        //     }
        //     continue;
        // }

        if (line_length < (LINE_BUFFER_SIZE - 1u)) {
            line_buffer[line_length] = (char)byte;
            line_length++;
        } else {
            line_buffer[line_length] = '\0';
            echo_line(line_buffer);
            line_length = 0u;
            print_prompt();
        }
    }
}
