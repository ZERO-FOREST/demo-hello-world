#ifndef LV_CONF_H
#define LV_CONF_H

// ====================
//   ESP32-S3 N16R8 优化配置
// ====================

// 颜色设置
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

// 内存配置 - 针对8MB PSRAM优化
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (256U * 1024U)  // 256K内存池，充分利用PSRAM

// 刷新设置
#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_INDEV_DEF_READ_PERIOD 30

// 定时器配置
#define LV_TICK_CUSTOM 1

// DPI设置
#define LV_DPI_DEF 130

// 启用日志
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_INFO
#define LV_LOG_PRINTF 1

// 断言设置
#define LV_USE_ASSERT_STYLE 1

// 字体设置
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

// 主题
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 0

// 动画
#define LV_USE_ANIM 1

// 组件启用
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BTN 1
#define LV_USE_BTNMATRIX 1
#define LV_USE_CANVAS 1
#define LV_USE_CHECKBOX 1
#define LV_USE_DROPDOWN 1
#define LV_USE_IMG 1
#define LV_USE_LABEL 1
#define LV_USE_LINE 1
#define LV_USE_ROLLER 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_TEXTAREA 1
#define LV_USE_TABLE 1

// GPU加速（ESP32-S3支持）
#define LV_USE_GPU_ESP32_S3 1

// 🚀 ESP32-S3图形加速优化配置
// 启用硬件加速的具体功能
#define LV_GPU_ESP32_S3_USE_DRAW_BLEND 1      // Alpha混合加速
#define LV_GPU_ESP32_S3_USE_DRAW_BG 1         // 背景绘制加速  
#define LV_GPU_ESP32_S3_USE_DRAW_RECT 1       // 矩形绘制加速
#define LV_GPU_ESP32_S3_USE_DRAW_ARC 1        // 圆弧绘制加速

// 性能监控（开发阶段启用）
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

// 📐 渲染优化
#define LV_DISP_ROT_MAX_BUF (32*1024)        // 旋转缓冲区（32KB）
#define LV_IMG_CACHE_DEF_SIZE 1              // 图像缓存

// 🎯 针对你的320x240屏幕优化
#define LV_HOR_RES_MAX 320
#define LV_VER_RES_MAX 240

#endif /*LV_CONF_H*/