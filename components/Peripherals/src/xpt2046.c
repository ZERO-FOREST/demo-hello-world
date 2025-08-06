/**
 * @file xpt2046.c
 * @brief XPT2046 Touch Screen Controller Driver Implementation
 * @author Your Name
 * @date 2024
 */

#include "xpt2046.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

// ========================================
// 私有变量和常量
// ========================================
static const char *TAG = "XPT2046";
static xpt2046_handle_t g_xpt2046_handle = {0};

// ========================================
// 私有函数声明
// ========================================
static esp_err_t xpt2046_spi_init(void);
static esp_err_t xpt2046_gpio_init(void);
static uint16_t xpt2046_read_channel(uint8_t command);
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
    
    ESP_LOGI(TAG, "SPI device added to shared bus successfully");
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
    
    ESP_LOGI(TAG, "GPIO initialized successfully");
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
        ESP_LOGE(TAG, "SPI transmit failed");
        return 0;
    }
    
    // XPT2046返回12位数据，位于rx_data[1]的高4位和rx_data[2]的8位
    uint16_t result = ((rx_data[1] & 0x7F) << 5) | (rx_data[2] >> 3);
    return result;
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
    
    ESP_LOGI(TAG, "Initializing XPT2046 touch controller");
    
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
    ESP_LOGI(TAG, "XPT2046 initialized successfully");
    
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
    ESP_LOGI(TAG, "XPT2046 deinitialized");
    
    return ESP_OK;
}

/**
 * @brief 读取原始触摸数据
 */
esp_err_t xpt2046_read_raw(xpt2046_data_t *data)
{
    if (!g_xpt2046_handle.is_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 检查是否有触摸
    if (!xpt2046_is_touched()) {
        data->pressed = false;
        return ESP_OK;
    }
    
    uint16_t x_samples[XPT2046_SAMPLES];
    uint16_t y_samples[XPT2046_SAMPLES];
    uint16_t z1_samples[XPT2046_SAMPLES];
    uint16_t z2_samples[XPT2046_SAMPLES];
    
    // 多次采样
    for (int i = 0; i < XPT2046_SAMPLES; i++) {
        x_samples[i] = xpt2046_read_channel(XPT2046_CMD_X_POS);
        y_samples[i] = xpt2046_read_channel(XPT2046_CMD_Y_POS);
        z1_samples[i] = xpt2046_read_channel(XPT2046_CMD_Z1_POS);
        z2_samples[i] = xpt2046_read_channel(XPT2046_CMD_Z2_POS);
        vTaskDelay(1);  // 小延时
    }
    
    // 滤波处理
    data->x = xpt2046_filter_samples(x_samples, XPT2046_SAMPLES);
    data->y = xpt2046_filter_samples(y_samples, XPT2046_SAMPLES);
    
    // 计算压力值 (简化计算)
    int16_t z1 = xpt2046_filter_samples(z1_samples, XPT2046_SAMPLES);
    int16_t z2 = xpt2046_filter_samples(z2_samples, XPT2046_SAMPLES);
    
    if (z1 > 0) {
        data->z = (data->x * (z2 - z1)) / z1;
    } else {
        data->z = 0;
    }
    
    // 判断是否有有效触摸（压力阈值）
    data->pressed = (data->z > 100) && (data->z < 4000);
    
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
        // 显示原始触摸数据和校准后的数据
        ESP_LOGI(TAG, "Raw touch: x=%d, y=%d -> Calibrated: x=%d, y=%d", 
                 raw_data.x, raw_data.y, *x, *y);
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
        ESP_LOGI(TAG, "Calibration parameters updated");
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
    
    ESP_LOGI(TAG, "Two-point calibration completed");
    ESP_LOGI(TAG, "X: %d-%d, Y: %d-%d", cal->x_min, cal->x_max, cal->y_min, cal->y_max);
}

/**
 * @brief 获取驱动句柄
 */
xpt2046_handle_t* xpt2046_get_handle(void)
{
    return &g_xpt2046_handle;
} 