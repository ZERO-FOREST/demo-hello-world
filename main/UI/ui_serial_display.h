/**
 * @file ui_serial_display.h
 * @brief 串口显示界面头文件
 * @author Your Name
 * @date 2024
 */
#ifndef UI_SERIAL_DISPLAY_H
#define UI_SERIAL_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief 创建串口显示界面
 * @param parent 父容器对象
 */
void ui_serial_display_create(lv_obj_t *parent);

/**
 * @brief 销毁串口显示界面
 */
void ui_serial_display_destroy(void);

/**
 * @brief 添加数据到串口显示
 * @param data 数据指针
 * @param len 数据长度
 */
void ui_serial_display_add_data(const char *data, size_t len);

/**
 * @brief 添加文本到串口显示
 * @param text 文本字符串
 */
void ui_serial_display_add_text(const char *text);

#ifdef __cplusplus
}
#endif

#endif // UI_SERIAL_DISPLAY_H
