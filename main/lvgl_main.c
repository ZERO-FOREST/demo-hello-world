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

// 声明启动动画函数 (从ui.h或直接声明)
void ui_start_animation_create(lv_obj_t *parent, ui_start_anim_finished_cb_t finished_cb);

static void lv_tick_task(void *arg) {
    (void) arg;
    lv_tick_inc(10);
}

/**
 * @brief 启动动画完成后的回调函数
 */
static void on_start_anim_finished(void)
{
    ESP_LOGI("LVGL_DEMO", "Start animation finished, creating main UI...");
    
    // 动画结束后，在这里创建你的主UI
    // 例如: 创建一个简单的开关
    lv_obj_t *switch1 = lv_switch_create(lv_scr_act());
    lv_obj_align(switch1, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(switch1, 100, 50);
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

    // 创建并启动动画，动画结束后调用 on_start_anim_finished
    ui_start_animation_create(lv_scr_act(), on_start_anim_finished);

    ESP_LOGI(TAG, "LVGL UI creation initiated with start animation");
    
    // LVGL主循环 - 专用任务处理
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));  // 100Hz刷新率
    }
}