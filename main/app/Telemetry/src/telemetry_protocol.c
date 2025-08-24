#include "telemetry_protocol.h"
#include <string.h>

/**
 * @brief 计算 Modbus CRC16 校验码
 * 
 * @param data 待计算数据指针
 * @param len 数据长度
 * @return uint16_t CRC16 校验码
 */
static uint16_t crc16_modbus(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

/**
 * @brief 完成数据帧的封装，填充帧头、长度和 CRC
 * 
 * @param buffer 帧缓冲区
 * @param payload_len 负载数据长度
 * @param type 帧类型
 */
static void frame_finish(uint8_t *buffer, size_t payload_len, uint8_t type)
{
    telemetry_header_t *header = (telemetry_header_t *)buffer;
    header->header1 = FRAME_HEADER_1;
    header->header2 = FRAME_HEADER_2;
    header->type = type;

    // 长度 = 类型(1) + 负载 + CRC(2)
    size_t data_len_for_crc = 1 + payload_len;
    header->len = data_len_for_crc + 2;

    uint16_t crc = crc16_modbus(&buffer[3], data_len_for_crc);
    buffer[sizeof(telemetry_header_t) + payload_len] = crc & 0xFF;
    buffer[sizeof(telemetry_header_t) + payload_len + 1] = (crc >> 8) & 0xFF;
}

size_t telemetry_encode_rc_command(uint8_t *buffer, size_t buffer_size, uint8_t channel_count, const uint16_t *channels) {
    if (channel_count == 0 || channel_count > 8) {
        return 0;
    }

    size_t payload_len = 1 + channel_count * sizeof(uint16_t);
    size_t frame_len = sizeof(telemetry_header_t) + payload_len + 2; // 2 for CRC

    if (buffer_size < frame_len) {
        return 0;
    }

    // 负载
    buffer[sizeof(telemetry_header_t)] = channel_count;
    memcpy(&buffer[sizeof(telemetry_header_t) + 1], channels, channel_count * sizeof(uint16_t));

    frame_finish(buffer, payload_len, FRAME_TYPE_RC);

    return frame_len;
}

size_t telemetry_encode_ext_command(uint8_t *buffer, size_t buffer_size, uint8_t cmd_id, const uint8_t *params, uint8_t param_len) {
    size_t payload_len = 1 + 1 + param_len; // cmd_id + param_len + params
    size_t frame_len = sizeof(telemetry_header_t) + payload_len + 2; // 2 for CRC

    if (buffer_size < frame_len) {
        return 0;
    }

    // 负载
    uint8_t *payload_ptr = &buffer[sizeof(telemetry_header_t)];
    *payload_ptr++ = cmd_id;
    *payload_ptr++ = param_len;
    if (param_len > 0 && params != NULL) {
        memcpy(payload_ptr, params, param_len);
    }

    frame_finish(buffer, payload_len, FRAME_TYPE_EXT_CMD);

    return frame_len;
}
