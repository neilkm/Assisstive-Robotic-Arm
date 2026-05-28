#include "packet.h"

#include <gtest/gtest.h>
#include <cmath>

namespace {

bool nearEqual(float a, float b) { return std::fabs(a - b) < 0.0001f; }

bool feedFrame(const uint8_t* frame, size_t len, arm_uart_parser_t* parser)
{
    for (size_t i = 0; i < len; ++i) {
        if (arm_uart_parser_feed(parser, frame[i])) return true;
    }
    return false;
}

arm_actual_state_t makeActualState(float base = 10.0f)
{
    arm_actual_state_t s {};
    for (size_t i = 0; i < ARM_UART_JOINT_COUNT; ++i) {
        s.actual_joint_angles[i] = base + static_cast<float>(i);
    }
    s.force_sensor = 42.25f;
    return s;
}

arm_desired_state_t makeDesiredState(float base = 100.0f)
{
    arm_desired_state_t s {};
    for (size_t i = 0; i < ARM_UART_JOINT_COUNT; ++i) {
        s.desired_joint_angles[i] = base + static_cast<float>(i * 2u);
    }
    return s;
}

} // namespace

// ─── CRC ─────────────────────────────────────────────────────────────────────

TEST(ArmUartCrc, SameInputSameOutput)
{
    const uint8_t d[] = {0x01, 0x07, 0x1C};
    EXPECT_EQ(arm_uart_crc16_ccitt(d, sizeof(d)), arm_uart_crc16_ccitt(d, sizeof(d)));
}

TEST(ArmUartCrc, DifferentInputDifferentOutput)
{
    const uint8_t d1[] = {0x01, 0x02};
    const uint8_t d2[] = {0x01, 0x03};
    EXPECT_NE(arm_uart_crc16_ccitt(d1, 2), arm_uart_crc16_ccitt(d2, 2));
}

TEST(ArmUartCrc, SingleByteDifference)
{
    uint8_t d[8] = {0xA5, 0x5A, 0x01, 0x01, 0x00, 0x20, 0x00, 0x00};
    const uint16_t crc_orig = arm_uart_crc16_ccitt(d, sizeof(d));
    d[3] ^= 0x01;
    EXPECT_NE(arm_uart_crc16_ccitt(d, sizeof(d)), crc_orig);
}

// ─── Build-frame guards ───────────────────────────────────────────────────────

TEST(ArmUartBuildFrame, NullOutputReturnsZero)
{
    uint8_t payload[4] = {};
    EXPECT_EQ(arm_uart_build_frame(0x01, 0, payload, 4, nullptr, 40), 0u);
}

TEST(ArmUartBuildFrame, NullPayloadReturnsZero)
{
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    EXPECT_EQ(arm_uart_build_frame(0x01, 0, nullptr, 4, frame, sizeof(frame)), 0u);
}

TEST(ArmUartBuildFrame, OversizedPayloadReturnsZero)
{
    uint8_t payload[ARM_UART_MAX_PAYLOAD_SIZE + 1] = {};
    uint8_t frame[100];
    EXPECT_EQ(arm_uart_build_frame(0x01, 0, payload,
                                   static_cast<uint8_t>(ARM_UART_MAX_PAYLOAD_SIZE + 1),
                                   frame, sizeof(frame)), 0u);
}

TEST(ArmUartBuildFrame, CapacityTooSmallReturnsZero)
{
    uint8_t payload[4] = {};
    uint8_t frame[4]; // too small for header(6) + payload(4) + crc(2)
    EXPECT_EQ(arm_uart_build_frame(0x01, 0, payload, 4, frame, sizeof(frame)), 0u);
}

