/*
 * @Author: tidycraze 2595256284@qq.com
 * @Date: 2025-08-22 16:32:04
 * @LastEditors: tidycraze 2595256284@qq.com
 * @LastEditTime: 2025-08-22 17:15:37
 * @FilePath: \demo-hello-world\main\components_init.c
 * @Description: 用于集中初始化必要外设
 * 
 * Copyright (c) 2025 by ${git_name_email}, All Rights Reserved. 
 */

 #include <dirent.h>
 #include <stdlib.h>
 #include <string.h>

 #include "esp_err.h"
 #include "esp_log.h"
 #include "esp_spiffs.h"
 #include "nvs_flash.h"
 #include "wifi_manager.h"

 #include "battery_monitor.h"
 #include "calibration_manager.h"

 const char *TAG = "components_init";

 /**
 * @brief 初始化SPIFFS文件系统
 * @return esp_err_t ESP_OK成功，其他错误码失败
 */
esp_err_t spiffs_init(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",
        .max_files = 5,
        .format_if_mount_failed = false 
    };

    // 挂载SPIFFS分区
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    // 获取SPIFFS分区信息
    size_t total = 0, used = 0;
    ret = esp_spiffs_info("spiffs", &total, &used);
    if (ret != ESP_OK) {
        esp_vfs_spiffs_unregister("spiffs");
        return ret;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    // 检查SPIFFS分区是否正常工作
    if (used > total) {
        ESP_LOGW(TAG, "Number of used bytes cannot be larger than total. Performing SPIFFS check.");
        ret = esp_spiffs_check("spiffs");
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPIFFS check failed (%s)", esp_err_to_name(ret));
            return ret;
        } else {
            ESP_LOGI(TAG, "SPIFFS check successful");
        }
    }

    ESP_LOGI(TAG, "SPIFFS mounted successfully");
    return ESP_OK;
}

/**
 * @brief 卸载SPIFFS文件系统
 */
void spiffs_deinit(void) {
    esp_vfs_spiffs_unregister("spiffs");
    ESP_LOGI(TAG, "SPIFFS unmounted");
}

/**
 * @brief 初始化所有必要组件
 * @return esp_err_t ESP_OK成功，其他错误码失败
 */
esp_err_t components_init(void) {
    esp_err_t ret;

    // 初始化NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化SPIFFS文件系统
    ret = spiffs_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS");
        return ret;
    }

    // 初始化校准管理器
    ret = calibration_manager_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration manager initialized");
    } else {
        ESP_LOGW(TAG, "Calibration manager init failed: %s", esp_err_to_name(ret));
    }

    // 初始化电池监测模块
    ret = battery_monitor_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Battery monitor initialized");
    } else {
        ESP_LOGW(TAG, "Battery monitor init failed: %s", esp_err_to_name(ret));
    }

    // 其他组件初始化可以在这里添加

    ESP_LOGI(TAG, "All components initialized successfully");
    return ESP_OK;
}