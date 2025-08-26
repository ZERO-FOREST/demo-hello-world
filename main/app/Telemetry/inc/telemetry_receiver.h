#ifndef TELEMETRY_RECEIVER_H
#define TELEMETRY_RECEIVER_H

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

// 遥测接收器端口
#define TELEMETRY_RECEIVER_PORT 6667

/**
 * @brief 初始化遥测接收器
 *
 * @return 成功返回0, 失败返回-1
 */
int telemetry_receiver_init(void);

/**
 * @brief 启动遥测接收器
 *
 * @return 成功返回0, 失败返回-1
 */
int telemetry_receiver_start(void);

/**
 * @brief 停止遥测接收器
 */
void telemetry_receiver_stop(void);

/**
 * @brief 检查遥测接收器是否正在运行
 *
 * @return true 正在运行, false 未运行
 */
bool telemetry_receiver_is_running(void);

/**
 * @brief 获取接收器socket句柄
 *
 * @return socket句柄, 失败返回-1
 */
int telemetry_receiver_get_socket(void);

/**
 * @brief 接受客户端连接
 * 此函数应该在主循环中调用
 */
void telemetry_receiver_accept_connections(void);

#ifdef __cplusplus
}
#endif

#endif // TELEMETRY_RECEIVER_H
