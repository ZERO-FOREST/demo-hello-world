/*
 * @Author: tidycraze 2595256284@qq.com
 * @Date: 2025-08-28 14:10:35
 * @LastEditors: tidycraze 2595256284@qq.com
 * @LastEditTime: 2025-08-28 14:36:21
 * @FilePath: \demo-hello-world\main\app\lsm6ds_control.c
 * @Description: 陀螺仪控制任务
 * 
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "lsm6ds_control.h"

static char* TAG = "LSM6DS3_CTRL";

#define COMPLEMENTARY_FILTER_ALPHA 0.98f
#define DT 0.1f // 任务延时为100ms

static float pitch = 0.0f;
static float roll = 0.0f;
static SemaphoreHandle_t attitude_mutex = NULL;

// 用于存储陀螺仪零点漂移的变量
static float gyro_bias_x = 0.0f;
static float gyro_bias_y = 0.0f;
static float gyro_bias_z = 0.0f;

TaskHandle_t s_lsm6ds3_control_task = NULL;

static void lsm6ds_calibrate_gyro(void)
{
    ESP_LOGI(TAG, "Calibrating gyroscope, please keep the device still...");
    const int num_samples = 200;
    float gx_sum = 0.0f, gy_sum = 0.0f, gz_sum = 0.0f;
    lsm6ds3_data_t sensor_data;

    for (int i = 0; i < num_samples; i++) {
        if (lsm6ds3_read_all(&sensor_data) == ESP_OK) {
            gx_sum += sensor_data.gyro.x;
            gy_sum += sensor_data.gyro.y;
            gz_sum += sensor_data.gyro.z;
        }
        vTaskDelay(pdMS_TO_TICKS(5)); // 短暂延时
    }

    gyro_bias_x = gx_sum / num_samples;
    gyro_bias_y = gy_sum / num_samples;
    gyro_bias_z = gz_sum / num_samples;

    ESP_LOGI(TAG, "Gyroscope calibration finished. Bias X: %.2f, Y: %.2f, Z: %.2f", gyro_bias_x, gyro_bias_y, gyro_bias_z);
}


static void lsm6ds3_control_task(void* pvParameters)
{
    esp_err_t ret;
    lsm6ds3_data_t sensor_data;

    // 配置加速度计: 104Hz, ±2g
    ret = lsm6ds3_config_accel(LSM6DS3_ODR_104_HZ, LSM6DS3_ACCEL_FS_2G);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure accelerometer");
        lsm6ds3_deinit();
        vTaskDelete(NULL);
    }
    
    // 配置陀螺仪: 104Hz, ±250dps
    ret = lsm6ds3_config_gyro(LSM6DS3_ODR_104_HZ, LSM6DS3_GYRO_FS_250DPS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure gyroscope");
        lsm6ds3_deinit();
        vTaskDelete(NULL);
    }
    
    // 启用传感器
    lsm6ds3_accel_enable(true);
    lsm6ds3_gyro_enable(true);

    // 在任务开始时执行校准
    lsm6ds_calibrate_gyro();
    
    while (1) {

        ret = lsm6ds3_read_all(&sensor_data);
        if (ret == ESP_OK) {
            // 应用零点漂移校准
            float gyro_x_corrected = sensor_data.gyro.x - gyro_bias_x;
            float gyro_y_corrected = sensor_data.gyro.y - gyro_bias_y;

            // 从加速度计计算角度
            float pitch_acc = atan2f(sensor_data.accel.y, sensor_data.accel.z) * 180 / M_PI;
            float roll_acc = atan2f(-sensor_data.accel.x, sqrt(sensor_data.accel.y * sensor_data.accel.y + sensor_data.accel.z * sensor_data.accel.z)) * 180 / M_PI;

            if (xSemaphoreTake(attitude_mutex, portMAX_DELAY) == pdTRUE) {
                // 互补滤波器
                pitch = COMPLEMENTARY_FILTER_ALPHA * (pitch + gyro_x_corrected * DT) + (1 - COMPLEMENTARY_FILTER_ALPHA) * pitch_acc;
                roll = COMPLEMENTARY_FILTER_ALPHA * (roll + gyro_y_corrected * DT) + (1 - COMPLEMENTARY_FILTER_ALPHA) * roll_acc;
                xSemaphoreGive(attitude_mutex);
            }
            
            ESP_LOGI(TAG, "Pitch: %.2f, Roll: %.2f", pitch, roll);
        }
        //10hz更新频率
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void lsm6ds_control_get_attitude(attitude_data_t* data)
{
    if (data != NULL && xSemaphoreTake(attitude_mutex, portMAX_DELAY) == pdTRUE) {
        data->pitch = pitch;
        data->roll = roll;
        xSemaphoreGive(attitude_mutex);
    }
}

esp_err_t init_lsm6ds3_control_task(void)
{
    if (attitude_mutex == NULL) {
        attitude_mutex = xSemaphoreCreateMutex();
        if (attitude_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create attitude mutex");
            return ESP_FAIL;
        }
    }

    if (s_lsm6ds3_control_task != NULL) {
        ESP_LOGW(TAG, "LSM6DS3 control task already running");
        return ESP_OK;
    }
    // 创建LSM6DS3控制任务
    BaseType_t result = xTaskCreatePinnedToCore(lsm6ds3_control_task, 
                                                "lsm6ds3_control",
                                                4096,
                                                NULL,
                                                5,
                                                &s_lsm6ds3_control_task,
                                                0);

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LSM6DS3 control task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "LSM6DS3 control task created successfully on Core 0");
    return ESP_OK;
}

TaskHandle_t get_lsm6ds3_control_task_handle(void) { return s_lsm6ds3_control_task; }
