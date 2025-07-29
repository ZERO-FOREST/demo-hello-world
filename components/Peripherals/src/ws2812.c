#include "ws2812.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char* TAG = "WS2812";

static bool s_is_initialized = false;
static uint16_t s_led_count = 0;
static ws2812_color_t* s_led_buffer = NULL;
static uint8_t s_global_brightness = 255;
static rmt_channel_handle_t s_rmt_channel = NULL;
static rmt_encoder_handle_t s_rmt_encoder = NULL;

static const rmt_symbol_word_t s_ws2812_bit0 = {
    .level0 = 1,
    .duration0 = WS2812_T0H_TICKS,
    .level1 = 0,
    .duration1 = WS2812_T0L_TICKS,
};

static const rmt_symbol_word_t s_ws2812_bit1 = {
    .level0 = 1,
    .duration0 = WS2812_T1H_TICKS,
    .level1 = 0,
    .duration1 = WS2812_T1L_TICKS,
};

static const rmt_symbol_word_t s_ws2812_reset = {
    .level0 = 0,
    .duration0 = WS2812_RESET_TICKS,
    .level1 = 0,
    .duration1 = 0,
};

// RMT编码器实现
typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t* bytes_encoder;
    rmt_encoder_t* copy_encoder;
    int state;
} ws2812_encoder_t;

static size_t ws2812_encode(rmt_encoder_t* encoder, rmt_channel_handle_t channel, const void* primary_data,
                            size_t data_size, rmt_encode_state_t* ret_state) {
    ws2812_encoder_t* ws2812_encoder = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encode_state_t session_state = 0;
    rmt_encode_state_t state = 0;
    size_t encoded_symbols = 0;

    switch (ws2812_encoder->state) {
    case 0: // 发送RGB数据
        encoded_symbols += ws2812_encoder->bytes_encoder->encode(ws2812_encoder->bytes_encoder, channel, primary_data,
                                                                 data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            ws2812_encoder->state = 1; // 转到复位状态
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out;
        }
    case 1: // 发送复位信号
        encoded_symbols += ws2812_encoder->copy_encoder->encode(ws2812_encoder->copy_encoder, channel, &s_ws2812_reset,
                                                                sizeof(s_ws2812_reset), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            ws2812_encoder->state = 0; // 复位状态
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out;
        }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t ws2812_encoder_reset(rmt_encoder_t* encoder) {
    ws2812_encoder_t* ws2812_encoder = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encoder_reset(ws2812_encoder->bytes_encoder);
    rmt_encoder_reset(ws2812_encoder->copy_encoder);
    ws2812_encoder->state = 0;
    return ESP_OK;
}

static esp_err_t ws2812_encoder_del(rmt_encoder_t* encoder) {
    ws2812_encoder_t* ws2812_encoder = __containerof(encoder, ws2812_encoder_t, base);
    rmt_del_encoder(ws2812_encoder->bytes_encoder);
    rmt_del_encoder(ws2812_encoder->copy_encoder);
    free(ws2812_encoder);
    return ESP_OK;
}

// 创建WS2812编码器
static esp_err_t ws2812_new_encoder(rmt_encoder_handle_t* ret_encoder) {
    esp_err_t ret = ESP_OK;
    ws2812_encoder_t* ws2812_encoder = NULL;

    ws2812_encoder = calloc(1, sizeof(ws2812_encoder_t));
    if (!ws2812_encoder) {
        ESP_LOGE(TAG, "no mem for ws2812 encoder");
        return ESP_ERR_NO_MEM;
    }

    ws2812_encoder->base.encode = ws2812_encode;
    ws2812_encoder->base.del = ws2812_encoder_del;
    ws2812_encoder->base.reset = ws2812_encoder_reset;

    // 字节编码器配置
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = s_ws2812_bit0,
        .bit1 = s_ws2812_bit1,
        .flags.msb_first = 1,
    };

    ret = rmt_new_bytes_encoder(&bytes_encoder_config, &ws2812_encoder->bytes_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "create bytes encoder failed: %s", esp_err_to_name(ret));
        goto err;
    }

    // 复制编码器配置
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ret = rmt_new_copy_encoder(&copy_encoder_config, &ws2812_encoder->copy_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "create copy encoder failed: %s", esp_err_to_name(ret));
        goto err;
    }

    *ret_encoder = &ws2812_encoder->base;
    return ESP_OK;

