/**
 * @file tcp_heartbeat_manager.c
 * @brief TCP心跳管理器实现
 * @author TidyCraze
 * @date 2025-09-04
 */
#include "tcp_heartbeat_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

static const char *TAG = "TCP_HEARTBEAT";

// ----------------- 全局变量 -----------------
static tcp_heartbeat_manager_t g_heartbeat_manager = {0};
static bool g_manager_initialized = false;

// ----------------- 内部函数声明 -----------------
static bool tcp_heartbeat_connect(void);
static void tcp_heartbeat_disconnect(void);
static bool tcp_heartbeat_send_packet(void);
static uint16_t tcp_heartbeat_create_packet(uint8_t *buffer, uint16_t buffer_size);
static void tcp_heartbeat_timer_callback(TimerHandle_t xTimer);
static void tcp_heartbeat_reconnect_timer_callback(TimerHandle_t xTimer);
static void tcp_heartbeat_task(void *pvParameters);
static void tcp_heartbeat_set_state(tcp_heartbeat_state_t new_state);
static uint64_t tcp_heartbeat_get_timestamp_ms(void);
static bool tcp_heartbeat_is_socket_valid(void);
static void tcp_heartbeat_update_stats_on_connect(void);
static void tcp_heartbeat_update_stats_on_disconnect(void);

