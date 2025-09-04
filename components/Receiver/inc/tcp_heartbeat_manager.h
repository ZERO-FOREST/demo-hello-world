#ifndef TCP_HEARTBEAT_MANAGER_H
#define TCP_HEARTBEAT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "tcp_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// ----------------- 配置宏定义 -----------------
#define TCP_HEARTBEAT_SERVER_PORT 7878          // 服务端口，可通过修改此宏调整
#define TCP_HEARTBEAT_INTERVAL_MS 30000         // 心跳发送间隔：30秒
#define TCP_RECONNECT_INTERVAL_MS 5000          // 自动重连间隔：5秒
#define TCP_HEARTBEAT_TIMEOUT_MS 90000          // 心跳超时时间：90秒（3倍心跳间隔）
#define TCP_SEND_TIMEOUT_MS 5000                // 发送超时时间：5秒
#define TCP_RECV_TIMEOUT_MS 1000                // 接收超时时间：1秒

// 设备状态定义（符合协议文档）
#define DEVICE_STATUS_IDLE 0x00                 // 空闲
#define DEVICE_STATUS_RUNNING 0x01              // 正常运行
#define DEVICE_STATUS_ERROR 0x02                // 错误

// ----------------- 连接状态枚举 -----------------
typedef enum {
    TCP_HB_STATE_DISCONNECTED = 0,          // 未连接
    TCP_HB_STATE_CONNECTING,                // 连接中
    TCP_HB_STATE_CONNECTED,                 // 已连接
    TCP_HB_STATE_RECONNECTING,              // 重连中
    TCP_HB_STATE_ERROR                      // 错误状态
} tcp_heartbeat_state_t;

// ----------------- 心跳统计信息 -----------------
typedef struct {
    uint32_t heartbeat_sent_count;          // 已发送心跳包数量
    uint32_t heartbeat_failed_count;        // 发送失败心跳包数量
    uint32_t connection_count;              // 连接次数
    uint32_t reconnection_count;            // 重连次数
    uint64_t last_heartbeat_time;           // 最后一次心跳时间戳（毫秒）
    uint64_t connection_start_time;         // 连接开始时间戳（毫秒）
    uint64_t total_connected_time;          // 总连接时间（毫秒）
} tcp_heartbeat_stats_t;

// ----------------- 心跳管理器配置 -----------------
typedef struct {
    char server_ip[16];                     // 服务器IP地址
    uint16_t server_port;                   // 服务器端口
    uint32_t heartbeat_interval_ms;         // 心跳间隔
    uint32_t reconnect_interval_ms;         // 重连间隔
    uint32_t heartbeat_timeout_ms;          // 心跳超时时间
    uint8_t device_status;                  // 当前设备状态
    bool auto_reconnect_enabled;           // 是否启用自动重连
} tcp_heartbeat_config_t;

// ----------------- 心跳管理器结构体 -----------------
typedef struct {
    tcp_heartbeat_config_t config;          // 配置信息
    tcp_heartbeat_state_t state;            // 当前状态
    tcp_heartbeat_stats_t stats;            // 统计信息
    int socket_fd;                          // TCP套接字文件描述符
    TimerHandle_t heartbeat_timer;          // 心跳定时器
    TimerHandle_t reconnect_timer;          // 重连定时器
    TaskHandle_t heartbeat_task_handle;     // 心跳任务句柄
    bool is_initialized;                    // 是否已初始化
    bool is_running;                        // 是否正在运行
} tcp_heartbeat_manager_t;

// ----------------- 函数声明 -----------------

/**
 * @brief 初始化TCP心跳管理器
 * @param server_ip 服务器IP地址
 * @param server_port 服务器端口（默认使用TCP_HEARTBEAT_SERVER_PORT）
 * @return true 初始化成功，false 初始化失败
 */
bool tcp_heartbeat_manager_init(const char *server_ip, uint16_t server_port);

/**
 * @brief 启动TCP心跳管理器
 * @return true 启动成功，false 启动失败
 */
bool tcp_heartbeat_manager_start(void);

/**
 * @brief 停止TCP心跳管理器
 */
void tcp_heartbeat_manager_stop(void);

/**
 * @brief 销毁TCP心跳管理器，释放所有资源
 */
void tcp_heartbeat_manager_destroy(void);

/**
 * @brief 获取当前连接状态
 * @return 当前连接状态
 */
tcp_heartbeat_state_t tcp_heartbeat_manager_get_state(void);

/**
 * @brief 获取心跳统计信息
 * @return 心跳统计信息指针
 */
const tcp_heartbeat_stats_t* tcp_heartbeat_manager_get_stats(void);

/**
 * @brief 设置设备状态
 * @param status 设备状态（DEVICE_STATUS_IDLE/RUNNING/ERROR）
 */
void tcp_heartbeat_manager_set_device_status(uint8_t status);

/**
 * @brief 手动触发心跳发送
 * @return true 发送成功，false 发送失败
 */
bool tcp_heartbeat_manager_send_heartbeat_now(void);

/**
 * @brief 手动触发重连
 * @return true 重连启动成功，false 重连启动失败
 */
bool tcp_heartbeat_manager_reconnect_now(void);

/**
 * @brief 启用或禁用自动重连
 * @param enabled true 启用，false 禁用
 */
void tcp_heartbeat_manager_set_auto_reconnect(bool enabled);

/**
 * @brief 检查连接是否健康
 * @return true 连接健康，false 连接异常
 */
bool tcp_heartbeat_manager_is_connection_healthy(void);

/**
 * @brief 重置统计信息
 */
void tcp_heartbeat_manager_reset_stats(void);

/**
 * @brief 打印心跳管理器状态信息（用于调试）
 */
void tcp_heartbeat_manager_print_status(void);

#ifdef __cplusplus
}
#endif

#endif // TCP_HEARTBEAT_MANAGER_H