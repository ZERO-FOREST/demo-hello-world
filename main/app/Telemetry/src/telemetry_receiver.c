#include "telemetry_receiver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "telemetry_main.h"
#include "telemetry_protocol.h"
#include "telemetry_sender.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

static const char* TAG = "telemetry_receiver";

// 内部函数声明
static void handle_client_connection(int client_sock);
static void process_received_frame(const parsed_frame_t* frame);

// 全局变量
static int g_listen_sock = -1;
static bool g_server_running = false;

int telemetry_receiver_init(void) {
    ESP_LOGI(TAG, "Initializing telemetry receiver");
    return 0;
}

/**
 * @brief 启动接收器
 *
 * @return 0 成功，-1 失败
 */
int telemetry_receiver_start(void) {
    if (g_server_running) {
        ESP_LOGW(TAG, "Receiver already running");
        return 0;
    }

    // 创建监听socket
    g_listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (g_listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket");
        return -1;
    }

    int opt = 1;
    setsockopt(g_listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = INADDR_ANY;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(TELEMETRY_RECEIVER_PORT);

    int err = bind(g_listen_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind to port %d", TELEMETRY_RECEIVER_PORT);
        lwip_close(g_listen_sock);
        g_listen_sock = -1;
        return -1;
    }

    err = listen(g_listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen");
        lwip_close(g_listen_sock);
        g_listen_sock = -1;
        return -1;
    }

    g_server_running = true;
    ESP_LOGI(TAG, "Telemetry receiver started on port %d", TELEMETRY_RECEIVER_PORT);
    return 0;
}

/**
 * @brief 停止接收器
 */
void telemetry_receiver_stop(void) {
    if (!g_server_running) {
        return;
    }

    g_server_running = false;

    if (g_listen_sock >= 0) {
        lwip_close(g_listen_sock);
        g_listen_sock = -1;
    }

    ESP_LOGI(TAG, "Telemetry receiver stopped");
}

bool telemetry_receiver_is_running(void) { return g_server_running; }

int telemetry_receiver_get_socket(void) { return g_listen_sock; }

/**
 * @brief 接受客户端连接
 */
void telemetry_receiver_accept_connections(void) {
    if (!g_server_running || g_listen_sock < 0) {
        return;
    }

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    ESP_LOGI(TAG, "Waiting for a client to connect...");
    int client_sock = accept(g_listen_sock, (struct sockaddr*)&client_addr, &client_addr_len);

    if (client_sock >= 0) {
        ESP_LOGI(TAG, "Client connected from %s:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // 设置套接字为非阻塞
        int flags = fcntl(client_sock, F_GETFL, 0);
        fcntl(client_sock, F_SETFL, flags | O_NONBLOCK);

        handle_client_connection(client_sock);
        lwip_close(client_sock);
        ESP_LOGI(TAG, "Client disconnected");
    } else {
        ESP_LOGE(TAG, "Accept failed with error: %d", errno);
    }
}

/**
 * @brief 处理客户端连接
 *
 * @param client_sock 客户端套接字
 */
static void handle_client_connection(int client_sock) {
    uint8_t rx_buffer[512];
    size_t buffer_len = 0;
    uint32_t last_packet_time = xTaskGetTickCount();

    // 激活发送器
    telemetry_sender_set_client_socket(client_sock);

    ESP_LOGI(TAG, "Client connection handler started.");

    while (g_server_running) {
        // 从socket读取数据
        int len = recv(client_sock, rx_buffer + buffer_len, sizeof(rx_buffer) - buffer_len, 0);

        if (len > 0) {
            buffer_len += len;
            last_packet_time = xTaskGetTickCount();
            ESP_LOGD(TAG, "Received %d bytes, buffer now %d bytes", len, buffer_len);

            // 循环处理缓冲区中的数据，直到无法解析出完整的帧
            while (buffer_len > 0) {
                parsed_frame_t frame;
                size_t frame_len = telemetry_protocol_parse_frame(rx_buffer, buffer_len, &frame);

                if (frame_len > 0) {
                    // 成功解析出一个帧
                    ESP_LOGD(TAG, "Parsed a frame of length %d", frame_len);
                    process_received_frame(&frame);

                    // 从缓冲区移除已处理的帧
                    memmove(rx_buffer, rx_buffer + frame_len, buffer_len - frame_len);
                    buffer_len -= frame_len;
                } else {
                    // 缓冲区中没有完整的帧，等待更多数据
                    break;
                }
            }
        } else if (len == 0) {
            ESP_LOGI(TAG, "Connection closed by client");
            break;
        } else {
            // len < 0
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            }
            // 没有数据可读，正常情况
        }

        // 检查心跳超时 (例如，10秒内未收到任何数据包)
        if (xTaskGetTickCount() - last_packet_time > pdMS_TO_TICKS(10000)) {
            ESP_LOGW(TAG, "Client timeout");
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // 短暂延时，避免CPU占用过高
    }

    // 停用发送器
    telemetry_sender_deactivate();
    ESP_LOGI(TAG, "Client connection handler finished.");
}

/**
 * @brief 处理接收到的帧
 *
 * @param frame 解析后的帧
 */
static void process_received_frame(const parsed_frame_t* frame) {
    if (!frame->crc_ok) {
        ESP_LOGW(TAG, "Received a frame with bad CRC. Type: 0x%02X", frame->header.type);
        return;
    }

    switch (frame->header.type) {
    case FRAME_TYPE_TELEMETRY:
        if (frame->payload_len == sizeof(telemetry_data_payload_t)) {
            const telemetry_data_payload_t* telemetry_data = (const telemetry_data_payload_t*)frame->payload;
            ESP_LOGI(TAG, "Received telemetry: V=%umV, I=%umA, Roll=%.2f", telemetry_data->voltage_mv,
                     telemetry_data->current_ma, telemetry_data->roll_deg / 100.0f);

            // 将数据传递给主服务
            telemetry_service_update_data(telemetry_data);

        } else {
            ESP_LOGW(TAG, "Received telemetry frame with incorrect payload size: %d", frame->payload_len);
        }
        break;

    case FRAME_TYPE_EXT_CMD:
        // TODO: 处理扩展命令
        ESP_LOGI(TAG, "Received extended command frame (not implemented)");
        break;

    // 不应该收到遥控或心跳包，因为这是ESP32发送的
    case FRAME_TYPE_RC:
    case FRAME_TYPE_HEARTBEAT:
        ESP_LOGW(TAG, "Received unexpected frame type from client: 0x%02X", frame->header.type);
        break;

    default:
        ESP_LOGW(TAG, "Received unknown frame type: 0x%02X", frame->header.type);
        break;
    }
}
