/**
 * @file xpt2046.c
 * @brief XPT2046 Touch Screen Controller Driver Implementation
 * @author Your Name
 * @date 2024
 */

#include "xpt2046.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <string.h>
#include <math.h>

// ========================================
// 私有变量和常量
// ========================================
/* disable logging to save resources */
#ifndef XPT2046_DISABLE_LOG
#define XPT2046_DISABLE_LOG 1
#endif
#if XPT2046_DISABLE_LOG
#define ESP_LOGI(tag, fmt, ...) ((void)0)
#define ESP_LOGW(tag, fmt, ...) ((void)0)
#define ESP_LOGE(tag, fmt, ...) ((void)0)
#endif

static const char *TAG = "XPT2046";
static xpt2046_handle_t g_xpt2046_handle = {0};

// 基于厂商代码的优化参数
#define READ_TIMES 3           // 读取次数（降采样以降低延迟）
#define LOST_VAL 1            // 丢弃值数量
#define ERR_RANGE 50          // 误差范围
#define PRESSURE_THRESHOLD_MIN 10    // 压力阈值最小值
#define PRESSURE_THRESHOLD_MAX 4000  // 压力阈值最大值

// ========================================
// 私有函数声明
// ========================================
static esp_err_t xpt2046_spi_init(void);
static esp_err_t xpt2046_gpio_init(void);
static uint16_t xpt2046_read_channel(uint8_t command);
static uint16_t xpt2046_read_xoy(uint8_t xy);
static bool xpt2046_read_xy2(uint16_t *x, uint16_t *y);
static int16_t xpt2046_filter_samples(uint16_t *samples, int count);
static void xpt2046_apply_calibration(int16_t raw_x, int16_t raw_y, int16_t *cal_x, int16_t *cal_y);

// ========================================
// SPI通信函数
// ========================================

/**
 * @brief 初始化SPI接口（共享模式）
 */
static esp_err_t xpt2046_spi_init(void)
{
    esp_err_t ret;
    
    // 注意：不初始化SPI总线，因为ST7789已经初始化了SPI2总线
    // 只需要添加XPT2046设备到现有总线
    
    // SPI设备配置
    spi_device_interface_config_t dev_config = {
        .clock_speed_hz = XPT2046_SPI_CLOCK_HZ,
        .mode = 0,                              // SPI模式0
        .spics_io_num = XPT2046_PIN_CS,
        .queue_size = 1,
        .pre_cb = NULL,
        .post_cb = NULL,
    };
    
    // 添加SPI设备到已有总线
    ret = spi_bus_add_device(XPT2046_SPI_HOST, &dev_config, &g_xpt2046_handle.spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    /* added to shared bus */
    return ESP_OK;
}

/**
 * @brief 初始化GPIO
 */
static esp_err_t xpt2046_gpio_init(void)
{
    // IRQ引脚配置为输入，带上拉
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << XPT2046_PIN_IRQ),
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    gpio_config(&io_conf);
    
    /* gpio ready */
    return ESP_OK;
}

/**
 * @brief 读取XPT2046某个通道的数据
 */
static uint16_t xpt2046_read_channel(uint8_t command)
{
    esp_err_t ret;
    spi_transaction_t trans = {0};
    
    uint8_t tx_data[3] = {command, 0x00, 0x00};
    uint8_t rx_data[3] = {0};
    
    trans.length = 24;                          // 24位数据
    trans.tx_buffer = tx_data;
    trans.rx_buffer = rx_data;
    
    ret = spi_device_polling_transmit(g_xpt2046_handle.spi_handle, &trans);
    if (ret != ESP_OK) {
        /* spi transmit failed */
        return 0;
    }
    
    // XPT2046返回12位数据，位于rx_data[1]的高4位和rx_data[2]的8位
    uint16_t result = ((rx_data[1] & 0x7F) << 5) | (rx_data[2] >> 3);
    return result;
}

/**
 * @brief 读取X或Y坐标（基于厂商代码的滤波算法）
 */
