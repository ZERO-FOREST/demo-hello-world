/*
 * @Author: tidycraze 2595256284@qq.com
 * @Date: 2025-01-06 15:30:00
 * @LastEditors: tidycraze 2595256284@qq.com
 * @LastEditTime: 2025-01-06 15:30:00
 * @FilePath: \demo-hello-world\components\Receiver\example\tcp_event_driven_example.c
 * @Description: 事件驱动TCP任务管理器使用示例
 * 
 */

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "task.h"
#include "wifi_pairing_manager.h"

static const char *TAG = "TCP_Event_Example";

/**
 * @brief WIFI事件回调函数
 * @param state WIFI配对状态
 * @param ssid 连接的SSID（可能为NULL）
 */
static void wifi_event_callback(wifi_pairing_state_t state, const char* ssid) {
    switch (state) {
        case WIFI_PAIRING_STATE_IDLE:
            ESP_LOGI(TAG, "WIFI状态: 空闲");
            break;
            
        case WIFI_PAIRING_STATE_SCANNING:
            ESP_LOGI(TAG, "WIFI状态: 扫描中");
            break;
            
        case WIFI_PAIRING_STATE_CONNECTING:
            ESP_LOGI(TAG, "WIFI状态: 连接中");
            break;
            
        case WIFI_PAIRING_STATE_CONNECTED:
            ESP_LOGI(TAG, "WIFI状态: 已连接到 %s", ssid ? ssid : "Unknown");
            break;
            
        case WIFI_PAIRING_STATE_DISCONNECTED:
            ESP_LOGI(TAG, "WIFI状态: 已断开");
            break;
            
        default:
            ESP_LOGW(TAG, "WIFI状态: 未知状态 %d", state);
            break;
    }
}

/**
 * @brief 事件驱动TCP任务示例主函数
 */
void tcp_event_driven_example(void) {
    ESP_LOGI(TAG, "启动事件驱动TCP任务示例");
    
    // 1. 配置WIFI配对管理器
    wifi_pairing_config_t wifi_config = {
        .target_ssids = {"TidyC", "MyWiFi", "TestAP"},  // 目标SSID列表
        .target_passwords = {"22989822", "password123", "testpass"},  // 对应密码
        .target_count = 3,
        .scan_interval_ms = 10000,  // 10秒扫描间隔
        .connect_timeout_ms = 15000  // 15秒连接超时
    };
    
    // 2. 初始化WIFI配对管理器
    esp_err_t ret = wifi_pairing_manager_init(&wifi_config, wifi_event_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WIFI配对管理器初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "WIFI配对管理器初始化成功");
    
    // 3. 初始化TCP任务管理器
    ret = tcp_task_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TCP任务管理器初始化失败: %s", esp_err_to_name(ret));
        wifi_pairing_manager_deinit();
        return;
    }
    ESP_LOGI(TAG, "TCP任务管理器初始化成功");
    
    // 4. 启动WIFI配对管理器
    ret = wifi_pairing_manager_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WIFI配对管理器启动失败: %s", esp_err_to_name(ret));
        tcp_task_manager_stop();
        wifi_pairing_manager_deinit();
        return;
    }
    ESP_LOGI(TAG, "WIFI配对管理器启动成功");
    
    // 5. 启动TCP任务管理器
    ret = tcp_task_manager_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TCP任务管理器启动失败: %s", esp_err_to_name(ret));
        wifi_pairing_manager_stop();
        wifi_pairing_manager_deinit();
        return;
    }
    ESP_LOGI(TAG, "TCP任务管理器启动成功");
    
    ESP_LOGI(TAG, "事件驱动TCP任务示例启动完成");
    ESP_LOGI(TAG, "系统将自动管理WIFI连接和TCP连接的生命周期");
    
    // 6. 主循环 - 监控系统状态
    uint32_t status_counter = 0;
    while (1) {
        // 每30秒打印一次系统状态
        if (++status_counter >= 300) {  // 30秒 / 100ms = 300次
            wifi_pairing_state_t wifi_state = wifi_pairing_get_state();
            ESP_LOGI(TAG, "系统状态检查 - WIFI状态: %d", wifi_state);
            
            if (wifi_state == WIFI_PAIRING_STATE_CONNECTED) {
                wifi_credentials_t credentials;
                if (wifi_pairing_get_current_credentials(&credentials) == ESP_OK) {
                    ESP_LOGI(TAG, "当前连接的WIFI: %s", credentials.ssid);
                }
            }
            
            status_counter = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief 清理资源（可选调用）
 */
void tcp_event_driven_cleanup(void) {
    ESP_LOGI(TAG, "清理事件驱动TCP任务资源");
    
    // 停止TCP任务管理器
    tcp_task_manager_stop();
    
    // 停止并清理WIFI配对管理器
    wifi_pairing_manager_stop();
    wifi_pairing_manager_deinit();
    
    ESP_LOGI(TAG, "资源清理完成");
}