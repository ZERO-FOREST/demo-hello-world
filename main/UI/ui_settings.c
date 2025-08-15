/**
 * @file ui_settings.c
 * @brief 设置界面实现
 * @author TidyCraze
 * @date 2025-08-14
 */
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "theme_manager.h"
#include "ui.h"

static const char* TAG = "UI_SETTINGS";

// 语言设置 (定义在ui.h中)

// 全局语言设置（临时改为英文，直到添加中文字体）
static ui_language_t g_current_language = LANG_ENGLISH;

// 语言文本定义
typedef struct {
    const char* settings_title;
    const char* language_label;
    const char* theme_label;
    const char* about_label;
    const char* back_button;
    const char* english_text;
    const char* chinese_text;
    const char* light_theme;
    const char* dark_theme;
    const char* version_info;
    const char* language_changed;
} ui_text_t;

// 英文文本
static const ui_text_t english_text = {.settings_title = "SETTINGS",
                                       .language_label = "Language:",
                                       .theme_label = "Theme:",
                                       .about_label = "About:",
                                       .back_button = "Back",
                                       .english_text = "English",
                                       .chinese_text = "Chinese",
                                       .light_theme = "Light",
                                       .dark_theme = "Dark",
                                       .version_info = "ESP32-S3 Demo v1.0.0",
                                       .language_changed = "Language Changed!"};

// 中文文本（需要中文字体支持）
static const ui_text_t chinese_text = {.settings_title = "设置",
                                       .language_label = "语言:",
                                       .theme_label = "主题:",
                                       .about_label = "关于:",
                                       .back_button = "返回",
                                       .english_text = "英文",
                                       .chinese_text = "中文",
                                       .light_theme = "浅色",
                                       .dark_theme = "深色",
                                       .version_info = "ESP32-S3 演示 v1.0.0",
                                       .language_changed = "语言已切换!"};

// 获取当前语言文本
static const ui_text_t* get_current_text(void) {
    return (g_current_language == LANG_CHINESE) ? &chinese_text : &english_text;
}

// 获取当前字体
static const lv_font_t* get_current_font(void) {
    if (g_current_language == LANG_CHINESE) {
        // 如果有中文字体，返回中文字体
        // return &ui_font_chinese_16;
        return &lv_font_montserrat_16; // 临时使用英文字体
    }
    return &lv_font_montserrat_16;
}

// NVS保存语言设置
static void save_language_setting(ui_language_t lang) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("ui_settings", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_u8(nvs_handle, "language", (uint8_t)lang);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Language setting saved: %s", lang == LANG_CHINESE ? "Chinese" : "English");
    }
}

// NVS读取语言设置
static ui_language_t load_language_setting(void) {
    nvs_handle_t nvs_handle;
    uint8_t lang = LANG_ENGLISH;
    esp_err_t err = nvs_open("ui_settings", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t required_size = sizeof(uint8_t);
        err = nvs_get_u8(nvs_handle, "language", &lang);
        nvs_close(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Language setting loaded: %s", lang == LANG_CHINESE ? "Chinese" : "English");
        }
    }
    return (ui_language_t)lang;
}

// 语言切换回调
static void language_switch_cb(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    bool is_chinese = lv_obj_has_state(sw, LV_STATE_CHECKED);

    g_current_language = is_chinese ? LANG_CHINESE : LANG_ENGLISH;
    save_language_setting(g_current_language);

    // 显示切换提示
    lv_obj_t* screen = lv_scr_act();
    lv_obj_t* msgbox = lv_msgbox_create(screen, "Info", get_current_text()->language_changed, NULL, true);
    lv_obj_center(msgbox);

    ESP_LOGI(TAG, "Language switched to: %s", is_chinese ? "Chinese" : "English");
}

// 返回按钮回调
static void back_btn_cb(lv_event_t* e) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);
        ui_main_menu_create(screen); // 返回主菜单
    }
}

