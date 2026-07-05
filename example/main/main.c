/*
 * wb-idf-lwm2m Example
 *
 * Demonstrates LWM2M registration with a server (e.g. Leshan or ThingsBoard)
 * using the wb-idf-lwm2m component built on wb-idf-coap-client.
 *
 * Uses the standard object factory functions to create:
 *   /0/0  - LWM2M Security
 *   /1/0  - LWM2M Server
 *   /3/0  - Device (Manufacturer, Model, Serial, FW Version, …)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "wb_lwm2m.h"
#include "wb_lwm2m_object.h"
#include "wb_lwm2m_std_objects.h"

static const char *TAG = "LWM2M_EXAMPLE";

static const char *default_wifi_ssid = CONFIG_EXAMPLE_WIFI_SSID;
static const char *default_wifi_password = CONFIG_EXAMPLE_WIFI_PASSWORD;
static const char *default_server_uri = CONFIG_EXAMPLE_LWM2M_SERVER_URI;
static const char *default_endpoint_name = CONFIG_EXAMPLE_LWM2M_ENDPOINT_NAME;
static const uint32_t default_lifetime = CONFIG_EXAMPLE_LWM2M_LIFETIME;
static const char *default_psk_identity = CONFIG_EXAMPLE_LWM2M_PSK_IDENTITY;
static const char *default_psk_key_hex = CONFIG_EXAMPLE_LWM2M_PSK_KEY_HEX;

#define PSK_KEY_MAX_LEN 64

static bool hex_nibble(char c, uint8_t *out)
{
    if (c >= '0' && c <= '9') {
        *out = (uint8_t)(c - '0');
        return true;
    }
    if (c >= 'a' && c <= 'f') {
        *out = (uint8_t)(c - 'a' + 10);
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        *out = (uint8_t)(c - 'A' + 10);
        return true;
    }
    return false;
}

static esp_err_t load_psk_key_from_kconfig(uint8_t *key_out, size_t key_out_size, size_t *key_out_len)
{
    if (key_out == NULL || key_out_len == NULL || default_psk_key_hex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t hex_len = strlen(default_psk_key_hex);
    if (hex_len == 0 || (hex_len % 2) != 0) {
        ESP_LOGE(TAG, "CONFIG_EXAMPLE_LWM2M_PSK_KEY_HEX must contain an even number of hex chars");
        return ESP_ERR_INVALID_ARG;
    }

    size_t key_len = hex_len / 2;
    if (key_len > key_out_size) {
        ESP_LOGE(TAG, "PSK key too long (%u bytes, max %u)", (unsigned)key_len, (unsigned)key_out_size);
        return ESP_ERR_INVALID_SIZE;
    }

    for (size_t i = 0; i < key_len; ++i) {
        uint8_t hi = 0;
        uint8_t lo = 0;
        if (!hex_nibble(default_psk_key_hex[i * 2], &hi) || !hex_nibble(default_psk_key_hex[i * 2 + 1], &lo)) {
            ESP_LOGE(TAG, "Invalid hex character in CONFIG_EXAMPLE_LWM2M_PSK_KEY_HEX at index %u", (unsigned)(i * 2));
            return ESP_ERR_INVALID_ARG;
        }
        key_out[i] = (uint8_t)((hi << 4) | lo);
    }

    *key_out_len = key_len;
    return ESP_OK;
}

static void log_psk_debug(const uint8_t *key, size_t key_len)
{
    ESP_LOGI(TAG, "PSK debug: identity='%s' (len=%u)",
             default_psk_identity ? default_psk_identity : "",
             (unsigned)(default_psk_identity ? strlen(default_psk_identity) : 0));
    ESP_LOGI(TAG, "PSK debug: key hex length from Kconfig=%u", (unsigned)strlen(default_psk_key_hex));
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, key, key_len, ESP_LOG_INFO);
}

/* ── WiFi ── */

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
#define WIFI_MAX_RETRY 5

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying WiFi connection...");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = {0},
            .password = {0},
        },
    };

    size_t ssid_len = strlen(default_wifi_ssid);
    size_t password_len = strlen(default_wifi_password);
    if (ssid_len > sizeof(wifi_config.sta.ssid)) {
        ssid_len = sizeof(wifi_config.sta.ssid);
    }
    if (password_len > sizeof(wifi_config.sta.password)) {
        password_len = sizeof(wifi_config.sta.password);
    }
    memcpy(wifi_config.sta.ssid, default_wifi_ssid, ssid_len);
    memcpy(wifi_config.sta.password, default_wifi_password, password_len);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Waiting for WiFi...");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected");
    } else {
        ESP_LOGE(TAG, "WiFi connection failed");
    }
}

/* ── Execute Callbacks ── */

static esp_err_t reboot_cb(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                           const uint8_t *args, size_t args_len, void *user_ctx)
{
    ESP_LOGW(TAG, "Reboot requested by server!");
    esp_restart();
    return ESP_OK;
}

