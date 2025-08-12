/**
 * @file i2s_tdm_simple_test.c
 * @brief 简化的I2S TDM测试程序 - 单MAX98357
 */

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2s_tdm.h"

static const char *TAG = "I2S_TDM_TEST";

// 测试参数
#define TEST_SAMPLE_RATE   44100
#define TEST_FRAME_SIZE    512
#define TEST_FREQ_HZ       440.0f  // A4音符

// 生成简单的正弦波测试信号
static void generate_test_signal(int16_t *buffer, uint32_t samples) {
    static float phase = 0.0f;
    const float phase_increment = 2.0f * 3.14159265358979f * TEST_FREQ_HZ / (float)TEST_SAMPLE_RATE;
    const float amplitude = 0.02f; // 大幅降低音量，避免"得得得"声音
    
    for (uint32_t i = 0; i < samples; i++) {
        float sample = amplitude * sinf(phase);
        buffer[i] = (int16_t)(sample * 32767.0f);
        phase += phase_increment;
        if (phase >= 2.0f * 3.14159265358979f) {
            phase -= 2.0f * 3.14159265358979f;
        }
    }
}

// 简单的音频播放测试任务
static void audio_test_task(void *arg) {
    (void)arg;
    
    ESP_LOGI(TAG, "Starting audio test task");
    
    // 分配缓冲区
    int16_t *audio_buffer = malloc(TEST_FRAME_SIZE * sizeof(int16_t));
    
    if (!audio_buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        vTaskDelete(NULL);
        return;
    }
    
    // 生成测试信号
    generate_test_signal(audio_buffer, TEST_FRAME_SIZE);
    
    ESP_LOGI(TAG, "Playing test tone at %d Hz", (int)TEST_FREQ_HZ);
    
    // 播放测试信号
    for (int loop = 0; loop < 10; loop++) { // 播放10次
        ESP_LOGI(TAG, "Test loop %d/10", loop + 1);
        
        for (int frame = 0; frame < 50; frame++) { // 每循环50帧
            size_t bytes_written = 0;
            esp_err_t ret = i2s_tdm_write(audio_buffer, TEST_FRAME_SIZE * sizeof(int16_t), &bytes_written);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write audio data: %s", esp_err_to_name(ret));
                break;
            }
            
            // 短暂延迟
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        // 循环间暂停
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "Audio test completed");
    
    free(audio_buffer);
    vTaskDelete(NULL);
}

// 麦克风测试任务
static void mic_test_task(void *arg) {
    (void)arg;
    
    ESP_LOGI(TAG, "Starting microphone test task");
    
    int16_t *mic_buffer = malloc(TEST_FRAME_SIZE * sizeof(int16_t));
    if (!mic_buffer) {
        ESP_LOGE(TAG, "Failed to allocate microphone buffer");
        vTaskDelete(NULL);
        return;
    }
    
    // 监听麦克风5秒
    for (int second = 0; second < 5; second++) {
        ESP_LOGI(TAG, "Microphone test second %d/5", second + 1);
        
        for (int frame = 0; frame < 100; frame++) { // 每秒100帧
            size_t bytes_read = 0;
            esp_err_t ret = i2s_tdm_read(mic_buffer, TEST_FRAME_SIZE * sizeof(int16_t), &bytes_read);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to read microphone data: %s", esp_err_to_name(ret));
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            
            // 计算音频电平
            int32_t sum = 0;
            uint32_t samples = bytes_read / sizeof(int16_t);
            
            for (uint32_t i = 0; i < samples; i++) {
                sum += abs(mic_buffer[i]);
            }
            
            int16_t avg_level = (int16_t)(sum / samples);
            
            // 每10帧打印一次
            if (frame % 10 == 0) {
                ESP_LOGI(TAG, "Mic level: %d", avg_level);
            }
            
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    
    ESP_LOGI(TAG, "Microphone test completed");
    
    free(mic_buffer);
    vTaskDelete(NULL);
}

esp_err_t i2s_tdm_simple_test_init(void) {
    ESP_LOGI(TAG, "Initializing I2S TDM simple test");
    
    // 初始化I2S TDM
    esp_err_t ret = i2s_tdm_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_tdm_init failed: %d", ret);
        return ret;
    }
    
    // 启动I2S TDM
    ret = i2s_tdm_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_tdm_start failed: %d", ret);
        i2s_tdm_deinit();
        return ret;
    }
    
    // 创建测试任务
    xTaskCreatePinnedToCore(audio_test_task, "audio_test", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(mic_test_task, "mic_test", 4096, NULL, 4, NULL, 0);
    
    ESP_LOGI(TAG, "I2S TDM simple test started");
    return ESP_OK;
}

esp_err_t i2s_tdm_simple_test_deinit(void) {
    ESP_LOGI(TAG, "Stopping I2S TDM simple test");
    
    i2s_tdm_stop();
    i2s_tdm_deinit();
    
    ESP_LOGI(TAG, "I2S TDM simple test stopped");
    return ESP_OK;
}
