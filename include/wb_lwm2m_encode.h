/*
 * WhirlingBits LWM2M Client — Encoding helpers
 * Provides CoRE Link-Format and SenML-JSON encoding for LWM2M.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @defgroup wb_lwm2m_encode Encoding Helpers
 * @brief CoRE Link-Format and SenML-JSON encoding for LWM2M payloads
 *
 * This module provides serialization functions used internally by the
 * LWM2M client and available for direct use by applications:
 *
 * ### CoRE Link-Format (RFC 6690)
 *
 * Used in the LWM2M Registration request body to advertise available
 * objects and instances. Produces strings like:
 * @verbatim
 *   </1/0>,</3/0>,</5/0>
 * @endverbatim
 *
 * ### SenML-JSON (RFC 8428)
 *
 * Used for resource value notifications (Notify) and Read responses.
 * Content-Format ID: 110 (application/senml+json).
 *
 * Produces JSON arrays like:
 * @code{.json}
 * [{"bn":"/3/0/","n":"0","vs":"WhirlingBits"}]
 * [{"bn":"/3/0/","n":"10","v":245760}]
 * @endcode
 *
 * ### Type Mapping
 *
 * | LWM2M Type | SenML Key | Example |
 * |------------|-----------|---------|
 * | String / CoreLnk | "vs" | `"vs":"hello"` |
 * | Integer / Time | "v" | `"v":42` |
 * | UInteger | "v" | `"v":42` |
 * | Float | "v" | `"v":23.5` |
 * | Boolean | "vb" | `"vb":true` |
 * | Opaque | "vd" | `"vd":"AQID"` (base64) |
 * | Objlink | "vlo" | `"vlo":"3:0"` |
 *
 * @{
 */

#ifndef __WB_LWM2M_ENCODE_H__
#define __WB_LWM2M_ENCODE_H__

#include <stdint.h>
#include <stddef.h>
#include "wb_lwm2m_object.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encode registered objects as CoRE Link-Format for registration payload
 *
 * Produces a string like: </1/0>,</3/0>,</5/0>
 *
 * @param objects        Array of object definitions
 * @param object_count   Number of objects
 * @param buf            Output buffer
 * @param buf_size       Size of output buffer
 * @return Number of bytes written (excluding NUL), or -1 on error
 */
int wb_lwm2m_encode_link_format(const wb_lwm2m_object_def_t *const *objects,
                                size_t object_count,
                                char *buf, size_t buf_size);

/**
 * @brief Encode a single resource value as SenML-JSON
 *
 * Supports all LWM2M data types:
 *   String/CoreLnk  -> "vs"  (string value)
 *   Integer/Time     -> "v"   (numeric value, signed)
 *   UInteger         -> "v"   (numeric value, unsigned)
 *   Float            -> "v"   (numeric value)
 *   Boolean          -> "vb"  (boolean value)
 *   Opaque           -> "vd"  (base64-encoded data value)
 *   Objlink          -> "vlo" (object link "objId:instId")
 *
 * Produces: [{"bn":"/3/0/","n":"0","vs":"WhirlingBits"}]
 *
 * @param obj_id     Object ID
 * @param inst_id    Instance ID
 * @param res_id     Resource ID
 * @param value      Resource value
 * @param buf        Output buffer
 * @param buf_size   Size of output buffer
 * @return Number of bytes written (excluding NUL), or -1 on error
 */
int wb_lwm2m_encode_senml_json(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                               const wb_lwm2m_value_t *value,
                               char *buf, size_t buf_size);

/**
 * @brief Encode all readable resources of an object instance as SenML-JSON
 *
 * Iterates all resources, calls each read callback, and produces a single
 * SenML-JSON array with a shared base name.
 *
 * @param obj       Object definition
 * @param inst_id   Instance ID
 * @param buf       Output buffer
 * @param buf_size  Size of output buffer
 * @return Number of bytes written (excluding NUL), or -1 on error
 */
int wb_lwm2m_encode_senml_json_instance(const wb_lwm2m_object_def_t *obj,
                                        uint16_t inst_id,
                                        char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

/** @} */ /* end of wb_lwm2m_encode group */

#endif /* __WB_LWM2M_ENCODE_H__ */
