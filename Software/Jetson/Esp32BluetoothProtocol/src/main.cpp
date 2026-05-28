#include "bluetooth_uart_driver.h"
#include "packet.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

struct Options {
    std::string device = "/dev/rfcomm0";
    int baudrate = 115200;
    int duration_ms = 10000;
    bool rx_hex = false;
};

void printUsage(const char* argv0)
{
    std::printf("Usage: %s [--device /dev/rfcomm0] [--baud 115200] [--duration-ms 10000] [--rx-hex]\n", argv0);
}

bool parseInt(const char* text, int* out)
{
    if (text == nullptr || out == nullptr) {
        return false;
    }
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (*text == '\0' || *end != '\0' || value < 0 || value > 10000000L) {
        return false;
    }
    *out = static_cast<int>(value);
    return true;
}

bool parseOptions(int argc, char** argv, Options* options)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            options->device = argv[++i];
        } else if (std::strcmp(argv[i], "--baud") == 0 && i + 1 < argc) {
            if (!parseInt(argv[++i], &options->baudrate)) {
                return false;
            }
        } else if (std::strcmp(argv[i], "--duration-ms") == 0 && i + 1 < argc) {
            if (!parseInt(argv[++i], &options->duration_ms)) {
                return false;
            }
        } else if (std::strcmp(argv[i], "--rx-hex") == 0) {
            options->rx_hex = true;
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            std::exit(0);
        } else {
            return false;
        }
    }
    return true;
}

void printButtonState(uint8_t sequence, const esp32_button_state_t& state)
{
    std::printf("rx_state seq=%" PRIu8 " mask=0x%02" PRIx8 " buttons=[", sequence, state.buttons_pressed_mask);
    for (uint8_t i = 0u; i < ESP32_BUTTON_COUNT; ++i) {
        std::printf("%s%s", (state.buttons_pressed_mask & (1u << i)) != 0u ? "on" : "off",
                    i + 1u < ESP32_BUTTON_COUNT ? "," : "");
    }
    std::printf("]\n");
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parseOptions(argc, argv, &options)) {
        printUsage(argv[0]);
        return 2;
    }

    BluetoothUartDriver uart;
    std::string error;
    if (!uart.openPort(options.device, options.baudrate, &error)) {
        std::fprintf(stderr, "Bluetooth UART open failed: %s\n", error.c_str());
        return 1;
    }

    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);

    uint8_t rx_bytes[128];
    uint8_t ack_frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    size_t total_rx_bytes = 0u;
    size_t valid_frames = 0u;
    size_t state_frames = 0u;
    size_t ack_tx_frames = 0u;
    size_t non_state_frames = 0u;

    std::printf("listening device=%s duration_ms=%d\n", options.device.c_str(), options.duration_ms);

    int elapsed_ms = 0;
    while (elapsed_ms < options.duration_ms) {
        const int poll_ms = 20;
        const ssize_t count = uart.readBytes(rx_bytes, sizeof(rx_bytes), poll_ms, &error);
        if (count < 0) {
            std::fprintf(stderr, "Bluetooth UART read failed: %s\n", error.c_str());
            return 1;
        }
        elapsed_ms += poll_ms;
        if (count == 0) {
            continue;
        }

        total_rx_bytes += static_cast<size_t>(count);
        for (ssize_t byte_index = 0; byte_index < count; ++byte_index) {
            if (options.rx_hex) {
                std::printf("%02X ", rx_bytes[byte_index]);
            }

            if (!esp32_button_parser_feed(&parser, rx_bytes[byte_index])) {
                continue;
            }

            ++valid_frames;
            esp32_button_state_t state {};
            uint8_t sequence = 0u;
            if (esp32_button_decode_state_packet(parser.bytes, parser.frame_length, &state, &sequence)) {
                ++state_frames;
                printButtonState(sequence, state);

                const size_t ack_len = esp32_button_build_ack_packet(sequence, ack_frame, sizeof(ack_frame));
                if (ack_len == 0u || !uart.writeAll(ack_frame, ack_len, &error)) {
                    std::fprintf(stderr, "Bluetooth UART ACK write failed: %s\n", error.c_str());
                    return 1;
                }
                ++ack_tx_frames;
                std::printf("tx_ack seq=%" PRIu8 "\n", sequence);
            } else {
                ++non_state_frames;
            }
        }
    }

    if (options.rx_hex) {
        std::printf("\n");
    }
    std::printf("rx_bytes=%zu\n", total_rx_bytes);
    std::printf("valid_frames=%zu\n", valid_frames);
    std::printf("state_frames=%zu\n", state_frames);
    std::printf("ack_tx_frames=%zu\n", ack_tx_frames);
    std::printf("non_state_frames=%zu\n", non_state_frames);

    if (state_frames == 0u) {
        std::printf("No ESP32 button frames were observed. Check Bluetooth pairing, rfcomm binding, ESP32 power, and the device path.\n");
        return 1;
    }

    return 0;
}
