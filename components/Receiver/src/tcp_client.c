#include "tcp_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

// static const char *TAG = "tcp_client"; // Unused

// ----------------- WiFi事件处理 -----------------
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define ESP32_IP "192.168.97.247"
#define ESP32_PORT 6667

// ----------------- 全局变量 -----------------
static int client_socket = -1;
static client_state_t client_state = CLIENT_STATE_DISCONNECTED;
static uint8_t recv_buffer[RECV_BUFFER_SIZE];
static uint8_t frame_buffer[FRAME_BUFFER_SIZE];
static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

// 模拟遥测数据
static simulated_telemetry_t sim_telemetry = {
    .voltage_mv = 3850,   // 3.85V
    .current_ma = 150,    // 150mA
    .roll_deg = 5,        // 0.05 deg
    .pitch_deg = -10,     // -0.10 deg
    .yaw_deg = 2500,      // 25.00 deg
    .altitude_cm = 1000,  // 10m
};

// ----------------- 内部函数 -----------------

static void set_client_state(client_state_t new_state) {
    pthread_mutex_lock(&state_mutex);
    client_state = new_state;
    pthread_mutex_unlock(&state_mutex);
}

static int find_frame_header(const uint8_t *buffer, int buffer_len) {
    for (int i = 0; i < buffer_len - 1; i++) {
        if (buffer[i] == 0xAA && buffer[i + 1] == 0x55) {
            return i;
        }
    }
    return -1;
}

// ----------------- 公共函数实现 -----------------

bool tcp_client_init(void) {
    printf("TCP客户端初始化\n");
    set_client_state(CLIENT_STATE_DISCONNECTED);
    return true;
}

bool tcp_client_connect(void) {
    if (client_state == CLIENT_STATE_CONNECTED) {
        return true;
    }
    
    printf("正在连接到 ESP32 (%s:%d)...\n", ESP32_IP, ESP32_PORT);
    set_client_state(CLIENT_STATE_CONNECTING);
    
    // 创建socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        printf("创建socket失败: %s\n", strerror(errno));
        set_client_state(CLIENT_STATE_ERROR);
        return false;
    }
    
    // 设置服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ESP32_PORT);
    
    if (inet_pton(AF_INET, ESP32_IP, &server_addr.sin_addr) <= 0) {
        printf("无效的IP地址: %s\n", ESP32_IP);
        close(client_socket);
        client_socket = -1;
        set_client_state(CLIENT_STATE_ERROR);
        return false;
    }
    
    // 连接服务器
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("连接失败: %s\n", strerror(errno));
        close(client_socket);
        client_socket = -1;
        set_client_state(CLIENT_STATE_DISCONNECTED);
        return false;
    }
    
    printf("连接成功!\n");
    set_client_state(CLIENT_STATE_CONNECTED);
    return true;
}

void tcp_client_disconnect(void) {
    if (client_socket >= 0) {
        close(client_socket);
        client_socket = -1;
    }
    set_client_state(CLIENT_STATE_DISCONNECTED);
    printf("连接已断开\n");
}

client_state_t tcp_client_get_state(void) {
    client_state_t state;
    pthread_mutex_lock(&state_mutex);
    state = client_state;
    pthread_mutex_unlock(&state_mutex);
    return state;
}

bool tcp_client_send_telemetry(const telemetry_data_payload_t *telemetry_data) {
    if (client_state != CLIENT_STATE_CONNECTED || client_socket < 0) {
        return false;
    }
    
    uint16_t frame_len = create_telemetry_frame(telemetry_data, frame_buffer, FRAME_BUFFER_SIZE);
    if (frame_len == 0) {
        printf("创建遥测帧失败\n");
        return false;
    }
    
    ssize_t sent = send(client_socket, frame_buffer, frame_len, 0);
    if (sent != frame_len) {
        printf("发送遥测数据失败: %s\n", strerror(errno));
        set_client_state(CLIENT_STATE_ERROR);
        return false;
    }
    
    printf("--> 发送遥测数据: ");
    for (int i = 0; i < frame_len; i++) {
        printf("%02x", frame_buffer[i]);
    }
    printf("\n");
    
    return true;
}