// ----------------- CRC16计算函数 -----------------
static uint16_t calculate_crc16_modbus(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// ----------------- 内部函数实现 -----------------

static uint64_t tcp_heartbeat_get_timestamp_ms(void) {
    return (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void tcp_heartbeat_set_state(tcp_heartbeat_state_t new_state) {
    if (g_heartbeat_manager.state != new_state) {
        tcp_heartbeat_state_t old_state = g_heartbeat_manager.state;
        g_heartbeat_manager.state = new_state;
        
        ESP_LOGI(TAG, "状态变更: %d -> %d", old_state, new_state);
        
        // 状态变更时的特殊处理
        if (new_state == TCP_HB_STATE_CONNECTED) {
            tcp_heartbeat_update_stats_on_connect();
        } else if (old_state == TCP_HB_STATE_CONNECTED) {
            tcp_heartbeat_update_stats_on_disconnect();
        }
    }
}

static bool tcp_heartbeat_is_socket_valid(void) {
    return (g_heartbeat_manager.socket_fd >= 0);
}

static void tcp_heartbeat_update_stats_on_connect(void) {
    g_heartbeat_manager.stats.connection_count++;
    g_heartbeat_manager.stats.connection_start_time = tcp_heartbeat_get_timestamp_ms();
    ESP_LOGI(TAG, "连接建立，连接次数: %lu", g_heartbeat_manager.stats.connection_count);
}

static void tcp_heartbeat_update_stats_on_disconnect(void) {
    uint64_t current_time = tcp_heartbeat_get_timestamp_ms();
    if (g_heartbeat_manager.stats.connection_start_time > 0) {
        uint64_t session_time = current_time - g_heartbeat_manager.stats.connection_start_time;
        g_heartbeat_manager.stats.total_connected_time += session_time;
        ESP_LOGI(TAG, "连接断开，本次连接时长: %llu ms，总连接时长: %llu ms", 
                session_time, g_heartbeat_manager.stats.total_connected_time);
    }
}

static uint16_t tcp_heartbeat_create_packet(uint8_t *buffer, uint16_t buffer_size) {
    if (!buffer || buffer_size < 7) {
        ESP_LOGE(TAG, "缓冲区无效或太小");
        return 0;
    }
    
    // 按照协议文档格式创建心跳包
    // | 帧头 (2B) | 长度 (1B) | 类型 (1B) | 负载 (1B) | CRC16 (2B) |
    // AA 55 04 03 01 CRC
    
    uint8_t frame_data[5];
    
    // 帧头
    buffer[0] = 0xAA;
    buffer[1] = 0x55;
    
    // 长度：从类型到CRC16的总长度 = 1(类型) + 1(设备状态) + 2(CRC) = 4
    buffer[2] = 0x04;
    frame_data[0] = 0x04;
    
    // 类型：心跳包
    buffer[3] = FRAME_TYPE_HEARTBEAT;
    frame_data[1] = FRAME_TYPE_HEARTBEAT;
    
    // 负载：设备状态
    buffer[4] = g_heartbeat_manager.config.device_status;
    frame_data[2] = g_heartbeat_manager.config.device_status;
    
    // 计算CRC16（对长度+类型+负载进行校验）
    uint16_t crc = calculate_crc16_modbus(frame_data, 3);
    frame_data[3] = crc & 0xFF;         // CRC低字节
    frame_data[4] = (crc >> 8) & 0xFF;  // CRC高字节
    
    // 填入CRC
    buffer[5] = frame_data[3];
    buffer[6] = frame_data[4];
    
    ESP_LOGD(TAG, "心跳包创建: AA 55 %02X %02X %02X %02X %02X", 
            buffer[2], buffer[3], buffer[4], buffer[5], buffer[6]);
    
    return 7; // 心跳包总长度
}

static bool tcp_heartbeat_send_packet(void) {
    if (!tcp_heartbeat_is_socket_valid()) {
        ESP_LOGW(TAG, "套接字无效，无法发送心跳包");
        return false;
    }
    
    uint8_t heartbeat_buffer[16];
    uint16_t packet_len = tcp_heartbeat_create_packet(heartbeat_buffer, sizeof(heartbeat_buffer));
    
    if (packet_len == 0) {
        ESP_LOGE(TAG, "创建心跳包失败");
        g_heartbeat_manager.stats.heartbeat_failed_count++;
        return false;
    }
    
    // 发送心跳包
    int sent_bytes = send(g_heartbeat_manager.socket_fd, heartbeat_buffer, packet_len, 0);
    
    if (sent_bytes == packet_len) {
        g_heartbeat_manager.stats.heartbeat_sent_count++;
        g_heartbeat_manager.stats.last_heartbeat_time = tcp_heartbeat_get_timestamp_ms();
        
        ESP_LOGI(TAG, "已发送心跳 [%lu] - 时间戳: %llu, 设备状态: %d, 连接: %s:%d", 
                g_heartbeat_manager.stats.heartbeat_sent_count,
                g_heartbeat_manager.stats.last_heartbeat_time,
                g_heartbeat_manager.config.device_status,
                g_heartbeat_manager.config.server_ip,
                g_heartbeat_manager.config.server_port);
        
        return true;
    } else {
        g_heartbeat_manager.stats.heartbeat_failed_count++;
        ESP_LOGE(TAG, "发送心跳包失败: %s (发送了 %d/%d 字节)", strerror(errno), sent_bytes, packet_len);
        
        // 发送失败可能表示连接异常
        if (sent_bytes < 0) {
            ESP_LOGW(TAG, "检测到网络异常，准备重连");
            tcp_heartbeat_set_state(TCP_HB_STATE_RECONNECTING);
        }
        
        return false;
    }
}

static bool tcp_heartbeat_connect(void) {
    if (tcp_heartbeat_is_socket_valid()) {
        ESP_LOGW(TAG, "套接字已存在，先关闭旧连接");
        tcp_heartbeat_disconnect();
    }
    
    ESP_LOGI(TAG, "正在连接到服务器 %s:%d...", 
            g_heartbeat_manager.config.server_ip, 
            g_heartbeat_manager.config.server_port);
    
    tcp_heartbeat_set_state(TCP_HB_STATE_CONNECTING);
    
    // 创建套接字
    g_heartbeat_manager.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_heartbeat_manager.socket_fd < 0) {
        ESP_LOGE(TAG, "创建套接字失败: %s", strerror(errno));
        tcp_heartbeat_set_state(TCP_HB_STATE_ERROR);
        return false;
    }
    
    // 设置套接字选项
    struct timeval timeout;
    timeout.tv_sec = TCP_SEND_TIMEOUT_MS / 1000;
    timeout.tv_usec = (TCP_SEND_TIMEOUT_MS % 1000) * 1000;
    
    if (setsockopt(g_heartbeat_manager.socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGW(TAG, "设置发送超时失败: %s", strerror(errno));
    }
    
    timeout.tv_sec = TCP_RECV_TIMEOUT_MS / 1000;
    timeout.tv_usec = (TCP_RECV_TIMEOUT_MS % 1000) * 1000;
    
    if (setsockopt(g_heartbeat_manager.socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGW(TAG, "设置接收超时失败: %s", strerror(errno));
    }
    
    // 配置服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_heartbeat_manager.config.server_port);
    
    if (inet_pton(AF_INET, g_heartbeat_manager.config.server_ip, &server_addr.sin_addr) <= 0) {
        ESP_LOGE(TAG, "无效的IP地址: %s", g_heartbeat_manager.config.server_ip);
        close(g_heartbeat_manager.socket_fd);
        g_heartbeat_manager.socket_fd = -1;
        tcp_heartbeat_set_state(TCP_HB_STATE_ERROR);
        return false;
    }
    
    // 连接服务器
    if (connect(g_heartbeat_manager.socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "连接服务器失败: %s", strerror(errno));
        close(g_heartbeat_manager.socket_fd);
        g_heartbeat_manager.socket_fd = -1;
        tcp_heartbeat_set_state(TCP_HB_STATE_ERROR);
        return false;
    }
    
    ESP_LOGI(TAG, "成功连接到服务器 %s:%d", 
            g_heartbeat_manager.config.server_ip, 
            g_heartbeat_manager.config.server_port);
    
    tcp_heartbeat_set_state(TCP_HB_STATE_CONNECTED);
    
    // 启动心跳定时器
    if (g_heartbeat_manager.heartbeat_timer) {
        xTimerStart(g_heartbeat_manager.heartbeat_timer, 0);
        ESP_LOGI(TAG, "心跳定时器已启动，间隔: %lu ms", g_heartbeat_manager.config.heartbeat_interval_ms);
    }
    
    return true;
}

static void tcp_heartbeat_disconnect(void) {
    ESP_LOGI(TAG, "断开TCP连接");
    
    // 停止心跳定时器
    if (g_heartbeat_manager.heartbeat_timer) {
        xTimerStop(g_heartbeat_manager.heartbeat_timer, 0);
    }
    
    // 关闭套接字
    if (tcp_heartbeat_is_socket_valid()) {
        close(g_heartbeat_manager.socket_fd);
        g_heartbeat_manager.socket_fd = -1;
    }
    
    tcp_heartbeat_set_state(TCP_HB_STATE_DISCONNECTED);
}

static void tcp_heartbeat_timer_callback(TimerHandle_t xTimer) {
    (void)xTimer; // 避免未使用参数警告
    
    if (g_heartbeat_manager.state == TCP_HB_STATE_CONNECTED) {
        if (!tcp_heartbeat_send_packet()) {
            ESP_LOGW(TAG, "心跳发送失败，可能需要重连");
            
            // 如果启用了自动重连，启动重连流程
            if (g_heartbeat_manager.config.auto_reconnect_enabled) {
                tcp_heartbeat_set_state(TCP_HB_STATE_RECONNECTING);
                if (g_heartbeat_manager.reconnect_timer) {
                    xTimerStart(g_heartbeat_manager.reconnect_timer, 0);
                }
            }
        }
    } else {
        ESP_LOGD(TAG, "当前状态不是已连接，跳过心跳发送");
    }
}

static void tcp_heartbeat_reconnect_timer_callback(TimerHandle_t xTimer) {
    (void)xTimer; // 避免未使用参数警告
    
    if (g_heartbeat_manager.state == TCP_HB_STATE_RECONNECTING) {
        ESP_LOGI(TAG, "尝试重连... (第 %lu 次)", g_heartbeat_manager.stats.reconnection_count + 1);
        
        // 先断开现有连接
        tcp_heartbeat_disconnect();
        
        // 尝试重新连接
        if (tcp_heartbeat_connect()) {
            ESP_LOGI(TAG, "重连成功");
            // 停止重连定时器
            xTimerStop(g_heartbeat_manager.reconnect_timer, 0);
        } else {
            g_heartbeat_manager.stats.reconnection_count++;
            ESP_LOGW(TAG, "重连失败，将在 %lu ms 后重试", g_heartbeat_manager.config.reconnect_interval_ms);
            // 定时器会自动重复
        }
    }
}

static void tcp_heartbeat_task(void *pvParameters) {
    (void)pvParameters; // 避免未使用参数警告
    
    ESP_LOGI(TAG, "心跳管理任务启动");
    
    while (g_heartbeat_manager.is_running) {
        // 检查连接健康状态
        if (g_heartbeat_manager.state == TCP_HB_STATE_CONNECTED) {
            uint64_t current_time = tcp_heartbeat_get_timestamp_ms();
            uint64_t time_since_last_heartbeat = current_time - g_heartbeat_manager.stats.last_heartbeat_time;
            
            // 检查是否超过心跳超时时间
            if (time_since_last_heartbeat > g_heartbeat_manager.config.heartbeat_timeout_ms) {
                ESP_LOGW(TAG, "心跳超时检测到异常，距离上次心跳: %llu ms", time_since_last_heartbeat);
                
                if (g_heartbeat_manager.config.auto_reconnect_enabled) {
                    tcp_heartbeat_set_state(TCP_HB_STATE_RECONNECTING);
                    if (g_heartbeat_manager.reconnect_timer) {
                        xTimerStart(g_heartbeat_manager.reconnect_timer, 0);
                    }
                }
            }
        }
        
        // 任务休眠
        vTaskDelay(pdMS_TO_TICKS(1000)); // 每秒检查一次
    }
    
    ESP_LOGI(TAG, "心跳管理任务结束");
    vTaskDelete(NULL);
}

// ----------------- 公共函数实现 -----------------

bool tcp_heartbeat_manager_init(const char *server_ip, uint16_t server_port) {
    if (g_manager_initialized) {
        ESP_LOGW(TAG, "心跳管理器已初始化");
        return true;
    }
    
    if (!server_ip) {
        ESP_LOGE(TAG, "服务器IP地址不能为空");
        return false;
    }
    
    ESP_LOGI(TAG, "初始化TCP心跳管理器...");
    
    // 清零结构体
    memset(&g_heartbeat_manager, 0, sizeof(g_heartbeat_manager));
    
    // 配置初始化
    strncpy(g_heartbeat_manager.config.server_ip, server_ip, sizeof(g_heartbeat_manager.config.server_ip) - 1);
    g_heartbeat_manager.config.server_port = (server_port > 0) ? server_port : TCP_HEARTBEAT_SERVER_PORT;
    g_heartbeat_manager.config.heartbeat_interval_ms = TCP_HEARTBEAT_INTERVAL_MS;
    g_heartbeat_manager.config.reconnect_interval_ms = TCP_RECONNECT_INTERVAL_MS;
    g_heartbeat_manager.config.heartbeat_timeout_ms = TCP_HEARTBEAT_TIMEOUT_MS;
    g_heartbeat_manager.config.device_status = DEVICE_STATUS_IDLE;
    g_heartbeat_manager.config.auto_reconnect_enabled = true;
    
    // 状态初始化
    g_heartbeat_manager.state = TCP_HB_STATE_DISCONNECTED;
    g_heartbeat_manager.socket_fd = -1;
    g_heartbeat_manager.is_initialized = false;
    g_heartbeat_manager.is_running = false;
    
    // 创建心跳定时器
    g_heartbeat_manager.heartbeat_timer = xTimerCreate(
        "HeartbeatTimer",
        pdMS_TO_TICKS(g_heartbeat_manager.config.heartbeat_interval_ms),
        pdTRUE, // 自动重载
        NULL,
        tcp_heartbeat_timer_callback
    );
    
    if (!g_heartbeat_manager.heartbeat_timer) {
        ESP_LOGE(TAG, "创建心跳定时器失败");
        return false;
    }
    
    // 创建重连定时器
    g_heartbeat_manager.reconnect_timer = xTimerCreate(
        "ReconnectTimer",
        pdMS_TO_TICKS(g_heartbeat_manager.config.reconnect_interval_ms),
        pdTRUE, // 自动重载
        NULL,
        tcp_heartbeat_reconnect_timer_callback
    );
    
    if (!g_heartbeat_manager.reconnect_timer) {
        ESP_LOGE(TAG, "创建重连定时器失败");
        xTimerDelete(g_heartbeat_manager.heartbeat_timer, 0);
        return false;
    }
    
    g_heartbeat_manager.is_initialized = true;
    g_manager_initialized = true;
    
    ESP_LOGI(TAG, "TCP心跳管理器初始化成功 - 服务器: %s:%d, 心跳间隔: %lu ms", 
            g_heartbeat_manager.config.server_ip,
            g_heartbeat_manager.config.server_port,
            g_heartbeat_manager.config.heartbeat_interval_ms);
    
    return true;
}

bool tcp_heartbeat_manager_start(void) {
    if (!g_manager_initialized) {
        ESP_LOGE(TAG, "心跳管理器未初始化");
        return false;
    }
    
    if (g_heartbeat_manager.is_running) {
        ESP_LOGW(TAG, "心跳管理器已在运行");
        return true;
    }
    
    ESP_LOGI(TAG, "启动TCP心跳管理器...");
    
    g_heartbeat_manager.is_running = true;
    
    // 创建心跳管理任务
    BaseType_t result = xTaskCreate(
        tcp_heartbeat_task,
        "HeartbeatTask",
        4096,
        NULL,
        5, // 优先级
        &g_heartbeat_manager.heartbeat_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "创建心跳管理任务失败");
        g_heartbeat_manager.is_running = false;
        return false;
    }
    
    // 尝试初始连接
    if (!tcp_heartbeat_connect()) {
        ESP_LOGW(TAG, "初始连接失败，将启动自动重连");
        if (g_heartbeat_manager.config.auto_reconnect_enabled) {
            tcp_heartbeat_set_state(TCP_HB_STATE_RECONNECTING);
            xTimerStart(g_heartbeat_manager.reconnect_timer, 0);
        }
    }
    
    ESP_LOGI(TAG, "TCP心跳管理器启动成功");
    return true;
}

void tcp_heartbeat_manager_stop(void) {
    if (!g_manager_initialized || !g_heartbeat_manager.is_running) {
        ESP_LOGW(TAG, "心跳管理器未运行");
        return;
    }
    
    ESP_LOGI(TAG, "停止TCP心跳管理器...");
    
    g_heartbeat_manager.is_running = false;
    
    // 停止定时器
    if (g_heartbeat_manager.heartbeat_timer) {
        xTimerStop(g_heartbeat_manager.heartbeat_timer, portMAX_DELAY);
    }
    
    if (g_heartbeat_manager.reconnect_timer) {
        xTimerStop(g_heartbeat_manager.reconnect_timer, portMAX_DELAY);
    }
    
    // 断开连接
    tcp_heartbeat_disconnect();
    
    // 等待任务结束
    if (g_heartbeat_manager.heartbeat_task_handle) {
        // 任务会自己删除
        g_heartbeat_manager.heartbeat_task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "TCP心跳管理器已停止");
}

void tcp_heartbeat_manager_destroy(void) {
    if (!g_manager_initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "销毁TCP心跳管理器...");
    
    // 先停止
    tcp_heartbeat_manager_stop();
    
    // 删除定时器
    if (g_heartbeat_manager.heartbeat_timer) {
        xTimerDelete(g_heartbeat_manager.heartbeat_timer, portMAX_DELAY);
        g_heartbeat_manager.heartbeat_timer = NULL;
    }
    
    if (g_heartbeat_manager.reconnect_timer) {
        xTimerDelete(g_heartbeat_manager.reconnect_timer, portMAX_DELAY);
        g_heartbeat_manager.reconnect_timer = NULL;
    }
    
    // 清零结构体
    memset(&g_heartbeat_manager, 0, sizeof(g_heartbeat_manager));
    g_heartbeat_manager.socket_fd = -1;
    
    g_manager_initialized = false;
    
    ESP_LOGI(TAG, "TCP心跳管理器已销毁");
}

tcp_heartbeat_state_t tcp_heartbeat_manager_get_state(void) {
    return g_heartbeat_manager.state;
}

const tcp_heartbeat_stats_t* tcp_heartbeat_manager_get_stats(void) {
    return &g_heartbeat_manager.stats;
}

void tcp_heartbeat_manager_set_device_status(uint8_t status) {
    if (status > DEVICE_STATUS_ERROR) {
        ESP_LOGW(TAG, "无效的设备状态: %d", status);
        return;
    }
    
    if (g_heartbeat_manager.config.device_status != status) {
        ESP_LOGI(TAG, "设备状态变更: %d -> %d", g_heartbeat_manager.config.device_status, status);
        g_heartbeat_manager.config.device_status = status;
    }
}

bool tcp_heartbeat_manager_send_heartbeat_now(void) {
    if (!g_manager_initialized) {
        ESP_LOGE(TAG, "心跳管理器未初始化");
        return false;
    }
    
    if (g_heartbeat_manager.state != TCP_HB_STATE_CONNECTED) {
        ESP_LOGW(TAG, "当前未连接，无法发送心跳");
        return false;
    }
    
    return tcp_heartbeat_send_packet();
}

bool tcp_heartbeat_manager_reconnect_now(void) {
    if (!g_manager_initialized) {
        ESP_LOGE(TAG, "心跳管理器未初始化");
        return false;
    }
    
    ESP_LOGI(TAG, "手动触发重连");
    
    // 先断开
    tcp_heartbeat_disconnect();
    
    // 重新连接
    return tcp_heartbeat_connect();
}

void tcp_heartbeat_manager_set_auto_reconnect(bool enabled) {
    g_heartbeat_manager.config.auto_reconnect_enabled = enabled;
    ESP_LOGI(TAG, "自动重连 %s", enabled ? "已启用" : "已禁用");
}

bool tcp_heartbeat_manager_is_connection_healthy(void) {
    if (g_heartbeat_manager.state != TCP_HB_STATE_CONNECTED) {
        return false;
    }
    
    uint64_t current_time = tcp_heartbeat_get_timestamp_ms();
    uint64_t time_since_last_heartbeat = current_time - g_heartbeat_manager.stats.last_heartbeat_time;
    
    return (time_since_last_heartbeat <= g_heartbeat_manager.config.heartbeat_timeout_ms);
}

void tcp_heartbeat_manager_reset_stats(void) {
    ESP_LOGI(TAG, "重置统计信息");
    memset(&g_heartbeat_manager.stats, 0, sizeof(g_heartbeat_manager.stats));
}

void tcp_heartbeat_manager_print_status(void) {
    ESP_LOGI(TAG, "=== TCP心跳管理器状态 ===");
    ESP_LOGI(TAG, "服务器: %s:%d", g_heartbeat_manager.config.server_ip, g_heartbeat_manager.config.server_port);
    ESP_LOGI(TAG, "当前状态: %d", g_heartbeat_manager.state);
    ESP_LOGI(TAG, "设备状态: %d", g_heartbeat_manager.config.device_status);
    ESP_LOGI(TAG, "自动重连: %s", g_heartbeat_manager.config.auto_reconnect_enabled ? "启用" : "禁用");
    ESP_LOGI(TAG, "心跳间隔: %lu ms", g_heartbeat_manager.config.heartbeat_interval_ms);
    ESP_LOGI(TAG, "已发送心跳: %lu", g_heartbeat_manager.stats.heartbeat_sent_count);
    ESP_LOGI(TAG, "发送失败: %lu", g_heartbeat_manager.stats.heartbeat_failed_count);
    ESP_LOGI(TAG, "连接次数: %lu", g_heartbeat_manager.stats.connection_count);
    ESP_LOGI(TAG, "重连次数: %lu", g_heartbeat_manager.stats.reconnection_count);
    ESP_LOGI(TAG, "最后心跳时间: %llu ms", g_heartbeat_manager.stats.last_heartbeat_time);
    ESP_LOGI(TAG, "总连接时长: %llu ms", g_heartbeat_manager.stats.total_connected_time);
    ESP_LOGI(TAG, "连接健康: %s", tcp_heartbeat_manager_is_connection_healthy() ? "是" : "否");
    ESP_LOGI(TAG, "========================");
}