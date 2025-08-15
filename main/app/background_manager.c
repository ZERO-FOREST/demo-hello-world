/**
 * @file background_manager.c
 * @brief 后台管理模块实现 - 管理时间和电池电量的后台更新
 * @author TidyCraze
 * @date 2025-01-27
 */

#include "background_manager.h"
#include "battery_monitor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "wifi_manager.h"
#include <string.h>

static const char* TAG = "BACKGROUND_MANAGER";

// 全局变量
static TaskHandle_t s_background_task_handle = NULL;
static SemaphoreHandle_t s_data_mutex = NULL;
static bool s_initialized = false;
static bool s_task_running = false;

// 数据存储
static background_time_info_t s_current_time = {0};
static background_battery_info_t s_current_battery = {0};
static bool s_time_changed = false;
static bool s_battery_changed = false;

// 时间相关
static uint64_t s_last_time_update = 0;
static uint64_t s_last_battery_update = 0;
static uint64_t s_start_time = 0;

// 后台任务函数
static void background_manager_task(void* pvParameters) {
    ESP_LOGI(TAG, "Background manager task started on core %d", xPortGetCoreID());
    
    // 记录启动时间
    s_start_time = esp_timer_get_time();
    
    while (s_task_running) {
        uint64_t current_time_us = esp_timer_get_time();
        
        // 更新时间（每秒更新一次）
        if (current_time_us - s_last_time_update >= 1000000) { // 1秒 = 1,000,000微秒
            if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                // 首先尝试从网络获取时间
                char time_str[32];
                if (wifi_manager_get_time_str(time_str, sizeof(time_str))) {
                    // 解析网络时间
                    int hour, minute, second = 0;
                    if (sscanf(time_str, "%d:%d:%d", &hour, &minute, &second) >= 2) {
                        s_current_time.hour = hour;
                        s_current_time.minute = minute;
                        s_current_time.second = second;
                        s_current_time.is_network_time = true;
                        s_current_time.is_valid = true;
                        s_time_changed = true;
                        ESP_LOGD(TAG, "Network time updated: %02d:%02d:%02d", hour, minute, second);
                    }
                } else {
                    // 使用本地时间
                    uint64_t elapsed_seconds = (current_time_us - s_start_time) / 1000000;
                    s_current_time.hour = (elapsed_seconds / 3600) % 24;
                    s_current_time.minute = (elapsed_seconds / 60) % 60;
                    s_current_time.second = elapsed_seconds % 60;
                    s_current_time.is_network_time = false;
                    s_current_time.is_valid = true;
                    s_time_changed = true;
                    ESP_LOGD(TAG, "Local time updated: %02d:%02d:%02d", 
                             s_current_time.hour, s_current_time.minute, s_current_time.second);
                }
                s_last_time_update = current_time_us;
                xSemaphoreGive(s_data_mutex);
            }
        }
        
        // 更新电池电量（每5秒更新一次）
        if (current_time_us - s_last_battery_update >= 5000000) { // 5秒 = 5,000,000微秒
            if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                battery_info_t battery_info;
                if (battery_monitor_read(&battery_info) == ESP_OK) {
                    // 检查是否有变化
                    if (s_current_battery.percentage != battery_info.percentage ||
                        s_current_battery.voltage_mv != battery_info.voltage_mv ||
                        s_current_battery.is_low_battery != battery_info.is_low_battery ||
                        s_current_battery.is_critical != battery_info.is_critical) {
                        
                        s_current_battery.voltage_mv = battery_info.voltage_mv;
                        s_current_battery.percentage = battery_info.percentage;
                        s_current_battery.is_low_battery = battery_info.is_low_battery;
                        s_current_battery.is_critical = battery_info.is_critical;
                        s_current_battery.is_valid = true;
                        s_battery_changed = true;
                        
                        ESP_LOGD(TAG, "Battery updated: %dmV, %d%%, Low: %d, Critical: %d",
                                 battery_info.voltage_mv, battery_info.percentage,
                                 battery_info.is_low_battery, battery_info.is_critical);
                    }
                } else {
                    s_current_battery.is_valid = false;
                    ESP_LOGW(TAG, "Failed to read battery info");
                }
                s_last_battery_update = current_time_us;
                xSemaphoreGive(s_data_mutex);
            }
        }
        
        // 任务延时
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms检查一次
    }
    
    ESP_LOGI(TAG, "Background manager task stopped");
    vTaskDelete(NULL);
}