// 关于信息回调
static void about_btn_cb(lv_event_t* e) {
    lv_obj_t* screen = lv_scr_act();
    const char* about_msg = "ESP32-S3 Demo System\n\n"
                            "Features:\n"
                            "• LVGL GUI\n"
                            "• WiFi Management\n"
                            "• Power Management\n"
                            "• Multi-language Support\n\n"
                            "Hardware: ESP32-S3-N16R8\n"
                            "Display: ST7789 240x282";

    lv_obj_t* msgbox = lv_msgbox_create(screen, "About", about_msg, NULL, true);
    lv_obj_set_size(msgbox, 280, 200);
    lv_obj_center(msgbox);
}

// 主题切换回调
static void theme_switch_cb(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    bool is_dark = lv_obj_has_state(sw, LV_STATE_CHECKED);

    // 切换主题
    theme_type_t new_theme = is_dark ? THEME_DARK : THEME_MORANDI;
    theme_set_current(new_theme);

    // 应用新主题到当前屏幕
    lv_obj_t* screen = lv_scr_act();
    theme_apply_to_screen(screen);

    // 显示切换提示
    const theme_t* theme = theme_get_current_theme();
    lv_obj_t* msgbox = lv_msgbox_create(screen, "Theme Changed", theme->name, NULL, true);
    lv_obj_center(msgbox);

    ESP_LOGI(TAG, "Theme switched to: %s", theme->name);
}

// 主题下拉列表回调
static void theme_dropdown_cb(lv_event_t* e) {
    lv_obj_t* dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);

    // 根据选择的索引确定主题类型
    theme_type_t theme_type;
    switch (selected) {
    case 0:
        theme_type = THEME_MORANDI;
        break;
    case 1:
        theme_type = THEME_DARK;
        break;
    case 2:
        theme_type = THEME_LIGHT;
        break;
    case 3:
        theme_type = THEME_BLUE;
        break;
    case 4:
        theme_type = THEME_GREEN;
        break;
    default:
        theme_type = THEME_MORANDI;
        break;
    }

    // 设置新主题
    theme_set_current(theme_type);

    // 应用新主题到当前屏幕
    lv_obj_t* screen = lv_scr_act();
    theme_apply_to_screen(screen);

    // 显示切换提示
    const theme_t* theme = theme_get_current_theme();
    lv_obj_t* msgbox = lv_msgbox_create(screen, "Theme Changed", theme->name, NULL, true);
    lv_obj_center(msgbox);

    ESP_LOGI(TAG, "Theme switched to: %s", theme->name);
}

