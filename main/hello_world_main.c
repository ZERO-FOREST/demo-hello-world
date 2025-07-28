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
#include "esp_timer.h"
#include "esp_log.h"
#include "lvgl_main.h"

void app_main(void) {
    ESP_LOGI("system", "App main running on core %d", xPortGetCoreID());

    xTaskCreatePinnedToCore(
        lvgl_main_task,     // 任务函数
        "LVGL_Main",        // 任务名称
        8192,               // 堆栈大小 (8KB)
        NULL,               // 参数
        5,                  // 优先级 (中等)
        NULL,               // 任务句柄
        1                   // 绑定到Core 1 (用户核心)
    );

    while (1) {
        ESP_LOGI("system", "Main task free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(5000));  // 5秒打印一次状态
    }
}