/**
 * @file ui_test.c
 * @brief 测试页面 - 用于验证LoadProhibited错误
 * @author Your Name
 * @date 2024
 */
#include "esp_log.h"
#include "theme_manager.h"
#include "joystick_adc.h"
#include "ui.h"

#include "Mysybmol.h"

static const char* TAG = "UI_TEST";

static lv_obj_t* joy1_x_raw_label;
static lv_obj_t* joy1_y_raw_label;
static lv_obj_t* joy1_x_norm_label;
static lv_obj_t* joy1_y_norm_label;
static lv_obj_t* joy2_x_raw_label;
static lv_obj_t* joy2_y_raw_label;
static lv_obj_t* joy2_x_norm_label;
static lv_obj_t* joy2_y_norm_label;
static lv_timer_t* joystick_timer;

static void joystick_update_timer_cb(lv_timer_t* timer) {
    joystick_data_t data;
    if (joystick_adc_read(&data) == ESP_OK) {
        lv_label_set_text_fmt(joy1_x_raw_label, "Joy1 X Raw: %d", data.raw_joy1_x);
        lv_label_set_text_fmt(joy1_y_raw_label, "Joy1 Y Raw: %d", data.raw_joy1_y);
        lv_label_set_text_fmt(joy1_x_norm_label, "Joy1 X Norm: %d", data.norm_joy1_x);
        lv_label_set_text_fmt(joy1_y_norm_label, "Joy1 Y Norm: %d", data.norm_joy1_y);
        lv_label_set_text_fmt(joy2_x_raw_label, "Joy2 X Raw: %d", data.raw_joy2_x);
        lv_label_set_text_fmt(joy2_y_raw_label, "Joy2 Y Raw: %d", data.raw_joy2_y);
        lv_label_set_text_fmt(joy2_x_norm_label, "Joy2 X Norm: %d", data.norm_joy2_x);
        lv_label_set_text_fmt(joy2_y_norm_label, "Joy2 Y Norm: %d", data.norm_joy2_y);
    }
}

// 自定义返回按钮回调 - 处理测试界面的特殊逻辑
static void test_back_btn_callback(lv_event_t* e) {
    if (joystick_timer) {
        lv_timer_del(joystick_timer);
        joystick_timer = NULL;
    }
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);
        ui_main_menu_create(screen);
    }
}

// 开关回调
static void test_switch_cb(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    ESP_LOGI(TAG, "Test switch: %s", is_on ? "ON" : "OFF");
}

// 创建测试界面
void ui_test_create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Test UI");

    // 应用当前主题到屏幕
    theme_apply_to_screen(parent);

    // 1. 创建页面父级容器（统一管理整个页面）
    lv_obj_t* page_parent_container;
    ui_create_page_parent_container(parent, &page_parent_container);

    // 2. 创建顶部栏容器（包含返回按钮和标题）
    lv_obj_t* top_bar_container;
    lv_obj_t* title_container;
    ui_create_top_bar(page_parent_container, "TEST PAGE", &top_bar_container, &title_container);

    // 替换顶部栏的返回按钮回调为自定义回调
    lv_obj_t* back_btn = lv_obj_get_child(top_bar_container, 0); // 获取返回按钮
    if (back_btn) {
        lv_obj_remove_event_cb(back_btn, NULL); // 移除默认回调
        lv_obj_add_event_cb(back_btn, test_back_btn_callback, LV_EVENT_CLICKED, NULL);
    }

    // 3. 创建页面内容容器
    lv_obj_t* content_container;
    ui_create_page_content_area(page_parent_container, &content_container);

    // 4. 在content_container中添加页面内容
    lv_obj_t* cont = lv_obj_create(content_container);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(cont);

    joy1_x_raw_label = lv_label_create(cont);
    joy1_y_raw_label = lv_label_create(cont);
    joy1_x_norm_label = lv_label_create(cont);
    joy1_y_norm_label = lv_label_create(cont);
    joy2_x_raw_label = lv_label_create(cont);
    joy2_y_raw_label = lv_label_create(cont);
    joy2_x_norm_label = lv_label_create(cont);
    joy2_y_norm_label = lv_label_create(cont);

    lv_label_set_text(joy1_x_raw_label, "Joy1 X Raw: 0");
    lv_label_set_text(joy1_y_raw_label, "Joy1 Y Raw: 0");
    lv_label_set_text(joy1_x_norm_label, "Joy1 X Norm: 0");
    lv_label_set_text(joy1_y_norm_label, "Joy1 Y Norm: 0");
    lv_label_set_text(joy2_x_raw_label, "Joy2 X Raw: 0");
    lv_label_set_text(joy2_y_raw_label, "Joy2 Y Raw: 0");
    lv_label_set_text(joy2_x_norm_label, "Joy2 X Norm: 0");
    lv_label_set_text(joy2_y_norm_label, "Joy2 Y Norm: 0");

    joystick_timer = lv_timer_create(joystick_update_timer_cb, 50, NULL);

    ESP_LOGI(TAG, "Test UI created successfully");
}
