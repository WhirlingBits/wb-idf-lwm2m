/*
 * WhirlingBits LWM2M — Standard Object Definitions
 *
 * Pre-built OMA LWM2M standard object definitions according to
 * OMA-TS-LightweightM2M_Core-V1_2 and OMA registered objects:
 *
 *   Object 0  — LWM2M Security
 *   Object 1  — LWM2M Server
 *   Object 3  — Device
 *   Object 4  — Connectivity Monitoring
 *   Object 5  — Firmware Update
 *   Object 6  — Location
 *   Object 7  — Connectivity Statistics
 *
 * Usage:
 *   1. Fill a wb_lwm2m_device_info_t struct with your device data
 *   2. Call wb_lwm2m_create_device_object() to get a ready-to-use object def
 *   3. Register it with wb_lwm2m_add_object()
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @defgroup wb_lwm2m_std_objects Standard Object Factory
 * @brief Pre-built factory functions for OMA LWM2M standard objects
 *
 * This module provides convenient factory functions that create fully
 * configured `wb_lwm2m_object_def_t` instances for the standard OMA
 * LWM2M objects. Each factory function:
 *
 * 1. Allocates a `wb_lwm2m_object_def_t` on the heap
 * 2. Populates all mandatory resources with appropriate callbacks
 * 3. Adds optional resources when the corresponding info field is non-NULL
 * 4. Returns ownership to the caller (free with wb_lwm2m_destroy_std_object())
 *
 * ### Supported Standard Objects
 *
 * | Object | ID | Factory Function |
 * |--------|----|-----------------|
 * | Security | /0 | wb_lwm2m_create_security_object() |
 * | Server | /1 | wb_lwm2m_create_server_object() |
 * | Device | /3 | wb_lwm2m_create_device_object() |
 * | Connectivity Monitoring | /4 | wb_lwm2m_create_conn_mon_object() |
 * | Firmware Update | /5 | wb_lwm2m_create_firmware_object() |
 * | Location | /6 | wb_lwm2m_create_location_object() |
 * | Connectivity Statistics | /7 | wb_lwm2m_create_conn_stats_object() |
 *
 * ### Usage Pattern
 *
 * @code{.c}
 * // Create objects
 * wb_lwm2m_object_def_t *dev = wb_lwm2m_create_device_object(&dev_info, NULL);
 * wb_lwm2m_object_def_t *srv = wb_lwm2m_create_server_object(&srv_info, 0, NULL, NULL, NULL);
 *
 * // Register with client
 * wb_lwm2m_add_object(client, dev);
 * wb_lwm2m_add_object(client, srv);
 *
 * // ... use client ...
 *
 * // Cleanup
 * wb_lwm2m_destroy_std_object(dev);
 * wb_lwm2m_destroy_std_object(srv);
 * @endcode
 *
 * @{
 */

#ifndef __WB_LWM2M_STD_OBJECTS_H__
#define __WB_LWM2M_STD_OBJECTS_H__

#include "wb_lwm2m_object.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ────────────────────────────────────────────────────────
 *  Security Object (/0)
 * ──────────────────────────────────────────────────────── */

/**
 * @brief Configuration for the LWM2M Security Object (/0)
 *
 * Contains all parameters needed to create a Security Object instance.
 * The Security Object defines the keying material and server URI used
 * to establish the DTLS session with the LWM2M server.
 */

typedef struct {
    const char *server_uri;             /**< LWM2M Server URI (mandatory) */
    bool is_bootstrap;                  /**< true if this is a Bootstrap server */
    wb_lwm2m_security_mode_t mode;      /**< Security mode (PSK, Cert, NoSec, …) */
    const uint8_t *pub_key_identity;    /**< Public key or PSK identity (may be NULL) */
    size_t pub_key_identity_len;
    const uint8_t *server_pub_key;      /**< Server public key (may be NULL) */
    size_t server_pub_key_len;
    const uint8_t *secret_key;          /**< Secret key or PSK (may be NULL) */
    size_t secret_key_len;
    uint16_t short_server_id;           /**< Short Server ID (1-65534) */
} wb_lwm2m_security_info_t;

/**
 * @brief Create an LWM2M Security Object (/0) definition
 *
 * @param info     Security parameters
 * @param inst_id  Instance ID (typically 0)
 * @return Heap-allocated object def, or NULL on error. Caller owns the memory.
 */
wb_lwm2m_object_def_t *wb_lwm2m_create_security_object(const wb_lwm2m_security_info_t *info,
                                                        uint16_t inst_id);

/* ────────────────────────────────────────────────────────
 *  Server Object (/1)
 * ──────────────────────────────────────────────────────── */

/**
 * @brief Configuration for the LWM2M Server Object (/1)
 *
 * Defines the server registration parameters: lifetime, binding mode,
 * and notification behavior. The Short Server ID must match the value
 * used in the corresponding Security Object.
 */

