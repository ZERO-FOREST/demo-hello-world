/**
 * @file i2s_tdm.h
 * @brief I2S TDM配置 - 数字麦克风 + MAX9357解码器
 * @author Your Name
 * @date 2024
 */

#ifndef I2S_TDM_H
#define I2S_TDM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2s_tdm.h"
#include "driver/gpio.h"
#include "esp_err.h"

// ========================================
// 硬件连接配置
// ========================================
#define I2S_TDM_BCLK_PIN       23    // 位时钟
#define I2S_TDM_LRCK_PIN       24    // 帧时钟/WS
#define I2S_TDM_DATA_OUT_PIN   25    // 数据输出 (到MAX9357)
#define I2S_TDM_DATA_IN_PIN    26    // 数据输入 (来自数字麦克风)

// ========================================
// TDM配置参数
// ========================================
#define I2S_TDM_SAMPLE_RATE    48000     // 采样率 48kHz
#define I2S_TDM_BITS_PER_SAMPLE 16       // 16位采样
#define I2S_TDM_CHANNELS       8         // TDM通道数 (支持多通道)
#define I2S_TDM_SLOT_BIT_WIDTH 16       // 每个时隙的位数
#define I2S_TDM_SLOT_MASK      0xFF     // 时隙掩码 (8个时隙)

// TDM时隙分配
#define I2S_TDM_MIC_SLOT       0        // 麦克风使用时隙0
#define I2S_TDM_SPEAKER_SLOT   1        // 扬声器使用时隙1

// ========================================
// 数据结构
// ========================================
typedef struct {
    i2s_chan_handle_t tx_handle;    // 发送通道句柄
    i2s_chan_handle_t rx_handle;    // 接收通道句柄
    bool is_initialized;             // 初始化状态
    uint32_t sample_rate;           // 当前采样率
    uint16_t buffer_size;           // 缓冲区大小
} i2s_tdm_handle_t;

// ========================================
// 函数声明
// ========================================

/**
 * @brief 初始化I2S TDM
 * @return ESP_OK 成功, 其他值表示错误
 */
esp_err_t i2s_tdm_init(void);

/**
 * @brief 反初始化I2S TDM
 * @return ESP_OK 成功
 */
esp_err_t i2s_tdm_deinit(void);

/**
 * @brief 启动I2S TDM
 * @return ESP_OK 成功
 */
esp_err_t i2s_tdm_start(void);

/**
 * @brief 停止I2S TDM
 * @return ESP_OK 成功
 */
esp_err_t i2s_tdm_stop(void);

/**
 * @brief 写入音频数据到扬声器
 * @param data 音频数据
 * @param size 数据大小
 * @param bytes_written 实际写入的字节数
 * @return ESP_OK 成功
 */
esp_err_t i2s_tdm_write(const void *data, size_t size, size_t *bytes_written);

/**
 * @brief 读取麦克风音频数据
 * @param data 音频数据缓冲区
 * @param size 缓冲区大小
 * @param bytes_read 实际读取的字节数
 * @return ESP_OK 成功
 */
esp_err_t i2s_tdm_read(void *data, size_t size, size_t *bytes_read);

/**
 * @brief 设置采样率
 * @param sample_rate 采样率
 * @return ESP_OK 成功
 */
esp_err_t i2s_tdm_set_sample_rate(uint32_t sample_rate);

/**
 * @brief 获取当前采样率
 * @return 当前采样率
 */
uint32_t i2s_tdm_get_sample_rate(void);

/**
 * @brief 检查I2S TDM是否已初始化
 * @return true 已初始化, false 未初始化
 */
bool i2s_tdm_is_initialized(void);

/**
 * @brief 获取缓冲区大小
 * @return 缓冲区大小
 */
uint16_t i2s_tdm_get_buffer_size(void);

#ifdef __cplusplus
}
#endif

#endif // I2S_TDM_H 