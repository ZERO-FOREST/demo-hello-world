#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include "tcp_protocol.h"
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#ifdef __cplusplus
extern "C" {
#endif

// ----------------- 配置常量 -----------------
#define ESP32_SERVER_IP "192.168.97.247"  // 服务器ESP32的IP地址
#define ESP32_SERVER_PORT 6667             // 服务器ESP32的端口
#define RECV_BUFFER_SIZE 1024              // 接收缓冲区大小
#define FRAME_BUFFER_SIZE 256              // 帧缓冲区大小
#define RECONNECT_DELAY_MS 5000            // 重连延时

// WiFi配置
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASS      "YOUR_WIFI_PASSWORD"
#define WIFI_MAXIMUM_RETRY  5

// ----------------- 客户端状态 -----------------
typedef enum {
    CLIENT_STATE_DISCONNECTED = 0,
    CLIENT_STATE_CONNECTING,
    CLIENT_STATE_CONNECTED,
    CLIENT_STATE_ERROR
} client_state_t;

// ----------------- 模拟遥测数据 -----------------
typedef struct {
    uint16_t voltage_mv;
    uint16_t current_ma;
    int16_t roll_deg;
    int16_t pitch_deg;
    int16_t yaw_deg;
    int32_t altitude_cm;
} simulated_telemetry_t;

// ----------------- 函数声明 -----------------

/**
 * @brief 初始化TCP客户端
 * @return true 成功, false 失败
 */
bool tcp_client_init(void);

/**
 * @brief 连接到ESP32服务器
 * @return true 成功, false 失败
 */
bool tcp_client_connect(void);

/**
 * @brief 断开连接
 */
void tcp_client_disconnect(void);

/**
 * @brief 获取客户端状态
 * @return 当前状态
 */
client_state_t tcp_client_get_state(void);

/**
 * @brief 发送遥测数据
 * @param telemetry_data 遥测数据指针
 * @return true 成功, false 失败
 */
bool tcp_client_send_telemetry(const telemetry_data_payload_t *telemetry_data);

/**
 * @brief 处理接收到的数据
 * @return true 继续处理, false 连接断开
 */
bool tcp_client_process_received_data(void);

/**
 * @brief 主循环任务
 * 包含自动重连和数据处理
 */
void tcp_client_task(void);

/**
 * @brief 更新模拟遥测数据
 * @param sim_data 模拟数据指针
 */
void update_simulated_telemetry(simulated_telemetry_t *sim_data);

/**
 * @brief 打印接收到的数据帧信息
 * @param frame 协议帧指针
 */
void print_received_frame(const protocol_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif // TCP_CLIENT_H
