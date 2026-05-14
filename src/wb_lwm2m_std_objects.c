/*
 * WhirlingBits LWM2M — Standard Object Implementations
 *
 * Factory functions for all OMA standard LWM2M objects (0-7).
 * Each create function returns a heap-allocated wb_lwm2m_object_def_t
 * with fully wired resource definitions.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wb_lwm2m_std_objects.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "WB_LWM2M_OBJ";

/* ────────────────────────────────────────────────────────
 *  Helpers
 * ──────────────────────────────────────────────────────── */

/**
 * Allocate a zeroed wb_lwm2m_object_def_t on the heap.
 */
static wb_lwm2m_object_def_t *alloc_object(void)
{
    wb_lwm2m_object_def_t *obj = calloc(1, sizeof(wb_lwm2m_object_def_t));
    if (obj == NULL) {
        ESP_LOGE(TAG, "Failed to allocate object definition");
    }
    return obj;
}

/**
 * Append a resource definition. Returns false if the resources array is full.
 */
static bool add_resource(wb_lwm2m_object_def_t *obj, const wb_lwm2m_resource_def_t *res)
{
    if (obj->resource_count >= WB_LWM2M_MAX_RESOURCES) {
        ESP_LOGW(TAG, "Object /%u: max resources reached, cannot add res %u",
                 obj->id, res->id);
        return false;
    }
    obj->resources[obj->resource_count++] = *res;
    return true;
}

/* ────────────────────────────────────────────────────────
 *  Security Object (/0)
 *
 *  Resource   Type      Operations
 *  0          String    R   — Server URI
 *  1          Boolean   R   — Bootstrap-Server
 *  2          Integer   R   — Security Mode
 *  3          Opaque    R   — Public Key or Identity
 *  4          Opaque    R   — Server Public Key
 *  5          Opaque    R   — Secret Key
 *  10         Integer   R   — Short Server ID
 * ──────────────────────────────────────────────────────── */

/** Internal context stored in user_ctx */
typedef struct {
    wb_lwm2m_security_info_t info;
} security_ctx_t;

static esp_err_t security_read_cb(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                                  wb_lwm2m_value_t *value, void *user_ctx)
{
    const security_ctx_t *ctx = (const security_ctx_t *)user_ctx;
    if (ctx == NULL) return ESP_ERR_INVALID_STATE;

    switch (res_id) {
    case WB_LWM2M_RES_SEC_SERVER_URI:
        *value = WB_LWM2M_VALUE_STRING(ctx->info.server_uri ? ctx->info.server_uri : "");
        return ESP_OK;
    case WB_LWM2M_RES_SEC_BOOTSTRAP:
        *value = WB_LWM2M_VALUE_BOOL(ctx->info.is_bootstrap);
        return ESP_OK;
    case WB_LWM2M_RES_SEC_MODE:
        *value = WB_LWM2M_VALUE_INT(ctx->info.mode);
        return ESP_OK;
    case WB_LWM2M_RES_SEC_PUB_KEY_IDENTITY:
        if (ctx->info.pub_key_identity && ctx->info.pub_key_identity_len > 0) {
            *value = WB_LWM2M_VALUE_OPAQUE(ctx->info.pub_key_identity, ctx->info.pub_key_identity_len);
        } else {
            *value = WB_LWM2M_VALUE_OPAQUE(NULL, 0);
        }
        return ESP_OK;
    case WB_LWM2M_RES_SEC_SERVER_PUB_KEY:
        if (ctx->info.server_pub_key && ctx->info.server_pub_key_len > 0) {
            *value = WB_LWM2M_VALUE_OPAQUE(ctx->info.server_pub_key, ctx->info.server_pub_key_len);
        } else {
            *value = WB_LWM2M_VALUE_OPAQUE(NULL, 0);
        }
        return ESP_OK;
    case WB_LWM2M_RES_SEC_SECRET_KEY:
        if (ctx->info.secret_key && ctx->info.secret_key_len > 0) {
            *value = WB_LWM2M_VALUE_OPAQUE(ctx->info.secret_key, ctx->info.secret_key_len);
        } else {
            *value = WB_LWM2M_VALUE_OPAQUE(NULL, 0);
        }
        return ESP_OK;
    case WB_LWM2M_RES_SEC_SHORT_SERVER_ID:
        *value = WB_LWM2M_VALUE_INT(ctx->info.short_server_id);
        return ESP_OK;
    default:
        return ESP_ERR_NOT_FOUND;
    }
}

