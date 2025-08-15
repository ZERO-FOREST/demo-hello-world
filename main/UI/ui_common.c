/**
 * @file ui_common.c
 * @brief 通用UI组件
 * @author TidyCraze
 * @date 2025-01-27
 */

#include "esp_log.h"
#include "font/lv_symbol_def.h"
#include "game.h"
#include "ui.h"

static const char* TAG = "UI_COMMON";

// 统一的返回按钮回调函数 - 返回到主菜单
static void back_button_callback(lv_event_t* e) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);
        ui_main_menu_create(screen);
    }
}

// 返回到游戏菜单的回调函数
static void back_to_game_menu_callback(lv_event_t* e) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);
        // 直接调用游戏菜单创建函数
        extern void ui_game_menu_create(lv_obj_t * parent);
        ui_game_menu_create(screen);
    }
}

// 创建统一的back按钮 - 返回到主菜单
void ui_create_back_button(lv_obj_t* parent, const char* text) {
    (void)text; // 忽略text参数，统一使用符号
    // 创建back按钮 - 统一放置在左上角，与页面标题对齐
    lv_obj_t* back_btn = lv_btn_create(parent);
    lv_obj_set_size(back_btn, 40, 30);                 // 使用符号后可以更小
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10); // 左上角，与标题区域对齐

    // 设置按钮样式
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x666666), 0); // 灰色背景
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_80, 0);
    lv_obj_set_style_radius(back_btn, 6, 0);       // 圆角
    lv_obj_set_style_shadow_width(back_btn, 2, 0); // 轻微阴影
    lv_obj_set_style_shadow_ofs_y(back_btn, 1, 0);
    lv_obj_set_style_shadow_opa(back_btn, LV_OPA_30, 0);

    // 添加点击事件
    lv_obj_add_event_cb(back_btn, back_button_callback, LV_EVENT_CLICKED, NULL);

    // 创建按钮标签 - 使用关闭符号（更像返回按钮）
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_16, 0);  // 符号字体稍大一些
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0); // 白色文字
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "Back button created at top-left position");
}

// 创建返回到游戏菜单的back按钮
void ui_create_game_back_button(lv_obj_t* parent, const char* text) {
    (void)text; // 忽略text参数，统一使用符号
    // 创建back按钮 - 统一放置在左上角，与页面标题对齐
    lv_obj_t* back_btn = lv_btn_create(parent);
    lv_obj_set_size(back_btn, 40, 30);                 // 使用符号后可以更小
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10); // 左上角，与标题区域对齐

    // 设置按钮样式
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x666666), 0); // 灰色背景
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_80, 0);
    lv_obj_set_style_radius(back_btn, 6, 0);       // 圆角
    lv_obj_set_style_shadow_width(back_btn, 2, 0); // 轻微阴影
    lv_obj_set_style_shadow_ofs_y(back_btn, 1, 0);
    lv_obj_set_style_shadow_opa(back_btn, LV_OPA_30, 0);

    // 添加点击事件 - 返回到游戏菜单
    lv_obj_add_event_cb(back_btn, back_to_game_menu_callback, LV_EVENT_CLICKED, NULL);

    // 创建按钮标签 - 使用关闭符号（更像返回按钮）
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_16, 0);  // 符号字体稍大一些
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0); // 白色文字
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "Game back button created at top-left position");
}
