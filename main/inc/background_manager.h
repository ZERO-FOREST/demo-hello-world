/**
 * @file background_manager.h
 * @brief 后台管理模块 - 管理时间和电池电量的后台更新
 * @author TidyCraze
 * @date 2025-01-27
 */

#ifndef BACKGROUND_MANAGER_H
#define BACKGROUND_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// 时间信息结构体
typedef struct {
    uint8_t hour;
    uint8_t minute;
    bool is_network_synced;  // 是否为网络同步时间
    bool is_valid;           // 时间是否有效
} background_time_info_t;

// 电池信息结构体
typedef struct {
    int voltage_mv;
    int percentage;
    bool is_low_battery;
    bool is_critical;
    bool is_valid;
} background_battery_info_t;

// 系统状态结构体
typedef struct {
    background_time_info_t time;
    background_battery_info_t battery;
    bool wifi_connected;
    char ip_addr[16];
} background_system_info_t;

/**
 * @brief 初始化后台管理模块
 * @return esp_err_t
 */
esp_err_t background_manager_init(void);

/**
 * @brief 反初始化后台管理模块
 * @return esp_err_t
 */
esp_err_t background_manager_deinit(void);

/**
 * @brief 启动后台管理任务
 * @return esp_err_t
 */
esp_err_t background_manager_start(void);

/**
 * @brief 停止后台管理任务
 * @return esp_err_t
 */
esp_err_t background_manager_stop(void);

/**
 * @brief 获取当前时间信息
 * @param time_info 输出时间信息
 * @return esp_err_t
 */
esp_err_t background_manager_get_time(background_time_info_t* time_info);

/**
 * @brief 获取当前电池信息
 * @param battery_info 输出电池信息
 * @return esp_err_t
 */
esp_err_t background_manager_get_battery(background_battery_info_t* battery_info);

/**
 * @brief 获取完整的系统信息
 * @param system_info 输出系统信息
 * @return esp_err_t
 */
esp_err_t background_manager_get_system_info(background_system_info_t* system_info);

/**
 * @brief 获取时间字符串（格式化，只显示小时:分钟）
 * @param time_str 输出缓冲区
 * @param max_len 缓冲区最大长度
 * @return esp_err_t
 */
esp_err_t background_manager_get_time_str(char* time_str, size_t max_len);

/**
 * @brief 获取电池电量字符串（格式化）
 * @param battery_str 输出缓冲区
 * @param max_len 缓冲区最大长度
 * @return esp_err_t
 */
esp_err_t background_manager_get_battery_str(char* battery_str, size_t max_len);

/**
 * @brief 检查时间是否需要更新显示
 * @return true - 需要更新, false - 不需要更新
 */
bool background_manager_is_time_changed(void);

/**
 * @brief 检查电池是否需要更新显示
 * @return true - 需要更新, false - 不需要更新
 */
bool background_manager_is_battery_changed(void);

/**
 * @brief 标记时间已更新显示
 */
void background_manager_mark_time_displayed(void);

/**
 * @brief 标记电池已更新显示
 */
void background_manager_mark_battery_displayed(void);

#ifdef __cplusplus
}
#endif

#endif // BACKGROUND_MANAGER_H
