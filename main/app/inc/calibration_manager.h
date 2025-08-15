/**
 * @file calibration_manager.h
 * @brief 外设校准管理器头文件
 * @author Your Name
 * @date 2024
 */
#ifndef CALIBRATION_MANAGER_H
#define CALIBRATION_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "joystick_adc.h"
#include "lsm6ds3.h"

// 校准状态结构
typedef struct {
    bool joystick_calibrated;
    bool gyroscope_calibrated;
    bool accelerometer_calibrated;
    bool battery_calibrated;
    bool touchscreen_calibrated;
} calibration_status_t;

// 摇杆校准数据结构
typedef struct {
    int16_t center_x;
    int16_t center_y;
    int16_t min_x;
    int16_t max_x;
    int16_t min_y;
    int16_t max_y;
    float deadzone;
    bool calibrated;
} joystick_calibration_t;

// 陀螺仪校准数据结构
typedef struct {
    float bias_x;
    float bias_y;
    float bias_z;
    float scale_x;
    float scale_y;
    float scale_z;
    bool calibrated;
} gyroscope_calibration_t;

// 加速度计校准数据结构
typedef struct {
    float bias_x;
    float bias_y;
    float bias_z;
    float scale_x;
    float scale_y;
    float scale_z;
    bool calibrated;
} accelerometer_calibration_t;

// 电池校准数据结构
typedef struct {
    float voltage_scale;
    float voltage_offset;
    bool calibrated;
} battery_calibration_t;

// 触摸屏校准数据结构
typedef struct {
    float matrix[6];  // 3x2变换矩阵
    bool calibrated;
} touchscreen_calibration_t;

// 初始化校准管理器
esp_err_t calibration_manager_init(void);

// 反初始化校准管理器
void calibration_manager_deinit(void);

// 获取校准状态
const calibration_status_t* get_calibration_status(void);

// 摇杆校准
esp_err_t calibrate_joystick(void);

// 陀螺仪校准
esp_err_t calibrate_gyroscope(void);

// 加速度计校准
esp_err_t calibrate_accelerometer(void);

// 获取摇杆校准数据
const joystick_calibration_t* get_joystick_calibration(void);

// 获取陀螺仪校准数据
const gyroscope_calibration_t* get_gyroscope_calibration(void);

// 获取加速度计校准数据
const accelerometer_calibration_t* get_accelerometer_calibration(void);

// 应用摇杆校准
esp_err_t apply_joystick_calibration(joystick_data_t *data);

// 应用陀螺仪校准
esp_err_t apply_gyroscope_calibration(float *gyro_x, float *gyro_y, float *gyro_z);

// 应用加速度计校准
esp_err_t apply_accelerometer_calibration(float *accel_x, float *accel_y, float *accel_z);

#ifdef __cplusplus
}
#endif

#endif // CALIBRATION_MANAGER_H
