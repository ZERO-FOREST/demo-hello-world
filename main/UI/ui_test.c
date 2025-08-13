/**
 * @file ui_test.c
 * @brief 测试页面 - 用于验证LoadProhibited错误
 * @author Your Name
 * @date 2024
 */
#include "ui.h"
#include "esp_log.h"

static const char *TAG = "UI_TEST";

// 返回按钮回调
static void back_btn_cb(lv_event_t *e) {
    lv_obj_t *screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);
        ui_main_menu_create(screen);
    }
}

// 开关回调
static void test_switch_cb(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    ESP_LOGI(TAG, "Test switch: %s", is_on ? "ON" : "OFF");
}

// 创建测试界面
void ui_test_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Test UI");
    
    // 设置背景
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xf5f5f5), 0);
    
    // 创建标题
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "TEST PAGE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // 创建测试开关
    lv_obj_t *test_switch = lv_switch_create(parent);
    lv_obj_align(test_switch, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(test_switch, test_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // 创建开关标签
    lv_obj_t *switch_label = lv_label_create(parent);
    lv_label_set_text(switch_label, "Test Switch");
    lv_obj_set_style_text_font(switch_label, &lv_font_montserrat_16, 0);
    lv_obj_align_to(switch_label, test_switch, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    
    // 创建返回按钮
    lv_obj_t *back_btn = lv_btn_create(parent);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x666666), 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
    lv_obj_center(back_label);
    
    ESP_LOGI(TAG, "Test UI created successfully");
}
