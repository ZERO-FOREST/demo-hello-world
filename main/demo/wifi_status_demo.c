/**
 * @file wifi_status_demo.c
 * @brief WiFi状态显示测试程序
 * @author TidyCraze
 * @date 2025-01-27
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_manager.h"
#include "background_manager.h"

static const char* TAG = "WIFI_STATUS_DEMO";

void wifi_status_demo_task(void* pvParameters) {
    ESP_LOGI(TAG, "WiFi status demo task started");
    
    // 等待后台管理模块初始化
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    while (1) {
        // 获取WiFi信息
        wifi_manager_info_t wifi_info = wifi_manager_get_info();
        
        // 获取系统信息
        background_system_info_t system_info;
        if (background_manager_get_system_info(&system_info) == ESP_OK) {
            ESP_LOGI(TAG, "=== WiFi Status Demo ===");
            ESP_LOGI(TAG, "WiFi State: %d", wifi_info.state);
            ESP_LOGI(TAG, "WiFi Connected: %s", system_info.wifi_connected ? "Yes" : "No");
            ESP_LOGI(TAG, "IP Address: %s", system_info.ip_addr);
            ESP_LOGI(TAG, "Time: %02d:%02d", system_info.time.hour, system_info.time.minute);
            ESP_LOGI(TAG, "Battery: %d%%", system_info.battery.percentage);
            ESP_LOGI(TAG, "========================");
        } else {
            ESP_LOGW(TAG, "Failed to get system info");
        }
        
        // 每10秒输出一次状态
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

esp_err_t wifi_status_demo_init(void) {
    ESP_LOGI(TAG, "Initializing WiFi status demo");
    
    // 创建WiFi状态监控任务
    BaseType_t result = xTaskCreate(
        wifi_status_demo_task,
        "WiFi_Status_Demo",
        4096,
        NULL,
        1,
        NULL
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create WiFi status demo task");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "WiFi status demo initialized");
    return ESP_OK;
}
