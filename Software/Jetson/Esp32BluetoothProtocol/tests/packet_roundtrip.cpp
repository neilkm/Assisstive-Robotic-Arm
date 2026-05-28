#include "packet.h"

#include <cstdio>

namespace {

bool feedFrame(const uint8_t* frame, size_t frame_len, esp32_button_parser_t* parser)
{
    for (size_t i = 0u; i < frame_len; ++i) {
        if (esp32_button_parser_feed(parser, frame[i])) {
            return true;
        }
    }
    return false;
}

} // namespace

int main()
{
    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);

    esp32_button_state_t state_tx {};
    state_tx.buttons_pressed_mask = 0b00101101u;
    const size_t state_len = esp32_button_build_state_packet(&state_tx, 42u, frame, sizeof(frame));
    if (state_len == 0u || !feedFrame(frame, state_len, &parser)) {
        std::fprintf(stderr, "state packet did not round-trip through parser\n");
        return 1;
    }

    esp32_button_state_t state_rx {};
    uint8_t state_sequence = 0u;
    if (!esp32_button_decode_state_packet(parser.bytes, parser.frame_length, &state_rx, &state_sequence)) {
        std::fprintf(stderr, "state packet decode failed\n");
        return 1;
    }
    if (state_sequence != 42u || state_rx.buttons_pressed_mask != state_tx.buttons_pressed_mask) {
        std::fprintf(stderr, "state packet mismatch\n");
        return 1;
    }

    const size_t ack_len = esp32_button_build_ack_packet(state_sequence, frame, sizeof(frame));
    if (ack_len == 0u || !feedFrame(frame, ack_len, &parser)) {
        std::fprintf(stderr, "ack packet did not round-trip through parser\n");
        return 1;
    }

    uint8_t ack_sequence = 0u;
    if (!esp32_button_decode_ack_packet(parser.bytes, parser.frame_length, &ack_sequence)) {
        std::fprintf(stderr, "ack packet decode failed\n");
        return 1;
    }
    if (ack_sequence != state_sequence) {
        std::fprintf(stderr, "ack sequence mismatch\n");
        return 1;
    }

    frame[ack_len - 1u] ^= 0x01u;
    esp32_button_parser_init(&parser);
    if (feedFrame(frame, ack_len, &parser)) {
        std::fprintf(stderr, "corrupt packet passed CRC\n");
        return 1;
    }

    return 0;
}
