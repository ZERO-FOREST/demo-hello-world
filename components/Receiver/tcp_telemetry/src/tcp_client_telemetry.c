/**
 * @file tcp_client_telemetry.c
 * @brief TCP遥测客户端实现
 * @author TidyCraze
 * @date 2025-09-05
 */
#include "tcp_client_telemetry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

static const char *TAG = "TCP_CLIENT_TELEMETRY";

// ----------------- 遥测客户端结构体 -----------------
typedef struct {
    tcp_client_telemetry_config_t config;    // 配置信息
    tcp_client_telemetry_state_t state;      // 当前状态
    tcp_client_telemetry_stats_t stats;      // 统计信息
    int socket_fd;                           // TCP套接字文件描述符
    TaskHandle_t telemetry_task_handle;      // 遥测任务句柄
    bool is_initialized;                     // 是否已初始化
    bool is_running;                         // 是否正在运行
    uint8_t recv_buffer[TCP_CLIENT_TELEMETRY_RECV_BUFFER_SIZE]; // 接收缓冲区
    uint8_t frame_buffer[TCP_CLIENT_TELEMETRY_FRAME_BUFFER_SIZE]; // 帧缓冲区
} tcp_client_telemetry_manager_t;

// ----------------- 全局变量 -----------------
static tcp_client_telemetry_manager_t g_telemetry_client = {0};
static bool g_telemetry_client_initialized = false;

// 模拟遥测数据
static tcp_client_telemetry_sim_data_t g_sim_telemetry = {
    .voltage_mv = 3850,   // 3.85V
    .current_ma = 150,    // 150mA
    .roll_deg = 5,        // 0.05 deg
    .pitch_deg = -10,     // -0.10 deg
    .yaw_deg = 2500,      // 25.00 deg
    .altitude_cm = 1000,  // 10m
};

// ----------------- 内部函数声明 -----------------
static bool tcp_client_telemetry_connect_internal(void);
static void tcp_client_telemetry_disconnect_internal(void);
static void tcp_client_telemetry_set_state(tcp_client_telemetry_state_t new_state);
static uint64_t tcp_client_telemetry_get_timestamp_ms(void);
static bool tcp_client_telemetry_is_socket_valid(void);
static void tcp_client_telemetry_update_stats_on_connect(void);
static void tcp_client_telemetry_update_stats_on_disconnect(void);
static int tcp_client_telemetry_find_frame_header(const uint8_t *buffer, int buffer_len);
static void tcp_client_telemetry_task_function(void *pvParameters);

// ----------------- 内部函数实现 -----------------