TEST(ArmUartBuildFrame, ValidFrameHasMagicBytes)
{
    uint8_t payload[4] = {1, 2, 3, 4};
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    const size_t len = arm_uart_build_frame(0x01, 5u, payload, 4, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    EXPECT_EQ(frame[0], ARM_UART_MAGIC0);
    EXPECT_EQ(frame[1], ARM_UART_MAGIC1);
    EXPECT_EQ(frame[2], ARM_UART_VERSION);
    EXPECT_EQ(frame[3], 0x01u);
    EXPECT_EQ(frame[4], 5u);
    EXPECT_EQ(frame[5], 4u);
}

// ─── Actual-state roundtrip ───────────────────────────────────────────────────

TEST(ArmUartActualState, Roundtrip)
{
    const arm_actual_state_t tx = makeActualState();
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    const size_t len = arm_uart_build_actual_state_packet(&tx, 7u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    ASSERT_TRUE(feedFrame(frame, len, &parser));

    arm_actual_state_t rx {};
    uint8_t seq = 0;
    ASSERT_TRUE(arm_uart_decode_actual_state_packet(parser.bytes, parser.frame_length, &rx, &seq));
    EXPECT_EQ(seq, 7u);
    EXPECT_TRUE(nearEqual(rx.force_sensor, tx.force_sensor));
    for (size_t i = 0; i < ARM_UART_JOINT_COUNT; ++i) {
        EXPECT_TRUE(nearEqual(rx.actual_joint_angles[i], tx.actual_joint_angles[i])) << "joint " << i;
    }
}

TEST(ArmUartActualState, NullStateReturnsZero)
{
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    EXPECT_EQ(arm_uart_build_actual_state_packet(nullptr, 0u, frame, sizeof(frame)), 0u);
}

TEST(ArmUartActualState, ZeroValues)
{
    arm_actual_state_t tx {};
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    const size_t len = arm_uart_build_actual_state_packet(&tx, 0u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    ASSERT_TRUE(feedFrame(frame, len, &parser));

    arm_actual_state_t rx {};
    ASSERT_TRUE(arm_uart_decode_actual_state_packet(parser.bytes, parser.frame_length, &rx, nullptr));
    for (size_t i = 0; i < ARM_UART_JOINT_COUNT; ++i) {
        EXPECT_TRUE(nearEqual(rx.actual_joint_angles[i], 0.0f));
    }
    EXPECT_TRUE(nearEqual(rx.force_sensor, 0.0f));
}

TEST(ArmUartActualState, NegativeAngles)
{
    arm_actual_state_t tx {};
    for (size_t i = 0; i < ARM_UART_JOINT_COUNT; ++i) {
        tx.actual_joint_angles[i] = -90.0f + static_cast<float>(i) * 10.0f;
    }
    tx.force_sensor = -1.5f;

    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    const size_t len = arm_uart_build_actual_state_packet(&tx, 1u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    ASSERT_TRUE(feedFrame(frame, len, &parser));

    arm_actual_state_t rx {};
    ASSERT_TRUE(arm_uart_decode_actual_state_packet(parser.bytes, parser.frame_length, &rx, nullptr));
    for (size_t i = 0; i < ARM_UART_JOINT_COUNT; ++i) {
        EXPECT_TRUE(nearEqual(rx.actual_joint_angles[i], tx.actual_joint_angles[i])) << "joint " << i;
    }
    EXPECT_TRUE(nearEqual(rx.force_sensor, tx.force_sensor));
}

TEST(ArmUartActualState, LargeAngles)
{
    arm_actual_state_t tx {};
    for (size_t i = 0; i < ARM_UART_JOINT_COUNT; ++i) {
        tx.actual_joint_angles[i] = 3600.0f * static_cast<float>(i + 1u);
    }
    tx.force_sensor = 9999.99f;

    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    const size_t len = arm_uart_build_actual_state_packet(&tx, 255u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    ASSERT_TRUE(feedFrame(frame, len, &parser));

    arm_actual_state_t rx {};
    uint8_t seq = 0;
    ASSERT_TRUE(arm_uart_decode_actual_state_packet(parser.bytes, parser.frame_length, &rx, &seq));
    EXPECT_EQ(seq, 255u);
    for (size_t i = 0; i < ARM_UART_JOINT_COUNT; ++i) {
        EXPECT_TRUE(nearEqual(rx.actual_joint_angles[i], tx.actual_joint_angles[i]));
    }
}

// ─── Desired-state roundtrip ──────────────────────────────────────────────────

TEST(ArmUartDesiredState, Roundtrip)
{
    const arm_desired_state_t tx = makeDesiredState();
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    const size_t len = arm_uart_build_desired_state_packet(&tx, 9u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    ASSERT_TRUE(feedFrame(frame, len, &parser));

    arm_desired_state_t rx {};
    uint8_t seq = 0;
    ASSERT_TRUE(arm_uart_decode_desired_state_packet(parser.bytes, parser.frame_length, &rx, &seq));
    EXPECT_EQ(seq, 9u);
    for (size_t i = 0; i < ARM_UART_JOINT_COUNT; ++i) {
        EXPECT_TRUE(nearEqual(rx.desired_joint_angles[i], tx.desired_joint_angles[i])) << "joint " << i;
    }
}

TEST(ArmUartDesiredState, NullStateReturnsZero)
{
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    EXPECT_EQ(arm_uart_build_desired_state_packet(nullptr, 0u, frame, sizeof(frame)), 0u);
}

TEST(ArmUartDesiredState, SequenceWrapsAt255)
{
    const arm_desired_state_t tx = makeDesiredState();
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    ASSERT_GT(arm_uart_build_desired_state_packet(&tx, 255u, frame, sizeof(frame)), 0u);
    ASSERT_TRUE(feedFrame(frame, arm_uart_build_desired_state_packet(&tx, 255u, frame, sizeof(frame)), &parser));

    arm_desired_state_t rx {};
    uint8_t seq = 0;
    ASSERT_TRUE(arm_uart_decode_desired_state_packet(parser.bytes, parser.frame_length, &rx, &seq));
    EXPECT_EQ(seq, 255u);
}

// ─── Parser ───────────────────────────────────────────────────────────────────

TEST(ArmUartParser, CorruptCrcRejected)
{
    const arm_actual_state_t tx = makeActualState();
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    const size_t len = arm_uart_build_actual_state_packet(&tx, 1u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    frame[len - 1u] ^= 0xFF;
    EXPECT_FALSE(feedFrame(frame, len, &parser));
}

TEST(ArmUartParser, GarbageBeforeMagicStillParses)
{
    const arm_actual_state_t tx = makeActualState();
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    const size_t len = arm_uart_build_actual_state_packet(&tx, 3u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);

    const uint8_t garbage[] = {0x00, 0x11, 0x22, 0x33};
    for (uint8_t b : garbage) {
        arm_uart_parser_feed(&parser, b);
    }
    EXPECT_TRUE(feedFrame(frame, len, &parser));
}

TEST(ArmUartParser, SequentialFramesAllParse)
{
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    for (uint8_t seq = 0; seq < 20; ++seq) {
        arm_actual_state_t tx {};
        tx.force_sensor = static_cast<float>(seq) * 1.5f;
        const size_t len = arm_uart_build_actual_state_packet(&tx, seq, frame, sizeof(frame));
        ASSERT_TRUE(feedFrame(frame, len, &parser)) << "seq=" << (int)seq;

        arm_actual_state_t rx {};
        uint8_t rseq = 0;
        ASSERT_TRUE(arm_uart_decode_actual_state_packet(parser.bytes, parser.frame_length, &rx, &rseq));
        EXPECT_EQ(rseq, seq);
        EXPECT_TRUE(nearEqual(rx.force_sensor, tx.force_sensor));
    }
}

TEST(ArmUartParser, WrongTypeMismatchOnDecode)
{
    const arm_desired_state_t tx = makeDesiredState();
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    const size_t len = arm_uart_build_desired_state_packet(&tx, 1u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    ASSERT_TRUE(feedFrame(frame, len, &parser));

    // Decoding a desired-state frame as actual-state should fail
    arm_actual_state_t rx {};
    EXPECT_FALSE(arm_uart_decode_actual_state_packet(parser.bytes, parser.frame_length, &rx, nullptr));
}

TEST(ArmUartParser, BadVersionRejected)
{
    const arm_actual_state_t tx = makeActualState();
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    const size_t len = arm_uart_build_actual_state_packet(&tx, 1u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    frame[2] = 0xFF; // corrupt version byte (CRC will also be wrong, but version check fires first)
    EXPECT_FALSE(feedFrame(frame, len, &parser));
}

TEST(ArmUartParser, StrayMagicByteBeforeFrame)
{
    arm_actual_state_t tx2 {};
    tx2.force_sensor = 7.0f;

    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    const size_t len = arm_uart_build_actual_state_packet(&tx2, 2u, frame, sizeof(frame));

    arm_uart_parser_feed(&parser, ARM_UART_MAGIC0);
    arm_uart_parser_feed(&parser, 0xDE);
    arm_uart_parser_feed(&parser, 0xAD);
    EXPECT_TRUE(feedFrame(frame, len, &parser));
}

TEST(ArmUartParser, NullParserDoesNotCrash)
{
    EXPECT_EQ(arm_uart_parser_feed(nullptr, 0xA5), 0);
    arm_uart_parser_init(nullptr);
}

TEST(ArmUartParser, DuplicateFrameInStream)
{
    const arm_actual_state_t tx = makeActualState();
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    const size_t len = arm_uart_build_actual_state_packet(&tx, 5u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);

    // Feed the frame twice; the first should parse and the second should also parse after reset
    EXPECT_TRUE(feedFrame(frame, len, &parser));
    arm_actual_state_t rx {};
    uint8_t seq = 0;
    EXPECT_TRUE(arm_uart_decode_actual_state_packet(parser.bytes, parser.frame_length, &rx, &seq));
    EXPECT_EQ(seq, 5u);

    EXPECT_TRUE(feedFrame(frame, len, &parser));
    EXPECT_TRUE(arm_uart_decode_actual_state_packet(parser.bytes, parser.frame_length, &rx, &seq));
    EXPECT_EQ(seq, 5u);
}

// ─── Decode guards ────────────────────────────────────────────────────────────

TEST(ArmUartDecode, NullFrameReturnsZero)
{
    arm_actual_state_t rx {};
    EXPECT_FALSE(arm_uart_decode_actual_state_packet(nullptr, 40, &rx, nullptr));
}

TEST(ArmUartDecode, NullStateReturnsZero)
{
    uint8_t frame[40] = {};
    EXPECT_FALSE(arm_uart_decode_actual_state_packet(frame, 40, nullptr, nullptr));
}

TEST(ArmUartDecode, WrongFrameLengthReturnsZero)
{
    const arm_actual_state_t tx = makeActualState();
    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    const size_t len = arm_uart_build_actual_state_packet(&tx, 1u, frame, sizeof(frame));
    ASSERT_GT(len, 0u);
    ASSERT_TRUE(feedFrame(frame, len, &parser));

    arm_actual_state_t rx {};
    // Off-by-one length should fail
    EXPECT_FALSE(arm_uart_decode_actual_state_packet(parser.bytes, parser.frame_length - 1u, &rx, nullptr));
}

// ─── Float serialisation ──────────────────────────────────────────────────────

TEST(ArmUartFloat, WriteReadRoundtrip)
{
    const float values[] = {0.0f, 1.0f, -1.0f, 3.14159f, -3.14159f, 1e30f, -1e-30f, 180.0f, -180.0f};
    uint8_t buf[4];
    for (float v : values) {
        arm_uart_write_float_le(buf, v);
        EXPECT_TRUE(nearEqual(arm_uart_read_float_le(buf), v)) << "value=" << v;
    }
}
