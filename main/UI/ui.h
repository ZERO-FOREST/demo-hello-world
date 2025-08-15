/**
 * @file UI.h
 * @brief UI模块统一头文件
 * @author Your Name
 * @date 2024
 */
#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "theme_manager.h"

// --- LVGL 主任务 ---
// 这个任务初始化并运行LVGL的主循环
void lvgl_main_task(void *pvParameters);


// --- 启动动画 UI ---

/**
 * @brief 启动动画完成时的回调函数类型
 */
typedef void (*ui_start_anim_finished_cb_t)(void);

/**
 * @brief 创建并开始播放启动动画
 * @param parent    父对象，通常是 lv_scr_act()
 * @param finished_cb 动画播放完成时调用的回调函数
 */
void ui_start_animation_create(lv_obj_t *parent, ui_start_anim_finished_cb_t finished_cb);

/**
 * @brief 创建主菜单界面
 * @param parent 父对象，通常是 lv_scr_act()
 */
void ui_main_menu_create(lv_obj_t *parent);

/**
 * @brief 创建WiFi设置界面
 * @param parent 父对象，通常是 lv_scr_act()
 */
void ui_wifi_settings_create(lv_obj_t *parent);

/**
 * @brief 创建系统设置界面
 * @param parent 父对象，通常是 lv_scr_act()
 */
void ui_settings_create(lv_obj_t *parent);

/**
 * @brief 更新电池电量显示
 * 由任务调用，每5分钟更新一次
 */
void ui_main_update_battery_display(void);

// --- 语言设置相关 ---
// 语言类型枚举
typedef enum {
    LANG_ENGLISH = 0,
    LANG_CHINESE = 1
} ui_language_t;

/**
 * @brief 获取当前语言设置
 * @return 当前语言
 */
ui_language_t ui_get_current_language(void);

/**
 * @brief 设置语言
 * @param lang 要设置的语言
 */
void ui_set_language(ui_language_t lang);


// --- 在这里添加您未来的其他UI模块声明 ---
//
// void ui_other_screen_create(lv_obj_t* parent);
//
// ---

// --- 串口显示界面 ---
void ui_serial_display_create(lv_obj_t *parent);
void ui_serial_display_destroy(void);
void ui_serial_display_add_data(const char *data, size_t len);
void ui_serial_display_add_text(const char *text);

// --- 校准和测试界面 ---
void ui_calibration_create(lv_obj_t *parent);
void ui_calibration_destroy(void);

// 统一的back按钮创建函数
void ui_create_back_button(lv_obj_t* parent, const char* text);
void ui_create_game_back_button(lv_obj_t* parent, const char* text);


#ifdef __cplusplus
}
#endif

#endif // UI_H 