wb_lwm2m_object_def_t *wb_lwm2m_create_security_object(const wb_lwm2m_security_info_t *info,
                                                        uint16_t inst_id)
{
    if (info == NULL) return NULL;

    wb_lwm2m_object_def_t *obj = alloc_object();
    if (obj == NULL) return NULL;

    security_ctx_t *ctx = calloc(1, sizeof(security_ctx_t));
    if (ctx == NULL) { free(obj); return NULL; }
    ctx->info = *info;

    obj->id = WB_LWM2M_OBJ_SECURITY;
    obj->name = "LWM2M Security";
    obj->multiple_instances = true;
    obj->instances[0].id = inst_id;
    obj->instance_count = 1;
    obj->user_ctx = ctx;

    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 0,  .type = WB_LWM2M_RES_TYPE_STRING,  .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = security_read_cb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 1,  .type = WB_LWM2M_RES_TYPE_BOOLEAN, .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = security_read_cb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 2,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = security_read_cb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 3,  .type = WB_LWM2M_RES_TYPE_OPAQUE,  .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = security_read_cb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 4,  .type = WB_LWM2M_RES_TYPE_OPAQUE,  .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = security_read_cb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 5,  .type = WB_LWM2M_RES_TYPE_OPAQUE,  .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = security_read_cb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 10, .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .read_cb = security_read_cb });

    return obj;
}

/* ────────────────────────────────────────────────────────
 *  Server Object (/1)
 *
 *  Resource   Type      Operations
 *  0          Integer   R   — Short Server ID
 *  1          Integer   RW  — Lifetime
 *  2          Integer   RW  — Default Min Period
 *  3          Integer   RW  — Default Max Period
 *  4          Execute   E   — Disable
 *  5          Integer   RW  — Disable Timeout
 *  6          Boolean   RW  — Notification Storing
 *  7          String    RW  — Binding
 *  8          Execute   E   — Registration Update Trigger
 * ──────────────────────────────────────────────────────── */

typedef struct {
    wb_lwm2m_server_info_t info;
} server_ctx_t;

static esp_err_t server_read_cb(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                                wb_lwm2m_value_t *value, void *user_ctx)
{
    const server_ctx_t *ctx = (const server_ctx_t *)user_ctx;
    if (ctx == NULL) return ESP_ERR_INVALID_STATE;

    switch (res_id) {
    case WB_LWM2M_RES_SRV_SHORT_ID:
        *value = WB_LWM2M_VALUE_INT(ctx->info.short_server_id);
        return ESP_OK;
    case WB_LWM2M_RES_SRV_LIFETIME:
        *value = WB_LWM2M_VALUE_INT(ctx->info.lifetime);
        return ESP_OK;
    case WB_LWM2M_RES_SRV_MIN_PERIOD:
        *value = WB_LWM2M_VALUE_INT(ctx->info.default_min_period);
        return ESP_OK;
    case WB_LWM2M_RES_SRV_MAX_PERIOD:
        *value = WB_LWM2M_VALUE_INT(ctx->info.default_max_period);
        return ESP_OK;
    case WB_LWM2M_RES_SRV_DISABLE_TIMEOUT:
        *value = WB_LWM2M_VALUE_INT(ctx->info.disable_timeout);
        return ESP_OK;
    case WB_LWM2M_RES_SRV_NOTIFICATION_STORING:
        *value = WB_LWM2M_VALUE_BOOL(ctx->info.notification_storing);
        return ESP_OK;
    case WB_LWM2M_RES_SRV_BINDING:
        *value = WB_LWM2M_VALUE_STRING(ctx->info.binding ? ctx->info.binding : "U");
        return ESP_OK;
    default:
        return ESP_ERR_NOT_FOUND;
    }
}

