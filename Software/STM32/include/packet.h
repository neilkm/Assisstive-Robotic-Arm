#ifndef ARM_UART_PACKET_H
#define ARM_UART_PACKET_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARM_UART_JOINT_COUNT 7u
#define ARM_UART_MAGIC0 0xA5u
#define ARM_UART_MAGIC1 0x5Au
#define ARM_UART_VERSION 1u
#define ARM_UART_MAX_PAYLOAD_SIZE 32u
#define ARM_UART_HEADER_SIZE 6u
#define ARM_UART_CRC_SIZE 2u
#define ARM_UART_MAX_FRAME_SIZE (ARM_UART_HEADER_SIZE + ARM_UART_MAX_PAYLOAD_SIZE + ARM_UART_CRC_SIZE)
#define ARM_UART_DESIRED_PAYLOAD_SIZE (ARM_UART_JOINT_COUNT * sizeof(float))
#define ARM_UART_ACTUAL_PAYLOAD_SIZE ((ARM_UART_JOINT_COUNT + 1u) * sizeof(float))

typedef enum {
    ARM_UART_PACKET_ACTUAL_STATE = 0x01u,
    ARM_UART_PACKET_DESIRED_STATE = 0x02u
} arm_uart_packet_type_t;

typedef struct {
    float actual_joint_angles[ARM_UART_JOINT_COUNT];
    float force_sensor;
} arm_actual_state_t;

typedef struct {
    float desired_joint_angles[ARM_UART_JOINT_COUNT];
} arm_desired_state_t;

typedef struct {
    uint8_t bytes[ARM_UART_MAX_FRAME_SIZE];
    size_t length;
    size_t expected_length;
    size_t frame_length;
} arm_uart_parser_t;

static inline uint16_t arm_uart_crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0u; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8u;
        for (uint8_t bit = 0u; bit < 8u; ++bit) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((crc << 1u) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1u);
            }
        }
    }
    return crc;
}

static inline void arm_uart_parser_init(arm_uart_parser_t *parser)
{
    if (parser == NULL) {
        return;
    }

    parser->length = 0u;
    parser->expected_length = 0u;
    parser->frame_length = 0u;
}

static inline void arm_uart_write_float_le(uint8_t *dst, float value)
{
    uint32_t raw = 0u;
    memcpy(&raw, &value, sizeof(raw));
    dst[0] = (uint8_t)(raw & 0xFFu);
    dst[1] = (uint8_t)((raw >> 8u) & 0xFFu);
    dst[2] = (uint8_t)((raw >> 16u) & 0xFFu);
    dst[3] = (uint8_t)((raw >> 24u) & 0xFFu);
}

static inline float arm_uart_read_float_le(const uint8_t *src)
{
    uint32_t raw = ((uint32_t)src[0]) |
                   ((uint32_t)src[1] << 8u) |
                   ((uint32_t)src[2] << 16u) |
                   ((uint32_t)src[3] << 24u);
    float value = 0.0f;
    memcpy(&value, &raw, sizeof(value));
    return value;
}

static inline size_t arm_uart_build_frame(uint8_t type,
                                          uint8_t sequence,
                                          const uint8_t *payload,
                                          uint8_t payload_len,
                                          uint8_t *out_frame,
                                          size_t out_capacity)
{
    if (out_frame == NULL ||
        payload == NULL ||
        payload_len > ARM_UART_MAX_PAYLOAD_SIZE ||
        out_capacity < (size_t)(ARM_UART_HEADER_SIZE + payload_len + ARM_UART_CRC_SIZE)) {
        return 0u;
    }

    out_frame[0] = ARM_UART_MAGIC0;
    out_frame[1] = ARM_UART_MAGIC1;
    out_frame[2] = ARM_UART_VERSION;
    out_frame[3] = type;
    out_frame[4] = sequence;
    out_frame[5] = payload_len;
    memcpy(&out_frame[ARM_UART_HEADER_SIZE], payload, payload_len);

    const uint16_t crc = arm_uart_crc16_ccitt(&out_frame[2], 4u + payload_len);
    out_frame[ARM_UART_HEADER_SIZE + payload_len] = (uint8_t)(crc & 0xFFu);
    out_frame[ARM_UART_HEADER_SIZE + payload_len + 1u] = (uint8_t)((crc >> 8u) & 0xFFu);

    return (size_t)(ARM_UART_HEADER_SIZE + payload_len + ARM_UART_CRC_SIZE);
}

static inline size_t arm_uart_build_actual_state_packet(const arm_actual_state_t *state,
                                                        uint8_t sequence,
                                                        uint8_t *out_frame,
                                                        size_t out_capacity)
{
    if (state == NULL) {
        return 0u;
    }

    uint8_t payload[ARM_UART_ACTUAL_PAYLOAD_SIZE];
    for (size_t i = 0u; i < ARM_UART_JOINT_COUNT; ++i) {
        arm_uart_write_float_le(&payload[i * sizeof(float)], state->actual_joint_angles[i]);
    }
    arm_uart_write_float_le(&payload[ARM_UART_JOINT_COUNT * sizeof(float)], state->force_sensor);

    return arm_uart_build_frame((uint8_t)ARM_UART_PACKET_ACTUAL_STATE,
                                sequence,
                                payload,
                                (uint8_t)sizeof(payload),
                                out_frame,
                                out_capacity);
}

