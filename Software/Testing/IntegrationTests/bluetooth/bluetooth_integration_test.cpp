// bluetooth_integration_test
//
// Exercises the ESP32 <-> Jetson Bluetooth SPP packet protocol.
//
// Virtual mode (default / CI):
//   Two threads share a Unix socketpair.
//   "ESP32 simulator" sends button-state packets one at a time, waiting for
//   an ACK after each.  The "Jetson side" runs a continuous reader loop that
//   ACKs every unique state packet it receives.
//
// Hardware mode (--mode=hardware):
//   Opens a real RFCOMM device after the ESP32 has been paired.
//
// Tests:
//   normal     – N exchanges, latency + packet-loss stats
//   drops      – ESP32 sim skips every Nth packet (simulates RF loss)
//   duplicates – ESP32 sim sends each packet twice; Jetson detects duplicates
//   powercycle – mid-stream the socketpair is recreated (MCU power cycle)
//   all        – runs all four (default)
//
// Usage:
//   bluetooth_integration_test [options]
//   --mode=virtual|hardware
//   --device=<path>              (default: /dev/rfcomm0)
//   --test=normal|drops|duplicates|powercycle|all
//   --iterations=N               (default: 50)
//   --drop-every=N               (default: 5)
//   --powercycle-after=N         (default: 25)
//   --timeout-ms=N               (default: 300)

#include "../common/virtual_serial.h"
#include "../common/test_stats.h"
#include "esp32_button_packet.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifdef INTEGRATION_TEST_USE_HARDWARE
#include "bluetooth_uart_driver.h"
#endif

// ─── Config ───────────────────────────────────────────────────────────────────

struct Config {
    bool use_hardware    = false;
    std::string device   = "/dev/rfcomm0";
    int iterations       = 50;
    int timeout_ms       = 300;
    int drop_every       = 5;
    int powercycle_after = 25;
    bool run_normal      = true;
    bool run_drops       = true;
    bool run_duplicates  = true;
    bool run_powercycle  = true;
};

static Config parseArgs(int argc, char** argv)
{
    Config c;
    for (int i = 1; i < argc; ++i) {
        const std::string a(argv[i]);
        if      (a == "--mode=hardware")            c.use_hardware     = true;
        else if (a == "--mode=virtual")              c.use_hardware     = false;
        else if (a.substr(0, 9)  == "--device=")    c.device           = a.substr(9);
        else if (a.substr(0, 13) == "--iterations=") c.iterations       = std::stoi(a.substr(13));
        else if (a.substr(0, 13) == "--timeout-ms=") c.timeout_ms       = std::stoi(a.substr(13));
        else if (a.substr(0, 13) == "--drop-every=") c.drop_every       = std::stoi(a.substr(13));
        else if (a.substr(0, 18) == "--powercycle-after=") c.powercycle_after = std::stoi(a.substr(18));
        else if (a == "--test=normal")     { c.run_drops = c.run_duplicates = c.run_powercycle = false; }
        else if (a == "--test=drops")      { c.run_normal = c.run_duplicates = c.run_powercycle = false; }
        else if (a == "--test=duplicates") { c.run_normal = c.run_drops = c.run_powercycle = false; }
        else if (a == "--test=powercycle") { c.run_normal = c.run_drops = c.run_duplicates = false; }
    }
    return c;
}

// ─── Button mask formatter ────────────────────────────────────────────────────

static void printMask(uint8_t mask)
{
    printf("0x%02X [", mask);
    bool any = false;
    for (int b = 0; b < 6; ++b) {
        if (mask & (1 << b)) {
            if (any) printf(" ");
            printf("B%d", b + 1);
            any = true;
        }
    }
    if (!any) printf("none");
    printf("]");
}

// ─── Virtual ESP32 simulator ──────────────────────────────────────────────────

struct SimConfig {
    int  drop_every     = 0;
    bool send_duplicate = false;
    int  stop_after     = 0;
};

struct SimStats {
    int sent    = 0;
    int acked   = 0;
    int dropped = 0;
    std::vector<int64_t> ack_latencies_us;
};