// 创建设置界面
void ui_settings_create(lv_obj_t* parent) {
    // 加载保存的语言设置
    g_current_language = load_language_setting();
    const ui_text_t* text = get_current_text();
    const lv_font_t* font = get_current_font();

    // 应用当前主题到屏幕
    theme_apply_to_screen(parent);

    // 创建页面父级容器（统一管理整个页面）
    lv_obj_t* page_parent_container;
    ui_create_page_parent_container(parent, &page_parent_container);

    // 创建顶部栏容器（包含返回按钮和标题）
    lv_obj_t* top_bar_container;
    lv_obj_t* title_container;
    ui_create_top_bar(page_parent_container, text->settings_title, &top_bar_container, &title_container);

    // 创建页面内容容器
    lv_obj_t* content_container;
    ui_create_page_content_area(page_parent_container, &content_container);

    // 语言设置
    lv_obj_t* lang_label = lv_label_create(content_container);
    lv_label_set_text(lang_label, text->language_label);
    theme_apply_to_label(lang_label, false); // 应用主题到标签
    lv_obj_align(lang_label, LV_ALIGN_TOP_LEFT, 10, 10);

    lv_obj_t* lang_switch = lv_switch_create(content_container);
    lv_obj_align(lang_switch, LV_ALIGN_TOP_RIGHT, -10, 10);
    theme_apply_to_switch(lang_switch); // 应用主题到开关
    if (g_current_language == LANG_CHINESE) {
        lv_obj_add_state(lang_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(lang_switch, language_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 语言状态标签
    lv_obj_t* lang_status = lv_label_create(content_container);
    lv_label_set_text(lang_status, g_current_language == LANG_CHINESE ? text->chinese_text : text->english_text);
    theme_apply_to_label(lang_status, false); // 应用主题到标签
    lv_obj_align_to(lang_status, lang_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);

    // 主题设置
    lv_obj_t* theme_label = lv_label_create(content_container);
    lv_label_set_text(theme_label, text->theme_label);
    theme_apply_to_label(theme_label, false); // 应用主题到标签
    lv_obj_align_to(theme_label, lang_status, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20);

    // 创建主题下拉列表容器
    lv_obj_t* theme_dropdown_cont = lv_obj_create(content_container);
    lv_obj_set_size(theme_dropdown_cont, 200, 40);
    lv_obj_align_to(theme_dropdown_cont, theme_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
    lv_obj_set_style_bg_opa(theme_dropdown_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(theme_dropdown_cont, 0, 0);
    lv_obj_set_style_pad_all(theme_dropdown_cont, 0, 0);

    // 创建主题下拉列表
    lv_obj_t* theme_dropdown = lv_dropdown_create(theme_dropdown_cont);
    lv_obj_set_size(theme_dropdown, 200, 35);
    lv_obj_align(theme_dropdown, LV_ALIGN_CENTER, 0, 0);

    // 设置下拉列表样式
    lv_obj_set_style_radius(theme_dropdown, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(theme_dropdown, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(theme_dropdown, theme_get_color(theme_get_current_theme()->colors.border),
                                  LV_PART_MAIN);
    lv_obj_set_style_bg_color(theme_dropdown, theme_get_color(theme_get_current_theme()->colors.surface), LV_PART_MAIN);
    lv_obj_set_style_text_color(theme_dropdown, theme_get_color(theme_get_current_theme()->colors.text_primary),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(theme_dropdown, &lv_font_montserrat_14, LV_PART_MAIN);

    // 设置下拉列表选项
    lv_dropdown_set_options(theme_dropdown, "Morandi\nDark\nLight\nBlue\nGreen");

    // 设置当前选中的主题
    theme_type_t current_theme_type = theme_get_current();
    int selected_index = 0;
    switch (current_theme_type) {
    case THEME_MORANDI:
        selected_index = 0;
        break;
    case THEME_DARK:
        selected_index = 1;
        break;
    case THEME_LIGHT:
        selected_index = 2;
        break;
    case THEME_BLUE:
        selected_index = 3;
        break;
    case THEME_GREEN:
        selected_index = 4;
        break;
    default:
        selected_index = 0;
        break;
    }
    lv_dropdown_set_selected(theme_dropdown, selected_index);

    // 添加下拉列表事件回调
    lv_obj_add_event_cb(theme_dropdown, theme_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 关于按钮
    lv_obj_t* about_btn = lv_btn_create(content_container);
    lv_obj_set_size(about_btn, 220, 35);
    lv_obj_align(about_btn, LV_ALIGN_BOTTOM_MID, 0, -10);

    // 设置关于按钮样式
    lv_obj_set_style_radius(about_btn, 6, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(about_btn, 3, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(about_btn, LV_OPA_30, LV_PART_MAIN);

    theme_apply_to_button(about_btn, true); // 应用主题到按钮（只改变颜色）
    lv_obj_add_event_cb(about_btn, about_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* about_label = lv_label_create(about_btn);
    lv_label_set_text(about_label, text->about_label);
    theme_apply_to_label(about_label, false); // 应用主题到标签
    lv_obj_center(about_label);

    ESP_LOGI(TAG, "Settings UI created with language: %s", g_current_language == LANG_CHINESE ? "Chinese" : "English");
}

// 获取当前语言设置
ui_language_t ui_get_current_language(void) { return g_current_language; }

// 设置语言
void ui_set_language(ui_language_t lang) {
    g_current_language = lang;
    save_language_setting(lang);
}