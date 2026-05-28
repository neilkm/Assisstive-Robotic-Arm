#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_spp_api.h"
#include "esp32_button_packet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define ESP32_BUTTON_DEVICE_NAME "ArmESP32Buttons"
#define ESP32_BUTTON_SPP_SERVER_NAME "ArmButtonSpp"
#define ESP32_BUTTON_SEND_PERIOD_MS 100u
#define ESP32_BUTTON_ACK_TIMEOUT_MS 250u
#define ESP32_ECHO_LINE_BUFFER_SIZE 128u

static const char *TAG = "esp32_buttons";

static volatile bool spp_connected = false;
static volatile uint32_t spp_handle = 0u;

#ifdef ESP32_BT_ECHO_TEST
static char echo_line_buffer[ESP32_ECHO_LINE_BUFFER_SIZE];
static size_t echo_line_length = 0u;

static void spp_write_string(const char *text)
{
    if (spp_connected && spp_handle != 0u && text != NULL) {
        esp_spp_write((uint32_t)spp_handle, (int)strlen(text), (uint8_t *)text);
    }
}

static void reset_echo_line(void)
{
    echo_line_length = 0u;
    echo_line_buffer[0] = '\0';
}

static void echo_received_line(void)
{
    char response[ESP32_ECHO_LINE_BUFFER_SIZE + 8u];
    snprintf(response, sizeof(response), "Rx [%s]\r\n", echo_line_buffer);
    spp_write_string(response);
    reset_echo_line();
}

static void handle_echo_rx_byte(uint8_t byte)
{
    if (byte == '\r') {
        return;
    }
    if (byte == '\n') {
        echo_received_line();
        return;
    }

    if (echo_line_length < ESP32_ECHO_LINE_BUFFER_SIZE - 1u) {
        echo_line_buffer[echo_line_length++] = (char)byte;
        echo_line_buffer[echo_line_length] = '\0';
    }
}
#else
static volatile bool waiting_for_ack = false;
static volatile uint8_t waiting_sequence = 0u;
static esp32_button_parser_t rx_parser;

static uint8_t generate_test_button_mask(void)
{
    return (uint8_t)(esp_random() & ((1u << ESP32_BUTTON_COUNT) - 1u));
}

static void handle_rx_byte(uint8_t byte)
{
    if (!esp32_button_parser_feed(&rx_parser, byte)) {
        return;
    }

    uint8_t acknowledged_sequence = 0u;
    if (esp32_button_decode_ack_packet(rx_parser.bytes, rx_parser.frame_length, &acknowledged_sequence) &&
        waiting_for_ack &&
        acknowledged_sequence == waiting_sequence) {
        waiting_for_ack = false;
        ESP_LOGD(TAG, "ACK sequence=%" PRIu8, acknowledged_sequence);
    }
}
#endif

static void spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event) {
    case ESP_SPP_INIT_EVT:
        ESP_ERROR_CHECK(esp_bt_dev_set_device_name(ESP32_BUTTON_DEVICE_NAME));
        ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE));
        ESP_ERROR_CHECK(esp_spp_start_srv(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_SLAVE, 0, ESP32_BUTTON_SPP_SERVER_NAME));
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        spp_handle = param->srv_open.handle;
        spp_connected = true;
#ifdef ESP32_BT_ECHO_TEST
        reset_echo_line();
        spp_write_string("\r\n========================================\r\n");
        spp_write_string(" ESP32 Bluetooth UART Echo Test Runner\r\n");
        spp_write_string(" Type a string from the Jetson terminal and press Enter.\r\n");
        spp_write_string(" Echo format: Rx [message]\r\n");
        spp_write_string("========================================\r\n> ");
#else
        waiting_for_ack = false;
        esp32_button_parser_init(&rx_parser);
#endif
        ESP_LOGI(TAG, "Jetson SPP connected");
        break;
    case ESP_SPP_CLOSE_EVT:
        spp_connected = false;
        spp_handle = 0u;
