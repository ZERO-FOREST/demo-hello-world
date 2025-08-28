#ifndef TCP_PROTOCOL_H
#define TCP_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------- 协议常量 -----------------
#define FRAME_HEADER 0xAA55
#define FRAME_TYPE_REMOTE_CONTROL 0x01
#define FRAME_TYPE_TELEMETRY 0x02
#define FRAME_TYPE_HEARTBEAT 0x03
#define FRAME_TYPE_EXTENDED_CMD 0x04

#define MAX_CHANNELS 8
#define MAX_PAYLOAD_SIZE 128
#define MIN_FRAME_SIZE 7  // 最小帧长度 (心跳包)

// ----------------- 数据结构定义 -----------------

// 遥控数据结构
typedef struct {
    uint8_t channel_count;
    uint16_t channels[MAX_CHANNELS];
} remote_control_payload_t;

// 遥测数据结构 (与ESP32发送的格式匹配)
typedef struct {
    uint16_t voltage_mv;    // 电压 (mV)
    uint16_t current_ma;    // 电流 (mA)
    int16_t roll_deg;       // Roll角 (0.01°)
    int16_t pitch_deg;      // Pitch角 (0.01°)
    int16_t yaw_deg;        // Yaw角 (0.01°)
    int32_t altitude_cm;    // 高度 (cm)
} telemetry_data_payload_t;

// 心跳数据结构
typedef struct {
    uint8_t device_status;  // 0:空闲, 1:正常运行, 2:错误
} heartbeat_payload_t;

// 扩展命令结构
typedef struct {
    uint8_t cmd_id;
    uint8_t param_len;
    uint8_t params[MAX_PAYLOAD_SIZE - 2];
} extended_cmd_payload_t;

// 通用数据帧结构
typedef struct {
    uint16_t header;
    uint8_t length;
    uint8_t frame_type;
    union {
        remote_control_payload_t remote_control;
        telemetry_data_payload_t telemetry;
        heartbeat_payload_t heartbeat;
        extended_cmd_payload_t extended_cmd;
        uint8_t raw_payload[MAX_PAYLOAD_SIZE];
    } payload;
    uint16_t crc;
} protocol_frame_t;

// 解析状态枚举
typedef enum {
    PARSE_SUCCESS = 0,
    PARSE_ERROR_INVALID_HEADER,
    PARSE_ERROR_INVALID_CRC,
    PARSE_ERROR_INVALID_LENGTH,
    PARSE_ERROR_BUFFER_TOO_SMALL
} parse_result_t;

// ----------------- 函数声明 -----------------

/**
 * @brief 计算CRC16 (Modbus)
 * @param data 数据指针
 * @param length 数据长度
 * @return CRC16值
 */
uint16_t calculate_crc16_modbus(const uint8_t *data, uint16_t length);

/**
 * @brief 创建遥测数据帧
 * @param telemetry_data 遥测数据指针
 * @param frame_buffer 输出帧缓冲区
 * @param buffer_size 缓冲区大小
 * @return 实际帧长度，0表示错误
 */
uint16_t create_telemetry_frame(const telemetry_data_payload_t *telemetry_data, 
                               uint8_t *frame_buffer, uint16_t buffer_size);

/**
 * @brief 解析协议帧
 * @param data 原始数据指针
 * @param data_len 数据长度
 * @param frame 输出解析后的帧结构
 * @return 解析结果
 */
parse_result_t parse_protocol_frame(const uint8_t *data, uint16_t data_len, protocol_frame_t *frame);

/**
 * @brief 处理接收到的遥控命令
 * @param remote_data 遥控数据指针
 */
void handle_remote_control_data(const remote_control_payload_t *remote_data);

/**
 * @brief 处理接收到的心跳包
 * @param heartbeat_data 心跳数据指针
 */
void handle_heartbeat_data(const heartbeat_payload_t *heartbeat_data);

/**
 * @brief 处理接收到的扩展命令
 * @param cmd_data 扩展命令数据指针
 */
void handle_extended_command(const extended_cmd_payload_t *cmd_data);

#ifdef __cplusplus
}
#endif

#endif // TCP_PROTOCOL_H
