/*
 * WhirlingBits LWM2M Client — Encoding helpers
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wb_lwm2m_encode.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

/* ── Base64 encoder for Opaque values (SenML "vd") ── */

static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_encode(const uint8_t *src, size_t src_len, char *dst, size_t dst_size)
{
    size_t needed = 4 * ((src_len + 2) / 3) + 1;
    if (dst_size < needed) return -1;

    size_t di = 0;
    for (size_t i = 0; i < src_len; i += 3) {
        uint32_t n = ((uint32_t)src[i]) << 16;
        if (i + 1 < src_len) n |= ((uint32_t)src[i + 1]) << 8;
        if (i + 2 < src_len) n |= ((uint32_t)src[i + 2]);

        dst[di++] = b64_table[(n >> 18) & 0x3F];
        dst[di++] = b64_table[(n >> 12) & 0x3F];
        dst[di++] = (i + 1 < src_len) ? b64_table[(n >> 6) & 0x3F] : '=';
        dst[di++] = (i + 2 < src_len) ? b64_table[n & 0x3F] : '=';
    }
    dst[di] = '\0';
    return (int)di;
}

/* ── Link-Format ── */

int wb_lwm2m_encode_link_format(const wb_lwm2m_object_def_t *const *objects,
                                size_t object_count,
                                char *buf, size_t buf_size)
{
    if (objects == NULL || buf == NULL || buf_size == 0) {
        return -1;
    }

    size_t offset = 0;
    bool first = true;

    for (size_t i = 0; i < object_count; i++) {
        const wb_lwm2m_object_def_t *obj = objects[i];
        if (obj == NULL) {
            continue;
        }

        for (size_t j = 0; j < obj->instance_count; j++) {
            int written;
            if (first) {
                written = snprintf(buf + offset, buf_size - offset,
                                   "</%" PRIu16 "/%" PRIu16 ">",
                                   obj->id, obj->instances[j].id);
                first = false;
            } else {
                written = snprintf(buf + offset, buf_size - offset,
                                   ",</%" PRIu16 "/%" PRIu16 ">",
                                   obj->id, obj->instances[j].id);
            }
            if (written < 0 || (size_t)written >= buf_size - offset) {
                return -1;
            }
            offset += (size_t)written;
        }
    }

    return (int)offset;
}

/* ── SenML-JSON (single resource) ── */

int wb_lwm2m_encode_senml_json(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                               const wb_lwm2m_value_t *value,
                               char *buf, size_t buf_size)
{
    if (value == NULL || buf == NULL || buf_size == 0) {
        return -1;
    }

    int written = -1;

    switch (value->type) {
    case WB_LWM2M_RES_TYPE_STRING:
    case WB_LWM2M_RES_TYPE_CORELNK:
        written = snprintf(buf, buf_size,
                           "[{\"bn\":\"/%" PRIu16 "/%" PRIu16 "/\",\"n\":\"%" PRIu16 "\",\"vs\":\"%.*s\"}]",
                           obj_id, inst_id, res_id,
                           (int)value->str_val.len, value->str_val.str);
        break;

    case WB_LWM2M_RES_TYPE_INTEGER:
        written = snprintf(buf, buf_size,
                           "[{\"bn\":\"/%" PRIu16 "/%" PRIu16 "/\",\"n\":\"%" PRIu16 "\",\"v\":%" PRId64 "}]",
                           obj_id, inst_id, res_id, value->int_val);
        break;

    case WB_LWM2M_RES_TYPE_UINTEGER:
        written = snprintf(buf, buf_size,
                           "[{\"bn\":\"/%" PRIu16 "/%" PRIu16 "/\",\"n\":\"%" PRIu16 "\",\"v\":%" PRIu64 "}]",
                           obj_id, inst_id, res_id, value->uint_val);
        break;

    case WB_LWM2M_RES_TYPE_TIME:
        written = snprintf(buf, buf_size,
                           "[{\"bn\":\"/%" PRIu16 "/%" PRIu16 "/\",\"n\":\"%" PRIu16 "\",\"v\":%" PRId64 "}]",
                           obj_id, inst_id, res_id, value->time_val);
        break;

    case WB_LWM2M_RES_TYPE_FLOAT:
        written = snprintf(buf, buf_size,
                           "[{\"bn\":\"/%" PRIu16 "/%" PRIu16 "/\",\"n\":\"%" PRIu16 "\",\"v\":%g}]",
                           obj_id, inst_id, res_id, value->float_val);
        break;

    case WB_LWM2M_RES_TYPE_BOOLEAN:
        written = snprintf(buf, buf_size,
                           "[{\"bn\":\"/%" PRIu16 "/%" PRIu16 "/\",\"n\":\"%" PRIu16 "\",\"vb\":%s}]",
                           obj_id, inst_id, res_id,
                           value->bool_val ? "true" : "false");
        break;

    case WB_LWM2M_RES_TYPE_OPAQUE: {
        char b64[344];  /* enough for 256 bytes of binary input */
        int b64_len = base64_encode(value->opaque_val.data, value->opaque_val.len,
                                    b64, sizeof(b64));
        if (b64_len < 0) return -1;
        written = snprintf(buf, buf_size,
                           "[{\"bn\":\"/%" PRIu16 "/%" PRIu16 "/\",\"n\":\"%" PRIu16 "\",\"vd\":\"%s\"}]",
                           obj_id, inst_id, res_id, b64);
        break;
    }

    case WB_LWM2M_RES_TYPE_OBJLINK:
        written = snprintf(buf, buf_size,
                           "[{\"bn\":\"/%" PRIu16 "/%" PRIu16 "/\",\"n\":\"%" PRIu16 "\",\"vlo\":\"%" PRIu16 ":%" PRIu16 "\"}]",
                           obj_id, inst_id, res_id,
                           value->objlink_val.obj_id, value->objlink_val.inst_id);
        break;

    case WB_LWM2M_RES_TYPE_NONE:
    default:
        return -1;
    }

    if (written < 0 || (size_t)written >= buf_size) {
        return -1;
    }
    return written;
}

