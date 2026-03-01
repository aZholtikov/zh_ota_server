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

static char *_ota_path = NULL;
static char *_ota_ws_path = "/zh_ota_server";

static esp_err_t _ota_page_handler(httpd_req_t *req);
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
    _ota_path = (char *)heap_caps_calloc(1, strlen(path), MALLOC_CAP_8BIT);
    strcpy(_ota_path, path);
    _ota_page.uri = _ota_path;
    _ota_page_ws.uri = _ota_ws_path;
    esp_err_t err = httpd_register_uri_handler(server, &_ota_page);
    ZH_ERROR_CHECK(err == ESP_OK, err, NULL, "OTA server initialization failed. Register uri handler failed.");
    err = httpd_register_uri_handler(server, &_ota_page_ws);
    ZH_ERROR_CHECK(err == ESP_OK, err, NULL, "OTA server initialization failed. Register uri handler failed.");
    ZH_LOGI("OTA server initialization completed successfully.");
    return ESP_OK;
}

esp_err_t _ota_page_handler(httpd_req_t *req)
{
    esp_err_t err = httpd_resp_send(req, (const char *)zh_ota_server_html_start, zh_ota_server_html_end - zh_ota_server_html_start);
    ZH_ERROR_CHECK(err == ESP_OK, err, NULL, "HTTP server internal error.");
    return ESP_OK;
}

esp_err_t _ota_ws_page_handler(httpd_req_t *req) // -V2008
{
    char buf[1000] = {0};
    esp_ota_handle_t ota_handle = {0};
    int remaining = req->content_len;
    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    ZH_ERROR_CHECK(ota_partition != NULL, ESP_FAIL, NULL, "OTA get next update partition failed.");
    esp_err_t err = esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    ZH_ERROR_CHECK(err == ESP_OK, err, NULL, "OTA begin failed.");
    while (remaining > 0)
    {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
        {
            continue;
        }
        else if (recv_len <= 0)
        {
            err = httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
            ZH_ERROR_CHECK(err == ESP_OK, err, NULL, "HTTP server internal error.");
            return ESP_FAIL;
        }
        err = esp_ota_write(ota_handle, (const void *)buf, recv_len);
        ZH_ERROR_CHECK(err == ESP_OK, err, err = httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash error.");
                       ZH_ERROR_CHECK(err == ESP_OK, err, NULL, "HTTP server internal error."), "OTA write failed.");
        remaining -= recv_len;
    }
    err = esp_ota_end(ota_handle);
    ZH_ERROR_CHECK(err == ESP_OK, err, err = httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error.");
                   ZH_ERROR_CHECK(err == ESP_OK, err, NULL, "HTTP server internal error."), "OTA end failed.");
    err = esp_ota_set_boot_partition(ota_partition);
    ZH_ERROR_CHECK(err == ESP_OK, err, err = httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error.");
                   ZH_ERROR_CHECK(err == ESP_OK, err, NULL, "HTTP server internal error."), "OTA set boot partition failed.");
    err = httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");
    ZH_ERROR_CHECK(err == ESP_OK, err, NULL, "HTTP server internal error.");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    esp_restart();
    return ESP_OK;
}