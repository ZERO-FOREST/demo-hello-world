#ifndef TELEMETRY_SENDER_H
#define TELEMETRY_SENDER_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化遥测发送器
 *
 * @return 成功返回0, 失败返回-1
 */
int telemetry_sender_init(void);

/**
 * @brief 设置客户端socket
 *
 * @param client_sock 客户端socket句柄
 */
void telemetry_sender_set_client_socket(int client_sock);

/**
 * @brief 检查遥测发送器是否激活
 *
 * @return true 已激活, false 未激活
 */
bool telemetry_sender_is_active(void);

/**
 * @brief 处理遥测数据发送
 * 此函数应该在主循环中调用
 */
void telemetry_sender_process(void);

/**
 * @brief 停用遥测发送器
 */
void telemetry_sender_deactivate(void);

#ifdef __cplusplus
}
#endif

#endif // TELEMETRY_SENDER_H
