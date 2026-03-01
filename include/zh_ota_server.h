/**
 * @file zh_ota_server.h
 */

#pragma once

#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Start OTA server.
     *
     * @param[in] server HTTP server handle.
     * @param[in] path OTA server path.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_ota_server_init(httpd_handle_t server, const char *path);

#ifdef __cplusplus
}
#endif