/**
 * @file ui_test.c
 * @brief 测试页面 - 用于测试
 * @author Your Name
 * @date 2024
 */
#include "esp_log.h"
#include "joystick_adc.h"
#include "misc/lv_color.h"
#include "theme_manager.h"
#include "ui.h"

#include "Mysybmol.h"

static const char* TAG = "UI_TEST";

// 自定义返回按钮回调 - 处理测试界面的特殊逻辑
static void test_back_btn_callback(lv_event_t* e) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);
        ui_main_menu_create(screen);
    }
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

    // 创建测试标签
    lv_obj_t* label = lv_label_create(cont);

    lv_font_t* loaded_font = get_loaded_font();
    lv_obj_set_style_text_font(label, loaded_font, LV_PART_MAIN);
    lv_label_set_text(label, "你好,世界!\n字体分区加载成功!");
    lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN); // 绿色表示成功
    ESP_LOGI(TAG, "Font from partition applied successfully");

    lv_obj_center(label);

    ESP_LOGI(TAG, "Test UI created successfully");
}