static esp_err_t factory_reset_cb(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                                  const uint8_t *args, size_t args_len, void *user_ctx)
{
    ESP_LOGW(TAG, "Factory reset requested by server!");
    nvs_flash_erase();
    esp_restart();
    return ESP_OK;
}

/* ── Firmware Update State ── */

static wb_lwm2m_fw_state_t fw_state = WB_LWM2M_FW_STATE_IDLE;
static wb_lwm2m_fw_update_result_t fw_result = WB_LWM2M_FW_RESULT_DEFAULT;
static char fw_package_uri[256] = {0};
static char fw_package_name[64] = "iot-firmware";
static char fw_package_version[32] = "1.0.0";

static esp_err_t fw_read_cb(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                            wb_lwm2m_value_t *value, void *user_ctx)
{
    switch (res_id) {
    case WB_LWM2M_RES_FW_STATE:
        *value = WB_LWM2M_VALUE_INT((int)fw_state);
        return ESP_OK;
    case WB_LWM2M_RES_FW_UPDATE_RESULT:
        *value = WB_LWM2M_VALUE_INT((int)fw_result);
        return ESP_OK;
    case WB_LWM2M_RES_FW_PKG_NAME:
        *value = WB_LWM2M_VALUE_STRING(fw_package_name);
        return ESP_OK;
    case WB_LWM2M_RES_FW_PKG_VERSION:
        *value = WB_LWM2M_VALUE_STRING(fw_package_version);
        return ESP_OK;
    case WB_LWM2M_RES_FW_PACKAGE_URI:
        *value = WB_LWM2M_VALUE_STRING(fw_package_uri);
        return ESP_OK;
    case WB_LWM2M_RES_FW_DELIVERY_METHOD:
        *value = WB_LWM2M_VALUE_INT(1);
        return ESP_OK;
    default:
        return ESP_ERR_NOT_FOUND;
    }
}

