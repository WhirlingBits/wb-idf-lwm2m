# wb-idf-lwm2m

A lightweight OMA LWM2M 1.0 client component for ESP-IDF, built on top of [wb-idf-coap-client](https://gitlab.whirlingbits.de/whirlingbits/wb-idf-coap-client).

## Features

- **LWM2M Registration Interface** — Register, Update, Deregister with any LWM2M server
- **Object/Resource Model** — Define LWM2M objects with typed resources and read/write/execute callbacks
- **Automatic Registration Updates** — Timer-based keepalive at 80% of configured lifetime
- **SenML-JSON Encoding** — Resource value notifications in standard SenML-JSON format
- **CoRE Link-Format** — Registration payload per RFC 6690
- **Security** — Supports plain CoAP, DTLS-PSK, and DTLS-PKI (via wb-idf-coap-client)
- **Standard Objects** — Predefined constants for Device (/3), Server (/1), Firmware Update (/5) etc.
- **Compatible** with Leshan, ThingsBoard, and other LWM2M servers

## Dependencies

- [wb-idf-coap-client](https://github.com/WhirlingBits/wb-idf-coap-client) (CoAP transport layer)
- ESP-IDF >= 5.0

## Installation

Add as an ESP-IDF component. In your project's `idf_component.yml`:

```yaml
dependencies:
  whirlingbits/wb-idf-lwm2m:
    version: "*"
    git: https://github.com/WhirlingBits/wb-idf-lwm2m.git
```

Or clone into your project's `components/` directory.

## Quick Start

### 1. Define Objects

```c
#include "wb_lwm2m.h"
#include "wb_lwm2m_object.h"

static esp_err_t device_read_cb(uint16_t obj_id, uint16_t inst_id, uint16_t res_id,
                                wb_lwm2m_value_t *value, void *user_ctx)
{
    switch (res_id) {
    case WB_LWM2M_RES_DEVICE_MANUFACTURER:
        *value = WB_LWM2M_VALUE_STRING("WhirlingBits");
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_MODEL_NUMBER:
        *value = WB_LWM2M_VALUE_STRING("ESP32-LWM2M");
        return ESP_OK;
    case WB_LWM2M_RES_DEVICE_FW_VERSION:
        *value = WB_LWM2M_VALUE_STRING("1.0.0");
        return ESP_OK;
    default:
        return ESP_ERR_NOT_FOUND;
    }
}

static const wb_lwm2m_object_def_t device_object = {
    .id = WB_LWM2M_OBJ_DEVICE,
    .name = "Device",
    .instances = { { .id = 0 } },
    .instance_count = 1,
    .resources = {
        { .id = 0, .type = WB_LWM2M_RES_TYPE_STRING, .operations = WB_LWM2M_OP_READ, .read_cb = device_read_cb },
        { .id = 1, .type = WB_LWM2M_RES_TYPE_STRING, .operations = WB_LWM2M_OP_READ, .read_cb = device_read_cb },
        { .id = 3, .type = WB_LWM2M_RES_TYPE_STRING, .operations = WB_LWM2M_OP_READ, .read_cb = device_read_cb },
    },
    .resource_count = 3,
};
```

### 2. Create and Start Client

```c
wb_lwm2m_client_config_t config = {
    .server_uri = "coap://leshan.eclipseprojects.io:5683",
    .endpoint_name = "my-esp32-device",
    .lifetime = 300,
    .security = WB_COAP_SECURITY_NONE,
    .event_handler = my_event_handler,
};

wb_lwm2m_client_handle_t lwm2m = wb_lwm2m_init(&config);
wb_lwm2m_add_object(lwm2m, &device_object);
wb_lwm2m_start(lwm2m);
```

### 3. Send Notifications

```c
/* Notify server about a changed resource */
wb_lwm2m_notify(lwm2m, WB_LWM2M_OBJ_DEVICE, 0, WB_LWM2M_RES_DEVICE_MEMORY_FREE);
```

## ThingsBoard Integration

For ThingsBoard's LWM2M device integration:

```c
wb_lwm2m_client_config_t config = {
    .server_uri = "coap://YOUR_TB_HOST:5685",
    .endpoint_name = "YOUR_DEVICE_ENDPOINT",  /* Must match the TB device profile */
    .lifetime = 300,
    .lwm2m_version = "1.0",
    .security = WB_COAP_SECURITY_PSK,
    .psk = {
        .identity = "YOUR_DEVICE_ENDPOINT",
        .key = psk_key_bytes,
        .key_len = sizeof(psk_key_bytes),
    },
    .event_handler = my_event_handler,
};
```

> **Note**: The objects registered here must match the LWM2M Device Profile
> configured in ThingsBoard.

## API Reference

| Function | Description |
|----------|-------------|
| `wb_lwm2m_init()` | Create client instance |
| `wb_lwm2m_add_object()` | Register an LWM2M object (before start) |
| `wb_lwm2m_start()` | Connect and register with server |
| `wb_lwm2m_update_registration()` | Manually trigger registration update |
| `wb_lwm2m_notify()` | Send resource value notification |
| `wb_lwm2m_stop()` | Deregister and disconnect |
| `wb_lwm2m_destroy()` | Free all resources |
| `wb_lwm2m_is_registered()` | Check registration state |
| `wb_lwm2m_get_state()` | Get detailed client state |

## Architecture

```
┌──────────────────────────────────────┐
│           User Application           │
├──────────────────────────────────────┤
│  wb-idf-lwm2m                        │
│  ├── Object/Resource Model           │
│  ├── Registration Interface          │
│  ├── Link-Format / SenML Encoding    │
│  └── Lifecycle Management            │
├──────────────────────────────────────┤
│  wb-idf-coap-client                  │
│  └── GET / POST / PUT / DELETE       │
│  └── Observe / DTLS-PSK / DTLS-PKI  │
├──────────────────────────────────────┤
│  libcoap (espressif/coap)            │
├──────────────────────────────────────┤
│  ESP-IDF (lwIP, mbedTLS, FreeRTOS)   │
└──────────────────────────────────────┘
```

## Limitations

- **Client-initiated only**: Since wb-idf-coap-client is a CoAP client wrapper, server-initiated Read/Write/Execute requests (where the LWM2M server sends CoAP requests to the device) are not yet supported. Extending this would require CoAP server capabilities on the device side.
- **No Bootstrap**: LWM2M Bootstrap interface is not implemented.
- **No Block-wise OTA**: Firmware Update object execution is application-level only.

## License

Apache-2.0