typedef struct {
    uint16_t short_server_id;           /**< Short Server ID (must match Security obj) */
    uint32_t lifetime;                  /**< Registration Lifetime in seconds */
    uint32_t default_min_period;        /**< Default Minimum Period (0 = disabled) */
    uint32_t default_max_period;        /**< Default Maximum Period (0 = disabled) */
    uint32_t disable_timeout;           /**< Disable Timeout (0 = disabled) */
    bool notification_storing;          /**< Store notifications while offline? */
    const char *binding;                /**< Binding mode: "U", "UQ", "S", "SQ", "US" */
} wb_lwm2m_server_info_t;

/**
 * @brief Create an LWM2M Server Object (/1) definition
 *
 * @param info     Server parameters
 * @param inst_id  Instance ID (typically 0)
 * @param reg_update_cb  Optional execute callback for Reg Update Trigger (res 8)
 * @param disable_cb     Optional execute callback for Disable (res 4)
 * @param user_ctx User context for callbacks
 * @return Heap-allocated object def, or NULL on error. Caller owns the memory.
 */
wb_lwm2m_object_def_t *wb_lwm2m_create_server_object(const wb_lwm2m_server_info_t *info,
                                                      uint16_t inst_id,
                                                      wb_lwm2m_execute_cb_t reg_update_cb,
                                                      wb_lwm2m_execute_cb_t disable_cb,
                                                      void *user_ctx);

/* ────────────────────────────────────────────────────────
 *  Device Object (/3)
 * ──────────────────────────────────────────────────────── */

/**
 * @brief Configuration for the LWM2M Device Object (/3)
 *
 * Provides device identity and diagnostic information. Mandatory fields
 * (manufacturer, model_number, serial_number, binding) are always included.
 * Optional fields are included only when non-NULL.
 *
 * Dynamic resources (battery level, free memory, current time, error code)
 * are auto-populated by a built-in read callback unless overridden via
 * the read_cb field.
 */

typedef struct {
    /* Mandatory */
    const char *manufacturer;           /**< Manufacturer name (mandatory, res 0) */
    const char *model_number;           /**< Model number (mandatory, res 1) */
    const char *serial_number;          /**< Serial number (mandatory, res 2) */
    const char *binding;                /**< Supported binding modes (mandatory, res 16) */

    /* Optional static strings */
    const char *firmware_version;       /**< Firmware version (res 3) */
    const char *device_type;            /**< Device type (res 17) */
    const char *hardware_version;       /**< Hardware version (res 18) */
    const char *software_version;       /**< Software version (res 19) */
    const char *utc_offset;             /**< UTC offset, e.g. "+02:00" (res 14) */
    const char *timezone;               /**< IANA timezone, e.g. "Europe/Berlin" (res 15) */

    /* Optional dynamic callbacks — if NULL, a built-in default is used */
    wb_lwm2m_read_cb_t read_cb;         /**< Custom read callback for dynamic resources */
    wb_lwm2m_execute_cb_t reboot_cb;    /**< Reboot execute callback (res 4) */
    wb_lwm2m_execute_cb_t factory_reset_cb; /**< Factory Reset execute callback (res 5) */
    wb_lwm2m_execute_cb_t reset_error_cb;   /**< Reset Error Code execute callback (res 12) */
} wb_lwm2m_device_info_t;

/**
 * @brief Create an LWM2M Device Object (/3) definition
 *
 * All mandatory resources (0, 1, 2, 11, 16) are always included.
 * Optional resources are included when the corresponding field in info is non-NULL.
 *
 * Dynamic resources (battery level, free memory, error code, current time)
 * use a built-in read callback. Override with info->read_cb if needed.
 *
 * @param info     Device info
 * @param user_ctx User context for callbacks
 * @return Heap-allocated object def, or NULL on error. Caller owns the memory.
 */
wb_lwm2m_object_def_t *wb_lwm2m_create_device_object(const wb_lwm2m_device_info_t *info,
                                                      void *user_ctx);

/* ────────────────────────────────────────────────────────
 *  Connectivity Monitoring Object (/4)
 * ──────────────────────────────────────────────────────── */

/**
 * @brief Configuration for the Connectivity Monitoring Object (/4)
 *
 * All resource values are provided by the user's read callback.
 * Typical values include network bearer type, signal strength,
 * IP addresses, and link quality.
 */

typedef struct {
    wb_lwm2m_read_cb_t read_cb;         /**< Read callback for all resources (mandatory) */
} wb_lwm2m_conn_mon_info_t;

/**
 * @brief Create a Connectivity Monitoring Object (/4) definition
 *
 * Resources: Network Bearer (0), Avail Bearers (1), Signal Strength (2),
 *            Link Quality (3), IP Addresses (4), Router IPs (5),
 *            Link Utilization (6), APN (7), Cell ID (8), SMNC (9), SMCC (10)
 *
 * @param info     Config with read callback
 * @param user_ctx User context
 * @return Heap-allocated object def, or NULL. Caller owns the memory.
 */
