/**
 * @file zh_ota_server.h
 *
 * @brief Over-the-Air (OTA) firmware update server component for ESP-IDF.
 *
 * Provides an HTTP-based OTA server component for ESP32 microcontrollers.
 * The server serves a web interface via HTTP GET and accepts firmware
 * binary updates via HTTP POST. The incoming firmware is written to the
 * next available OTA partition and activated upon successful completion.
 *
 * Key features:
 * - Web-based firmware upload interface
 * - Automatic partition detection and flashing
 * - Automatic reboot and boot partition activation
 * - ESP-IDF HTTP server integration
 *
 * @note This component requires esp_http_server and esp_ota_ops to be
 *       included in the project.
 * @warning Flashing an invalid firmware image may brick the device.
 *          Always ensure a valid fallback mechanism is in place.
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
     * @brief Initialize and register the OTA server.
     *
     * Starts the OTA server initialization process, allocates memory for
     * the URI path, and registers the HTTP GET (web interface) and HTTP POST
     * (firmware upload) handlers with the provided HTTP server instance.
     *
     * @param[in] server HTTP server handle obtained from httpd_start() (must not be NULL)
     * @param[in] path URI path for the OTA server (e.g., "/ota")
     *
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_ARG if invalid server handle or path
     * @return ESP_ERR_NO_MEM if memory allocation failed
     * @return ESP_FAIL if failed to register URI handler
     *
     * @note The path string is copied internally and must not be freed
     *       by the caller.
     * @warning The memory allocated for the path is never freed.
     *          Call this function only once during application lifetime.
     */
    esp_err_t zh_ota_server_init(httpd_handle_t server, const char *path);

#ifdef __cplusplus
}
#endif