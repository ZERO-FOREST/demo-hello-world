#include "tcp_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include <string.h>
#include <errno.h>

static const char *TAG = "tcp_client";

// ----------------- WiFi事件处理 -----------------
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

// ----------------- 全局变量 -----------------
static int client_socket = -1;
static client_state_t client_state = CLIENT_STATE_DISCONNECTED;
static uint8_t recv_buffer[RECV_BUFFER_SIZE];
static uint8_t frame_buffer[FRAME_BUFFER_SIZE];

// 模拟遥测数据
static simulated_telemetry_t sim_telemetry = {
    .voltage_mv = 3850,   // 3.85V
    .current_ma = 150,    // 150mA
    .roll_deg = 5,        // 0.05 deg
    .pitch_deg = -10,     // -0.10 deg
    .yaw_deg = 2500,      // 25.00 deg
    .altitude_cm = 1000,  // 10m
};

// WiFi事件处理函数
static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// WiFi初始化
void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 WIFI_SSID, WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 WIFI_SSID, WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

// 内部函数
static int find_frame_header(const uint8_t *buffer, int buffer_len) {
    for (int i = 0; i < buffer_len - 1; i++) {
        if (buffer[i] == 0xAA && buffer[i + 1] == 0x55) {
            return i;
        }
    }
    return -1;
}

// 公共函数实现
bool tcp_client_init(void) {
    ESP_LOGI(TAG, "TCP客户端初始化");
    client_state = CLIENT_STATE_DISCONNECTED;
    
    // 初始化WiFi
    wifi_init_sta();
    
    return true;
}

bool tcp_client_connect(void) {
    if (client_state == CLIENT_STATE_CONNECTED) {
        return true;
    }
    
    ESP_LOGI(TAG, "正在连接到服务器 (%s:%d)...", ESP32_SERVER_IP, ESP32_SERVER_PORT);
    client_state = CLIENT_STATE_CONNECTING;
    
    // 创建socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        ESP_LOGE(TAG, "创建socket失败");
        client_state = CLIENT_STATE_ERROR;
        return false;
    }
    
    // 设置服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ESP32_SERVER_PORT);
    
    if (inet_pton(AF_INET, ESP32_SERVER_IP, &server_addr.sin_addr) <= 0) {
        ESP_LOGE(TAG, "无效的IP地址: %s", ESP32_SERVER_IP);
        close(client_socket);
        client_socket = -1;
        client_state = CLIENT_STATE_ERROR;
        return false;
    }
    
    // 连接服务器
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "连接失败");
        close(client_socket);
        client_socket = -1;
        client_state = CLIENT_STATE_DISCONNECTED;
        return false;
    }
    
    ESP_LOGI(TAG, "连接成功!");
    client_state = CLIENT_STATE_CONNECTED;
    return true;
}

void tcp_client_disconnect(void) {
    if (client_socket >= 0) {
        close(client_socket);
        client_socket = -1;
    }
    client_state = CLIENT_STATE_DISCONNECTED;
    ESP_LOGI(TAG, "连接已断开");
}

client_state_t tcp_client_get_state(void) {
    return client_state;
}

bool tcp_client_send_telemetry(const telemetry_data_payload_t *telemetry_data) {
    if (client_state != CLIENT_STATE_CONNECTED || client_socket < 0) {
        return false;
    }
    
    uint16_t frame_len = create_telemetry_frame(telemetry_data, frame_buffer, FRAME_BUFFER_SIZE);
    if (frame_len == 0) {
        ESP_LOGE(TAG, "创建遥测帧失败");
        return false;
    }
    
    int sent = send(client_socket, frame_buffer, frame_len, 0);
    if (sent != frame_len) {
        ESP_LOGE(TAG, "发送遥测数据失败");
        client_state = CLIENT_STATE_ERROR;
        return false;
    }
    
    char hex_str[512] = {0};
    for (int i = 0; i < frame_len; i++) {
        sprintf(hex_str + strlen(hex_str), "%02x", frame_buffer[i]);
    }
    ESP_LOGI(TAG, "--> 发送遥测数据: %s", hex_str);
    
    return true;
}

