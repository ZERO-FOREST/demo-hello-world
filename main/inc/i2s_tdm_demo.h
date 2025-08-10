/**
 * @file i2s_tdm_demo.h
 * @brief I2S TDM 演示接口
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t i2s_tdm_demo_init(void);
esp_err_t i2s_tdm_demo_deinit(void);
esp_err_t i2s_tdm_demo_set_sample_rate(uint32_t sample_rate);

/**
 * @file i2s_tdm_demo.h
 * @brief I2S TDM演示头文件
 * @author Your Name
 * @date 2024
 */

#ifndef I2S_TDM_DEMO_H
#define I2S_TDM_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

/**
 * @brief 初始化I2S TDM演示
 * @return ESP_OK 成功, 其他值表示错误
 */
esp_err_t i2s_tdm_demo_init(void);

/**
 * @brief 停止I2S TDM演示
 * @return ESP_OK 成功
 */
esp_err_t i2s_tdm_demo_deinit(void);

/**
 * @brief 设置采样率
 * @param sample_rate 采样率
 * @return ESP_OK 成功
 */
esp_err_t i2s_tdm_demo_set_sample_rate(uint32_t sample_rate);

#ifdef __cplusplus
}
#endif

#endif // I2S_TDM_DEMO_H 