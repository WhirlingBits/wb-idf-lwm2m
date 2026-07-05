/*
 * WhirlingBits LWM2M — TLV Encoder/Decoder
 *
 * Implements the OMA LWM2M TLV (Type-Length-Value) binary format
 * as defined in OMA-TS-LightweightM2M_Core-V1_0, §6.4.3.
 *
 * TLV is the default content-format for LWM2M 1.0 object/instance reads.
 * CoAP Content-Format: application/vnd.oma.lwm2m+tlv (11542)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @defgroup wb_lwm2m_tlv TLV Encoder/Decoder
 * @brief Binary TLV encoding and decoding per OMA LWM2M spec §6.4.3
 *
 * This module implements the OMA LWM2M TLV (Type-Length-Value) binary
 * content-format used for Read responses and Write requests in LWM2M 1.0.
 *
 * CoAP Content-Format: `application/vnd.oma.lwm2m+tlv` (11542)
 *
 * ### TLV Record Structure
 *
 * Each TLV record consists of:
 * @verbatim
 *   [Type Byte] [Identifier: 1-2 bytes] [Length: 0-3 bytes] [Value: N bytes]
 *
 *   Type Byte (bits 7-6):
 *     00 = Object Instance
 *     01 = Resource Instance (within multi-resource)
 *     10 = Multiple Resource (container)
 *     11 = Resource (single value)
 * @endverbatim
 *
 * ### Identifier Types
 *
 * | Type | Bits 7-6 | Enum |
 * |------|----------|------|
 * | Object Instance | 00 | WB_LWM2M_TLV_TYPE_OBJECT_INSTANCE |
 * | Resource Instance | 01 | WB_LWM2M_TLV_TYPE_RESOURCE_INSTANCE |
 * | Multiple Resource | 10 | WB_LWM2M_TLV_TYPE_MULTI_RESOURCE |
 * | Resource | 11 | WB_LWM2M_TLV_TYPE_RESOURCE |
 *
 * ### Content-Format Constants
 *
 * | Constant | Value | MIME Type |
 * |----------|-------|-----------|
 * | WB_LWM2M_CONTENT_FORMAT_TLV | 11542 | application/vnd.oma.lwm2m+tlv |
 * | WB_LWM2M_CONTENT_FORMAT_JSON | 11543 | application/vnd.oma.lwm2m+json |
 * | WB_LWM2M_CONTENT_FORMAT_SENML_JSON | 110 | application/senml+json |
 *
 * ### Encoding Example
 *
 * @code{.c}
 * uint8_t buf[128];
 * wb_lwm2m_value_t val = WB_LWM2M_VALUE_STRING("WhirlingBits");
 *
 * int len = wb_lwm2m_tlv_encode_resource(0, &val, buf, sizeof(buf));
 * // buf now contains the TLV-encoded manufacturer string
 * @endcode
 *
 * ### Decoding Example
 *
 * @code{.c}
 * wb_lwm2m_tlv_type_t type;
 * uint16_t id;
 * size_t val_offset, val_len;
 *
 * int consumed = wb_lwm2m_tlv_decode_header(buf, buf_len,
 *                                           &type, &id, &val_offset, &val_len);
 * if (consumed > 0) {
 *     wb_lwm2m_value_t value;
 *     wb_lwm2m_tlv_decode_value(WB_LWM2M_RES_TYPE_STRING,
 *                               buf + val_offset, val_len, &value);
 * }
 * @endcode
 *
 * @{
 */

#ifndef __WB_LWM2M_TLV_H__
#define __WB_LWM2M_TLV_H__

#include <stdint.h>
#include <stddef.h>
#include "wb_lwm2m_object.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief TLV Identifier Types (OMA-TS-LightweightM2M_Core §6.4.3)
 *
 * Encoded in bits 7-6 of the TLV type byte.
 */

typedef enum {
    WB_LWM2M_TLV_TYPE_OBJECT_INSTANCE       = 0x00,  /**< Bits 7-6 = 00 */
    WB_LWM2M_TLV_TYPE_RESOURCE_INSTANCE     = 0x40,  /**< Bits 7-6 = 01 */
    WB_LWM2M_TLV_TYPE_MULTI_RESOURCE        = 0x80,  /**< Bits 7-6 = 10 */
    WB_LWM2M_TLV_TYPE_RESOURCE              = 0xC0,  /**< Bits 7-6 = 11 */
} wb_lwm2m_tlv_type_t;

/**
 * @name TLV Content-Format IDs
 * CoAP Content-Format identifiers for LWM2M payload types.
 * @{
 */
#define WB_LWM2M_CONTENT_FORMAT_TLV       11542  /**< application/vnd.oma.lwm2m+tlv */
#define WB_LWM2M_CONTENT_FORMAT_JSON      11543  /**< application/vnd.oma.lwm2m+json */
#define WB_LWM2M_CONTENT_FORMAT_SENML_JSON 110   /**< application/senml+json */
/** @} */

/**
 * @brief Encode a single resource value as a TLV record
 *
 * Output format (per OMA spec §6.4.3):
 *   [type_byte] [id: 1-2 bytes] [length: 0-3 bytes] [value: N bytes]
 *
 * @param res_id   Resource ID
 * @param value    Resource value to encode
 * @param buf      Output buffer
 * @param buf_size Size of output buffer
 * @return Number of bytes written, or -1 on error
 */
int wb_lwm2m_tlv_encode_resource(uint16_t res_id,
                                 const wb_lwm2m_value_t *value,
                                 uint8_t *buf, size_t buf_size);

/**
 * @brief Encode an entire object instance as concatenated TLV records
 *
 * Reads each resource via its read callback and concatenates TLV records.
 *
 * @param obj       Object definition
 * @param inst_id   Instance ID
 * @param buf       Output buffer
 * @param buf_size  Size of output buffer
 * @return Number of bytes written, or -1 on error
 */
int wb_lwm2m_tlv_encode_instance(const wb_lwm2m_object_def_t *obj,
                                 uint16_t inst_id,
                                 uint8_t *buf, size_t buf_size);

/**
 * @brief Decode a single TLV record from a buffer
 *
 * On success, *tlv_type, *id, *value_offset, *value_len describe the record.
 *
 * @param buf           Input buffer
 * @param buf_len       Remaining buffer length
 * @param tlv_type      Output: TLV identifier type
 * @param id            Output: resource/instance ID
 * @param value_offset  Output: offset of the value data within buf
 * @param value_len     Output: length of the value data
 * @return Total bytes consumed (header + value), or -1 on error
 */
int wb_lwm2m_tlv_decode_header(const uint8_t *buf, size_t buf_len,
                               wb_lwm2m_tlv_type_t *tlv_type,
                               uint16_t *id,
                               size_t *value_offset,
                               size_t *value_len);

/**
 * @brief Decode a TLV value into an lwm2m_value_t based on the expected type
 *
 * @param type          Expected resource data type
 * @param data          Pointer to the TLV value bytes
 * @param data_len      Length of value bytes
 * @param value         Output: decoded value
 * @return ESP_OK on success, ESP_ERR_INVALID_SIZE if length is unexpected
 */
esp_err_t wb_lwm2m_tlv_decode_value(wb_lwm2m_res_type_t type,
                                    const uint8_t *data, size_t data_len,
                                    wb_lwm2m_value_t *value);

#ifdef __cplusplus
}
#endif

/** @} */ /* end of wb_lwm2m_tlv group */

#endif /* __WB_LWM2M_TLV_H__ */
