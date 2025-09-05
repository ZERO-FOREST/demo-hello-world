/**
 * @file tcp_common_protocol.h
 * @brief TCP通信协议公共定义
 * @author TidyCraze
 * @date 2025-09-05
 */

#ifndef TCP_COMMON_PROTOCOL_H
#define TCP_COMMON_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------- 协议常量 -----------------
#define PROTOCOL_SYNC_WORD 0xAA55               // 同步字
#define FRAME_TYPE_HEARTBEAT 0x03               // 心跳帧类型
#define FRAME_TYPE_TELEMETRY 0x02               // 遥测帧类型
#define FRAME_TYPE_COMMAND 0x01                 // 命令帧类型
#define FRAME_TYPE_EXTENDED 0x04                // 扩展帧类型

#define MAX_PAYLOAD_SIZE 128                    // 最大负载大小
#define MIN_FRAME_SIZE 8                        // 最小帧大小

// ----------------- 协议头结构 -----------------
typedef struct __attribute__((packed)) {
    uint16_t sync_word;                         // 同步字 0xAA55
    uint8_t frame_type;                         // 帧类型
    uint8_t sequence_number;                    // 序列号
    uint16_t payload_length;                    // 负载长度
} protocol_header_t;

// ----------------- 负载结构定义 -----------------

// 心跳负载结构
typedef struct __attribute__((packed)) {
    uint8_t device_status;                      // 设备状态
    uint32_t timestamp;                         // 时间戳
} heartbeat_payload_t;

// 遥测数据负载结构
typedef struct __attribute__((packed)) {
    uint16_t voltage_mv;                        // 电压 (mV)
    uint16_t current_ma;                        // 电流 (mA)
    int16_t roll_deg;                           // Roll角 (0.01°)
    int16_t pitch_deg;                          // Pitch角 (0.01°)
    int16_t yaw_deg;                            // Yaw角 (0.01°)
    int32_t altitude_cm;                        // 高度 (cm)
} telemetry_data_payload_t;

// 命令负载结构
typedef struct __attribute__((packed)) {
    uint8_t command_type;                       // 命令类型
    uint8_t parameter;                          // 参数
} command_payload_t;

// 扩展命令负载结构
typedef struct __attribute__((packed)) {
    uint8_t cmd_id;                             // 命令ID
    uint8_t param_len;                          // 参数长度
    uint8_t params[MAX_PAYLOAD_SIZE - 2];       // 参数数据
} extended_cmd_payload_t;

// ----------------- 协议帧结构 -----------------
typedef struct __attribute__((packed)) {
    protocol_header_t header;                   // 协议头
    uint8_t payload[MAX_PAYLOAD_SIZE];          // 负载数据
    uint16_t crc;                               // CRC校验
} protocol_frame_t;

// ----------------- 函数声明 -----------------

/**
 * @brief 计算CRC16 (Modbus)
 * @param data 数据指针
 * @param length 数据长度
 * @return CRC16值
 */
uint16_t calculate_crc16_modbus(const uint8_t *data, uint16_t length);

/**
 * @brief 创建心跳帧
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @param device_status 设备状态
 * @param timestamp 时间戳
 * @return 实际帧长度，0表示错误
 */
uint16_t create_heartbeat_frame(uint8_t *buffer, uint16_t buffer_size, 
                               uint8_t device_status, uint32_t timestamp);

/**
 * @brief 创建遥测数据帧
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @param telemetry_data 遥测数据指针
 * @return 实际帧长度，0表示错误
 */
uint16_t create_telemetry_frame_common(uint8_t *buffer, uint16_t buffer_size, 
                                      const telemetry_data_payload_t *telemetry_data);

/**
 * @brief 验证帧的完整性
 * @param frame 协议帧指针
 * @param frame_size 帧大小
 * @return true 验证通过，false 验证失败
 */
bool validate_frame(const protocol_frame_t *frame, uint16_t frame_size);

#ifdef __cplusplus
}
#endif

#endif // TCP_COMMON_PROTOCOL_H