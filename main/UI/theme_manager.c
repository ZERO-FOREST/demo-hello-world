/**
 * @file theme_manager.c
 * @brief 主题管理系统实现
 * @author TidyCraze
 * @date 2025-08-14
 */

#include "theme_manager.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char* TAG = "THEME_MANAGER";

// 当前主题
static theme_type_t g_current_theme = THEME_MORANDI;

// 莫兰迪主题
static const theme_t morandi_theme = {.name = "Morandi",
                                      .colors =
                                          {
                                              .background = 0xF6E9DB,     // 莫兰迪米色背景
                                              .surface = 0xFFFFFF,        // 白色表面
                                              .primary = 0xAB9E96,        // 莫兰迪棕色
                                              .secondary = 0xBCA79E,      // 莫兰迪浅棕
                                              .accent = 0xC8BAAF,         // 莫兰迪米棕
                                              .text_primary = 0x2C2C2C,   // 深灰文字
                                              .text_secondary = 0x666666, // 中灰文字
                                              .text_inverse = 0xFFFFFF,   // 白色文字
                                              .border = 0xE0E0E0,         // 浅灰边框
                                              .shadow = 0x000000,         // 黑色阴影
                                              .success = 0x4CAF50,        // 绿色成功
                                              .warning = 0xFF9800,        // 橙色警告
                                              .error = 0xF44336           // 红色错误
                                          },
                                      .title_font = &lv_font_montserrat_24,
                                      .body_font = &lv_font_montserrat_16,
                                      .small_font = &lv_font_montserrat_14};

// 深色主题
static const theme_t dark_theme = {.name = "Dark",
                                   .colors =
                                       {
                                           .background = 0x1A1A2E,     // 深蓝黑背景
                                           .surface = 0x16213E,        // 深蓝表面
                                           .primary = 0x0F3460,        // 深蓝主色
                                           .secondary = 0x533483,      // 紫色次色
                                           .accent = 0xE94560,         // 红色强调
                                           .text_primary = 0xFFFFFF,   // 白色文字
                                           .text_secondary = 0xB0B0B0, // 浅灰文字
                                           .text_inverse = 0x000000,   // 黑色文字
                                           .border = 0x2D2D2D,         // 深灰边框
                                           .shadow = 0x000000,         // 黑色阴影
                                           .success = 0x4CAF50,        // 绿色成功
                                           .warning = 0xFF9800,        // 橙色警告
                                           .error = 0xF44336           // 红色错误
                                       },
                                   .title_font = &lv_font_montserrat_24,
                                   .body_font = &lv_font_montserrat_16,
                                   .small_font = &lv_font_montserrat_14};

// 浅色主题
static const theme_t light_theme = {.name = "Light",
                                    .colors =
                                        {
                                            .background = 0xFFFFFF,     // 白色背景
                                            .surface = 0xF5F5F5,        // 浅灰表面
                                            .primary = 0x2196F3,        // 蓝色主色
                                            .secondary = 0x03A9F4,      // 浅蓝次色
                                            .accent = 0xFF5722,         // 橙色强调
                                            .text_primary = 0x212121,   // 深灰文字
                                            .text_secondary = 0x757575, // 中灰文字
                                            .text_inverse = 0xFFFFFF,   // 白色文字
                                            .border = 0xE0E0E0,         // 浅灰边框
                                            .shadow = 0x000000,         // 黑色阴影
                                            .success = 0x4CAF50,        // 绿色成功
                                            .warning = 0xFF9800,        // 橙色警告
                                            .error = 0xF44336           // 红色错误
                                        },
                                    .title_font = &lv_font_montserrat_24,
                                    .body_font = &lv_font_montserrat_16,
                                    .small_font = &lv_font_montserrat_14};

// 蓝色主题
static const theme_t blue_theme = {.name = "Blue",
                                   .colors =
                                       {
                                           .background = 0xE3F2FD,     // 浅蓝背景
                                           .surface = 0xFFFFFF,        // 白色表面
                                           .primary = 0x1976D2,        // 深蓝主色
                                           .secondary = 0x42A5F5,      // 中蓝次色
                                           .accent = 0xFFC107,         // 黄色强调
                                           .text_primary = 0x1565C0,   // 深蓝文字
                                           .text_secondary = 0x5E92F3, // 中蓝文字
                                           .text_inverse = 0xFFFFFF,   // 白色文字
                                           .border = 0xBBDEFB,         // 浅蓝边框
                                           .shadow = 0x1976D2,         // 深蓝阴影
                                           .success = 0x4CAF50,        // 绿色成功
                                           .warning = 0xFF9800,        // 橙色警告
                                           .error = 0xF44336           // 红色错误
                                       },
                                   .title_font = &lv_font_montserrat_24,
                                   .body_font = &lv_font_montserrat_16,
                                   .small_font = &lv_font_montserrat_14};

