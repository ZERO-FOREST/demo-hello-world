/**
 * @file ui_test.c
 * @brief 测试页面 - 用于验证LoadProhibited错误
 * @author Your Name
 * @date 2024
 */
#include "esp_log.h"
#include "ui.h"

static const char* TAG = "UI_TEST";

// 返回按钮回调
static void back_btn_cb(lv_event_t* e) {
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

    // 设置背景
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xF6E9DB), LV_PART_MAIN); // 使用莫兰迪色系

    // 创建统一标题
    ui_create_page_title(parent, "TEST PAGE");

    // 创建测试开关
    lv_obj_t* test_switch = lv_switch_create(parent);
    lv_obj_align(test_switch, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(test_switch, test_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 创建开关标签
    lv_obj_t* switch_label = lv_label_create(parent);
    lv_label_set_text(switch_label, "Test Switch");
    lv_obj_set_style_text_font(switch_label, &lv_font_montserrat_16, 0);
    lv_obj_align_to(switch_label, test_switch, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    // 创建返回按钮 - 使用统一的back按钮函数
    ui_create_back_button(parent, "Back");

    ESP_LOGI(TAG, "Test UI created successfully");
}
