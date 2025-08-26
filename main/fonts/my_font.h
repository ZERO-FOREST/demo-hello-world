#ifndef MY_FONT_H
#define MY_FONT_H

#include "lvgl.h"
#include <stdbool.h>

extern const lv_font_t Mysymbol;
extern lv_font_t* font_cn;

#define MYSYMBOL_SERIAL_DISPLAY "\uE60A"
#define MYSYMBOL_WIFI "\uE60E"
#define MYSYMBOL_NO_WIFI "\uE632"
#define MYSYMBOL_CALIBRATION "\uE61E"
#define MYSYMBOL_GAME "\uE8A7"
#define MYSYMBOL_IMAGE_TRAN "\uE603"

/**
 * @brief 从 'font' 分区初始化字体
 */
void font_init(void);

/**
 * @brief 检查字体是否加载完成
 */
bool is_font_loaded(void);

/**
 * @brief 获取加载的字体
 */
lv_font_t* get_loaded_font(void);

#endif // MY_FONT_H