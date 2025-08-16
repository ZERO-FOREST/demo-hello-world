/**
 * @file wifi_status_demo.h
 * @brief WiFi状态显示测试头文件
 * @author TidyCraze
 * @date 2025-01-27
 */

#ifndef WIFI_STATUS_DEMO_H
#define WIFI_STATUS_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

/**
 * @brief 初始化WiFi状态显示测试
 * @return esp_err_t
 */
esp_err_t wifi_status_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_STATUS_DEMO_H
