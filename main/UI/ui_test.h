/**
 * @file ui_test.h
 * @brief 测试页面头文件
 * @author Your Name
 * @date 2024
 */

#ifndef UI_TEST_H
#define UI_TEST_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建测试界面
 * @param parent 父容器
 */
void ui_test_create(lv_obj_t* parent);

#ifdef __cplusplus
}
#endif

#endif // UI_TEST_H
