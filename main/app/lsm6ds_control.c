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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lsm6ds_control.h"

static char* TAG = "LSM6DS3_CTRL";

TaskHandle_t s_lsm6ds3_control_task = NULL;

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
    while (1) {

        ret = lsm6ds3_read_all(&sensor_data);
        //10hz更新频率
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t init_lsm6ds3_control_task(void)
{
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
