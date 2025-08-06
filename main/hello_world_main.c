/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "power_management.h"
#include "task_init.h"         
#include "lsm6ds3_demo.h"
#include "st7789.h"
#include "wifi_manager.h"
#include "battery_monitor.h"


static const char *TAG = "MAIN";

void app_main(void) {
    // 初始化NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "ESP32-S3 Demo Application Starting...");
    ESP_LOGI(TAG, "App main running on core %d", xPortGetCoreID());

    // 初始化电池监测模块
    ret = battery_monitor_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Battery monitor initialized");
    } else {
        ESP_LOGW(TAG, "Battery monitor init failed: %s", esp_err_to_name(ret));
    }

    // 初始化并启动WiFi连接
    ret = wifi_manager_init(NULL);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi manager initialized");
        ret = wifi_manager_start();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "WiFi connection started");
        } else {
            ESP_LOGW(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGW(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Available Demos:");
    ESP_LOGI(TAG, "  1. LVGL + Power Management (default)");
    ESP_LOGI(TAG, "  2. WS2812 LED Demo (compile with -DWS2812_DEMO_ONLY)");
    ESP_LOGI(TAG, "  3. LSM6DS3 IMU Sensor Demo");
    ESP_LOGI(TAG, "");

    // 使用任务初始化模块统一管理任务
    ret = init_all_tasks();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize tasks: %s", esp_err_to_name(ret));
        return;
    }
    
    // 启动触摸测试（可选）
    // touch_test_start();  // 取消注释以启用触摸测试

    ESP_LOGI(TAG, "=== System Information ===");
    ESP_LOGI(TAG, "Hardware: ESP32-S3 N16R8 (16MB Flash + 8MB PSRAM)");
    ESP_LOGI(TAG, "Display: ST7789 320x240 TFT");
    ESP_LOGI(TAG, "Touch: XPT2046 Resistive Touch");
    ESP_LOGI(TAG, "WS2812: GPIO48, Up to 256 LEDs");
    ESP_LOGI(TAG, "LSM6DS3: I2C/SPI 6-axis IMU Sensor");
    ESP_LOGI(TAG, "========================");

    // 显示当前运行的任务
    vTaskDelay(pdMS_TO_TICKS(1000));  // 等待任务启动
    list_running_tasks();

    // 主任务进入轻量级监控循环
    while (1) {
        ESP_LOGI(TAG, "Main loop: System running normally, free heap: %lu bytes", 
                (unsigned long)esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(30000));  // 30秒打印一次状态
    }
}