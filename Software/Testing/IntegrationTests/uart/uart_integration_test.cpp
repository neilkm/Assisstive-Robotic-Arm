// uart_integration_test
//
// Exercises the STM32 <-> Jetson UART packet protocol.
//
// Virtual mode (default / CI):
//   Spawns an in-process "STM32 simulator" thread connected via a Unix
//   socketpair.  The simulator reads desired-state frames and echoes each
//   one back as an actual-state frame (mirroring all 7 joint angles + zero
//   force), matching the real STM32 firmware's STM_UART_PROTOCOL_TEST mode.
//
// Hardware mode (--mode=hardware):
//   Opens a real UART device (default /dev/ttyACM0) to talk to a Nucleo
//   flashed with the protocol echo firmware.
//
// Tests:
//   normal     – N exchanges, latency + packet-loss stats
//   drops      – STM32 sim drops every Nth response; verify Jetson detects
//   duplicates – STM32 sim sends each response twice; parser must handle it
//   all        – runs all three sequentially (default)
//
// Usage:
//   uart_integration_test [options]
//   --mode=virtual|hardware     (default: virtual)
//   --device=<path>             (default: /dev/ttyACM0)
//   --baud=<rate>               (default: 115200)
//   --test=normal|drops|duplicates|all  (default: all)
//   --iterations=N              (default: 100)
//   --drop-every=N              (default: 5)
//   --timeout-ms=N              (default: 150)

#include "../common/virtual_serial.h"
#include "../common/test_stats.h"
#include "packet.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#ifdef INTEGRATION_TEST_USE_HARDWARE
#include "uart_driver.h"
#endif

// ─── Config ───────────────────────────────────────────────────────────────────

struct Config {
    bool use_hardware     = false;
    std::string device    = "/dev/ttyACM0";
    int baud              = 115200;
    int iterations        = 100;
    int timeout_ms        = 150;
    int drop_every        = 5;
    bool run_normal       = true;
    bool run_drops        = true;
    bool run_duplicates   = true;
};

static Config parseArgs(int argc, char** argv)
{
    Config c;
    for (int i = 1; i < argc; ++i) {
        const std::string a(argv[i]);
        if      (a == "--mode=hardware")           c.use_hardware = true;
        else if (a == "--mode=virtual")             c.use_hardware = false;
        else if (a.substr(0, 9)  == "--device=")   c.device       = a.substr(9);
        else if (a.substr(0, 7)  == "--baud=")      c.baud         = std::stoi(a.substr(7));
        else if (a.substr(0, 13) == "--iterations=") c.iterations   = std::stoi(a.substr(13));
        else if (a.substr(0, 13) == "--timeout-ms=") c.timeout_ms   = std::stoi(a.substr(13));
        else if (a.substr(0, 13) == "--drop-every=") c.drop_every   = std::stoi(a.substr(13));
        else if (a == "--test=normal")     { c.run_drops = false; c.run_duplicates = false; }
        else if (a == "--test=drops")      { c.run_normal = false; c.run_duplicates = false; }
        else if (a == "--test=duplicates") { c.run_normal = false; c.run_drops = false; }
    }
    return c;
}

// ─── Formatting helpers ───────────────────────────────────────────────────────

static void printAngles(const char* tag, const float* angles, int count)
{
    printf("  %s [", tag);
    for (int i = 0; i < count; ++i) {
        printf("%s%6.1f", i ? " " : "", angles[i]);
    }
    printf("]\n");
}

static void printExchangeOk(uint8_t seq, int64_t latency_us)
{
    printf("  seq=%3u  RECV  %6lld us  OK\n", (unsigned)seq, (long long)latency_us);
}

static void printExchangeTimeout(uint8_t seq)
{
    printf("  seq=%3u  TIMEOUT\n", (unsigned)seq);
}

static void printExchangeCorrupt(uint8_t seq)
{
    printf("  seq=%3u  CORRUPT frame\n", (unsigned)seq);
}

static void printExchangeDuplicate(uint8_t seq)
{
    printf("  seq=%3u  DUP (extra frame ignored)\n", (unsigned)seq);
}

