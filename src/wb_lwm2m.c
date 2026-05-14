/*
 * WhirlingBits LWM2M Client for ESP-IDF
 *
 * Implements registration, registration update, deregistration, and
 * resource notification on top of wb-idf-coap-client.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wb_lwm2m.h"
#include "wb_lwm2m_encode.h"

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static const char *TAG = "WB_LWM2M";

/* ── Limits ── */

#define WB_LWM2M_MAX_OBJECTS       16
#define WB_LWM2M_LINK_BUF_SIZE    512
#define WB_LWM2M_SENML_BUF_SIZE   256
#define WB_LWM2M_LOCATION_MAX     128

/* ── Internal client structure ── */

struct wb_lwm2m_client {
    /* Configuration (copied at init) */
    char *server_uri;
    char *endpoint_name;
    uint32_t lifetime;
    char binding[8];
    char lwm2m_version[8];
    wb_coap_security_t security;
    wb_coap_psk_cfg_t psk;
    wb_coap_pki_cfg_t pki;
    wb_lwm2m_event_handler_t event_handler;
    void *user_ctx;
    uint32_t task_stack_size;
    uint8_t task_priority;

    /* Object registry */
    const wb_lwm2m_object_def_t *objects[WB_LWM2M_MAX_OBJECTS];
    size_t object_count;

    /* CoAP client */
    wb_coap_client_handle_t coap;

    /* Registration state */
    wb_lwm2m_state_t state;
    char reg_location[WB_LWM2M_LOCATION_MAX];   /**< Server-assigned location after POST /rd */

    /* Registration update timer */
    TimerHandle_t update_timer;
};

/* ── Forward declarations ── */

static void coap_event_handler(wb_coap_event_data_t *event_data, void *user_ctx);
static void dispatch_lwm2m_event(struct wb_lwm2m_client *c, wb_lwm2m_event_type_t type,
                                 const void *data, size_t data_len);
static esp_err_t send_registration(struct wb_lwm2m_client *c);
static void update_timer_cb(TimerHandle_t timer);
static void start_update_timer(struct wb_lwm2m_client *c);
static void stop_update_timer(struct wb_lwm2m_client *c);

/* ── Public API ── */

wb_lwm2m_client_handle_t wb_lwm2m_init(const wb_lwm2m_client_config_t *config)
{
    if (config == NULL || config->server_uri == NULL || config->endpoint_name == NULL) {
        ESP_LOGE(TAG, "Invalid config: server_uri and endpoint_name are required");
        return NULL;
    }

    struct wb_lwm2m_client *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        ESP_LOGE(TAG, "Out of memory");
        return NULL;
    }

    /* Copy configuration */
    c->server_uri = strdup(config->server_uri);
    c->endpoint_name = strdup(config->endpoint_name);
    if (c->server_uri == NULL || c->endpoint_name == NULL) {
        free(c->server_uri);
        free(c->endpoint_name);
        free(c);
        return NULL;
    }

    c->lifetime = config->lifetime > 0 ? config->lifetime : 86400;
    snprintf(c->binding, sizeof(c->binding), "%s",
             config->binding ? config->binding : "U");
    snprintf(c->lwm2m_version, sizeof(c->lwm2m_version), "%s",
             config->lwm2m_version ? config->lwm2m_version : "1.0");
    c->security = config->security;
    c->psk = config->psk;
    c->pki = config->pki;
    c->event_handler = config->event_handler;
    c->user_ctx = config->user_ctx;
    c->task_stack_size = config->task_stack_size;
    c->task_priority = config->task_priority;
    c->state = WB_LWM2M_STATE_IDLE;

    ESP_LOGI(TAG, "LWM2M client initialized, ep=%s, server=%s", c->endpoint_name, c->server_uri);
    return c;
}

