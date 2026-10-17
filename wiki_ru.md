# Описание компонента zh_ota_server

## Общие сведения

**zh_ota_server** — это компонент для ESP-IDF, реализующий сервер обновлений прошивки по воздуху (Over-The-Air, OTA) для микроконтроллеров ESP32.

Компонент предоставляет веб-интерфейс для загрузки бинарных файлов прошивки через браузер и автоматически записывает их в свободный OTA-раздел ESP32 с последующей активацией и перезагрузкой устройства.

## Основные возможности

1. **Веб-интерфейс для загрузки** — встроенная HTML-страница для выбора и отправки файла прошивки через браузер.
2. **Автоматическое определение раздела** — компонент сам находит следующий доступный OTA-раздел через API ESP-IDF.
3. **Построчная запись прошивки** — данные принимаются блоками по 1000 байт для экономии оперативной памяти.
4. **Автоматическая активация и перезагрузка** — после успешной записи прошивки раздел становится главным, и устройство перезагружается.
5. **Интеграция с ESP-IDF HTTP server** — компонент использует встроенный HTTP-сервер ESP-IDF и регистрирует свои обработчики как стандартные URI-хендлеры.
6. **Встроенная HTML-страница** — веб-интерфейс хранится как встроенный массив байт (EMBED_FILES), не требуя внешней файловой системы.
7. **Детальное логирование ошибок** — при ошибках логируются файл, строка и код возврата ESP-IDF.

## Внутреннее устройство

### Глобальные переменные

| Переменная | Тип | Описание |
|------------|-----|----------|
| `_ota_path` | `char *` | Выделенная динамически память для URI-пути OTA-сервера (не освобождается) |
| `_ota_ws_path` | `const char *` | URI-путь для POST-обработчика (`"/zh_ota_server"`) |
| `_is_initialized` | `volatile bool` | Флаг инициализации компонента, предотвращающий повторный вызов `zh_ota_server_init()` |

### Структуры обработчиков

| Поле | Тип | Описание |
|------|-----|----------|
| `_ota_page.uri` | `char *` | URI-путь для GET-запроса (устанавливается при инициализации) |
| `_ota_page.method` | `httpd_method_t` | HTTP-метод: `HTTP_GET` |
| `_ota_page.handler` | `httpd_req_handler_t` | Указатель на `_ota_page_handler()` |
| `_ota_page_ws.uri` | `char *` | URI-путь для POST-запроса: `"/zh_ota_server"` |
| `_ota_page_ws.method` | `httpd_method_t` | HTTP-метод: `HTTP_POST` |
| `_ota_page_ws.handler` | `httpd_req_handler_t` | Указатель на `_ota_ws_page_handler()` |

### Встроенные ресурсы

HTML-страница `zh_ota_server.html` встраивается в прошивку через механизм `EMBED_FILES` CMake. Доступна через внешние символы:

| Символ | Описание |
|--------|----------|
| `zh_ota_server_html_start[]` | Начало встроенного HTML-массива |
| `zh_ota_server_html_end[]` | Конец встроенного HTML-массива |

Размер страницы вычисляется как `zh_ota_server_html_end - zh_ota_server_html_start`.

### Управление памятью

При инициализации `zh_ota_path` выделяется через `heap_caps_calloc()` с флагом `MALLOC_CAP_8BIT`. Память **не освобождается** никогда — это осознанный выбор, так как функция инициализации может быть вызвана только один раз за время работы приложения.

## API (интерфейс)

### Инициализация

| Функция | Описание |
|---------|----------|
| `zh_ota_server_init()` | Инициализирует и регистрирует обработчики OTA-сервера в предоставленном экземпляре HTTP-сервера. Принимает handle сервера (`httpd_handle_t`) и URI-путь (например, `"/ota"`). Возвращает `ESP_OK` при успехе, `ESP_ERR_INVALID_ARG` при некорректных аргументах, `ESP_ERR_NO_MEM` при нехватке памяти, `ESP_FAIL` при ошибке регистрации обработчика. |

## Потокобезопасность

Компонент **не является потокобезопасным**. Глобальные переменные `_ota_path` и `_is_initialized` защищены только проверкой на повторную инициализацию (`_is_initialized == false`), но не мьютексами. Обработчики HTTP-запросов вызываются в контексте потока HTTP-сервера и не используют механизмы синхронизации.

## Алгоритмы

