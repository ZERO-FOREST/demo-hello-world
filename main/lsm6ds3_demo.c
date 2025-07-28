/**
 * @file lsm6ds3_demo.c
 * @brief LSM6DS3 陀螺仪驱动演示程序
 * @author Your Name
 * @date 2024
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "lsm6ds3.h"

static const char *TAG = "LSM6DS3_DEMO";

/**
 * @brief LSM6DS3传感器演示任务
 */
void lsm6ds3_demo_task(void *pvParameters)
{
    esp_err_t ret;
    lsm6ds3_data_t sensor_data;
    int sample_count = 0;
    
    ESP_LOGI(TAG, "LSM6DS3 Demo Task Started");
    
    // 初始化LSM6DS3传感器
    ret = lsm6ds3_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LSM6DS3: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "LSM6DS3 initialized successfully");
    
    // 配置加速度计: 104Hz, ±2g
    ret = lsm6ds3_config_accel(LSM6DS3_ODR_104_HZ, LSM6DS3_ACCEL_FS_2G);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure accelerometer");
        lsm6ds3_deinit();
        vTaskDelete(NULL);
        return;
    }
    
    // 配置陀螺仪: 104Hz, ±250dps
    ret = lsm6ds3_config_gyro(LSM6DS3_ODR_104_HZ, LSM6DS3_GYRO_FS_250DPS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure gyroscope");
        lsm6ds3_deinit();
        vTaskDelete(NULL);
        return;
    }
    
    // 启用传感器
    lsm6ds3_accel_enable(true);
    lsm6ds3_gyro_enable(true);
    
    ESP_LOGI(TAG, "LSM6DS3 configured and enabled");
    ESP_LOGI(TAG, "Starting sensor data reading...");
    ESP_LOGI(TAG, "Format: [Sample] Accel(g): X, Y, Z | Gyro(dps): X, Y, Z | Temp(°C)");
    ESP_LOGI(TAG, "================================================================");
    
    // 主循环 - 读取传感器数据
    while (1) {
        // 读取所有传感器数据
        ret = lsm6ds3_read_all(&sensor_data);
        if (ret == ESP_OK) {
            sample_count++;
            
            // 打印传感器数据
            printf("[%4d] Accel(g): %6.2f, %6.2f, %6.2f | Gyro(dps): %7.2f, %7.2f, %7.2f | Temp: %5.1f°C\n",
                   sample_count,
                   sensor_data.accel.x, sensor_data.accel.y, sensor_data.accel.z,
                   sensor_data.gyro.x, sensor_data.gyro.y, sensor_data.gyro.z,
                   sensor_data.temp.temperature);
            
            // 每100个样本打印一次标题
            if (sample_count % 100 == 0) {
                ESP_LOGI(TAG, "Sample count: %d", sample_count);
            }
        } else {
            ESP_LOGE(TAG, "Failed to read sensor data: %s", esp_err_to_name(ret));
        }
        
        // 延时100ms (10Hz采样率)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief 启动LSM6DS3演示
 */
void start_lsm6ds3_demo(void)
{
    // 创建LSM6DS3演示任务
    xTaskCreate(lsm6ds3_demo_task, "lsm6ds3_demo", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "LSM6DS3 demo task created");
} 