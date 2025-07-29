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


// --- 在这里添加您未来的其他UI模块声明 ---
//
// void ui_main_screen_create(lv_obj_t* parent);
// void ui_settings_screen_create(lv_obj_t* parent);
//
// ---


#ifdef __cplusplus
}
#endif

#endif // UI_H 