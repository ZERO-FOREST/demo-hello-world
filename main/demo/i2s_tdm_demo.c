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

typedef struct {
    float freq_hz;   // 频率（0 表示休止符）
    float beats;     // 拍数（以四分音符为 1 拍）
} note_t;

// C 大调简易版本（前两段 + 重复），四分音符为 1 拍，二分音符为 2 拍
static const note_t s_twinkle[] = {
    // C4 C4 G4 G4 A4 A4 G4(2)
    {261.63f, 1.0f}, {261.63f, 1.0f}, {392.00f, 1.0f}, {392.00f, 1.0f}, {440.00f, 1.0f}, {440.00f, 1.0f}, {392.00f, 2.0f},
    // F4 F4 E4 E4 D4 D4 C4(2)
    {349.23f, 1.0f}, {349.23f, 1.0f}, {329.63f, 1.0f}, {329.63f, 1.0f}, {293.66f, 1.0f}, {293.66f, 1.0f}, {261.63f, 2.0f},
    // G4 G4 F4 F4 E4 E4 D4(2)
    {392.00f, 1.0f}, {392.00f, 1.0f}, {349.23f, 1.0f}, {349.23f, 1.0f}, {329.63f, 1.0f}, {329.63f, 1.0f}, {293.66f, 2.0f},
    // G4 G4 F4 F4 E4 E4 D4(2)
    {392.00f, 1.0f}, {392.00f, 1.0f}, {349.23f, 1.0f}, {349.23f, 1.0f}, {329.63f, 1.0f}, {329.63f, 1.0f}, {293.66f, 2.0f},
    // C4 C4 G4 G4 A4 A4 G4(2)
    {261.63f, 1.0f}, {261.63f, 1.0f}, {392.00f, 1.0f}, {392.00f, 1.0f}, {440.00f, 1.0f}, {440.00f, 1.0f}, {392.00f, 2.0f},
    // F4 F4 E4 E4 D4 D4 C4(2)
    {349.23f, 1.0f}, {349.23f, 1.0f}, {329.63f, 1.0f}, {329.63f, 1.0f}, {293.66f, 1.0f}, {293.66f, 1.0f}, {261.63f, 2.0f},
};

// 演示参数
static TaskHandle_t s_i2s_demo_task = NULL;
static TaskHandle_t s_mic_demo_task = NULL;
static uint32_t s_bpm = 120; // 速度（每分钟拍数），默认 120 BPM

// 简单淡入淡出，减少音符间的爆音（单位：样本数）
static const uint32_t FADE_SAMPLES = 2400;

// 生成正弦波样本
static void generate_sine_wave(int16_t *buffer, uint32_t samples, float freq_hz, float amplitude) {
    static float phase = 0.0f;
    const float phase_increment = 2.0f * PI_F * freq_hz / (float)DEMO_SAMPLE_RATE;
    
    for (uint32_t i = 0; i < samples; i++) {
        float sample = amplitude * sinf(phase);
        buffer[i] = (int16_t)(sample * 32767.0f);
        phase += phase_increment;
        if (phase >= 2.0f * PI_F) {
            phase -= 2.0f * PI_F;
        }
    }
}

// 应用淡入淡出效果
static void apply_fade(int16_t *buffer, uint32_t samples, bool fade_in, bool fade_out) {
    if (fade_in && samples > FADE_SAMPLES) {
        for (uint32_t i = 0; i < FADE_SAMPLES; i++) {
            float fade_factor = (float)i / (float)FADE_SAMPLES;
            buffer[i] = (int16_t)((float)buffer[i] * fade_factor);
        }
    }
    
    if (fade_out && samples > FADE_SAMPLES) {
        for (uint32_t i = 0; i < FADE_SAMPLES; i++) {
            float fade_factor = (float)i / (float)FADE_SAMPLES;
            buffer[samples - 1 - i] = (int16_t)((float)buffer[samples - 1 - i] * fade_factor);
        }
    }
}

