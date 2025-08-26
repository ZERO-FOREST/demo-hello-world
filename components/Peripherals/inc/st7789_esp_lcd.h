/**
 * @file st7789_esp_lcd.h
 * @brief ST7789 TFT LCD Driver for ESP32-S3 using ESP-LCD component
 * @author Your Name
 * @date 2024
 */

#ifndef ST7789_ESP_LCD_H
#define ST7789_ESP_LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

// ========================================
// 硬件连接配置 (可根据实际连接修改)
// ========================================
#define ST7789_SPI_HOST         SPI2_HOST
#define ST7789_PIN_MOSI         11          // SDA/MOSI
#define ST7789_PIN_CLK          12          // SCL/CLK  
#define ST7789_PIN_CS           10          // CS 片选
#define ST7789_PIN_DC           13          // DC 数据/命令选择
#define ST7789_PIN_RST          14          // RST 复位
#define ST7789_PIN_BLK          9           // 背光控制 (可选)
#define ST7789_PIN_POWER        6          // 电源控制

// ========================================
// 显示器参数配置
// ========================================
#define ST7789_WIDTH            240         // 屏幕物理宽度
#define ST7789_HEIGHT           320         // 屏幕物理高度
#define ST7789_ROTATION         2           // 默认旋转方向
#define ST7789_RGB_ORDER        0           // RGB顺序 0=RGB, 1=BGR
#define ST7789_COLOR_SWAP       1           // 颜色字节交换 0=关闭, 1=开启

// ========================================
// SPI传输配置
// ========================================
#define ST7789_SPI_CLOCK_HZ     80000000    // 80MHz SPI时钟 (最大速度)

// ========================================
// ST7789 命令定义
// ========================================
#define ST7789_CMD_NOP          0x00
#define ST7789_CMD_SWRESET      0x01
#define ST7789_CMD_RDDID        0x04
#define ST7789_CMD_RDDST        0x09

#define ST7789_CMD_SLPIN        0x10
#define ST7789_CMD_SLPOUT       0x11
#define ST7789_CMD_PTLON        0x12
#define ST7789_CMD_NORON        0x13

#define ST7789_CMD_INVOFF       0x20
#define ST7789_CMD_INVON        0x21
#define ST7789_CMD_DISPOFF      0x28
#define ST7789_CMD_DISPON       0x29
#define ST7789_CMD_CASET        0x2A
#define ST7789_CMD_RASET        0x2B
#define ST7789_CMD_RAMWR        0x2C
#define ST7789_CMD_RAMRD        0x2E

#define ST7789_CMD_PTLAR        0x30
#define ST7789_CMD_COLMOD       0x3A
#define ST7789_CMD_MADCTL       0x36

#define ST7789_MADCTL_MY        0x80  // Row Address Order
#define ST7789_MADCTL_MX        0x40  // Column Address Order
#define ST7789_MADCTL_MV        0x20  // Row/Column Exchange
#define ST7789_MADCTL_ML        0x10  // Line Address Order
#define ST7789_MADCTL_RGB       0x00  // RGB Order (0=RGB, 0x08=BGR)

// Commands from working STM32 driver, required for full init
#define ST7789_CMD_PORCTRL      0xB2  // Porch Control
#define ST7789_CMD_GCTRL        0xB7  // Gate Control
#define ST7789_CMD_VCOMS        0xBB  // VCOM Setting
#define ST7789_CMD_LCMCTRL      0xC2  // LCM Control
#define ST7789_CMD_VDVVRHEN     0xC3  // VDV and VRH Command Enable
#define ST7789_CMD_VRHSET       0xC4  // VRH Set
#define ST7789_CMD_VDVSET       0xC6  // VDV Set
#define ST7789_CMD_PWCTRL1      0xD0  // Power Control 1
#define ST7789_CMD_GMCTRP1      0xE0  // Positive Gamma Correction
#define ST7789_CMD_GMCTRN1      0xE1  // Negative Gamma Correction