static esp_err_t server_write_cb(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                                 const wb_lwm2m_value_t *value, void *user_ctx)
{
    server_ctx_t *ctx = (server_ctx_t *)user_ctx;
    if (ctx == NULL) return ESP_ERR_INVALID_STATE;

    switch (res_id) {
    case WB_LWM2M_RES_SRV_LIFETIME:
        ctx->info.lifetime = (uint32_t)wb_lwm2m_value_get_int(value);
        return ESP_OK;
    case WB_LWM2M_RES_SRV_MIN_PERIOD:
        ctx->info.default_min_period = (uint32_t)wb_lwm2m_value_get_int(value);
        return ESP_OK;
    case WB_LWM2M_RES_SRV_MAX_PERIOD:
        ctx->info.default_max_period = (uint32_t)wb_lwm2m_value_get_int(value);
        return ESP_OK;
    case WB_LWM2M_RES_SRV_DISABLE_TIMEOUT:
        ctx->info.disable_timeout = (uint32_t)wb_lwm2m_value_get_int(value);
        return ESP_OK;
    case WB_LWM2M_RES_SRV_NOTIFICATION_STORING:
        ctx->info.notification_storing = value->bool_val;
        return ESP_OK;
    default:
        return ESP_ERR_NOT_FOUND;
    }
}

wb_lwm2m_object_def_t *wb_lwm2m_create_server_object(const wb_lwm2m_server_info_t *info,
                                                      uint16_t inst_id,
                                                      wb_lwm2m_execute_cb_t reg_update_cb,
                                                      wb_lwm2m_execute_cb_t disable_cb,
                                                      void *user_ctx)
{
    if (info == NULL) return NULL;

    wb_lwm2m_object_def_t *obj = alloc_object();
    if (obj == NULL) return NULL;

    server_ctx_t *ctx = calloc(1, sizeof(server_ctx_t));
    if (ctx == NULL) { free(obj); return NULL; }
    ctx->info = *info;

    obj->id = WB_LWM2M_OBJ_SERVER;
    obj->name = "LWM2M Server";
    obj->multiple_instances = true;
    obj->instances[0].id = inst_id;
    obj->instance_count = 1;
    obj->user_ctx = ctx;

    /* Short Server ID — R, mandatory */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 0,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = server_read_cb });
    /* Lifetime — RW, mandatory */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 1,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_RW,
        .mandatory = true,  .read_cb = server_read_cb, .write_cb = server_write_cb });
    /* Default Min Period — RW */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 2,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_RW,
        .mandatory = false, .read_cb = server_read_cb, .write_cb = server_write_cb });
    /* Default Max Period — RW */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 3,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_RW,
        .mandatory = false, .read_cb = server_read_cb, .write_cb = server_write_cb });
    /* Disable — E */
    if (disable_cb) {
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 4,  .type = WB_LWM2M_RES_TYPE_NONE,  .operations = WB_LWM2M_OP_EXECUTE,
            .mandatory = false, .execute_cb = disable_cb });
    }
    /* Disable Timeout — RW */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 5,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_RW,
        .mandatory = false, .read_cb = server_read_cb, .write_cb = server_write_cb });
    /* Notification Storing — RW, mandatory */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 6,  .type = WB_LWM2M_RES_TYPE_BOOLEAN, .operations = WB_LWM2M_OP_RW,
        .mandatory = true,  .read_cb = server_read_cb, .write_cb = server_write_cb });
    /* Binding — RW, mandatory */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 7,  .type = WB_LWM2M_RES_TYPE_STRING,  .operations = WB_LWM2M_OP_RW,
        .mandatory = true,  .read_cb = server_read_cb });
    /* Registration Update Trigger — E, mandatory */
    if (reg_update_cb) {
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 8,  .type = WB_LWM2M_RES_TYPE_NONE,  .operations = WB_LWM2M_OP_EXECUTE,
            .mandatory = true, .execute_cb = reg_update_cb });
    }

    return obj;
}

