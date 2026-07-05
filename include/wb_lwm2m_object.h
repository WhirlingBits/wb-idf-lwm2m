/*
 * WhirlingBits LWM2M Object / Resource Model
 *
 * Defines the LWM2M object tree: Objects → Instances → Resources.
 * Resources support read/write/execute callbacks.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @defgroup wb_lwm2m_object Object / Resource Model
 * @brief LWM2M data model with typed resources and callback-driven access
 *
 * This module defines the core LWM2M data model used throughout the
 * wb-idf-lwm2m component:
 *
 * - **Object definitions** (`wb_lwm2m_object_def_t`) — Top-level container
 *   with an Object ID, human-readable name, instance list, and resource list.
 *
 * - **Resource definitions** (`wb_lwm2m_resource_def_t`) — Per-resource metadata
 *   including data type, allowed operations (R/W/E), and user callbacks.
 *
 * - **Value type** (`wb_lwm2m_value_t`) — Tagged union supporting all 10 LWM2M
 *   data types (String, Integer, UInteger, Float, Boolean, Opaque, Time,
 *   Objlink, CoreLnk, None).
 *
 * - **Callback types** — `wb_lwm2m_read_cb_t`, `wb_lwm2m_write_cb_t`,
 *   `wb_lwm2m_execute_cb_t` for server-initiated operations.
 *
 * - **Helper macros** — `WB_LWM2M_VALUE_STRING()`, `WB_LWM2M_VALUE_INT()`,
 *   etc. for convenient value construction.
 *
 * - **Accessor functions** — `wb_lwm2m_value_get_int()`,
 *   `wb_lwm2m_value_get_float()`, `wb_lwm2m_type_name()` for type-safe access.
 *
 * ### Object Tree Structure
 *
 * LWM2M uses a three-level addressing scheme:
 * @verbatim
 *   /ObjectID/InstanceID/ResourceID
 *   Example: /3/0/0 → Device Object, Instance 0, Manufacturer
 * @endverbatim
 *
 * ### Resource Data Types (OMA LWM2M Core Spec §C.1)
 *
 * | Type | Enum | Macro | Size |
 * |------|------|-------|------|
 * | String | WB_LWM2M_RES_TYPE_STRING | WB_LWM2M_VALUE_STRING(s) | 0..N bytes UTF-8 |
 * | Integer | WB_LWM2M_RES_TYPE_INTEGER | WB_LWM2M_VALUE_INT(v) | 1/2/4/8 bytes signed |
 * | UInteger | WB_LWM2M_RES_TYPE_UINTEGER | WB_LWM2M_VALUE_UINT(v) | 1/2/4/8 bytes unsigned |
 * | Float | WB_LWM2M_RES_TYPE_FLOAT | WB_LWM2M_VALUE_FLOAT(v) | 4/8 bytes IEEE 754 |
 * | Boolean | WB_LWM2M_RES_TYPE_BOOLEAN | WB_LWM2M_VALUE_BOOL(v) | 1 byte |
 * | Opaque | WB_LWM2M_RES_TYPE_OPAQUE | WB_LWM2M_VALUE_OPAQUE(d,l) | 0..N bytes binary |
 * | Time | WB_LWM2M_RES_TYPE_TIME | WB_LWM2M_VALUE_TIME(t) | 4/8 bytes Unix epoch |
 * | Objlink | WB_LWM2M_RES_TYPE_OBJLINK | WB_LWM2M_VALUE_OBJLINK(o,i) | 4 bytes |
 * | CoreLnk | WB_LWM2M_RES_TYPE_CORELNK | WB_LWM2M_VALUE_CORELNK(s) | RFC 6690 string |
 * | None | WB_LWM2M_RES_TYPE_NONE | — | Execute-only |
 *
 * ### Resource Operations
 *
 * | Bitmask | Meaning |
 * |---------|---------|
 * | WB_LWM2M_OP_READ | Server can read this resource |
 * | WB_LWM2M_OP_WRITE | Server can write this resource |
 * | WB_LWM2M_OP_EXECUTE | Server can execute this resource |
 * | WB_LWM2M_OP_RW | Shorthand for READ + WRITE |
 *
 * ### Example: Custom Temperature Object (IPSO 3303)
 *
 * @code{.c}
 * static esp_err_t temp_read(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
 *                            wb_lwm2m_value_t *value, void *user_ctx) {
 *     if (res_id == 5700) {
 *         *value = WB_LWM2M_VALUE_FLOAT(23.5);
 *         return ESP_OK;
 *     }
 *     return ESP_ERR_NOT_FOUND;
 * }
 *
 * static const wb_lwm2m_object_def_t temp_obj = {
 *     .id = 3303,
 *     .name = "Temperature",
 *     .instances = {{ .id = 0 }},
 *     .instance_count = 1,
 *     .resources = {
 *         { .id = 5700, .type = WB_LWM2M_RES_TYPE_FLOAT,
 *           .operations = WB_LWM2M_OP_READ, .read_cb = temp_read },
 *     },
 *     .resource_count = 1,
 * };
 * @endcode
 *
 * @{
 */

