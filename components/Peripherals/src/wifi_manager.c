#include "wifi_manager.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const char* TAG = "WIFI_MANAGER";

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static int s_retry_num = 0;

// 全局变量来保存WiFi信息和回调
static wifi_manager_info_t g_wifi_info;
static wifi_manager_event_cb_t g_event_cb = NULL;

#define WIFI_NVS_NAMESPACE "wifi_config"
#define WIFI_NVS_KEY_SSID "ssid"
#define WIFI_NVS_KEY_PASSWORD "password"

#define MAX_WIFI_LIST_SIZE 5

typedef struct {
    char ssid[32];
    char password[64];
} wifi_config_entry_t;

static wifi_config_entry_t wifi_list[MAX_WIFI_LIST_SIZE];
static int32_t wifi_list_size = 0;

// 前向声明
static bool load_wifi_config_from_nvs(char* ssid, size_t ssid_len, char* password, size_t password_len);
static void save_wifi_config_to_nvs(const char* ssid, const char* password);
static void save_wifi_list_to_nvs(void);
static void load_wifi_list_from_nvs(void);
static void add_wifi_to_list(const char* ssid, const char* password);

/**
 * @brief WiFi事件处理器
 */
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        g_wifi_info.state = WIFI_STATE_CONNECTING;
        esp_wifi_connect();
        ESP_LOGI(TAG, "STA Start, connecting to AP...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        g_wifi_info.state = WIFI_STATE_DISCONNECTED;
        strcpy(g_wifi_info.ip_addr, "N/A");

        if (s_retry_num < 5) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP (%d/5)", s_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Connect to the AP fail");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        snprintf(g_wifi_info.ip_addr, sizeof(g_wifi_info.ip_addr), IPSTR, IP2STR(&event->ip_info.ip));
        g_wifi_info.state = WIFI_STATE_CONNECTED;
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Got IP address: %s", g_wifi_info.ip_addr);

        // Add connected WiFi to the list
        add_wifi_to_list("TidyC", "22989822");
        add_wifi_to_list("Sysware-HP", "12345678");
        add_wifi_to_list("tidy", "22989822");
        add_wifi_to_list("Xiaomi", "22989822");

        ESP_LOGI(TAG, "Starting time synchronization...");
        wifi_manager_sync_time();

        // 等待时间同步完成
        int retry = 0;
        const int max_retry = 10;
        while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && retry < max_retry) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            retry++;
            ESP_LOGI(TAG, "Waiting for time sync... (%d/%d)", retry, max_retry);
        }

        if (retry < max_retry) {
            ESP_LOGI(TAG, "Time synchronized successfully!");
        } else {
            ESP_LOGW(TAG, "Time sync timeout!");
        }
    }

    // 如果设置了回调函数，则调用它
    if (g_event_cb) {
        g_event_cb();
    }
}

/**
 * @brief 初始化WiFi底层 (NVS, Netif, Event Loop)
 */
static esp_err_t wifi_init_stack(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 初始化WiFi驱动
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    // 注册事件处理器
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));
    return ESP_OK;
}

esp_err_t wifi_manager_init(wifi_manager_event_cb_t event_cb) {
    g_event_cb = event_cb;
    memset(&g_wifi_info, 0, sizeof(g_wifi_info));
    g_wifi_info.state = WIFI_STATE_DISABLED;
    strcpy(g_wifi_info.ip_addr, "N/A");

    // 先初始化WiFi底层
    esp_err_t ret = wifi_init_stack();
    if (ret != ESP_OK) {
        return ret;
    }

    // 获取MAC地址
    ret = esp_wifi_get_mac(ESP_IF_WIFI_STA, g_wifi_info.mac_addr);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get MAC address: %s", esp_err_to_name(ret));
    }

    return ESP_OK;
}

esp_err_t wifi_manager_start(void) {
    s_wifi_event_group = xEventGroupCreate();

    wifi_config_t wifi_config = {
        .sta =
            {
                // 连接到用户指定的WiFi
                .ssid = "TidyC",
                .password = "22989822",
                /* 设置最低安全等级为WPA/WPA2 PSK */
                .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
            },
    };

    char ssid[32] = {0};
    char password[64] = {0};
    if (load_wifi_config_from_nvs(ssid, sizeof(ssid), password, sizeof(password))) {
        strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    } else {
        strncpy((char*)wifi_config.sta.ssid, "TidyC", sizeof(wifi_config.sta.ssid));
        strncpy((char*)wifi_config.sta.password, "22989822", sizeof(wifi_config.sta.password));
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // WiFi启动后设置发射功率为8dBm（较为适中的功率）
    esp_err_t power_ret = esp_wifi_set_max_tx_power(32); // 8dBm = 32/4
    if (power_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set WiFi power: %s", esp_err_to_name(power_ret));
    }

    ESP_LOGI(TAG, "wifi_manager_start finished.");

    load_wifi_list_from_nvs();
    for (int i = 0; i < wifi_list_size; i++) {
        strncpy((char*)wifi_config.sta.ssid, wifi_list[i].ssid, sizeof(wifi_config.sta.ssid));
        strncpy((char*)wifi_config.sta.password, wifi_list[i].password, sizeof(wifi_config.sta.password));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
                                               pdMS_TO_TICKS(10000));
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Connected to WiFi: %s", wifi_list[i].ssid);
            break;
        } else {
            ESP_LOGW(TAG, "Failed to connect to WiFi: %s", wifi_list[i].ssid);
        }
    }

    return ESP_OK;
}

