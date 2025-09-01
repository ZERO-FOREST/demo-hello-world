/*
 * @Author: tidycraze 2595256284@qq.com
 * @Date: 2025-09-01 13:24:54
 * @LastEditors: tidycraze 2595256284@qq.com
 * @LastEditTime: 2025-09-01 14:43:28
 * @FilePath: \demo-hello-world\main\fonts\my_font.h
 * @Description: 
 * 
 */
#ifndef MY_FONT_H
#define MY_FONT_H

#include "lvgl.h"
#include <stdbool.h>

extern const lv_font_t Mysymbol;
extern lv_font_t* font_cn;

#define MYSYMBOL_BACK "\uE66B"
#define MYSYMBOL_MUSIC "\uE6AB"
#define MYSYMBOL_PLUGS "\uE6BC"
#define MYSYMBOL_PLUGS_CONNECTED "\uE6BD"
#define MYSYMBOL_SPEAKER "\uE6FD"
#define MYSYMBOL_SPEAKERX "\uE6FE"
#define MYSYMBOL_WIFI_NONE "\uE75B"
#define MYSYMBOL_WIFI_LOW "\uE759"
#define MYSYMBOL_WIFI_MEDIUM "\uE75C"
#define MYSYMBOL_WIFI_HIGH "\uE75A"
#define MYSYMBOL_NO_WIFI "\uE7A0"
#define MYSYMBOL_PHONE "\uE7C3"
#define MYSYMBOL_BROADCAST "\uE7D5"

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