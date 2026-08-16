# ESP32 ESP-IDF component for simple OTA server

## Wiki

[EN](WIKI_EN.md) | [RU](WIKI_RU.md)

## Tested on

1. [ESP32 ESP-IDF v6.0.0](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/index.html)

## SAST Tools

[PVS-Studio](https://pvs-studio.com/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.

## Features

1. HTTP-based OTA firmware updates via standard HTTP POST requests.
2. Automatic partition management with next available OTA partition detection.
3. Embedded web interface for browser-based firmware uploads.
4. Automatic boot activation and device reboot after successful update.
5. Chunked data reception with 1000-byte blocks for efficient memory usage.
6. Seamless integration with ESP-IDF HTTP server and OTA APIs.
7. Comprehensive error handling with detailed ESP_LOGE logging.
8. Low memory footprint with minimal RAM consumption.

## Using

In an existing project, run the following command to install the component:

```text
cd ../your_project/components
git clone https://github.com/aZholtikov/zh_ota_server
```

In the application, include the header:

```c
#include "zh_ota_server.h"
```

## Examples

See Wiki [EN](WIKI_EN.md#usage-examples) | [RU](WIKI_RU.md#примеры-использования)
