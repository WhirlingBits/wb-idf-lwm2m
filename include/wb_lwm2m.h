/*
 * WhirlingBits LWM2M Client for ESP-IDF
 * A lightweight LWM2M client built on top of wb-idf-coap-client.
 * Implements OMA LWM2M 1.0 Registration Interface and Object Model.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @defgroup wb_lwm2m Client Lifecycle
 * @brief LWM2M client initialization, registration, and runtime management
 *
 * This module provides the top-level API for the wb-idf-lwm2m component.
 * It manages the full client lifecycle:
 *
 * 1. **Init** — Allocate resources, create internal CoAP client
 * 2. **Add Objects** — Register LWM2M object definitions
 * 3. **Start** — Connect to server, send Registration request
 * 4. **Runtime** — Automatic registration updates, notify on value changes
 * 5. **Stop** — Deregister from server, tear down CoAP session
 * 6. **Destroy** — Free all allocated resources
 *
 * ### Client State Machine
 *
 * | State | Description |
 * |-------|-------------|
 * | IDLE | Created but not started |
 * | CONNECTING | CoAP session being established |
 * | REGISTERING | Registration request sent, awaiting response |
 * | REGISTERED | Successfully registered with server |
 * | UPDATING | Registration update in progress |
 * | DEREGISTERING | Deregistration request sent |
 * | ERROR | Unrecoverable error occurred |
 *
 * ### Event Handling
 *
 * All lifecycle events are delivered through a user-provided callback
 * (`wb_lwm2m_event_handler_t`). The callback is invoked from the internal
 * CoAP processing task — avoid blocking operations inside it.
 *
 * ### Thread Safety
 *
 * - wb_lwm2m_notify(), wb_lwm2m_is_registered(), wb_lwm2m_get_state()
 *   are thread-safe and may be called from any FreeRTOS task.
 * - wb_lwm2m_add_object() must only be called before wb_lwm2m_start().
 * - wb_lwm2m_start() / wb_lwm2m_stop() / wb_lwm2m_destroy() must not
 *   be called concurrently.
 *
 * ### Example
 *
 * @code{.c}
 * wb_lwm2m_client_handle_t client = wb_lwm2m_init(&config);
 * wb_lwm2m_add_object(client, security_obj);
 * wb_lwm2m_add_object(client, server_obj);
 * wb_lwm2m_add_object(client, device_obj);
 * wb_lwm2m_start(client);
 *
 * // ... application loop ...
 * wb_lwm2m_notify(client, 3, 0, 10); // notify free memory change
 *
 * wb_lwm2m_stop(client);
 * wb_lwm2m_destroy(client);
 * @endcode
 *
 * @{
 */

#ifndef __WB_LWM2M_H__
#define __WB_LWM2M_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "wb_coap_client.h"
#include "wb_lwm2m_object.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LWM2M client state enumeration
 *
 * Reflects the current phase of the client lifecycle.
 * Query with wb_lwm2m_get_state().
 */

typedef enum {
    WB_LWM2M_STATE_IDLE,
    WB_LWM2M_STATE_CONNECTING,
    WB_LWM2M_STATE_REGISTERING,
    WB_LWM2M_STATE_REGISTERED,
    WB_LWM2M_STATE_UPDATING,
    WB_LWM2M_STATE_DEREGISTERING,
    WB_LWM2M_STATE_ERROR,
} wb_lwm2m_state_t;

/**
 * @brief LWM2M event type enumeration
 *
 * Events delivered to the user callback during client operation.
 */

typedef enum {
    WB_LWM2M_EVENT_CONNECTED,       /**< CoAP session established */
    WB_LWM2M_EVENT_DISCONNECTED,    /**< CoAP session lost */
    WB_LWM2M_EVENT_REGISTERED,      /**< Registration successful */
    WB_LWM2M_EVENT_REG_FAILED,      /**< Registration failed */
    WB_LWM2M_EVENT_REG_UPDATED,     /**< Registration update successful */
    WB_LWM2M_EVENT_DEREGISTERED,    /**< Deregistration successful */
    WB_LWM2M_EVENT_BOOTSTRAP_DONE,  /**< Bootstrap completed (future use) */
    WB_LWM2M_EVENT_ERROR,           /**< Generic error */
} wb_lwm2m_event_type_t;

/**
 * @brief LWM2M event data passed to the user callback
 *
 * Contains the event type and optional event-specific payload.
 * The pointer is valid only for the duration of the callback invocation.
 */
typedef struct {
    wb_lwm2m_event_type_t type;
    const void *data;    /**< Event-specific data (may be NULL) */
    size_t data_len;
} wb_lwm2m_event_t;

/**
 * @brief User event handler callback
 *
 * Called from the internal CoAP task when a lifecycle event occurs.
 * Avoid blocking operations inside this callback.
 *
 * @param event     Event data (valid only during callback invocation)
 * @param user_ctx  User context pointer from wb_lwm2m_client_config_t
 */
