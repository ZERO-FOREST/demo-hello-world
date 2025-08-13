/**
 * @file serial_display.h
 * @brief 串口屏幕显示模块头文件 - 通过WiFi TCP接收数据并发送到串口屏幕
 * @author Your Name
 * @date 2024
 */
#ifndef SERIAL_DISPLAY_H
#define SERIAL_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

/**
 * @brief 初始化串口显示模块
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t serial_display_init(void);

/**
 * @brief 启动串口显示服务
 * @param port TCP服务器监听端口
 * @return true成功，false失败
 */
bool serial_display_start(uint16_t port);

/**
 * @brief 停止串口显示服务
 */
void serial_display_stop(void);

/**
 * @brief 发送文本到串口屏幕
 * @param text 要发送的文本字符串
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t serial_display_send_text(const char *text);

/**
 * @brief 发送二进制数据到串口屏幕
 * @param data 要发送的数据指针
 * @param len 数据长度
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t serial_display_send_data(const uint8_t *data, size_t len);

/**
 * @brief 检查串口显示服务是否正在运行
 * @return true正在运行，false未运行
 */
bool serial_display_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // SERIAL_DISPLAY_H