#ifdef ESP32_BT_ECHO_TEST
        reset_echo_line();
#else
        waiting_for_ack = false;
#endif
        ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE));
        ESP_LOGI(TAG, "Jetson SPP disconnected");
        break;
    case ESP_SPP_DATA_IND_EVT:
        for (uint16_t i = 0u; i < param->data_ind.len; ++i) {
#ifdef ESP32_BT_ECHO_TEST
            handle_echo_rx_byte(param->data_ind.data[i]);
#else
            handle_rx_byte(param->data_ind.data[i]);
#endif
        }
        break;
    default:
        break;
    }
}

static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    if (event == ESP_BT_GAP_PIN_REQ_EVT) {
        ESP_LOGI(TAG, "Bluetooth PIN requested; using 1234");
        esp_bt_pin_code_t pin_code = {'1', '2', '3', '4'};
        ESP_ERROR_CHECK(esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code));
    }
}

static void init_bluetooth_spp(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    bluedroid_cfg.ssp_en = false;
    ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bluedroid_cfg));
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    esp_bt_pin_code_t pin_code = {'1', '2', '3', '4'};
    ESP_ERROR_CHECK(esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, 4, pin_code));
    ESP_ERROR_CHECK(esp_bt_gap_register_callback(gap_callback));
    ESP_ERROR_CHECK(esp_spp_register_callback(spp_callback));

    esp_spp_cfg_t spp_cfg = {
        .mode = ESP_SPP_MODE_CB,
        .enable_l2cap_ertm = true,
        .tx_buffer_size = 0,
    };
    ESP_ERROR_CHECK(esp_spp_enhanced_init(&spp_cfg));
}

void app_main(void)
{
#ifdef ESP32_BT_ECHO_TEST
    reset_echo_line();
#else
    esp32_button_parser_init(&rx_parser);
#endif
    init_bluetooth_spp();

#ifdef ESP32_BT_ECHO_TEST
    ESP_LOGI(TAG, "ESP32 Bluetooth UART echo test ready as %s", ESP32_BUTTON_DEVICE_NAME);

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
#else
    uint8_t sequence = 0u;
    uint8_t tx_frame[ESP32_BUTTON_MAX_FRAME_SIZE];
    size_t tx_len = 0u;
    TickType_t last_send_tick = 0u;

    ESP_LOGI(TAG, "ESP32 button Bluetooth SPP service ready as %s", ESP32_BUTTON_DEVICE_NAME);

    while (true) {
        if (!spp_connected) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        const TickType_t now = xTaskGetTickCount();
        const TickType_t elapsed_ms = (now - last_send_tick) * portTICK_PERIOD_MS;
        const bool resend_due = waiting_for_ack && elapsed_ms >= ESP32_BUTTON_ACK_TIMEOUT_MS;
        const bool new_sample_due = !waiting_for_ack && elapsed_ms >= ESP32_BUTTON_SEND_PERIOD_MS;

        if (resend_due || new_sample_due) {
            if (new_sample_due) {
                esp32_button_state_t state = {
                    .buttons_pressed_mask = generate_test_button_mask(),
                };
                sequence++;
                tx_len = esp32_button_build_state_packet(&state, sequence, tx_frame, sizeof(tx_frame));
                waiting_sequence = sequence;
                waiting_for_ack = true;
                ESP_LOGI(TAG, "TX sequence=%" PRIu8 " buttons=0x%02" PRIx8, sequence, state.buttons_pressed_mask);
            } else {
                ESP_LOGW(TAG, "retransmitting unacknowledged sequence=%" PRIu8, waiting_sequence);
            }

            if (tx_len != 0u && spp_handle != 0u) {
                esp_spp_write((uint32_t)spp_handle, (int)tx_len, tx_frame);
                last_send_tick = now;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
#endif
}
