/**
 * @file xpt2046.h
 * @brief XPT2046 Touch Screen Controller Driver for ESP32-S3
 * @author Your Name
 * @date 2024
 */

#ifndef XPT2046_H
#define XPT2046_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

// ========================================
// 硬件连接配置 (共享ST7789的SPI接口)
// ========================================
#define XPT2046_SPI_HOST        SPI2_HOST       // 与ST7789共享SPI2
#define XPT2046_PIN_MOSI        11              // 与ST7789共享MOSI
#define XPT2046_PIN_MISO        16              // XPT2046 MISO (独立)
#define XPT2046_PIN_CLK         12              // 与ST7789共享CLK
#define XPT2046_PIN_CS          18              // XPT2046独立的CS
#define XPT2046_PIN_IRQ         19              // 触摸中断引脚

// ========================================
// SPI传输配置
// ========================================
#define XPT2046_SPI_CLOCK_HZ    1000000     // 1MHz SPI时钟
#define XPT2046_SAMPLES         3           // 采样次数（去噪）

// ========================================
// XPT2046 命令定义
// ========================================
#define XPT2046_CMD_X_POS       0x90        // 读取X坐标
#define XPT2046_CMD_Y_POS       0xD0        // 读取Y坐标
#define XPT2046_CMD_Z1_POS      0xB0        // 读取Z1压力
#define XPT2046_CMD_Z2_POS      0xC0        // 读取Z2压力

// ========================================
// 触摸校准参数
// ========================================
typedef struct {
    int16_t x_min;
    int16_t x_max;
    int16_t y_min;
    int16_t y_max;
    bool swap_xy;                           // 是否交换XY坐标
    bool invert_x;                          // 是否反转X坐标
    bool invert_y;                          // 是否反转Y坐标
} xpt2046_calibration_t;

// ========================================
// 触摸数据结构
// ========================================
typedef struct {
    int16_t x;                              // X坐标 (0-4095)
    int16_t y;                              // Y坐标 (0-4095)
    int16_t z;                              // 压力值
    bool pressed;                           // 是否有触摸
} xpt2046_data_t;

// ========================================
// 驱动句柄
// ========================================
typedef struct {
    spi_device_handle_t spi_handle;
    xpt2046_calibration_t calibration;
    bool is_initialized;
    uint16_t screen_width;
    uint16_t screen_height;
} xpt2046_handle_t;

// ========================================
// 函数声明
// ========================================

/**
 * @brief 初始化XPT2046触摸屏
 * @param screen_width 屏幕宽度
 * @param screen_height 屏幕高度
 * @return ESP_OK 成功, 其他值表示错误
 */
esp_err_t xpt2046_init(uint16_t screen_width, uint16_t screen_height);

/**
 * @brief 反初始化XPT2046
 * @return ESP_OK 成功
 */
esp_err_t xpt2046_deinit(void);

/**
 * @brief 读取原始触摸数据
 * @param data 输出的触摸数据
 * @return ESP_OK 成功读取到有效数据
 */
esp_err_t xpt2046_read_raw(xpt2046_data_t *data);

/**
 * @brief 读取校准后的触摸坐标
 * @param x 输出X坐标 (屏幕像素坐标)
 * @param y 输出Y坐标 (屏幕像素坐标)
 * @param pressed 输出是否有触摸
 * @return ESP_OK 成功
 */
esp_err_t xpt2046_read_touch(int16_t *x, int16_t *y, bool *pressed);

/**
 * @brief 设置校准参数
 * @param calibration 校准参数
 */
void xpt2046_set_calibration(const xpt2046_calibration_t *calibration);

/**
 * @brief 获取默认校准参数
 * @param calibration 输出默认校准参数
 */
void xpt2046_get_default_calibration(xpt2046_calibration_t *calibration);

/**
 * @brief 检查是否有触摸（通过IRQ引脚）
 * @return true 有触摸, false 无触摸
 */
bool xpt2046_is_touched(void);

/**
 * @brief 进行简单的两点校准
 * @param raw_x1 第一个点的原始X坐标
 * @param raw_y1 第一个点的原始Y坐标  
 * @param screen_x1 第一个点的屏幕X坐标
 * @param screen_y1 第一个点的屏幕Y坐标
 * @param raw_x2 第二个点的原始X坐标
 * @param raw_y2 第二个点的原始Y坐标
 * @param screen_x2 第二个点的屏幕X坐标
 * @param screen_y2 第二个点的屏幕Y坐标
 */
void xpt2046_calibrate_two_point(int16_t raw_x1, int16_t raw_y1, int16_t screen_x1, int16_t screen_y1,
                                  int16_t raw_x2, int16_t raw_y2, int16_t screen_x2, int16_t screen_y2);

/**
 * @brief 获取驱动句柄
 * @return 驱动句柄指针
 */
xpt2046_handle_t* xpt2046_get_handle(void);

#ifdef __cplusplus
}
#endif

#endif /* XPT2046_H */ 