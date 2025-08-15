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
    lv_obj_set_size(back_btn, 40, 40);                 // 使用符号后可以更小
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

    // 创建按钮标签
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

    // 创建按钮标签
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_16, 0);  // 符号字体稍大一些
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0); // 白色文字
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "Game back button created at top-left position");
}

// 创建页面父级容器（统一管理整个页面）
void ui_create_page_parent_container(lv_obj_t* parent, lv_obj_t** page_parent_container) {
    // 创建页面父级容器 - 占满整个屏幕
    *page_parent_container = lv_obj_create(parent);
    lv_obj_set_size(*page_parent_container, 240, 320); // 占满屏幕
    lv_obj_align(*page_parent_container, LV_ALIGN_CENTER, 0, 0);

    // 设置父级容器样式 - 不可滚动
    lv_obj_set_style_bg_opa(*page_parent_container, LV_OPA_0, LV_PART_MAIN); // 透明背景
    lv_obj_set_style_border_width(*page_parent_container, 0, LV_PART_MAIN);  // 无边框
    lv_obj_set_style_pad_all(*page_parent_container, 0, LV_PART_MAIN);       // 无内边距
    lv_obj_set_style_radius(*page_parent_container, 0, LV_PART_MAIN);        // 无圆角
    lv_obj_clear_flag(*page_parent_container, LV_OBJ_FLAG_SCROLLABLE);       // 禁止滚动

    ESP_LOGI(TAG, "Page parent container created");
}

// 创建顶部栏容器（包含返回按钮和标题）
void ui_create_top_bar(lv_obj_t* parent, const char* title_text, lv_obj_t** top_bar_container,
                       lv_obj_t** title_container) {
    // 创建顶部栏容器 - 240x30
    *top_bar_container = lv_obj_create(parent);
    lv_obj_set_size(*top_bar_container, 240, 40);
    lv_obj_align(*top_bar_container, LV_ALIGN_TOP_MID, 0, 0);

    // 设置顶部栏容器的样式 - 添加主题颜色以便查看边界
    lv_obj_set_style_bg_color(*top_bar_container, theme_get_color(theme_get_current_theme()->colors.surface),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(*top_bar_container, LV_OPA_50, LV_PART_MAIN); // 半透明背景
    lv_obj_set_style_border_width(*top_bar_container, 1, LV_PART_MAIN);   // 添加边框以便查看边界
    lv_obj_set_style_border_color(*top_bar_container, theme_get_color(theme_get_current_theme()->colors.border),
                                  LV_PART_MAIN);
    lv_obj_set_style_pad_all(*top_bar_container, 0, LV_PART_MAIN); // 无内边距
    lv_obj_set_style_radius(*top_bar_container, 0, LV_PART_MAIN);  // 无圆角
    lv_obj_clear_flag(*top_bar_container, LV_OBJ_FLAG_SCROLLABLE); // 禁止滚动

    // 创建返回按钮 - 左侧放置，大小40x30
    lv_obj_t* back_btn = lv_btn_create(*top_bar_container);
    lv_obj_set_size(back_btn, 40, 30);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 0, 0);

    // 设置返回按钮样式
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x666666), 0);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_80, 0);
    lv_obj_set_style_radius(back_btn, 6, 0);
    lv_obj_set_style_shadow_width(back_btn, 2, 0);
    lv_obj_set_style_shadow_ofs_y(back_btn, 1, 0);
    lv_obj_set_style_shadow_opa(back_btn, LV_OPA_30, 0);

    // 添加返回按钮点击事件
    lv_obj_add_event_cb(back_btn, back_button_callback, LV_EVENT_CLICKED, NULL);

    // 创建返回按钮标签
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);

    // 创建标题容器 - 除开返回按钮的剩余区域
    *title_container = lv_obj_create(*top_bar_container);
    lv_obj_set_size(*title_container, 200, 40);               // 240-40=200宽度
    lv_obj_align(*title_container, LV_ALIGN_RIGHT_MID, 0, 0); // 右对齐，紧贴返回按钮

    // 清除标题容器的所有样式
    lv_obj_set_style_bg_opa(*title_container, LV_OPA_0, LV_PART_MAIN); // 透明背景
    lv_obj_set_style_border_width(*title_container, 0, LV_PART_MAIN);  // 无边框
    lv_obj_set_style_pad_all(*title_container, 0, LV_PART_MAIN);       // 无内边距
    lv_obj_set_style_radius(*title_container, 0, LV_PART_MAIN);        // 无圆角
    lv_obj_clear_flag(*title_container, LV_OBJ_FLAG_SCROLLABLE);       // 禁止滚动

    // 创建标题标签 - 居中放置，字体大小20
    lv_obj_t* title = lv_label_create(*title_container);
    lv_label_set_text(title, title_text);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);                   // 居中对齐
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0); // 字体大小20
    lv_obj_set_style_text_color(title, theme_get_color(theme_get_current_theme()->colors.text_primary), 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_SCROLLABLE);

    ESP_LOGI(TAG, "Top bar created: %s", title_text);
}

// 创建页面内容容器（除开顶部栏的区域）
void ui_create_page_content_area(lv_obj_t* parent, lv_obj_t** content_container) {
    // 创建页面内容容器 - 240x290，从顶部栏下方开始
    *content_container = lv_obj_create(parent);
    lv_obj_set_size(*content_container, 240, 280);             // 高度为屏幕高度减去顶部栏高度和边距
    lv_obj_align(*content_container, LV_ALIGN_TOP_MID, 0, 40); // 从顶部栏下方开始 (10+30=40)

    // 设置内容区域容器的样式 - 添加主题背景色以便查看边界，允许滚动
    lv_obj_set_style_bg_color(*content_container, theme_get_color(theme_get_current_theme()->colors.background),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(*content_container, LV_OPA_30, LV_PART_MAIN); // 半透明背景
    lv_obj_set_style_border_width(*content_container, 1, LV_PART_MAIN);   // 添加边框以便查看边界
    lv_obj_set_style_border_color(*content_container, theme_get_color(theme_get_current_theme()->colors.border),
                                  LV_PART_MAIN);
    lv_obj_set_style_pad_all(*content_container, 0, LV_PART_MAIN); // 移除内边距，避免横向滚动
    lv_obj_set_style_radius(*content_container, 0, LV_PART_MAIN);  // 无圆角
    lv_obj_add_flag(*content_container, LV_OBJ_FLAG_SCROLLABLE);   // 允许滚动

    ESP_LOGI(TAG, "Page content area created (scrollable)");
}

// 创建统一的页面标题（保留原有函数以兼容性）
void ui_create_page_title(lv_obj_t* parent, const char* title_text) {
    // 创建标题容器 - 统一高度30像素
    lv_obj_t* title_container = lv_obj_create(parent);
    lv_obj_set_size(title_container, 240, 30);
    lv_obj_align(title_container, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_opa(title_container, LV_OPA_0, LV_PART_MAIN); // 透明背景
    lv_obj_set_style_border_width(title_container, 0, 0);
    lv_obj_set_style_pad_all(title_container, 0, 0);
    lv_obj_clear_flag(title_container, LV_OBJ_FLAG_SCROLLABLE); // 禁止滚动

    // 创建标题标签
    lv_obj_t* title = lv_label_create(title_container);
    lv_label_set_text(title, title_text);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);                   // 居中对齐
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0); // 统一使用20号字体
    lv_obj_set_style_text_color(title, theme_get_color(theme_get_current_theme()->colors.text_primary), 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_SCROLLABLE); // 禁止滚动

    ESP_LOGI(TAG, "Page title created: %s", title_text);
}