bool tcp_client_process_received_data(void) {
    if (client_state != CLIENT_STATE_CONNECTED || client_socket < 0) {
        return false;
    }
    
    static uint8_t data_buffer[RECV_BUFFER_SIZE];
    static int buffer_pos = 0;
    
    // 接收数据
    ssize_t received = recv(client_socket, recv_buffer, RECV_BUFFER_SIZE, MSG_DONTWAIT);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true; // 没有数据可读，继续
        }
        printf("接收数据失败: %s\n", strerror(errno));
        set_client_state(CLIENT_STATE_ERROR);
        return false;
    } else if (received == 0) {
        printf("服务器关闭了连接\n");
        set_client_state(CLIENT_STATE_DISCONNECTED);
        return false;
    }
    
    printf("<-- 收到原始数据: ");
    for (int i = 0; i < received; i++) {
        printf("%02x", recv_buffer[i]);
    }
    printf("\n");
    
    // 将新数据添加到缓冲区
    if (buffer_pos + received < RECV_BUFFER_SIZE) {
        memcpy(&data_buffer[buffer_pos], recv_buffer, received);
        buffer_pos += received;
    } else {
        // 缓冲区溢出，重置
        printf("接收缓冲区溢出，重置缓冲区\n");
        memcpy(data_buffer, recv_buffer, received);
        buffer_pos = received;
    }
    
    // 处理缓冲区中的数据帧
    while (buffer_pos >= MIN_FRAME_SIZE) {
        // 寻找帧头
        int header_pos = find_frame_header(data_buffer, buffer_pos);
        if (header_pos == -1) {
            // 没找到帧头，清空缓冲区
            buffer_pos = 0;
            break;
        }
        
        if (header_pos > 0) {
            // 丢弃帧头前的无效数据
            memmove(data_buffer, &data_buffer[header_pos], buffer_pos - header_pos);
            buffer_pos -= header_pos;
        }
        
        if (buffer_pos < 3) {
            // 数据不足以读取长度字段
            break;
        }
        
        // 获取帧长度
        uint8_t frame_length_field = data_buffer[2];
        uint16_t total_frame_size = 2 + 1 + frame_length_field + 2; // 帧头+长度+内容+CRC
        
        if (buffer_pos < total_frame_size) {
            // 数据不完整，等待更多数据
            printf("数据不完整, 需要%d字节, 现有%d字节\n", total_frame_size, buffer_pos);
            break;
        }
        
        // 解析数据帧
        protocol_frame_t frame;
        parse_result_t result = parse_protocol_frame(data_buffer, total_frame_size, &frame);
        
        if (result == PARSE_SUCCESS) {
            print_received_frame(&frame);
            
            // 根据帧类型处理数据
            switch (frame.frame_type) {
                case FRAME_TYPE_REMOTE_CONTROL:
                    handle_remote_control_data(&frame.payload.remote_control);
                    break;
                case FRAME_TYPE_HEARTBEAT:
                    handle_heartbeat_data(&frame.payload.heartbeat);
                    break;
                case FRAME_TYPE_EXTENDED_CMD:
                    handle_extended_command(&frame.payload.extended_cmd);
                    break;
                default:
                    printf("收到未知类型的帧: 0x%02X\n", frame.frame_type);
                    break;
            }
        } else {
            printf("解析帧失败: %d\n", result);
        }
        
        // 移除已处理的帧
        memmove(data_buffer, &data_buffer[total_frame_size], buffer_pos - total_frame_size);
        buffer_pos -= total_frame_size;
    }
    
    return true;
}

