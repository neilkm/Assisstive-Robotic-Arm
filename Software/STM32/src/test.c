#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"
#include "UART_common.h"

#define LINE_BUFFER_SIZE 96u

static void echo_line(const char *line)
{
    (void)uart_put_string("received: ", UART_TIMEOUT_FOREVER);
    (void)uart_put_string(line, UART_TIMEOUT_FOREVER);
    (void)uart_put_string("\r\n", UART_TIMEOUT_FOREVER);
}

int main(void)
{
    char line_buffer[LINE_BUFFER_SIZE];
    size_t line_length = 0u;

    HAL_Init();
    uart_init();

    (void)uart_put_string("STM32 UART echo ready\r\n", UART_TIMEOUT_FOREVER);

    for (;;) {
        uint8_t byte = 0u;

        if (uart_get_char(&byte, UART_TIMEOUT_FOREVER) != UART_OK) {
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
