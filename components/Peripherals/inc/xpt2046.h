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
#define XPT2046_SPI_CLOCK_HZ    2000000     // 2MHz SPI时钟，提高吞吐
#define XPT2046_SAMPLES         1           // Z通道采样次数（简化以降低延迟）

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
 * @brief 进行四点校准（基于厂商代码的校准算法）
 * @param raw_x1 第一个点的原始X坐标
 * @param raw_y1 第一个点的原始Y坐标  
 * @param screen_x1 第一个点的屏幕X坐标
 * @param screen_y1 第一个点的屏幕Y坐标
 * @param raw_x2 第二个点的原始X坐标
 * @param raw_y2 第二个点的原始Y坐标
 * @param screen_x2 第二个点的屏幕X坐标
 * @param screen_y2 第二个点的屏幕Y坐标
 * @param raw_x3 第三个点的原始X坐标
 * @param raw_y3 第三个点的原始Y坐标
 * @param screen_x3 第三个点的屏幕X坐标
 * @param screen_y3 第三个点的屏幕Y坐标
 * @param raw_x4 第四个点的原始X坐标
 * @param raw_y4 第四个点的原始Y坐标
 * @param screen_x4 第四个点的屏幕X坐标
 * @param screen_y4 第四个点的屏幕Y坐标
 * @return ESP_OK 成功
 */
esp_err_t xpt2046_calibrate_four_point(int16_t raw_x1, int16_t raw_y1, int16_t screen_x1, int16_t screen_y1,
                                       int16_t raw_x2, int16_t raw_y2, int16_t screen_x2, int16_t screen_y2,
                                       int16_t raw_x3, int16_t raw_y3, int16_t screen_x3, int16_t screen_y3,
                                       int16_t raw_x4, int16_t raw_y4, int16_t screen_x4, int16_t screen_y4);

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

/**
 * @brief 触摸校准工具 - 显示校准点
 * @param x 屏幕X坐标
 * @param y 屏幕Y坐标
 * @param color 显示颜色
 */
void xpt2046_draw_calibration_point(int16_t x, int16_t y, uint16_t color);

/**
 * @brief 触摸校准工具 - 读取校准点坐标
 * @param point_num 校准点编号 (1-4)
 * @param raw_x 输出原始X坐标
 * @param raw_y 输出原始Y坐标
 * @param screen_x 输出屏幕X坐标
 * @param screen_y 输出屏幕Y坐标
 * @return true 成功读取, false 失败
 */
bool xpt2046_read_calibration_point(uint8_t point_num, int16_t *raw_x, int16_t *raw_y, 
                                   int16_t *screen_x, int16_t *screen_y);

/**
 * @brief 触摸校准工具 - 执行四点校准流程
 * @param screen_width 屏幕宽度
 * @param screen_height 屏幕高度
 * @return ESP_OK 校准成功
 */
esp_err_t xpt2046_run_calibration(uint16_t screen_width, uint16_t screen_height);

#ifdef __cplusplus
}
#endif

#endif /* XPT2046_H */

/*
 * 使用示例：
 * 
 * 1. 初始化触摸屏：
 *    esp_err_t ret = xpt2046_init(240, 320);
 *    if (ret != ESP_OK) {
 *        ESP_LOGE(TAG, "XPT2046 init failed");
 *        return;
 *    }
 * 
 * 2. 读取触摸坐标：
 *    int16_t x, y;
 *    bool pressed;
 *    esp_err_t ret = xpt2046_read_touch(&x, &y, &pressed);
 *    if (ret == ESP_OK && pressed) {
 *        printf("Touch at x:%d y:%d\n", x, y);
 *    }
 * 
 * 3. 四点校准：
 *    esp_err_t ret = xpt2046_run_calibration(240, 320);
 *    if (ret == ESP_OK) {
 *        printf("Calibration completed\n");
 *    }
 * 
 * 4. 手动设置校准参数：
 *    xpt2046_calibration_t cal;
 *    cal.x_min = 200; cal.x_max = 3900;
 *    cal.y_min = 200; cal.y_max = 3900;
 *    cal.swap_xy = false;
 *    cal.invert_x = false;
 *    cal.invert_y = false;
 *    xpt2046_set_calibration(&cal);
 * 
 * 5. 在LVGL中使用：
 *    // 在lv_port_indev.c中已经集成
 *    // 只需要调用lv_port_indev_init()即可
 */ 