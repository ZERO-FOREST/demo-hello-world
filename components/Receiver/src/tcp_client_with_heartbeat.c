/**
 * @file tcp_client_with_heartbeat.c
 * @brief TCP客户端心跳管理实现
 * @author TidyCraze
 * @date 2025-09-04
 */

#include "tcp_client_with_heartbeat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "TCP_CLIENT_HB";

// ----------------- 全局变量 -----------------
static tcp_client_hb_status_t g_client_status = TCP_CLIENT_HB_STATUS_STOPPED;
static tcp_client_hb_stats_t g_client_stats = {0};
static TaskHandle_t g_client_task_handle = NULL;
static bool g_client_initialized = false;
static bool g_task_running = false;

// 遥测数据统计
static uint32_t g_telemetry_sent_count = 0;
static uint32_t g_telemetry_failed_count = 0;
static uint64_t g_last_telemetry_time = 0;

// 模拟遥测数据（与原tcp_client.c保持一致）
static simulated_telemetry_t g_sim_telemetry = {
    .voltage_mv = 3850,   // 3.85V
    .current_ma = 150,    // 150mA
    .roll_deg = 5,        // 0.05 deg
    .pitch_deg = -10,     // -0.10 deg
    .yaw_deg = 2500,      // 25.00 deg
    .altitude_cm = 1000,  // 10m
};

// ----------------- 内部函数声明 -----------------
static void tcp_client_hb_task(void *pvParameters);
static void tcp_client_hb_set_status(tcp_client_hb_status_t new_status);
static void tcp_client_hb_update_stats(void);
static uint64_t tcp_client_hb_get_timestamp_ms(void);
static void tcp_client_hb_update_simulated_telemetry(simulated_telemetry_t *sim_data);

// ----------------- 内部函数实现 -----------------