typedef void (*wb_lwm2m_event_handler_t)(wb_lwm2m_event_t *event, void *user_ctx);

/**
 * @brief LWM2M client configuration
 *
 * Passed to wb_lwm2m_init() to configure server connection, security,
 * and task parameters.
 */

typedef struct {
    const char *server_uri;             /**< LWM2M server URI, e.g. "coap://leshan.eclipseprojects.io:5683" */
    const char *endpoint_name;          /**< Device endpoint name (unique identifier) */
    uint32_t lifetime;                  /**< Registration lifetime in seconds (default: 86400) */
    const char *binding;                /**< Binding mode: "U" (UDP), "UQ", "S", "SQ" (default: "U") */
    const char *lwm2m_version;          /**< LWM2M version: "1.0" or "1.1" (default: "1.0") */

    /* Security (passed to wb-idf-coap-client) */
    wb_coap_security_t security;
    wb_coap_psk_cfg_t psk;
    wb_coap_pki_cfg_t pki;

    /* Callbacks */
    wb_lwm2m_event_handler_t event_handler;
    void *user_ctx;

    /* Task configuration */
    uint32_t task_stack_size;           /**< Stack size for CoAP task (default: 8192) */
    uint8_t task_priority;              /**< Task priority (default: 5) */
} wb_lwm2m_client_config_t;

/**
 * @brief Opaque client handle
 *
 * Returned by wb_lwm2m_init() and passed to all other API functions.
 */
typedef struct wb_lwm2m_client *wb_lwm2m_client_handle_t;

/* ── Public API ────────────────────────────────────────────────────────── */

/**
 * @brief Create an LWM2M client instance
 *
 * Allocates resources and creates the underlying CoAP client.
 * Does NOT connect or register yet — call wb_lwm2m_start() for that.
 *
 * @param config  Client configuration
 * @return Client handle, or NULL on failure
 */
wb_lwm2m_client_handle_t wb_lwm2m_init(const wb_lwm2m_client_config_t *config);

/**
 * @brief Register an LWM2M object with the client
 *
 * Must be called BEFORE wb_lwm2m_start(). The object definition
 * is referenced (not copied), so it must remain valid.
 *
 * @param client  Client handle
 * @param obj     Object definition
 * @return ESP_OK on success
 */
esp_err_t wb_lwm2m_add_object(wb_lwm2m_client_handle_t client,
                              const wb_lwm2m_object_def_t *obj);

/**
 * @brief Start the LWM2M client
 *
 * Connects the CoAP client and sends the LWM2M Registration request.
 * Registration updates are sent automatically based on the configured lifetime.
 *
 * @param client  Client handle
 * @return ESP_OK on success
 */
esp_err_t wb_lwm2m_start(wb_lwm2m_client_handle_t client);

/**
 * @brief Send a registration update
 *
 * Typically called automatically, but can be triggered manually
 * (e.g., after object list changes).
 *
 * @param client  Client handle
 * @return ESP_OK on success
 */
esp_err_t wb_lwm2m_update_registration(wb_lwm2m_client_handle_t client);

/**
 * @brief Deregister and stop the LWM2M client
 *
 * Sends a Deregistration request and tears down the CoAP session.
 *
 * @param client  Client handle
 * @return ESP_OK on success
 */
esp_err_t wb_lwm2m_stop(wb_lwm2m_client_handle_t client);

/**
 * @brief Destroy the LWM2M client and free all resources
 *
 * @param client  Client handle
 * @return ESP_OK on success
 */
esp_err_t wb_lwm2m_destroy(wb_lwm2m_client_handle_t client);

/**
 * @brief Get the current LWM2M client state
 *
 * @param client  Client handle
 * @return Current state
 */
wb_lwm2m_state_t wb_lwm2m_get_state(wb_lwm2m_client_handle_t client);

/**
 * @brief Notify the server about a changed resource value
 *
 * Sends a CoAP POST with the resource value in SenML-JSON format.
 * Can be used to proactively push telemetry data.
 *
 * @param client   Client handle
 * @param obj_id   Object ID
 * @param inst_id  Instance ID
 * @param res_id   Resource ID
 * @return ESP_OK if the notification was queued
 */
esp_err_t wb_lwm2m_notify(wb_lwm2m_client_handle_t client,
                          uint16_t obj_id, uint16_t inst_id, uint16_t res_id);

/**
 * @brief Check if the client is currently registered
 *
 * @param client  Client handle
 * @return true if registered
 */
bool wb_lwm2m_is_registered(wb_lwm2m_client_handle_t client);

#ifdef __cplusplus
}
#endif

/** @} */ /* end of wb_lwm2m group */

#endif /* __WB_LWM2M_H__ */
