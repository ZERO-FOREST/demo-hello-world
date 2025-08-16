/**
 * @file wifi_time_sync_demo.h
 * @brief WiFi时间同步测试头文件
 * @author TidyCraze
 * @date 2025-01-27
 */

#ifndef WIFI_TIME_SYNC_DEMO_H
#define WIFI_TIME_SYNC_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

/**
 * @brief 初始化WiFi时间同步测试
 * @return esp_err_t
 */
esp_err_t wifi_time_sync_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_TIME_SYNC_DEMO_H
