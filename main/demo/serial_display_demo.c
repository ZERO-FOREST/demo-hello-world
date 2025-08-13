/**
 * @file serial_display_demo.c
 * @brief 串口显示演示程序
 * @author Your Name
 * @date 2024
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "wifi_manager.h"
#include "serial_display.h"
#include "../UI/ui_serial_display.h"

static const char *TAG = "SERIAL_DISPLAY_DEMO";

// 演示任务
static void demo_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Serial display demo started");
    
    // 等待WiFi连接
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    while (1) {
        wifi_manager_info_t wifi_info = wifi_manager_get_info();
        if (wifi_info.state == WIFI_STATE_CONNECTED) {
            ESP_LOGI(TAG, "WiFi connected! IP: %s", wifi_info.ip_addr);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // 初始化串口显示模块
    esp_err_t ret = serial_display_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize serial display");
        vTaskDelete(NULL);
        return;
    }
    
    // 启动串口显示服务，监听端口8080
    if (!serial_display_start(8080)) {
        ESP_LOGE(TAG, "Failed to start serial display service");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Serial display service started on port 8080");
    ESP_LOGI(TAG, "You can now send data via TCP to display on serial screen");
    ESP_LOGI(TAG, "Example: Use netcat or telnet to connect to port 8080");
    
    // 发送一些初始文本到串口屏幕
    serial_display_send_text("Serial Display Demo\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    serial_display_send_text("WiFi TCP -> Serial Screen\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    serial_display_send_text("Ready to receive data...\r\n");
    
    // 同时发送到UI显示
    ui_serial_display_add_text("Serial Display Demo");
    vTaskDelay(pdMS_TO_TICKS(1000));
    ui_serial_display_add_text("WiFi TCP -> Serial Screen");
    vTaskDelay(pdMS_TO_TICKS(1000));
    ui_serial_display_add_text("Ready to receive data...");
    
    // 主循环 - 监控服务状态
    int counter = 0;
    while (1) {
        if (serial_display_is_running()) {
            // 每30秒发送一次状态信息
            if (counter % 30 == 0) {
                char status_msg[128];
                snprintf(status_msg, sizeof(status_msg), 
                        "Status: Running, Counter: %d\r\n", counter);
                serial_display_send_text(status_msg);
            }
        } else {
            ESP_LOGW(TAG, "Serial display service stopped unexpectedly");
            // 尝试重新启动
            if (serial_display_start(8080)) {
                ESP_LOGI(TAG, "Serial display service restarted");
            }
        }
        
        counter++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// 初始化函数
esp_err_t serial_display_demo_init(void)
{
    ESP_LOGI(TAG, "Initializing serial display demo...");
    
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 初始化WiFi管理器
    ret = wifi_manager_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager");
        return ret;
    }
    
    // 启动WiFi连接
    ret = wifi_manager_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi");
        return ret;
    }
    
    // 创建演示任务
    if (xTaskCreate(demo_task, "serial_demo", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create demo task");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Serial display demo initialized successfully");
    return ESP_OK;
}
