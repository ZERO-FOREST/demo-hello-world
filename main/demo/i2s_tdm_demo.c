/**
 * @file i2s_tdm_demo.c
 * @brief I2S TDM使用示例 - 数字麦克风 + MAX9357解码器
 * @author Your Name
 * @date 2024
 */

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "i2s_tdm.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char* TAG = "I2S_TDM_DEMO";

// 音频缓冲区
#define AUDIO_BUFFER_SIZE 1024
static int16_t audio_buffer[AUDIO_BUFFER_SIZE];
static int16_t mic_buffer[AUDIO_BUFFER_SIZE];

// 任务句柄
static TaskHandle_t audio_task_handle = NULL;
static TaskHandle_t mic_task_handle = NULL;

// 音频处理任务
static void audio_output_task(void* pvParameters) {
    ESP_LOGI(TAG, "Audio output task started");

    // 生成测试音频 (1kHz正弦波)
    int16_t test_tone[AUDIO_BUFFER_SIZE];
    for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
        // 生成1kHz正弦波，采样率48kHz
        float t = (float)i / 48000.0f;
        test_tone[i] = (int16_t)(32767.0f * sin(2.0f * M_PI * 1000.0f * t));
    }

    size_t bytes_written;
    int buffer_index = 0;

    while (1) {
        // 循环播放测试音频
        esp_err_t ret = i2s_tdm_write(&test_tone[buffer_index], sizeof(int16_t), &bytes_written);

        if (ret == ESP_OK) {
            buffer_index = (buffer_index + 1) % AUDIO_BUFFER_SIZE;
        } else {
            ESP_LOGE(TAG, "Failed to write audio data");
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms延迟
    }
}

// 麦克风录音任务
static void mic_record_task(void* pvParameters) {
    ESP_LOGI(TAG, "Microphone recording task started");

    size_t bytes_read;
    int sample_count = 0;

    while (1) {
        // 读取麦克风数据
        esp_err_t ret = i2s_tdm_read(mic_buffer, sizeof(mic_buffer), &bytes_read);

        if (ret == ESP_OK && bytes_read > 0) {
            sample_count += bytes_read / sizeof(int16_t);

            // 计算音频电平
            int32_t sum = 0;
            for (int i = 0; i < bytes_read / sizeof(int16_t); i++) {
                sum += abs(mic_buffer[i]);
            }
            int avg_level = sum / (bytes_read / sizeof(int16_t));

            // 每1000个样本打印一次电平
            if (sample_count % 1000 == 0) {
                ESP_LOGI(TAG, "Microphone level: %d, samples: %d", avg_level, sample_count);
            }
        } else {
            ESP_LOGE(TAG, "Failed to read microphone data");
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms延迟
    }
}

/**
 * @brief 初始化I2S TDM演示
 */
esp_err_t i2s_tdm_demo_init(void) {
    ESP_LOGI(TAG, "Initializing I2S TDM demo...");

    // 初始化I2S TDM
    esp_err_t ret = i2s_tdm_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S TDM: %s", esp_err_to_name(ret));
        return ret;
    }

    // 启动I2S TDM
    ret = i2s_tdm_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start I2S TDM: %s", esp_err_to_name(ret));
        return ret;
    }

    // 创建音频输出任务
    BaseType_t task_ret = xTaskCreate(audio_output_task, "audio_output", 4096, NULL, 5, &audio_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio output task");
        return ESP_FAIL;
    }

    // 创建麦克风录音任务
    task_ret = xTaskCreate(mic_record_task, "mic_record", 4096, NULL, 4, &mic_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create microphone recording task");
        vTaskDelete(audio_task_handle);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "I2S TDM demo initialized successfully");
    ESP_LOGI(TAG, "Pin configuration:");
    ESP_LOGI(TAG, "  BCLK: GPIO%d", I2S_TDM_BCLK_PIN);
    ESP_LOGI(TAG, "  LRCK: GPIO%d", I2S_TDM_LRCK_PIN);
    ESP_LOGI(TAG, "  DATA_OUT: GPIO%d (to MAX9357)", I2S_TDM_DATA_OUT_PIN);
    ESP_LOGI(TAG, "  DATA_IN: GPIO%d (from digital mic)", I2S_TDM_DATA_IN_PIN);
    ESP_LOGI(TAG, "TDM configuration:");
    ESP_LOGI(TAG, "  Sample Rate: %luHz", i2s_tdm_get_sample_rate());
    ESP_LOGI(TAG, "  Bits per sample: %d", I2S_TDM_BITS_PER_SAMPLE);
    ESP_LOGI(TAG, "  Mic slot: %d", I2S_TDM_MIC_SLOT);
    ESP_LOGI(TAG, "  Speaker slot: %d", I2S_TDM_SPEAKER_SLOT);

    return ESP_OK;
}

/**
 * @brief 停止I2S TDM演示
 */
esp_err_t i2s_tdm_demo_deinit(void) {
    ESP_LOGI(TAG, "Deinitializing I2S TDM demo...");

    // 删除任务
    if (audio_task_handle) {
        vTaskDelete(audio_task_handle);
        audio_task_handle = NULL;
    }

    if (mic_task_handle) {
        vTaskDelete(mic_task_handle);
        mic_task_handle = NULL;
    }

    // 停止I2S TDM
    esp_err_t ret = i2s_tdm_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop I2S TDM: %s", esp_err_to_name(ret));
    }

    // 反初始化I2S TDM
    ret = i2s_tdm_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize I2S TDM: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "I2S TDM demo deinitialized");
    return ESP_OK;
}

/**
 * @brief 设置采样率
 */
esp_err_t i2s_tdm_demo_set_sample_rate(uint32_t sample_rate) {
    ESP_LOGI(TAG, "Setting sample rate to %lu Hz", sample_rate);

    esp_err_t ret = i2s_tdm_set_sample_rate(sample_rate);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set sample rate: %s", esp_err_to_name(ret));
    }

    return ret;
}
