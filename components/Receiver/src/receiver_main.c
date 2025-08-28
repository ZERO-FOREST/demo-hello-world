#include "tcp_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "tcp_receiver";

// 重写协议处理函数
void handle_remote_control_data(const remote_control_payload_t *remote_data) {
    if (!remote_data) return;
    
    ESP_LOGI(TAG, "[遥控数据]通道数: %d", remote_data->channel_count);
    for (uint8_t i = 0; i < remote_data->channel_count && i < MAX_CHANNELS; i++) {
        const char* ch_name;
        switch (i) {
            case 0: ch_name = "油门"; break;
            case 1: ch_name = "方向"; break;
            case 2: ch_name = "俯仰"; break;
            case 3: ch_name = "横滚"; break;
            default: ch_name = "辅助"; break;
        }
        ESP_LOGI(TAG, "  %s(CH%d): %d", ch_name, i+1, remote_data->channels[i]);
    }
}

void handle_heartbeat_data(const heartbeat_payload_t *heartbeat_data) {
    if (!heartbeat_data) return;
    
    const char* status_map[] = {"空闲", "正常运行", "错误"};
    const char* status_str = (heartbeat_data->device_status <= 2) ? 
                            status_map[heartbeat_data->device_status] : "未知";
    
    ESP_LOGI(TAG, "【心跳包】设备状态: %s (0x%02X)", status_str, heartbeat_data->device_status);
}

void handle_extended_command(const extended_cmd_payload_t *cmd_data) {
    if (!cmd_data) return;
    
    ESP_LOGI(TAG, "【扩展命令】ID: 0x%02X, 参数长度: %d", cmd_data->cmd_id, cmd_data->param_len);
    
    // 根据命令ID解析具体命令
    switch (cmd_data->cmd_id) {
        case 0x10:
            if (cmd_data->param_len >= 2) {
                uint16_t freq = cmd_data->params[0] | (cmd_data->params[1] << 8);
                ESP_LOGI(TAG, "  设置PWM频率: %d Hz", freq);
            }
            break;
        case 0x11:
            if (cmd_data->param_len >= 1) {
                const char* mode = (cmd_data->params[0] == 0) ? "手动" : "自动";
                ESP_LOGI(TAG, "  模式切换: %s", mode);
            }
            break;
        case 0x12:
            ESP_LOGI(TAG, "  校准传感器");
            break;
        case 0x13:
            ESP_LOGI(TAG, "  请求遥测数据");
            break;
        case 0x14:
            if (cmd_data->param_len >= 1) {
                const char* light = (cmd_data->params[0] == 0) ? "关闭" : "开启";
                ESP_LOGI(TAG, "  灯光控制: %s", light);
            }
            break;
        default:
            ESP_LOGI(TAG, "  未知命令");
            if (cmd_data->param_len > 0) {
                char hex_str[256] = {0};
                for (uint8_t i = 0; i < cmd_data->param_len && i < 100; i++) {
                    sprintf(hex_str + strlen(hex_str), "%02X ", cmd_data->params[i]);
                }
                ESP_LOGI(TAG, "  参数: %s", hex_str);
            }
            break;
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "  ESP32 TCP 客户端接收端 v1.0");
    ESP_LOGI(TAG, "  基于协议文档的C语言实现");
    ESP_LOGI(TAG, "===========================================");
    
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "请确保服务器ESP32已启动并运行在 %s:%d", ESP32_SERVER_IP, ESP32_SERVER_PORT);
    
    // 启动TCP客户端任务
    tcp_client_task();
}
