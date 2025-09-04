#ifndef TCP_CLIENT_WITH_HEARTBEAT_H
#define TCP_CLIENT_WITH_HEARTBEAT_H

#include "tcp_client.h"
#include "tcp_heartbeat_manager.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------- 集成配置 -----------------
#define TCP_CLIENT_HB_DEFAULT_SERVER_IP "192.168.97.247"
#define TCP_CLIENT_HB_DEFAULT_SERVER_PORT TCP_HEARTBEAT_SERVER_PORT  // 使用心跳管理器的默认端口7878

// ----------------- 集成状态枚举 -----------------
typedef enum {
    TCP_CLIENT_HB_STATUS_STOPPED = 0,       // 已停止
    TCP_CLIENT_HB_STATUS_STARTING,           // 启动中
    TCP_CLIENT_HB_STATUS_RUNNING,            // 运行中
    TCP_CLIENT_HB_STATUS_STOPPING,           // 停止中
    TCP_CLIENT_HB_STATUS_ERROR               // 错误状态
} tcp_client_hb_status_t;

// ----------------- 集成统计信息 -----------------
typedef struct {
    // 心跳统计
    const tcp_heartbeat_stats_t *heartbeat_stats;
    
    // 遥测统计
    uint32_t telemetry_sent_count;
    uint32_t telemetry_failed_count;
    uint64_t last_telemetry_time;
    
    // 系统状态
    tcp_client_hb_status_t client_status;
    tcp_heartbeat_state_t heartbeat_state;
    client_state_t legacy_client_state;
} tcp_client_hb_stats_t;

// ----------------- 函数声明 -----------------

/**
 * @brief 初始化带心跳的TCP客户端
 * @param server_ip 服务器IP地址（NULL使用默认值）
 * @param server_port 服务器端口（0使用默认值7878）
 * @return true 初始化成功，false 初始化失败
 */
bool tcp_client_hb_init(const char *server_ip, uint16_t server_port);

/**
 * @brief 启动带心跳的TCP客户端
 * @return true 启动成功，false 启动失败
 */
bool tcp_client_hb_start(void);

/**
 * @brief 停止带心跳的TCP客户端
 */
void tcp_client_hb_stop(void);

/**
 * @brief 销毁带心跳的TCP客户端，释放所有资源
 */
void tcp_client_hb_destroy(void);

/**
 * @brief 获取客户端状态
 * @return 客户端状态
 */
tcp_client_hb_status_t tcp_client_hb_get_status(void);

/**
 * @brief 获取综合统计信息
 * @return 统计信息指针
 */
const tcp_client_hb_stats_t* tcp_client_hb_get_stats(void);

/**
 * @brief 设置设备状态（影响心跳包内容）
 * @param status 设备状态（DEVICE_STATUS_IDLE/RUNNING/ERROR）
 */
void tcp_client_hb_set_device_status(uint8_t status);

/**
 * @brief 发送遥测数据
 * @param telemetry_data 遥测数据指针
 * @return true 发送成功，false 发送失败
 */
bool tcp_client_hb_send_telemetry(const telemetry_data_payload_t *telemetry_data);

/**
 * @brief 手动触发心跳发送
 * @return true 发送成功，false 发送失败
 */
bool tcp_client_hb_send_heartbeat_now(void);

/**
 * @brief 手动触发重连
 * @return true 重连启动成功，false 重连启动失败
 */
bool tcp_client_hb_reconnect_now(void);

/**
 * @brief 启用或禁用自动重连
 * @param enabled true 启用，false 禁用
 */
void tcp_client_hb_set_auto_reconnect(bool enabled);

/**
 * @brief 检查连接是否健康
 * @return true 连接健康，false 连接异常
 */
bool tcp_client_hb_is_connection_healthy(void);

/**
 * @brief 重置统计信息
 */
void tcp_client_hb_reset_stats(void);

/**
 * @brief 打印客户端状态信息（用于调试）
 */
void tcp_client_hb_print_status(void);

/**
 * @brief 启动带心跳的TCP客户端任务（替代原有的tcp_client_task）
 * @param task_name 任务名称
 * @param stack_size 堆栈大小
 * @param priority 任务优先级
 * @return true 任务创建成功，false 任务创建失败
 */
bool tcp_client_hb_start_task(const char *task_name, uint32_t stack_size, uint8_t priority);

/**
 * @brief 停止TCP客户端任务
 */
void tcp_client_hb_stop_task(void);

#ifdef __cplusplus
}
#endif

#endif // TCP_CLIENT_WITH_HEARTBEAT_H