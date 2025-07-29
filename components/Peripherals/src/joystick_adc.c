/**
 * @file joystick_adc.c
 * @brief 使用ADC采集摇杆数据的驱动实现
 * @author Your Name
 * @date 2024
 */

#include "joystick_adc.h"
#include "esp_log.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "JOYSTICK_ADC";

// ADC句柄
static adc_oneshot_unit_handle_t s_adc1_handle;
static adc_cali_handle_t s_cali_handle = NULL;

// ADC校准函数
static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void adc_calibration_deinit(adc_cali_handle_t handle);

/**
 * @brief 初始化ADC
 */
esp_err_t joystick_adc_init(void) {
    // ADC1 初始化配置
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config1, &s_adc1_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // ADC 通道配置
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = JOYSTICK_ADC_ATTEN,
    };

    // 配置摇杆1的X, Y通道
    ret = adc_oneshot_config_channel(s_adc1_handle, JOYSTICK1_ADC_X_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config ADC channel[%d]", JOYSTICK1_ADC_X_CHANNEL);
        return ret;
    }
    ret = adc_oneshot_config_channel(s_adc1_handle, JOYSTICK1_ADC_Y_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config ADC channel[%d]", JOYSTICK1_ADC_Y_CHANNEL);
        return ret;
    }

    // 配置摇杆2的X, Y通道
    ret = adc_oneshot_config_channel(s_adc1_handle, JOYSTICK2_ADC_X_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config ADC channel[%d]", JOYSTICK2_ADC_X_CHANNEL);
        return ret;
    }
    ret = adc_oneshot_config_channel(s_adc1_handle, JOYSTICK2_ADC_Y_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config ADC channel[%d]", JOYSTICK2_ADC_Y_CHANNEL);
        return ret;
    }

    // 初始化ADC校准
    if (!adc_calibration_init(ADC_UNIT_1, JOYSTICK_ADC_ATTEN, &s_cali_handle)) {
        ESP_LOGW(TAG, "ADC calibration failed, voltage reading may be inaccurate");
    }

    ESP_LOGI(TAG, "Joystick ADC initialized successfully.");
    return ESP_OK;
}

/**
 * @brief 反初始化ADC
 */
esp_err_t joystick_adc_deinit(void) {
    if (s_adc1_handle) {
        ESP_ERROR_CHECK(adc_oneshot_del_unit(s_adc1_handle));
        s_adc1_handle = NULL;
    }
    adc_calibration_deinit(s_cali_handle);
    ESP_LOGI(TAG, "Joystick ADC de-initialized.");
    return ESP_OK;
}

/**
 * @brief 读取摇杆数据
 */
esp_err_t joystick_adc_read(joystick_data_t *data) {
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_ERROR_CHECK(adc_oneshot_read(s_adc1_handle, JOYSTICK1_ADC_X_CHANNEL, &data->joy1_x));
    ESP_ERROR_CHECK(adc_oneshot_read(s_adc1_handle, JOYSTICK1_ADC_Y_CHANNEL, &data->joy1_y));
    ESP_ERROR_CHECK(adc_oneshot_read(s_adc1_handle, JOYSTICK2_ADC_X_CHANNEL, &data->joy2_x));
    ESP_ERROR_CHECK(adc_oneshot_read(s_adc1_handle, JOYSTICK2_ADC_Y_CHANNEL, &data->joy2_y));

    if (s_cali_handle) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_cali_handle, data->joy1_x, &data->joy1_x_mv));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_cali_handle, data->joy1_y, &data->joy1_y_mv));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_cali_handle, data->joy2_x, &data->joy2_x_mv));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_cali_handle, data->joy2_y, &data->joy2_y_mv));
    } else {
        // 如果没有校准，则不计算电压值
        data->joy1_x_mv = 0;
        data->joy1_y_mv = 0;
        data->joy2_x_mv = 0;
        data->joy2_y_mv = 0;
    }
    
    return ESP_OK;
}

/**
 * @brief 初始化ADC校准
 */
static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
    if (ret == ESP_OK) {
        calibrated = true;
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip ADC calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory for ADC calibration");
    }

    return calibrated;
}

/**
 * @brief 反初始化ADC校准
 */
static void adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (handle) {
        ESP_LOGI(TAG, "deregister ADC calibration scheme");
        ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
    }
#endif
} 