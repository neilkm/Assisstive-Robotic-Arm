#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "../Tests/test.h"

#ifndef UART_RX_BUF_SIZE
#define UART_RX_BUF_SIZE 8u
#endif

#ifndef UART_TX_BUF_SIZE
#define UART_TX_BUF_SIZE 8u
#endif

#define UART_TEST_TIMEOUT 1u
#define UART_TEST_REG_CONTROL_TX_IRQ_ENABLE (1u << 0)
#define UART_TEST_REG_CONTROL_CONFIGURED (1u << 1)

#include "UART_common.h"

static uint8_t uart_test_control_reg;
static uint8_t uart_test_rx_reg;
static uint8_t uart_test_tx_reg;
static std::deque<uint8_t> uart_test_rx_input;
static std::vector<uint8_t> uart_test_tx_output;
static uint16_t uart_test_rx_sem_count;
static uint16_t uart_test_tx_sem_count;
static unsigned int uart_test_enter_critical_count;
static unsigned int uart_test_exit_critical_count;

static void uart_test_configure(void);
static void uart_test_enable_tx_interrupt(void);
static void uart_test_disable_tx_interrupt(void);
static uint8_t uart_test_read_rx_byte(void);
static void uart_test_write_tx_byte(uint8_t byte);
static void uart_test_rx_sem_init(uint16_t initial_count);
static void uart_test_tx_sem_init(uint16_t initial_count);
static uart_status_t uart_test_rx_sem_wait(uint32_t timeout_ticks);
static uart_status_t uart_test_tx_sem_wait(uint32_t timeout_ticks);
static void uart_test_rx_sem_signal_from_isr(void);
static void uart_test_tx_sem_signal_from_isr(void);
static void uart_test_enter_critical(void);
static void uart_test_exit_critical(void);

#define UART_CONFIGURE() uart_test_configure()
#define UART_ENABLE_TX_INTERRUPT() uart_test_enable_tx_interrupt()
#define UART_DISABLE_TX_INTERRUPT() uart_test_disable_tx_interrupt()
#define UART_READ_RX_BYTE() uart_test_read_rx_byte()
#define UART_WRITE_TX_BYTE(byte_) uart_test_write_tx_byte((byte_))
#define UART_RX_SEM_INIT(initial_count_) uart_test_rx_sem_init((initial_count_))
#define UART_TX_SEM_INIT(initial_count_) uart_test_tx_sem_init((initial_count_))
#define UART_RX_SEM_WAIT(timeout_ticks_) uart_test_rx_sem_wait((timeout_ticks_))
#define UART_TX_SEM_WAIT(timeout_ticks_) uart_test_tx_sem_wait((timeout_ticks_))
#define UART_RX_SEM_SIGNAL_FROM_ISR() uart_test_rx_sem_signal_from_isr()
#define UART_TX_SEM_SIGNAL_FROM_ISR() uart_test_tx_sem_signal_from_isr()
#define UART_ENTER_CRITICAL() uart_test_enter_critical()
#define UART_EXIT_CRITICAL() uart_test_exit_critical()

#include "UART_common.c"

static void uart_test_configure(void)
{
    uart_test_control_reg |= UART_TEST_REG_CONTROL_CONFIGURED;
}

static void uart_test_enable_tx_interrupt(void)
{
    uart_test_control_reg |= UART_TEST_REG_CONTROL_TX_IRQ_ENABLE;
}

static void uart_test_disable_tx_interrupt(void)
{
    uart_test_control_reg &= static_cast<uint8_t>(~UART_TEST_REG_CONTROL_TX_IRQ_ENABLE);
}

static void uart_test_queue_rx_input(uint8_t byte)
{
    // Simulate a hardware receive register FIFO that the RX ISR can consume.
    uart_test_rx_input.push_back(byte);
}

static uint8_t uart_test_read_rx_byte(void)
{
    // The driver calls this through UART_READ_RX_BYTE() from uart_rx_isr().
    EXPECT_FALSE(uart_test_rx_input.empty());
    if (uart_test_rx_input.empty()) {
        return 0u;
    }

    const uint8_t byte = uart_test_rx_input.front();
    uart_test_rx_input.pop_front();
    uart_test_rx_reg = byte;
    return byte;
}

static void uart_test_write_tx_byte(uint8_t byte)
{
    // Capture bytes that the TX ISR writes to the simulated UART data register.
    uart_test_tx_reg = byte;
    uart_test_tx_output.push_back(byte);
}

static void uart_test_rx_sem_init(uint16_t initial_count)
{
    uart_test_rx_sem_count = initial_count;
}

static void uart_test_tx_sem_init(uint16_t initial_count)
{
    uart_test_tx_sem_count = initial_count;
}

static uart_status_t uart_test_rx_sem_wait(uint32_t timeout_ticks)
{
    if (uart_test_rx_sem_count > 0u) {
        uart_test_rx_sem_count--;
        return UART_OK;
    }

    return (timeout_ticks == UART_TIMEOUT_FOREVER) ? UART_ERR_EMPTY : UART_ERR_TIMEOUT;
}