### Обработка OTA-обновления (`_ota_ws_page_handler`)

1. Выделение буфера размером **1000 байт** для приёма данных.
2. Определение следующего доступного OTA-раздела через `esp_ota_get_next_update_partition(NULL)`.
3. Начало записи в раздел через `esp_ota_begin()` с размером `OTA_SIZE_UNKNOWN`.
4. **Цикл приёма данных:**
   - Приём данных из HTTP-запроса через `httpd_req_recv()` блоками до `sizeof(buf)` (1000 байт).
   - При таймауте сокета (`HTTPD_SOCK_ERR_TIMEOUT`) цикл продолжается без действия.
   - При ошибке приёма отправляется HTTP 500 и вызывается `esp_ota_abort()`.
   - Запись данных в flash через `esp_ota_write()`; при ошибке отправляется HTTP 500 и вызывается `esp_ota_abort()`.
   - Уменьшение счётчика оставшихся байт (`remaining -= recv_len`).
5. Завершение записи через `esp_ota_end()` (валидация образа).
6. Установка раздела как загрузочного через `esp_ota_set_boot_partition()`.
7. Отправка HTTP-ответа `"Firmware update complete, rebooting now!\n"`.
8. Задержка **500 мс** через `vTaskDelay()` и вызов `esp_restart()` для перезагрузки.

## Примеры использования

### Базовый пример

```c
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "zh_ota_server.h"

static httpd_handle_t http_server_handle = NULL;

void app_main(void)
{
    esp_log_level_set("zh_ota_server", ESP_LOG_ERROR);
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_run(NULL, NULL);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&http_server_handle, &config);

    zh_ota_server_init(http_server_handle, "/ota");
}
```

### Пример с Wi-Fi в режиме точки доступа

```c
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "zh_ota_server.h"

#define WIFI_SSID "ESP32-OTA"
#define WIFI_PASS "password123"

static httpd_handle_t http_server_handle = NULL;

void app_main(void)
{
    esp_log_level_set("zh_ota_server", ESP_LOG_DEBUG);
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

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

## Логирование

Компонент использует ESP-IDF логирование с тегом `"zh_ota_server"`.

Макрос `ZH_LOGI` — для информационных сообщений. Макрос `ZH_LOGE` — для ошибок, автоматически добавляет имя файла, номер строки и строковое представление кода ошибки ESP-IDF.

```c
esp_log_level_set("zh_ota_server", ESP_LOG_DEBUG);
```

## Ошибки

| Код | Описание |
|-----|----------|
| `ESP_OK` | Успешное выполнение |
| `ESP_ERR_INVALID_ARG` | Неверный аргумент: NULL-указатель на сервер или путь |
| `ESP_ERR_INVALID_STATE` | Повторная инициализация: компонент уже был инициализирован |
| `ESP_ERR_NO_MEM` | Нехватка памяти при выделении для URI-пути |
| `ESP_FAIL` | Ошибка регистрации URI-обработчика, записи в flash, завершения OTA или установки загрузочного раздела |

## Ограничения

1. **Однократная инициализация** — `zh_ota_server_init()` можно вызвать только один раз за время работы приложения. Повторный вызов вернёт `ESP_ERR_INVALID_STATE`.
2. **Утечка памяти URI-пути** — память, выделенная для `_ota_path`, никогда не освобождается.
3. **Риск «окирпичивания» устройства** — запись невалидного образа прошивки сделает устройство не загружаемым. Необходимо иметь механизм восстановления (например, USB-прошивку).
4. **Отсутствие потокобезопасности** — компонент не защищён мьютексами; одновременный доступ из нескольких потоков может привести к неопределённому поведению.
5. **Размер буфера 1000 байт** — фиксированный размер буфера приёма может быть недостаточен для оптимальной производительности при больших объёмах данных.
6. **Отсутствие контрольной суммы** — валидация образа выполняется только через `esp_ota_end()`, что не гарантирует корректность прошивки на уровне содержимого.

## Требования для сборки

- ESP-IDF (фреймворк для ESP32)
- `esp_http_server` — встроенный HTTP-сервер ESP-IDF
- `app_update` — модуль ESP-IDF для управления OTA-разделами и загрузкой
- FreeRTOS (входит в состав ESP-IDF)

## Установка

```bash
cd ваш_проект/components
git clone https://github.com/aZholtikov/zh_ota_server
```

```c
#include "zh_ota_server.h"
```