void update_simulated_telemetry(simulated_telemetry_t *sim_data) {
    // 模拟数据变化
    static int counter = 0;
    counter++;
    
    // 模拟电压在3.7V-4.0V之间变化
    sim_data->voltage_mv = 3700 + (counter % 300);
    
    // 模拟电流在100-200mA之间变化
    sim_data->current_ma = 100 + (counter % 100);
    
    // 模拟姿态角变化
    sim_data->roll_deg = (counter % 360) - 180;
    sim_data->pitch_deg = (counter % 180) - 90;
    sim_data->yaw_deg = counter % 3600;
    
    // 模拟高度变化
    sim_data->altitude_cm = 1000 + (counter % 500);
}

void print_received_frame(const protocol_frame_t *frame) {
    if (!frame) return;
    
    switch (frame->frame_type) {
        case FRAME_TYPE_REMOTE_CONTROL: {
            const remote_control_payload_t *rc = &frame->payload.remote_control;
            printf("收到遥控数据: 通道数=%d", rc->channel_count);
            for (uint8_t i = 0; i < rc->channel_count && i < MAX_CHANNELS; i++) {
                if (i == 0) printf(", 油门=%d", rc->channels[i]);
                else if (i == 1) printf(", 方向=%d", rc->channels[i]);
                else printf(", CH%d=%d", i+1, rc->channels[i]);
            }
            printf("\n");
            break;
        }
        
        case FRAME_TYPE_HEARTBEAT: {
            const heartbeat_payload_t *hb = &frame->payload.heartbeat;
            const char* status_map[] = {"空闲", "正常运行", "错误"};
            const char* status_str = (hb->device_status <= 2) ? status_map[hb->device_status] : "未知";
            printf("收到心跳: 设备状态=%s\n", status_str);
            break;
        }
        
        case FRAME_TYPE_EXTENDED_CMD: {
            const extended_cmd_payload_t *cmd = &frame->payload.extended_cmd;
            printf("收到扩展命令: ID=0x%02X, 参数长度=%d\n", cmd->cmd_id, cmd->param_len);
            break;
        }
        
        default:
            printf("收到未知类型帧: 0x%02X\n", frame->frame_type);
            break;
    }
}

void tcp_client_task(void) {
    printf("TCP客户端任务启动\n");
    
    if (!tcp_client_init()) {
        printf("TCP客户端初始化失败\n");
        return;
    }
    
    time_t last_telemetry_send = 0;
    
    while (1) {
        client_state_t current_state = tcp_client_get_state();
        
        switch (current_state) {
            case CLIENT_STATE_DISCONNECTED:
            case CLIENT_STATE_ERROR:
                if (!tcp_client_connect()) {
                    printf("连接失败，%d秒后重试...\n", RECONNECT_DELAY_MS / 1000);
                    usleep(RECONNECT_DELAY_MS * 1000);
                }
                break;
                
            case CLIENT_STATE_CONNECTED: {
                // 处理接收到的数据
                if (!tcp_client_process_received_data()) {
                    tcp_client_disconnect();
                    break;
                }
                
                // 每秒发送一次遥测数据
                time_t current_time = time(NULL);
                if (current_time - last_telemetry_send >= 1) {
                    update_simulated_telemetry(&sim_telemetry);
                    
                    telemetry_data_payload_t telemetry = {
                        .voltage_mv = sim_telemetry.voltage_mv,
                        .current_ma = sim_telemetry.current_ma,
                        .roll_deg = sim_telemetry.roll_deg,
                        .pitch_deg = sim_telemetry.pitch_deg,
                        .yaw_deg = sim_telemetry.yaw_deg,
                        .altitude_cm = sim_telemetry.altitude_cm
                    };
                    
                    if (!tcp_client_send_telemetry(&telemetry)) {
                        printf("发送遥测数据失败\n");
                        set_client_state(CLIENT_STATE_ERROR);
                    }
                    
                    last_telemetry_send = current_time;
                }
                
                usleep(100000); // 100ms
                break;
            }
            
            default:
                usleep(100000); // 100ms
                break;
        }
    }
}