esp_err_t wb_lwm2m_add_object(wb_lwm2m_client_handle_t client,
                              const wb_lwm2m_object_def_t *obj)
{
    if (client == NULL || obj == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (client->object_count >= WB_LWM2M_MAX_OBJECTS) {
        ESP_LOGE(TAG, "Maximum number of objects reached");
        return ESP_ERR_NO_MEM;
    }
    if (client->state != WB_LWM2M_STATE_IDLE) {
        ESP_LOGE(TAG, "Cannot add objects after start");
        return ESP_ERR_INVALID_STATE;
    }

    client->objects[client->object_count++] = obj;
    ESP_LOGI(TAG, "Added object /%" PRIu16 " (%s), %zu instances, %zu resources",
             obj->id, obj->name ? obj->name : "unnamed",
             obj->instance_count, obj->resource_count);
    return ESP_OK;
}

esp_err_t wb_lwm2m_start(wb_lwm2m_client_handle_t client)
{
    if (client == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (client->state != WB_LWM2M_STATE_IDLE) {
        ESP_LOGW(TAG, "Client already started");
        return ESP_ERR_INVALID_STATE;
    }

    /* Create the underlying CoAP client */
    wb_coap_client_config_t coap_cfg = {
        .uri = client->server_uri,
        .security = client->security,
        .psk = client->psk,
        .pki = client->pki,
        .event_handler = coap_event_handler,
        .user_ctx = client,
        .task_stack_size = client->task_stack_size,
        .task_priority = client->task_priority,
    };

    client->coap = wb_coap_client_init(&coap_cfg);
    if (client->coap == NULL) {
        ESP_LOGE(TAG, "Failed to init CoAP client");
        return ESP_FAIL;
    }

    client->state = WB_LWM2M_STATE_CONNECTING;
    esp_err_t err = wb_coap_client_start(client->coap);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start CoAP client: %s", esp_err_to_name(err));
        wb_coap_client_destroy(client->coap);
        client->coap = NULL;
        client->state = WB_LWM2M_STATE_IDLE;
        return err;
    }

    ESP_LOGI(TAG, "LWM2M client starting...");
    return ESP_OK;
}

esp_err_t wb_lwm2m_update_registration(wb_lwm2m_client_handle_t client)
{
    if (client == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (client->state != WB_LWM2M_STATE_REGISTERED) {
        ESP_LOGW(TAG, "Not registered, cannot update");
        return ESP_ERR_INVALID_STATE;
    }
    if (client->reg_location[0] == '\0') {
        ESP_LOGW(TAG, "No registration location, cannot update");
        return ESP_ERR_INVALID_STATE;
    }

    client->state = WB_LWM2M_STATE_UPDATING;

    /* POST to /rd/{location} with no body (simple keepalive update) */
    ESP_LOGI(TAG, "Sending registration update to %s", client->reg_location);
    esp_err_t err = wb_coap_client_post(client->coap, client->reg_location, NULL, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send registration update: %s", esp_err_to_name(err));
        client->state = WB_LWM2M_STATE_REGISTERED;
    }
    return err;
}

esp_err_t wb_lwm2m_stop(wb_lwm2m_client_handle_t client)
{
    if (client == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    stop_update_timer(client);

    /* Send deregistration if we have a location */
    if (client->state == WB_LWM2M_STATE_REGISTERED && client->reg_location[0] != '\0') {
        client->state = WB_LWM2M_STATE_DEREGISTERING;
        ESP_LOGI(TAG, "Sending deregistration to %s", client->reg_location);
        wb_coap_client_delete(client->coap, client->reg_location);
        /* Give it a moment to send */
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (client->coap) {
        wb_coap_client_stop(client->coap);
    }

    client->state = WB_LWM2M_STATE_IDLE;
    client->reg_location[0] = '\0';
    dispatch_lwm2m_event(client, WB_LWM2M_EVENT_DEREGISTERED, NULL, 0);

    ESP_LOGI(TAG, "LWM2M client stopped");
    return ESP_OK;
}

esp_err_t wb_lwm2m_destroy(wb_lwm2m_client_handle_t client)
{
    if (client == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (client->state != WB_LWM2M_STATE_IDLE) {
        wb_lwm2m_stop(client);
    }

    if (client->coap) {
        wb_coap_client_destroy(client->coap);
        client->coap = NULL;
    }

    if (client->update_timer) {
        xTimerDelete(client->update_timer, pdMS_TO_TICKS(1000));
        client->update_timer = NULL;
    }

    free(client->server_uri);
    free(client->endpoint_name);
    free(client);

    ESP_LOGI(TAG, "LWM2M client destroyed");
    return ESP_OK;
}

wb_lwm2m_state_t wb_lwm2m_get_state(wb_lwm2m_client_handle_t client)
{
    if (client == NULL) {
        return WB_LWM2M_STATE_IDLE;
    }
    return client->state;
}

bool wb_lwm2m_is_registered(wb_lwm2m_client_handle_t client)
{
    return client != NULL && client->state == WB_LWM2M_STATE_REGISTERED;
}

esp_err_t wb_lwm2m_notify(wb_lwm2m_client_handle_t client,
                          uint16_t obj_id, uint16_t inst_id, uint16_t res_id)
{
    if (client == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (client->state != WB_LWM2M_STATE_REGISTERED) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Find the object and resource */
    const wb_lwm2m_object_def_t *obj = NULL;
    for (size_t i = 0; i < client->object_count; i++) {
        if (client->objects[i]->id == obj_id) {
            obj = client->objects[i];
            break;
        }
    }
    if (obj == NULL) {
        ESP_LOGW(TAG, "Object /%" PRIu16 " not found", obj_id);
        return ESP_ERR_NOT_FOUND;
    }

    const wb_lwm2m_resource_def_t *res = NULL;
    for (size_t i = 0; i < obj->resource_count; i++) {
        if (obj->resources[i].id == res_id) {
            res = &obj->resources[i];
            break;
        }
    }
    if (res == NULL || res->read_cb == NULL) {
        ESP_LOGW(TAG, "Resource /%" PRIu16 "/%" PRIu16 "/%" PRIu16 " not readable",
                 obj_id, inst_id, res_id);
        return ESP_ERR_NOT_FOUND;
    }

    /* Read the current value via callback */
    wb_lwm2m_value_t value = {0};
    esp_err_t err = res->read_cb(obj_id, inst_id, res_id, &value, obj->user_ctx);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Read callback failed for /%" PRIu16 "/%" PRIu16 "/%" PRIu16,
                 obj_id, inst_id, res_id);
        return err;
    }

    /* Encode as SenML-JSON */
    char buf[WB_LWM2M_SENML_BUF_SIZE];
    int len = wb_lwm2m_encode_senml_json(obj_id, inst_id, res_id, &value, buf, sizeof(buf));
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to encode SenML-JSON");
        return ESP_FAIL;
    }

    /* POST to the resource path */
    char path[32];
    snprintf(path, sizeof(path), "/%" PRIu16 "/%" PRIu16 "/%" PRIu16,
             obj_id, inst_id, res_id);

    ESP_LOGD(TAG, "Notify %s: %s", path, buf);
    return wb_coap_client_post(client->coap, path,
                               (const uint8_t *)buf, (size_t)len,
                               WB_COAP_FORMAT_JSON);
}

/* ── Internal: CoAP event handler ── */

static void coap_event_handler(wb_coap_event_data_t *event_data, void *user_ctx)
{
    struct wb_lwm2m_client *c = (struct wb_lwm2m_client *)user_ctx;
    if (c == NULL || event_data == NULL) {
        return;
    }

    switch (event_data->event) {
    case WB_COAP_EVENT_CONNECTED:
        ESP_LOGI(TAG, "CoAP connected, sending registration...");
        dispatch_lwm2m_event(c, WB_LWM2M_EVENT_CONNECTED, NULL, 0);
        send_registration(c);
        break;

    case WB_COAP_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "CoAP disconnected");
        stop_update_timer(c);
        c->state = WB_LWM2M_STATE_ERROR;
        c->reg_location[0] = '\0';
        dispatch_lwm2m_event(c, WB_LWM2M_EVENT_DISCONNECTED, NULL, 0);
        break;

    case WB_COAP_EVENT_RESPONSE:
        if (event_data->response) {
            wb_coap_response_t *resp = event_data->response;
            const char *path = resp->request_path;
            wb_coap_response_code_t code = resp->code;

            ESP_LOGD(TAG, "Response for %s: 0x%02X", path ? path : "(null)", code);

            /* Handle registration response */
            if (c->state == WB_LWM2M_STATE_REGISTERING && path &&
                strcmp(path, "/rd") == 0) {
                if (code == WB_COAP_RESP_CREATED) {
                    /* Extract Location-Path from response options */
                    if (resp->location_path[0] != '\0') {
                        snprintf(c->reg_location, sizeof(c->reg_location),
                                 "%s", resp->location_path);
                    } else {
                        /* Fallback: use endpoint name as location */
                        snprintf(c->reg_location, sizeof(c->reg_location),
                                 "/rd/%s", c->endpoint_name);
                    }

                    c->state = WB_LWM2M_STATE_REGISTERED;
                    ESP_LOGI(TAG, "Registration successful, location=%s", c->reg_location);
                    start_update_timer(c);
                    dispatch_lwm2m_event(c, WB_LWM2M_EVENT_REGISTERED, NULL, 0);
                } else {
                    c->state = WB_LWM2M_STATE_ERROR;
                    ESP_LOGE(TAG, "Registration failed with code 0x%02X", code);
                    dispatch_lwm2m_event(c, WB_LWM2M_EVENT_REG_FAILED, NULL, 0);
                }
            }
            /* Handle registration update response */
            else if (c->state == WB_LWM2M_STATE_UPDATING) {
                if (code == WB_COAP_RESP_CHANGED) {
                    c->state = WB_LWM2M_STATE_REGISTERED;
                    ESP_LOGI(TAG, "Registration update successful");
                    dispatch_lwm2m_event(c, WB_LWM2M_EVENT_REG_UPDATED, NULL, 0);
                } else {
                    ESP_LOGW(TAG, "Registration update failed (0x%02X), re-registering...", code);
                    c->state = WB_LWM2M_STATE_REGISTERING;
                    c->reg_location[0] = '\0';
                    send_registration(c);
                }
            }
            /* Handle deregistration response */
            else if (c->state == WB_LWM2M_STATE_DEREGISTERING) {
                c->state = WB_LWM2M_STATE_IDLE;
                c->reg_location[0] = '\0';
                ESP_LOGI(TAG, "Deregistration complete");
                dispatch_lwm2m_event(c, WB_LWM2M_EVENT_DEREGISTERED, NULL, 0);
            }
        }
        break;

    case WB_COAP_EVENT_ERROR:
        ESP_LOGE(TAG, "CoAP error");
        if (c->state == WB_LWM2M_STATE_REGISTERING) {
            c->state = WB_LWM2M_STATE_ERROR;
            dispatch_lwm2m_event(c, WB_LWM2M_EVENT_REG_FAILED, NULL, 0);
        } else {
            dispatch_lwm2m_event(c, WB_LWM2M_EVENT_ERROR, NULL, 0);
        }
        break;
    }
}

/* ── Internal: Send LWM2M Registration ── */

static esp_err_t send_registration(struct wb_lwm2m_client *c)
{
    c->state = WB_LWM2M_STATE_REGISTERING;

    /* Build the link-format payload: </1/0>,</3/0>,</5/0>,... */
    char link_buf[WB_LWM2M_LINK_BUF_SIZE];
    int link_len = wb_lwm2m_encode_link_format(c->objects, c->object_count,
                                               link_buf, sizeof(link_buf));
    if (link_len < 0) {
        ESP_LOGE(TAG, "Failed to encode link-format payload");
        c->state = WB_LWM2M_STATE_ERROR;
        return ESP_FAIL;
    }

    /*
     * LWM2M Registration: POST /rd?ep=<name>&lt=<lifetime>&b=<binding>&lwm2m=<version>
     *
     * Since wb-idf-coap-client splits by '/' for URI-Path options,
     * we need to put query params in the path. The CoAP client will
     * send them as URI-Path options. We construct the full path with
     * query parameters embedded.
     *
     * Note: Query parameters should be CoAP URI-Query options, but
     * wb-idf-coap-client currently only handles URI-Path splitting.
     * As a workaround, we encode query params in the path, which is
     * compatible with many LWM2M servers.
     */
    char reg_path[256];
    snprintf(reg_path, sizeof(reg_path),
             "/rd?ep=%s&lt=%" PRIu32 "&b=%s&lwm2m=%s",
             c->endpoint_name, c->lifetime, c->binding, c->lwm2m_version);

    ESP_LOGI(TAG, "Sending registration: POST %s", reg_path);
    ESP_LOGI(TAG, "Payload (%d bytes): %s", link_len, link_buf);

    /* Content-Format: application/link-format (40) */
    esp_err_t err = wb_coap_client_post(c->coap, reg_path,
                                        (const uint8_t *)link_buf, (size_t)link_len,
                                        WB_COAP_FORMAT_LINK_FORMAT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send registration: %s", esp_err_to_name(err));
        c->state = WB_LWM2M_STATE_ERROR;
    }
    return err;
}

/* ── Internal: Event dispatch ── */

static void dispatch_lwm2m_event(struct wb_lwm2m_client *c, wb_lwm2m_event_type_t type,
                                 const void *data, size_t data_len)
{
    if (c->event_handler == NULL) {
        return;
    }
    wb_lwm2m_event_t event = {
        .type = type,
        .data = data,
        .data_len = data_len,
    };
    c->event_handler(&event, c->user_ctx);
}

/* ── Internal: Registration update timer ── */

static void update_timer_cb(TimerHandle_t timer)
{
    struct wb_lwm2m_client *c = (struct wb_lwm2m_client *)pvTimerGetTimerID(timer);
    if (c == NULL) {
        return;
    }

    if (c->state == WB_LWM2M_STATE_REGISTERED) {
        ESP_LOGD(TAG, "Registration update timer fired");
        wb_lwm2m_update_registration(c);
    }
}

static void start_update_timer(struct wb_lwm2m_client *c)
{
    stop_update_timer(c);

    /* Send update at ~80% of the lifetime to avoid expiration */
    uint32_t interval_ms = (c->lifetime * 800);  /* 80% of lifetime in ms */
    if (interval_ms < 10000) {
        interval_ms = 10000;  /* Minimum 10 seconds */
    }

    c->update_timer = xTimerCreate("lwm2m_update", pdMS_TO_TICKS(interval_ms),
                                   pdTRUE, c, update_timer_cb);
    if (c->update_timer != NULL) {
        xTimerStart(c->update_timer, pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Registration update timer started (interval=%" PRIu32 " ms)", interval_ms);
    }
}

static void stop_update_timer(struct wb_lwm2m_client *c)
{
    if (c->update_timer != NULL) {
        xTimerStop(c->update_timer, pdMS_TO_TICKS(1000));
        xTimerDelete(c->update_timer, pdMS_TO_TICKS(1000));
        c->update_timer = NULL;
    }
}