err:
    if (ws2812_encoder) {
        if (ws2812_encoder->bytes_encoder) {
            rmt_del_encoder(ws2812_encoder->bytes_encoder);
        }
        if (ws2812_encoder->copy_encoder) {
            rmt_del_encoder(ws2812_encoder->copy_encoder);
        }
        free(ws2812_encoder);
    }
    return ret;
}

// 基础函数实现
esp_err_t ws2812_init(uint16_t num_leds) {
    if (s_is_initialized) {
        ESP_LOGW(TAG, "WS2812 already initialized");
        return ESP_OK;
    }

    if (num_leds == 0 || num_leds > WS2812_MAX_LEDS) {
        ESP_LOGE(TAG, "Invalid LED count: %d (max: %d)", num_leds, WS2812_MAX_LEDS);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "🌈 Initializing WS2812 on GPIO%d with %d LEDs", WS2812_GPIO_PIN, num_leds);

    // 分配LED缓冲区
    s_led_buffer = calloc(num_leds, sizeof(ws2812_color_t));
    if (!s_led_buffer) {
        ESP_LOGE(TAG, "Failed to allocate LED buffer");
        return ESP_ERR_NO_MEM;
    }

    // 配置RMT通道
    rmt_tx_channel_config_t channel_config = {
        .gpio_num = WS2812_GPIO_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000, // 10MHz分辨率，100ns单位
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };

    esp_err_t ret = rmt_new_tx_channel(&channel_config, &s_rmt_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "create RMT channel failed: %s", esp_err_to_name(ret));
        free(s_led_buffer);
        s_led_buffer = NULL;
        return ret;
    }

    // 创建WS2812编码器
    ret = ws2812_new_encoder(&s_rmt_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "create WS2812 encoder failed: %s", esp_err_to_name(ret));
        rmt_del_channel(s_rmt_channel);
        s_rmt_channel = NULL;
        free(s_led_buffer);
        s_led_buffer = NULL;
        return ret;
    }

    // 启用RMT通道
    ret = rmt_enable(s_rmt_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "enable RMT channel failed: %s", esp_err_to_name(ret));
        rmt_del_encoder(s_rmt_encoder);
        s_rmt_encoder = NULL;
        rmt_del_channel(s_rmt_channel);
        s_rmt_channel = NULL;
        free(s_led_buffer);
        s_led_buffer = NULL;
        return ret;
    }

    s_led_count = num_leds;
    s_is_initialized = true;

    ESP_LOGI(TAG, "✅ WS2812 initialized successfully");
    return ESP_OK;
}

esp_err_t ws2812_deinit(void) {
    if (!s_is_initialized) {
        return ESP_OK;
    }

    // 先清空所有LED
    ws2812_clear_all();
    ws2812_refresh();

    // 清理资源
    if (s_rmt_channel) {
        rmt_disable(s_rmt_channel);
        rmt_del_channel(s_rmt_channel);
        s_rmt_channel = NULL;
    }

    if (s_rmt_encoder) {
        rmt_del_encoder(s_rmt_encoder);
        s_rmt_encoder = NULL;
    }

    if (s_led_buffer) {
        free(s_led_buffer);
        s_led_buffer = NULL;
    }

    s_led_count = 0;
    s_is_initialized = false;

    ESP_LOGI(TAG, "WS2812 deinitialized");
    return ESP_OK;
}