// 绿色主题
static const theme_t green_theme = {.name = "Green",
                                    .colors =
                                        {
                                            .background = 0xE8F5E8,     // 浅绿背景
                                            .surface = 0xFFFFFF,        // 白色表面
                                            .primary = 0x388E3C,        // 深绿主色
                                            .secondary = 0x66BB6A,      // 中绿次色
                                            .accent = 0xFF6F00,         // 橙色强调
                                            .text_primary = 0x2E7D32,   // 深绿文字
                                            .text_secondary = 0x558B2F, // 中绿文字
                                            .text_inverse = 0xFFFFFF,   // 白色文字
                                            .border = 0xC8E6C9,         // 浅绿边框
                                            .shadow = 0x388E3C,         // 深绿阴影
                                            .success = 0x4CAF50,        // 绿色成功
                                            .warning = 0xFF9800,        // 橙色警告
                                            .error = 0xF44336           // 红色错误
                                        },
                                    .title_font = &lv_font_montserrat_24,
                                    .body_font = &lv_font_montserrat_16,
                                    .small_font = &lv_font_montserrat_14};

// 主题数组
static const theme_t* themes[] = {&morandi_theme, &dark_theme, &light_theme, &blue_theme, &green_theme};

// 定义每个主题的按钮颜色数组
static lv_color_t morandi_button_colors[3];
static lv_color_t dark_button_colors[3];
static lv_color_t light_button_colors[3];
static lv_color_t blue_button_colors[3];
static lv_color_t green_button_colors[3];

// 初始化按钮颜色数组
static void init_button_colors() {
    morandi_button_colors[0] = lv_color_hex(0xAB9E96);
    morandi_button_colors[1] = lv_color_hex(0xBCA79E);
    morandi_button_colors[2] = lv_color_hex(0xC8BAAF);

    dark_button_colors[0] = lv_color_hex(0x0F3460);
    dark_button_colors[1] = lv_color_hex(0x533483);
    dark_button_colors[2] = lv_color_hex(0xE94560);

    light_button_colors[0] = lv_color_hex(0x2196F3);
    light_button_colors[1] = lv_color_hex(0x03A9F4);
    light_button_colors[2] = lv_color_hex(0xFF5722);

    blue_button_colors[0] = lv_color_hex(0x1976D2);
    blue_button_colors[1] = lv_color_hex(0x42A5F5);
    blue_button_colors[2] = lv_color_hex(0xFFC107);

    green_button_colors[0] = lv_color_hex(0x388E3C);
    green_button_colors[1] = lv_color_hex(0x66BB6A);
    green_button_colors[2] = lv_color_hex(0xFF6F00);
}

// 获取当前主题的按钮颜色数组
static const lv_color_t* get_current_button_colors(size_t* count) {
    const theme_t* theme = theme_get_current_theme();
    if (theme == &morandi_theme) {
        *count = sizeof(morandi_button_colors) / sizeof(lv_color_t);
        return morandi_button_colors;
    } else if (theme == &dark_theme) {
        *count = sizeof(dark_button_colors) / sizeof(lv_color_t);
        return dark_button_colors;
    } else if (theme == &light_theme) {
        *count = sizeof(light_button_colors) / sizeof(lv_color_t);
        return light_button_colors;
    } else if (theme == &blue_theme) {
        *count = sizeof(blue_button_colors) / sizeof(lv_color_t);
        return blue_button_colors;
    } else if (theme == &green_theme) {
        *count = sizeof(green_button_colors) / sizeof(lv_color_t);
        return green_button_colors;
    }
    *count = 0;
    return NULL;
}

// 主题管理器初始化
esp_err_t theme_manager_init(void) {
    init_button_colors(); // 初始化按钮颜色
    g_current_theme = theme_load_setting();
    ESP_LOGI(TAG, "Theme manager initialized with theme: %s", themes[g_current_theme]->name);
    return ESP_OK;
}

// 获取当前主题类型
theme_type_t theme_get_current(void) { return g_current_theme; }

// 获取指定主题
const theme_t* theme_get_theme(theme_type_t type) {
    if (type >= THEME_COUNT) {
        return themes[THEME_MORANDI]; // 默认返回莫兰迪主题
    }
    return themes[type];
}