/* ── SenML-JSON (full instance — all readable resources) ── */

/**
 * Write one SenML value field into buf+offset.
 * Returns bytes written or -1 on error.
 */
static int senml_write_value(const wb_lwm2m_value_t *value,
                             char *buf, size_t buf_size, size_t offset)
{
    int w = -1;
    switch (value->type) {
    case WB_LWM2M_RES_TYPE_STRING:
    case WB_LWM2M_RES_TYPE_CORELNK:
        w = snprintf(buf + offset, buf_size - offset,
                     ",\"vs\":\"%.*s\"}",
                     (int)value->str_val.len, value->str_val.str);
        break;
    case WB_LWM2M_RES_TYPE_INTEGER:
        w = snprintf(buf + offset, buf_size - offset,
                     ",\"v\":%" PRId64 "}", value->int_val);
        break;
    case WB_LWM2M_RES_TYPE_UINTEGER:
        w = snprintf(buf + offset, buf_size - offset,
                     ",\"v\":%" PRIu64 "}", value->uint_val);
        break;
    case WB_LWM2M_RES_TYPE_TIME:
        w = snprintf(buf + offset, buf_size - offset,
                     ",\"v\":%" PRId64 "}", value->time_val);
        break;
    case WB_LWM2M_RES_TYPE_FLOAT:
        w = snprintf(buf + offset, buf_size - offset,
                     ",\"v\":%g}", value->float_val);
        break;
    case WB_LWM2M_RES_TYPE_BOOLEAN:
        w = snprintf(buf + offset, buf_size - offset,
                     ",\"vb\":%s}", value->bool_val ? "true" : "false");
        break;
    case WB_LWM2M_RES_TYPE_OPAQUE: {
        char b64[344];
        if (base64_encode(value->opaque_val.data, value->opaque_val.len,
                          b64, sizeof(b64)) < 0) return -1;
        w = snprintf(buf + offset, buf_size - offset,
                     ",\"vd\":\"%s\"}", b64);
        break;
    }
    case WB_LWM2M_RES_TYPE_OBJLINK:
        w = snprintf(buf + offset, buf_size - offset,
                     ",\"vlo\":\"%" PRIu16 ":%" PRIu16 "\"}",
                     value->objlink_val.obj_id, value->objlink_val.inst_id);
        break;
    default:
        return -1;
    }
    if (w < 0 || (size_t)w >= buf_size - offset) return -1;
    return w;
}

int wb_lwm2m_encode_senml_json_instance(const wb_lwm2m_object_def_t *obj,
                                        uint16_t inst_id,
                                        char *buf, size_t buf_size)
{
    if (obj == NULL || buf == NULL || buf_size == 0) {
        return -1;
    }

    size_t offset = 0;
    bool first_record = true;

    if (buf_size < 2) return -1;
    buf[offset++] = '[';

    for (size_t i = 0; i < obj->resource_count; i++) {
        const wb_lwm2m_resource_def_t *res = &obj->resources[i];
        if (res->type == WB_LWM2M_RES_TYPE_NONE || res->read_cb == NULL) {
            continue;
        }

        wb_lwm2m_value_t value = {0};
        if (res->read_cb(obj->id, inst_id, res->id, &value, obj->user_ctx) != ESP_OK) {
            continue;
        }

        if (!first_record) {
            if (offset >= buf_size) return -1;
            buf[offset++] = ',';
        }
        first_record = false;

        /* Record header: first record carries base name */
        int w;
        if (offset == 1) {  /* first record */
            w = snprintf(buf + offset, buf_size - offset,
                         "{\"bn\":\"/%" PRIu16 "/%" PRIu16 "/\",\"n\":\"%" PRIu16 "\"",
                         obj->id, inst_id, res->id);
        } else {
            w = snprintf(buf + offset, buf_size - offset,
                         "{\"n\":\"%" PRIu16 "\"", res->id);
        }
        if (w < 0 || (size_t)w >= buf_size - offset) return -1;
        offset += (size_t)w;

        /* Value field */
        w = senml_write_value(&value, buf, buf_size, offset);
        if (w < 0) return -1;
        offset += (size_t)w;
    }

    if (offset + 1 >= buf_size) return -1;
    buf[offset++] = ']';
    buf[offset] = '\0';

    return (int)offset;
}
