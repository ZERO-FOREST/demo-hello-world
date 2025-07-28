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
#include "power_management.h"  // 添加电源管理
#include "task_init.h"         // 添加任务初始化模块
#include "lsm6ds3_demo.h"      // 添加LSM6DS3演示

static const char *TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "ESP32-S3 Demo Application Starting...");
    ESP_LOGI(TAG, "App main running on core %d", xPortGetCoreID());

    // 首先检查唤醒原因
    check_wakeup_reason();
    
    // 配置自动电源管理
    configure_auto_power_management();

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Available Demos:");
    ESP_LOGI(TAG, "  1. LVGL + Power Management (default)");
    ESP_LOGI(TAG, "  2. WS2812 LED Demo (compile with -DWS2812_DEMO_ONLY)");
    ESP_LOGI(TAG, "  3. LSM6DS3 IMU Sensor Demo");
    ESP_LOGI(TAG, "");

    // 使用任务初始化模块统一管理任务
    esp_err_t ret = init_all_tasks();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize tasks: %s", esp_err_to_name(ret));
        return;
    }

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