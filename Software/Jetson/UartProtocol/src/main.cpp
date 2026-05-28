#include "packet.h"
#include "uart_driver.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

namespace {

struct Options {
    std::string device = "/dev/ttyTHS1";
    int baudrate = 115200;
    int iterations = 20;
    int period_ms = 100;
    bool rx_hex = false;
};

void printUsage(const char* program)
{
    std::fprintf(stderr,
                 "Usage: %s [--device PATH] [--baud BAUD] [--iterations N] [--period-ms N] [--rx-hex]\n",
                 program);
}

bool parseInt(const char* text, int* value)
{
    if (text == nullptr || value == nullptr) {
        return false;
    }

    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (*text == '\0' || *end != '\0') {
        return false;
    }
    *value = static_cast<int>(parsed);
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
        } else if (std::strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            if (!parseInt(argv[++i], &options->iterations)) {
                return false;
            }
        } else if (std::strcmp(argv[i], "--period-ms") == 0 && i + 1 < argc) {
            if (!parseInt(argv[++i], &options->period_ms)) {
                return false;
            }
        } else if (std::strcmp(argv[i], "--rx-hex") == 0) {
            options->rx_hex = true;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            std::exit(0);
        } else {
            return false;
        }
    }

    return options->baudrate > 0 && options->iterations > 0 && options->period_ms > 0;
}

arm_desired_state_t makeDesiredState(uint8_t sequence)
{
    arm_desired_state_t state {};
    for (size_t i = 0u; i < ARM_UART_JOINT_COUNT; ++i) {
        state.desired_joint_angles[i] = 100.0f + static_cast<float>(sequence) + (static_cast<float>(i) * 5.0f);
    }
    return state;
}

void printDesired(uint8_t sequence, const arm_desired_state_t& state)
{
    std::printf("TX desired seq=%u angles=[", sequence);
    for (size_t i = 0u; i < ARM_UART_JOINT_COUNT; ++i) {
        std::printf("%s%.3f", (i == 0u) ? "" : ", ", state.desired_joint_angles[i]);
    }
    std::printf("]\n");
}

void printActual(uint8_t sequence, const arm_actual_state_t& state)
{
    std::printf("RX actual  seq=%u angles=[", sequence);
    for (size_t i = 0u; i < ARM_UART_JOINT_COUNT; ++i) {
        std::printf("%s%.3f", (i == 0u) ? "" : ", ", state.actual_joint_angles[i]);
    }
    std::printf("] force=%.3f\n", state.force_sensor);
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parseOptions(argc, argv, &options)) {
        printUsage(argv[0]);
        return 2;
    }

    UartDriver uart;
    std::string error;
    if (!uart.openPort(options.device, options.baudrate, &error)) {
        std::fprintf(stderr, "UART open failed: %s\n", error.c_str());
        return 1;
    }

    std::printf("Opened %s at %d baud\n", options.device.c_str(), options.baudrate);

    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    uint8_t tx_frame[ARM_UART_MAX_FRAME_SIZE];
    uint8_t tx_sequence = 0u;
    int received_packets = 0;
    size_t received_bytes = 0u;
    size_t valid_frames = 0u;
    size_t non_actual_frames = 0u;

    for (int i = 0; i < options.iterations; ++i) {
        const arm_desired_state_t desired = makeDesiredState(tx_sequence);
        const size_t tx_len = arm_uart_build_desired_state_packet(&desired,
                                                                  tx_sequence,
                                                                  tx_frame,
                                                                  sizeof(tx_frame));
        if (tx_len == 0u || !uart.writeAll(tx_frame, tx_len, &error)) {
            std::fprintf(stderr, "UART write failed: %s\n", error.c_str());
            return 1;
        }
        printDesired(tx_sequence, desired);
        tx_sequence++;

        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(options.period_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            uint8_t rx_bytes[64];
            const ssize_t count = uart.readBytes(rx_bytes, sizeof(rx_bytes), 10, &error);
            if (count < 0) {
                std::fprintf(stderr, "UART read failed: %s\n", error.c_str());
                return 1;
            }
            if (count > 0) {
                received_bytes += static_cast<size_t>(count);
                if (options.rx_hex) {
                    std::printf("RX raw    bytes=%zd hex=", count);
                    for (ssize_t byte_index = 0; byte_index < count; ++byte_index) {
                        std::printf("%02X", rx_bytes[byte_index]);
                    }
                    std::printf("\n");
                }
            }

            for (ssize_t byte_index = 0; byte_index < count; ++byte_index) {
                if (arm_uart_parser_feed(&parser, rx_bytes[byte_index])) {
                    valid_frames++;
                    arm_actual_state_t actual {};
                    uint8_t rx_sequence = 0u;
                    if (arm_uart_decode_actual_state_packet(parser.bytes,
                                                            parser.frame_length,
                                                            &actual,
                                                            &rx_sequence)) {
                        printActual(rx_sequence, actual);
                        received_packets++;
                    } else {
                        non_actual_frames++;
                    }
                }
            }
        }
    }

    std::printf("Complete: sent_desired=%d rx_bytes=%zu valid_frames=%zu received_actual=%d non_actual_frames=%zu\n",
                options.iterations,
                received_bytes,
                valid_frames,
                received_packets,
                non_actual_frames);
    if (received_bytes == 0u) {
        std::printf("No RX bytes were observed. Check device selection, TX/RX crossover wiring, common ground, and STM UART pins.\n");
    } else if (valid_frames == 0u) {
        std::printf("RX bytes were observed, but no valid protocol frames decoded. Re-run with --rx-hex to inspect framing.\n");
    }
    return (received_packets > 0) ? 0 : 1;
}
