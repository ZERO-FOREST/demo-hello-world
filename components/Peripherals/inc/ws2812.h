#ifndef WS2812_H
#define WS2812_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// WS2812配置参数
#define WS2812_GPIO_PIN         48          // GPIO48 引脚
#define WS2812_RMT_CHANNEL      0           // RMT通道0
#define WS2812_MAX_LEDS         256         // 最大LED数量
#define WS2812_RMT_TX_BUFFER    400         // RMT发送缓冲区大小

// WS2812时序参数 (基于10MHz RMT时钟, 1 tick = 100ns)
// 根据WS2812B数据手册进行调整
#define WS2812_T0H_TICKS        4           // 0码高电平时间 (0.4µs)
#define WS2812_T0L_TICKS        8           // 0码低电平时间 (0.8µs)
#define WS2812_T1H_TICKS        8           // 1码高电平时间 (0.8µs)
#define WS2812_T1L_TICKS        4           // 1码低电平时间 (0.4µs)
#define WS2812_RESET_TICKS      500         // 复位信号时间 (50µs), 确保 > 50us

// 颜色结构体
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} ws2812_color_t;

// 预定义颜色常量
#define WS2812_COLOR_BLACK      {0, 0, 0}
#define WS2812_COLOR_WHITE      {255, 255, 255}
#define WS2812_COLOR_RED        {255, 0, 0}
#define WS2812_COLOR_GREEN      {0, 255, 0}
#define WS2812_COLOR_BLUE       {0, 0, 255}
#define WS2812_COLOR_YELLOW     {255, 255, 0}
#define WS2812_COLOR_CYAN       {0, 255, 255}
#define WS2812_COLOR_MAGENTA    {255, 0, 255}
#define WS2812_COLOR_ORANGE     {255, 165, 0}
#define WS2812_COLOR_PURPLE     {128, 0, 128}
#define WS2812_COLOR_PINK       {255, 192, 203}

// 特效类型
typedef enum {
    WS2812_EFFECT_STATIC,       // 静态颜色
    WS2812_EFFECT_BREATHING,    // 呼吸灯
    WS2812_EFFECT_RAINBOW,      // 彩虹效果
    WS2812_EFFECT_CHASE,        // 追逐效果
    WS2812_EFFECT_TWINKLE,      // 闪烁效果
    WS2812_EFFECT_WAVE,         // 波浪效果
} ws2812_effect_t;

// 效果配置
typedef struct {
    ws2812_effect_t type;       // 效果类型
    uint32_t speed;             // 速度 (ms)
    uint8_t brightness;         // 亮度 (0-255)
    ws2812_color_t color1;      // 主颜色
    ws2812_color_t color2;      // 副颜色
} ws2812_effect_config_t;

// 基础函数
esp_err_t ws2812_init(uint16_t num_leds);
esp_err_t ws2812_deinit(void);
esp_err_t ws2812_set_pixel(uint16_t index, ws2812_color_t color);
esp_err_t ws2812_set_all(ws2812_color_t color);
esp_err_t ws2812_clear_all(void);
esp_err_t ws2812_refresh(void);

// 颜色工具函数
ws2812_color_t ws2812_hsv_to_rgb(uint16_t hue, uint8_t saturation, uint8_t value);
ws2812_color_t ws2812_rgb(uint8_t red, uint8_t green, uint8_t blue);
ws2812_color_t ws2812_scale_color(ws2812_color_t color, uint8_t scale);
uint32_t ws2812_color_to_u32(ws2812_color_t color);

// 特效函数
esp_err_t ws2812_effect_rainbow(uint8_t brightness, uint32_t delay_ms);
esp_err_t ws2812_effect_breathing(ws2812_color_t color, uint32_t period_ms);
esp_err_t ws2812_effect_chase(ws2812_color_t color, uint8_t chase_length, uint32_t delay_ms);
esp_err_t ws2812_effect_twinkle(ws2812_color_t color, uint8_t count, uint32_t delay_ms);
esp_err_t ws2812_effect_wave(ws2812_color_t color1, ws2812_color_t color2, uint32_t period_ms);

// 高级函数
esp_err_t ws2812_set_brightness(uint8_t brightness);
esp_err_t ws2812_fade_to_color(ws2812_color_t target_color, uint32_t duration_ms);
esp_err_t ws2812_fire_effect(uint32_t delay_ms);
esp_err_t ws2812_police_lights(uint32_t delay_ms);

// 状态查询
uint16_t ws2812_get_led_count(void);
bool ws2812_is_initialized(void);
ws2812_color_t ws2812_get_pixel(uint16_t index);

// 预设场景
esp_err_t ws2812_scene_christmas(void);
esp_err_t ws2812_scene_party(void);
esp_err_t ws2812_scene_relax(void);
esp_err_t ws2812_scene_focus(void);

#ifdef __cplusplus
}
#endif

#endif // WS2812_H 