#include "wifi_manager.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "WIFI_MANAGER";

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

// 全局变量来保存WiFi信息和回调
static wifi_manager_info_t g_wifi_info;
static wifi_manager_event_cb_t g_event_cb = NULL;

/**
 * @brief WiFi事件处理器
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
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
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        snprintf(g_wifi_info.ip_addr, sizeof(g_wifi_info.ip_addr), IPSTR, IP2STR(&event->ip_info.ip));
        g_wifi_info.state = WIFI_STATE_CONNECTED;
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Got IP address: %s", g_wifi_info.ip_addr);
    }

    // 如果设置了回调函数，则调用它
    if (g_event_cb) {
        g_event_cb();
    }
}

/**
 * @brief 初始化WiFi底层 (NVS, Netif, Event Loop)
 */
static esp_err_t wifi_init_stack(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册事件处理器
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    return ESP_OK;
}


esp_err_t wifi_manager_init(wifi_manager_event_cb_t event_cb)
{
    g_event_cb = event_cb;
    memset(&g_wifi_info, 0, sizeof(g_wifi_info));
    g_wifi_info.state = WIFI_STATE_DISABLED;
    strcpy(g_wifi_info.ip_addr, "N/A");
    
    // 获取MAC地址
    ESP_ERROR_CHECK(esp_wifi_get_mac(ESP_IF_WIFI_STA, g_wifi_info.mac_addr));

    return wifi_init_stack();
}

esp_err_t wifi_manager_start(void)
{
    s_wifi_event_group = xEventGroupCreate();
    
    wifi_config_t wifi_config = {
        .sta = {
            // 在这里填入您的WiFi名称和密码
            // 注意：SSID 和 密码 都必须是C语言字符串（用双引号括起来）
            .ssid = "YOUR_WIFI_SSID",
            .password = "YOUR_WIFI_PASSWORD",
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_manager_start finished.");
    return ESP_OK;
}

esp_err_t wifi_manager_stop(void)
{
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_OK) {
        g_wifi_info.state = WIFI_STATE_DISABLED;
        strcpy(g_wifi_info.ip_addr, "N/A");
        if (g_event_cb) g_event_cb();
    }
    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;
    ESP_LOGI(TAG, "WiFi stopped.");
    return err;
}

esp_err_t wifi_manager_set_power(int8_t power_dbm)
{
    if (power_dbm < 2 || power_dbm > 20) {
        return ESP_ERR_INVALID_ARG;
    }
    // ESP-IDF 内部使用 0.25dBm 为单位, 8 -> 2dBm, 80 -> 20dBm
    int8_t power_val = (int8_t)(power_dbm * 4);
    esp_err_t err = esp_wifi_set_max_tx_power(power_val);
    if(err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi Tx Power set to %d dBm", power_dbm);
    }
    return err;
}

wifi_manager_info_t wifi_manager_get_info(void)
{
    return g_wifi_info;
} 