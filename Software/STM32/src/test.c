#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "stm32f4xx_hal.h"
#include "test_format.h"
#include "UART_common.h"

#define LINE_BUFFER_SIZE 96u
#define STATUS_PERIOD_MS 10000u
#define STATUS_BUFFER_SIZE 96u

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
    common_test_write_harness_banner(uart_format_writer, NULL, "STM32 UART Echo Firmware");
    common_test_write_text(uart_format_writer, NULL, "Board: Nucleo-F446RE\r\n");
    common_test_write_text(uart_format_writer, NULL, "UART: USART2 via ST-LINK VCP, 115200 baud, 8N1\r\n");
    common_test_write_text(uart_format_writer, NULL, "Type a line and press Enter. The STM32 will echo it back.\r\n");
    common_test_write_text(uart_format_writer, NULL, "A status line prints every 10 seconds while the firmware runs.\r\n");
    common_test_write_newline(uart_format_writer, NULL);
}

static void print_status_message(size_t line_length)
{
    char status_buffer[STATUS_BUFFER_SIZE];
    const uart_ring_buf_t *ring_buffer = uart_get_ring_buffer();
    const uint32_t uptime_seconds = HAL_GetTick() / 1000u;

    (void)snprintf(status_buffer,
                   sizeof(status_buffer),
                   "[STATUS] uptime=%lus rx=%u/%u tx=%u/%u line=%u\r\n",
                   (unsigned long)uptime_seconds,
                   (unsigned int)ring_buffer->rx_count,
                   (unsigned int)UART_RX_BUF_SIZE,
                   (unsigned int)ring_buffer->tx_count,
                   (unsigned int)UART_TX_BUF_SIZE,
                   (unsigned int)line_length);
    uart_write_string_blocking(status_buffer);
}

static void echo_line(const char *line)
{
    uart_write_string_blocking("received: ");
    uart_write_string_blocking(line);
    uart_write_string_blocking("\r\n");
}

int main(void)
{
    char line_buffer[LINE_BUFFER_SIZE];
    size_t line_length = 0u;
    uint32_t last_status_ms = 0u;

    HAL_Init();
    uart_init();

    print_intro_message();
    print_status_message(line_length);
    last_status_ms = HAL_GetTick();

    for (;;) {
        uint8_t byte = 0u;
        const uint32_t now_ms = HAL_GetTick();

        if ((now_ms - last_status_ms) >= STATUS_PERIOD_MS) {
            print_status_message(line_length);
            last_status_ms = now_ms;
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