static uint16_t xpt2046_read_xoy(uint8_t xy)
{
    uint16_t i, j;
    uint16_t buf[READ_TIMES];
    uint16_t sum = 0;
    uint16_t temp;
    
    // 多次读取
    for (i = 0; i < READ_TIMES; i++) {
        buf[i] = xpt2046_read_channel(xy);
    }
    
    // 排序
    for (i = 0; i < READ_TIMES - 1; i++) {
        for (j = i + 1; j < READ_TIMES; j++) {
            if (buf[i] > buf[j]) {
                temp = buf[i];
                buf[i] = buf[j];
                buf[j] = temp;
            }
        }
    }
    
    // 去除最大最小值后取平均
    sum = 0;
    for (i = LOST_VAL; i < READ_TIMES - LOST_VAL; i++) {
        sum += buf[i];
    }
    temp = sum / (READ_TIMES - 2 * LOST_VAL);
    
    return temp;
}

/**
 * @brief 读取X,Y坐标（基于厂商代码的双次读取验证）
 */
static bool xpt2046_read_xy2(uint16_t *x, uint16_t *y)
{
    uint16_t x1, y1;
    uint16_t x2, y2;
    bool flag;
    
    // 第一次读取
    x1 = xpt2046_read_xoy(XPT2046_CMD_X_POS);
    y1 = xpt2046_read_xoy(XPT2046_CMD_Y_POS);
    
    // 第二次读取
    x2 = xpt2046_read_xoy(XPT2046_CMD_X_POS);
    y2 = xpt2046_read_xoy(XPT2046_CMD_Y_POS);
    
    // 验证两次读取的误差是否在允许范围内
    if (((x2 <= x1 && x1 < x2 + ERR_RANGE) || (x1 <= x2 && x2 < x1 + ERR_RANGE)) &&
        ((y2 <= y1 && y1 < y2 + ERR_RANGE) || (y1 <= y2 && y2 < y1 + ERR_RANGE))) {
        *x = (x1 + x2) / 2;
        *y = (y1 + y2) / 2;
        return true;
    } else {
        return false;
    }
}

/**
 * @brief 对采样数据进行滤波（去除最大最小值后取平均）
 */
static int16_t xpt2046_filter_samples(uint16_t *samples, int count)
{
    if (count <= 0) return 0;
    if (count == 1) return samples[0];
    
    // 简单排序
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (samples[i] > samples[j]) {
                uint16_t temp = samples[i];
                samples[i] = samples[j];
                samples[j] = temp;
            }
        }
    }
    
    // 去除最大最小值后取平均
    if (count <= 2) {
        return samples[0];
    }
    
    uint32_t sum = 0;
    for (int i = 1; i < count - 1; i++) {
        sum += samples[i];
    }
    
    return sum / (count - 2);
}

/**
 * @brief 应用校准参数
 */
static void xpt2046_apply_calibration(int16_t raw_x, int16_t raw_y, int16_t *cal_x, int16_t *cal_y)
{
    xpt2046_calibration_t *cal = &g_xpt2046_handle.calibration;
    
    int16_t x = raw_x;
    int16_t y = raw_y;
    
    // 交换XY坐标
    if (cal->swap_xy) {
        int16_t temp = x;
        x = y;
        y = temp;
    }
    
    // 反转坐标
    if (cal->invert_x) {
        x = 4095 - x;
    }
    if (cal->invert_y) {
        y = 4095 - y;
    }
    
    // 坐标映射到屏幕范围
    *cal_x = (x - cal->x_min) * g_xpt2046_handle.screen_width / (cal->x_max - cal->x_min);
    *cal_y = (y - cal->y_min) * g_xpt2046_handle.screen_height / (cal->y_max - cal->y_min);
    
    // 限制在屏幕范围内
    if (*cal_x < 0) *cal_x = 0;
    if (*cal_x >= g_xpt2046_handle.screen_width) *cal_x = g_xpt2046_handle.screen_width - 1;
    if (*cal_y < 0) *cal_y = 0;
    if (*cal_y >= g_xpt2046_handle.screen_height) *cal_y = g_xpt2046_handle.screen_height - 1;
}

