#ifndef TELEMETRY_TCP_H
#define TELEMETRY_TCP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief 初始化遥测TCP客户端
 */
void telemetry_tcp_client_init(void);

/**
 * @brief 连接到遥测服务器
 *
 * @param host 服务器主机名或IP地址
 * @param port 服务器端口号
 * @return 成功返回0, 失败返回-1
 */
int telemetry_tcp_client_connect(const char *host, int port);

/**
 * @brief 从遥测服务器断开连接
 */
void telemetry_tcp_client_disconnect(void);

/**
 * @brief 发送数据到遥测服务器
 *
 * @param data 要发送的数据
 * @param len 数据长度
 * @return 发送的字节数, 失败返回-1
 */
int telemetry_tcp_client_send(const void *data, size_t len);

/**
 * @brief 检查TCP客户端是否已连接
 * 
 * @return true 已连接, false 未连接
 */
bool telemetry_tcp_client_is_connected(void);

/**
 * @brief 启动TCP服务器
 *
 * @param port 服务器监听端口号
 * @return 成功返回0, 失败返回-1
 */
int telemetry_tcp_server_start(int port);

/**
 * @brief 停止TCP服务器
 */
void telemetry_tcp_server_stop(void);

/**
 * @brief 检查TCP服务器是否正在运行
 * 
 * @return true 正在运行, false 未运行
 */
bool telemetry_tcp_server_is_running(void);

#endif // TELEMETRY_TCP_H