wb_lwm2m_object_def_t *wb_lwm2m_create_conn_mon_object(const wb_lwm2m_conn_mon_info_t *info,
                                                        void *user_ctx);

/* ────────────────────────────────────────────────────────
 *  Firmware Update Object (/5)
 * ──────────────────────────────────────────────────────── */

/**
 * @brief Configuration for the Firmware Update Object (/5)
 *
 * Enables OTA firmware updates via the LWM2M server. The user provides
 * callbacks for reading state/result, writing the package URI, and
 * executing the update.
 *
 * ### State Machine
 *
 * | State | Value | Transition |
 * |-------|-------|-----------|
 * | IDLE | 0 | → DOWNLOADING (on Package URI write) |
 * | DOWNLOADING | 1 | → DOWNLOADED (download complete) |
 * | DOWNLOADED | 2 | → UPDATING (on Update execute) |
 * | UPDATING | 3 | → IDLE (on success or failure) |
 */

typedef struct {
    wb_lwm2m_read_cb_t read_cb;         /**< Read callback for state/result/pkg info */
    wb_lwm2m_write_cb_t write_cb;       /**< Write callback for Package (0) and Package URI (1) */
    wb_lwm2m_execute_cb_t update_cb;    /**< Execute callback for Update (res 2) */
} wb_lwm2m_firmware_info_t;

/**
 * @brief Create a Firmware Update Object (/5) definition
 *
 * Resources: Package (0), Package URI (1), Update (2), State (3),
 *            Update Result (5), Pkg Name (6), Pkg Version (7),
 *            Protocol Support (8), Delivery Method (9)
 *
 * @param info     Config with callbacks
 * @param user_ctx User context
 * @return Heap-allocated object def, or NULL. Caller owns the memory.
 */
wb_lwm2m_object_def_t *wb_lwm2m_create_firmware_object(const wb_lwm2m_firmware_info_t *info,
                                                       void *user_ctx);

/* ────────────────────────────────────────────────────────
 *  Location Object (/6)
 * ──────────────────────────────────────────────────────── */

/**
 * @brief Configuration for the Location Object (/6)
 *
 * Provides GPS/GNSS position data. The user's read callback must supply
 * at minimum latitude, longitude, and timestamp (mandatory resources).
 */

typedef struct {
    wb_lwm2m_read_cb_t read_cb;         /**< Read callback for location data (mandatory) */
} wb_lwm2m_location_info_t;

/**
 * @brief Create a Location Object (/6) definition
 *
 * Resources: Latitude (0, mandatory), Longitude (1, mandatory),
 *            Altitude (2), Radius (3), Velocity (4),
 *            Timestamp (5, mandatory), Speed (6)
 *
 * @param info     Config with read callback
 * @param user_ctx User context
 * @return Heap-allocated object def, or NULL. Caller owns the memory.
 */
wb_lwm2m_object_def_t *wb_lwm2m_create_location_object(const wb_lwm2m_location_info_t *info,
                                                        void *user_ctx);

/* ────────────────────────────────────────────────────────
 *  Connectivity Statistics Object (/7)
 * ──────────────────────────────────────────────────────── */

/**
 * @brief Configuration for the Connectivity Statistics Object (/7)
 *
 * Tracks network traffic statistics. The read callback provides counters,
 * while start/stop execute callbacks control the measurement period.
 */

typedef struct {
    wb_lwm2m_read_cb_t read_cb;         /**< Read callback for statistics */
    wb_lwm2m_execute_cb_t start_cb;     /**< Start (res 6) execute callback */
    wb_lwm2m_execute_cb_t stop_cb;      /**< Stop (res 7) execute callback */
} wb_lwm2m_conn_stats_info_t;

/**
 * @brief Create a Connectivity Statistics Object (/7) definition
 *
 * Resources: SMS Tx Counter (0), SMS Rx Counter (1),
 *            Tx Data (2), Rx Data (3),
 *            Max Message Size (4), Average Message Size (5),
 *            Start/Reset (6), Stop (7), Collection Period (8)
 *
 * @param info     Config with callbacks
 * @param user_ctx User context
 * @return Heap-allocated object def, or NULL. Caller owns the memory.
 */
wb_lwm2m_object_def_t *wb_lwm2m_create_conn_stats_object(const wb_lwm2m_conn_stats_info_t *info,
                                                          void *user_ctx);

/* ── Utility ── */

/**
 * @brief Free a standard object definition previously created by wb_lwm2m_create_*
 *
 * Safe to call with NULL (no-op). Must be called after the client is destroyed,
 * as the client holds a reference to the object definition.
 *
 * @param obj  Object definition to free (may be NULL)
 */
void wb_lwm2m_destroy_std_object(wb_lwm2m_object_def_t *obj);

#ifdef __cplusplus
}
#endif

/** @} */ /* end of wb_lwm2m_std_objects group */

#endif /* __WB_LWM2M_STD_OBJECTS_H__ */
