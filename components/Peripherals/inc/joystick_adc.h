/**
 * @file joystick_adc.h
 * @brief 使用ADC采集摇杆数据的驱动
 * @author Your Name
 * @date 2024
 */

#ifndef JOYSTICK_ADC_H
#define JOYSTICK_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

// ========================================
// 摇杆硬件配置
// ========================================
// 摇杆1
#define JOYSTICK1_ADC_X_CHANNEL ADC_CHANNEL_0 // IO1
#define JOYSTICK1_ADC_Y_CHANNEL ADC_CHANNEL_1 // IO2

// 摇杆2
#define JOYSTICK2_ADC_X_CHANNEL ADC_CHANNEL_3 // IO4
#define JOYSTICK2_ADC_Y_CHANNEL ADC_CHANNEL_4 // IO5

// ADC衰减配置
#define JOYSTICK_ADC_ATTEN      ADC_ATTEN_DB_11

// ========================================
// 数据结构定义
// ========================================
typedef struct {
    int joy1_x;     // 摇杆1 X轴原始值
    int joy1_y;     // 摇杆1 Y轴原始值
    int joy2_x;     // 摇杆2 X轴原始值
    int joy2_y;     // 摇杆2 Y轴原始值
    int joy1_x_mv;  // 摇杆1 X轴电压 (mV)
    int joy1_y_mv;  // 摇杆1 Y轴电压 (mV)
    int joy2_x_mv;  // 摇杆2 X轴电压 (mV)
    int joy2_y_mv;  // 摇杆2 Y轴电压 (mV)
} joystick_data_t;

// ========================================
// 函数声明
// ========================================

/**
 * @brief 初始化ADC用于摇杆数据采集
 *
 * @return
 *     - ESP_OK: 成功
 *     - ESP_FAIL: 初始化失败
 */
esp_err_t joystick_adc_init(void);

/**
 * @brief 反初始化ADC
 *
 * @return
 *     - ESP_OK: 成功
 */
esp_err_t joystick_adc_deinit(void);

/**
 * @brief 从两个摇杆读取数据
 *
 * @param[out] data 指向joystick_data_t结构体的指针，用于存储读取的数据
 * @return
 *     - ESP_OK: 成功
 *     - ESP_ERR_INVALID_ARG: 参数错误
 */
esp_err_t joystick_adc_read(joystick_data_t *data);

#ifdef __cplusplus
}
#endif

#endif // JOYSTICK_ADC_H 