static esp_err_t fw_write_cb(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                             const wb_lwm2m_value_t *value, void *user_ctx)
{
    if (res_id == WB_LWM2M_RES_FW_PACKAGE_URI) {
        if (value == NULL || value->type != WB_LWM2M_RES_TYPE_STRING || value->str_val.str == NULL) {
            ESP_LOGE(TAG, "OTA: Invalid firmware URI write payload");
            fw_result = WB_LWM2M_FW_RESULT_INVALID_URI;
            return ESP_ERR_INVALID_ARG;
        }

        size_t uri_len = value->str_val.len;
        if (uri_len >= sizeof(fw_package_uri)) {
            uri_len = sizeof(fw_package_uri) - 1;
        }
        memcpy(fw_package_uri, value->str_val.str, uri_len);
        fw_package_uri[uri_len] = '\0';

        ESP_LOGI(TAG, "OTA: Received firmware URI: %s", fw_package_uri);
        if (fw_package_uri[0]) {
            fw_state = WB_LWM2M_FW_STATE_DOWNLOADING;
            fw_result = WB_LWM2M_FW_RESULT_DEFAULT;
            ESP_LOGI(TAG, "OTA: State -> DOWNLOADING");
            fw_state = WB_LWM2M_FW_STATE_DOWNLOADED;
            ESP_LOGI(TAG, "OTA: State -> DOWNLOADED (simulated)");
        } else {
            fw_result = WB_LWM2M_FW_RESULT_INVALID_URI;
            return ESP_ERR_INVALID_ARG;
        }
        return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t fw_update_cb(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                              const uint8_t *args, size_t args_len, void *user_ctx)
{
    ESP_LOGW(TAG, "OTA: Update Execute triggered by server!");
    if (fw_state != WB_LWM2M_FW_STATE_DOWNLOADED) {
        ESP_LOGE(TAG, "OTA: Cannot update - not in DOWNLOADED state (current=%d)", fw_state);
        fw_result = WB_LWM2M_FW_RESULT_UPDATE_FAILED;
        return ESP_FAIL;
    }
    fw_state = WB_LWM2M_FW_STATE_UPDATING;
    ESP_LOGI(TAG, "OTA: State -> UPDATING");
    fw_result = WB_LWM2M_FW_RESULT_SUCCESS;
    fw_state = WB_LWM2M_FW_STATE_IDLE;
    ESP_LOGI(TAG, "OTA: Update complete (simulated). Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

/* ── LWM2M Event Handler ── */

static void lwm2m_event_handler(wb_lwm2m_event_t *event, void *user_ctx)
{
    switch (event->type) {
    case WB_LWM2M_EVENT_CONNECTED:
        ESP_LOGI(TAG, "EVENT: Connected to server");
        break;
    case WB_LWM2M_EVENT_REGISTERED:
        ESP_LOGI(TAG, "EVENT: Registration successful");
        break;
    case WB_LWM2M_EVENT_REG_FAILED:
        ESP_LOGE(TAG, "EVENT: Registration FAILED");
        break;
    case WB_LWM2M_EVENT_REG_UPDATED:
        ESP_LOGI(TAG, "EVENT: Registration updated");
        break;
    case WB_LWM2M_EVENT_DEREGISTERED:
        ESP_LOGI(TAG, "EVENT: Deregistered");
        break;
    case WB_LWM2M_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "EVENT: Disconnected");
        break;
    case WB_LWM2M_EVENT_ERROR:
        ESP_LOGE(TAG, "EVENT: Error");
        break;
    default:
        break;
    }
}

/* ── Main ── */

void app_main(void)
{
    uint8_t psk_key[PSK_KEY_MAX_LEN] = {0};
    size_t psk_key_len = 0;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_ERROR_CHECK(load_psk_key_from_kconfig(psk_key, sizeof(psk_key), &psk_key_len));

    ESP_LOGI(TAG, "Using WiFi SSID: %s", default_wifi_ssid);
    ESP_LOGI(TAG, "Using LwM2M server URI: %s", default_server_uri);
    ESP_LOGI(TAG, "Using LwM2M endpoint: %s", default_endpoint_name);
    ESP_LOGI(TAG, "Using PSK identity: %s", default_psk_identity);
    ESP_LOGI(TAG, "Using PSK key length: %u bytes", (unsigned)psk_key_len);
    log_psk_debug(psk_key, psk_key_len);

    wifi_init_sta();

    wb_lwm2m_object_def_t *security_obj = wb_lwm2m_create_security_object(
        &(wb_lwm2m_security_info_t){
            .server_uri = default_server_uri,
            .is_bootstrap = false,
            .mode = WB_LWM2M_SEC_MODE_PSK,
            .short_server_id = 1,
            .pub_key_identity = (const uint8_t *)default_psk_identity,
            .pub_key_identity_len = strlen(default_psk_identity),
            .secret_key = psk_key,
            .secret_key_len = psk_key_len,
        }, 0);

    wb_lwm2m_object_def_t *server_obj = wb_lwm2m_create_server_object(
        &(wb_lwm2m_server_info_t){
            .short_server_id = 1,
            .lifetime = default_lifetime,
            .binding = "U",
            .notification_storing = false,
        }, 0, NULL, NULL, NULL);

    wb_lwm2m_object_def_t *device_obj = wb_lwm2m_create_device_object(
        &(wb_lwm2m_device_info_t){
            .manufacturer      = "WhirlingBits",
            .model_number      = "ESP32-WB-LWM2M",
            .serial_number     = "0001",
            .binding           = "U",
            .firmware_version  = "1.0.0",
            .device_type       = "IoT Sensor",
            .hardware_version  = "1.0",
            .software_version  = "1.0.0",
            .utc_offset        = "+01:00",
            .timezone          = "Europe/Berlin",
            .reboot_cb         = reboot_cb,
            .factory_reset_cb  = factory_reset_cb,
        }, NULL);

    wb_lwm2m_object_def_t *firmware_obj = wb_lwm2m_create_firmware_object(
        &(wb_lwm2m_firmware_info_t){
            .read_cb   = fw_read_cb,
            .write_cb  = fw_write_cb,
            .update_cb = fw_update_cb,
        }, NULL);

    wb_lwm2m_client_config_t config = {
        .server_uri = default_server_uri,
        .endpoint_name = default_endpoint_name,
        .lifetime = default_lifetime,
        .binding = "U",
        .lwm2m_version = "1.0",
        .security = WB_COAP_SECURITY_PSK,
        .psk = {
            .identity = default_psk_identity,
            .key = psk_key,
            .key_len = psk_key_len,
        },
        .event_handler = lwm2m_event_handler,
        .user_ctx = NULL,
    };

    wb_lwm2m_client_handle_t lwm2m = wb_lwm2m_init(&config);
    if (lwm2m == NULL) {
        ESP_LOGE(TAG, "Failed to init LWM2M client");
        return;
    }

    if (security_obj) wb_lwm2m_add_object(lwm2m, security_obj);
    if (server_obj)   wb_lwm2m_add_object(lwm2m, server_obj);
    if (device_obj)   wb_lwm2m_add_object(lwm2m, device_obj);
    if (firmware_obj) wb_lwm2m_add_object(lwm2m, firmware_obj);

    ret = wb_lwm2m_start(lwm2m);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start LWM2M client: %s", esp_err_to_name(ret));
        wb_lwm2m_destroy(lwm2m);
        return;
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        if (wb_lwm2m_is_registered(lwm2m)) {
            ESP_LOGI(TAG, "Sending memory free notification...");
            wb_lwm2m_notify(lwm2m, WB_LWM2M_OBJ_DEVICE, 0,
                            WB_LWM2M_RES_DEVICE_MEMORY_FREE);
        }
    }

    wb_lwm2m_destroy(lwm2m);
    wb_lwm2m_destroy_std_object(security_obj);
    wb_lwm2m_destroy_std_object(server_obj);
    wb_lwm2m_destroy_std_object(device_obj);
    wb_lwm2m_destroy_std_object(firmware_obj);
}
