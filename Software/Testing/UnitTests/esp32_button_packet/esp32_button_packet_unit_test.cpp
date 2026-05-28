#include "esp32_button_packet.h"

#include <gtest/gtest.h>

namespace {

bool feedFrame(const uint8_t* frame, size_t len, esp32_button_parser_t* parser)
{
    for (size_t i = 0; i < len; ++i) {
        if (esp32_button_parser_feed(parser, frame[i])) return true;
    }
    return false;
}

} // namespace

// ─── CRC ─────────────────────────────────────────────────────────────────────

TEST(Esp32ButtonCrc, SameInputSameOutput)
{
    const uint8_t d[] = {0x10, 0x05, 0x01, 0x2B};
    EXPECT_EQ(esp32_button_crc16_ccitt(d, sizeof(d)), esp32_button_crc16_ccitt(d, sizeof(d)));
}

TEST(Esp32ButtonCrc, DifferentInputDifferentOutput)
{
    const uint8_t d1[] = {0x10, 0x01};
    const uint8_t d2[] = {0x10, 0x02};
    EXPECT_NE(esp32_button_crc16_ccitt(d1, 2), esp32_button_crc16_ccitt(d2, 2));
}

// ─── Build-frame guards ───────────────────────────────────────────────────────

TEST(Esp32ButtonBuildFrame, ValidStateFrameHasMagic)
{
    const uint8_t payload = 0x3F;
    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    const size_t len = esp32_button_build_frame(
        static_cast<uint8_t>(ESP32_BUTTON_PACKET_STATE), 1u,
        &payload, 1u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    EXPECT_EQ(frame[0], ESP32_BUTTON_MAGIC0);
    EXPECT_EQ(frame[1], ESP32_BUTTON_MAGIC1);
    EXPECT_EQ(frame[2], ESP32_BUTTON_VERSION);
}

TEST(Esp32ButtonBuildFrame, NullOutputReturnsZero)
{
    const uint8_t payload = 0x01;
    EXPECT_EQ(esp32_button_build_frame(0x10, 0, &payload, 1, nullptr, 8), 0u);
}

TEST(Esp32ButtonBuildFrame, NullPayloadWithNonzeroLenReturnsZero)
{
    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    EXPECT_EQ(esp32_button_build_frame(0x10, 0, nullptr, 1, frame, sizeof(frame)), 0u);
}

TEST(Esp32ButtonBuildFrame, OversizedPayloadReturnsZero)
{
    const uint8_t payload[4] = {1, 2, 3, 4};
    uint8_t frame[20];
    EXPECT_EQ(esp32_button_build_frame(0x10, 0, payload, 4, frame, sizeof(frame)), 0u);
}

TEST(Esp32ButtonBuildFrame, CapacityTooSmallReturnsZero)
{
    const uint8_t payload = 0x01;
    uint8_t frame[4]; // too small for header(6) + payload(1) + crc(2)
    EXPECT_EQ(esp32_button_build_frame(0x10, 0, &payload, 1, frame, sizeof(frame)), 0u);
}

// ─── State packet roundtrip ───────────────────────────────────────────────────

TEST(Esp32ButtonStatePacket, AllButtonsPressed)
{
    esp32_button_state_t tx {};
    tx.buttons_pressed_mask = 0x3Fu; // bits 0-5 set

    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);

    const size_t len = esp32_button_build_state_packet(&tx, 10u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    ASSERT_TRUE(feedFrame(frame, len, &parser));

    esp32_button_state_t rx {};
    uint8_t seq = 0;
    ASSERT_TRUE(esp32_button_decode_state_packet(parser.bytes, parser.frame_length, &rx, &seq));
    EXPECT_EQ(seq, 10u);
    EXPECT_EQ(rx.buttons_pressed_mask, 0x3Fu);
}

TEST(Esp32ButtonStatePacket, NoButtonsPressed)
{
    esp32_button_state_t tx {};
    tx.buttons_pressed_mask = 0x00u;

    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);

    ASSERT_GT(esp32_button_build_state_packet(&tx, 0u, frame, sizeof(frame)), 0u);
    ASSERT_TRUE(feedFrame(frame, esp32_button_build_state_packet(&tx, 0u, frame, sizeof(frame)), &parser));

    esp32_button_state_t rx {};
    ASSERT_TRUE(esp32_button_decode_state_packet(parser.bytes, parser.frame_length, &rx, nullptr));
    EXPECT_EQ(rx.buttons_pressed_mask, 0x00u);
}