// 播放旋律任务
static void i2s_tdm_melody_task(void *arg) {
    (void)arg;
    
    const uint32_t total_notes = sizeof(s_twinkle) / sizeof(s_twinkle[0]);
    const float beat_duration_ms = 60000.0f / (float)s_bpm; // 每拍持续时间（毫秒）
    
    int16_t *audio_buffer = malloc(FRAME_SAMPLES * sizeof(int16_t));
    
    if (!audio_buffer) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Starting melody playback with %d notes, BPM: %d", total_notes, s_bpm);
    
    for (uint32_t note_idx = 0; note_idx < total_notes; note_idx++) {
        const note_t *note = &s_twinkle[note_idx];
        
        if (note->freq_hz == 0.0f) {
            // 休止符
            uint32_t rest_samples = (uint32_t)(note->beats * beat_duration_ms * DEMO_SAMPLE_RATE / 1000.0f);
            vTaskDelay(pdMS_TO_TICKS(rest_samples * 1000 / DEMO_SAMPLE_RATE));
            continue;
        }
        
        // 计算音符持续时间
        uint32_t note_samples = (uint32_t)(note->beats * beat_duration_ms * DEMO_SAMPLE_RATE / 1000.0f);
        uint32_t frames = (note_samples + FRAME_SAMPLES - 1) / FRAME_SAMPLES;
        
        ESP_LOGI(TAG, "Playing note %d: %.2f Hz for %.1f beats (%d samples)", 
                 note_idx, note->freq_hz, note->beats, note_samples);
        
        for (uint32_t frame = 0; frame < frames; frame++) {
            uint32_t samples_this_frame = (frame == frames - 1) ? 
                (note_samples % FRAME_SAMPLES ? note_samples % FRAME_SAMPLES : FRAME_SAMPLES) : 
                FRAME_SAMPLES;
            
            // 生成单声道音频 - 降低音量避免"得得得"声音
            generate_sine_wave(audio_buffer, samples_this_frame, note->freq_hz, 0.05f);
            
            // 应用淡入淡出
            bool fade_in = (frame == 0);
            bool fade_out = (frame == frames - 1);
            apply_fade(audio_buffer, samples_this_frame, fade_in, fade_out);
            
            // 写入TDM数据
            size_t bytes_written = 0;
            esp_err_t ret = i2s_tdm_write(audio_buffer, samples_this_frame * sizeof(int16_t), &bytes_written);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write audio data: %s", esp_err_to_name(ret));
                break;
            }
        }
    }
    
    ESP_LOGI(TAG, "Melody playback completed");
    
    free(audio_buffer);
    vTaskDelete(NULL);
}

// 麦克风监听任务
static void i2s_tdm_mic_task(void *arg) {
    (void)arg;
    
    int16_t *mic_buffer = malloc(FRAME_SAMPLES * sizeof(int16_t)); // 单麦克风
    if (!mic_buffer) {
        ESP_LOGE(TAG, "Failed to allocate microphone buffer");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Starting microphone monitoring");
    
    while (1) {
        size_t bytes_read = 0;
        esp_err_t ret = i2s_tdm_read(mic_buffer, FRAME_SAMPLES * sizeof(int16_t), &bytes_read);
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
        
        // 每100ms打印一次音频电平
        
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms
    }
    
    free(mic_buffer);
    vTaskDelete(NULL);
}

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

    // 创建旋律播放任务
    if (s_i2s_demo_task == NULL) {
        xTaskCreatePinnedToCore(i2s_tdm_melody_task, "i2s_melody", 4096, NULL, 5, &s_i2s_demo_task, 0);
    }
    
    // 创建麦克风监听任务
    if (s_mic_demo_task == NULL) {
        xTaskCreatePinnedToCore(i2s_tdm_mic_task, "i2s_mic", 4096, NULL, 4, &s_mic_demo_task, 0);
    }

    ESP_LOGI(TAG, "I2S TDM demo started - Single MAX98357 + Single Microphone");
    return ESP_OK;
}

esp_err_t i2s_tdm_demo_deinit(void)
{
    if (s_i2s_demo_task) {
        vTaskDelete(s_i2s_demo_task);
        s_i2s_demo_task = NULL;
    }
    
    if (s_mic_demo_task) {
        vTaskDelete(s_mic_demo_task);
        s_mic_demo_task = NULL;
    }
    
    i2s_tdm_stop();
    i2s_tdm_deinit();
    ESP_LOGI(TAG, "I2S TDM demo stopped");
    return ESP_OK;
}

esp_err_t i2s_tdm_demo_set_sample_rate(uint32_t sample_rate)
{
    return i2s_tdm_set_sample_rate(sample_rate);
}

