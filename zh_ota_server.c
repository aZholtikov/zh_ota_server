#include "zh_ota_server.h"

static const char *TAG = "zh_ota_server";

#define ZH_LOGI(msg, ...) ESP_LOGI(TAG, msg, ##__VA_ARGS__)
#define ZH_LOGE(msg, err, ...) ESP_LOGE(TAG, "[%s:%d:%s] " msg, __FILE__, __LINE__, esp_err_to_name(err), ##__VA_ARGS__)

#define ZH_ERROR_CHECK(cond, err, cleanup, msg, ...) \
    if (!(cond))                                     \
    {                                                \
        ZH_LOGE(msg, err, ##__VA_ARGS__);            \
        cleanup;                                     \
        return err;                                  \
    }

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

static char *_ota_path = NULL;                /*!< Internal storage for the OTA server URI path */
static char *_ota_ws_path = "/zh_ota_server"; /*!< Fixed URI path for WebSocket-style firmware upload */

/**
 * @brief HTTP GET handler for the OTA web interface.
 *
 * Serves the embedded HTML page for firmware upload via HTTP response.
 *
 * @param req HTTP request structure
 *
 * @return ESP_OK on success
 * @return ESP_FAIL if failed to send the response
 */
static esp_err_t _ota_page_handler(httpd_req_t *req);

/**
 * @brief HTTP POST handler for firmware upload via OTA.
 *
 * Receives firmware binary data via HTTP POST, writes it to the next
 * available OTA partition, and activates it as the new boot partition.
 * The device reboots automatically after a successful update.
 *
 * @param req HTTP request containing the firmware binary payload
 *
 * @return ESP_OK on success
 * @return ESP_FAIL if OTA operations or HTTP response failed
 *
 * @note The firmware size is taken from the request Content-Length header.
 * @warning An invalid firmware image will render the device unbootable
 *          until a valid image is flashed via another mechanism.
 */
static esp_err_t _ota_ws_page_handler(httpd_req_t *req);

extern const uint8_t zh_ota_server_html_start[] asm("_binary_zh_ota_server_html_start");
extern const uint8_t zh_ota_server_html_end[] asm("_binary_zh_ota_server_html_end");

httpd_uri_t _ota_page = {
    .uri = NULL,
    .method = HTTP_GET,
    .handler = _ota_page_handler,
    .user_ctx = NULL};

httpd_uri_t _ota_page_ws = {
    .uri = NULL,
    .method = HTTP_POST,
    .handler = _ota_ws_page_handler,
    .user_ctx = NULL};

esp_err_t zh_ota_server_init(httpd_handle_t server, const char *path)
{
    ZH_LOGI("OTA server initialization started.");
    ZH_ERROR_CHECK(server != NULL && path != NULL, ESP_ERR_INVALID_ARG, NULL, "OTA server initialization failed. Invalid argument.");
    _ota_path = (char *)heap_caps_calloc(1, strlen(path) + 1, MALLOC_CAP_8BIT);
    ZH_ERROR_CHECK(_ota_path != NULL, ESP_ERR_NO_MEM, NULL, "OTA server initialization failed. Memory allocation fail or no free memory in the heap.");
    strcpy(_ota_path, path);
    _ota_page.uri = _ota_path;
    _ota_page_ws.uri = _ota_ws_path;
    ZH_ERROR_CHECK(httpd_register_uri_handler(server, &_ota_page) == ESP_OK, ESP_FAIL, heap_caps_free(_ota_path), "OTA server initialization failed. Register uri handler failed.");
    ZH_ERROR_CHECK(httpd_register_uri_handler(server, &_ota_page_ws) == ESP_OK, ESP_FAIL, heap_caps_free(_ota_path), "OTA server initialization failed. Register uri handler failed.");
    ZH_LOGI("OTA server initialization completed successfully.");
    return ESP_OK;
}

esp_err_t _ota_page_handler(httpd_req_t *req)
{
    ZH_ERROR_CHECK(httpd_resp_send(req, (const char *)zh_ota_server_html_start, zh_ota_server_html_end - zh_ota_server_html_start) == ESP_OK, ESP_FAIL, NULL, "HTTP server internal error.");
    return ESP_OK;
}

esp_err_t _ota_ws_page_handler(httpd_req_t *req) // -V2008
{
    char buf[1000] = {0};
    esp_ota_handle_t ota_handle = {0};
    int remaining = req->content_len;
    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    ZH_ERROR_CHECK(ota_partition != NULL, ESP_FAIL, NULL, "OTA get next update partition failed.");
    ZH_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle) == ESP_OK, ESP_FAIL, NULL, "OTA begin failed.");
    while (remaining > 0)
    {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
        {
            continue;
        }
        else if (recv_len <= 0)
        {
            ZH_ERROR_CHECK(httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error") == ESP_OK, ESP_FAIL, NULL, "HTTP server internal error.");
            return ESP_FAIL;
        }
        ZH_ERROR_CHECK(esp_ota_write(ota_handle, (const void *)buf, recv_len) == ESP_OK, ESP_FAIL,
                       {ZH_ERROR_CHECK(httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash error.") == ESP_OK, ESP_FAIL, NULL, "HTTP server internal error.")}, "OTA write failed.");
        remaining -= recv_len;
    }
    ZH_ERROR_CHECK(esp_ota_end(ota_handle) == ESP_OK, ESP_FAIL,
                   {ZH_ERROR_CHECK(httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error.") == ESP_OK, ESP_FAIL, NULL, "HTTP server internal error.")}, "OTA end failed.");
    ZH_ERROR_CHECK(esp_ota_set_boot_partition(ota_partition) == ESP_OK, ESP_FAIL,
                   {ZH_ERROR_CHECK(httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error.") == ESP_OK, ESP_FAIL, NULL, "HTTP server internal error.")}, "OTA set boot partition failed.");
    ZH_ERROR_CHECK(httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n") == ESP_OK, ESP_FAIL, NULL, "HTTP server internal error.");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    esp_restart();
    return ESP_OK;
}