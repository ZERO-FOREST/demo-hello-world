#include "key.h"
#include "joystick_adc.h"
#include "esp_log.h"
#include <esp_timer.h>

#define KEY_THRESHOLD 50  // 归一化值阈值，超过此值判定为按下
#define KEY_REPEAT_DELAY_MS 500  // 保持按下后多少毫秒后再次输出事件

static bool up_active = false;
static bool down_active = false;
static bool left_active = false;
static bool right_active = false;
static uint64_t last_up_ts = 0;
static uint64_t last_down_ts = 0;
static uint64_t last_left_ts = 0;
static uint64_t last_right_ts = 0;

void key_init(void) {
    if (joystick_adc_init() != ESP_OK) {
        ESP_LOGW("KEY", "Failed to init joystick ADC");
    }
}

key_dir_t key_scan(void) {
    joystick_data_t data;
    if (joystick_adc_read(&data) != ESP_OK) {
        // 读取失败时重置状态
        up_active = down_active = left_active = right_active = false;
        return KEY_NONE;
    }
    key_dir_t events = KEY_NONE;
    uint64_t now = esp_timer_get_time() / 1000ULL;

    // 向上
    if (data.norm_joy1_y > KEY_THRESHOLD) {
        if (!up_active) {
            events |= KEY_UP;
            up_active = true;
            last_up_ts = now;
        } else if (now - last_up_ts >= KEY_REPEAT_DELAY_MS) {
            events |= KEY_UP;
            last_up_ts = now;
        }
    } else {
        up_active = false;
    }
    // 向下
    if (data.norm_joy1_y < -KEY_THRESHOLD) {
        if (!down_active) {
            events |= KEY_DOWN;
            down_active = true;
            last_down_ts = now;
        } else if (now - last_down_ts >= KEY_REPEAT_DELAY_MS) {
            events |= KEY_DOWN;
            last_down_ts = now;
        }
    } else {
        down_active = false;
    }
    // 向右
    if (data.norm_joy1_x > KEY_THRESHOLD) {
        if (!right_active) {
            events |= KEY_RIGHT;
            right_active = true;
            last_right_ts = now;
        } else if (now - last_right_ts >= KEY_REPEAT_DELAY_MS) {
            events |= KEY_RIGHT;
            last_right_ts = now;
        }
    } else {
        right_active = false;
    }
    // 向左
    if (data.norm_joy1_x < -KEY_THRESHOLD) {
        if (!left_active) {
            events |= KEY_LEFT;
            left_active = true;
            last_left_ts = now;
        } else if (now - last_left_ts >= KEY_REPEAT_DELAY_MS) {
            events |= KEY_LEFT;
            last_left_ts = now;
        }
    } else {
        left_active = false;
    }

    return events;
}