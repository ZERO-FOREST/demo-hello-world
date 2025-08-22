/*
 * @Author: ESP32 Demo
 * @Date: 2025-01-12
 * @Description: SPIFFS文件系统读写测试头文件
 */

#ifndef SPIFFS_TEST_DEMO_H
#define SPIFFS_TEST_DEMO_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化SPIFFS文件系统
 * @return esp_err_t ESP_OK成功，其他错误码失败
 */
esp_err_t spiffs_init(void);

/**
 * @brief 测试读取font_noto_sans_sc_16_2bpp.bin文件
 * @return esp_err_t ESP_OK成功，其他错误码失败
 */
esp_err_t test_read_font_file(void);

/**
 * @brief 测试写入helloword.txt文件
 * @return esp_err_t ESP_OK成功，其他错误码失败
 */
esp_err_t test_write_hello_file(void);

/**
 * @brief 测试读取helloword.txt文件
 * @return esp_err_t ESP_OK成功，其他错误码失败
 */
esp_err_t test_read_hello_file(void);

/**
 * @brief 列出SPIFFS分区中的所有文件
 * @return esp_err_t ESP_OK成功，其他错误码失败
 */
esp_err_t list_spiffs_files(void);

/**
 * @brief 运行SPIFFS完整测试套件
 * @return esp_err_t ESP_OK成功，其他错误码失败
 */
esp_err_t run_spiffs_test_suite(void);

/**
 * @brief 卸载SPIFFS文件系统
 */
void spiffs_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // SPIFFS_TEST_DEMO_H
