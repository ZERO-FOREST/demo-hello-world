/*
 * @Author: tidycraze 2595256284@qq.com
 * @Date: 2025-07-28 11:29:59
 * @LastEditors: tidycraze 2595256284@qq.com
 * @LastEditTime: 2025-08-21 09:57:19
 * @FilePath: \demo-hello-world\main\hello_world_main.c
 * @Description:
 *
 * Copyright (c) 2025 by ${git_name_email}, All Rights Reserved.
 */

#include "background_manager.h"
#include "battery_monitor.h"
#include "calibration_manager.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lsm6ds3_demo.h"
#include "nvs_flash.h"
#include "power_management.h"
#include "sdkconfig.h"
#include "spiffs_test_demo.h"
#include "st7789.h"
#include "task_init.h"
#include "wifi_manager.h"
#include <inttypes.h>
#include <stdio.h>


static const char* TAG = "MAIN";

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

    // 初始化校准管理器
    ret = calibration_manager_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration manager initialized");
    } else {
        ESP_LOGW(TAG, "Calibration manager init failed: %s", esp_err_to_name(ret));
    }

    // 初始化电池监测模块
    ret = battery_monitor_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Battery monitor initialized");
    } else {
        ESP_LOGW(TAG, "Battery monitor init failed: %s", esp_err_to_name(ret));
    }

    // 运行SPIFFS文件系统测试
    ESP_LOGI(TAG, "Starting SPIFFS file system test...");
    ret = run_spiffs_test_suite();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS test failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS test completed successfully");
    }

    // 使用任务初始化模块统一管理任务
    ret = init_all_tasks();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize tasks: %s", esp_err_to_name(ret));
        return;
    }

    // 显示当前运行的任务
    vTaskDelay(pdMS_TO_TICKS(1000)); // 等待任务启动
    list_running_tasks();

    // 主任务进入轻量级监控循环
    while (1) {
        ESP_LOGI(TAG, "Main loop: System running normally, free heap: %lu bytes",
                 (unsigned long)esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(30000)); // 30秒打印一次状态
    }
}