/* ────────────────────────────────────────────────────────
 *  Device Object (/3)
 *
 *  Res  Type       Ops   M   Description
 *  0    String     R     Y   Manufacturer
 *  1    String     R     Y   Model Number
 *  2    String     R     Y   Serial Number
 *  3    String     R         Firmware Version
 *  4    Execute    E     Y   Reboot
 *  5    Execute    E         Factory Reset
 *  6    Integer[]  R         Available Power Sources
 *  7    Integer[]  R         Power Source Voltage
 *  8    Integer[]  R         Power Source Current
 *  9    Integer    R         Battery Level
 *  10   Integer    R         Memory Free
 *  11   Integer[]  R     Y   Error Code
 *  12   Execute    E         Reset Error Code
 *  13   Time       RW        Current Time
 *  14   String     R         UTC Offset
 *  15   String     R         Timezone
 *  16   String     R     Y   Supported Binding
 *  17   String     R         Device Type
 *  18   String     R         Hardware Version
 *  19   String     R         Software Version
 *  20   Integer    R         Battery Status
 *  21   Integer    R         Memory Total
 * ──────────────────────────────────────────────────────── */

typedef struct {
    wb_lwm2m_device_info_t info;
    int64_t current_time;   /* writable current time offset */
} device_ctx_t;

static esp_err_t device_default_read_cb(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                                        wb_lwm2m_value_t *value, void *user_ctx)
{
    const device_ctx_t *ctx = (const device_ctx_t *)user_ctx;
    if (ctx == NULL) return ESP_ERR_INVALID_STATE;

    switch (res_id) {
    /* Static strings */
    case WB_LWM2M_RES_DEVICE_MANUFACTURER:
        *value = WB_LWM2M_VALUE_STRING(ctx->info.manufacturer ? ctx->info.manufacturer : "");
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_MODEL_NUMBER:
        *value = WB_LWM2M_VALUE_STRING(ctx->info.model_number ? ctx->info.model_number : "");
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_SERIAL_NUMBER:
        *value = WB_LWM2M_VALUE_STRING(ctx->info.serial_number ? ctx->info.serial_number : "");
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_FW_VERSION:
        *value = WB_LWM2M_VALUE_STRING(ctx->info.firmware_version ? ctx->info.firmware_version : "");
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_UTC_OFFSET:
        *value = WB_LWM2M_VALUE_STRING(ctx->info.utc_offset ? ctx->info.utc_offset : "");
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_TIMEZONE:
        *value = WB_LWM2M_VALUE_STRING(ctx->info.timezone ? ctx->info.timezone : "");
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_BINDING:
        *value = WB_LWM2M_VALUE_STRING(ctx->info.binding ? ctx->info.binding : "U");
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_TYPE:
        *value = WB_LWM2M_VALUE_STRING(ctx->info.device_type ? ctx->info.device_type : "");
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_HW_VERSION:
        *value = WB_LWM2M_VALUE_STRING(ctx->info.hardware_version ? ctx->info.hardware_version : "");
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_SW_VERSION:
        *value = WB_LWM2M_VALUE_STRING(ctx->info.software_version ? ctx->info.software_version : "");
        return ESP_OK;

    /* Dynamic resources */
    case WB_LWM2M_RES_DEVICE_BATTERY_LEVEL:
        *value = WB_LWM2M_VALUE_INT(100);  /* Override via custom read_cb */
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_MEMORY_FREE:
        *value = WB_LWM2M_VALUE_INT((int64_t)esp_get_free_heap_size() / 1024);
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_ERROR_CODE:
        *value = WB_LWM2M_VALUE_INT(0);  /* 0 = No Error */
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_CURRENT_TIME:
        *value = WB_LWM2M_VALUE_TIME((int64_t)time(NULL));
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_BATTERY_STATUS:
        *value = WB_LWM2M_VALUE_INT(0);  /* 0 = Normal */
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_MEMORY_TOTAL:
        *value = WB_LWM2M_VALUE_INT(320);  /* Typical ESP32 SRAM in KB — override via custom read_cb */
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_POWER_SOURCES:
        *value = WB_LWM2M_VALUE_INT(0);  /* 0 = DC power */
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_POWER_VOLTAGE:
        *value = WB_LWM2M_VALUE_INT(3300);  /* 3.3V in mV */
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_POWER_CURRENT:
        *value = WB_LWM2M_VALUE_INT(0);
        return ESP_OK;
    default:
        return ESP_ERR_NOT_FOUND;
    }
}

