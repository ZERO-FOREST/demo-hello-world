/**
 * @file i2s_tdm_demo.c
 * @brief I2S TDM演示 - 单MAX98357 + 单麦克风输入
 */

#include <math.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2s_tdm.h"
#include "i2s_tdm_demo.h"

static const char *TAG = "I2S_TDM_DEMO";

#define DEMO_SAMPLE_RATE   44100u
#define DEMO_CHANNELS      1u
#define FRAME_SAMPLES      256u   // 每通道样本数/帧
#define PI_F               3.14159265358979f

esp_err_t i2s_tdm_demo_init(void)
{
    esp_err_t ret = i2s_tdm_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_tdm_init failed: %d", ret);
        return ret;
    }

    ret = i2s_tdm_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_tdm_start failed: %d", ret);
        i2s_tdm_deinit();
        return ret;
    }

    ESP_LOGI(TAG, "I2S TDM demo started - Single MAX98357 + Single Microphone");
    return ESP_OK;
}

esp_err_t i2s_tdm_demo_deinit(void)
{    
    i2s_tdm_stop();
    i2s_tdm_deinit();
    ESP_LOGI(TAG, "I2S TDM demo stopped");
    return ESP_OK;
}

esp_err_t i2s_tdm_demo_set_sample_rate(uint32_t sample_rate)
{
    return i2s_tdm_set_sample_rate(sample_rate);
}

