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

    // 创建一个简单的标签
    lv_obj_t *switch1 = lv_switch_create(lv_scr_act());
    lv_obj_align(switch1, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_size(switch1, 100, 50);

    ESP_LOGI(TAG, "LVGL UI created successfully");
    
    // LVGL主循环 - 专用任务处理
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));  // 100Hz刷新率
    }
}