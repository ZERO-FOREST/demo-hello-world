/**
 * @file lvgl_main.c
 * @brief LVGL主任务，负责UI的初始化和流程控制
 * @author Your Name
 * @date 2024
 */
#include "ui.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 动画完成后的回调函数
static void show_main_menu_cb(void) {
    ui_main_menu_create(lv_scr_act());
}

static void lv_tick_task(void *arg) {
    (void) arg;
    lv_tick_inc(10);
}

void lvgl_main_task(void *pvParameters) {

    const char *TAG = "LVGL_DEMO";
    ESP_LOGI(TAG, "LVGL task started on core %d", xPortGetCoreID());
    
    // 初始化LVGL
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

    esp_timer_handle_t periodic_timer;
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "lv_tick"
    };
    
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10000));  // 10ms周期
    ESP_LOGI(TAG, "LVGL tick timer started (10ms period)");

    // 创建并启动开机动画，动画结束后调用 show_main_menu_cb
    ui_start_animation_create(lv_scr_act(), show_main_menu_cb);

    ESP_LOGI(TAG, "LVGL UI flow started with animation");
    
    // LVGL主循环 - 专用任务处理
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));  // 100Hz刷新率
    }
}