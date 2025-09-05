/**
 * @file tcp_refactored_example.c
 * @brief 重构后TCP客户端使用示例
 * @author TidyCraze
 * @date 2025-09-05
 * 
 * 本示例展示如何使用重构后的独立心跳检测和遥测传输模块
 */

#include "tcp_hb/inc/tcp_client_hb.h"
#include "tcp_telemetry/inc/tcp_client_telemetry.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "TCP_REFACTORED_EXAMPLE";

// 服务器配置
#define SERVER_IP "192.168.97.247"
#define HEARTBEAT_PORT 7878
#define TELEMETRY_PORT 6667

// 任务配置
#define HEARTBEAT_TASK_STACK_SIZE 4096
#define TELEMETRY_TASK_STACK_SIZE 4096
#define HEARTBEAT_TASK_PRIORITY 5
#define TELEMETRY_TASK_PRIORITY 5

/**
 * @brief 基础使用示例
 * 展示如何初始化和启动独立的心跳和遥测模块
 */
void tcp_refactored_basic_example(void) {
    ESP_LOGI(TAG, "=== TCP重构基础使用示例 ===");
    
    // 1. 初始化心跳检测模块
    if (tcp_client_hb_init(SERVER_IP, HEARTBEAT_PORT)) {
        ESP_LOGI(TAG, "心跳模块初始化成功");
        
        // 启动心跳任务
        if (tcp_client_hb_start("heartbeat_task", HEARTBEAT_TASK_STACK_SIZE, HEARTBEAT_TASK_PRIORITY)) {
            ESP_LOGI(TAG, "心跳任务启动成功");
        } else {
            ESP_LOGE(TAG, "心跳任务启动失败");
        }
    } else {
        ESP_LOGE(TAG, "心跳模块初始化失败");
    }
    
    // 2. 初始化遥测传输模块
    if (tcp_client_telemetry_init(SERVER_IP, TELEMETRY_PORT)) {
        ESP_LOGI(TAG, "遥测模块初始化成功");
        
        // 启动遥测任务
        if (tcp_client_telemetry_start("telemetry_task", TELEMETRY_TASK_STACK_SIZE, TELEMETRY_TASK_PRIORITY)) {
            ESP_LOGI(TAG, "遥测任务启动成功");
        } else {
            ESP_LOGE(TAG, "遥测任务启动失败");
        }
    } else {
        ESP_LOGE(TAG, "遥测模块初始化失败");
    }
    
    ESP_LOGI(TAG, "两个模块已独立启动，开始运行");
}

/**
 * @brief 高级使用示例
 * 展示如何配置不同的服务器和端口，以及模块管理
 */
void tcp_refactored_advanced_example(void) {
    ESP_LOGI(TAG, "=== TCP重构高级使用示例 ===");
    
    // 使用不同的服务器配置
    const char *hb_server_ip = "192.168.1.100";
    const char *telemetry_server_ip = "192.168.1.101";
    uint16_t custom_hb_port = 8080;
    uint16_t custom_telemetry_port = 8081;
    
    // 1. 初始化心跳模块到专用心跳服务器
    if (tcp_client_hb_init(hb_server_ip, custom_hb_port)) {
        ESP_LOGI(TAG, "心跳模块初始化成功 -> %s:%d", hb_server_ip, custom_hb_port);
        
        // 设置设备状态
        tcp_client_hb_set_device_status(TCP_CLIENT_HB_DEVICE_STATUS_RUNNING);
        
        // 启动心跳任务
        tcp_client_hb_start("custom_hb_task", 8192, 6);
    }
    
    // 2. 初始化遥测模块到专用遥测服务器
    if (tcp_client_telemetry_init(telemetry_server_ip, custom_telemetry_port)) {
        ESP_LOGI(TAG, "遥测模块初始化成功 -> %s:%d", telemetry_server_ip, custom_telemetry_port);
        
        // 启动遥测任务
        tcp_client_telemetry_start("custom_telemetry_task", 8192, 6);
    }
    
    // 3. 运行一段时间后检查状态
    vTaskDelay(pdMS_TO_TICKS(10000)); // 等待10秒
    
    // 打印心跳模块状态
    ESP_LOGI(TAG, "心跳模块状态: %d", tcp_client_hb_get_state());
    tcp_client_hb_print_status();
    
    // 打印遥测模块状态
    ESP_LOGI(TAG, "遥测模块状态: %d", tcp_client_telemetry_get_state());
    tcp_client_telemetry_print_status();
    
    // 4. 演示模块独立控制
    ESP_LOGI(TAG, "演示模块独立控制...");
    
    // 停止心跳模块但保持遥测模块运行
    tcp_client_hb_stop();
    ESP_LOGI(TAG, "心跳模块已停止，遥测模块继续运行");
    
    vTaskDelay(pdMS_TO_TICKS(5000)); // 等待5秒
    
    // 重新启动心跳模块
    tcp_client_hb_start("restarted_hb_task", 4096, 5);
    ESP_LOGI(TAG, "心跳模块已重新启动");
}