static SimStats esp32SimThread(int device_fd, int total, const SimConfig& sc,
                               int timeout_ms, std::atomic<bool>& ready)
{
    SimStats ss;
    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);
    ready = true;

    for (int i = 0; i < total; ++i) {
        if (sc.stop_after > 0 && i >= sc.stop_after) break;

        if (sc.drop_every > 0 && (i % sc.drop_every == 0)) {
            printf("  [ESP32] seq=%3d  DROP  (simulated RF loss)\n", i);
            fflush(stdout);
            ++ss.dropped;
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms + 50));
            continue;
        }

        esp32_button_state_t state {};
        state.buttons_pressed_mask = static_cast<uint8_t>(i & 0x3Fu);

        uint8_t frame[ESP32_BUTTON_MAX_FRAME_SIZE];
        const size_t len = esp32_button_build_state_packet(
            &state, static_cast<uint8_t>(i & 0xFFu), frame, sizeof(frame));
        if (len == 0) continue;

        printf("  [ESP32] seq=%3d  SEND  mask=", i);
        printMask(state.buttons_pressed_mask);
        if (sc.send_duplicate) printf("  (sending duplicate)");
        printf("\n");
        fflush(stdout);

        const int64_t t0 = usNow();
        fd_write_all(device_fd, frame, len);
        if (sc.send_duplicate) fd_write_all(device_fd, frame, len);
        ++ss.sent;

        // Wait for ACK
        const int64_t deadline = t0 + 1000LL * 1000;
        uint8_t rx;
        bool got_ack = false;
        while (usNow() < deadline) {
            const ssize_t n = fd_read_timeout(device_fd, &rx, 1, 20);
            if (n <= 0) continue;
            if (!esp32_button_parser_feed(&parser, rx)) continue;
            uint8_t aseq = 0;
            if (esp32_button_decode_ack_packet(parser.bytes, parser.frame_length, &aseq)) {
                const int64_t lat = usNow() - t0;
                ss.ack_latencies_us.push_back(lat);
                ++ss.acked;
                printf("  [ESP32] seq=%3d  ACK   received  %lld us\n",
                       (int)aseq, (long long)lat);
                fflush(stdout);
                got_ack = true;
                break;
            }
        }
        if (!got_ack) {
            printf("  [ESP32] seq=%3d  ACK   TIMEOUT (no ACK within 1000ms)\n", i);
            fflush(stdout);
        }
    }
    return ss;
}

// ─── Jetson continuous reader ─────────────────────────────────────────────────

static void runJetsonLoop(int jetson_fd, TestStats& stats,
                          int unique_target, int timeout_ms)
{
    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);
    int last_seq = -1;

    while (stats.received + stats.timeouts < unique_target) {
        const int64_t t0 = usNow();
        uint8_t rx;
        const ssize_t n = fd_read_timeout(jetson_fd, &rx, 1, timeout_ms);
        if (n <= 0) {
            printf("  [Jetson] TIMEOUT (no packet within %dms)\n", timeout_ms);
            fflush(stdout);
            ++stats.timeouts;
            continue;
        }

        if (!esp32_button_parser_feed(&parser, rx)) continue;

        esp32_button_state_t state {};
        uint8_t seq = 0;
        if (!esp32_button_decode_state_packet(parser.bytes, parser.frame_length, &state, &seq)) {
            printf("  [Jetson] CORRUPT frame\n");
            fflush(stdout);
            ++stats.corrupt;
            continue;
        }

        if (static_cast<int>(seq) == last_seq) {
            printf("  [Jetson] seq=%3u  DUP   (duplicate ignored)\n", (unsigned)seq);
            fflush(stdout);
            ++stats.duplicates;
            continue;
        }

        last_seq = static_cast<int>(seq);
        ++stats.received;

        uint8_t ack_frame[ESP32_BUTTON_MAX_FRAME_SIZE];
        const size_t ack_len = esp32_button_build_ack_packet(seq, ack_frame, sizeof(ack_frame));
        fd_write_all(jetson_fd, ack_frame, ack_len);
        stats.recordLatency(usNow() - t0);

        printf("  [Jetson] seq=%3u  RECV  mask=", (unsigned)seq);
        printMask(state.buttons_pressed_mask);
        printf("  ACK sent\n");
        fflush(stdout);
    }
}

// ─── Test cases ───────────────────────────────────────────────────────────────