static esp_err_t device_read_dispatch(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                                      wb_lwm2m_value_t *value, void *user_ctx)
{
    const device_ctx_t *ctx = (const device_ctx_t *)user_ctx;
    if (ctx == NULL) return ESP_ERR_INVALID_STATE;

    /* If user provided a custom read callback, try it first */
    if (ctx->info.read_cb != NULL) {
        esp_err_t err = ctx->info.read_cb(obj_id, inst_id, res_id, value, user_ctx);
        if (err == ESP_OK) return ESP_OK;
        /* Fall through to default if the custom cb returns NOT_FOUND */
        if (err != ESP_ERR_NOT_FOUND) return err;
    }
    return device_default_read_cb(obj_id, inst_id, res_id, value, user_ctx);
}

wb_lwm2m_object_def_t *wb_lwm2m_create_device_object(const wb_lwm2m_device_info_t *info,
                                                      void *user_ctx)
{
    if (info == NULL) return NULL;

    wb_lwm2m_object_def_t *obj = alloc_object();
    if (obj == NULL) return NULL;

    device_ctx_t *ctx = calloc(1, sizeof(device_ctx_t));
    if (ctx == NULL) { free(obj); return NULL; }
    ctx->info = *info;

    obj->id = WB_LWM2M_OBJ_DEVICE;
    obj->name = "Device";
    obj->multiple_instances = false;
    obj->instances[0].id = 0;
    obj->instance_count = 1;
    obj->user_ctx = ctx;

    wb_lwm2m_read_cb_t rcb = device_read_dispatch;

    /* Mandatory resources */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 0,  .type = WB_LWM2M_RES_TYPE_STRING,  .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 1,  .type = WB_LWM2M_RES_TYPE_STRING,  .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 2,  .type = WB_LWM2M_RES_TYPE_STRING,  .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = rcb });

    /* Firmware Version */
    if (info->firmware_version) {
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 3,  .type = WB_LWM2M_RES_TYPE_STRING,  .operations = WB_LWM2M_OP_READ,
            .mandatory = false, .read_cb = rcb });
    }

    /* Reboot — mandatory execute */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 4,  .type = WB_LWM2M_RES_TYPE_NONE, .operations = WB_LWM2M_OP_EXECUTE,
        .mandatory = true,  .execute_cb = info->reboot_cb });

    /* Factory Reset */
    if (info->factory_reset_cb) {
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 5,  .type = WB_LWM2M_RES_TYPE_NONE, .operations = WB_LWM2M_OP_EXECUTE,
            .mandatory = false, .execute_cb = info->factory_reset_cb });
    }

    /* Power Sources (multiple) */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 6,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .multiple = true, .read_cb = rcb });
    /* Power Source Voltage (multiple) */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 7,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .multiple = true, .read_cb = rcb });
    /* Power Source Current (multiple) */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 8,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .multiple = true, .read_cb = rcb });

    /* Battery Level */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 9,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .read_cb = rcb });
    /* Memory Free */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 10, .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .read_cb = rcb });
    /* Error Code — mandatory, multiple */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 11, .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = true, .multiple = true, .read_cb = rcb });

    /* Reset Error Code */
    if (info->reset_error_cb) {
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 12, .type = WB_LWM2M_RES_TYPE_NONE, .operations = WB_LWM2M_OP_EXECUTE,
            .mandatory = false, .execute_cb = info->reset_error_cb });
    }

    /* Current Time */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 13, .type = WB_LWM2M_RES_TYPE_TIME, .operations = WB_LWM2M_OP_RW,
        .mandatory = false, .read_cb = rcb });

    /* UTC Offset */
    if (info->utc_offset) {
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 14, .type = WB_LWM2M_RES_TYPE_STRING, .operations = WB_LWM2M_OP_RW,
            .mandatory = false, .read_cb = rcb });
    }

    /* Timezone */
    if (info->timezone) {
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 15, .type = WB_LWM2M_RES_TYPE_STRING, .operations = WB_LWM2M_OP_RW,
            .mandatory = false, .read_cb = rcb });
    }

    /* Binding — mandatory */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 16, .type = WB_LWM2M_RES_TYPE_STRING, .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = rcb });

    /* Device Type */
    if (info->device_type) {
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 17, .type = WB_LWM2M_RES_TYPE_STRING, .operations = WB_LWM2M_OP_READ,
            .mandatory = false, .read_cb = rcb });
    }

    /* Hardware Version */
    if (info->hardware_version) {
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 18, .type = WB_LWM2M_RES_TYPE_STRING, .operations = WB_LWM2M_OP_READ,
            .mandatory = false, .read_cb = rcb });
    }

    /* Software Version */
    if (info->software_version) {
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 19, .type = WB_LWM2M_RES_TYPE_STRING, .operations = WB_LWM2M_OP_READ,
            .mandatory = false, .read_cb = rcb });
    }

    /* Battery Status */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 20, .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .read_cb = rcb });

    /* Memory Total */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 21, .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .read_cb = rcb });

    return obj;
}

