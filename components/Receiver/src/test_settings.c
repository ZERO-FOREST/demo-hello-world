#include "settings_manager.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "TestSettings";

void test_settings_manager(void) {
    ESP_LOGI(TAG, "Testing Settings Manager...");
    
    // 初始化设置管理器
    esp_err_t ret = settings_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize settings manager");
        return;
    }
    
    // 测试USB设置接口
    ESP_LOGI(TAG, "Testing USB interface...");
    
    // 设置WIFI SSID
    ret = settings_set_via_usb(SETTING_WIFI_SSID, "MyWiFiNetwork");
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WIFI SSID set successfully");
    }
    
    // 设置WIFI密码
    ret = settings_set_via_usb(SETTING_WIFI_PASSWORD, "MyPassword123");
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WIFI Password set successfully");
    }
    
    // 设置JPEG质量
    ret = settings_set_via_usb(SETTING_JPEG_QUALITY, "90");
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "JPEG Quality set successfully");
    }
    
    // 测试SPI设置接口
    ESP_LOGI(TAG, "Testing SPI interface...");
    
    // 通过SPI设置JPEG质量（HEX格式）
    uint8_t quality_data[] = {0x5A}; // 90的十六进制
    ret = settings_set_via_spi(SETTING_JPEG_QUALITY, quality_data, sizeof(quality_data));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "JPEG Quality set via SPI successfully");
    }
    
    // 获取设置值验证
    setting_value_t value;
    
    ret = settings_get(SETTING_WIFI_SSID, &value);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Current WIFI SSID: %s", value.str_value);
    }
    
    ret = settings_get(SETTING_JPEG_QUALITY, &value);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Current JPEG Quality: %u", value.uint8_value);
    }
    
    // 保存设置到NVS
    ret = settings_save_to_nvs();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Settings saved to NVS successfully");
    }
    
    ESP_LOGI(TAG, "Settings Manager test completed");
}