static bool runNormalTest(const Config& cfg)
{
    printSection("NORMAL: button-state / ACK exchange with latency + packet-loss stats");

    VirtualSerialPair pair;
    TestStats stats;
    SimConfig sc;
    std::atomic<bool> ready{false};

    std::thread esp32_thread([&]() {
        esp32SimThread(pair.deviceFd(), cfg.iterations, sc, cfg.timeout_ms, ready);
    });

    while (!ready) std::this_thread::sleep_for(std::chrono::milliseconds(1));

    runJetsonLoop(pair.jetsonFd(), stats, cfg.iterations, cfg.timeout_ms);
    esp32_thread.join();

    stats.print("Normal");
    const bool passed = (stats.received == cfg.iterations &&
                         stats.timeouts == 0 &&
                         stats.corrupt  == 0);
    printResult("Normal exchange", passed);
    return passed;
}

static bool runDropsTest(const Config& cfg)
{
    printSection("DROPS: ESP32 sim skips every Nth packet (simulates RF loss)");
    printf("  drop-every=%d  (expect ~%d dropped)\n",
           cfg.drop_every, cfg.iterations / cfg.drop_every);

    VirtualSerialPair pair;
    TestStats stats;
    SimConfig sc;
    sc.drop_every = cfg.drop_every;
    std::atomic<bool> ready{false};

    std::thread esp32_thread([&]() {
        esp32SimThread(pair.deviceFd(), cfg.iterations, sc, cfg.timeout_ms, ready);
    });

    while (!ready) std::this_thread::sleep_for(std::chrono::milliseconds(1));

    runJetsonLoop(pair.jetsonFd(), stats, cfg.iterations, cfg.timeout_ms);
    esp32_thread.join();

    stats.print("Drops");
    const int expected_drops = cfg.iterations / cfg.drop_every;
    const bool passed = (stats.timeouts >= expected_drops - 1 &&
                         stats.timeouts <= expected_drops + 1 &&
                         stats.corrupt  == 0);
    printResult("Drop simulation", passed);
    printf("  expected ~%d drops, got %d\n", expected_drops, stats.timeouts);
    return passed;
}

static bool runDuplicatesTest(const Config& cfg)
{
    printSection("DUPLICATES: ESP32 sim sends each state packet twice");

    VirtualSerialPair pair;
    TestStats stats;
    SimConfig sc;
    sc.send_duplicate = true;
    std::atomic<bool> ready{false};

    std::thread esp32_thread([&]() {
        esp32SimThread(pair.deviceFd(), cfg.iterations, sc, cfg.timeout_ms, ready);
    });

    while (!ready) std::this_thread::sleep_for(std::chrono::milliseconds(1));

    runJetsonLoop(pair.jetsonFd(), stats, cfg.iterations, cfg.timeout_ms);
    esp32_thread.join();

    stats.print("Duplicates");
    const bool passed = (stats.received   == cfg.iterations &&
                         stats.corrupt    == 0 &&
                         stats.duplicates >= cfg.iterations - 1);
    printResult("Duplicate injection", passed);
    printf("  %d unique received, %d duplicates detected\n",
           stats.received, stats.duplicates);
    return passed;
}

static bool runPowerCycleTest(const Config& cfg)
{
    printSection("POWER CYCLE: ESP32 disconnects mid-stream, Jetson reconnects");
    printf("  power cycle after %d of %d packets\n",
           cfg.powercycle_after, cfg.iterations);

    VirtualSerialPair pair;
    TestStats pre_stats;
    TestStats post_stats;

    // ── Phase 1 ───────────────────────────────────────────────────────────────
    SimConfig sc1;
    sc1.stop_after = cfg.powercycle_after;
    std::atomic<bool> ready1{false};
    std::thread esp32_phase1([&]() {
        esp32SimThread(pair.deviceFd(), cfg.powercycle_after, sc1, cfg.timeout_ms, ready1);
    });

    while (!ready1) std::this_thread::sleep_for(std::chrono::milliseconds(1));

    runJetsonLoop(pair.jetsonFd(), pre_stats, cfg.powercycle_after, cfg.timeout_ms);
    esp32_phase1.join();

    // ── Power cycle ───────────────────────────────────────────────────────────
    printf("\n  [ ** POWER CYCLE ** ] Recreating virtual serial pair...\n");
    const int64_t cycle_t0 = usNow();
    pair.recreate();
    const int64_t cycle_us = usNow() - cycle_t0;
    printf("  Reconnect setup time: %lld us\n\n", (long long)cycle_us);

    // ── Phase 2 ───────────────────────────────────────────────────────────────
    const int remaining = cfg.iterations - cfg.powercycle_after;
    SimConfig sc2;
    std::atomic<bool> ready2{false};
    std::thread esp32_phase2([&]() {
        esp32SimThread(pair.deviceFd(), remaining, sc2, cfg.timeout_ms, ready2);
    });

    while (!ready2) std::this_thread::sleep_for(std::chrono::milliseconds(1));

    runJetsonLoop(pair.jetsonFd(), post_stats, remaining, cfg.timeout_ms);
    esp32_phase2.join();

    printf("\n  Pre-cycle:");  pre_stats.print("Pre-cycle");
    printf("\n  Post-cycle:"); post_stats.print("Post-cycle");

    const bool passed = (pre_stats.received  == cfg.powercycle_after &&
                         post_stats.received == remaining &&
                         pre_stats.corrupt   == 0 &&
                         post_stats.corrupt  == 0);
    printResult("Power cycle / reconnect", passed);
    printf("  pre=%d/%d  post=%d/%d  reconnect=%lld us\n",
           pre_stats.received, cfg.powercycle_after,
           post_stats.received, remaining,
           (long long)cycle_us);
    return passed;
}

