/**
 * @file wifi_time_sync_demo.c
 * @brief WiFi时间同步测试程序
 * @author TidyCraze
 * @date 2025-01-27
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_manager.h"
#include "background_manager.h"

static const char* TAG = "WIFI_TIME_SYNC_DEMO";

void wifi_time_sync_demo_task(void* pvParameters) {
    ESP_LOGI(TAG, "WiFi time sync demo task started");
    
    // 等待后台管理模块初始化
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 记录初始时间
    background_time_info_t initial_time;
    if (background_manager_get_time(&initial_time) == ESP_OK) {
        ESP_LOGI(TAG, "Initial time: %02d:%02d", initial_time.hour, initial_time.minute);
    }
    
    // 模拟WiFi连接状态变化
    ESP_LOGI(TAG, "=== WiFi Time Sync Demo ===");
    ESP_LOGI(TAG, "1. Starting with disconnected WiFi");
    ESP_LOGI(TAG, "2. Simulating WiFi connection...");
    
    // 等待一段时间模拟连接过程
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 检查WiFi状态
    wifi_manager_info_t wifi_info = wifi_manager_get_info();
    ESP_LOGI(TAG, "WiFi State: %d", wifi_info.state);
    
    if (wifi_info.state == WIFI_STATE_CONNECTED) {
        ESP_LOGI(TAG, "WiFi connected! Time should be synced automatically.");
        
        // 等待时间同步
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // 检查同步后的时间
        background_time_info_t synced_time;
        if (background_manager_get_time(&synced_time) == ESP_OK) {
            ESP_LOGI(TAG, "Time after sync: %02d:%02d", synced_time.hour, synced_time.minute);
            ESP_LOGI(TAG, "Network synced: %s", synced_time.is_network_synced ? "Yes" : "No");
        }
    } else {
        ESP_LOGW(TAG, "WiFi not connected, time sync will not occur");
    }
    
    ESP_LOGI(TAG, "=== Demo completed ===");
    
    // 持续监控状态
    while (1) {
        wifi_info = wifi_manager_get_info();
        background_time_info_t current_time;
        
        if (background_manager_get_time(&current_time) == ESP_OK) {
            ESP_LOGI(TAG, "Status: WiFi=%s, Time=%02d:%02d, Synced=%s",
                     wifi_info.state == WIFI_STATE_CONNECTED ? "Connected" : "Disconnected",
                     current_time.hour, current_time.minute,
                     current_time.is_network_synced ? "Yes" : "No");
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // 每10秒输出一次状态
    }
}

esp_err_t wifi_time_sync_demo_init(void) {
    ESP_LOGI(TAG, "Initializing WiFi time sync demo");
    
    // 创建WiFi时间同步测试任务
    BaseType_t result = xTaskCreate(
        wifi_time_sync_demo_task,
        "WiFi_Time_Sync_Demo",
        4096,
        NULL,
        1,
        NULL
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create WiFi time sync demo task");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "WiFi time sync demo initialized");
    return ESP_OK;
}
