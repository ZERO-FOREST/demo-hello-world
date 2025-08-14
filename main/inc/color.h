/**
 * @file color.h
 * @brief 颜色定义
 * @author TidyCraze
 * @date 2025-08-14
 */
#pragma once
#include <stdint.h>

// 莫兰迪色系定义 - 使用常量颜色值
typedef struct {
    uint32_t color_hex;
    const char* name;
} morandi_color_t;

// 完整的莫兰迪色系 - 使用十六进制值
static const morandi_color_t morandi_colors[] = {

    {0xAB9E96, "Trending"},   // 浅棕灰
    {0xBCA79E, "Artists"},    // 暖棕灰
    {0xC8BAAF, "Albums"},     // 米棕灰
    {0xD2CBC4, "Playlists"},  // 浅灰白
    {0xF1EBE4, "Background"}, // 米白背景

    // 扩展的莫兰迪色系
    {0x9B8B7A, "Deep Brown"},  // 深棕灰
    {0xA89B8C, "Warm Gray"},   // 暖灰色
    {0xB5A89A, "Beige Gray"},  // 米灰色
    {0xC2B5A7, "Light Beige"}, // 浅米色
    {0xCFC2B4, "Pale Beige"},  // 淡米色
    {0xDCCFC1, "Cream"},       // 奶油色
    {0xE9DCCE, "Off White"},   // 米白色
    {0xF6E9DB, "Pearl"},       // 珍珠白

    // 冷色调莫兰迪
    {0x8B9BA8, "Cool Gray"},       // 冷灰色
    {0x98A8B5, "Blue Gray"},       // 蓝灰色
    {0xA5B5C2, "Light Blue Gray"}, // 浅蓝灰
    {0xB2C2CF, "Pale Blue"},       // 淡蓝色
    {0xBFCFDC, "Sky Blue"},        // 天蓝色

    // 暖色调莫兰迪
    {0xA89B8C, "Warm Taupe"}, // 暖驼色
    {0xB5A89A, "Mushroom"},   // 蘑菇色
    {0xC2B5A7, "Sand"},       // 沙色
    {0xCFC2B4, "Linen"},      // 亚麻色
    {0xDCCFC1, "Ivory"},      // 象牙色

    // 绿色系莫兰迪
    {0x8BA89B, "Sage"},       // 鼠尾草绿
    {0x98B5A8, "Mint"},       // 薄荷绿
    {0xA5C2B5, "Pale Green"}, // 淡绿色
    {0xB2CFC2, "Mint Cream"}, // 薄荷奶油

    // 粉色系莫兰迪
    {0xA89B9B, "Dusty Rose"}, // 灰玫瑰
    {0xB5A8A8, "Blush"},      // 腮红粉
    {0xC2B5B5, "Pale Pink"},  // 淡粉色
    {0xCFC2C2, "Rose Gold"},  // 玫瑰金
};

#define MORANDI_COLORS_COUNT (sizeof(morandi_colors) / sizeof(morandi_colors[0]))