esp_err_t wifi_manager_stop(void) {
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_OK) {
        g_wifi_info.state = WIFI_STATE_DISABLED;
        strcpy(g_wifi_info.ip_addr, "N/A");
        if (g_event_cb)
            g_event_cb();
    }
    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;
    ESP_LOGI(TAG, "WiFi stopped.");
    return err;
}

esp_err_t wifi_manager_set_power(int8_t power_dbm) {
    if (power_dbm < 2 || power_dbm > 20) {
        return ESP_ERR_INVALID_ARG;
    }
    // ESP-IDF 内部使用 0.25dBm 为单位, 8 -> 2dBm, 80 -> 20dBm
    int8_t power_val = (int8_t)(power_dbm * 4);
    esp_err_t err = esp_wifi_set_max_tx_power(power_val);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi Tx Power set to %d dBm", power_dbm);
    }
    return err;
}

wifi_manager_info_t wifi_manager_get_info(void) { return g_wifi_info; }

/**
 * @brief 时间同步回调函数
 */
static void time_sync_notification_cb(struct timeval* tv) { ESP_LOGI(TAG, "Time synchronized!"); }

/**
 * @brief 启动时间同步
 */
void wifi_manager_sync_time(void) {
    ESP_LOGI(TAG, "Initializing SNTP time sync...");

    // 设置时区为北京时间 (UTC+8)
    setenv("TZ", "CST-8", 1);
    tzset();

    // 配置SNTP
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com"); // 阿里云NTP服务器
    esp_sntp_setservername(1, "ntp1.aliyun.com");
    esp_sntp_setservername(2, "ntp2.aliyun.com");

    // 设置更新间隔（15分钟）
    esp_sntp_set_sync_interval(900000);

    // 设置时间同步回调
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    // 启动SNTP
    esp_sntp_init();

    ESP_LOGI(TAG, "SNTP time sync started with Aliyun NTP servers");
}

/**
 * @brief 获取当前时间字符串
 * @param time_str 输出缓冲区
 * @param max_len 缓冲区最大长度
 * @return 是否成功获取时间
 */
bool wifi_manager_get_time_str(char* time_str, size_t max_len) {
    time_t now = 0;
    struct tm timeinfo = {0};

    // 获取当前时间
    time(&now);
    localtime_r(&now, &timeinfo);

    // 检查时间是否已同步（年份应该大于2020）
    if (timeinfo.tm_year < (2020 - 1900)) {
        snprintf(time_str, max_len, "Syncing...");
        return false;
    }

    // 格式化时间字符串：只显示时间 HH:MM
    strftime(time_str, max_len, "%H:%M", &timeinfo);
    return true;
}

static void save_wifi_config_to_nvs(const char* ssid, const char* password) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_str(nvs_handle, WIFI_NVS_KEY_SSID, ssid);
        nvs_set_str(nvs_handle, WIFI_NVS_KEY_PASSWORD, password);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "WiFi config saved to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
    }
}

static bool load_wifi_config_from_nvs(char* ssid, size_t ssid_len, char* password, size_t password_len) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        err = nvs_get_str(nvs_handle, WIFI_NVS_KEY_SSID, ssid, &ssid_len);
        if (err == ESP_OK) {
            err = nvs_get_str(nvs_handle, WIFI_NVS_KEY_PASSWORD, password, &password_len);
            if (err == ESP_OK) {
                nvs_close(nvs_handle);
                ESP_LOGI(TAG, "WiFi config loaded from NVS");
                return true;
            }
        }
        nvs_close(nvs_handle);
    }
    ESP_LOGW(TAG, "No WiFi config found in NVS");
    return false;
}

static void save_wifi_list_to_nvs() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_blob(nvs_handle, "wifi_list", wifi_list, sizeof(wifi_list));
        nvs_set_i32(nvs_handle, "wifi_list_size", wifi_list_size);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "WiFi list saved to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for saving WiFi list: %s", esp_err_to_name(err));
    }
}

static void load_wifi_list_from_nvs() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t required_size = sizeof(wifi_list);
        err = nvs_get_blob(nvs_handle, "wifi_list", wifi_list, &required_size);
        if (err == ESP_OK) {
            nvs_get_i32(nvs_handle, "wifi_list_size", &wifi_list_size);
            ESP_LOGI(TAG, "WiFi list loaded from NVS");
        } else {
            ESP_LOGW(TAG, "No WiFi list found in NVS");
        }
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for loading WiFi list: %s", esp_err_to_name(err));
    }
}

static void add_wifi_to_list(const char* ssid, const char* password) {
    if (wifi_list_size < MAX_WIFI_LIST_SIZE) {
        strncpy(wifi_list[wifi_list_size].ssid, ssid, sizeof(wifi_list[wifi_list_size].ssid));
        strncpy(wifi_list[wifi_list_size].password, password, sizeof(wifi_list[wifi_list_size].password));
        wifi_list_size++;
        save_wifi_list_to_nvs();
        ESP_LOGI(TAG, "WiFi added to list: %s", ssid);
    } else {
        ESP_LOGW(TAG, "WiFi list is full, cannot add: %s", ssid);
    }
}