/**
 * @file audio_receiver.h
 * @brief 音频接收和播放应用头文件
 * @author GitHub Copilot
 * @date 2025-09-01
 */
#ifndef AUDIO_RECEIVER_H
#define AUDIO_RECEIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief 启动音频接收服务
 * @return esp_err_t 成功返回ESP_OK
 */
esp_err_t audio_receiver_start(void);

/**
 * @brief 停止音频接收服务
 */
void audio_receiver_stop(void);

/**
 * @brief 检查是否正在接收音频
 * @return bool 如果正在接收音频返回true
 */
bool audio_receiver_is_receiving(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_RECEIVER_H