/* ────────────────────────────────────────────────────────
 *  Connectivity Monitoring Object (/4)
 *
 *  Res  Type       Ops  M  Description
 *  0    Integer    R    Y  Network Bearer
 *  1    Integer[]  R    Y  Available Network Bearer
 *  2    Integer    R    Y  Radio Signal Strength
 *  3    Integer    R       Link Quality
 *  4    String[]   R    Y  IP Addresses
 *  5    String[]   R       Router IP Addresses
 *  6    Integer    R       Link Utilization
 *  7    String[]   R       APN
 *  8    Integer    R       Cell ID
 *  9    Integer    R       SMNC
 *  10   Integer    R       SMCC
 * ──────────────────────────────────────────────────────── */

wb_lwm2m_object_def_t *wb_lwm2m_create_conn_mon_object(const wb_lwm2m_conn_mon_info_t *info,
                                                        void *user_ctx)
{
    if (info == NULL || info->read_cb == NULL) return NULL;

    wb_lwm2m_object_def_t *obj = alloc_object();
    if (obj == NULL) return NULL;

    obj->id = WB_LWM2M_OBJ_CONN_MONITORING;
    obj->name = "Connectivity Monitoring";
    obj->multiple_instances = false;
    obj->instances[0].id = 0;
    obj->instance_count = 1;
    obj->user_ctx = user_ctx;

    wb_lwm2m_read_cb_t rcb = info->read_cb;

    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 0,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 1,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .multiple = true, .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 2,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 3,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 4,  .type = WB_LWM2M_RES_TYPE_STRING,  .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .multiple = true, .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 5,  .type = WB_LWM2M_RES_TYPE_STRING,  .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .multiple = true, .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 6,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 7,  .type = WB_LWM2M_RES_TYPE_STRING,  .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .multiple = true, .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 8,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 9,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 10, .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .read_cb = rcb });

    return obj;
}