esp_err_t background_manager_init(void) {
    if (s_initialized) {
        ESP_LOGW(TAG, "Background manager already initialized");
        return ESP_OK;
    }
    
    // 创建互斥锁
    s_data_mutex = xSemaphoreCreateMutex();
    if (s_data_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // 初始化数据
    memset(&s_current_time, 0, sizeof(s_current_time));
    memset(&s_current_battery, 0, sizeof(s_current_battery));
    s_time_changed = false;
    s_battery_changed = false;
    s_last_time_update = 0;
    s_last_battery_update = 0;
    s_start_time = 0;
    
    s_initialized = true;
    ESP_LOGI(TAG, "Background manager initialized");
    return ESP_OK;
}

esp_err_t background_manager_deinit(void) {
    if (!s_initialized) {
        return ESP_OK;
    }
    
    // 停止任务
    background_manager_stop();
    
    // 删除互斥锁
    if (s_data_mutex) {
        vSemaphoreDelete(s_data_mutex);
        s_data_mutex = NULL;
    }
    
    s_initialized = false;
    ESP_LOGI(TAG, "Background manager deinitialized");
    return ESP_OK;
}

esp_err_t background_manager_start(void) {
    if (!s_initialized) {
        ESP_LOGE(TAG, "Background manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_task_running) {
        ESP_LOGW(TAG, "Background manager task already running");
        return ESP_OK;
    }
    
    s_task_running = true;
    
    BaseType_t result = xTaskCreatePinnedToCore(
        background_manager_task,
        "Background_Mgr",
        4096,  // 4KB堆栈
        NULL,
        2,     // 优先级2
        &s_background_task_handle,
        0      // 运行在Core 0
    );
    
    if (result != pdPASS) {
        s_task_running = false;
        ESP_LOGE(TAG, "Failed to create background manager task");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Background manager task started");
    return ESP_OK;
}

esp_err_t background_manager_stop(void) {
    if (!s_task_running) {
        return ESP_OK;
    }
    
    s_task_running = false;
    
    if (s_background_task_handle) {
        vTaskDelete(s_background_task_handle);
        s_background_task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "Background manager task stopped");
    return ESP_OK;
}

esp_err_t background_manager_get_time(background_time_info_t* time_info) {
    if (!s_initialized || !time_info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(time_info, &s_current_time, sizeof(background_time_info_t));
        xSemaphoreGive(s_data_mutex);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

esp_err_t background_manager_get_battery(background_battery_info_t* battery_info) {
    if (!s_initialized || !battery_info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(battery_info, &s_current_battery, sizeof(background_battery_info_t));
        xSemaphoreGive(s_data_mutex);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

esp_err_t background_manager_get_system_info(background_system_info_t* system_info) {
    if (!s_initialized || !system_info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        system_info->time = s_current_time;
        system_info->battery = s_current_battery;
        
        // 获取WiFi信息
        wifi_manager_info_t wifi_info = wifi_manager_get_info();
        system_info->wifi_connected = (wifi_info.state == WIFI_STATE_CONNECTED);
        strncpy(system_info->ip_addr, wifi_info.ip_addr, sizeof(system_info->ip_addr) - 1);
        system_info->ip_addr[sizeof(system_info->ip_addr) - 1] = '\0';
        
        xSemaphoreGive(s_data_mutex);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

esp_err_t background_manager_get_time_str(char* time_str, size_t max_len) {
    if (!s_initialized || !time_str || max_len < 9) {
        return ESP_ERR_INVALID_ARG;
    }
    
    background_time_info_t time_info;
    esp_err_t ret = background_manager_get_time(&time_info);
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (time_info.is_valid) {
        snprintf(time_str, max_len, "%02d:%02d:%02d", 
                 time_info.hour, time_info.minute, time_info.second);
    } else {
        strncpy(time_str, "00:00:00", max_len);
    }
    
    return ESP_OK;
}

esp_err_t background_manager_get_battery_str(char* battery_str, size_t max_len) {
    if (!s_initialized || !battery_str || max_len < 5) {
        return ESP_ERR_INVALID_ARG;
    }
    
    background_battery_info_t battery_info;
    esp_err_t ret = background_manager_get_battery(&battery_info);
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (battery_info.is_valid) {
        snprintf(battery_str, max_len, "%d%%", battery_info.percentage);
    } else {
        strncpy(battery_str, "0%", max_len);
    }
    
    return ESP_OK;
}

bool background_manager_is_time_changed(void) {
    if (!s_initialized) {
        return false;
    }
    
    bool changed = false;
    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        changed = s_time_changed;
        xSemaphoreGive(s_data_mutex);
    }
    
    return changed;
}

bool background_manager_is_battery_changed(void) {
    if (!s_initialized) {
        return false;
    }
    
    bool changed = false;
    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        changed = s_battery_changed;
        xSemaphoreGive(s_data_mutex);
    }
    
    return changed;
}

void background_manager_mark_time_displayed(void) {
    if (!s_initialized) {
        return;
    }
    
    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_time_changed = false;
        xSemaphoreGive(s_data_mutex);
    }
}

void background_manager_mark_battery_displayed(void) {
    if (!s_initialized) {
        return;
    }
    
    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_battery_changed = false;
        xSemaphoreGive(s_data_mutex);
    }
}