#ifndef __WB_LWM2M_OBJECT_H__
#define __WB_LWM2M_OBJECT_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name Standard LWM2M Object IDs
 * OMA-TS-LightweightM2M_Core-V1_2 defined object identifiers.
 * @{
 */

#define WB_LWM2M_OBJ_SECURITY          0
#define WB_LWM2M_OBJ_SERVER            1
#define WB_LWM2M_OBJ_ACCESS_CONTROL    2
#define WB_LWM2M_OBJ_DEVICE            3
#define WB_LWM2M_OBJ_CONN_MONITORING   4
#define WB_LWM2M_OBJ_FIRMWARE_UPDATE   5
#define WB_LWM2M_OBJ_LOCATION          6
#define WB_LWM2M_OBJ_CONN_STATISTICS   7
/** @} */

/**
 * @name Security Object (/0) Resource IDs
 * @{
 */

#define WB_LWM2M_RES_SEC_SERVER_URI            0   /**< String     — LWM2M Server URI */
#define WB_LWM2M_RES_SEC_BOOTSTRAP             1   /**< Boolean    — Bootstrap-Server */
#define WB_LWM2M_RES_SEC_MODE                  2   /**< Integer    — Security Mode (0-4) */
#define WB_LWM2M_RES_SEC_PUB_KEY_IDENTITY      3   /**< Opaque     — Public Key or Identity */
#define WB_LWM2M_RES_SEC_SERVER_PUB_KEY        4   /**< Opaque     — Server Public Key */
#define WB_LWM2M_RES_SEC_SECRET_KEY            5   /**< Opaque     — Secret Key */
#define WB_LWM2M_RES_SEC_SHORT_SERVER_ID       10  /**< Integer    — Short Server ID */
/** @} */

/**
 * @name Server Object (/1) Resource IDs
 * @{
 */

#define WB_LWM2M_RES_SRV_SHORT_ID              0   /**< Integer    — Short Server ID */
#define WB_LWM2M_RES_SRV_LIFETIME              1   /**< Integer    — Lifetime (seconds) */
#define WB_LWM2M_RES_SRV_MIN_PERIOD            2   /**< Integer    — Default Min Period */
#define WB_LWM2M_RES_SRV_MAX_PERIOD            3   /**< Integer    — Default Max Period */
#define WB_LWM2M_RES_SRV_DISABLE               4   /**< Execute    — Disable */
#define WB_LWM2M_RES_SRV_DISABLE_TIMEOUT       5   /**< Integer    — Disable Timeout */
#define WB_LWM2M_RES_SRV_NOTIFICATION_STORING  6   /**< Boolean    — Notification Storing */
#define WB_LWM2M_RES_SRV_BINDING               7   /**< String     — Binding */
#define WB_LWM2M_RES_SRV_REG_UPDATE_TRIGGER    8   /**< Execute    — Registration Update Trigger */
/** @} */

/**
 * @name Device Object (/3) Resource IDs
 * @{
 */

#define WB_LWM2M_RES_DEVICE_MANUFACTURER       0   /**< String     — Manufacturer */
#define WB_LWM2M_RES_DEVICE_MODEL_NUMBER       1   /**< String     — Model Number */
#define WB_LWM2M_RES_DEVICE_SERIAL_NUMBER      2   /**< String     — Serial Number */
#define WB_LWM2M_RES_DEVICE_FW_VERSION         3   /**< String     — Firmware Version */
#define WB_LWM2M_RES_DEVICE_REBOOT             4   /**< Execute    — Reboot */
#define WB_LWM2M_RES_DEVICE_FACTORY_RESET      5   /**< Execute    — Factory Reset */
#define WB_LWM2M_RES_DEVICE_POWER_SOURCES      6   /**< Integer[]  — Available Power Sources */
#define WB_LWM2M_RES_DEVICE_POWER_VOLTAGE      7   /**< Integer[]  — Power Source Voltage (mV) */
#define WB_LWM2M_RES_DEVICE_POWER_CURRENT      8   /**< Integer[]  — Power Source Current (mA) */
#define WB_LWM2M_RES_DEVICE_BATTERY_LEVEL      9   /**< Integer    — Battery Level (%) */
#define WB_LWM2M_RES_DEVICE_MEMORY_FREE        10  /**< Integer    — Memory Free (KB) */
#define WB_LWM2M_RES_DEVICE_ERROR_CODE         11  /**< Integer[]  — Error Code */
#define WB_LWM2M_RES_DEVICE_RESET_ERROR_CODE   12  /**< Execute    — Reset Error Code */
#define WB_LWM2M_RES_DEVICE_CURRENT_TIME       13  /**< Time       — Current Time */
#define WB_LWM2M_RES_DEVICE_UTC_OFFSET         14  /**< String     — UTC Offset */
#define WB_LWM2M_RES_DEVICE_TIMEZONE           15  /**< String     — Timezone */
#define WB_LWM2M_RES_DEVICE_BINDING            16  /**< String     — Supported Binding */
#define WB_LWM2M_RES_DEVICE_TYPE               17  /**< String     — Device Type */
#define WB_LWM2M_RES_DEVICE_HW_VERSION         18  /**< String     — Hardware Version */
#define WB_LWM2M_RES_DEVICE_SW_VERSION         19  /**< String     — Software Version */
#define WB_LWM2M_RES_DEVICE_BATTERY_STATUS     20  /**< Integer    — Battery Status */
#define WB_LWM2M_RES_DEVICE_MEMORY_TOTAL       21  /**< Integer    — Memory Total (KB) */
/** @} */

/**
 * @name Firmware Update Object (/5) Resource IDs
 * @{
 */

#define WB_LWM2M_RES_FW_PACKAGE                0   /**< Opaque     — Package */
#define WB_LWM2M_RES_FW_PACKAGE_URI            1   /**< String     — Package URI */
#define WB_LWM2M_RES_FW_UPDATE                 2   /**< Execute    — Update */
#define WB_LWM2M_RES_FW_STATE                  3   /**< Integer    — State (0-3) */
#define WB_LWM2M_RES_FW_UPDATE_RESULT          5   /**< Integer    — Update Result (0-9) */
#define WB_LWM2M_RES_FW_PKG_NAME               6   /**< String     — Package Name */
#define WB_LWM2M_RES_FW_PKG_VERSION            7   /**< String     — Package Version */
#define WB_LWM2M_RES_FW_PROTOCOL_SUPPORT       8   /**< Integer[]  — Protocol Support */
#define WB_LWM2M_RES_FW_DELIVERY_METHOD        9   /**< Integer    — Delivery Method */
/** @} */

/**
 * @name Location Object (/6) Resource IDs
 * @{
 */

#define WB_LWM2M_RES_LOC_LATITUDE              0   /**< Float      — Latitude */
#define WB_LWM2M_RES_LOC_LONGITUDE             1   /**< Float      — Longitude */
#define WB_LWM2M_RES_LOC_ALTITUDE              2   /**< Float      — Altitude (m) */
#define WB_LWM2M_RES_LOC_RADIUS                3   /**< Float      — Radius (m) */
#define WB_LWM2M_RES_LOC_VELOCITY              4   /**< Opaque     — Velocity */
#define WB_LWM2M_RES_LOC_TIMESTAMP             5   /**< Time       — Timestamp */
#define WB_LWM2M_RES_LOC_SPEED                 6   /**< Float      — Speed (m/s) */
/** @} */

/**
 * @name Connectivity Monitoring Object (/4) Resource IDs
 * @{
 */

#define WB_LWM2M_RES_CONN_NETWORK_BEARER       0   /**< Integer    — Network Bearer */
#define WB_LWM2M_RES_CONN_AVAIL_BEARERS        1   /**< Integer[]  — Available Network Bearer */
#define WB_LWM2M_RES_CONN_RADIO_SIGNAL         2   /**< Integer    — Radio Signal Strength (dBm) */
#define WB_LWM2M_RES_CONN_LINK_QUALITY         3   /**< Integer    — Link Quality */
#define WB_LWM2M_RES_CONN_IP_ADDRESSES         4   /**< String[]   — IP Addresses */
#define WB_LWM2M_RES_CONN_ROUTER_IP            5   /**< String[]   — Router IP Addresses */
#define WB_LWM2M_RES_CONN_LINK_UTILIZATION     6   /**< Integer    — Link Utilization (%) */
#define WB_LWM2M_RES_CONN_APN                  7   /**< String[]   — APN */
#define WB_LWM2M_RES_CONN_CELL_ID              8   /**< Integer    — Cell ID */
#define WB_LWM2M_RES_CONN_SMNC                 9   /**< Integer    — SMNC */
#define WB_LWM2M_RES_CONN_SMCC                 10  /**< Integer    — SMCC */
/** @} */

/**
 * @brief LWM2M Security Modes (Security Object /0, Resource /0/0/2)
 *
 * Defines the DTLS mode used between the LWM2M client and server.
 */

typedef enum {
    WB_LWM2M_SEC_MODE_PSK           = 0,    /**< Pre-Shared Key */
    WB_LWM2M_SEC_MODE_RAW_PUB_KEY   = 1,    /**< Raw Public Key */
    WB_LWM2M_SEC_MODE_CERTIFICATE   = 2,    /**< Certificate */
    WB_LWM2M_SEC_MODE_NOSEC         = 3,    /**< NoSec (plain CoAP) */
    WB_LWM2M_SEC_MODE_CERT_EST      = 4,    /**< Certificate with EST */
} wb_lwm2m_security_mode_t;

/**
 * @brief Firmware Update States (/5/0/3)
 *
 * State machine for the OTA firmware update process.
 * Transition: IDLE → DOWNLOADING → DOWNLOADED → UPDATING → IDLE
 */

typedef enum {
    WB_LWM2M_FW_STATE_IDLE          = 0,
    WB_LWM2M_FW_STATE_DOWNLOADING   = 1,
    WB_LWM2M_FW_STATE_DOWNLOADED    = 2,
    WB_LWM2M_FW_STATE_UPDATING      = 3,
} wb_lwm2m_fw_state_t;

/**
 * @brief Firmware Update Result Codes (/5/0/5)
 *
 * Reported by the device after a firmware update attempt completes or fails.
 */

typedef enum {
    WB_LWM2M_FW_RESULT_DEFAULT              = 0,
    WB_LWM2M_FW_RESULT_SUCCESS              = 1,
    WB_LWM2M_FW_RESULT_NOT_ENOUGH_FLASH     = 2,
    WB_LWM2M_FW_RESULT_OUT_OF_RAM           = 3,
    WB_LWM2M_FW_RESULT_CONN_LOST            = 4,
    WB_LWM2M_FW_RESULT_INTEGRITY_FAILED     = 5,
    WB_LWM2M_FW_RESULT_UNSUPPORTED_PKG      = 6,
    WB_LWM2M_FW_RESULT_INVALID_URI          = 7,
    WB_LWM2M_FW_RESULT_UPDATE_FAILED        = 8,
    WB_LWM2M_FW_RESULT_UNSUPPORTED_PROTOCOL = 9,
} wb_lwm2m_fw_update_result_t;

/**
 * @brief LWM2M Resource Data Type enumeration
 *
 * Maps to OMA LWM2M Core Spec §C.1. Each resource has exactly one type
 * that determines its TLV/SenML encoding and value union member.
 */

typedef enum {
    WB_LWM2M_RES_TYPE_NONE      = 0,     /**< Execute-only resource (no data) */
    WB_LWM2M_RES_TYPE_STRING    = 1,     /**< UTF-8 string, 0..65535 bytes */
    WB_LWM2M_RES_TYPE_INTEGER   = 2,     /**< Signed integer: 8/16/32/64-bit */
    WB_LWM2M_RES_TYPE_UINTEGER  = 3,     /**< Unsigned integer: 8/16/32/64-bit (LWM2M 1.1+) */
    WB_LWM2M_RES_TYPE_FLOAT     = 4,     /**< IEEE 754 float: 32-bit or 64-bit */
    WB_LWM2M_RES_TYPE_BOOLEAN   = 5,     /**< Boolean: 0 or 1 */
    WB_LWM2M_RES_TYPE_OPAQUE    = 6,     /**< Opaque binary data, 0..65535 bytes */
    WB_LWM2M_RES_TYPE_TIME      = 7,     /**< Unix timestamp (signed 32/64-bit integer) */
    WB_LWM2M_RES_TYPE_OBJLINK   = 8,     /**< Object Link: ObjectID:InstanceID */
    WB_LWM2M_RES_TYPE_CORELNK   = 9,     /**< CoRE Link-Format string (RFC 6690) */
} wb_lwm2m_res_type_t;

/**
 * @name Resource Operations (bitmask)
 * Combine with bitwise OR to specify allowed operations per resource.
 * @{
 */
#define WB_LWM2M_OP_READ      (1 << 0)  /**< Server can read this resource */
#define WB_LWM2M_OP_WRITE     (1 << 1)  /**< Server can write this resource */
#define WB_LWM2M_OP_EXECUTE   (1 << 2)  /**< Server can execute this resource */
#define WB_LWM2M_OP_RW        (WB_LWM2M_OP_READ | WB_LWM2M_OP_WRITE)  /**< Read + Write */
/** @} */

/**
 * @brief Tagged union representing any LWM2M resource value
 *
 * Use the helper macros (WB_LWM2M_VALUE_STRING, WB_LWM2M_VALUE_INT, etc.)
 * to construct values. Use wb_lwm2m_value_get_int() / wb_lwm2m_value_get_float()
 * for type-safe reading with automatic conversions.
 */

typedef struct {
    wb_lwm2m_res_type_t type;
    union {
        /** WB_LWM2M_RES_TYPE_STRING / WB_LWM2M_RES_TYPE_CORELNK */
        struct {
            const char *str;
            size_t len;
        } str_val;

        /** WB_LWM2M_RES_TYPE_INTEGER */
        int64_t int_val;

        /** WB_LWM2M_RES_TYPE_UINTEGER */
        uint64_t uint_val;

        /** WB_LWM2M_RES_TYPE_FLOAT */
        double float_val;

        /** WB_LWM2M_RES_TYPE_BOOLEAN */
        bool bool_val;

        /** WB_LWM2M_RES_TYPE_OPAQUE */
        struct {
            const uint8_t *data;
            size_t len;
        } opaque_val;

        /** WB_LWM2M_RES_TYPE_TIME (seconds since 1970-01-01T00:00Z) */
        int64_t time_val;

        /** WB_LWM2M_RES_TYPE_OBJLINK */
        struct {
            uint16_t obj_id;
            uint16_t inst_id;
        } objlink_val;
    };
} wb_lwm2m_value_t;

/**
 * @name Callback Types
 * User-provided functions invoked by the LWM2M engine when the server
 * reads, writes, or executes a resource.
 * @{
 */

/**
 * @brief Read callback — invoked when a resource value is needed
 *
 * @param obj_id    Object ID
 * @param inst_id   Instance ID
 * @param res_id    Resource ID
 * @param value     Output: fill this with the current value
 * @param user_ctx  User context
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if resource not available
 */
typedef esp_err_t (*wb_lwm2m_read_cb_t)(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                                        wb_lwm2m_value_t *value, void *user_ctx);

/**
 * @brief Write callback — invoked when the server writes a resource
 *
 * @param obj_id    Object ID
 * @param inst_id   Instance ID
 * @param res_id    Resource ID
 * @param value     New value to write
 * @param user_ctx  User context
 * @return ESP_OK on success
 */
typedef esp_err_t (*wb_lwm2m_write_cb_t)(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                                         const wb_lwm2m_value_t *value, void *user_ctx);

/**
 * @brief Execute callback — invoked when the server executes a resource
 *
 * @param obj_id    Object ID
 * @param inst_id   Instance ID
 * @param res_id    Resource ID
 * @param args      Optional arguments (may be NULL)
 * @param args_len  Arguments length
 * @param user_ctx  User context
 * @return ESP_OK on success
 */
typedef esp_err_t (*wb_lwm2m_execute_cb_t)(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                                           const uint8_t *args, size_t args_len, void *user_ctx);
/** @} */

/**
 * @brief Single resource definition within an LWM2M object
 *
 * Each resource has an ID, data type, allowed operations, and optional
 * callbacks for server-initiated read/write/execute.
 */

typedef struct {
    uint16_t id;                        /**< Resource ID */
    wb_lwm2m_res_type_t type;           /**< Data type */
    uint8_t operations;                 /**< Bitmask of WB_LWM2M_OP_* */
    bool mandatory;                     /**< Required by the object spec? */
    bool multiple;                      /**< true if this is a multi-instance resource */
    wb_lwm2m_read_cb_t read_cb;         /**< Read callback (if OP_READ) */
    wb_lwm2m_write_cb_t write_cb;       /**< Write callback (if OP_WRITE) */
    wb_lwm2m_execute_cb_t execute_cb;   /**< Execute callback (if OP_EXECUTE) */
} wb_lwm2m_resource_def_t;

/**
 * @brief LWM2M Object Instance
 *
 * Simple struct holding the instance ID. An object may have up to
 * WB_LWM2M_MAX_INSTANCES instances.
 */

typedef struct {
    uint16_t id;                        /**< Instance ID (e.g. 0) */
} wb_lwm2m_instance_t;

/* Maximum object instances and resources per object */
#define WB_LWM2M_MAX_INSTANCES     4
#define WB_LWM2M_MAX_RESOURCES     24

/**
 * @brief Complete LWM2M Object Definition
 *
 * Top-level structure that fully describes an LWM2M object including its
 * ID, instances, and all resource definitions with their callbacks.
 * Pass to wb_lwm2m_add_object() to register with the client.
 *
 * @note The object definition is referenced (not copied) by the client,
 *       so it must remain valid for the lifetime of the client.
 */

typedef struct {
    uint16_t id;                                            /**< Object ID (e.g. 3 for Device) */
    const char *name;                                       /**< Human-readable name (optional) */
    bool multiple_instances;                                /**< true if object supports multiple instances */
    wb_lwm2m_instance_t instances[WB_LWM2M_MAX_INSTANCES];  /**< Object instances */
    size_t instance_count;                                  /**< Number of instances */
    wb_lwm2m_resource_def_t resources[WB_LWM2M_MAX_RESOURCES]; /**< Resource definitions */
    size_t resource_count;                                  /**< Number of resources */
    void *user_ctx;                                         /**< User context passed to callbacks */
} wb_lwm2m_object_def_t;

/**
 * @name Value Construction Macros
 * Convenience macros for constructing wb_lwm2m_value_t instances.
 * Use these in read callbacks to return resource values.
 * @{
 */

/** Construct a String value */
#define WB_LWM2M_VALUE_STRING(s) \
    (wb_lwm2m_value_t){ .type = WB_LWM2M_RES_TYPE_STRING, .str_val = { .str = (s), .len = strlen(s) } }

/** Construct a String value with explicit length (for non-NUL-terminated) */
#define WB_LWM2M_VALUE_STRING_LEN(s, l) \
    (wb_lwm2m_value_t){ .type = WB_LWM2M_RES_TYPE_STRING, .str_val = { .str = (s), .len = (l) } }

/** Construct a signed Integer value */
#define WB_LWM2M_VALUE_INT(v) \
    (wb_lwm2m_value_t){ .type = WB_LWM2M_RES_TYPE_INTEGER, .int_val = (int64_t)(v) }

/** Construct an unsigned Integer value (LWM2M 1.1+) */
#define WB_LWM2M_VALUE_UINT(v) \
    (wb_lwm2m_value_t){ .type = WB_LWM2M_RES_TYPE_UINTEGER, .uint_val = (uint64_t)(v) }

/** Construct a Float value (double precision) */
#define WB_LWM2M_VALUE_FLOAT(v) \
    (wb_lwm2m_value_t){ .type = WB_LWM2M_RES_TYPE_FLOAT, .float_val = (double)(v) }

/** Construct a Boolean value */
#define WB_LWM2M_VALUE_BOOL(v) \
    (wb_lwm2m_value_t){ .type = WB_LWM2M_RES_TYPE_BOOLEAN, .bool_val = (bool)(v) }

/** Construct an Opaque (binary) value */
#define WB_LWM2M_VALUE_OPAQUE(d, l) \
    (wb_lwm2m_value_t){ .type = WB_LWM2M_RES_TYPE_OPAQUE, .opaque_val = { .data = (const uint8_t *)(d), .len = (l) } }

/** Construct a Time value (Unix timestamp) */
#define WB_LWM2M_VALUE_TIME(t) \
    (wb_lwm2m_value_t){ .type = WB_LWM2M_RES_TYPE_TIME, .time_val = (int64_t)(t) }

/** Construct an Object Link value */
#define WB_LWM2M_VALUE_OBJLINK(oid, iid) \
    (wb_lwm2m_value_t){ .type = WB_LWM2M_RES_TYPE_OBJLINK, .objlink_val = { .obj_id = (oid), .inst_id = (iid) } }

/** Construct a CoRE Link-Format value */
#define WB_LWM2M_VALUE_CORELNK(s) \
    (wb_lwm2m_value_t){ .type = WB_LWM2M_RES_TYPE_CORELNK, .str_val = { .str = (s), .len = strlen(s) } }
/** @} */

/**
 * @name Value Accessor Functions
 * Inline helper functions for type-safe value reading with automatic
 * type coercion where sensible (e.g., int → float).
 * @{
 */

/** Get type name as string (for logging) */
static inline const char *wb_lwm2m_type_name(wb_lwm2m_res_type_t type)
{
    switch (type) {
    case WB_LWM2M_RES_TYPE_NONE:     return "None";
    case WB_LWM2M_RES_TYPE_STRING:   return "String";
    case WB_LWM2M_RES_TYPE_INTEGER:  return "Integer";
    case WB_LWM2M_RES_TYPE_UINTEGER: return "Unsigned Integer";
    case WB_LWM2M_RES_TYPE_FLOAT:    return "Float";
    case WB_LWM2M_RES_TYPE_BOOLEAN:  return "Boolean";
    case WB_LWM2M_RES_TYPE_OPAQUE:   return "Opaque";
    case WB_LWM2M_RES_TYPE_TIME:     return "Time";
    case WB_LWM2M_RES_TYPE_OBJLINK:  return "Objlink";
    case WB_LWM2M_RES_TYPE_CORELNK:  return "CoreLnk";
    default:                         return "Unknown";
    }
}

/** Get integer from value (auto-converts from int, uint, time, bool) */
static inline int64_t wb_lwm2m_value_get_int(const wb_lwm2m_value_t *v)
{
    switch (v->type) {
    case WB_LWM2M_RES_TYPE_INTEGER:  return v->int_val;
    case WB_LWM2M_RES_TYPE_UINTEGER: return (int64_t)v->uint_val;
    case WB_LWM2M_RES_TYPE_TIME:     return v->time_val;
    case WB_LWM2M_RES_TYPE_BOOLEAN:  return v->bool_val ? 1 : 0;
    default:                         return 0;
    }
}

/** Get float from value (auto-converts from int, uint) */
static inline double wb_lwm2m_value_get_float(const wb_lwm2m_value_t *v)
{
    switch (v->type) {
    case WB_LWM2M_RES_TYPE_FLOAT:    return v->float_val;
    case WB_LWM2M_RES_TYPE_INTEGER:  return (double)v->int_val;
    case WB_LWM2M_RES_TYPE_UINTEGER: return (double)v->uint_val;
    default:                         return 0.0;
    }
}

/** Check if value carries string-like data (String or CoreLnk) */
static inline bool wb_lwm2m_value_is_string(const wb_lwm2m_value_t *v)
{
    return v->type == WB_LWM2M_RES_TYPE_STRING || v->type == WB_LWM2M_RES_TYPE_CORELNK;
}

/** Check if value carries numeric data (Integer, UInteger, Float, Time) */
static inline bool wb_lwm2m_value_is_numeric(const wb_lwm2m_value_t *v)
{
    return v->type == WB_LWM2M_RES_TYPE_INTEGER || v->type == WB_LWM2M_RES_TYPE_UINTEGER ||
           v->type == WB_LWM2M_RES_TYPE_FLOAT   || v->type == WB_LWM2M_RES_TYPE_TIME;
}
/** @} */

#ifdef __cplusplus
}
#endif

/** @} */ /* end of wb_lwm2m_object group */

#endif /* __WB_LWM2M_OBJECT_H__ */