// ========================================
// 颜色定义
// ========================================
#define ST7789_BLACK            0x0000
#define ST7789_WHITE            0xFFFF
#define ST7789_RED              0xF800
#define ST7789_GREEN            0x07E0
#define ST7789_BLUE             0x001F
#define ST7789_YELLOW           0xFFE0
#define ST7789_CYAN             0x07FF
#define ST7789_MAGENTA          0xF81F

// ========================================
// 函数声明
// ========================================

/**
 * @brief 初始化ST7789显示屏 (使用ESP-LCD组件)
 * @return ESP_OK成功，其他值失败
 */
esp_err_t st7789_esp_lcd_init(void);

/**
 * @brief 反初始化ST7789显示屏
 * @return ESP_OK成功，其他值失败
 */
esp_err_t st7789_esp_lcd_deinit(void);

/**
 * @brief 清屏
 * @param color 清屏颜色
 * @return ESP_OK成功，其他值失败
 */
esp_err_t st7789_esp_lcd_clear_screen(uint16_t color);

/**
 * @brief 设置显示窗口
 * @param x0 起始X坐标
 * @param y0 起始Y坐标
 * @param x1 结束X坐标
 * @param y1 结束Y坐标
 * @return ESP_OK成功，其他值失败
 */
esp_err_t st7789_esp_lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief 绘制像素
 * @param x X坐标
 * @param y Y坐标
 * @param color 像素颜色
 * @return ESP_OK成功，其他值失败
 */
esp_err_t st7789_esp_lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief 绘制矩形
 * @param x 起始X坐标
 * @param y 起始Y坐标
 * @param width 宽度
 * @param height 高度
 * @param color 颜色
 * @return ESP_OK成功，其他值失败
 */
esp_err_t st7789_esp_lcd_draw_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

/**
 * @brief 填充矩形
 * @param x 起始X坐标
 * @param y 起始Y坐标
 * @param width 宽度
 * @param height 高度
 * @param color 颜色
 * @return ESP_OK成功，其他值失败
 */
esp_err_t st7789_esp_lcd_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

/**
 * @brief 绘制图像数据
 * @param x 起始X坐标
 * @param y 起始Y坐标
 * @param width 图像宽度
 * @param height 图像高度
 * @param data 图像数据指针
 * @return ESP_OK成功，其他值失败
 */
esp_err_t st7789_esp_lcd_draw_image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *data);

/**
 * @brief 开启/关闭显示
 * @param enable true开启，false关闭
 * @return ESP_OK成功，其他值失败
 */
esp_err_t st7789_esp_lcd_display_enable(bool enable);

/**
 * @brief 开启/关闭背光
 * @param enable true开启，false关闭
 * @return ESP_OK成功，其他值失败
 */
esp_err_t st7789_esp_lcd_backlight_enable(bool enable);

/**
 * @brief 开启/关闭电源
 * @param enable true开启，false关闭
 * @return ESP_OK成功，其他值失败
 */
esp_err_t st7789_esp_lcd_power_enable(bool enable);

/**
 * @brief 设置显示方向
 * @param rotation 旋转角度 (0, 90, 180, 270)
 * @return ESP_OK成功，其他值失败
 */
esp_err_t st7789_esp_lcd_set_rotation(int rotation);

/**
 * @brief 获取LCD面板句柄 (供LVGL使用)
 * @return LCD面板句柄
 */
esp_lcd_panel_handle_t st7789_esp_lcd_get_panel_handle(void);

/**
 * @brief 获取LCD面板IO句柄 (供LVGL使用)
 * @return LCD面板IO句柄
 */
esp_lcd_panel_io_handle_t st7789_esp_lcd_get_panel_io_handle(void);

#ifdef __cplusplus
}
#endif

#endif /* ST7789_ESP_LCD_H */