// ─── Virtual STM32 simulator ──────────────────────────────────────────────────

struct SimConfig {
    int  drop_every     = 0;
    bool send_duplicate = false;
};

static void stm32SimThread(int device_fd, int expected_packets, const SimConfig& sc,
                           std::atomic<int>& sim_sent, std::atomic<bool>& sim_done)
{
    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    uint8_t buf[1];
    int received = 0;

    while (received < expected_packets) {
        const ssize_t n = fd_read_timeout(device_fd, buf, 1, 500);
        if (n <= 0) continue;
        if (!arm_uart_parser_feed(&parser, buf[0])) continue;

        arm_desired_state_t desired {};
        uint8_t seq = 0;
        if (!arm_uart_decode_desired_state_packet(parser.bytes, parser.frame_length, &desired, &seq)) continue;

        ++received;

        if (sc.drop_every > 0 && (received % sc.drop_every == 0)) continue;

        arm_actual_state_t actual {};
        for (size_t j = 0; j < ARM_UART_JOINT_COUNT; ++j)
            actual.actual_joint_angles[j] = desired.desired_joint_angles[j];
        actual.force_sensor = 0.0f;

        uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
        const size_t len = arm_uart_build_actual_state_packet(&actual, seq, frame, sizeof(frame));
        if (len == 0) continue;

        fd_write_all(device_fd, frame, len);
        if (sc.send_duplicate) fd_write_all(device_fd, frame, len);
        ++sim_sent;
    }
    sim_done = true;
}

// ─── Single-exchange helper (virtual) ────────────────────────────────────────

static bool runVirtualExchange(int jetson_fd, uint8_t seq,
                               const float* desired_angles, TestStats& stats,
                               int timeout_ms)
{
    printAngles("SEND", desired_angles, ARM_UART_JOINT_COUNT);

    arm_desired_state_t desired {};
    for (size_t i = 0; i < ARM_UART_JOINT_COUNT; ++i)
        desired.desired_joint_angles[i] = desired_angles[i];

    uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
    const size_t len = arm_uart_build_desired_state_packet(&desired, seq, frame, sizeof(frame));
    if (len == 0) return false;

    ++stats.sent;
    const int64_t t0 = usNow();
    if (!fd_write_all(jetson_fd, frame, len)) return false;

    arm_uart_parser_t parser;
    arm_uart_parser_init(&parser);

    const int64_t deadline_us = t0 + static_cast<int64_t>(timeout_ms) * 1000;
    bool got_response = false;
    uint8_t rx_buf[1];

    while (usNow() < deadline_us) {
        const int remain_ms = static_cast<int>((deadline_us - usNow()) / 1000);
        const ssize_t n = fd_read_timeout(jetson_fd, rx_buf, 1, remain_ms > 0 ? remain_ms : 1);
        if (n <= 0) break;

        if (!arm_uart_parser_feed(&parser, rx_buf[0])) continue;

        arm_actual_state_t actual {};
        uint8_t rseq = 0;
        if (!arm_uart_decode_actual_state_packet(parser.bytes, parser.frame_length, &actual, &rseq)) {
            ++stats.corrupt;
            printExchangeCorrupt(seq);
            continue;
        }

        if (!got_response) {
            const int64_t latency = usNow() - t0;
            stats.recordLatency(latency);
            ++stats.received;
            got_response = true;
            printAngles("RECV", actual.actual_joint_angles, ARM_UART_JOINT_COUNT);
            printExchangeOk(rseq, latency);
        } else {
            ++stats.duplicates;
            printExchangeDuplicate(rseq);
        }
    }

    if (!got_response) {
        ++stats.timeouts;
        printExchangeTimeout(seq);
    }
    return got_response;
}

// ─── Test cases ───────────────────────────────────────────────────────────────

