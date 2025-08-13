/**
 * @file serial_display_demo.h
 * @brief 串口显示演示程序头文件
 * @author Your Name
 * @date 2024
 */
#ifndef SERIAL_DISPLAY_DEMO_H
#define SERIAL_DISPLAY_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

/**
 * @brief 初始化串口显示演示程序
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t serial_display_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif // SERIAL_DISPLAY_DEMO_H
