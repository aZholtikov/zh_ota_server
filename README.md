# ESP32 ESP-IDF component for simple OTA server

## Tested on

1. [ESP32 ESP-IDF v5.5.3](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32/index.html)

## SAST Tools

[PVS-Studio](https://pvs-studio.com/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.

## Using

In an existing project, run the following command to install the component:

```text
cd ../your_project/components
git clone https://github.com/aZholtikov/zh_ota_server
```

In the application, add the component:

```c
#include "zh_ota_server.h"
```

## Example

OTA server by address <http://192.168.4.1/ota>

```c
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "zh_ota_server.h"

#define WIFI_SSID "SSID"
#define WIFI_PASS "password"

static httpd_handle_t http_server_handle = NULL;

void app_main(void)
{
    esp_log_level_set("zh_ota_server", ESP_LOG_ERROR);
    nvs_flash_init();
    esp_event_loop_create_default();
    esp_netif_init();
    esp_netif_create_default_wifi_ap();
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
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&http_server_handle, &config);
    zh_ota_server_init(http_server_handle, "/ota");
}
```
