#ifndef MYSYBMOL_H
#define MYSYBMOL_H

#include "lvgl.h"
#include <stdbool.h>

extern const lv_font_t Mysybmol;
extern lv_font_t* font_cn;

#define MYSYBMOL_SERIAL_DISPLAY "\uE60A"
#define MYSYBMOL_NO_WIFI "\uE632"
#define MYSYBMOL_CALIBRATION "\uE61E"
#define MYSYBMOL_GAME "\uE8A7"
#define MYSYBMOL_IMAGE_TRAN "\uE603"

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

#endif // MYSYBMOL_H