TEST(Esp32ButtonStatePacket, EachButtonIndividually)
{
    for (uint8_t bit = 0; bit < ESP32_BUTTON_COUNT; ++bit) {
        const uint8_t mask = static_cast<uint8_t>(1u << bit);
        esp32_button_state_t tx {};
        tx.buttons_pressed_mask = mask;

        uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
        esp32_button_parser_t parser;
        esp32_button_parser_init(&parser);

        const size_t len = esp32_button_build_state_packet(&tx, bit, frame, sizeof(frame));
        ASSERT_GT(len, 0u) << "bit=" << (int)bit;
        ASSERT_TRUE(feedFrame(frame, len, &parser)) << "bit=" << (int)bit;

        esp32_button_state_t rx {};
        uint8_t seq = 0;
        ASSERT_TRUE(esp32_button_decode_state_packet(parser.bytes, parser.frame_length, &rx, &seq));
        EXPECT_EQ(seq, bit);
        EXPECT_EQ(rx.buttons_pressed_mask, mask) << "bit=" << (int)bit;
    }
}

TEST(Esp32ButtonStatePacket, HighBitsStripped)
{
    esp32_button_state_t tx {};
    tx.buttons_pressed_mask = 0xFFu; // bits 6 and 7 should be stripped

    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);

    const size_t len = esp32_button_build_state_packet(&tx, 0u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    ASSERT_TRUE(feedFrame(frame, len, &parser));

    esp32_button_state_t rx {};
    ASSERT_TRUE(esp32_button_decode_state_packet(parser.bytes, parser.frame_length, &rx, nullptr));
    EXPECT_EQ(rx.buttons_pressed_mask, 0x3Fu); // only lower 6 bits
}

TEST(Esp32ButtonStatePacket, NullStateReturnsZero)
{
    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    EXPECT_EQ(esp32_button_build_state_packet(nullptr, 0u, frame, sizeof(frame)), 0u);
}

// ─── ACK packet roundtrip ─────────────────────────────────────────────────────

TEST(Esp32ButtonAckPacket, Roundtrip)
{
    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);

    const size_t len = esp32_button_build_ack_packet(42u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    ASSERT_TRUE(feedFrame(frame, len, &parser));

    uint8_t seq = 0;
    ASSERT_TRUE(esp32_button_decode_ack_packet(parser.bytes, parser.frame_length, &seq));
    EXPECT_EQ(seq, 42u);
}

TEST(Esp32ButtonAckPacket, HasZeroPayloadLength)
{
    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    const size_t len = esp32_button_build_ack_packet(0u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    EXPECT_EQ(frame[5], 0u); // payload_len field
}

TEST(Esp32ButtonAckPacket, AllSequenceNumbers)
{
    for (int seq = 0; seq <= 255; seq += 17) { // sample every 17th
        uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
        esp32_button_parser_t parser;
        esp32_button_parser_init(&parser);

        const size_t len = esp32_button_build_ack_packet(static_cast<uint8_t>(seq), frame, sizeof(frame));
        ASSERT_GT(len, 0u);
        ASSERT_TRUE(feedFrame(frame, len, &parser));

        uint8_t rseq = 0;
        ASSERT_TRUE(esp32_button_decode_ack_packet(parser.bytes, parser.frame_length, &rseq));
        EXPECT_EQ(rseq, static_cast<uint8_t>(seq));
    }
}

// ─── Parser ───────────────────────────────────────────────────────────────────

TEST(Esp32ButtonParser, CorruptCrcStateRejected)
{
    esp32_button_state_t tx {};
    tx.buttons_pressed_mask = 0x15u;

    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);

    const size_t len = esp32_button_build_state_packet(&tx, 1u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    frame[len - 1u] ^= 0x01u;
    EXPECT_FALSE(feedFrame(frame, len, &parser));
}

TEST(Esp32ButtonParser, CorruptCrcAckRejected)
{
    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);

    const size_t len = esp32_button_build_ack_packet(5u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    frame[len - 1u] ^= 0x01u;
    EXPECT_FALSE(feedFrame(frame, len, &parser));
}

TEST(Esp32ButtonParser, GarbageBeforeMagic)
{
    esp32_button_state_t tx {};
    tx.buttons_pressed_mask = 0x2Au;

    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);

    const size_t len = esp32_button_build_state_packet(&tx, 3u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);

    const uint8_t garbage[] = {0x00, 0xFF, 0xAB, 0xCD};
    for (uint8_t b : garbage) {
        esp32_button_parser_feed(&parser, b);
    }
    EXPECT_TRUE(feedFrame(frame, len, &parser));
}

