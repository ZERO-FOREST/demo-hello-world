#ifndef KEY_H
#define KEY_H

#include <stdint.h>

typedef enum {
    KEY_NONE  = 0x00,
    KEY_UP    = 0x01,
    KEY_DOWN  = 0x02,
    KEY_LEFT  = 0x04,
    KEY_RIGHT = 0x08
} key_dir_t;

/**
 * @brief 初始化按键接口（初始化摇杆ADC）
 */
void key_init(void);

/**
 * @brief 扫描摇杆并返回当前按键状态，按键状态可组合多方向
 * @return key_dir_t 方向按键的按位掩码
 */
key_dir_t key_scan(void);

#endif // KEY_H