#ifndef UI_NUMERIC_KEYPAD_H
#define UI_NUMERIC_KEYPAD_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// 小键盘回调函数类型
typedef void (*numeric_keypad_cb_t)(const char* password, void* user_data);

/**
 * @brief 创建数字小键盘
 * @param parent 父容器
 * @param title 标题
 * @param current_password 当前密码（用于显示）
 * @param callback 确认回调函数
 * @param user_data 用户数据
 * @return 小键盘对象
 */
lv_obj_t* ui_numeric_keypad_create(lv_obj_t* parent, const char* title, 
                                   const char* current_password,
                                   numeric_keypad_cb_t callback, 
                                   void* user_data);

/**
 * @brief 销毁数字小键盘
 * @param keypad 小键盘对象
 */
void ui_numeric_keypad_destroy(lv_obj_t* keypad);

#ifdef __cplusplus
}
#endif

#endif // UI_NUMERIC_KEYPAD_H