// 获取当前主题
const theme_t* theme_get_current_theme(void) { return themes[g_current_theme]; }

// 设置当前主题
esp_err_t theme_set_current(theme_type_t type) {
    if (type >= THEME_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    g_current_theme = type;
    theme_save_setting(type);
    ESP_LOGI(TAG, "Theme changed to: %s", themes[type]->name);
    return ESP_OK;
}

// 保存主题设置到NVS
esp_err_t theme_save_setting(theme_type_t type) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("ui_settings", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_u8(nvs_handle, "theme", (uint8_t)type);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Theme setting saved: %s", themes[type]->name);
    }
    return err;
}

// 从NVS加载主题设置
theme_type_t theme_load_setting(void) {
    nvs_handle_t nvs_handle;
    uint8_t theme = THEME_MORANDI;
    esp_err_t err = nvs_open("ui_settings", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t required_size = sizeof(uint8_t);
        err = nvs_get_u8(nvs_handle, "theme", &theme);
        nvs_close(nvs_handle);
        if (err == ESP_OK && theme < THEME_COUNT) {
            ESP_LOGI(TAG, "Theme setting loaded: %s", themes[theme]->name);
            return (theme_type_t)theme;
        }
    }
    return THEME_MORANDI; // 默认返回莫兰迪主题
}

// 应用主题到屏幕
void theme_apply_to_screen(lv_obj_t* screen) {
    if (!screen)
        return;

    const theme_t* theme = theme_get_current_theme();
    lv_obj_set_style_bg_color(screen, theme_get_color(theme->colors.background), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_invalidate(screen);
}

// 应用主题到容器
void theme_apply_to_container(lv_obj_t* container) {
    if (!container)
        return;

    const theme_t* theme = theme_get_current_theme();
    // 只改变颜色，不改变样式
    lv_obj_set_style_bg_color(container, theme_get_color(theme->colors.surface), LV_PART_MAIN);
    lv_obj_set_style_border_color(container, theme_get_color(theme->colors.border), LV_PART_MAIN);
}

// 应用主题到按钮
void theme_apply_to_button(lv_obj_t* button, bool is_primary) {
    if (!button)
        return;

    size_t color_count;
    const lv_color_t* button_colors = get_current_button_colors(&color_count);
    if (button_colors && color_count > 0) {
        static int color_index = 0;
        lv_color_t bg_color = button_colors[color_index % color_count];
        color_index++;

        // 只改变颜色，不改变样式
        lv_obj_set_style_bg_color(button, bg_color, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(button, theme_get_color(theme_get_current_theme()->colors.text_inverse),
                                    LV_PART_MAIN);
    }
}

// 应用主题到标签
void theme_apply_to_label(lv_obj_t* label, bool is_title) {
    if (!label)
        return;

    const theme_t* theme = theme_get_current_theme();
    lv_color_t text_color =
        is_title ? theme_get_color(theme->colors.text_primary) : theme_get_color(theme->colors.text_secondary);

    lv_obj_set_style_text_color(label, text_color, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, is_title ? theme->title_font : theme->body_font, LV_PART_MAIN);
}

// 应用主题到开关
void theme_apply_to_switch(lv_obj_t* switch_obj) {
    if (!switch_obj)
        return;

    const theme_t* theme = theme_get_current_theme();
    lv_obj_set_style_bg_color(switch_obj, theme_get_color(theme->colors.secondary), LV_PART_MAIN);
    lv_obj_set_style_bg_color(switch_obj, theme_get_color(theme->colors.primary), LV_PART_INDICATOR);
}

// 颜色转换函数
lv_color_t theme_get_color(uint32_t hex_color) { return lv_color_hex(hex_color); }

// 获取背景色
lv_color_t theme_get_background_color(void) {
    const theme_t* theme = theme_get_current_theme();
    return theme_get_color(theme->colors.background);
}

// 获取表面色
lv_color_t theme_get_surface_color(void) {
    const theme_t* theme = theme_get_current_theme();
    return theme_get_color(theme->colors.surface);
}

// 获取主色调
lv_color_t theme_get_primary_color(void) {
    const theme_t* theme = theme_get_current_theme();
    return theme_get_color(theme->colors.primary);
}

// 获取文字色
lv_color_t theme_get_text_color(void) {
    const theme_t* theme = theme_get_current_theme();
    return theme_get_color(theme->colors.text_primary);
}