bool tcp_client_process_received_data(void) {
    if (client_state != CLIENT_STATE_CONNECTED || client_socket < 0) {
        return false;
    }
    
    static uint8_t data_buffer[RECV_BUFFER_SIZE];
    static int buffer_pos = 0;
    
    // 接收数据
    int received = recv(client_socket, recv_buffer, RECV_BUFFER_SIZE, MSG_DONTWAIT);
    if (received < 0) {
        int err = errno;
        if (err == EAGAIN || err == EWOULDBLOCK) {
            return true; // 没有数据可读，继续
        }
        ESP_LOGE(TAG, "接收数据失败");
        client_state = CLIENT_STATE_ERROR;
        return false;
    } else if (received == 0) {
        ESP_LOGI(TAG, "服务器关闭了连接");
        client_state = CLIENT_STATE_DISCONNECTED;
        return false;
    }
    
    char hex_str[2048] = {0};
    for (int i = 0; i < received; i++) {
        sprintf(hex_str + strlen(hex_str), "%02x", recv_buffer[i]);
    }
    ESP_LOGI(TAG, "<-- 收到原始数据: %s", hex_str);
    
    // 将新数据添加到缓冲区
    if (buffer_pos + received < RECV_BUFFER_SIZE) {
        memcpy(&data_buffer[buffer_pos], recv_buffer, received);
        buffer_pos += received;
    } else {
        // 缓冲区溢出，重置
        ESP_LOGW(TAG, "接收缓冲区溢出，重置缓冲区");
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
            ESP_LOGD(TAG, "数据不完整, 需要%d字节, 现有%d字节", total_frame_size, buffer_pos);
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
                    ESP_LOGW(TAG, "收到未知类型的帧: 0x%02X", frame.frame_type);
                    break;
            }
        } else {
            ESP_LOGE(TAG, "解析帧失败: %d", result);
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
            ESP_LOGI(TAG, "收到遥控数据: 通道数=%d", rc->channel_count);
            for (uint8_t i = 0; i < rc->channel_count && i < MAX_CHANNELS; i++) {
                if (i == 0) ESP_LOGI(TAG, "  油门=%d", rc->channels[i]);
                else if (i == 1) ESP_LOGI(TAG, "  方向=%d", rc->channels[i]);
                else ESP_LOGI(TAG, "  CH%d=%d", i+1, rc->channels[i]);
            }
            break;
        }
        
        case FRAME_TYPE_HEARTBEAT: {
            const heartbeat_payload_t *hb = &frame->payload.heartbeat;
            const char* status_map[] = {"空闲", "正常运行", "错误"};
            const char* status_str = (hb->device_status <= 2) ? status_map[hb->device_status] : "未知";
            ESP_LOGI(TAG, "收到心跳: 设备状态=%s", status_str);
            break;
        }
        
        case FRAME_TYPE_EXTENDED_CMD: {
            const extended_cmd_payload_t *cmd = &frame->payload.extended_cmd;
            ESP_LOGI(TAG, "收到扩展命令: ID=0x%02X, 参数长度=%d", cmd->cmd_id, cmd->param_len);
            break;
        }
        
        default:
            ESP_LOGW(TAG, "收到未知类型帧: 0x%02X", frame->frame_type);
            break;
    }
}

void tcp_client_task(void) {
    ESP_LOGI(TAG, "TCP客户端任务启动");
    
    if (!tcp_client_init()) {
        ESP_LOGE(TAG, "TCP客户端初始化失败");
        return;
    }
    
    TickType_t last_telemetry_send = 0;
    
    while (1) {
        client_state_t current_state = tcp_client_get_state();
        
        switch (current_state) {
            case CLIENT_STATE_DISCONNECTED:
            case CLIENT_STATE_ERROR:
                if (!tcp_client_connect()) {
                    ESP_LOGI(TAG, "连接失败，%d毫秒后重试...", RECONNECT_DELAY_MS);
                    vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
                }
                break;
                
            case CLIENT_STATE_CONNECTED: {
                // 处理接收到的数据
                if (!tcp_client_process_received_data()) {
                    tcp_client_disconnect();
                    break;
                }
                
                // 每秒发送一次遥测数据
                TickType_t current_time = xTaskGetTickCount();
                if (current_time - last_telemetry_send >= pdMS_TO_TICKS(1000)) {
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
                        ESP_LOGE(TAG, "发送遥测数据失败");
                        client_state = CLIENT_STATE_ERROR;
                    }
                    
                    last_telemetry_send = current_time;
                }
                
                vTaskDelay(pdMS_TO_TICKS(100)); // 100ms
                break;
            }
            
            default:
                vTaskDelay(pdMS_TO_TICKS(100)); // 100ms
                break;
        }
    }
}
