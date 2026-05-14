/*
 * WhirlingBits LWM2M — TLV Encoder/Decoder
 *
 * OMA-TS-LightweightM2M_Core-V1_0, §6.4.3
 *
 * TLV byte layout:
 *   Bits 7-6: Type of Identifier
 *     00 = Object Instance
 *     01 = Resource Instance (inside multi-resource)
 *     10 = Multiple Resource (container)
 *     11 = Resource (single value)
 *   Bit  5: Identifier field length (0 = 8-bit, 1 = 16-bit)
 *   Bits 4-3: Length of "Length" field (00 = in bits 2-0, 01..11 = 1..3 bytes)
 *   Bits 2-0: Length (when bits 4-3 = 00) — 3 bits → 0..7 bytes
 *
 * Integer values: big-endian, smallest necessary size (1,2,4,8 bytes)
 * Float values: IEEE 754, big-endian, 4 or 8 bytes
 * Boolean: 1 byte (0 or 1)
 * Objlink: 4 bytes (2 × uint16 big-endian)
 * Time: same as Integer (signed)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wb_lwm2m_tlv.h"
#include <string.h>
#include <math.h>

/* ── Integer encoding helpers ── */

/**
 * @brief Determine minimal byte count for a signed integer (1, 2, 4, or 8)
 */
static size_t int_byte_count(int64_t v)
{
    if (v >= INT8_MIN && v <= INT8_MAX)   return 1;
    if (v >= INT16_MIN && v <= INT16_MAX) return 2;
    if (v >= INT32_MIN && v <= INT32_MAX) return 4;
    return 8;
}

/**
 * @brief Determine minimal byte count for an unsigned integer
 */
static size_t uint_byte_count(uint64_t v)
{
    if (v <= UINT8_MAX)  return 1;
    if (v <= UINT16_MAX) return 2;
    if (v <= UINT32_MAX) return 4;
    return 8;
}

/**
 * @brief Write a signed integer as big-endian bytes
 */
static void write_int_be(uint8_t *buf, int64_t v, size_t n)
{
    uint64_t u = (uint64_t)v;
    for (size_t i = 0; i < n; i++) {
        buf[n - 1 - i] = (uint8_t)(u & 0xFF);
        u >>= 8;
    }
}

/**
 * @brief Write an unsigned integer as big-endian bytes
 */
static void write_uint_be(uint8_t *buf, uint64_t v, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        buf[n - 1 - i] = (uint8_t)(v & 0xFF);
        v >>= 8;
    }
}

/**
 * @brief Read a signed integer from big-endian bytes
 */
static int64_t read_int_be(const uint8_t *buf, size_t n)
{
    if (n == 0) return 0;
    /* Sign-extend from the first byte */
    int64_t v = (int8_t)buf[0];
    for (size_t i = 1; i < n; i++) {
        v = (v << 8) | buf[i];
    }
    return v;
}

/**
 * @brief Read an unsigned integer from big-endian bytes
 */
static uint64_t read_uint_be(const uint8_t *buf, size_t n)
{
    uint64_t v = 0;
    for (size_t i = 0; i < n; i++) {
        v = (v << 8) | buf[i];
    }
    return v;
}

/**
 * @brief Write the TLV header (type + ID + length) and return bytes consumed
 *
 * @param buf       Output buffer
 * @param buf_size  Buffer size
 * @param tlv_type  TLV identifier type (masked into bits 7-6)
 * @param id        Resource/Instance ID
 * @param value_len Length of the value that follows
 * @return Header size in bytes, or -1 if buffer too small
 */
static int write_tlv_header(uint8_t *buf, size_t buf_size,
                            wb_lwm2m_tlv_type_t tlv_type,
                            uint16_t id, size_t value_len)
{
    uint8_t type_byte = (uint8_t)tlv_type;
    size_t hdr_size = 1;  /* type byte */

    /* ID field: 1 or 2 bytes */
    bool id_16bit = (id > 0xFF);
    if (id_16bit) {
        type_byte |= 0x20;  /* Bit 5 = 1 → 16-bit ID */
        hdr_size += 2;
    } else {
        hdr_size += 1;
    }

    /* Length field */
    size_t len_field_size;
    if (value_len <= 7) {
        type_byte |= (uint8_t)(value_len & 0x07);  /* Encoded in bits 2-0 */
        len_field_size = 0;
    } else if (value_len <= 0xFF) {
        type_byte |= 0x08;  /* bits 4-3 = 01 → 1-byte length */
        len_field_size = 1;
    } else if (value_len <= 0xFFFF) {
        type_byte |= 0x10;  /* bits 4-3 = 10 → 2-byte length */
        len_field_size = 2;
    } else if (value_len <= 0xFFFFFF) {
        type_byte |= 0x18;  /* bits 4-3 = 11 → 3-byte length */
        len_field_size = 3;
    } else {
        return -1;  /* Too large */
    }
    hdr_size += len_field_size;

    if (hdr_size + value_len > buf_size) {
        return -1;
    }

    /* Write type byte */
    size_t pos = 0;
    buf[pos++] = type_byte;

    /* Write ID */
    if (id_16bit) {
        buf[pos++] = (uint8_t)(id >> 8);
        buf[pos++] = (uint8_t)(id & 0xFF);
    } else {
        buf[pos++] = (uint8_t)id;
    }

    /* Write length field */
    if (len_field_size == 1) {
        buf[pos++] = (uint8_t)value_len;
    } else if (len_field_size == 2) {
        buf[pos++] = (uint8_t)(value_len >> 8);
        buf[pos++] = (uint8_t)(value_len & 0xFF);
    } else if (len_field_size == 3) {
        buf[pos++] = (uint8_t)(value_len >> 16);
        buf[pos++] = (uint8_t)((value_len >> 8) & 0xFF);
        buf[pos++] = (uint8_t)(value_len & 0xFF);
    }

    return (int)pos;
}