static bool runNormalTest(const Config& cfg)
{
    printSection("NORMAL: full-duplex packet exchange, latency + packet-loss stats");

    VirtualSerialPair pair;
    TestStats stats;
    SimConfig sc;
    std::atomic<int>  sim_sent{0};
    std::atomic<bool> sim_done{false};

    std::thread sim(stm32SimThread, pair.deviceFd(), cfg.iterations, sc,
                    std::ref(sim_sent), std::ref(sim_done));

    float angles[ARM_UART_JOINT_COUNT];
    for (int i = 0; i < cfg.iterations; ++i) {
        for (size_t j = 0; j < ARM_UART_JOINT_COUNT; ++j)
            angles[j] = static_cast<float>(i) + static_cast<float>(j) * 10.0f;
        printf("\n  --- exchange %d/%d ---\n", i + 1, cfg.iterations);
        runVirtualExchange(pair.jetsonFd(), static_cast<uint8_t>(i & 0xFF),
                           angles, stats, cfg.timeout_ms);
    }

    sim.join();
    stats.print("Normal");
    const bool passed = (stats.received == cfg.iterations && stats.corrupt == 0);
    printResult("Normal exchange", passed);
    return passed;
}

static bool runDropsTest(const Config& cfg)
{
    printSection("DROPS: STM32 sim drops every Nth response");
    printf("  drop-every=%d  (expect ~%.0f%% loss)\n",
           cfg.drop_every, 100.0f / static_cast<float>(cfg.drop_every));

    VirtualSerialPair pair;
    TestStats stats;
    SimConfig sc;
    sc.drop_every = cfg.drop_every;
    std::atomic<int>  sim_sent{0};
    std::atomic<bool> sim_done{false};

    std::thread sim(stm32SimThread, pair.deviceFd(), cfg.iterations, sc,
                    std::ref(sim_sent), std::ref(sim_done));

    float angles[ARM_UART_JOINT_COUNT] = {};
    for (int i = 0; i < cfg.iterations; ++i) {
        angles[0] = static_cast<float>(i);
        printf("\n  --- exchange %d/%d ---\n", i + 1, cfg.iterations);
        runVirtualExchange(pair.jetsonFd(), static_cast<uint8_t>(i & 0xFF),
                           angles, stats, cfg.timeout_ms);
    }

    sim.join();
    stats.print("Drops");

    const int expected_drops = cfg.iterations / cfg.drop_every;
    const int actual_drops   = stats.timeouts;
    const bool passed = (actual_drops >= expected_drops - 2 &&
                         actual_drops <= expected_drops + 2 &&
                         stats.corrupt == 0);
    printResult("Drop simulation", passed);
    printf("  expected ~%d drops, got %d\n", expected_drops, actual_drops);
    return passed;
}

static bool runDuplicatesTest(const Config& cfg)
{
    printSection("DUPLICATES: STM32 sim sends each response twice");

    VirtualSerialPair pair;
    TestStats stats;
    SimConfig sc;
    sc.send_duplicate = true;
    std::atomic<int>  sim_sent{0};
    std::atomic<bool> sim_done{false};

    std::thread sim(stm32SimThread, pair.deviceFd(), cfg.iterations, sc,
                    std::ref(sim_sent), std::ref(sim_done));

    float angles[ARM_UART_JOINT_COUNT] = {};
    for (int i = 0; i < cfg.iterations; ++i) {
        angles[0] = static_cast<float>(i * 2);
        printf("\n  --- exchange %d/%d ---\n", i + 1, cfg.iterations);
        runVirtualExchange(pair.jetsonFd(), static_cast<uint8_t>(i & 0xFF),
                           angles, stats, cfg.timeout_ms);
    }

    sim.join();
    stats.print("Duplicates");

    const bool passed = (stats.received == cfg.iterations &&
                         stats.corrupt  == 0 &&
                         stats.duplicates >= cfg.iterations - 5);
    printResult("Duplicate injection", passed);
    printf("  %d packets received, %d duplicates detected\n",
           stats.received, stats.duplicates);
    return passed;
}

// ─── Hardware path ────────────────────────────────────────────────────────────

