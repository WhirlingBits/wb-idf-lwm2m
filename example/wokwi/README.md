# Wokwi Simulation – wb-idf-coap-client Example (ESP32-S3)

Run the CoAP client example on a simulated **ESP32-S3-DevKitC-1** inside [Wokwi](https://wokwi.com).

## Prerequisites

| Tool | Purpose |
|---|---|
| **ESP-IDF ≥ 5.x** | Build toolchain |
| **Wokwi for VS Code** extension | Simulation inside VS Code |
| **Wokwi Gateway** (`wokwigw`) | Provides network access for the simulated ESP32 |

## Quick Start

### 1. Configure & Build

```bash
cd example
idf.py set-target esp32s3
idf.py menuconfig        # Set WiFi SSID / password / CoAP URI
idf.py build
```

> **Tip:** The defaults in `sdkconfig.defaults` point to the public Eclipse Californium test server (`coap://californium.eclipseprojects.io`). Adjust WiFi credentials to match your local network or Wokwi Gateway setup.

### 2. Start the Wokwi Gateway

The gateway bridges the simulated ESP32's network traffic to your host machine:

```bash
./wokwigw-linux
```

The gateway listens on `ws://localhost:9011` by default (configured in `wokwi.toml`).

### 3. Run the Simulation

Open the `wokwi.toml` file in VS Code and click **Start Simulation** (or press `F1` → *Wokwi: Start Simulator*).

The serial monitor will show:

```
I (xxx) coap_example: WiFi connected
I (xxx) coap_example: === Sending GET /.well-known/core ===
I (xxx) coap_example: Response 2.05 for GET /.well-known/core
I (xxx) coap_example: Payload (…):
...
I (xxx) coap_example: === Sending POST /test ===
I (xxx) coap_example: === Sending PUT /test ===
I (xxx) coap_example: === Observing /obs ===
```

## Files

| File | Description |
|---|---|
| `wokwi.toml` | Wokwi project config – points to the built ELF/BIN, enables network gateway |
| `diagram.json` | Board layout – ESP32-S3-DevKitC-1 with serial monitor |

## Network Configuration

The `[net]` section in `wokwi.toml` routes all TCP/UDP traffic through the Wokwi Gateway (`ws://localhost:9011`). This allows the simulated ESP32 to:

- Connect to WiFi (emulated)
- Resolve DNS names
- Reach external CoAP servers over the internet

## Troubleshooting

| Problem | Solution |
|---|---|
| WiFi connection fails | Make sure `wokwigw` is running before starting the simulation |
| CoAP timeout / no response | Verify the CoAP URI is reachable from your host (`coap-client -m get coap://californium.eclipseprojects.io/.well-known/core`) |
| ELF not found | Run `idf.py build` first – the simulation needs `build/wb-coap-client-example.elf` |
