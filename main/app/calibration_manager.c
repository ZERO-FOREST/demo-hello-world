/**
 * @file calibration_manager.c
 * @brief 外设校准管理器 - 管理各种外设的校准数据和NVS存储
 * @author Your Name
 * @date 2024
 */
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "calibration_manager.h"
#include "joystick_adc.h"
#include "lsm6ds3.h"

static const char *TAG = "CALIBRATION_MANAGER";

// NVS命名空间
#define NVS_NAMESPACE "calibration"

// 校准数据结构 - 存储在PSRAM中
typedef struct {
    // 摇杆校准数据
    struct {
        int16_t center_x;
        int16_t center_y;
        int16_t min_x;
        int16_t max_x;
        int16_t min_y;
        int16_t max_y;
        float deadzone;
        bool calibrated;
    } joystick;
    
    // 陀螺仪校准数据
    struct {
        float bias_x;
        float bias_y;
        float bias_z;
        float scale_x;
        float scale_y;
        float scale_z;
        bool calibrated;
    } gyroscope;
    
    // 加速度计校准数据
    struct {
        float bias_x;
        float bias_y;
        float bias_z;
        float scale_x;
        float scale_y;
        float scale_z;
        bool calibrated;
    } accelerometer;
    
    // 电池校准数据
    struct {
        float voltage_scale;
        float voltage_offset;
        bool calibrated;
    } battery;
    
    // 触摸屏校准数据
    struct {
        float matrix[6];  // 3x2变换矩阵
        bool calibrated;
    } touchscreen;
    
} calibration_data_t;

// 全局校准数据 - 存储在PSRAM
static calibration_data_t *g_calibration_data = NULL;
static bool g_initialized = false;

// 校准状态
static calibration_status_t g_calibration_status = {
    .joystick_calibrated = false,
    .gyroscope_calibrated = false,
    .accelerometer_calibrated = false,
    .battery_calibrated = false,
    .touchscreen_calibrated = false
};

// 初始化PSRAM校准数据
static esp_err_t init_psram_calibration_data(void)
{
    if (g_calibration_data != NULL) {
        return ESP_OK;
    }
    
    // 分配PSRAM内存
    g_calibration_data = (calibration_data_t *)heap_caps_malloc(
        sizeof(calibration_data_t), MALLOC_CAP_SPIRAM);
    
    if (g_calibration_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM for calibration data");
        return ESP_ERR_NO_MEM;
    }
    
    // 初始化数据结构
    memset(g_calibration_data, 0, sizeof(calibration_data_t));
    
    ESP_LOGI(TAG, "PSRAM calibration data initialized: %d bytes", 
             sizeof(calibration_data_t));
    return ESP_OK;
}

// 清理PSRAM校准数据
static void cleanup_psram_calibration_data(void)
{
    if (g_calibration_data != NULL) {
        heap_caps_free(g_calibration_data);
        g_calibration_data = NULL;
    }
}

