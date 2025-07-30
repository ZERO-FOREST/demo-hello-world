/**
 * @file ui_settings.c
 * @brief 设置界面实现
 * @author Your Name
 * @date 2024
 */
#include "ui.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "UI_SETTINGS";

// 语言设置 (定义在ui.h中)

// 全局语言设置（临时改为英文，直到添加中文字体）
static ui_language_t g_current_language = LANG_ENGLISH;

// 语言文本定义
typedef struct {
    const char *settings_title;
    const char *language_label;
    const char *theme_label;
    const char *about_label;
    const char *back_button;
    const char *english_text;
    const char *chinese_text;
    const char *light_theme;
    const char *dark_theme;
    const char *version_info;
    const char *language_changed;
} ui_text_t;

// 英文文本
static const ui_text_t english_text = {
    .settings_title = "SETTINGS",
    .language_label = "Language:",
    .theme_label = "Theme:",
    .about_label = "About:",
    .back_button = "Back",
    .english_text = "English",
    .chinese_text = "Chinese",
    .light_theme = "Light",
    .dark_theme = "Dark",
    .version_info = "ESP32-S3 Demo v1.0.0",
    .language_changed = "Language Changed!"
};

// 中文文本（需要中文字体支持）
static const ui_text_t chinese_text = {
    .settings_title = "设置",
    .language_label = "语言:",
    .theme_label = "主题:",
    .about_label = "关于:",
    .back_button = "返回",
    .english_text = "英文",
    .chinese_text = "中文",
    .light_theme = "浅色",
    .dark_theme = "深色",
    .version_info = "ESP32-S3 演示 v1.0.0",
    .language_changed = "语言已切换!"
};

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
        ESP_LOGI(TAG, "Language setting saved: %s", 
                 lang == LANG_CHINESE ? "Chinese" : "English");
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
            ESP_LOGI(TAG, "Language setting loaded: %s", 
                     lang == LANG_CHINESE ? "Chinese" : "English");
        }
    }
    return (ui_language_t)lang;
}

// 语言切换回调
static void language_switch_cb(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    bool is_chinese = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    g_current_language = is_chinese ? LANG_CHINESE : LANG_ENGLISH;
    save_language_setting(g_current_language);
    
    // 显示切换提示
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *msgbox = lv_msgbox_create(screen, "Info", 
                                        get_current_text()->language_changed, 
                                        NULL, true);
    lv_obj_center(msgbox);
    
    ESP_LOGI(TAG, "Language switched to: %s", 
             is_chinese ? "Chinese" : "English");
}

// 返回按钮回调
static void back_btn_cb(lv_event_t *e) {
    lv_obj_t *screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);
        ui_main_menu_create(screen); // 返回主菜单
    }
}

// 关于信息回调
static void about_btn_cb(lv_event_t *e) {
    lv_obj_t *screen = lv_scr_act();
    const char *about_msg = "ESP32-S3 Demo System\n\n"
                           "Features:\n"
                           "• LVGL GUI\n"
                           "• WiFi Management\n"
                           "• Power Management\n"
                           "• Multi-language Support\n\n"
                           "Hardware: ESP32-S3-N16R8\n"
                           "Display: ST7789 240x282";
    
    lv_obj_t *msgbox = lv_msgbox_create(screen, "About", about_msg, NULL, true);
    lv_obj_set_size(msgbox, 280, 200);
    lv_obj_center(msgbox);
}

// 主题切换回调
static void theme_switch_cb(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    bool is_dark = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    // 这里可以实现主题切换逻辑
    ESP_LOGI(TAG, "Theme switched to: %s", is_dark ? "Dark" : "Light");
    
    // 简单的主题切换演示
    lv_obj_t *screen = lv_scr_act();
    if (is_dark) {
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a2e), 0);
    } else {
        lv_obj_set_style_bg_color(screen, lv_color_hex(0xf0f0f0), 0);
    }
}