/* ────────────────────────────────────────────────────────
 *  Firmware Update Object (/5)
 *
 *  Res  Type       Ops  M  Description
 *  0    Opaque     W       Package
 *  1    String     W       Package URI
 *  2    Execute    E       Update
 *  3    Integer    R    Y  State
 *  5    Integer    R    Y  Update Result
 *  6    String     R       Package Name
 *  7    String     R       Package Version
 *  8    Integer[]  R       Firmware Update Protocol Support
 *  9    Integer    R       Firmware Update Delivery Method
 * ──────────────────────────────────────────────────────── */

wb_lwm2m_object_def_t *wb_lwm2m_create_firmware_object(const wb_lwm2m_firmware_info_t *info,
                                                       void *user_ctx)
{
    if (info == NULL) return NULL;

    wb_lwm2m_object_def_t *obj = alloc_object();
    if (obj == NULL) return NULL;

    obj->id = WB_LWM2M_OBJ_FIRMWARE_UPDATE;
    obj->name = "Firmware Update";
    obj->multiple_instances = false;
    obj->instances[0].id = 0;
    obj->instance_count = 1;
    obj->user_ctx = user_ctx;

    /* Package — W (opaque blob) */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 0,  .type = WB_LWM2M_RES_TYPE_OPAQUE, .operations = WB_LWM2M_OP_WRITE,
        .mandatory = false, .write_cb = info->write_cb });
    /* Package URI — W (string) */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 1,  .type = WB_LWM2M_RES_TYPE_STRING, .operations = WB_LWM2M_OP_WRITE,
        .mandatory = false, .write_cb = info->write_cb });
    /* Update — E */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 2,  .type = WB_LWM2M_RES_TYPE_NONE,   .operations = WB_LWM2M_OP_EXECUTE,
        .mandatory = false, .execute_cb = info->update_cb });
    /* State — R, mandatory */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 3,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = info->read_cb });
    /* Update Result — R, mandatory */
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 5,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = info->read_cb });
    /* Package Name — R */
    if (info->read_cb) {
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 6,  .type = WB_LWM2M_RES_TYPE_STRING,  .operations = WB_LWM2M_OP_READ,
            .mandatory = false, .read_cb = info->read_cb });
        /* Package Version — R */
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 7,  .type = WB_LWM2M_RES_TYPE_STRING,  .operations = WB_LWM2M_OP_READ,
            .mandatory = false, .read_cb = info->read_cb });
        /* Protocol Support — R, multiple */
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 8,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
            .mandatory = false, .multiple = true, .read_cb = info->read_cb });
        /* Delivery Method — R */
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 9,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
            .mandatory = false, .read_cb = info->read_cb });
    }

    return obj;
}

/* ────────────────────────────────────────────────────────
 *  Location Object (/6)
 *
 *  Res  Type      Ops  M  Description
 *  0    Float     R    Y  Latitude
 *  1    Float     R    Y  Longitude
 *  2    Float     R       Altitude
 *  3    Float     R       Radius
 *  4    Opaque    R       Velocity
 *  5    Time      R    Y  Timestamp
 *  6    Float     R       Speed
 * ──────────────────────────────────────────────────────── */

