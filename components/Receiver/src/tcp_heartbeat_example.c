/**
 * @file tcp_heartbeat_example.c
 * @brief TCP心跳客户端使用示例
 * 
 * 本文件展示如何使用带心跳功能的TCP客户端，替代原有的tcp_client_task函数。
 * 
 * 主要特性：
 * 1. 严格遵循协议文档中定义的心跳包格式
 * 2. 服务端口通过宏定义配置（默认7878）
 * 3. 30秒心跳间隔，5秒自动重连
 * 4. 完整的日志记录和连接管理
 * 5. 网络异常检测和自动恢复
 */

#include "tcp_client_with_heartbeat.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TCP_HB_EXAMPLE";

/**
 * @brief TCP心跳客户端使用示例
 * 
 * 此函数展示如何初始化和启动带心跳功能的TCP客户端。
 * 可以替代原有的tcp_client_task()函数。
 */
void tcp_heartbeat_example_task(void *pvParameters) {
    (void)pvParameters;
    
    ESP_LOGI(TAG, "TCP心跳客户端示例启动");
    
    // 1. 初始化带心跳的TCP客户端
    // 使用默认服务器IP和端口7878（通过宏定义配置）
    if (!tcp_client_hb_init(NULL, 0)) {
        ESP_LOGE(TAG, "TCP心跳客户端初始化失败");
        vTaskDelete(NULL);
        return;
    }
    
    // 2. 启动客户端（包含心跳管理器）
    if (!tcp_client_hb_start()) {
        ESP_LOGE(TAG, "TCP心跳客户端启动失败");
        tcp_client_hb_destroy();
        vTaskDelete(NULL);
        return;
    }
    
    // 3. 启动客户端任务（处理遥测数据发送）
    if (!tcp_client_hb_start_task("HeartbeatClient", 4096, 5)) {
        ESP_LOGE(TAG, "TCP心跳客户端任务启动失败");
        tcp_client_hb_stop();
        tcp_client_hb_destroy();
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "TCP心跳客户端启动成功");
    
    // 4. 监控循环
    uint32_t status_print_counter = 0;
    
    while (1) {
        // 每30秒打印一次状态信息
        if (++status_print_counter >= 300) { // 30秒 / 100ms = 300次
            tcp_client_hb_print_status();
            status_print_counter = 0;
        }
        
        // 检查连接健康状态
        if (!tcp_client_hb_is_connection_healthy()) {
            ESP_LOGW(TAG, "检测到连接异常");
        }
        
        // 检查客户端状态
        tcp_client_hb_status_t status = tcp_client_hb_get_status();
        if (status == TCP_CLIENT_HB_STATUS_ERROR) {
            ESP_LOGE(TAG, "客户端处于错误状态，尝试重启");
            
            // 停止并重新启动
            tcp_client_hb_stop();
            vTaskDelay(pdMS_TO_TICKS(1000)); // 等待1秒
            
            if (!tcp_client_hb_start()) {
                ESP_LOGE(TAG, "客户端重启失败");
            } else {
                ESP_LOGI(TAG, "客户端重启成功");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms
    }
}

/**
 * @brief 高级使用示例：自定义配置
 * 
 * 展示如何使用自定义服务器IP和端口
 */
void tcp_heartbeat_advanced_example(void) {
    ESP_LOGI(TAG, "TCP心跳客户端高级示例");
    
    // 使用自定义服务器配置
    const char *custom_server_ip = "192.168.1.100";
    uint16_t custom_server_port = 8080;
    
    // 初始化
    if (!tcp_client_hb_init(custom_server_ip, custom_server_port)) {
        ESP_LOGE(TAG, "自定义配置初始化失败");
        return;
    }
    
    // 禁用自动重连（可选）
    tcp_client_hb_set_auto_reconnect(false);
    
    // 设置设备状态
    tcp_client_hb_set_device_status(DEVICE_STATUS_RUNNING);
    
    // 启动
    if (tcp_client_hb_start()) {
        ESP_LOGI(TAG, "自定义配置启动成功");
        
        // 手动发送心跳测试
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (tcp_client_hb_send_heartbeat_now()) {
            ESP_LOGI(TAG, "手动心跳发送成功");
        }
        
        // 手动重连测试
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (tcp_client_hb_reconnect_now()) {
            ESP_LOGI(TAG, "手动重连成功");
        }
    }
    
    // 清理
    tcp_client_hb_destroy();
}

/**
 * @brief 统计信息监控示例
 * 
 * 展示如何监控心跳和遥测统计信息
 */
void tcp_heartbeat_stats_monitor_example(void) {
    ESP_LOGI(TAG, "统计信息监控示例");
    
    // 获取统计信息
    const tcp_client_hb_stats_t *stats = tcp_client_hb_get_stats();
    
    if (stats && stats->heartbeat_stats) {
        ESP_LOGI(TAG, "=== 统计信息 ===");
        ESP_LOGI(TAG, "心跳发送: %lu", stats->heartbeat_stats->heartbeat_sent_count);
        ESP_LOGI(TAG, "心跳失败: %lu", stats->heartbeat_stats->heartbeat_failed_count);
        ESP_LOGI(TAG, "连接次数: %lu", stats->heartbeat_stats->connection_count);
        ESP_LOGI(TAG, "重连次数: %lu", stats->heartbeat_stats->reconnection_count);
        ESP_LOGI(TAG, "遥测发送: %lu", stats->telemetry_sent_count);
        ESP_LOGI(TAG, "遥测失败: %lu", stats->telemetry_failed_count);
        ESP_LOGI(TAG, "总连接时长: %llu ms", stats->heartbeat_stats->total_connected_time);
        ESP_LOGI(TAG, "================");
    }
    
    // 重置统计信息（可选）
    // tcp_client_hb_reset_stats();
}

/**
 * @brief 替代原有tcp_client_task的简单示例
 * 
 * 这是最简单的使用方式，直接替代原有的tcp_client_task函数
 */
void tcp_client_task_replacement(void) {
    ESP_LOGI(TAG, "启动TCP心跳客户端（替代原tcp_client_task）");
    
    // 一行代码初始化并启动
    if (tcp_client_hb_init(NULL, 0) && tcp_client_hb_start()) {
        // 启动处理任务
        tcp_client_hb_start_task(NULL, 0, 0);
        
        ESP_LOGI(TAG, "TCP心跳客户端启动成功，开始自动心跳和遥测数据发送");
        ESP_LOGI(TAG, "心跳间隔: 30秒, 服务端口: 7878, 自动重连: 5秒间隔");
    } else {
        ESP_LOGE(TAG, "TCP心跳客户端启动失败");
    }
}

/**
 * @brief 主入口函数示例
 * 
 * 在main.c中调用此函数来启动TCP心跳客户端
 */
void start_tcp_heartbeat_client(void) {
    // 创建示例任务
    xTaskCreate(
        tcp_heartbeat_example_task,
        "TCPHeartbeatExample",
        4096,
        NULL,
        5,
        NULL
    );
    
    ESP_LOGI(TAG, "TCP心跳客户端示例任务已创建");
}