static uint64_t tcp_client_hb_get_timestamp_ms(void) {
    return (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void tcp_client_hb_set_status(tcp_client_hb_status_t new_status) {
    if (g_client_status != new_status) {
        ESP_LOGI(TAG, "客户端状态变更: %d -> %d", g_client_status, new_status);
        g_client_status = new_status;
    }
}

static void tcp_client_hb_update_stats(void) {
    // 更新统计信息
    g_client_stats.heartbeat_stats = tcp_heartbeat_manager_get_stats();
    g_client_stats.telemetry_sent_count = g_telemetry_sent_count;
    g_client_stats.telemetry_failed_count = g_telemetry_failed_count;
    g_client_stats.last_telemetry_time = g_last_telemetry_time;
    g_client_stats.client_status = g_client_status;
    g_client_stats.heartbeat_state = tcp_heartbeat_manager_get_state();
    g_client_stats.legacy_client_state = tcp_client_get_state();
}

static void tcp_client_hb_update_simulated_telemetry(simulated_telemetry_t *sim_data) {
    if (!sim_data) return;
    
    // 模拟数据变化（与原tcp_client.c保持一致）
    static int counter = 0;
    counter++;
    
    // 电压在3.7V到4.2V之间波动
    sim_data->voltage_mv = 3700 + (counter % 500);
    
    // 电流在100mA到300mA之间波动
    sim_data->current_ma = 100 + (counter % 200);
    
    // 姿态角小幅度变化
    sim_data->roll_deg = (counter % 100) - 50;    // -0.5° to +0.5°
    sim_data->pitch_deg = (counter % 60) - 30;    // -0.3° to +0.3°
    sim_data->yaw_deg = 2500 + (counter % 200);   // 25.0° ± 2.0°
    
    // 高度缓慢变化
    sim_data->altitude_cm = 1000 + (counter % 1000); // 10m ± 10m
}

static void tcp_client_hb_task(void *pvParameters) {
    (void)pvParameters; // 避免未使用参数警告
    
    ESP_LOGI(TAG, "带心跳的TCP客户端任务启动");
    
    tcp_client_hb_set_status(TCP_CLIENT_HB_STATUS_RUNNING);
    
    time_t last_telemetry_send = 0;
    
    while (g_task_running) {
        // 更新统计信息
        tcp_client_hb_update_stats();
        
        // 检查心跳管理器状态
        tcp_heartbeat_state_t hb_state = tcp_heartbeat_manager_get_state();
        
        if (hb_state == TCP_HB_STATE_CONNECTED) {
            // 连接正常，处理遥测数据发送
            time_t current_time = time(NULL);
            
            // 每秒发送一次遥测数据
            if (current_time - last_telemetry_send >= 1) {
                tcp_client_hb_update_simulated_telemetry(&g_sim_telemetry);
                
                telemetry_data_payload_t telemetry = {
                    .voltage_mv = g_sim_telemetry.voltage_mv,
                    .current_ma = g_sim_telemetry.current_ma,
                    .roll_deg = g_sim_telemetry.roll_deg,
                    .pitch_deg = g_sim_telemetry.pitch_deg,
                    .yaw_deg = g_sim_telemetry.yaw_deg,
                    .altitude_cm = g_sim_telemetry.altitude_cm
                };
                
                if (tcp_client_hb_send_telemetry(&telemetry)) {
                    ESP_LOGD(TAG, "遥测数据发送成功");
                } else {
                    ESP_LOGW(TAG, "遥测数据发送失败");
                }
                
                last_telemetry_send = current_time;
            }
            
            // 根据心跳状态设置设备状态
            tcp_heartbeat_manager_set_device_status(DEVICE_STATUS_RUNNING);
            
        } else if (hb_state == TCP_HB_STATE_CONNECTING || hb_state == TCP_HB_STATE_RECONNECTING) {
            // 连接中，设置设备状态为空闲
            tcp_heartbeat_manager_set_device_status(DEVICE_STATUS_IDLE);
            ESP_LOGD(TAG, "等待连接建立...");
            
        } else if (hb_state == TCP_HB_STATE_ERROR) {
            // 连接错误
            tcp_heartbeat_manager_set_device_status(DEVICE_STATUS_ERROR);
            ESP_LOGW(TAG, "心跳管理器处于错误状态");
            
        } else {
            // 未连接状态
            tcp_heartbeat_manager_set_device_status(DEVICE_STATUS_IDLE);
            ESP_LOGD(TAG, "心跳管理器未连接");
        }
        
        // 任务休眠
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms
    }
    
    ESP_LOGI(TAG, "带心跳的TCP客户端任务结束");
    tcp_client_hb_set_status(TCP_CLIENT_HB_STATUS_STOPPED);
    g_client_task_handle = NULL;
    vTaskDelete(NULL);
}

// ----------------- 公共函数实现 -----------------

bool tcp_client_hb_init(const char *server_ip, uint16_t server_port) {
    if (g_client_initialized) {
        ESP_LOGW(TAG, "客户端已初始化");
        return true;
    }
    
    ESP_LOGI(TAG, "初始化带心跳的TCP客户端...");
    
    tcp_client_hb_set_status(TCP_CLIENT_HB_STATUS_STARTING);
    
    // 使用默认值
    const char *ip = server_ip ? server_ip : TCP_CLIENT_HB_DEFAULT_SERVER_IP;
    uint16_t port = (server_port > 0) ? server_port : TCP_CLIENT_HB_DEFAULT_SERVER_PORT;
    
    // 初始化原有的TCP客户端
    if (!tcp_client_init()) {
        ESP_LOGE(TAG, "原有TCP客户端初始化失败");
        tcp_client_hb_set_status(TCP_CLIENT_HB_STATUS_ERROR);
        return false;
    }
    
    // 初始化心跳管理器
    if (!tcp_heartbeat_manager_init(ip, port)) {
        ESP_LOGE(TAG, "心跳管理器初始化失败");
        tcp_client_hb_set_status(TCP_CLIENT_HB_STATUS_ERROR);
        return false;
    }
    
    // 初始化统计信息
    memset(&g_client_stats, 0, sizeof(g_client_stats));
    g_telemetry_sent_count = 0;
    g_telemetry_failed_count = 0;
    g_last_telemetry_time = 0;
    
    g_client_initialized = true;
    tcp_client_hb_set_status(TCP_CLIENT_HB_STATUS_STOPPED);
    
    ESP_LOGI(TAG, "带心跳的TCP客户端初始化成功 - 服务器: %s:%d", ip, port);
    
    return true;
}

bool tcp_client_hb_start(void) {
    if (!g_client_initialized) {
        ESP_LOGE(TAG, "客户端未初始化");
        return false;
    }
    
    if (g_client_status == TCP_CLIENT_HB_STATUS_RUNNING) {
        ESP_LOGW(TAG, "客户端已在运行");
        return true;
    }
    
    ESP_LOGI(TAG, "启动带心跳的TCP客户端...");
    
    tcp_client_hb_set_status(TCP_CLIENT_HB_STATUS_STARTING);
    
    // 启动心跳管理器
    if (!tcp_heartbeat_manager_start()) {
        ESP_LOGE(TAG, "心跳管理器启动失败");
        tcp_client_hb_set_status(TCP_CLIENT_HB_STATUS_ERROR);
        return false;
    }
    
    ESP_LOGI(TAG, "带心跳的TCP客户端启动成功");
    tcp_client_hb_set_status(TCP_CLIENT_HB_STATUS_RUNNING);
    
    return true;
}

void tcp_client_hb_stop(void) {
    if (g_client_status == TCP_CLIENT_HB_STATUS_STOPPED) {
        ESP_LOGW(TAG, "客户端已停止");
        return;
    }
    
    ESP_LOGI(TAG, "停止带心跳的TCP客户端...");
    
    tcp_client_hb_set_status(TCP_CLIENT_HB_STATUS_STOPPING);
    
    // 停止任务
    tcp_client_hb_stop_task();
    
    // 停止心跳管理器
    tcp_heartbeat_manager_stop();
    
    // 断开原有TCP客户端连接
    tcp_client_disconnect();
    
    tcp_client_hb_set_status(TCP_CLIENT_HB_STATUS_STOPPED);
    
    ESP_LOGI(TAG, "带心跳的TCP客户端已停止");
}

void tcp_client_hb_destroy(void) {
    if (!g_client_initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "销毁带心跳的TCP客户端...");
    
    // 先停止
    tcp_client_hb_stop();
    
    // 销毁心跳管理器
    tcp_heartbeat_manager_destroy();
    
    // 清零统计信息
    memset(&g_client_stats, 0, sizeof(g_client_stats));
    g_telemetry_sent_count = 0;
    g_telemetry_failed_count = 0;
    g_last_telemetry_time = 0;
    
    g_client_initialized = false;
    
    ESP_LOGI(TAG, "带心跳的TCP客户端已销毁");
}

tcp_client_hb_status_t tcp_client_hb_get_status(void) {
    return g_client_status;
}

const tcp_client_hb_stats_t* tcp_client_hb_get_stats(void) {
    tcp_client_hb_update_stats();
    return &g_client_stats;
}

void tcp_client_hb_set_device_status(uint8_t status) {
    tcp_heartbeat_manager_set_device_status(status);
}

bool tcp_client_hb_send_telemetry(const telemetry_data_payload_t *telemetry_data) {
    if (!telemetry_data) {
        ESP_LOGE(TAG, "遥测数据指针为空");
        g_telemetry_failed_count++;
        return false;
    }
    
    // 检查心跳管理器连接状态
    if (tcp_heartbeat_manager_get_state() != TCP_HB_STATE_CONNECTED) {
        ESP_LOGW(TAG, "心跳管理器未连接，无法发送遥测数据");
        g_telemetry_failed_count++;
        return false;
    }
    
    // 使用原有的TCP客户端发送遥测数据
    if (tcp_client_send_telemetry(telemetry_data)) {
        g_telemetry_sent_count++;
        g_last_telemetry_time = tcp_client_hb_get_timestamp_ms();
        
        ESP_LOGD(TAG, "遥测数据发送成功 [%lu] - 电压: %d mV, 电流: %d mA, 姿态: (%d, %d, %d), 高度: %ld cm",
                g_telemetry_sent_count,
                telemetry_data->voltage_mv,
                telemetry_data->current_ma,
                telemetry_data->roll_deg,
                telemetry_data->pitch_deg,
                telemetry_data->yaw_deg,
                telemetry_data->altitude_cm);
        
        return true;
    } else {
        g_telemetry_failed_count++;
        ESP_LOGW(TAG, "遥测数据发送失败 [失败次数: %lu]", g_telemetry_failed_count);
        return false;
    }
}

bool tcp_client_hb_send_heartbeat_now(void) {
    return tcp_heartbeat_manager_send_heartbeat_now();
}

bool tcp_client_hb_reconnect_now(void) {
    return tcp_heartbeat_manager_reconnect_now();
}

void tcp_client_hb_set_auto_reconnect(bool enabled) {
    tcp_heartbeat_manager_set_auto_reconnect(enabled);
}

bool tcp_client_hb_is_connection_healthy(void) {
    return tcp_heartbeat_manager_is_connection_healthy();
}

void tcp_client_hb_reset_stats(void) {
    ESP_LOGI(TAG, "重置客户端统计信息");
    
    // 重置心跳统计
    tcp_heartbeat_manager_reset_stats();
    
    // 重置遥测统计
    g_telemetry_sent_count = 0;
    g_telemetry_failed_count = 0;
    g_last_telemetry_time = 0;
    
    // 更新统计信息
    tcp_client_hb_update_stats();
}

void tcp_client_hb_print_status(void) {
    tcp_client_hb_update_stats();
    
    ESP_LOGI(TAG, "=== 带心跳的TCP客户端状态 ===");
    ESP_LOGI(TAG, "客户端状态: %d", g_client_stats.client_status);
    ESP_LOGI(TAG, "心跳状态: %d", g_client_stats.heartbeat_state);
    ESP_LOGI(TAG, "原客户端状态: %d", g_client_stats.legacy_client_state);
    ESP_LOGI(TAG, "遥测发送成功: %lu", g_client_stats.telemetry_sent_count);
    ESP_LOGI(TAG, "遥测发送失败: %lu", g_client_stats.telemetry_failed_count);
    ESP_LOGI(TAG, "最后遥测时间: %llu ms", g_client_stats.last_telemetry_time);
    ESP_LOGI(TAG, "连接健康: %s", tcp_client_hb_is_connection_healthy() ? "是" : "否");
    ESP_LOGI(TAG, "==============================");
    
    // 打印心跳管理器状态
    tcp_heartbeat_manager_print_status();
}

bool tcp_client_hb_start_task(const char *task_name, uint32_t stack_size, uint8_t priority) {
    if (g_client_task_handle != NULL) {
        ESP_LOGW(TAG, "客户端任务已在运行");
        return true;
    }
    
    const char *name = task_name ? task_name : "TCPClientHBTask";
    uint32_t stack = (stack_size > 0) ? stack_size : 4096;
    uint8_t prio = (priority > 0) ? priority : 5;
    
    ESP_LOGI(TAG, "创建客户端任务: %s (堆栈: %lu, 优先级: %d)", name, stack, prio);
    
    g_task_running = true;
    
    BaseType_t result = xTaskCreate(
        tcp_client_hb_task,
        name,
        stack,
        NULL,
        prio,
        &g_client_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "创建客户端任务失败");
        g_task_running = false;
        return false;
    }
    
    ESP_LOGI(TAG, "客户端任务创建成功");
    return true;
}

void tcp_client_hb_stop_task(void) {
    if (g_client_task_handle == NULL) {
        ESP_LOGW(TAG, "客户端任务未运行");
        return;
    }
    
    ESP_LOGI(TAG, "停止客户端任务...");
    
    g_task_running = false;
    
    // 等待任务结束
    while (g_client_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "客户端任务已停止");
}