// 从NVS加载校准数据
static esp_err_t load_calibration_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return err;
    }
    
    size_t required_size = sizeof(calibration_data_t);
    err = nvs_get_blob(nvs_handle, "calibration_data", g_calibration_data, &required_size);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Calibration data loaded from NVS");
        
        // 更新校准状态
        g_calibration_status.joystick_calibrated = g_calibration_data->joystick.calibrated;
        g_calibration_status.gyroscope_calibrated = g_calibration_data->gyroscope.calibrated;
        g_calibration_status.accelerometer_calibrated = g_calibration_data->accelerometer.calibrated;
        g_calibration_status.battery_calibrated = g_calibration_data->battery.calibrated;
        g_calibration_status.touchscreen_calibrated = g_calibration_data->touchscreen.calibrated;
    } else {
        ESP_LOGW(TAG, "No calibration data found in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

// 保存校准数据到NVS
static esp_err_t save_calibration_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_blob(nvs_handle, "calibration_data", g_calibration_data, sizeof(calibration_data_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save calibration data: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit calibration data: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Calibration data saved to NVS");
    }
    
    nvs_close(nvs_handle);
    return err;
}

// 公共API函数

esp_err_t calibration_manager_init(void)
{
    if (g_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing calibration manager...");
    
    // 初始化PSRAM校准数据
    esp_err_t ret = init_psram_calibration_data();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 从NVS加载校准数据
    load_calibration_from_nvs();
    
    g_initialized = true;
    ESP_LOGI(TAG, "Calibration manager initialized successfully");
    return ESP_OK;
}

void calibration_manager_deinit(void)
{
    if (!g_initialized) {
        return;
    }
    
    // 保存校准数据到NVS
    save_calibration_to_nvs();
    
    // 清理PSRAM数据
    cleanup_psram_calibration_data();
    
    g_initialized = false;
    ESP_LOGI(TAG, "Calibration manager deinitialized");
}

// 摇杆校准函数
esp_err_t calibrate_joystick(void)
{
    if (!g_initialized || g_calibration_data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting joystick calibration...");
    
    // 读取当前摇杆值作为中心点
    joystick_data_t joystick_data;
    if (joystick_adc_read(&joystick_data) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read joystick data");
        return ESP_FAIL;
    }
    
    // 设置中心点
    g_calibration_data->joystick.center_x = joystick_data.x;
    g_calibration_data->joystick.center_y = joystick_data.y;
    
    // 初始化范围值
    g_calibration_data->joystick.min_x = joystick_data.x;
    g_calibration_data->joystick.max_x = joystick_data.x;
    g_calibration_data->joystick.min_y = joystick_data.y;
    g_calibration_data->joystick.max_y = joystick_data.y;
    
    // 设置死区
    g_calibration_data->joystick.deadzone = 0.1f; // 10%死区
    
    // 标记为已校准
    g_calibration_data->joystick.calibrated = true;
    g_calibration_status.joystick_calibrated = true;
    
    ESP_LOGI(TAG, "Joystick calibrated - Center: (%d, %d)", 
             g_calibration_data->joystick.center_x, g_calibration_data->joystick.center_y);
    
    return ESP_OK;
}

// 陀螺仪校准函数
esp_err_t calibrate_gyroscope(void)
{
    if (!g_initialized || g_calibration_data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting gyroscope calibration...");
    
    // 读取多次陀螺仪数据计算偏置
    const int samples = 100;
    float sum_x = 0, sum_y = 0, sum_z = 0;
    
    for (int i = 0; i < samples; i++) {
        lsm6ds3_data_t imu_data;
        if (lsm6ds3_read(&imu_data) == ESP_OK) {
            sum_x += imu_data.gyro.x;
            sum_y += imu_data.gyro.y;
            sum_z += imu_data.gyro.z;
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms延迟
    }
    
    // 计算平均偏置
    g_calibration_data->gyroscope.bias_x = sum_x / samples;
    g_calibration_data->gyroscope.bias_y = sum_y / samples;
    g_calibration_data->gyroscope.bias_z = sum_z / samples;
    
    // 设置默认比例因子
    g_calibration_data->gyroscope.scale_x = 1.0f;
    g_calibration_data->gyroscope.scale_y = 1.0f;
    g_calibration_data->gyroscope.scale_z = 1.0f;
    
    // 标记为已校准
    g_calibration_data->gyroscope.calibrated = true;
    g_calibration_status.gyroscope_calibrated = true;
    
    ESP_LOGI(TAG, "Gyroscope calibrated - Bias: (%.3f, %.3f, %.3f)", 
             g_calibration_data->gyroscope.bias_x,
             g_calibration_data->gyroscope.bias_y,
             g_calibration_data->gyroscope.bias_z);
    
    return ESP_OK;
}

// 加速度计校准函数
esp_err_t calibrate_accelerometer(void)
{
    if (!g_initialized || g_calibration_data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting accelerometer calibration...");
    
    // 读取多次加速度计数据计算偏置
    const int samples = 100;
    float sum_x = 0, sum_y = 0, sum_z = 0;
    
    for (int i = 0; i < samples; i++) {
        lsm6ds3_data_t imu_data;
        if (lsm6ds3_read(&imu_data) == ESP_OK) {
            sum_x += imu_data.accel_x;
            sum_y += imu_data.accel_y;
            sum_z += imu_data.accel_z;
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms延迟
    }
    
    // 计算平均偏置
    g_calibration_data->accelerometer.bias_x = sum_x / samples;
    g_calibration_data->accelerometer.bias_y = sum_y / samples;
    g_calibration_data->accelerometer.bias_z = sum_z / samples - 9.81f; // 减去重力
    
    // 设置默认比例因子
    g_calibration_data->accelerometer.scale_x = 1.0f;
    g_calibration_data->accelerometer.scale_y = 1.0f;
    g_calibration_data->accelerometer.scale_z = 1.0f;
    
    // 标记为已校准
    g_calibration_data->accelerometer.calibrated = true;
    g_calibration_status.accelerometer_calibrated = true;
    
    ESP_LOGI(TAG, "Accelerometer calibrated - Bias: (%.3f, %.3f, %.3f)", 
             g_calibration_data->accelerometer.bias_x,
             g_calibration_data->accelerometer.bias_y,
             g_calibration_data->accelerometer.bias_z);
    
    return ESP_OK;
}

// 获取校准状态
const calibration_status_t* get_calibration_status(void)
{
    return &g_calibration_status;
}

// 获取摇杆校准数据
const joystick_calibration_t* get_joystick_calibration(void)
{
    if (!g_initialized || g_calibration_data == NULL) {
        return NULL;
    }
    
    static joystick_calibration_t cal_data;
    cal_data.center_x = g_calibration_data->joystick.center_x;
    cal_data.center_y = g_calibration_data->joystick.center_y;
    cal_data.min_x = g_calibration_data->joystick.min_x;
    cal_data.max_x = g_calibration_data->joystick.max_x;
    cal_data.min_y = g_calibration_data->joystick.min_y;
    cal_data.max_y = g_calibration_data->joystick.max_y;
    cal_data.deadzone = g_calibration_data->joystick.deadzone;
    cal_data.calibrated = g_calibration_data->joystick.calibrated;
    
    return &cal_data;
}

// 获取陀螺仪校准数据
const gyroscope_calibration_t* get_gyroscope_calibration(void)
{
    if (!g_initialized || g_calibration_data == NULL) {
        return NULL;
    }
    
    static gyroscope_calibration_t cal_data;
    cal_data.bias_x = g_calibration_data->gyroscope.bias_x;
    cal_data.bias_y = g_calibration_data->gyroscope.bias_y;
    cal_data.bias_z = g_calibration_data->gyroscope.bias_z;
    cal_data.scale_x = g_calibration_data->gyroscope.scale_x;
    cal_data.scale_y = g_calibration_data->gyroscope.scale_y;
    cal_data.scale_z = g_calibration_data->gyroscope.scale_z;
    cal_data.calibrated = g_calibration_data->gyroscope.calibrated;
    
    return &cal_data;
}

// 获取加速度计校准数据
const accelerometer_calibration_t* get_accelerometer_calibration(void)
{
    if (!g_initialized || g_calibration_data == NULL) {
        return NULL;
    }
    
    static accelerometer_calibration_t cal_data;
    cal_data.bias_x = g_calibration_data->accelerometer.bias_x;
    cal_data.bias_y = g_calibration_data->accelerometer.bias_y;
    cal_data.bias_z = g_calibration_data->accelerometer.bias_z;
    cal_data.scale_x = g_calibration_data->accelerometer.scale_x;
    cal_data.scale_y = g_calibration_data->accelerometer.scale_y;
    cal_data.scale_z = g_calibration_data->accelerometer.scale_z;
    cal_data.calibrated = g_calibration_data->accelerometer.calibrated;
    
    return &cal_data;
}

// 应用摇杆校准
esp_err_t apply_joystick_calibration(joystick_data_t *data)
{
    if (!g_initialized || g_calibration_data == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_calibration_data->joystick.calibrated) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // 应用中心点偏移
    int16_t calibrated_x = data->x - g_calibration_data->joystick.center_x;
    int16_t calibrated_y = data->y - g_calibration_data->joystick.center_y;
    
    // 应用死区
    float deadzone = g_calibration_data->joystick.deadzone;
    int16_t max_range = 2048; // 假设12位ADC
    int16_t deadzone_threshold = (int16_t)(max_range * deadzone);
    
    if (abs(calibrated_x) < deadzone_threshold) {
        calibrated_x = 0;
    }
    if (abs(calibrated_y) < deadzone_threshold) {
        calibrated_y = 0;
    }
    
    data->x = calibrated_x;
    data->y = calibrated_y;
    
    return ESP_OK;
}

// 应用陀螺仪校准
esp_err_t apply_gyroscope_calibration(float *gyro_x, float *gyro_y, float *gyro_z)
{
    if (!g_initialized || g_calibration_data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!g_calibration_data->gyroscope.calibrated) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (gyro_x) {
        *gyro_x = (*gyro_x - g_calibration_data->gyroscope.bias_x) * g_calibration_data->gyroscope.scale_x;
    }
    if (gyro_y) {
        *gyro_y = (*gyro_y - g_calibration_data->gyroscope.bias_y) * g_calibration_data->gyroscope.scale_y;
    }
    if (gyro_z) {
        *gyro_z = (*gyro_z - g_calibration_data->gyroscope.bias_z) * g_calibration_data->gyroscope.scale_z;
    }
    
    return ESP_OK;
}

// 应用加速度计校准
esp_err_t apply_accelerometer_calibration(float *accel_x, float *accel_y, float *accel_z)
{
    if (!g_initialized || g_calibration_data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!g_calibration_data->accelerometer.calibrated) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (accel_x) {
        *accel_x = (*accel_x - g_calibration_data->accelerometer.bias_x) * g_calibration_data->accelerometer.scale_x;
    }
    if (accel_y) {
        *accel_y = (*accel_y - g_calibration_data->accelerometer.bias_y) * g_calibration_data->accelerometer.scale_y;
    }
    if (accel_z) {
        *accel_z = (*accel_z - g_calibration_data->accelerometer.bias_z) * g_calibration_data->accelerometer.scale_z;
    }
    
    return ESP_OK;
}