/* ── Public API: Encode ── */

int wb_lwm2m_tlv_encode_resource(uint16_t res_id,
                                 const wb_lwm2m_value_t *value,
                                 uint8_t *buf, size_t buf_size)
{
    if (value == NULL || buf == NULL || buf_size == 0) {
        return -1;
    }

    /* First, encode the value into a temp buffer to know its length */
    uint8_t val_buf[256];
    size_t val_len = 0;

    switch (value->type) {
    case WB_LWM2M_RES_TYPE_STRING:
    case WB_LWM2M_RES_TYPE_CORELNK:
        val_len = value->str_val.len;
        if (val_len > sizeof(val_buf)) return -1;
        memcpy(val_buf, value->str_val.str, val_len);
        break;

    case WB_LWM2M_RES_TYPE_INTEGER:
    case WB_LWM2M_RES_TYPE_TIME:
        val_len = int_byte_count(value->int_val);
        write_int_be(val_buf, value->int_val, val_len);
        break;

    case WB_LWM2M_RES_TYPE_UINTEGER:
        val_len = uint_byte_count(value->uint_val);
        write_uint_be(val_buf, value->uint_val, val_len);
        break;

    case WB_LWM2M_RES_TYPE_FLOAT: {
        /* Use 32-bit if value fits in float without precision loss, else 64-bit */
        float f32 = (float)value->float_val;
        if ((double)f32 == value->float_val && !isinf(f32)) {
            val_len = 4;
            uint32_t u;
            memcpy(&u, &f32, 4);
            /* Convert to big-endian */
            val_buf[0] = (uint8_t)(u >> 24);
            val_buf[1] = (uint8_t)(u >> 16);
            val_buf[2] = (uint8_t)(u >> 8);
            val_buf[3] = (uint8_t)(u);
        } else {
            val_len = 8;
            uint64_t u;
            double d = value->float_val;
            memcpy(&u, &d, 8);
            for (int i = 0; i < 8; i++) {
                val_buf[7 - i] = (uint8_t)(u & 0xFF);
                u >>= 8;
            }
        }
        break;
    }

    case WB_LWM2M_RES_TYPE_BOOLEAN:
        val_len = 1;
        val_buf[0] = value->bool_val ? 1 : 0;
        break;

    case WB_LWM2M_RES_TYPE_OPAQUE:
        val_len = value->opaque_val.len;
        if (val_len > sizeof(val_buf)) return -1;
        memcpy(val_buf, value->opaque_val.data, val_len);
        break;

    case WB_LWM2M_RES_TYPE_OBJLINK:
        val_len = 4;
        val_buf[0] = (uint8_t)(value->objlink_val.obj_id >> 8);
        val_buf[1] = (uint8_t)(value->objlink_val.obj_id & 0xFF);
        val_buf[2] = (uint8_t)(value->objlink_val.inst_id >> 8);
        val_buf[3] = (uint8_t)(value->objlink_val.inst_id & 0xFF);
        break;

    case WB_LWM2M_RES_TYPE_NONE:
    default:
        return -1;
    }

    /* Write header */
    int hdr_len = write_tlv_header(buf, buf_size, WB_LWM2M_TLV_TYPE_RESOURCE,
                                   res_id, val_len);
    if (hdr_len < 0) {
        return -1;
    }

    /* Check space for value */
    if ((size_t)hdr_len + val_len > buf_size) {
        return -1;
    }

    /* Write value */
    memcpy(buf + hdr_len, val_buf, val_len);
    return hdr_len + (int)val_len;
}