TEST(Esp32ButtonParser, SequentialStateAndAckFrames)
{
    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);

    for (uint8_t seq = 0; seq < 10; ++seq) {
        esp32_button_state_t tx {};
        tx.buttons_pressed_mask = seq;
        const size_t slen = esp32_button_build_state_packet(&tx, seq, frame, sizeof(frame));
        ASSERT_TRUE(feedFrame(frame, slen, &parser)) << "state seq=" << (int)seq;

        esp32_button_state_t rx {};
        uint8_t rseq = 0;
        ASSERT_TRUE(esp32_button_decode_state_packet(parser.bytes, parser.frame_length, &rx, &rseq));
        EXPECT_EQ(rseq, seq);
        EXPECT_EQ(rx.buttons_pressed_mask, static_cast<uint8_t>(seq & 0x3Fu));

        const size_t alen = esp32_button_build_ack_packet(seq, frame, sizeof(frame));
        ASSERT_TRUE(feedFrame(frame, alen, &parser)) << "ack seq=" << (int)seq;

        uint8_t aseq = 0;
        ASSERT_TRUE(esp32_button_decode_ack_packet(parser.bytes, parser.frame_length, &aseq));
        EXPECT_EQ(aseq, seq);
    }
}

TEST(Esp32ButtonParser, BadVersionRejected)
{
    esp32_button_state_t tx {};
    tx.buttons_pressed_mask = 0x01u;

    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);

    const size_t len = esp32_button_build_state_packet(&tx, 1u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    frame[2] = 0xFF; // corrupt version
    EXPECT_FALSE(feedFrame(frame, len, &parser));
}

TEST(Esp32ButtonParser, NullParserDoesNotCrash)
{
    EXPECT_EQ(esp32_button_parser_feed(nullptr, ESP32_BUTTON_MAGIC0), 0);
    esp32_button_parser_init(nullptr);
}

TEST(Esp32ButtonParser, DuplicateFrameParsedTwice)
{
    esp32_button_state_t tx {};
    tx.buttons_pressed_mask = 0x07u;

    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);

    const size_t len = esp32_button_build_state_packet(&tx, 7u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);

    EXPECT_TRUE(feedFrame(frame, len, &parser));
    esp32_button_state_t rx {};
    uint8_t seq = 0;
    ASSERT_TRUE(esp32_button_decode_state_packet(parser.bytes, parser.frame_length, &rx, &seq));
    EXPECT_EQ(seq, 7u);

    EXPECT_TRUE(feedFrame(frame, len, &parser));
    ASSERT_TRUE(esp32_button_decode_state_packet(parser.bytes, parser.frame_length, &rx, &seq));
    EXPECT_EQ(seq, 7u);
}

// ─── Decode guards ────────────────────────────────────────────────────────────

TEST(Esp32ButtonDecode, NullStateFrameReturnsZero)
{
    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE] = {};
    EXPECT_FALSE(esp32_button_decode_state_packet(nullptr, sizeof(frame), nullptr, nullptr));
    EXPECT_FALSE(esp32_button_decode_state_packet(frame, sizeof(frame), nullptr, nullptr));
}

TEST(Esp32ButtonDecode, NullAckFrameReturnsZero)
{
    EXPECT_FALSE(esp32_button_decode_ack_packet(nullptr, 8, nullptr));
}

TEST(Esp32ButtonDecode, WrongLengthReturnsZero)
{
    esp32_button_state_t tx {};
    uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);

    const size_t len = esp32_button_build_state_packet(&tx, 1u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    ASSERT_TRUE(feedFrame(frame, len, &parser));

    esp32_button_state_t rx {};
    EXPECT_FALSE(esp32_button_decode_state_packet(parser.bytes, parser.frame_length - 1u, &rx, nullptr));
}