// ========================================
// 公共函数实现
// ========================================

/**
 * @brief 初始化XPT2046
 */
esp_err_t xpt2046_init(uint16_t screen_width, uint16_t screen_height)
{
    esp_err_t ret;
    
    /* init touch controller */
    
    // 初始化句柄
    memset(&g_xpt2046_handle, 0, sizeof(xpt2046_handle_t));
    g_xpt2046_handle.screen_width = screen_width;
    g_xpt2046_handle.screen_height = screen_height;
    
    // 设置默认校准参数
    xpt2046_get_default_calibration(&g_xpt2046_handle.calibration);
    
    // 初始化GPIO
    ret = xpt2046_gpio_init();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 初始化SPI
    ret = xpt2046_spi_init();
    if (ret != ESP_OK) {
        return ret;
    }
    
    g_xpt2046_handle.is_initialized = true;
    /* xpt2046 ready */
    
    return ESP_OK;
}

/**
 * @brief 反初始化XPT2046
 */
esp_err_t xpt2046_deinit(void)
{
    if (!g_xpt2046_handle.is_initialized) {
        return ESP_OK;
    }
    
    // 释放SPI设备
    spi_bus_remove_device(g_xpt2046_handle.spi_handle);
    // spi_bus_free(XPT2046_SPI_HOST); // 不释放总线，因为总线可能被其他设备共享
    
    g_xpt2046_handle.is_initialized = false;
    /* xpt2046 deinitialized */
    
    return ESP_OK;
}

/**
 * @brief 读取原始触摸数据（基于厂商代码优化）
 */