int wb_lwm2m_tlv_encode_instance(const wb_lwm2m_object_def_t *obj,
                                 uint16_t inst_id,
                                 uint8_t *buf, size_t buf_size)
{
    if (obj == NULL || buf == NULL || buf_size == 0) {
        return -1;
    }

    size_t offset = 0;

    for (size_t i = 0; i < obj->resource_count; i++) {
        const wb_lwm2m_resource_def_t *res = &obj->resources[i];

        /* Skip execute-only resources (they have no value) */
        if (res->type == WB_LWM2M_RES_TYPE_NONE) {
            continue;
        }

        /* Skip resources without a read callback */
        if (res->read_cb == NULL) {
            continue;
        }

        /* Read the current value */
        wb_lwm2m_value_t value = {0};
        esp_err_t err = res->read_cb(obj->id, inst_id, res->id, &value, obj->user_ctx);
        if (err != ESP_OK) {
            continue;  /* Skip unreadable resources */
        }

        /* Encode as TLV */
        int written = wb_lwm2m_tlv_encode_resource(res->id, &value,
                                                   buf + offset, buf_size - offset);
        if (written < 0) {
            return -1;  /* Buffer too small */
        }
        offset += (size_t)written;
    }

    return (int)offset;
}

/* ── Public API: Decode ── */

int wb_lwm2m_tlv_decode_header(const uint8_t *buf, size_t buf_len,
                               wb_lwm2m_tlv_type_t *tlv_type,
                               uint16_t *id,
                               size_t *value_offset,
                               size_t *value_len)
{
    if (buf == NULL || buf_len < 2 || tlv_type == NULL ||
        id == NULL || value_offset == NULL || value_len == NULL) {
        return -1;
    }

    size_t pos = 0;
    uint8_t type_byte = buf[pos++];

    /* Bits 7-6: Identifier type */
    *tlv_type = (wb_lwm2m_tlv_type_t)(type_byte & 0xC0);

    /* Bit 5: ID length */
    bool id_16bit = (type_byte & 0x20) != 0;

    /* Bits 4-3: Length encoding */
    uint8_t len_type = (type_byte >> 3) & 0x03;

    /* Bits 2-0: Short length (if len_type == 0) */
    uint8_t short_len = type_byte & 0x07;

    /* Read ID */
    if (id_16bit) {
        if (pos + 2 > buf_len) return -1;
        *id = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
        pos += 2;
    } else {
        if (pos + 1 > buf_len) return -1;
        *id = buf[pos++];
    }

    /* Read length */
    size_t vlen;
    if (len_type == 0) {
        vlen = short_len;
    } else {
        size_t len_bytes = len_type;  /* 1, 2, or 3 */
        if (pos + len_bytes > buf_len) return -1;
        vlen = 0;
        for (size_t i = 0; i < len_bytes; i++) {
            vlen = (vlen << 8) | buf[pos++];
        }
    }

    *value_offset = pos;
    *value_len = vlen;

    /* Validate total length */
    if (pos + vlen > buf_len) {
        return -1;
    }

    return (int)(pos + vlen);
}

esp_err_t wb_lwm2m_tlv_decode_value(wb_lwm2m_res_type_t type,
                                    const uint8_t *data, size_t data_len,
                                    wb_lwm2m_value_t *value)
{
    if (data == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    value->type = type;

    switch (type) {
    case WB_LWM2M_RES_TYPE_STRING:
    case WB_LWM2M_RES_TYPE_CORELNK:
        value->str_val.str = (const char *)data;
        value->str_val.len = data_len;
        return ESP_OK;

    case WB_LWM2M_RES_TYPE_INTEGER:
    case WB_LWM2M_RES_TYPE_TIME:
        if (data_len != 1 && data_len != 2 && data_len != 4 && data_len != 8) {
            return ESP_ERR_INVALID_SIZE;
        }
        value->int_val = read_int_be(data, data_len);
        return ESP_OK;

    case WB_LWM2M_RES_TYPE_UINTEGER:
        if (data_len != 1 && data_len != 2 && data_len != 4 && data_len != 8) {
            return ESP_ERR_INVALID_SIZE;
        }
        value->uint_val = read_uint_be(data, data_len);
        return ESP_OK;

    case WB_LWM2M_RES_TYPE_FLOAT:
        if (data_len == 4) {
            uint32_t u = (uint32_t)read_uint_be(data, 4);
            float f;
            memcpy(&f, &u, 4);
            value->float_val = (double)f;
        } else if (data_len == 8) {
            uint64_t u = read_uint_be(data, 8);
            double d;
            memcpy(&d, &u, 8);
            value->float_val = d;
        } else {
            return ESP_ERR_INVALID_SIZE;
        }
        return ESP_OK;

    case WB_LWM2M_RES_TYPE_BOOLEAN:
        if (data_len != 1) {
            return ESP_ERR_INVALID_SIZE;
        }
        value->bool_val = (data[0] != 0);
        return ESP_OK;

    case WB_LWM2M_RES_TYPE_OPAQUE:
        value->opaque_val.data = data;
        value->opaque_val.len = data_len;
        return ESP_OK;

    case WB_LWM2M_RES_TYPE_OBJLINK:
        if (data_len != 4) {
            return ESP_ERR_INVALID_SIZE;
        }
        value->objlink_val.obj_id = ((uint16_t)data[0] << 8) | data[1];
        value->objlink_val.inst_id = ((uint16_t)data[2] << 8) | data[3];
        return ESP_OK;

    case WB_LWM2M_RES_TYPE_NONE:
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}
