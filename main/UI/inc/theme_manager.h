/**
 * @file theme_manager.h
 * @brief 主题管理系统
 * @author TidyCraze
 * @date 2025-08-14
 */

#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// 主题类型枚举
typedef enum {
    THEME_MORANDI = 0, // 莫兰迪色系
    THEME_DARK = 1,    // 深色主题
    THEME_LIGHT = 2,   // 浅色主题
    THEME_BLUE = 3,    // 蓝色主题
    THEME_GREEN = 4,   // 绿色主题
    THEME_COUNT        // 主题总数
} theme_type_t;

// 主题颜色结构
typedef struct {
    uint32_t background;     // 背景色
    uint32_t surface;        // 表面色（容器背景）
    uint32_t primary;        // 主色调
    uint32_t secondary;      // 次色调
    uint32_t accent;         // 强调色
    uint32_t text_primary;   // 主要文字色
    uint32_t text_secondary; // 次要文字色
    uint32_t text_inverse;   // 反色文字
    uint32_t border;         // 边框色
    uint32_t shadow;         // 阴影色
    uint32_t success;        // 成功色
    uint32_t warning;        // 警告色
    uint32_t error;          // 错误色
} theme_colors_t;

// 主题结构
typedef struct {
    const char* name;            // 主题名称
    theme_colors_t colors;       // 主题颜色
    const lv_font_t* title_font; // 标题字体
    const lv_font_t* body_font;  // 正文字体
    const lv_font_t* small_font; // 小字体
} theme_t;

// 主题管理函数
esp_err_t theme_manager_init(void);
theme_type_t theme_get_current(void);
const theme_t* theme_get_theme(theme_type_t type);
const theme_t* theme_get_current_theme(void);
esp_err_t theme_set_current(theme_type_t type);
esp_err_t theme_save_setting(theme_type_t type);
theme_type_t theme_load_setting(void);

// 主题应用函数
void theme_apply_to_screen(lv_obj_t* screen);
void theme_apply_to_container(lv_obj_t* container);
void theme_apply_to_button(lv_obj_t* button, bool is_primary);
void theme_apply_to_label(lv_obj_t* label, bool is_title);
void theme_apply_to_switch(lv_obj_t* switch_obj);

// 颜色获取函数
lv_color_t theme_get_color(uint32_t hex_color);
lv_color_t theme_get_background_color(void);
lv_color_t theme_get_surface_color(void);
lv_color_t theme_get_primary_color(void);
lv_color_t theme_get_text_color(void);

#ifdef __cplusplus
}
#endif

#endif // THEME_MANAGER_H
