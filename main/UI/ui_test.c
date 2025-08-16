/**
 * @file ui_test.c
 * @brief 测试页面 - 用于验证LoadProhibited错误
 * @author Your Name
 * @date 2024
 */
#include "custom_symbols.h"
#include "esp_log.h"
#include "theme_manager.h"
#include "ui.h"

static const char* TAG = "UI_TEST";

// 自定义返回按钮回调 - 处理测试界面的特殊逻辑
static void test_back_btn_callback(lv_event_t* e) {
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

    // 确保 content_container 不覆盖子控件的字体
    lv_obj_set_style_text_font(content_container, NULL, 0);

    // 4. 在content_container中添加页面内容

    // 创建所有符号标签
    lv_obj_t* symbol1 = create_symbol_label(content_container, SYMBOL_IMAGE_TRANS, lv_color_hex(0xFF0000));
    if (symbol1) {
        lv_obj_align(symbol1, LV_ALIGN_CENTER, -80, -60);
    } else {
        ESP_LOGE(TAG, "Failed to create SYMBOL_IMAGE_TRANS");
    }

    lv_obj_t* symbol2 = create_symbol_label(content_container, SYMBOL_GAME, lv_color_hex(0x00FF00));
    if (symbol2) {
        lv_obj_align(symbol2, LV_ALIGN_CENTER, -40, -60);
    } else {
        ESP_LOGE(TAG, "Failed to create SYMBOL_GAME");
    }

    lv_obj_t* symbol3 = create_symbol_label(content_container, SYMBOL_SERIAL_DISPLAY, lv_color_hex(0x0000FF));
    if (symbol3) {
        lv_obj_align(symbol3, LV_ALIGN_CENTER, 0, -60);
    } else {
        ESP_LOGE(TAG, "Failed to create SYMBOL_SERIAL_DISPLAY");
    }

    lv_obj_t* symbol4 = create_symbol_label(content_container, SYMBOL_NOWIFI, lv_color_hex(0xFFFF00));
    if (symbol4) {
        lv_obj_align(symbol4, LV_ALIGN_CENTER, 40, -60);
    } else {
        ESP_LOGE(TAG, "Failed to create SYMBOL_NOWIFI");
    }

    lv_obj_t* symbol5 = create_symbol_label(content_container, SYMBOL_CALIBRATION, lv_color_hex(0xFF00FF));
    if (symbol5) {
        lv_obj_align(symbol5, LV_ALIGN_CENTER, 80, -60);
    } else {
        ESP_LOGE(TAG, "Failed to create SYMBOL_CALIBRATION");
    }

    // 创建符号说明标签
    lv_obj_t* symbol_desc_label = lv_label_create(content_container);
    lv_label_set_text(symbol_desc_label, "Custom Symbols Test - Check Serial Output");
    theme_apply_to_label(symbol_desc_label, false);
    lv_obj_set_style_text_font(symbol_desc_label, &lv_font_montserrat_14, 0);
    lv_obj_align(symbol_desc_label, LV_ALIGN_CENTER, 0, 0);

    // 添加调试信息
    ESP_LOGI(TAG, "Testing symbols:");
    ESP_LOGI(TAG, "SYMBOL_IMAGE_TRANS: 0x%04X", SYMBOL_IMAGE_TRANS);
    ESP_LOGI(TAG, "SYMBOL_GAME: 0x%04X", SYMBOL_GAME);
    ESP_LOGI(TAG, "SYMBOL_SERIAL_DISPLAY: 0x%04X", SYMBOL_SERIAL_DISPLAY);
    ESP_LOGI(TAG, "SYMBOL_NOWIFI: 0x%04X", SYMBOL_NOWIFI);
    ESP_LOGI(TAG, "SYMBOL_CALIBRATION: 0x%04X", SYMBOL_CALIBRATION);

    // 创建测试开关
    lv_obj_t* test_switch = lv_switch_create(content_container);
    lv_obj_align(test_switch, LV_ALIGN_CENTER, 0, 20);
    theme_apply_to_switch(test_switch);
    lv_obj_add_event_cb(test_switch, test_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 创建开关标签
    lv_obj_t* switch_label = lv_label_create(content_container);
    lv_label_set_text(switch_label, "Test Switch");
    theme_apply_to_label(switch_label, false);
    lv_obj_set_style_text_font(switch_label, &lv_font_montserrat_16, 0);
    lv_obj_align_to(switch_label, test_switch, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    // 测试直接使用 Mysybmol 字体
    lv_obj_t* test_label = lv_label_create(content_container);
    lv_obj_set_style_text_font(test_label, &Mysybmol, 0);
    lv_label_set_text(test_label, "\xEE\x98\x83"); // U+E603
    lv_obj_align(test_label, LV_ALIGN_CENTER, 0, 40);

    ESP_LOGI(TAG, "Test UI created successfully");
}