// 创建设置界面
void ui_settings_create(lv_obj_t *parent) {
    // 加载保存的语言设置
    g_current_language = load_language_setting();
    const ui_text_t *text = get_current_text();
    const lv_font_t *font = get_current_font();
    
    // 设置背景
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xf5f5f5), 0);
    
    // 创建标题
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, text->settings_title);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // 创建设置容器
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 260, 180);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_color(cont, lv_color_white(), 0);
    lv_obj_set_style_border_width(cont, 1, 0);
    lv_obj_set_style_border_color(cont, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_radius(cont, 8, 0);
    
    // 语言设置
    lv_obj_t *lang_label = lv_label_create(cont);
    lv_label_set_text(lang_label, text->language_label);
    lv_obj_set_style_text_font(lang_label, font, 0);
    lv_obj_align(lang_label, LV_ALIGN_TOP_LEFT, 15, 15);
    
    lv_obj_t *lang_switch = lv_switch_create(cont);
    lv_obj_align(lang_switch, LV_ALIGN_TOP_RIGHT, -15, 10);
    if (g_current_language == LANG_CHINESE) {
        lv_obj_add_state(lang_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(lang_switch, language_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // 语言状态标签
    lv_obj_t *lang_status = lv_label_create(cont);
    lv_label_set_text(lang_status, g_current_language == LANG_CHINESE ? 
                      text->chinese_text : text->english_text);
    lv_obj_set_style_text_font(lang_status, font, 0);
    lv_obj_set_style_text_color(lang_status, lv_color_hex(0x666666), 0);
    lv_obj_align_to(lang_status, lang_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
    
    // 主题设置
    lv_obj_t *theme_label = lv_label_create(cont);
    lv_label_set_text(theme_label, text->theme_label);
    lv_obj_set_style_text_font(theme_label, font, 0);
    lv_obj_align_to(theme_label, lang_status, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20);
    
    lv_obj_t *theme_switch = lv_switch_create(cont);
    lv_obj_align(theme_switch, LV_ALIGN_TOP_RIGHT, -15, 70);
    lv_obj_add_event_cb(theme_switch, theme_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t *theme_status = lv_label_create(cont);
    lv_label_set_text(theme_status, text->light_theme);
    lv_obj_set_style_text_font(theme_status, font, 0);
    lv_obj_set_style_text_color(theme_status, lv_color_hex(0x666666), 0);
    lv_obj_align_to(theme_status, theme_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
    
    // 关于按钮
    lv_obj_t *about_btn = lv_btn_create(cont);
    lv_obj_set_size(about_btn, 220, 35);
    lv_obj_align(about_btn, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_color(about_btn, lv_color_hex(0x00d4aa), 0);
    lv_obj_add_event_cb(about_btn, about_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *about_label = lv_label_create(about_btn);
    lv_label_set_text(about_label, text->about_label);
    lv_obj_set_style_text_font(about_label, font, 0);
    lv_obj_set_style_text_color(about_label, lv_color_white(), 0);
    lv_obj_center(about_label);
    
    // 返回按钮
    lv_obj_t *back_btn = lv_btn_create(parent);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x666666), 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, text->back_button);
    lv_obj_set_style_text_font(back_label, font, 0);
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
    lv_obj_center(back_label);
    
    // 版本信息
    lv_obj_t *version = lv_label_create(parent);
    lv_label_set_text(version, text->version_info);
    lv_obj_set_style_text_font(version, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(version, lv_color_hex(0x999999), 0);
    lv_obj_align(version, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    
    ESP_LOGI(TAG, "Settings UI created with language: %s", 
             g_current_language == LANG_CHINESE ? "Chinese" : "English");
}

// 获取当前语言设置
ui_language_t ui_get_current_language(void) {
    return g_current_language;
}

// 设置语言
void ui_set_language(ui_language_t lang) {
    g_current_language = lang;
    save_language_setting(lang);
} 