static bool runHardwareNormalTest(const Config& cfg)
{
#ifdef INTEGRATION_TEST_USE_HARDWARE
    printSection("HARDWARE NORMAL: direct UART exchange with STM32");
    printf("  Opening %s at %d baud...\n", cfg.device.c_str(), cfg.baud);

    UartDriver driver;
    std::string err;
    if (!driver.openPort(cfg.device, cfg.baud, &err)) {
        printf("  SKIP: cannot open %s: %s\n", cfg.device.c_str(), err.c_str());
        return true;
    }
    printf("  Connected.\n\n");

    TestStats stats;
    for (int i = 0; i < cfg.iterations; ++i) {
        arm_desired_state_t desired {};
        for (size_t j = 0; j < ARM_UART_JOINT_COUNT; ++j)
            desired.desired_joint_angles[j] = static_cast<float>(i * ARM_UART_JOINT_COUNT + j);

        printAngles("  SEND", desired.desired_joint_angles, ARM_UART_JOINT_COUNT);

        uint8_t frame[ARM_UART_MAX_FRAME_SIZE];
        const size_t len = arm_uart_build_desired_state_packet(
            &desired, static_cast<uint8_t>(i & 0xFF), frame, sizeof(frame));

        ++stats.sent;
        const int64_t t0 = usNow();
        if (!driver.writeAll(frame, len, &err)) {
            printf("  seq=%3d  WRITE ERROR: %s\n", i, err.c_str());
            ++stats.timeouts;
            continue;
        }

        arm_uart_parser_t parser;
        arm_uart_parser_init(&parser);
        const int64_t deadline = t0 + static_cast<int64_t>(cfg.timeout_ms) * 1000;
        bool got = false;
        uint8_t rx;

        while (usNow() < deadline) {
            const ssize_t n = driver.readBytes(&rx, 1, 10, &err);
            if (n <= 0) continue;
            if (!arm_uart_parser_feed(&parser, rx)) continue;

            arm_actual_state_t actual {};
            uint8_t rseq = 0;
            if (!arm_uart_decode_actual_state_packet(parser.bytes, parser.frame_length, &actual, &rseq))
                continue;

            const int64_t latency = usNow() - t0;
            stats.recordLatency(latency);
            ++stats.received;
            got = true;
            printAngles("  RECV", actual.actual_joint_angles, ARM_UART_JOINT_COUNT);
            printExchangeOk(rseq, latency);
            break;
        }
        if (!got) {
            ++stats.timeouts;
            printExchangeTimeout(static_cast<uint8_t>(i & 0xFF));
        }
        printf("\n");
    }

    stats.print("Hardware normal");
    const bool passed = (stats.packetLossPct() < 5.0f);
    printResult("Hardware exchange", passed);
    return passed;
#else
    (void)cfg;
    printf("  [SKIP] Hardware test requires -DINTEGRATION_TEST_USE_HARDWARE=ON\n");
    return true;
#endif
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv)
{
    const Config cfg = parseArgs(argc, argv);

    printBanner("UART INTEGRATION TEST  (STM32 <-> Jetson)");
    printf("  mode=%s  device=%s  baud=%d  iterations=%d  timeout=%dms\n",
           cfg.use_hardware ? "hardware" : "virtual",
           cfg.device.c_str(), cfg.baud, cfg.iterations, cfg.timeout_ms);

    std::vector<std::pair<std::string, bool>> results;

    if (cfg.use_hardware) {
        results.push_back({"Hardware normal", runHardwareNormalTest(cfg)});
    } else {
        if (cfg.run_normal)     results.push_back({"Normal exchange",     runNormalTest(cfg)});
        if (cfg.run_drops)      results.push_back({"Drop simulation",     runDropsTest(cfg)});
        if (cfg.run_duplicates) results.push_back({"Duplicate injection", runDuplicatesTest(cfg)});
    }

    printBanner("SUMMARY");
    printSummaryHeader();
    bool all_pass = true;
    for (const auto& [name, pass] : results) {
        printSummaryRow(name.c_str(), pass);
        if (!pass) all_pass = false;
    }
    printf("\n");

    return all_pass ? 0 : 1;
}
