# zh_ota_server — OTA Firmware Update Server Component for ESP-IDF

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Installation](#installation)
- [API Reference](#api-reference)
- [Usage Examples](#usage-examples)
- [Technical Specifications](#technical-specifications)
- [Error Codes](#error-codes)
- [Contributing](#contributing)
- [License](#license)

---

## Overview

`zh_ota_server` is a lightweight Over-the-Air (OTA) firmware update server component for ESP-IDF (Espressif IoT Development Framework) designed for ESP32 microcontrollers. The component provides an HTTP-based server that accepts firmware binary updates via POST requests, writes them to the next available OTA partition, and automatically activates the new firmware.

The component handles the entire OTA workflow: receiving firmware data in chunks, flashing it to flash memory, validating the update, setting the boot partition, and rebooting the device. A web interface is embedded as a binary resource for convenient firmware uploads through a browser.

This component is ideal for devices deployed in the field that need remote firmware updates without physical access.

---

## Features

1. **HTTP-based OTA Updates**: Firmware upload via standard HTTP POST requests
2. **Automatic Partition Management**: Automatic detection and use of the next available OTA partition
3. **Embedded Web Interface**: Built-in HTML page for browser-based firmware uploads
4. **Automatic Boot Activation**: Device reboots and boots from the new partition after successful update
5. **Chunked Data Reception**: Efficient handling of firmware data in 1000-byte chunks
6. **ESP-IDF Integration**: Seamless integration with ESP-IDF HTTP server and OTA APIs
7. **Error Handling**: Comprehensive error checking with detailed ESP_LOGE logging
8. **Low Memory Footprint**: Minimal RAM usage with local buffering

---

## Installation

Navigate to your project's components directory:

```bash
cd ../your_project/components
```

Clone the repository:

```bash
git clone https://github.com/aZholtikov/zh_ota_server
```

In your application, include the header:

```c
#include "zh_ota_server.h"
```

The component will be automatically built with your project.

---

## API Reference

### zh_ota_server_init()

Initializes and registers the OTA server with the ESP-IDF HTTP server instance.

**Parameters:**

- `server` - HTTP server handle obtained from httpd_start(). Must not be NULL
- `path` - URI path for the OTA server endpoints (e.g., "/ota").

**Returns:**

- `ESP_OK` - OTA server successfully initialized and registered
- `ESP_ERR_INVALID_ARG` - Invalid server handle or path (NULL pointer)
- `ESP_ERR_NO_MEM` - Memory allocation failed for the path storage
- `ESP_FAIL` - Failed to register the URI handler with the HTTP server

**Example:**

```c
httpd_handle_t http_server_handle = NULL;
httpd_config_t config = HTTPD_DEFAULT_CONFIG();
httpd_start(&http_server_handle, &config);
zh_ota_server_init(http_server_handle, "/ota");
```

**Note:** The path string is copied internally and managed by the component. The caller must not free or modify the path after this function returns.

**Warning:** Memory allocated for the path is never freed. Call this function only once during the application lifetime.

---

## Usage Examples

### Basic OTA Server Setup

This example shows how to set up a Wi-Fi Access Point and start the OTA server:

```c
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "zh_ota_server.h"

#define WIFI_SSID "MyDevice"
#define WIFI_PASS "secure_password"

static httpd_handle_t http_server_handle = NULL;

void app_main(void)
{
    esp_log_level_set("zh_ota_server", ESP_LOG_ERROR);
    // Initialize NVS and Wi-Fi
    nvs_flash_init();
    esp_event_loop_create_default();
    esp_netif_init();
    esp_netif_create_default_wifi_ap();
    // Configure Wi-Fi AP
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_config);
    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();
    // Start HTTP server and register OTA handler
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&http_server_handle, &config);
    zh_ota_server_init(http_server_handle, "/ota");
}
```

### Remote Firmware Update Workflow

The typical firmware update workflow:

1. Device boots in Access Point mode (SSID: "MyDevice", IP: 192.168.4.1)
2. User connects via Wi-Fi to the device
3. User opens a browser and navigates to <http://192.168.4.1/ota>
4. User selects a firmware binary file (.bin)
5. Device receives, validates, and flashes the firmware
6. Device reboots and boots from the new partition

---

## Technical Specifications

| Parameter | Value |
| ----------- | ------- |
| **Max Firmware Size** | Limited by available OTA partition size |
| **Buffer Size** | 1000 bytes per chunk |
| **Memory Caps** | MALLOC_CAP_8BIT |
| **Thread Safety** | Not thread-safe |
| **ESP-IDF Version** | >= 5.0 |
| **Platform** | ESP32 series |
| **Language** | C (C99) |
| **Dependencies** | esp_http_server, esp_ota_ops, esp_log |

---

## Error Codes

| Error Code | Description |
| ------------ | ---------- |
| `ESP_OK` | Operation completed successfully |
| `ESP_ERR_INVALID_ARG` | Invalid argument (NULL server handle or path) |
| `ESP_ERR_NO_MEM` | Memory allocation failed (no free heap) |
| `ESP_FAIL` | General failure (URI registration, OTA operation, or HTTP response) |

---

## Contributing

Contributions are welcome! To contribute:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

Please ensure your code follows the existing style and includes appropriate documentation.

---

## License

This project is licensed under the Apache License, Version 2.0 - see the [LICENSE](LICENSE) file for details.

### Apache License, Version 2.0

Copyright (c) 2026 Alexey Zholtikov

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

---

## Additional Notes

- **Memory Management**: The URI path is allocated once during initialization and never freed. This is acceptable for embedded systems with stable memory availability
- **Security**: This component provides no authentication or encryption. Use in a trusted network environment only. For production, add HTTPS and authentication
- **Device Bricking Risk**: Flashing an invalid or corrupted firmware image will make the device unbootable until a valid image is flashed via another mechanism (e.g., UART bootloader)
- **Best Practices**:
  - Always validate firmware checksums before flashing
  - Implement a fallback mechanism (e.g., dual-bank OTA with backup)
  - Add authentication to prevent unauthorized firmware uploads
  - Consider adding a rollback feature in case of failed updates
  - Monitor free heap memory before starting large OTA transfers

---

*Generated for zh_ota_server v1.0.0*
