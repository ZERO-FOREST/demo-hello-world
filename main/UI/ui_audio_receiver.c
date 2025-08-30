/**
 * @file ui_audio_receiver.c
 * @brief 音频接收页面 - 用于接收和播放音频
 * @author Trae Builder
 * @date 2024
 */
#include "esp_log.h"
#include "lvgl.h"
#include "theme_manager.h"
#include "ui.h"

static const char* TAG = "UI_AUDIO_RECEIVER";

extern esp_err_t audio_receiver_start(void);
extern void audio_receiver_stop(void);

// 自定义返回按钮回调
static void audio_back_btn_callback(lv_event_t* e) {
    audio_receiver_stop();
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);
        ui_main_menu_create(screen);
    }
}

// 启动服务回调
static void start_service_btn_callback(lv_event_t* e) {
    audio_receiver_start();
    ESP_LOGI(TAG, "Audio receiver service started");
}

// 停止服务回调
static void stop_service_btn_callback(lv_event_t* e) {
    audio_receiver_stop();
    ESP_LOGI(TAG, "Audio receiver service stopped");
}

// 创建音频接收界面
void ui_audio_receiver_create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Audio Receiver UI");

    // 应用当前主题到屏幕
    theme_apply_to_screen(parent);

    // 1. 创建页面父级容器
    lv_obj_t* page_parent_container;
    ui_create_page_parent_container(parent, &page_parent_container);

    // 2. 创建顶部栏容器
    lv_obj_t* top_bar_container;
    lv_obj_t* title_container;
    ui_create_top_bar(page_parent_container, "AUDIO RECEIVER", false, &top_bar_container, &title_container, NULL);

    // 替换返回按钮回调
    lv_obj_t* back_btn = lv_obj_get_child(top_bar_container, 0);
    if (back_btn) {
        lv_obj_remove_event_cb(back_btn, NULL);
        lv_obj_add_event_cb(back_btn, audio_back_btn_callback, LV_EVENT_CLICKED, NULL);
    }

    // 3. 创建页面内容容器
    lv_obj_t* content_container;
    ui_create_page_content_area(page_parent_container, &content_container);

    // 4. 添加按钮
    lv_obj_t* start_btn = lv_btn_create(content_container);
    lv_obj_set_size(start_btn, 200, 50);
    lv_obj_align(start_btn, LV_ALIGN_CENTER, 0, -50);
    lv_obj_add_event_cb(start_btn, start_service_btn_callback, LV_EVENT_CLICKED, NULL);
    lv_obj_t* start_label = lv_label_create(start_btn);
    lv_label_set_text(start_label, "Start Service");
    lv_obj_center(start_label);

    lv_obj_t* stop_btn = lv_btn_create(content_container);
    lv_obj_set_size(stop_btn, 200, 50);
    lv_obj_align(stop_btn, LV_ALIGN_CENTER, 0, 50);
    lv_obj_add_event_cb(stop_btn, stop_service_btn_callback, LV_EVENT_CLICKED, NULL);
    lv_obj_t* stop_label = lv_label_create(stop_btn);
    lv_label_set_text(stop_label, "Stop Service");
    lv_obj_center(stop_label);

    ESP_LOGI(TAG, "Audio Receiver UI created successfully");
}