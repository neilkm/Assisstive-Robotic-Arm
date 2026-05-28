#ifndef ESP32_BUTTON_PACKET_H
#define ESP32_BUTTON_PACKET_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP32_BUTTON_COUNT 6u
#define ESP32_BUTTON_MAGIC0 0xB7u
#define ESP32_BUTTON_MAGIC1 0x32u
#define ESP32_BUTTON_VERSION 1u
#define ESP32_BUTTON_MAX_PAYLOAD_SIZE 1u
#define ESP32_BUTTON_HEADER_SIZE 6u
#define ESP32_BUTTON_CRC_SIZE 2u
#define ESP32_BUTTON_MAX_FRAME_SIZE (ESP32_BUTTON_HEADER_SIZE + ESP32_BUTTON_MAX_PAYLOAD_SIZE + ESP32_BUTTON_CRC_SIZE)
#define ESP32_BUTTON_STATE_PAYLOAD_SIZE 1u
#define ESP32_BUTTON_ACK_PAYLOAD_SIZE 0u

typedef enum {
    ESP32_BUTTON_PACKET_STATE = 0x10u,
    ESP32_BUTTON_PACKET_ACK = 0x11u
} esp32_button_packet_type_t;

typedef struct {
    uint8_t buttons_pressed_mask;
} esp32_button_state_t;

typedef struct {
    uint8_t bytes[ESP32_BUTTON_MAX_FRAME_SIZE];
    size_t length;
    size_t expected_length;
    size_t frame_length;
} esp32_button_parser_t;

static inline uint16_t esp32_button_crc16_ccitt(const uint8_t *data, size_t len)
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

static inline void esp32_button_parser_init(esp32_button_parser_t *parser)
{
    if (parser == NULL) {
        return;
    }

    parser->length = 0u;
    parser->expected_length = 0u;
    parser->frame_length = 0u;
}

static inline size_t esp32_button_build_frame(uint8_t type,
                                             uint8_t sequence,
                                             const uint8_t *payload,
                                             uint8_t payload_len,
                                             uint8_t *out_frame,
                                             size_t out_capacity)
{
    if (out_frame == NULL ||
        (payload == NULL && payload_len != 0u) ||
        payload_len > ESP32_BUTTON_MAX_PAYLOAD_SIZE ||
        out_capacity < (size_t)(ESP32_BUTTON_HEADER_SIZE + payload_len + ESP32_BUTTON_CRC_SIZE)) {
        return 0u;
    }

    out_frame[0] = ESP32_BUTTON_MAGIC0;
    out_frame[1] = ESP32_BUTTON_MAGIC1;
    out_frame[2] = ESP32_BUTTON_VERSION;
    out_frame[3] = type;
    out_frame[4] = sequence;
    out_frame[5] = payload_len;
    if (payload_len != 0u) {
        memcpy(&out_frame[ESP32_BUTTON_HEADER_SIZE], payload, payload_len);
    }

    const uint16_t crc = esp32_button_crc16_ccitt(&out_frame[2], 4u + payload_len);
    out_frame[ESP32_BUTTON_HEADER_SIZE + payload_len] = (uint8_t)(crc & 0xFFu);
    out_frame[ESP32_BUTTON_HEADER_SIZE + payload_len + 1u] = (uint8_t)((crc >> 8u) & 0xFFu);

    return (size_t)(ESP32_BUTTON_HEADER_SIZE + payload_len + ESP32_BUTTON_CRC_SIZE);
}

static inline size_t esp32_button_build_state_packet(const esp32_button_state_t *state,
                                                    uint8_t sequence,
                                                    uint8_t *out_frame,
                                                    size_t out_capacity)
{
    if (state == NULL) {
        return 0u;
    }

    const uint8_t payload = (uint8_t)(state->buttons_pressed_mask & ((1u << ESP32_BUTTON_COUNT) - 1u));
    return esp32_button_build_frame((uint8_t)ESP32_BUTTON_PACKET_STATE,
                                    sequence,
                                    &payload,
                                    ESP32_BUTTON_STATE_PAYLOAD_SIZE,
                                    out_frame,
                                    out_capacity);
}

static inline size_t esp32_button_build_ack_packet(uint8_t acknowledged_sequence,
                                                  uint8_t *out_frame,
                                                  size_t out_capacity)
{
    return esp32_button_build_frame((uint8_t)ESP32_BUTTON_PACKET_ACK,
                                    acknowledged_sequence,
                                    NULL,
                                    ESP32_BUTTON_ACK_PAYLOAD_SIZE,
                                    out_frame,
                                    out_capacity);
}

