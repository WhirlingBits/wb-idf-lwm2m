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

extern const uint8_t server_cert_pem_start[] asm("_binary_certificate_pem_start");
extern const uint8_t server_cert_pem_end[]   asm("_binary_certificate_pem_end");

static const char *TAG = "LWM2M_EXAMPLE";

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

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_EXAMPLE_WIFI_SSID,
            .password = CONFIG_EXAMPLE_WIFI_PASSWORD,
        },
    };
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

/* ── Configuration — adjust these to your setup ── */

#define LWM2M_SERVER_URI      "coaps://thingsboard.whirlingbits.de:5686"
#define LWM2M_ENDPOINT_NAME   "test1"
#define LWM2M_LIFETIME        300   /* 5 minutes */

/* PSK credentials — RFC 4279 hex key */
static const char *psk_identity = "test1";
static const uint8_t psk_key[] = {
    0xb6, 0x45, 0x39, 0x86, 0xe8, 0x90, 0x04, 0xcd,
    0x6d, 0x61, 0x59, 0xc1, 0xfe, 0x17, 0xbe, 0xbe,
    0x00, 0x08, 0xec, 0x8e, 0x89, 0x14, 0xac, 0xfb,
    0xeb, 0xb1, 0xfe, 0x82, 0x87, 0x9d, 0x87, 0xc5,
};

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
    /* Initialize NVS (required for WiFi/networking) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Connect to WiFi */
    wifi_init_sta();

    /* ── Create standard LWM2M objects using factory functions ── */

    /* Security Object /0/0 — PSK */
    wb_lwm2m_object_def_t *security_obj = wb_lwm2m_create_security_object(
        &(wb_lwm2m_security_info_t){
            .server_uri = LWM2M_SERVER_URI,
            .is_bootstrap = false,
            .mode = WB_LWM2M_SEC_MODE_PSK,
            .short_server_id = 1,
            .pub_key_identity = (const uint8_t *)psk_identity,
            .pub_key_identity_len = strlen(psk_identity),
            .secret_key = psk_key,
            .secret_key_len = sizeof(psk_key),
        }, 0);

    /* Server Object /1/0 */
    wb_lwm2m_object_def_t *server_obj = wb_lwm2m_create_server_object(
        &(wb_lwm2m_server_info_t){
            .short_server_id = 1,
            .lifetime = LWM2M_LIFETIME,
            .binding = "U",
            .notification_storing = false,
        }, 0, NULL, NULL, NULL);

    /* Device Object /3/0 — all standard resources auto-wired */
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

    /* Create LWM2M client */
    wb_lwm2m_client_config_t config = {
        .server_uri = LWM2M_SERVER_URI,
        .endpoint_name = LWM2M_ENDPOINT_NAME,
        .lifetime = LWM2M_LIFETIME,
        .binding = "U",
        .lwm2m_version = "1.0",
        .security = WB_COAP_SECURITY_PSK,
        .psk = {
            .identity = psk_identity,
            .key = psk_key,
            .key_len = sizeof(psk_key),
        },
        .event_handler = lwm2m_event_handler,
        .user_ctx = NULL,
    };

    wb_lwm2m_client_handle_t lwm2m = wb_lwm2m_init(&config);
    if (lwm2m == NULL) {
        ESP_LOGE(TAG, "Failed to init LWM2M client");
        return;
    }

    /* Register standard objects */
    if (security_obj) wb_lwm2m_add_object(lwm2m, security_obj);
    if (server_obj)   wb_lwm2m_add_object(lwm2m, server_obj);
    if (device_obj)   wb_lwm2m_add_object(lwm2m, device_obj);

    /* Start the LWM2M client (connects + registers) */
    ret = wb_lwm2m_start(lwm2m);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start LWM2M client: %s", esp_err_to_name(ret));
        wb_lwm2m_destroy(lwm2m);
        return;
    }

    /* Main loop: periodically send notifications */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        if (wb_lwm2m_is_registered(lwm2m)) {
            ESP_LOGI(TAG, "Sending memory free notification...");
            wb_lwm2m_notify(lwm2m, WB_LWM2M_OBJ_DEVICE, 0,
                            WB_LWM2M_RES_DEVICE_MEMORY_FREE);
        }
    }

    /* Cleanup (unreachable in this example) */
    wb_lwm2m_destroy(lwm2m);
    wb_lwm2m_destroy_std_object(security_obj);
    wb_lwm2m_destroy_std_object(server_obj);
    wb_lwm2m_destroy_std_object(device_obj);
}
