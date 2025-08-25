#ifndef TELEMETRY_MAIN_H
#define TELEMETRY_MAIN_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 遥测服务状态枚举
 */
typedef enum {
    TELEMETRY_STATUS_STOPPED = 0,
    TELEMETRY_STATUS_STARTING,
    TELEMETRY_STATUS_RUNNING,
    TELEMETRY_STATUS_STOPPING,
    TELEMETRY_STATUS_ERROR
} telemetry_status_t;

/**
 * @brief 遥测控制数据结构
 */
typedef struct {
    int32_t throttle;   // 油门值 (0-1000)
    int32_t direction;  // 方向值 (0-1000)
    float voltage;      // 电压
    float current;      // 电流
    float roll;         // 横滚角
    float pitch;        // 俯仰角
    float yaw;          // 偏航角
    float altitude;     // 高度
} telemetry_data_t;

/**
 * @brief 遥测服务回调函数类型
 */
typedef void (*telemetry_data_callback_t)(const telemetry_data_t *data);

/**
 * @brief 初始化遥测服务
 * 
 * @return 成功返回0, 失败返回-1
 */
int telemetry_service_init(void);

/**
 * @brief 启动遥测服务
 * 
 * @param callback 数据回调函数（可选，用于UI更新）
 * @return 成功返回0, 失败返回-1
 */
int telemetry_service_start(telemetry_data_callback_t callback);

/**
 * @brief 停止遥测服务
 * 
 * @return 成功返回0, 失败返回-1
 */
int telemetry_service_stop(void);

/**
 * @brief 获取遥测服务状态
 * 
 * @return 服务状态
 */
telemetry_status_t telemetry_service_get_status(void);

/**
 * @brief 发送控制命令
 * 
 * @param throttle 油门值 (0-1000)
 * @param direction 方向值 (0-1000)
 * @return 成功返回0, 失败返回-1
 */
int telemetry_service_send_control(int32_t throttle, int32_t direction);

/**
 * @brief 获取当前遥测数据
 * 
 * @param data 输出数据结构指针
 * @return 成功返回0, 失败返回-1
 */
int telemetry_service_get_data(telemetry_data_t *data);

/**
 * @brief 反初始化遥测服务
 */
void telemetry_service_deinit(void);

#endif // TELEMETRY_MAIN_H
