/*
 * WhirlingBits LWM2M Client — Encoding helpers
 * Provides CoRE Link-Format and SenML-JSON encoding for LWM2M.
 *
 * SPDX-License-Identifier: Apache-2.0
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

#endif /* __WB_LWM2M_ENCODE_H__ */
