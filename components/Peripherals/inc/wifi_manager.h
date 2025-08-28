/**
 * @file wifi_manager.h
 * @brief WiFi Manager for ESP32 - Simplified WiFi operations
 * @author Gemini
 * @date 2024
 */
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h" // 添加缺失的头文件
#include <stdbool.h>
#include <stdint.h>

// WiFi状态枚举
typedef enum {
    WIFI_STATE_DISABLED,
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
} wifi_manager_state_t;

// WiFi信息结构体
typedef struct {
    wifi_manager_state_t state;
    char ip_addr[16];
    uint8_t mac_addr[6];
    char ssid[33];       // 当前连接的SSID
} wifi_manager_info_t;

// WiFi状态变化时的回调函数类型
typedef void (*wifi_manager_event_cb_t)(void);

/**
 * @brief 初始化WiFi管理器
 * @param event_cb 状态变化时的回调函数, 可为NULL
 * @return esp_err_t
 */
esp_err_t wifi_manager_init(wifi_manager_event_cb_t event_cb);

/**
 * @brief 启动WiFi连接 (STA模式)
 * @note 请在 KConfig 中配置好 SSID 和密码 (CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD)
 * @return esp_err_t
 */
esp_err_t wifi_manager_start(void);

/**
 * @brief 停止WiFi
 * @return esp_err_t
 */
esp_err_t wifi_manager_stop(void);

/**
 * @brief 设置WiFi发射功率
 * @param power_dbm 发射功率, 单位dBm (范围 2 to 20)
 * @return esp_err_t
 */
esp_err_t wifi_manager_set_power(int8_t power_dbm);

/**
 * @brief 获取WiFi发射功率
 * @param power_dbm 指向int8_t的指针，用于存储获取到的功率值 (单位dBm)
 * @return esp_err_t
 */
esp_err_t wifi_manager_get_power(int8_t* power_dbm);

/**
 * @brief 获取当前的WiFi信息
 * @return wifi_manager_info_t 包含当前状态、IP和MAC地址的结构体
 */
wifi_manager_info_t wifi_manager_get_info(void);

/**
 * @brief 启动时间同步
 */
void wifi_manager_sync_time(void);

/**
 * @brief 获取当前时间字符串
 * @param time_str 输出缓冲区
 * @param max_len 缓冲区最大长度
 * @return 是否成功获取时间
 */
bool wifi_manager_get_time_str(char* time_str, size_t max_len);

/**
 * @brief 添加WiFi到列表
 * @param ssid WiFi名称
 * @param password WiFi密码
 * @return void
 */
static void add_wifi_to_list(const char* ssid, const char* password);

/**
 * @brief 从NVS加载WiFi列表
 *
 */
static void load_wifi_list_from_nvs();

/**
 * @brief 获取已保存的WiFi列表大小
 * @return int32_t WiFi数量
 */
int32_t wifi_manager_get_wifi_list_size(void);

/**
 * @brief 根据索引获取WiFi的SSID
 * @param index 索引
 * @return const char* SSID字符串, 如果索引无效则返回NULL
 */
const char* wifi_manager_get_wifi_ssid_by_index(int32_t index);

/**
 * @brief 连接到指定索引的WiFi
 * @param index 要连接的WiFi在列表中的索引
 * @return esp_err_t
 */
esp_err_t wifi_manager_connect_to_index(int32_t index);

/**
 * @brief 获取已保存的WiFi列表大小
 * @return int32_t WiFi数量
 */
int32_t wifi_manager_get_wifi_list_size(void);

/**
 * @brief 根据索引获取WiFi的SSID
 * @param index 索引
 * @return const char* SSID字符串, 如果索引无效则返回NULL
 */
const char* wifi_manager_get_wifi_ssid_by_index(int32_t index);

/**
 * @brief 连接到指定索引的WiFi
 * @param index 要连接的WiFi在列表中的索引
 * @return esp_err_t
 */
esp_err_t wifi_manager_connect_to_index(int32_t index);

/**
 * @brief 获取已保存的WiFi列表大小
 * @return int32_t WiFi数量
 */
int32_t wifi_manager_get_wifi_list_size(void);

/**
 * @brief 根据索引获取WiFi的SSID
 * @param index 索引
 * @return const char* SSID字符串, 如果索引无效则返回NULL
 */
const char* wifi_manager_get_wifi_ssid_by_index(int32_t index);

/**
 * @brief 连接到指定索引的WiFi
 * @param index 要连接的WiFi在列表中的索引
 * @return esp_err_t
 */
esp_err_t wifi_manager_connect_to_index(int32_t index);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H