/**
 * @brief 模块独立性验证示例
 * 验证两个模块完全独立运行，互不影响
 */
void tcp_refactored_independence_test(void) {
    ESP_LOGI(TAG, "=== 模块独立性验证测试 ===");
    
    // 1. 只启动心跳模块
    ESP_LOGI(TAG, "测试1: 只启动心跳模块");
    tcp_client_hb_init(SERVER_IP, HEARTBEAT_PORT);
    tcp_client_hb_start("hb_only_task", 4096, 5);
    
    vTaskDelay(pdMS_TO_TICKS(5000)); // 运行5秒
    
    const tcp_client_hb_stats_t *hb_stats = tcp_client_hb_get_stats();
    ESP_LOGI(TAG, "心跳模块独立运行 - 发送心跳: %lu", hb_stats->heartbeat_sent_count);
    
    // 2. 启动遥测模块（心跳模块继续运行）
    ESP_LOGI(TAG, "测试2: 启动遥测模块（心跳模块继续运行）");
    tcp_client_telemetry_init(SERVER_IP, TELEMETRY_PORT);
    tcp_client_telemetry_start("telemetry_independent_task", 4096, 5);
    
    vTaskDelay(pdMS_TO_TICKS(5000)); // 运行5秒
    
    const tcp_client_telemetry_stats_t *telemetry_stats = tcp_client_telemetry_get_stats();
    ESP_LOGI(TAG, "遥测模块独立运行 - 发送遥测: %lu", telemetry_stats->telemetry_sent_count);
    
    // 3. 停止心跳模块，验证遥测模块不受影响
    ESP_LOGI(TAG, "测试3: 停止心跳模块，验证遥测模块不受影响");
    uint32_t telemetry_count_before = telemetry_stats->telemetry_sent_count;
    
    tcp_client_hb_stop();
    ESP_LOGI(TAG, "心跳模块已停止");
    
    vTaskDelay(pdMS_TO_TICKS(3000)); // 等待3秒
    
    telemetry_stats = tcp_client_telemetry_get_stats();
    uint32_t telemetry_count_after = telemetry_stats->telemetry_sent_count;
    
    if (telemetry_count_after > telemetry_count_before) {
        ESP_LOGI(TAG, "✓ 验证通过: 遥测模块不受心跳模块停止影响");
        ESP_LOGI(TAG, "  停止前遥测数: %lu, 停止后遥测数: %lu", 
                telemetry_count_before, telemetry_count_after);
    } else {
        ESP_LOGW(TAG, "✗ 验证失败: 遥测模块可能受到影响");
    }
    
    // 4. 清理
    tcp_client_telemetry_stop();
    tcp_client_hb_destroy();
    tcp_client_telemetry_destroy();
    
    ESP_LOGI(TAG, "独立性测试完成");
}

/**
 * @brief 错误处理和恢复示例
 * 展示模块的错误处理和自动恢复能力
 */
void tcp_refactored_error_handling_example(void) {
    ESP_LOGI(TAG, "=== 错误处理和恢复示例 ===");
    
    // 使用无效IP测试错误处理
    const char *invalid_ip = "192.168.999.999";
    
    // 1. 测试心跳模块错误处理
    ESP_LOGI(TAG, "测试心跳模块错误处理");
    if (!tcp_client_hb_init(invalid_ip, HEARTBEAT_PORT)) {
        ESP_LOGI(TAG, "✓ 心跳模块正确处理了无效IP");
    }
    
    // 使用有效配置重新初始化
    tcp_client_hb_init(SERVER_IP, HEARTBEAT_PORT);
    tcp_client_hb_set_auto_reconnect(true); // 启用自动重连
    tcp_client_hb_start("hb_recovery_task", 4096, 5);
    
    // 2. 测试遥测模块错误处理
    ESP_LOGI(TAG, "测试遥测模块错误处理");
    if (!tcp_client_telemetry_init(invalid_ip, TELEMETRY_PORT)) {
        ESP_LOGI(TAG, "✓ 遥测模块正确处理了无效IP");
    }
    
    // 使用有效配置重新初始化
    tcp_client_telemetry_init(SERVER_IP, TELEMETRY_PORT);
    tcp_client_telemetry_set_auto_reconnect(true); // 启用自动重连
    tcp_client_telemetry_start("telemetry_recovery_task", 4096, 5);
    
    // 3. 监控连接健康状态
    ESP_LOGI(TAG, "监控连接健康状态");
    for (int i = 0; i < 10; i++) {
        bool hb_healthy = tcp_client_hb_is_connection_healthy();
        bool telemetry_healthy = tcp_client_telemetry_is_connection_healthy();
        
        ESP_LOGI(TAG, "第%d次检查 - 心跳健康: %s, 遥测健康: %s", 
                i + 1, 
                hb_healthy ? "是" : "否",
                telemetry_healthy ? "是" : "否");
        
        vTaskDelay(pdMS_TO_TICKS(2000)); // 每2秒检查一次
    }
    
    // 清理
    tcp_client_hb_stop();
    tcp_client_telemetry_stop();
    ESP_LOGI(TAG, "错误处理测试完成");
}