esp_err_t ws2812_set_pixel(uint16_t index, ws2812_color_t color) {
    if (!s_is_initialized) {
        ESP_LOGE(TAG, "WS2812 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (index >= s_led_count) {
        ESP_LOGE(TAG, "LED index %d out of range (max: %d)", index, s_led_count - 1);
        return ESP_ERR_INVALID_ARG;
    }

    // 应用全局亮度
    s_led_buffer[index].red = (color.red * s_global_brightness) / 255;
    s_led_buffer[index].green = (color.green * s_global_brightness) / 255;
    s_led_buffer[index].blue = (color.blue * s_global_brightness) / 255;

    return ESP_OK;
}

esp_err_t ws2812_set_all(ws2812_color_t color) {
    if (!s_is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    for (uint16_t i = 0; i < s_led_count; i++) {
        s_led_buffer[i].red = (color.red * s_global_brightness) / 255;
        s_led_buffer[i].green = (color.green * s_global_brightness) / 255;
        s_led_buffer[i].blue = (color.blue * s_global_brightness) / 255;
    }

    return ESP_OK;
}

esp_err_t ws2812_clear_all(void) {
    ws2812_color_t black = WS2812_COLOR_BLACK;
    return ws2812_set_all(black);
}

esp_err_t ws2812_refresh(void) {
    if (!s_is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // 准备发送数据（GRB格式）
    uint8_t* grb_data = malloc(s_led_count * 3);
    if (!grb_data) {
        ESP_LOGE(TAG, "Failed to allocate GRB buffer");
        return ESP_ERR_NO_MEM;
    }
    for (uint16_t i = 0; i < s_led_count; i++) {
        grb_data[i * 3 + 0] = s_led_buffer[i].green; // G
        grb_data[i * 3 + 1] = s_led_buffer[i].red;   // R
        grb_data[i * 3 + 2] = s_led_buffer[i].blue;  // B
    }

    // 发送数据
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    esp_err_t ret = rmt_transmit(s_rmt_channel, s_rmt_encoder, grb_data, s_led_count * 3, &tx_config);

    free(grb_data);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RMT transmit failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 等待传输完成
    rmt_tx_wait_all_done(s_rmt_channel, 1000);

    return ESP_OK;
}

// 颜色工具函数
ws2812_color_t ws2812_hsv_to_rgb(uint16_t hue, uint8_t saturation, uint8_t value) {
    ws2812_color_t rgb = {0, 0, 0};

    if (saturation == 0) {
        rgb.red = rgb.green = rgb.blue = value;
        return rgb;
    }

    uint8_t region = hue / 43;
    uint8_t remainder = (hue - (region * 43)) * 6;
    uint8_t p = (value * (255 - saturation)) >> 8;
    uint8_t q = (value * (255 - ((saturation * remainder) >> 8))) >> 8;
    uint8_t t = (value * (255 - ((saturation * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
    case 0:
        rgb.red = value;
        rgb.green = t;
        rgb.blue = p;
        break;
    case 1:
        rgb.red = q;
        rgb.green = value;
        rgb.blue = p;
        break;
    case 2:
        rgb.red = p;
        rgb.green = value;
        rgb.blue = t;
        break;
    case 3:
        rgb.red = p;
        rgb.green = q;
        rgb.blue = value;
        break;
    case 4:
        rgb.red = t;
        rgb.green = p;
        rgb.blue = value;
        break;
    default:
        rgb.red = value;
        rgb.green = p;
        rgb.blue = q;
        break;
    }

    return rgb;
}

ws2812_color_t ws2812_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    ws2812_color_t color = {red, green, blue};
    return color;
}

ws2812_color_t ws2812_scale_color(ws2812_color_t color, uint8_t scale) {
    ws2812_color_t scaled = {
        .red = (color.red * scale) / 255, .green = (color.green * scale) / 255, .blue = (color.blue * scale) / 255};
    return scaled;
}

uint32_t ws2812_color_to_u32(ws2812_color_t color) { return (color.red << 16) | (color.green << 8) | color.blue; }

// 特效函数
esp_err_t ws2812_effect_rainbow(uint8_t brightness, uint32_t delay_ms) {
    if (!s_is_initialized)
        return ESP_ERR_INVALID_STATE;

    static uint16_t hue_offset = 0;

    for (uint16_t i = 0; i < s_led_count; i++) {
        uint16_t hue = (hue_offset + (i * 255 / s_led_count)) % 256;
        ws2812_color_t color = ws2812_hsv_to_rgb(hue, 255, brightness);
        ws2812_set_pixel(i, color);
    }

    ws2812_refresh();
    hue_offset += 3;

    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    return ESP_OK;
}

esp_err_t ws2812_effect_breathing(ws2812_color_t color, uint32_t period_ms) {
    if (!s_is_initialized)
        return ESP_ERR_INVALID_STATE;

    static uint32_t time_counter = 0;
    float brightness = (sinf(2.0f * M_PI * time_counter / period_ms) + 1.0f) / 2.0f;

    ws2812_color_t scaled_color = ws2812_scale_color(color, (uint8_t)(brightness * 255));
    ws2812_set_all(scaled_color);
    ws2812_refresh();

    time_counter += 50;
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

esp_err_t ws2812_effect_chase(ws2812_color_t color, uint8_t chase_length, uint32_t delay_ms) {
    if (!s_is_initialized)
        return ESP_ERR_INVALID_STATE;

    static uint16_t position = 0;

    ws2812_clear_all();

    for (uint8_t i = 0; i < chase_length; i++) {
        uint16_t led_pos = (position + i) % s_led_count;
        uint8_t brightness = 255 - (i * 255 / chase_length);
        ws2812_color_t chase_color = ws2812_scale_color(color, brightness);
        ws2812_set_pixel(led_pos, chase_color);
    }

    ws2812_refresh();
    position = (position + 1) % s_led_count;

    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    return ESP_OK;
}

esp_err_t ws2812_fire_effect(uint32_t delay_ms) {
    if (!s_is_initialized)
        return ESP_ERR_INVALID_STATE;

    for (uint16_t i = 0; i < s_led_count; i++) {
        uint8_t heat = rand() % 255;

        // 火焰颜色映射
        ws2812_color_t color;
        if (heat < 85) {
            color.red = heat * 3;
            color.green = 0;
            color.blue = 0;
        } else if (heat < 170) {
            heat -= 85;
            color.red = 255;
            color.green = heat * 3;
            color.blue = 0;
        } else {
            heat -= 170;
            color.red = 255;
            color.green = 255;
            color.blue = heat * 3;
        }

        ws2812_set_pixel(i, color);
    }

    ws2812_refresh();
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    return ESP_OK;
}

// 高级函数
esp_err_t ws2812_set_brightness(uint8_t brightness) {
    s_global_brightness = brightness;
    return ESP_OK;
}

// 状态查询
uint16_t ws2812_get_led_count(void) { return s_led_count; }

bool ws2812_is_initialized(void) { return s_is_initialized; }

ws2812_color_t ws2812_get_pixel(uint16_t index) {
    ws2812_color_t black = WS2812_COLOR_BLACK;
    if (!s_is_initialized || index >= s_led_count) {
        return black;
    }
    return s_led_buffer[index];
}

// 预设场景
/*
@brief: 圣诞效果
@param: 无
@return: 错误码
*/
esp_err_t ws2812_scene_christmas(void) {
    if (!s_is_initialized)
        return ESP_ERR_INVALID_STATE;

    ws2812_color_t red = WS2812_COLOR_RED;
    ws2812_color_t green = WS2812_COLOR_GREEN;

    for (uint16_t i = 0; i < s_led_count; i++) {
        ws2812_set_pixel(i, (i % 2) ? red : green);
    }

    return ws2812_refresh();
}
/*
@brief: 派对效果
@param: 无
@return: 错误码
*/
esp_err_t ws2812_scene_party(void) {
    if (!s_is_initialized)
        return ESP_ERR_INVALID_STATE;

    for (uint16_t i = 0; i < s_led_count; i++) {
        uint16_t hue = rand() % 256;
        ws2812_color_t color = ws2812_hsv_to_rgb(hue, 255, 255);
        ws2812_set_pixel(i, color);
    }

    return ws2812_refresh();
}

/*
@brief: 放松效果
@param: 无
@return: 错误码
*/
esp_err_t ws2812_scene_relax(void) {
    if (!s_is_initialized)
        return ESP_ERR_INVALID_STATE;

    ws2812_color_t warm_white = ws2812_rgb(255, 180, 120);
    ws2812_set_all(warm_white);

    return ws2812_refresh();
}
/*
@brief: 聚焦效果
@param: 无
@return: 错误码
*/
esp_err_t ws2812_scene_focus(void) {
    if (!s_is_initialized)
        return ESP_ERR_INVALID_STATE;

    ws2812_color_t cool_white = ws2812_rgb(200, 220, 255);
    ws2812_set_all(cool_white);

    return ws2812_refresh();
}