static uart_status_t uart_test_tx_sem_wait(uint32_t timeout_ticks)
{
    if (uart_test_tx_sem_count > 0u) {
        uart_test_tx_sem_count--;
        return UART_OK;
    }

    return (timeout_ticks == UART_TIMEOUT_FOREVER) ? UART_ERR_FULL : UART_ERR_TIMEOUT;
}

static void uart_test_rx_sem_signal_from_isr(void)
{
    uart_test_rx_sem_count++;
}

static void uart_test_tx_sem_signal_from_isr(void)
{
    uart_test_tx_sem_count++;
}

static void uart_test_enter_critical(void)
{
    uart_test_enter_critical_count++;
}

static void uart_test_exit_critical(void)
{
    uart_test_exit_critical_count++;
}

class UartDriverTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Start each case from reset hardware state and a freshly initialized driver.
        uart_test_control_reg = 0u;
        uart_test_rx_reg = 0u;
        uart_test_tx_reg = 0u;
        uart_test_rx_input.clear();
        uart_test_tx_output.clear();
        uart_test_rx_sem_count = 0u;
        uart_test_tx_sem_count = 0u;
        uart_test_enter_critical_count = 0u;
        uart_test_exit_critical_count = 0u;
        uart_init();
    }

    void TearDown() override
    {
        // Every critical-section enter must be paired with an exit.
        EXPECT_EQ(uart_test_enter_critical_count, uart_test_exit_critical_count);
    }

    static uint8_t pattern_byte(size_t index)
    {
        // Generate deterministic data that wraps naturally for very large buffers.
        return static_cast<uint8_t>(index & 0xffu);
    }

    static void receive_byte_from_hardware(uint8_t byte)
    {
        // Queue one fake hardware byte, then let the driver's RX ISR store it.
        uart_test_queue_rx_input(byte);
        uart_rx_isr();
    }

    static void fill_rx_buffer(size_t count)
    {
        // Feed count bytes through the same RX ISR path used by real hardware.
        for (size_t i = 0u; i < count; ++i) {
            receive_byte_from_hardware(pattern_byte(i));
        }
    }

    static void fill_tx_buffer(size_t count)
    {
        // Fill the driver's TX ring through the public uart_put_char() API.
        for (size_t i = 0u; i < count; ++i) {
            ASSERT_EQ(UART_OK, uart_put_char(pattern_byte(i), UART_TEST_TIMEOUT));
        }
    }

    static void drain_tx_buffer(size_t count)
    {
        // Run the TX ISR count times so queued bytes reach the fake UART register.
        for (size_t i = 0u; i < count; ++i) {
            ASSERT_FALSE(uart_tx_is_empty());
            uart_tx_isr();
        }
        // One extra empty ISR disables TX interrupts after the last byte is sent.
        uart_tx_isr();
    }
};

TEST_F(UartDriverTest, InitSetsKnownState)
{
    // Verify init configures hardware hooks and resets both RX/TX ring state.
    const uart_ring_buf_t *ring = uart_get_ring_buffer();

    EXPECT_NE(0u, uart_test_control_reg & UART_TEST_REG_CONTROL_CONFIGURED);
    EXPECT_EQ(0u, uart_test_control_reg & UART_TEST_REG_CONTROL_TX_IRQ_ENABLE);
    EXPECT_EQ(0u, ring->rx_count);
    EXPECT_EQ(0u, ring->tx_count);
    EXPECT_EQ(static_cast<uint16_t>(UART_TX_BUF_SIZE), uart_test_tx_sem_count);
    EXPECT_TRUE(uart_is_empty());
    EXPECT_TRUE(uart_tx_is_empty());
}

TEST_F(UartDriverTest, RxNormalCase)
{
    // Nonzero RX capacity should accept one ISR byte and return it through get_char.
    if constexpr (UART_RX_BUF_SIZE == 0u) {
        GTEST_SKIP() << "RX normal case requires nonzero RX capacity";
    }

    uint8_t received = 0u;

    receive_byte_from_hardware('A');

    ASSERT_FALSE(uart_is_empty());
    EXPECT_EQ(UART_OK, uart_get_char(&received, UART_TEST_TIMEOUT));
    EXPECT_EQ('A', received);
    EXPECT_TRUE(uart_is_empty());
}

TEST_F(UartDriverTest, TxNormalCase)
{
    // Nonzero TX capacity should queue one byte and flush it through the TX ISR.
    if constexpr (UART_TX_BUF_SIZE == 0u) {
        GTEST_SKIP() << "TX normal case requires nonzero TX capacity";
    }

    EXPECT_EQ(UART_OK, uart_put_char('Z', UART_TEST_TIMEOUT));
    EXPECT_NE(0u, uart_test_control_reg & UART_TEST_REG_CONTROL_TX_IRQ_ENABLE);

    uart_tx_isr();
    EXPECT_EQ(std::vector<uint8_t>({'Z'}), uart_test_tx_output);
    EXPECT_TRUE(uart_tx_is_empty());

    uart_tx_isr();
    EXPECT_EQ(0u, uart_test_control_reg & UART_TEST_REG_CONTROL_TX_IRQ_ENABLE);
}