static inline int esp32_button_decode_state_packet(const uint8_t *frame,
                                                  size_t frame_len,
                                                  esp32_button_state_t *state,
                                                  uint8_t *sequence)
{
    if (frame == NULL ||
        state == NULL ||
        frame_len != (size_t)(ESP32_BUTTON_HEADER_SIZE + ESP32_BUTTON_STATE_PAYLOAD_SIZE + ESP32_BUTTON_CRC_SIZE) ||
        frame[3] != (uint8_t)ESP32_BUTTON_PACKET_STATE ||
        frame[5] != ESP32_BUTTON_STATE_PAYLOAD_SIZE) {
        return 0;
    }

    if (sequence != NULL) {
        *sequence = frame[4];
    }
    state->buttons_pressed_mask = (uint8_t)(frame[ESP32_BUTTON_HEADER_SIZE] & ((1u << ESP32_BUTTON_COUNT) - 1u));
    return 1;
}

static inline int esp32_button_decode_ack_packet(const uint8_t *frame,
                                                size_t frame_len,
                                                uint8_t *acknowledged_sequence)
{
    if (frame == NULL ||
        frame_len != (size_t)(ESP32_BUTTON_HEADER_SIZE + ESP32_BUTTON_ACK_PAYLOAD_SIZE + ESP32_BUTTON_CRC_SIZE) ||
        frame[3] != (uint8_t)ESP32_BUTTON_PACKET_ACK ||
        frame[5] != ESP32_BUTTON_ACK_PAYLOAD_SIZE) {
        return 0;
    }

    if (acknowledged_sequence != NULL) {
        *acknowledged_sequence = frame[4];
    }
    return 1;
}

static inline int esp32_button_parser_feed(esp32_button_parser_t *parser, uint8_t byte)
{
    if (parser == NULL) {
        return 0;
    }

    parser->frame_length = 0u;

    if (parser->length == 0u) {
        if (byte == ESP32_BUTTON_MAGIC0) {
            parser->bytes[parser->length++] = byte;
        }
        return 0;
    }

    if (parser->length == 1u) {
        if (byte == ESP32_BUTTON_MAGIC1) {
            parser->bytes[parser->length++] = byte;
        } else if (byte == ESP32_BUTTON_MAGIC0) {
            parser->bytes[0] = byte;
            parser->length = 1u;
        } else {
            parser->length = 0u;
        }
        return 0;
    }

    parser->bytes[parser->length++] = byte;

    if (parser->length == ESP32_BUTTON_HEADER_SIZE) {
        const uint8_t payload_len = parser->bytes[5];
        if (parser->bytes[2] != ESP32_BUTTON_VERSION || payload_len > ESP32_BUTTON_MAX_PAYLOAD_SIZE) {
            parser->length = 0u;
            parser->expected_length = 0u;
            return 0;
        }
        parser->expected_length = (size_t)(ESP32_BUTTON_HEADER_SIZE + payload_len + ESP32_BUTTON_CRC_SIZE);
    }

    if (parser->expected_length != 0u && parser->length >= parser->expected_length) {
        const uint8_t payload_len = parser->bytes[5];
        const size_t crc_offset = (size_t)(ESP32_BUTTON_HEADER_SIZE + payload_len);
        const uint16_t expected_crc = (uint16_t)parser->bytes[crc_offset] |
                                      ((uint16_t)parser->bytes[crc_offset + 1u] << 8u);
        const uint16_t actual_crc = esp32_button_crc16_ccitt(&parser->bytes[2], 4u + payload_len);
        const int valid = (expected_crc == actual_crc);
        if (valid) {
            parser->frame_length = parser->expected_length;
        }

        if (!valid && parser->bytes[parser->length - 1u] == ESP32_BUTTON_MAGIC0) {
            parser->bytes[0] = ESP32_BUTTON_MAGIC0;
            parser->length = 1u;
        } else {
            parser->length = 0u;
        }
        parser->expected_length = 0u;
        return valid;
    }

    if (parser->length >= ESP32_BUTTON_MAX_FRAME_SIZE) {
        parser->length = 0u;
        parser->expected_length = 0u;
    }

    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