static uint64_t tcp_client_telemetry_get_timestamp_ms(void) {
    return (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void tcp_client_telemetry_set_state(tcp_client_telemetry_state_t new_state) {
    if (g_telemetry_client.state != new_state) {
        tcp_client_telemetry_state_t old_state = g_telemetry_client.state;
        g_telemetry_client.state = new_state;
        
        ESP_LOGI(TAG, "状态变更: %d -> %d", old_state, new_state);
        
        // 状态变更时的特殊处理
        if (new_state == TCP_CLIENT_TELEMETRY_STATE_CONNECTED) {
            tcp_client_telemetry_update_stats_on_connect();
        } else if (old_state == TCP_CLIENT_TELEMETRY_STATE_CONNECTED) {
            tcp_client_telemetry_update_stats_on_disconnect();
        }
    }
}

static bool tcp_client_telemetry_is_socket_valid(void) {
    return (g_telemetry_client.socket_fd >= 0);
}

static void tcp_client_telemetry_update_stats_on_connect(void) {
    g_telemetry_client.stats.connection_count++;
    g_telemetry_client.stats.connection_start_time = tcp_client_telemetry_get_timestamp_ms();
    ESP_LOGI(TAG, "连接建立，连接次数: %lu", g_telemetry_client.stats.connection_count);
}

static void tcp_client_telemetry_update_stats_on_disconnect(void) {
    uint64_t current_time = tcp_client_telemetry_get_timestamp_ms();
    if (g_telemetry_client.stats.connection_start_time > 0) {
        uint64_t session_time = current_time - g_telemetry_client.stats.connection_start_time;
        g_telemetry_client.stats.total_connected_time += session_time;
        ESP_LOGI(TAG, "连接断开，本次连接时长: %llu ms，总连接时长: %llu ms", 
                session_time, g_telemetry_client.stats.total_connected_time);
    }
}

static int tcp_client_telemetry_find_frame_header(const uint8_t *buffer, int buffer_len) {
    for (int i = 0; i < buffer_len - 1; i++) {
        if (buffer[i] == 0xAA && buffer[i + 1] == 0x55) {
            return i;
        }
    }
    return -1;
}

static bool tcp_client_telemetry_connect_internal(void) {
    if (tcp_client_telemetry_is_socket_valid()) {
        ESP_LOGW(TAG, "套接字已连接");
        return true;
    }

    tcp_client_telemetry_set_state(TCP_CLIENT_TELEMETRY_STATE_CONNECTING);
    ESP_LOGI(TAG, "正在连接到 %s:%d", g_telemetry_client.config.server_ip, g_telemetry_client.config.server_port);

    // 创建套接字
    g_telemetry_client.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_telemetry_client.socket_fd < 0) {
        ESP_LOGE(TAG, "创建套接字失败: %s", strerror(errno));
        tcp_client_telemetry_set_state(TCP_CLIENT_TELEMETRY_STATE_ERROR);
        return false;
    }

    // 设置套接字选项
    struct timeval timeout;
    timeout.tv_sec = g_telemetry_client.config.send_timeout_ms / 1000;
    timeout.tv_usec = (g_telemetry_client.config.send_timeout_ms % 1000) * 1000;
    setsockopt(g_telemetry_client.socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    timeout.tv_sec = g_telemetry_client.config.recv_timeout_ms / 1000;
    timeout.tv_usec = (g_telemetry_client.config.recv_timeout_ms % 1000) * 1000;
    setsockopt(g_telemetry_client.socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // 配置服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_telemetry_client.config.server_port);
    
    if (inet_pton(AF_INET, g_telemetry_client.config.server_ip, &server_addr.sin_addr) <= 0) {
        ESP_LOGE(TAG, "无效的IP地址: %s", g_telemetry_client.config.server_ip);
        close(g_telemetry_client.socket_fd);
        g_telemetry_client.socket_fd = -1;
        tcp_client_telemetry_set_state(TCP_CLIENT_TELEMETRY_STATE_ERROR);
        return false;
    }

    // 设置连接超时（使用非阻塞模式）
    int flags = fcntl(g_telemetry_client.socket_fd, F_GETFL, 0);
    fcntl(g_telemetry_client.socket_fd, F_SETFL, flags | O_NONBLOCK);
    
    // 连接到服务器
    int connect_result = connect(g_telemetry_client.socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (connect_result < 0) {
        if (errno == EINPROGRESS) {
            // 非阻塞连接正在进行中，使用select等待完成
            fd_set write_fds;
            struct timeval timeout_val;
            timeout_val.tv_sec = 5;  // 5秒连接超时
            timeout_val.tv_usec = 0;
            
            FD_ZERO(&write_fds);
            FD_SET(g_telemetry_client.socket_fd, &write_fds);
            
            int select_result = select(g_telemetry_client.socket_fd + 1, NULL, &write_fds, NULL, &timeout_val);
            if (select_result > 0) {
                // 检查连接是否成功
                int error = 0;
                socklen_t len = sizeof(error);
                if (getsockopt(g_telemetry_client.socket_fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
                    ESP_LOGE(TAG, "连接失败: %s", strerror(error ? error : errno));
                    close(g_telemetry_client.socket_fd);
                    g_telemetry_client.socket_fd = -1;
                    tcp_client_telemetry_set_state(TCP_CLIENT_TELEMETRY_STATE_ERROR);
                    return false;
                }
            } else if (select_result == 0) {
                ESP_LOGE(TAG, "连接超时");
                close(g_telemetry_client.socket_fd);
                g_telemetry_client.socket_fd = -1;
                tcp_client_telemetry_set_state(TCP_CLIENT_TELEMETRY_STATE_ERROR);
                return false;
            } else {
                ESP_LOGE(TAG, "select失败: %s", strerror(errno));
                close(g_telemetry_client.socket_fd);
                g_telemetry_client.socket_fd = -1;
                tcp_client_telemetry_set_state(TCP_CLIENT_TELEMETRY_STATE_ERROR);
                return false;
            }
        } else {
            ESP_LOGE(TAG, "连接失败: %s", strerror(errno));
            close(g_telemetry_client.socket_fd);
            g_telemetry_client.socket_fd = -1;
            tcp_client_telemetry_set_state(TCP_CLIENT_TELEMETRY_STATE_ERROR);
            return false;
        }
    }
    
    // 恢复阻塞模式
    fcntl(g_telemetry_client.socket_fd, F_SETFL, flags);

    tcp_client_telemetry_set_state(TCP_CLIENT_TELEMETRY_STATE_CONNECTED);
    ESP_LOGI(TAG, "连接成功");
    
    return true;
}

static void tcp_client_telemetry_disconnect_internal(void) {
    if (tcp_client_telemetry_is_socket_valid()) {
        close(g_telemetry_client.socket_fd);
        g_telemetry_client.socket_fd = -1;
        ESP_LOGI(TAG, "连接已断开");
    }
    tcp_client_telemetry_set_state(TCP_CLIENT_TELEMETRY_STATE_DISCONNECTED);
}

static void tcp_client_telemetry_task_function(void *pvParameters) {
    (void)pvParameters;
    
    ESP_LOGI(TAG, "遥测任务启动");
    
    while (g_telemetry_client.is_running) {
        // 如果未连接，尝试连接
        if (g_telemetry_client.state == TCP_CLIENT_TELEMETRY_STATE_DISCONNECTED) {
            if (g_telemetry_client.config.auto_reconnect_enabled) {
                if (tcp_client_telemetry_connect_internal()) {
                    ESP_LOGI(TAG, "重连成功");
                } else {
                    ESP_LOGW(TAG, "重连失败，等待重试");
                    vTaskDelay(pdMS_TO_TICKS(g_telemetry_client.config.reconnect_delay_ms));
                    g_telemetry_client.stats.reconnection_count++;
                    continue;
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
        }
        
        // 处理接收数据
        if (g_telemetry_client.state == TCP_CLIENT_TELEMETRY_STATE_CONNECTED) {
            if (!tcp_client_telemetry_process_received_data()) {
                ESP_LOGW(TAG, "数据处理失败，连接可能断开");
                tcp_client_telemetry_disconnect_internal();
                continue;
            }
        }
        
        // 发送模拟遥测数据（示例）
        if (g_telemetry_client.state == TCP_CLIENT_TELEMETRY_STATE_CONNECTED) {
            // 更新模拟数据
            tcp_client_telemetry_update_sim_data(&g_sim_telemetry);
            
            // 创建遥测数据包
            telemetry_data_payload_t telemetry_data;
            telemetry_data.voltage_mv = g_sim_telemetry.voltage_mv;
            telemetry_data.current_ma = g_sim_telemetry.current_ma;
            telemetry_data.roll_deg = g_sim_telemetry.roll_deg;
            telemetry_data.pitch_deg = g_sim_telemetry.pitch_deg;
            telemetry_data.yaw_deg = g_sim_telemetry.yaw_deg;
            telemetry_data.altitude_cm = g_sim_telemetry.altitude_cm;
            
            // 发送遥测数据
            tcp_client_telemetry_send_data(&telemetry_data);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1秒发送一次遥测数据
    }
    
    ESP_LOGI(TAG, "遥测任务结束");
    vTaskDelete(NULL);
}

// ----------------- 公共接口实现 -----------------

bool tcp_client_telemetry_init(const char *server_ip, uint16_t server_port) {
    if (g_telemetry_client_initialized) {
        ESP_LOGW(TAG, "遥测客户端已初始化");
        return true;
    }

    if (!server_ip) {
        ESP_LOGE(TAG, "服务器IP地址不能为空");
        return false;
    }

    // 初始化配置
    memset(&g_telemetry_client, 0, sizeof(g_telemetry_client));
    strncpy(g_telemetry_client.config.server_ip, server_ip, sizeof(g_telemetry_client.config.server_ip) - 1);
    g_telemetry_client.config.server_port = (server_port > 0) ? server_port : TCP_CLIENT_TELEMETRY_DEFAULT_PORT;
    g_telemetry_client.config.reconnect_delay_ms = TCP_CLIENT_TELEMETRY_RECONNECT_DELAY_MS;
    g_telemetry_client.config.send_timeout_ms = TCP_CLIENT_TELEMETRY_SEND_TIMEOUT_MS;
    g_telemetry_client.config.recv_timeout_ms = TCP_CLIENT_TELEMETRY_RECV_TIMEOUT_MS;
    g_telemetry_client.config.auto_reconnect_enabled = true;
    
    g_telemetry_client.socket_fd = -1;
    g_telemetry_client.state = TCP_CLIENT_TELEMETRY_STATE_DISCONNECTED;
    g_telemetry_client.is_initialized = true;
    g_telemetry_client.is_running = false;

    g_telemetry_client_initialized = true;
    ESP_LOGI(TAG, "遥测客户端初始化成功，服务器: %s:%d", server_ip, g_telemetry_client.config.server_port);
    
    return true;
}

bool tcp_client_telemetry_start(const char *task_name, uint32_t stack_size, UBaseType_t task_priority) {
    if (!g_telemetry_client_initialized) {
        ESP_LOGE(TAG, "遥测客户端未初始化");
        return false;
    }

    if (g_telemetry_client.is_running) {
        ESP_LOGW(TAG, "遥测客户端已在运行");
        return true;
    }

    // 设置默认参数
    const char *name = task_name ? task_name : "tcp_telemetry_task";
    uint32_t stack = (stack_size > 0) ? stack_size : 4096;
    UBaseType_t priority = task_priority;

    // 创建任务
    BaseType_t result = xTaskCreate(
        tcp_client_telemetry_task_function,
        name,
        stack,
        NULL,
        priority,
        &g_telemetry_client.telemetry_task_handle
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "创建遥测任务失败");
        return false;
    }

    g_telemetry_client.is_running = true;
    ESP_LOGI(TAG, "遥测客户端启动成功");
    return true;
}

void tcp_client_telemetry_stop(void) {
    if (!g_telemetry_client.is_running) {
        return;
    }

    g_telemetry_client.is_running = false;
    
    // 断开连接
    tcp_client_telemetry_disconnect_internal();
    
    // 等待任务结束
    if (g_telemetry_client.telemetry_task_handle) {
        // 任务会自己删除
        g_telemetry_client.telemetry_task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "遥测客户端已停止");
}

void tcp_client_telemetry_destroy(void) {
    tcp_client_telemetry_stop();
    
    memset(&g_telemetry_client, 0, sizeof(g_telemetry_client));
    g_telemetry_client.socket_fd = -1;
    g_telemetry_client_initialized = false;
    
    ESP_LOGI(TAG, "遥测客户端已销毁");
}

bool tcp_client_telemetry_connect(void) {
    if (!g_telemetry_client_initialized) {
        ESP_LOGE(TAG, "遥测客户端未初始化");
        return false;
    }
    
    return tcp_client_telemetry_connect_internal();
}

void tcp_client_telemetry_disconnect(void) {
    tcp_client_telemetry_disconnect_internal();
}

tcp_client_telemetry_state_t tcp_client_telemetry_get_state(void) {
    return g_telemetry_client.state;
}

bool tcp_client_telemetry_send_data(const telemetry_data_payload_t *telemetry_data) {
    if (!tcp_client_telemetry_is_socket_valid() || !telemetry_data) {
        ESP_LOGE(TAG, "套接字无效或数据为空");
        g_telemetry_client.stats.telemetry_failed_count++;
        return false;
    }

    if (g_telemetry_client.state != TCP_CLIENT_TELEMETRY_STATE_CONNECTED) {
        ESP_LOGW(TAG, "未连接，无法发送遥测数据");
        g_telemetry_client.stats.telemetry_failed_count++;
        return false;
    }

    // 创建协议帧
    protocol_frame_t frame;
    frame.header.sync_word = PROTOCOL_SYNC_WORD;
    frame.header.frame_type = FRAME_TYPE_TELEMETRY;
    frame.header.sequence_number = g_telemetry_client.stats.telemetry_sent_count + 1;
    frame.header.payload_length = sizeof(telemetry_data_payload_t);
    
    // 复制遥测数据到帧负载
    memcpy(frame.payload, telemetry_data, sizeof(telemetry_data_payload_t));
    
    // 计算CRC（简化版本，实际应该使用完整的CRC计算）
    frame.crc = 0x1234; // 占位符
    
    // 发送数据
    int total_size = sizeof(protocol_header_t) + frame.header.payload_length + sizeof(uint16_t);
    int sent_bytes = send(g_telemetry_client.socket_fd, &frame, total_size, 0);
    
    if (sent_bytes != total_size) {
        ESP_LOGE(TAG, "发送遥测数据失败: %s", strerror(errno));
        g_telemetry_client.stats.telemetry_failed_count++;
        return false;
    }

    g_telemetry_client.stats.telemetry_sent_count++;
    g_telemetry_client.stats.last_telemetry_time = tcp_client_telemetry_get_timestamp_ms();
    g_telemetry_client.stats.bytes_sent += sent_bytes;
    
    ESP_LOGD(TAG, "遥测数据发送成功，已发送: %lu", g_telemetry_client.stats.telemetry_sent_count);
    return true;
}

bool tcp_client_telemetry_process_received_data(void) {
    if (!tcp_client_telemetry_is_socket_valid()) {
        return false;
    }

    // 接收数据
    int received_bytes = recv(g_telemetry_client.socket_fd, g_telemetry_client.recv_buffer, 
                             TCP_CLIENT_TELEMETRY_RECV_BUFFER_SIZE - 1, MSG_DONTWAIT);
    
    if (received_bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 非阻塞模式下没有数据可读
            return true;
        }
        ESP_LOGE(TAG, "接收数据失败: %s", strerror(errno));
        return false;
    }
    
    if (received_bytes == 0) {
        ESP_LOGW(TAG, "连接被对方关闭");
        return false;
    }
    
    g_telemetry_client.stats.bytes_received += received_bytes;
    ESP_LOGD(TAG, "接收到 %d 字节数据", received_bytes);
    
    // 查找帧头
    int header_pos = tcp_client_telemetry_find_frame_header(g_telemetry_client.recv_buffer, received_bytes);
    if (header_pos >= 0) {
        // 找到帧头，尝试解析完整帧
        if (received_bytes - header_pos >= sizeof(protocol_header_t)) {
            protocol_frame_t *frame = (protocol_frame_t *)(g_telemetry_client.recv_buffer + header_pos);
            tcp_client_telemetry_print_received_frame(frame);
        }
    }
    
    return true;
}

const tcp_client_telemetry_stats_t* tcp_client_telemetry_get_stats(void) {
    return &g_telemetry_client.stats;
}

void tcp_client_telemetry_set_auto_reconnect(bool enabled) {
    g_telemetry_client.config.auto_reconnect_enabled = enabled;
}

bool tcp_client_telemetry_is_connection_healthy(void) {
    return (g_telemetry_client.state == TCP_CLIENT_TELEMETRY_STATE_CONNECTED && 
            tcp_client_telemetry_is_socket_valid());
}

void tcp_client_telemetry_reset_stats(void) {
    memset(&g_telemetry_client.stats, 0, sizeof(g_telemetry_client.stats));
}

void tcp_client_telemetry_print_status(void) {
    ESP_LOGI(TAG, "=== 遥测客户端状态 ===");
    ESP_LOGI(TAG, "状态: %d", g_telemetry_client.state);
    ESP_LOGI(TAG, "服务器: %s:%d", g_telemetry_client.config.server_ip, g_telemetry_client.config.server_port);
    ESP_LOGI(TAG, "已发送遥测: %lu", g_telemetry_client.stats.telemetry_sent_count);
    ESP_LOGI(TAG, "发送失败: %lu", g_telemetry_client.stats.telemetry_failed_count);
    ESP_LOGI(TAG, "连接次数: %lu", g_telemetry_client.stats.connection_count);
    ESP_LOGI(TAG, "重连次数: %lu", g_telemetry_client.stats.reconnection_count);
    ESP_LOGI(TAG, "总连接时长: %llu ms", g_telemetry_client.stats.total_connected_time);
    ESP_LOGI(TAG, "发送字节数: %lu", g_telemetry_client.stats.bytes_sent);
    ESP_LOGI(TAG, "接收字节数: %lu", g_telemetry_client.stats.bytes_received);
}

void tcp_client_telemetry_update_sim_data(tcp_client_telemetry_sim_data_t *sim_data) {
    if (!sim_data) {
        return;
    }
    
    // 模拟数据变化
    sim_data->voltage_mv += (rand() % 21 - 10);  // ±10mV变化
    if (sim_data->voltage_mv < 3000) sim_data->voltage_mv = 3000;
    if (sim_data->voltage_mv > 4200) sim_data->voltage_mv = 4200;
    
    sim_data->current_ma += (rand() % 11 - 5);   // ±5mA变化
    if (sim_data->current_ma < 50) sim_data->current_ma = 50;
    if (sim_data->current_ma > 500) sim_data->current_ma = 500;
    
    sim_data->roll_deg += (rand() % 21 - 10);    // ±1.0度变化
    sim_data->pitch_deg += (rand() % 21 - 10);
    sim_data->yaw_deg += (rand() % 101 - 50);    // ±5.0度变化
    
    sim_data->altitude_cm += (rand() % 201 - 100); // ±1m变化
    if (sim_data->altitude_cm < 0) sim_data->altitude_cm = 0;
}

void tcp_client_telemetry_print_received_frame(const protocol_frame_t *frame) {
    if (!frame) {
        return;
    }
    
    ESP_LOGI(TAG, "=== 接收到帧 ===");
    ESP_LOGI(TAG, "同步字: 0x%04X", frame->header.sync_word);
    ESP_LOGI(TAG, "帧类型: %d", frame->header.frame_type);
    ESP_LOGI(TAG, "序列号: %d", frame->header.sequence_number);
    ESP_LOGI(TAG, "负载长度: %d", frame->header.payload_length);
    ESP_LOGI(TAG, "CRC: 0x%04X", frame->crc);
    
    // 根据帧类型解析负载
    if (frame->header.frame_type == FRAME_TYPE_COMMAND && 
        frame->header.payload_length >= sizeof(command_payload_t)) {
        command_payload_t *cmd = (command_payload_t *)frame->payload;
        ESP_LOGI(TAG, "命令类型: %d, 参数: %d", cmd->command_type, cmd->parameter);
    }
}