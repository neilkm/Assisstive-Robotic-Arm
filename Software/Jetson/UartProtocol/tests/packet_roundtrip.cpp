#include "packet.h"

#include <cmath>
#include <cstdio>

namespace {

bool nearEqual(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) < 0.0001f;
}

bool feedFrame(const uint8_t* frame, size_t frame_len, arm_uart_parser_t* parser)
{
    for (size_t i = 0u; i < frame_len; ++i) {
        if (arm_uart_parser_feed(parser, frame[i])) {
            return true;
        }
    }
    return false;
}

} // namespace

int main()
{
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    arm_actual_state_t actual_tx {};
    for (size_t i = 0u; i < ARM_UART_JOINT_COUNT; ++i) {
        actual_tx.actual_joint_angles[i] = 10.0f + static_cast<float>(i);
    }
    actual_tx.force_sensor = 42.25f;

    const size_t actual_len = arm_uart_build_actual_state_packet(&actual_tx, 7u, frame, sizeof(frame));
    if (actual_len == 0u || !feedFrame(frame, actual_len, &parser)) {
        std::fprintf(stderr, "actual packet did not build or parse\n");
        return 1;
    }

    arm_actual_state_t actual_rx {};
    uint8_t actual_sequence = 0u;
    if (!arm_uart_decode_actual_state_packet(parser.bytes, parser.frame_length, &actual_rx, &actual_sequence)) {
        std::fprintf(stderr, "actual packet did not decode\n");
        return 1;
    }
    if (actual_sequence != 7u || !nearEqual(actual_rx.force_sensor, actual_tx.force_sensor)) {
        std::fprintf(stderr, "actual sequence or force mismatch\n");
        return 1;
    }
    for (size_t i = 0u; i < ARM_UART_JOINT_COUNT; ++i) {
        if (!nearEqual(actual_rx.actual_joint_angles[i], actual_tx.actual_joint_angles[i])) {
            std::fprintf(stderr, "actual angle mismatch at %zu\n", i);
            return 1;
        }
    }

    arm_desired_state_t desired_tx {};
    for (size_t i = 0u; i < ARM_UART_JOINT_COUNT; ++i) {
        desired_tx.desired_joint_angles[i] = 100.0f + static_cast<float>(i * 2u);
    }

    const size_t desired_len = arm_uart_build_desired_state_packet(&desired_tx, 9u, frame, sizeof(frame));
    if (desired_len == 0u || !feedFrame(frame, desired_len, &parser)) {
        std::fprintf(stderr, "desired packet did not build or parse\n");
        return 1;
    }

    arm_desired_state_t desired_rx {};
    uint8_t desired_sequence = 0u;
    if (!arm_uart_decode_desired_state_packet(parser.bytes, parser.frame_length, &desired_rx, &desired_sequence)) {
        std::fprintf(stderr, "desired packet did not decode\n");
        return 1;
    }
    if (desired_sequence != 9u) {
        std::fprintf(stderr, "desired sequence mismatch\n");
        return 1;
    }
    for (size_t i = 0u; i < ARM_UART_JOINT_COUNT; ++i) {
        if (!nearEqual(desired_rx.desired_joint_angles[i], desired_tx.desired_joint_angles[i])) {
            std::fprintf(stderr, "desired angle mismatch at %zu\n", i);
            return 1;
        }
    }

    std::puts("packet round-trip passed");
    return 0;
}