static inline size_t arm_uart_build_desired_state_packet(const arm_desired_state_t *state,
                                                         uint8_t sequence,
                                                         uint8_t *out_frame,
                                                         size_t out_capacity)
{
    if (state == NULL) {
        return 0u;
    }

    uint8_t payload[ARM_UART_DESIRED_PAYLOAD_SIZE];
    for (size_t i = 0u; i < ARM_UART_JOINT_COUNT; ++i) {
        arm_uart_write_float_le(&payload[i * sizeof(float)], state->desired_joint_angles[i]);
    }

    return arm_uart_build_frame((uint8_t)ARM_UART_PACKET_DESIRED_STATE,
                                sequence,
                                payload,
                                (uint8_t)sizeof(payload),
                                out_frame,
                                out_capacity);
}

static inline int arm_uart_decode_actual_state_packet(const uint8_t *frame,
                                                      size_t frame_len,
                                                      arm_actual_state_t *state,
                                                      uint8_t *sequence)
{
    if (frame == NULL ||
        state == NULL ||
        frame_len != (size_t)(ARM_UART_HEADER_SIZE + ARM_UART_ACTUAL_PAYLOAD_SIZE + ARM_UART_CRC_SIZE) ||
        frame[3] != (uint8_t)ARM_UART_PACKET_ACTUAL_STATE ||
        frame[5] != ARM_UART_ACTUAL_PAYLOAD_SIZE) {
        return 0;
    }

    if (sequence != NULL) {
        *sequence = frame[4];
    }

    const uint8_t *payload = &frame[ARM_UART_HEADER_SIZE];
    for (size_t i = 0u; i < ARM_UART_JOINT_COUNT; ++i) {
        state->actual_joint_angles[i] = arm_uart_read_float_le(&payload[i * sizeof(float)]);
    }
    state->force_sensor = arm_uart_read_float_le(&payload[ARM_UART_JOINT_COUNT * sizeof(float)]);
    return 1;
}

static inline int arm_uart_decode_desired_state_packet(const uint8_t *frame,
                                                       size_t frame_len,
                                                       arm_desired_state_t *state,
                                                       uint8_t *sequence)
{
    if (frame == NULL ||
        state == NULL ||
        frame_len != (size_t)(ARM_UART_HEADER_SIZE + ARM_UART_DESIRED_PAYLOAD_SIZE + ARM_UART_CRC_SIZE) ||
        frame[3] != (uint8_t)ARM_UART_PACKET_DESIRED_STATE ||
        frame[5] != ARM_UART_DESIRED_PAYLOAD_SIZE) {
        return 0;
    }

    if (sequence != NULL) {
        *sequence = frame[4];
    }

    const uint8_t *payload = &frame[ARM_UART_HEADER_SIZE];
    for (size_t i = 0u; i < ARM_UART_JOINT_COUNT; ++i) {
        state->desired_joint_angles[i] = arm_uart_read_float_le(&payload[i * sizeof(float)]);
    }
    return 1;
}

static inline int arm_uart_parser_feed(arm_uart_parser_t *parser, uint8_t byte)
{
    if (parser == NULL) {
        return 0;
    }

    parser->frame_length = 0u;

    if (parser->length == 0u) {
        if (byte == ARM_UART_MAGIC0) {
            parser->bytes[parser->length++] = byte;
        }
        return 0;
    }

    if (parser->length == 1u) {
        if (byte == ARM_UART_MAGIC1) {
            parser->bytes[parser->length++] = byte;
        } else if (byte == ARM_UART_MAGIC0) {
            parser->bytes[0] = byte;
            parser->length = 1u;
        } else {
            parser->length = 0u;
        }
        return 0;
    }

    parser->bytes[parser->length++] = byte;

    if (parser->length == ARM_UART_HEADER_SIZE) {
        const uint8_t payload_len = parser->bytes[5];
        if (parser->bytes[2] != ARM_UART_VERSION || payload_len > ARM_UART_MAX_PAYLOAD_SIZE) {
            parser->length = 0u;
            parser->expected_length = 0u;
            return 0;
        }
        parser->expected_length = (size_t)(ARM_UART_HEADER_SIZE + payload_len + ARM_UART_CRC_SIZE);
    }

    if (parser->expected_length != 0u && parser->length >= parser->expected_length) {
        const uint8_t payload_len = parser->bytes[5];
        const size_t crc_offset = (size_t)(ARM_UART_HEADER_SIZE + payload_len);
        const uint16_t expected_crc = (uint16_t)parser->bytes[crc_offset] |
                                      ((uint16_t)parser->bytes[crc_offset + 1u] << 8u);
        const uint16_t actual_crc = arm_uart_crc16_ccitt(&parser->bytes[2], 4u + payload_len);
        const int valid = (expected_crc == actual_crc);
        if (valid) {
            parser->frame_length = parser->expected_length;
        }

        if (!valid && parser->bytes[parser->length - 1u] == ARM_UART_MAGIC0) {
            parser->bytes[0] = ARM_UART_MAGIC0;
            parser->length = 1u;
        } else {
            parser->length = 0u;
        }
        parser->expected_length = 0u;
        return valid;
    }

    if (parser->length >= ARM_UART_MAX_FRAME_SIZE) {
        parser->length = 0u;
        parser->expected_length = 0u;
    }

    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
