/**
 * @file i2s_tdm_demo.c
 * @brief 播放“Twinkle Twinkle Little Star”的 I2S TDM 演示，驱动 MAX98357
 */

#include <math.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2s_tdm.h"
#include "i2s_tdm_demo.h"

static const char *TAG = "I2S_TDM_DEMO";

#define DEMO_SAMPLE_RATE   48000u
#define DEMO_CHANNELS      2u
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
static uint32_t s_bpm = 120; // 速度（每分钟拍数），默认 120 BPM

// 简单淡入淡出，减少音符间的爆音（单位：样本数）
static const uint32_t FADE_SAMPLES = 2400; // 50ms @ 48kHz

static inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

static void render_note_block(float freq_hz, float amp, float phase_inc,
                              int16_t *frame_buf, uint32_t samples_per_ch,
                              float *io_phase, uint32_t note_pos)
{
    // 生成一帧（FRAME_SAMPLES/通道）
    for (uint32_t i = 0; i < samples_per_ch; ++i) {
        float env = 1.0f;
        uint32_t global_pos = note_pos + i;
        if (global_pos < FADE_SAMPLES) {
            env = (float)global_pos / (float)FADE_SAMPLES; // 淡入
        }
        // 注：淡出在调用者处按 note 剩余长度决定，这里仅在尾部帧时处理

        float sample_f = 0.0f;
        if (freq_hz > 0.0f) {
            sample_f = sinf(*io_phase) * amp * env;
            *io_phase += phase_inc;
            if (*io_phase >= 2.0f * PI_F) {
                *io_phase -= 2.0f * PI_F;
            }
        }
        int16_t s = (int16_t)(sample_f * 32767.0f);
        frame_buf[i * 2 + 0] = s; // L
        frame_buf[i * 2 + 1] = s; // R
    }
}

static void i2s_tdm_melody_task(void *pv)
{
    ESP_LOGI(TAG, "Melody task started @ %u BPM", (unsigned)s_bpm);

    const float quarter_note_sec = 60.0f / (float)s_bpm; // 四分音符时长
    int16_t frame_buf[FRAME_SAMPLES * DEMO_CHANNELS];

    while (1) {
        for (size_t n = 0; n < (sizeof(s_twinkle) / sizeof(s_twinkle[0])); ++n) {
            const float freq = s_twinkle[n].freq_hz;
            const float beats = s_twinkle[n].beats;
            const float note_sec = beats * quarter_note_sec;
            const uint32_t total_samples = (uint32_t)(note_sec * (float)DEMO_SAMPLE_RATE);
            const uint32_t total_frames = (total_samples + FRAME_SAMPLES - 1) / FRAME_SAMPLES;
            const float amp = 0.55f; // 音量
            const float phase_inc = (freq > 0.0f) ? (2.0f * PI_F * freq / (float)DEMO_SAMPLE_RATE) : 0.0f;
            float phase = 0.0f;

            uint32_t samples_written = 0;
            for (uint32_t f = 0; f < total_frames; ++f) {
                uint32_t remain = total_samples - samples_written;
                uint32_t this_frame = remain > FRAME_SAMPLES ? FRAME_SAMPLES : remain;

                // 渐出：最后一帧应用线性淡出
                uint32_t note_pos = samples_written;
                render_note_block(freq, amp, phase_inc, frame_buf, this_frame, &phase, note_pos);

                if (f == total_frames - 1 && this_frame > 0) {
                    for (uint32_t i = 0; i < this_frame; ++i) {
                        uint32_t global_pos = note_pos + i;
                        uint32_t dist_to_end = total_samples - global_pos;
                        float env = 1.0f;
                        if (dist_to_end < FADE_SAMPLES) {
                            env = (float)dist_to_end / (float)FADE_SAMPLES; // 淡出
                        }
                        int32_t l = (int32_t)((float)frame_buf[i * 2 + 0] * clamp01(env));
                        int32_t r = (int32_t)((float)frame_buf[i * 2 + 1] * clamp01(env));
                        frame_buf[i * 2 + 0] = (int16_t)l;
                        frame_buf[i * 2 + 1] = (int16_t)r;
                    }
                }

                size_t bytes_written = 0;
                if (this_frame > 0) {
                    (void)memset(&frame_buf[this_frame * 2], 0, (FRAME_SAMPLES - this_frame) * 2 * sizeof(int16_t));
                    (void)i2s_tdm_write(frame_buf, FRAME_SAMPLES * DEMO_CHANNELS * sizeof(int16_t), &bytes_written);
                }
                samples_written += this_frame;
            }

            // 音符间隙（休止符 10ms）
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // 句子间停顿
        vTaskDelay(pdMS_TO_TICKS(400));
    }
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

    if (s_i2s_demo_task == NULL) {
        xTaskCreatePinnedToCore(i2s_tdm_melody_task, "i2s_melody", 4096, NULL, 5, &s_i2s_demo_task, 0);
    }

    ESP_LOGI(TAG, "I2S TDM Twinkle demo started");
    return ESP_OK;
}

esp_err_t i2s_tdm_demo_deinit(void)
{
    if (s_i2s_demo_task) {
        vTaskDelete(s_i2s_demo_task);
        s_i2s_demo_task = NULL;
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

// 重复实现已移除（正弦波示例）