/**
 * @brief 性能和统计信息示例
 * 展示如何获取和使用模块的统计信息
 */
void tcp_refactored_statistics_example(void) {
    ESP_LOGI(TAG, "=== 性能和统计信息示例 ===");
    
    // 初始化两个模块
    tcp_client_hb_init(SERVER_IP, HEARTBEAT_PORT);
    tcp_client_telemetry_init(SERVER_IP, TELEMETRY_PORT);
    
    // 启动模块
    tcp_client_hb_start("hb_stats_task", 4096, 5);
    tcp_client_telemetry_start("telemetry_stats_task", 4096, 5);
    
    // 运行一段时间收集统计信息
    ESP_LOGI(TAG, "收集统计信息中...");
    vTaskDelay(pdMS_TO_TICKS(15000)); // 运行15秒
    
    // 获取并显示心跳模块统计信息
    const tcp_client_hb_stats_t *hb_stats = tcp_client_hb_get_stats();
    ESP_LOGI(TAG, "=== 心跳模块统计 ===");
    ESP_LOGI(TAG, "发送心跳包: %lu", hb_stats->heartbeat_sent_count);
    ESP_LOGI(TAG, "发送失败: %lu", hb_stats->heartbeat_failed_count);
    ESP_LOGI(TAG, "连接次数: %lu", hb_stats->connection_count);
    ESP_LOGI(TAG, "重连次数: %lu", hb_stats->reconnection_count);
    ESP_LOGI(TAG, "总连接时长: %llu ms", hb_stats->total_connected_time);
    
    // 获取并显示遥测模块统计信息
    const tcp_client_telemetry_stats_t *telemetry_stats = tcp_client_telemetry_get_stats();
    ESP_LOGI(TAG, "=== 遥测模块统计 ===");
    ESP_LOGI(TAG, "发送遥测包: %lu", telemetry_stats->telemetry_sent_count);
    ESP_LOGI(TAG, "发送失败: %lu", telemetry_stats->telemetry_failed_count);
    ESP_LOGI(TAG, "连接次数: %lu", telemetry_stats->connection_count);
    ESP_LOGI(TAG, "重连次数: %lu", telemetry_stats->reconnection_count);
    ESP_LOGI(TAG, "发送字节数: %lu", telemetry_stats->bytes_sent);
    ESP_LOGI(TAG, "接收字节数: %lu", telemetry_stats->bytes_received);
    ESP_LOGI(TAG, "总连接时长: %llu ms", telemetry_stats->total_connected_time);
    
    // 计算性能指标
    if (hb_stats->total_connected_time > 0) {
        float hb_success_rate = (float)hb_stats->heartbeat_sent_count / 
                               (hb_stats->heartbeat_sent_count + hb_stats->heartbeat_failed_count) * 100.0f;
        ESP_LOGI(TAG, "心跳成功率: %.2f%%", hb_success_rate);
    }
    
    if (telemetry_stats->total_connected_time > 0) {
        float telemetry_success_rate = (float)telemetry_stats->telemetry_sent_count / 
                                      (telemetry_stats->telemetry_sent_count + telemetry_stats->telemetry_failed_count) * 100.0f;
        ESP_LOGI(TAG, "遥测成功率: %.2f%%", telemetry_success_rate);
        
        float avg_throughput = (float)telemetry_stats->bytes_sent / (telemetry_stats->total_connected_time / 1000.0f);
        ESP_LOGI(TAG, "平均发送吞吐量: %.2f 字节/秒", avg_throughput);
    }
    
    // 清理
    tcp_client_hb_stop();
    tcp_client_telemetry_stop();
    ESP_LOGI(TAG, "统计信息示例完成");
}

/**
 * @brief 主示例函数
 * 依次运行所有示例
 */
void tcp_refactored_example_main(void) {
    ESP_LOGI(TAG, "开始TCP重构示例演示");
    
    // 1. 基础使用示例
    tcp_refactored_basic_example();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 清理上一个示例
    tcp_client_hb_destroy();
    tcp_client_telemetry_destroy();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 2. 高级使用示例
    tcp_refactored_advanced_example();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 清理
    tcp_client_hb_destroy();
    tcp_client_telemetry_destroy();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 3. 独立性验证测试
    tcp_refactored_independence_test();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 4. 错误处理示例
    tcp_refactored_error_handling_example();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 清理
    tcp_client_hb_destroy();
    tcp_client_telemetry_destroy();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 5. 统计信息示例
    tcp_refactored_statistics_example();
    
    // 最终清理
    tcp_client_hb_destroy();
    tcp_client_telemetry_destroy();
    
    ESP_LOGI(TAG, "所有TCP重构示例演示完成");
}