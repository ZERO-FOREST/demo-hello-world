/**
 * @file joystick_adc.c
 * @brief 使用旧版ADC API采集摇杆数据的驱动实现
 * @author Your Name
 * @date 2024
 */

#include "joystick_adc.h"
#include "esp_log.h"
#include "driver/adc.h"
#include <stdbool.h>
#include "nvs_flash.h"

static const char *TAG = "JOYSTICK_ADC";
#define NVS_NAMESPACE "joystick_cal"
#define NVS_CAL_KEY "cal_data"

// 校准数据
static joystick_cal_data_t s_cal_data;
// 滤波后的值
static float s_filtered_joy1_x = 0.0f;
static float s_filtered_joy1_y = 0.0f;

// 状态标志
static bool s_is_calibrating = false;
static bool s_is_calibrated = false;
static bool s_is_initialized = false;

// 私有函数声明
static int normalize_value(int value, joystick_axis_cal_t* cal);

/**
 * @brief 初始化ADC
 */
esp_err_t joystick_adc_init(void) {
    if (s_is_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = adc1_config_width(ADC_WIDTH_BIT_DEFAULT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config ADC width: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = adc1_config_channel_atten(JOYSTICK1_ADC_X_CHANNEL, JOYSTICK_ADC_ATTEN);
    if (ret != ESP_OK) { return ret; }
    ret = adc1_config_channel_atten(JOYSTICK1_ADC_Y_CHANNEL, JOYSTICK_ADC_ATTEN);
    if (ret != ESP_OK) { return ret; }

    // 尝试从NVS加载校准数据
    if (joystick_load_calibration_from_nvs() == ESP_OK) {
        ESP_LOGI(TAG, "Successfully loaded calibration data from NVS.");
    } else {
        ESP_LOGI(TAG, "No calibration data found in NVS, using default values.");
        // 使用默认值
        s_cal_data.joy1_x = (joystick_axis_cal_t){.min = 0, .max = 4095, .center = 2048};
        s_cal_data.joy1_y = (joystick_axis_cal_t){.min = 0, .max = 4095, .center = 2048};
        s_is_calibrated = false; // 明确未校准
    }

    // 初始化滤波器的初始值
    s_filtered_joy1_x = s_cal_data.joy1_x.center;
    s_filtered_joy1_y = s_cal_data.joy1_y.center;

    s_is_initialized = true;
    ESP_LOGI(TAG, "Joystick ADC initialized successfully.");
    return ESP_OK;
}

/**
 * @brief 反初始化ADC
 */
esp_err_t joystick_adc_deinit(void) {
    s_is_initialized = false;
    ESP_LOGI(TAG, "Joystick ADC de-initialized.");
    return ESP_OK;
}

/**
 * @brief 读取摇杆数据
 */
esp_err_t joystick_adc_read(joystick_data_t *data) {
    if (!s_is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 1. 读取原始值
    data->raw_joy1_x = adc1_get_raw(JOYSTICK1_ADC_X_CHANNEL);
    data->raw_joy1_y = adc1_get_raw(JOYSTICK1_ADC_Y_CHANNEL);

    // 2. 应用低通滤波器
    s_filtered_joy1_x = JOYSTICK_LOW_PASS_ALPHA * data->raw_joy1_x + (1.0f - JOYSTICK_LOW_PASS_ALPHA) * s_filtered_joy1_x;
    s_filtered_joy1_y = JOYSTICK_LOW_PASS_ALPHA * data->raw_joy1_y + (1.0f - JOYSTICK_LOW_PASS_ALPHA) * s_filtered_joy1_y;

    int joy1_x_int = (int)s_filtered_joy1_x;
    int joy1_y_int = (int)s_filtered_joy1_y;

    // 3. 如果在校准模式，更新最大/最小值
    if (s_is_calibrating) {
        if (joy1_x_int < s_cal_data.joy1_x.min) s_cal_data.joy1_x.min = joy1_x_int;
        if (joy1_x_int > s_cal_data.joy1_x.max) s_cal_data.joy1_x.max = joy1_x_int;
        if (joy1_y_int < s_cal_data.joy1_y.min) s_cal_data.joy1_y.min = joy1_y_int;
        if (joy1_y_int > s_cal_data.joy1_y.max) s_cal_data.joy1_y.max = joy1_y_int;
    }

    // 4. 计算电压值 (基于滤波后的值)
    data->joy1_x_mv = (joy1_x_int * 3300) / 4095;
    data->joy1_y_mv = (joy1_y_int * 3300) / 4095;
    
    // 5. 应用校准并归一化
    if (s_is_calibrated) {
        data->norm_joy1_x = normalize_value(joy1_x_int, &s_cal_data.joy1_x);
        data->norm_joy1_y = normalize_value(joy1_y_int, &s_cal_data.joy1_y);
    } else {
        // 如果未校准，提供一个基于默认中心点的粗略归一化
        data->norm_joy1_x = ((joy1_x_int - 2048) * 100) / 2048;
        data->norm_joy1_y = ((joy1_y_int - 2048) * 100) / 2048;
    }

    return ESP_OK;
}

/**
 * @brief 开始校准
 */
void joystick_start_calibration(void) {
    ESP_LOGI(TAG, "Starting joystick calibration...");
    s_is_calibrating = true;
    s_is_calibrated = false;

    // 重置校准数据
    s_cal_data.joy1_x = (joystick_axis_cal_t){.min = 4095, .max = 0, .center = 2048};
    s_cal_data.joy1_y = (joystick_axis_cal_t){.min = 4095, .max = 0, .center = 2048};
}

/**
 * @brief 结束校准
 */
void joystick_stop_calibration(void) {
    if (!s_is_calibrating) {
        return;
    }
    s_is_calibrating = false;
    
    // 简单的有效性检查
    if (s_cal_data.joy1_x.min >= s_cal_data.joy1_x.max || s_cal_data.joy1_y.min >= s_cal_data.joy1_y.max) {
        ESP_LOGE(TAG, "Calibration failed: Invalid min/max values. Please try again.");
        joystick_load_calibration_from_nvs(); // 恢复上次的校准
        return;
    }

    // 计算中心点
    s_cal_data.joy1_x.center = (s_cal_data.joy1_x.min + s_cal_data.joy1_x.max) / 2;
    s_cal_data.joy1_y.center = (s_cal_data.joy1_y.min + s_cal_data.joy1_y.max) / 2;
    
    s_is_calibrated = true;
    ESP_LOGI(TAG, "Joystick calibration finished.");
    ESP_LOGI(TAG, "J1X: min=%d max=%d center=%d", s_cal_data.joy1_x.min, s_cal_data.joy1_x.max, s_cal_data.joy1_x.center);
    ESP_LOGI(TAG, "J1Y: min=%d max=%d center=%d", s_cal_data.joy1_y.min, s_cal_data.joy1_y.max, s_cal_data.joy1_y.center);
    
    // 自动保存到NVS
    if (joystick_save_calibration_to_nvs() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save calibration data to NVS.");
    } else {
        ESP_LOGI(TAG, "Calibration data saved to NVS.");
    }
}

/**
 * @brief 检查是否已校准
 */
bool joystick_is_calibrated(void) {
    return s_is_calibrated;
}

/**
 * @brief 归一化处理函数
 */
static int normalize_value(int value, joystick_axis_cal_t* cal) {
    int dead_zone = 50; // 中心死区范围
    if (value > cal->center - dead_zone && value < cal->center + dead_zone) {
        return 0;
    }

    long result;
    if (value > cal->center) {
        result = (long)(value - cal->center) * 100 / (cal->max - cal->center);
    } else {
        result = (long)(value - cal->center) * 100 / (cal->center - cal->min);
    }
    
    if (result > 100) return 100;
    if (result < -100) return -100;
    
    return (int)result;
} 

/**
 * @brief 从NVS加载校准数据
 */
esp_err_t joystick_load_calibration_from_nvs(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            // 首次运行时命名空间不存在是正常情况
            ESP_LOGI(TAG, "Calibration namespace '%s' not found in NVS (first run).", NVS_NAMESPACE);
        } else {
            ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        }
        return err;
    }

    size_t required_size = sizeof(s_cal_data);
    err = nvs_get_blob(nvs_handle, NVS_CAL_KEY, &s_cal_data, &required_size);
    if (err == ESP_OK && required_size == sizeof(s_cal_data)) {
        s_is_calibrated = true;
    } else {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "Calibration data blob '%s' not found in namespace '%s'.", NVS_CAL_KEY, NVS_NAMESPACE);
        } else {
            ESP_LOGE(TAG, "Error (%s) reading NVS!", esp_err_to_name(err));
        }
        s_is_calibrated = false;
    }

    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief 将当前校准数据保存到NVS
 */
esp_err_t joystick_save_calibration_to_nvs(void) {
    if (!s_is_calibrated) {
        return ESP_ERR_INVALID_STATE;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_blob(nvs_handle, NVS_CAL_KEY, &s_cal_data, sizeof(s_cal_data));
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Error (%s) writing to NVS!", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
} 