TEST_F(UartDriverTest, RxBufferOverflowDropsBytesPastCapacity)
{
    // Filling beyond RX capacity should retain the first capacity bytes only.
    fill_rx_buffer(static_cast<size_t>(UART_RX_BUF_SIZE) + 2u);

    EXPECT_TRUE(uart_rx_is_full());
    EXPECT_EQ(UART_RX_BUF_SIZE, uart_get_ring_buffer()->rx_count);

    for (size_t i = 0u; i < UART_RX_BUF_SIZE; ++i) {
        uint8_t received = 0u;

        ASSERT_EQ(UART_OK, uart_get_char(&received, UART_TEST_TIMEOUT));
        EXPECT_EQ(pattern_byte(i), received);
    }

    uint8_t ignored = 0u;
    EXPECT_TRUE(uart_is_empty());
    EXPECT_EQ(UART_ERR_TIMEOUT, uart_get_char(&ignored, UART_TEST_TIMEOUT));
}

TEST_F(UartDriverTest, TxBufferOverflowRejectsBytesPastCapacity)
{
    // Filling beyond TX capacity should reject the extra byte without changing count.
    fill_tx_buffer(UART_TX_BUF_SIZE);

    EXPECT_TRUE(uart_is_full());
    EXPECT_EQ(UART_TX_BUF_SIZE, uart_get_ring_buffer()->tx_count);
    EXPECT_EQ(UART_ERR_TIMEOUT, uart_put_char('x', UART_TEST_TIMEOUT));
}

TEST_F(UartDriverTest, RxBufferZeroCapacity)
{
    // With logical RX capacity zero, an RX ISR byte must be dropped immediately.
    if constexpr (UART_RX_BUF_SIZE != 0u) {
        GTEST_SKIP() << "RX zero-capacity case requires UART_RX_BUF_SIZE == 0";
    }

    uint8_t received = 0u;

    receive_byte_from_hardware('A');

    EXPECT_TRUE(uart_rx_is_full());
    EXPECT_TRUE(uart_is_empty());
    EXPECT_EQ(0u, uart_get_ring_buffer()->rx_count);
    EXPECT_EQ(UART_ERR_TIMEOUT, uart_get_char(&received, UART_TEST_TIMEOUT));
}

TEST_F(UartDriverTest, TxBufferZeroCapacity)
{
    // With logical TX capacity zero, writes must fail and no byte should transmit.
    if constexpr (UART_TX_BUF_SIZE != 0u) {
        GTEST_SKIP() << "TX zero-capacity case requires UART_TX_BUF_SIZE == 0";
    }

    EXPECT_TRUE(uart_is_full());
    EXPECT_TRUE(uart_tx_is_empty());
    EXPECT_EQ(UART_ERR_TIMEOUT, uart_put_char('x', UART_TEST_TIMEOUT));
    uart_tx_isr();
    EXPECT_TRUE(uart_test_tx_output.empty());
}

TEST_F(UartDriverTest, RxBufferMaxIntCapacity)
{
    // UINT16_MAX capacity exercises the largest supported RX count without overflow.
    if constexpr (UART_RX_BUF_SIZE != UINT16_MAX) {
        GTEST_SKIP() << "RX max-int case requires UART_RX_BUF_SIZE == UINT16_MAX";
    }

    fill_rx_buffer(UART_RX_BUF_SIZE);

    EXPECT_TRUE(uart_rx_is_full());
    EXPECT_EQ(UINT16_MAX, uart_get_ring_buffer()->rx_count);

    receive_byte_from_hardware(0xaa);
    EXPECT_EQ(UINT16_MAX, uart_get_ring_buffer()->rx_count);

    for (size_t i = 0u; i < UART_RX_BUF_SIZE; ++i) {
        uint8_t received = 0u;

        ASSERT_EQ(UART_OK, uart_get_char(&received, UART_TEST_TIMEOUT));
        EXPECT_EQ(pattern_byte(i), received);
    }

    EXPECT_TRUE(uart_is_empty());
}

TEST_F(UartDriverTest, TxBufferMaxIntCapacity)
{
    // UINT16_MAX capacity exercises the largest supported TX count without overflow.
    if constexpr (UART_TX_BUF_SIZE != UINT16_MAX) {
        GTEST_SKIP() << "TX max-int case requires UART_TX_BUF_SIZE == UINT16_MAX";
    }

    fill_tx_buffer(UART_TX_BUF_SIZE);

    EXPECT_TRUE(uart_is_full());
    EXPECT_EQ(UINT16_MAX, uart_get_ring_buffer()->tx_count);
    EXPECT_EQ(UART_ERR_TIMEOUT, uart_put_char('x', UART_TEST_TIMEOUT));

    drain_tx_buffer(UART_TX_BUF_SIZE);
    EXPECT_TRUE(uart_tx_is_empty());
    EXPECT_EQ(static_cast<size_t>(UART_TX_BUF_SIZE), uart_test_tx_output.size());
}

int main(int argc, char **argv)
{
    return common_tests::run_pretty_gtest(argc, argv);
}
