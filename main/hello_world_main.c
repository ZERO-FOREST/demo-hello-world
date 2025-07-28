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
#include "lvgl.h"
#include "lv_port_disp.h"

static const char *TAG = "LVGL_DEMO";

// LVGL tick定时器
static void lv_tick_task(void *arg) {
    (void) arg;
    lv_tick_inc(10);
}

void app_main(void) {
    ESP_LOGI(TAG, "Hello LVGL on ESP32-S3!");

    // 初始化LVGL
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    // 初始化显示移植层
    lv_port_disp_init();

    // 创建定时器任务用于LVGL tick
    esp_timer_handle_t periodic_timer;
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "lv_tick"
    };
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10000));

    // 创建一个简单的标签
    lv_obj_t * label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello LVGL on ESP32-S3!\nN16R8 Config: 16MB Flash + 8MB PSRAM");
    lv_obj_center(label);

    // 创建一个按钮演示
    lv_obj_t * btn = lv_btn_create(lv_scr_act());
    lv_obj_set_pos(btn, 120, 150);
    lv_obj_set_size(btn, 80, 40);
    
    lv_obj_t * btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Click Me");
    lv_obj_center(btn_label);

    ESP_LOGI(TAG, "LVGL UI created successfully");

    // LVGL主循环
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}