wb_lwm2m_object_def_t *wb_lwm2m_create_location_object(const wb_lwm2m_location_info_t *info,
                                                        void *user_ctx)
{
    if (info == NULL || info->read_cb == NULL) return NULL;

    wb_lwm2m_object_def_t *obj = alloc_object();
    if (obj == NULL) return NULL;

    obj->id = WB_LWM2M_OBJ_LOCATION;
    obj->name = "Location";
    obj->multiple_instances = false;
    obj->instances[0].id = 0;
    obj->instance_count = 1;
    obj->user_ctx = user_ctx;

    wb_lwm2m_read_cb_t rcb = info->read_cb;

    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 0,  .type = WB_LWM2M_RES_TYPE_FLOAT,  .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 1,  .type = WB_LWM2M_RES_TYPE_FLOAT,  .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 2,  .type = WB_LWM2M_RES_TYPE_FLOAT,  .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 3,  .type = WB_LWM2M_RES_TYPE_FLOAT,  .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 4,  .type = WB_LWM2M_RES_TYPE_OPAQUE, .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 5,  .type = WB_LWM2M_RES_TYPE_TIME,   .operations = WB_LWM2M_OP_READ,
        .mandatory = true,  .read_cb = rcb });
    add_resource(obj, &(wb_lwm2m_resource_def_t){
        .id = 6,  .type = WB_LWM2M_RES_TYPE_FLOAT,  .operations = WB_LWM2M_OP_READ,
        .mandatory = false, .read_cb = rcb });

    return obj;
}

/* ────────────────────────────────────────────────────────
 *  Connectivity Statistics Object (/7)
 *
 *  Res  Type      Ops  M  Description
 *  0    Integer   R       SMS Tx Counter
 *  1    Integer   R       SMS Rx Counter
 *  2    Integer   R       Tx Data
 *  3    Integer   R       Rx Data
 *  4    Integer   R       Max Message Size
 *  5    Integer   R       Average Message Size
 *  6    Execute   E       Start/Reset
 *  7    Execute   E       Stop
 *  8    Integer   RW      Collection Period
 * ──────────────────────────────────────────────────────── */

wb_lwm2m_object_def_t *wb_lwm2m_create_conn_stats_object(const wb_lwm2m_conn_stats_info_t *info,
                                                          void *user_ctx)
{
    if (info == NULL) return NULL;

    wb_lwm2m_object_def_t *obj = alloc_object();
    if (obj == NULL) return NULL;

    obj->id = WB_LWM2M_OBJ_CONN_STATISTICS;
    obj->name = "Connectivity Statistics";
    obj->multiple_instances = false;
    obj->instances[0].id = 0;
    obj->instance_count = 1;
    obj->user_ctx = user_ctx;

    wb_lwm2m_read_cb_t rcb = info->read_cb;

    if (rcb) {
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 0,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
            .mandatory = false, .read_cb = rcb });
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 1,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
            .mandatory = false, .read_cb = rcb });
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 2,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
            .mandatory = false, .read_cb = rcb });
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 3,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
            .mandatory = false, .read_cb = rcb });
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 4,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
            .mandatory = false, .read_cb = rcb });
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 5,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_READ,
            .mandatory = false, .read_cb = rcb });
    }
    if (info->start_cb) {
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 6,  .type = WB_LWM2M_RES_TYPE_NONE, .operations = WB_LWM2M_OP_EXECUTE,
            .mandatory = false, .execute_cb = info->start_cb });
    }
    if (info->stop_cb) {
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 7,  .type = WB_LWM2M_RES_TYPE_NONE, .operations = WB_LWM2M_OP_EXECUTE,
            .mandatory = false, .execute_cb = info->stop_cb });
    }
    if (rcb) {
        add_resource(obj, &(wb_lwm2m_resource_def_t){
            .id = 8,  .type = WB_LWM2M_RES_TYPE_INTEGER, .operations = WB_LWM2M_OP_RW,
            .mandatory = false, .read_cb = rcb });
    }

    return obj;
}

/* ────────────────────────────────────────────────────────
 *  Destroy
 * ──────────────────────────────────────────────────────── */

void wb_lwm2m_destroy_std_object(wb_lwm2m_object_def_t *obj)
{
    if (obj == NULL) return;

    /* Free the internal context for objects that allocate one */
    if (obj->user_ctx != NULL &&
        (obj->id == WB_LWM2M_OBJ_SECURITY ||
         obj->id == WB_LWM2M_OBJ_SERVER ||
         obj->id == WB_LWM2M_OBJ_DEVICE)) {
        free(obj->user_ctx);
    }
    free(obj);
}