// ─── Hardware path ────────────────────────────────────────────────────────────

static bool runHardwareNormalTest(const Config& cfg)
{
#ifdef INTEGRATION_TEST_USE_HARDWARE
    printSection("HARDWARE NORMAL: Jetson reads button states from ESP32 via RFCOMM");
    printf("  Opening %s (Bluetooth RFCOMM)...\n", cfg.device.c_str());

    BluetoothUartDriver driver;
    std::string err;
    if (!driver.openPort(cfg.device, 115200, &err)) {
        printf("  SKIP: cannot open %s: %s\n", cfg.device.c_str(), err.c_str());
        return true;
    }
    printf("  Connected.\n\n");

    TestStats stats;
    esp32_button_parser_t parser;
    esp32_button_parser_init(&parser);
    int last_seq = -1;

    while (stats.received + stats.timeouts < cfg.iterations) {
        const int64_t t0 = usNow();
        uint8_t rx;
        const ssize_t n = driver.readBytes(&rx, 1, cfg.timeout_ms, &err);
        if (n <= 0) {
            printf("  [Jetson] TIMEOUT (no packet within %dms)\n", cfg.timeout_ms);
            fflush(stdout);
            ++stats.timeouts;
            continue;
        }

        if (!esp32_button_parser_feed(&parser, rx)) continue;

        esp32_button_state_t state {};
        uint8_t seq = 0;
        if (!esp32_button_decode_state_packet(parser.bytes, parser.frame_length, &state, &seq)) {
            printf("  [Jetson] CORRUPT frame\n");
            fflush(stdout);
            ++stats.corrupt;
            continue;
        }

        if (static_cast<int>(seq) == last_seq) {
            printf("  [Jetson] seq=%3u  DUP   (duplicate ignored)\n", (unsigned)seq);
            fflush(stdout);
            ++stats.duplicates;
            continue;
        }

        last_seq = static_cast<int>(seq);
        ++stats.received;

        uint8_t ack[ESP32_BUTTON_MAX_FRAME_SIZE];
        const size_t alen = esp32_button_build_ack_packet(seq, ack, sizeof(ack));
        driver.writeAll(ack, alen, &err);
        stats.recordLatency(usNow() - t0);

        printf("  [Jetson] seq=%3u  RECV  mask=", (unsigned)seq);
        printMask(state.buttons_pressed_mask);
        printf("  ACK sent  %lld us\n", (long long)(usNow() - t0));
        fflush(stdout);
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

    printBanner("BLUETOOTH INTEGRATION TEST  (ESP32 <-> Jetson)");
    printf("  mode=%s  device=%s  iterations=%d  timeout=%dms\n",
           cfg.use_hardware ? "hardware" : "virtual",
           cfg.device.c_str(), cfg.iterations, cfg.timeout_ms);

    std::vector<std::pair<std::string, bool>> results;

    if (cfg.use_hardware) {
        results.push_back({"Hardware normal", runHardwareNormalTest(cfg)});
    } else {
        if (cfg.run_normal)     results.push_back({"Normal exchange",         runNormalTest(cfg)});
        if (cfg.run_drops)      results.push_back({"Drop simulation",         runDropsTest(cfg)});
        if (cfg.run_duplicates) results.push_back({"Duplicate injection",     runDuplicatesTest(cfg)});
        if (cfg.run_powercycle) results.push_back({"Power cycle / reconnect", runPowerCycleTest(cfg)});
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
