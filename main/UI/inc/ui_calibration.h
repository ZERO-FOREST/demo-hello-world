/**
 * @file ui_calibration.h
 * @brief 校准和测试界面头文件
 * @author Your Name
 * @date 2024
 */
#ifndef UI_CALIBRATION_H
#define UI_CALIBRATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief 创建校准和测试界面
 * @param parent 父容器对象
 */
void ui_calibration_create(lv_obj_t *parent);

/**
 * @brief 销毁校准和测试界面
 */
void ui_calibration_destroy(void);

#ifdef __cplusplus
}
#endif

#endif // UI_CALIBRATION_H
