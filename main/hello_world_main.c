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
#include "lvgl_main.h"
#include "power_management.h"  // 🔋 添加电源管理
#include "sleep_demo.h"        // 🛌 添加睡眠演示

static const char *TAG = "MAIN";

// 🔋 电源管理任务包装函数
static void power_demo_task(void *pvParameters) {
    power_management_demo();
    vTaskDelete(NULL);  // 任务完成后删除自己
}

void app_main(void) {
    ESP_LOGI(TAG, "🚀 ESP32-S3 Demo Application Starting...");
    ESP_LOGI(TAG, "App main running on core %d", xPortGetCoreID());

    // 🌅 首先检查唤醒原因
    check_wakeup_reason();
    
    // 🔋 配置自动电源管理
    configure_auto_power_management();

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📋 Available Demos:");
    ESP_LOGI(TAG, "  1. LVGL + Power Management (default)");
    ESP_LOGI(TAG, "  2. Sleep Mode Demo (compile with -DSLEEP_DEMO_ONLY)");
    ESP_LOGI(TAG, "");

#ifdef SLEEP_DEMO_ONLY
    // 🛌 仅运行睡眠模式演示
    ESP_LOGI(TAG, "🛌 Starting Sleep Mode Demo Only...");
    ESP_LOGI(TAG, "💡 This will cycle through Light Sleep, Deep Sleep, and Hibernation");
    ESP_LOGI(TAG, "🔘 Press GPIO0 (BOOT button) to wake from sleep anytime");
    ESP_LOGI(TAG, "");
    
    simple_sleep_demo_main();  // 这个函数会进入睡眠循环
    
#else
    // 🎮 运行LVGL + 电源管理演示
    ESP_LOGI(TAG, "🎮 Starting LVGL + Power Management Demo...");
    
    // 🎮 启动LVGL任务
    xTaskCreatePinnedToCore(
        lvgl_main_task,     // 任务函数
        "LVGL_Main",        // 任务名称
        8192,               // 堆栈大小 (8KB)
        NULL,               // 参数
        5,                  // 优先级 (中等)
        NULL,               // 任务句柄
        1                   // 绑定到Core 1 (用户核心)
    );

    // 🎛️ 创建电源管理演示任务
    xTaskCreatePinnedToCore(
        power_demo_task,       // 任务包装函数
        "Power_Demo",          // 任务名称
        4096,                  // 堆栈大小 (4KB)
        NULL,                  // 参数
        3,                     // 优先级 (较低)
        NULL,                  // 任务句柄
        0                      // 绑定到Core 0 (系统核心)
    );

    while (1) {
        ESP_LOGI(TAG, "Main task free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(5000));  // 5秒打印一次状态
    }
#endif
}