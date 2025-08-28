/*
 * @Author: ESP32 Developer
 * @Date: 2025-08-26
 * @Description: TCP遥控+遥测协议定义
 */

#ifndef TCP_PROTOCOL_H
#define TCP_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 协议常量
#define FRAME_HEADER                0xAA55
#define FRAME_TYPE_REMOTE_CONTROL   0x01
#define FRAME_TYPE_TELEMETRY        0x02
#define FRAME_TYPE_HEARTBEAT        0x03
#define FRAME_TYPE_EXTENDED         0x04

#define MAX_CHANNELS                8
#define MAX_FRAME_SIZE              256
#define PROTOCOL_BUFFER_SIZE        1024

// 设备状态定义
typedef enum {
    DEVICE_STATUS_IDLE = 0x00,
    DEVICE_STATUS_RUNNING = 0x01,
    DEVICE_STATUS_ERROR = 0x02
} device_status_t;

// 遥控数据负载结构
typedef struct __attribute__((packed)) {
    uint8_t channel_count;
    uint16_t channels[MAX_CHANNELS];
} remote_control_payload_t;

// 遥测数据负载结构 - 与Python代码完全匹配
typedef struct __attribute__((packed)) {
    uint16_t voltage_mv;     // 电压，单位mV
    uint16_t current_ma;     // 电流，单位mA  
    int16_t roll_deg;        // Roll角，单位0.01度
    int16_t pitch_deg;       // Pitch角，单位0.01度
    int16_t yaw_deg;         // Yaw角，单位0.01度
    int32_t altitude_cm;     // 高度，单位cm
} telemetry_data_payload_t;

// 心跳数据负载结构
typedef struct __attribute__((packed)) {
    uint8_t device_status;
} heartbeat_payload_t;

// 扩展命令负载结构
typedef struct __attribute__((packed)) {
    uint8_t command_id;
    uint8_t param_length;
    uint8_t params[32];  // 最大32字节参数
} extended_command_payload_t;

// 数据帧结构
typedef struct __attribute__((packed)) {
    uint16_t header;    // 帧头 0xAA55
    uint8_t length;     // 长度字段
    uint8_t frame_type; // 帧类型
    uint8_t payload[MAX_FRAME_SIZE]; // 负载数据
    // CRC16会在payload后面
} protocol_frame_t;

// 解析后的帧数据结构
typedef struct {
    uint8_t frame_type;
    uint8_t payload_length;
    union {
        remote_control_payload_t remote_control;
        telemetry_data_payload_t telemetry;
        heartbeat_payload_t heartbeat;
        extended_command_payload_t extended;
        uint8_t raw_payload[MAX_FRAME_SIZE];
    } data;
} parsed_frame_t;

// 协议处理回调函数类型
typedef void (*protocol_callback_t)(const parsed_frame_t* frame);

// CRC16计算函数
uint16_t calculate_crc16(const uint8_t* data, size_t length);

// 帧构建函数
size_t build_telemetry_frame(uint8_t* buffer, const telemetry_data_payload_t* telemetry);
size_t build_heartbeat_frame(uint8_t* buffer, device_status_t status);
size_t build_remote_control_frame(uint8_t* buffer, const remote_control_payload_t* remote_control);

// 帧解析函数
esp_err_t parse_frame(const uint8_t* data, size_t data_length, parsed_frame_t* parsed_frame);

// 协议缓冲区处理
typedef struct {
    uint8_t buffer[PROTOCOL_BUFFER_SIZE];
    size_t write_pos;
    size_t read_pos;
    protocol_callback_t callback;
} protocol_buffer_t;

// 协议缓冲区初始化
void protocol_buffer_init(protocol_buffer_t* pb, protocol_callback_t callback);

// 向协议缓冲区添加数据
esp_err_t protocol_buffer_add_data(protocol_buffer_t* pb, const uint8_t* data, size_t length);

// 处理协议缓冲区中的完整帧
esp_err_t protocol_buffer_process(protocol_buffer_t* pb);

#ifdef __cplusplus
}
#endif

#endif // TCP_PROTOCOL_H