esp_err_t xpt2046_read_raw(xpt2046_data_t *data)
{
    if (!g_xpt2046_handle.is_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 使用优化的双次读取验证
    uint16_t x, y;
    if (!xpt2046_read_xy2(&x, &y)) {
        // 若两次读取差异过大，仍返回最后一次读到的坐标，避免未初始化
        x = xpt2046_read_xoy(XPT2046_CMD_X_POS);
        y = xpt2046_read_xoy(XPT2046_CMD_Y_POS);
    }
    
    // 读取压力值
    uint16_t z1_samples[XPT2046_SAMPLES];
    uint16_t z2_samples[XPT2046_SAMPLES];
    
    for (int i = 0; i < XPT2046_SAMPLES; i++) {
        z1_samples[i] = xpt2046_read_channel(XPT2046_CMD_Z1_POS);
        z2_samples[i] = xpt2046_read_channel(XPT2046_CMD_Z2_POS);
        vTaskDelay(1);
    }
    
    int16_t z1 = xpt2046_filter_samples(z1_samples, XPT2046_SAMPLES);
    int16_t z2 = xpt2046_filter_samples(z2_samples, XPT2046_SAMPLES);
    
    // 计算压力值
    if (z1 > 0) {
        data->z = (x * (z2 - z1)) / z1;
    } else {
        data->z = 0;
    }
    
    // 依据压力阈值判断是否按下（无需IRQ）
    data->pressed = (data->z > PRESSURE_THRESHOLD_MIN) && (data->z < PRESSURE_THRESHOLD_MAX);
    if (data->pressed) {
        data->x = x;
        data->y = y;
    }
    
    return ESP_OK;
}

/**
 * @brief 读取校准后的触摸坐标
 */
esp_err_t xpt2046_read_touch(int16_t *x, int16_t *y, bool *pressed)
{
    if (!x || !y || !pressed) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xpt2046_data_t raw_data;
    esp_err_t ret = xpt2046_read_raw(&raw_data);
    if (ret != ESP_OK) {
        return ret;
    }
    
    *pressed = raw_data.pressed;
    
    if (raw_data.pressed) {
        xpt2046_apply_calibration(raw_data.x, raw_data.y, x, y);
    } else {
        *x = 0;
        *y = 0;
    }
    
    return ESP_OK;
}

/**
 * @brief 设置校准参数
 */
void xpt2046_set_calibration(const xpt2046_calibration_t *calibration)
{
    if (calibration) {
        memcpy(&g_xpt2046_handle.calibration, calibration, sizeof(xpt2046_calibration_t));
    }
}

/**
 * @brief 获取默认校准参数
 */
void xpt2046_get_default_calibration(xpt2046_calibration_t *calibration)
{
    if (calibration) {
        calibration->x_min = 200;
        calibration->x_max = 3900;
        calibration->y_min = 200;
        calibration->y_max = 3900;
        calibration->swap_xy = false;
        // For ST7789_ROTATION = 2 (180 degrees), we need to invert both X and Y
        calibration->invert_x = false;
        calibration->invert_y = false;
    }
}

/**
 * @brief 检查是否有触摸
 */
bool xpt2046_is_touched(void)
{
    // IRQ引脚低电平表示有触摸
    return gpio_get_level(XPT2046_PIN_IRQ) == 0;
}

/**
 * @brief 四点校准（基于厂商代码的校准算法）
 */
esp_err_t xpt2046_calibrate_four_point(int16_t raw_x1, int16_t raw_y1, int16_t screen_x1, int16_t screen_y1,
                                       int16_t raw_x2, int16_t raw_y2, int16_t screen_x2, int16_t screen_y2,
                                       int16_t raw_x3, int16_t raw_y3, int16_t screen_x3, int16_t screen_y3,
                                       int16_t raw_x4, int16_t raw_y4, int16_t screen_x4, int16_t screen_y4)
{
    xpt2046_calibration_t *cal = &g_xpt2046_handle.calibration;
    
    // 计算X轴校准参数
    int16_t x_min = raw_x1, x_max = raw_x1;
    if (raw_x2 < x_min) x_min = raw_x2;
    if (raw_x2 > x_max) x_max = raw_x2;
    if (raw_x3 < x_min) x_min = raw_x3;
    if (raw_x3 > x_max) x_max = raw_x3;
    if (raw_x4 < x_min) x_min = raw_x4;
    if (raw_x4 > x_max) x_max = raw_x4;
    
    // 计算Y轴校准参数
    int16_t y_min = raw_y1, y_max = raw_y1;
    if (raw_y2 < y_min) y_min = raw_y2;
    if (raw_y2 > y_max) y_max = raw_y2;
    if (raw_y3 < y_min) y_min = raw_y3;
    if (raw_y3 > y_max) y_max = raw_y3;
    if (raw_y4 < y_min) y_min = raw_y4;
    if (raw_y4 > y_max) y_max = raw_y4;
    
    // 设置校准参数
    cal->x_min = x_min;
    cal->x_max = x_max;
    cal->y_min = y_min;
    cal->y_max = y_max;
    
    // 根据屏幕坐标判断是否需要反转
    cal->invert_x = (screen_x1 > screen_x2);
    cal->invert_y = (screen_y1 > screen_y3);
    
    /* four-point calibration completed */
    
    return ESP_OK;
}

/**
 * @brief 两点校准
 */
void xpt2046_calibrate_two_point(int16_t raw_x1, int16_t raw_y1, int16_t screen_x1, int16_t screen_y1,
                                  int16_t raw_x2, int16_t raw_y2, int16_t screen_x2, int16_t screen_y2)
{
    xpt2046_calibration_t *cal = &g_xpt2046_handle.calibration;
    
    // 根据两点计算映射参数
    if (screen_x1 < screen_x2) {
        cal->x_min = raw_x1;
        cal->x_max = raw_x2;
    } else {
        cal->x_min = raw_x2;
        cal->x_max = raw_x1;
        cal->invert_x = true;
    }
    
    if (screen_y1 < screen_y2) {
        cal->y_min = raw_y1;
        cal->y_max = raw_y2;
    } else {
        cal->y_min = raw_y2;
        cal->y_max = raw_y1;
        cal->invert_y = true;
    }
    
    /* two-point calibration completed */
}

/**
 * @brief 获取驱动句柄
 */
xpt2046_handle_t* xpt2046_get_handle(void)
{
    return &g_xpt2046_handle;
}

/**
 * @brief 触摸校准工具 - 显示校准点（基于厂商代码）
 */
void xpt2046_draw_calibration_point(int16_t x, int16_t y, uint16_t color)
{
    (void)x; (void)y; (void)color; /* no-op to save resources */
    
    // TODO: 实现实际的LCD绘制
    // 可以调用ST7789的绘制函数来显示校准点
    // 例如：st7789_draw_circle(x, y, 6, color);
    // 或者：st7789_draw_cross(x, y, 12, color);
}

/**
 * @brief 触摸校准工具 - 读取校准点坐标
 */
bool xpt2046_read_calibration_point(uint8_t point_num, int16_t *raw_x, int16_t *raw_y, 
                                   int16_t *screen_x, int16_t *screen_y)
{
    if (!raw_x || !raw_y || !screen_x || !screen_y || point_num < 1 || point_num > 4) {
        return false;
    }
    
    // 等待触摸
    uint32_t timeout = 0;
    while (!xpt2046_is_touched() && timeout < 10000) { // 10秒超时
        vTaskDelay(10);
        timeout += 10;
    }
    
    if (timeout >= 10000) {
        (void)point_num;
        return false;
    }
    
    // 读取原始坐标
    uint16_t x, y;
    if (!xpt2046_read_xy2(&x, &y)) {
        (void)point_num;
        return false;
    }
    
    *raw_x = x;
    *raw_y = y;
    
    // 根据点编号设置屏幕坐标
    switch (point_num) {
        case 1:
            *screen_x = 20;
            *screen_y = 20;
            break;
        case 2:
            *screen_x = g_xpt2046_handle.screen_width - 20;
            *screen_y = 20;
            break;
        case 3:
            *screen_x = 20;
            *screen_y = g_xpt2046_handle.screen_height - 20;
            break;
        case 4:
            *screen_x = g_xpt2046_handle.screen_width - 20;
            *screen_y = g_xpt2046_handle.screen_height - 20;
            break;
    }
    
    (void)point_num; (void)raw_x; (void)raw_y; (void)screen_x; (void)screen_y;
    
    return true;
}

/**
 * @brief 触摸校准工具 - 执行四点校准流程（基于厂商代码）
 */
esp_err_t xpt2046_run_calibration(uint16_t screen_width, uint16_t screen_height)
{
    /* start calibration */
    
    // 更新屏幕尺寸
    g_xpt2046_handle.screen_width = screen_width;
    g_xpt2046_handle.screen_height = screen_height;
    
    int16_t raw_x[4], raw_y[4];
    int16_t screen_x[4], screen_y[4];
    
    // 四点校准坐标
    screen_x[0] = 20; screen_y[0] = 20;
    screen_x[1] = screen_width - 20; screen_y[1] = 20;
    screen_x[2] = 20; screen_y[2] = screen_height - 20;
    screen_x[3] = screen_width - 20; screen_y[3] = screen_height - 20;
    
    // 显示校准点并读取坐标
    for (int i = 0; i < 4; i++) {
        // 显示校准点
        xpt2046_draw_calibration_point(screen_x[i], screen_y[i], 0xF800); // 红色
        
        /* prompt user */
        
        // 读取校准点
        if (!xpt2046_read_calibration_point(i + 1, &raw_x[i], &raw_y[i], 
                                           &screen_x[i], &screen_y[i])) {
            /* calibration point failed */
            return ESP_FAIL;
        }
        
        // 清除校准点
        xpt2046_draw_calibration_point(screen_x[i], screen_y[i], 0xFFFF); // 白色
        vTaskDelay(500); // 等待500ms
    }
    
    // 执行四点校准
    esp_err_t ret = xpt2046_calibrate_four_point(
        raw_x[0], raw_y[0], screen_x[0], screen_y[0],
        raw_x[1], raw_y[1], screen_x[1], screen_y[1],
        raw_x[2], raw_y[2], screen_x[2], screen_y[2],
        raw_x[3], raw_y[3], screen_x[3], screen_y[3]
    );
    
    (void)ret;
    
    return ret;
} 