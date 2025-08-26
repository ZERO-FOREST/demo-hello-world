#include "tcp_protocol.h"
#include <string.h>

// ----------------- CRC16 计算 (Modbus) -----------------
static const uint16_t crc16_table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

uint16_t calculate_crc16_modbus(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        uint8_t tbl_idx = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ crc16_table[tbl_idx];
    }
    return crc;
}

// ----------------- 协议帧处理函数 -----------------

uint16_t create_telemetry_frame(const telemetry_data_payload_t *telemetry_data, 
                               uint8_t *frame_buffer, uint16_t buffer_size) {
    if (!telemetry_data || !frame_buffer || buffer_size < 19) {
        return 0;
    }
    
    uint8_t payload_size = sizeof(telemetry_data_payload_t);
    uint8_t frame_length = 1 + payload_size; // 类型字段 + 负载
    uint16_t total_frame_size = 2 + 1 + frame_length + 2; // 帧头 + 长度 + 帧内容 + CRC
    
    if (buffer_size < total_frame_size) {
        return 0;
    }
    
    uint16_t pos = 0;
    
    // 帧头 (大端序)
    frame_buffer[pos++] = (FRAME_HEADER >> 8) & 0xFF;
    frame_buffer[pos++] = FRAME_HEADER & 0xFF;
    
    // 长度字段
    frame_buffer[pos++] = frame_length;
    
    // 帧类型
    frame_buffer[pos++] = FRAME_TYPE_TELEMETRY;
    
    // 负载数据 (小端序)
    memcpy(&frame_buffer[pos], telemetry_data, payload_size);
    pos += payload_size;
    
    // 计算CRC (对长度+类型+负载)
    uint16_t crc = calculate_crc16_modbus(&frame_buffer[2], 1 + 1 + payload_size);
    
    // CRC (小端序)
    frame_buffer[pos++] = crc & 0xFF;
    frame_buffer[pos++] = (crc >> 8) & 0xFF;
    
    return total_frame_size;
}

parse_result_t parse_protocol_frame(const uint8_t *data, uint16_t data_len, protocol_frame_t *frame) {
    if (!data || !frame || data_len < MIN_FRAME_SIZE) {
        return PARSE_ERROR_BUFFER_TOO_SMALL;
    }
    
    // 检查帧头 (大端序)
    uint16_t header = (data[0] << 8) | data[1];
    if (header != FRAME_HEADER) {
        return PARSE_ERROR_INVALID_HEADER;
    }
    
    // 解析长度和类型
    uint8_t length = data[2];
    uint8_t frame_type = data[3];
    
    uint8_t payload_len = length - 1; // 减去类型字段
    uint16_t expected_frame_size = 2 + 1 + length + 2; // 帧头 + 长度 + (类型+负载) + CRC
    
    if (data_len < expected_frame_size) {
        return PARSE_ERROR_BUFFER_TOO_SMALL;
    }
    
    // 验证CRC
    uint16_t received_crc = data[4 + payload_len] | (data[4 + payload_len + 1] << 8);
    uint16_t calculated_crc = calculate_crc16_modbus(&data[2], 1 + length);
    
    if (received_crc != calculated_crc) {
        return PARSE_ERROR_INVALID_CRC;
    }
    
    // 填充解析结果
    frame->header = header;
    frame->length = length;
    frame->frame_type = frame_type;
    frame->crc = received_crc;
    
    // 根据帧类型解析负载
    const uint8_t *payload_data = &data[4];
    
    switch (frame_type) {
        case FRAME_TYPE_REMOTE_CONTROL:
            if (payload_len >= 1) {
                frame->payload.remote_control.channel_count = payload_data[0];
                uint8_t channels = frame->payload.remote_control.channel_count;
                if (channels > MAX_CHANNELS) channels = MAX_CHANNELS;
                
                for (uint8_t i = 0; i < channels && (1 + i * 2 + 1) < payload_len; i++) {
                    frame->payload.remote_control.channels[i] = 
                        payload_data[1 + i * 2] | (payload_data[1 + i * 2 + 1] << 8);
                }
            }
            break;
            
        case FRAME_TYPE_TELEMETRY:
            if (payload_len >= sizeof(telemetry_data_payload_t)) {
                memcpy(&frame->payload.telemetry, payload_data, sizeof(telemetry_data_payload_t));
            }
            break;
            
        case FRAME_TYPE_HEARTBEAT:
            if (payload_len >= 1) {
                frame->payload.heartbeat.device_status = payload_data[0];
            }
            break;
            
        case FRAME_TYPE_EXTENDED_CMD:
            if (payload_len >= 2) {
                frame->payload.extended_cmd.cmd_id = payload_data[0];
                frame->payload.extended_cmd.param_len = payload_data[1];
                uint8_t param_len = frame->payload.extended_cmd.param_len;
                if (param_len > (MAX_PAYLOAD_SIZE - 2)) param_len = MAX_PAYLOAD_SIZE - 2;
                if (param_len > 0 && (2 + param_len) <= payload_len) {
                    memcpy(frame->payload.extended_cmd.params, &payload_data[2], param_len);
                }
            }
            break;
            
        default:
            // 未知类型，复制原始负载
            if (payload_len <= MAX_PAYLOAD_SIZE) {
                memcpy(frame->payload.raw_payload, payload_data, payload_len);
            }
            break;
    }
    
    return PARSE_SUCCESS;
}

// ----------------- 默认数据处理函数 -----------------

__attribute__((weak)) void handle_remote_control_data(const remote_control_payload_t *remote_data) {
    // 默认实现，用户可以重写
    if (!remote_data) return;
    
    // 打印遥控数据示例
    // printf("遥控数据: 通道数=%d\n", remote_data->channel_count);
    // for (uint8_t i = 0; i < remote_data->channel_count && i < MAX_CHANNELS; i++) {
    //     printf("  CH%d: %d\n", i+1, remote_data->channels[i]);
    // }
}

__attribute__((weak)) void handle_heartbeat_data(const heartbeat_payload_t *heartbeat_data) {
    // 默认实现，用户可以重写
    if (!heartbeat_data) return;
    
    // 打印心跳数据示例
    // const char* status_str[] = {"空闲", "正常运行", "错误", "未知"};
    // uint8_t status_idx = (heartbeat_data->device_status <= 2) ? heartbeat_data->device_status : 3;
    // printf("心跳: 设备状态=%s\n", status_str[status_idx]);
}

__attribute__((weak)) void handle_extended_command(const extended_cmd_payload_t *cmd_data) {
    // 默认实现，用户可以重写
    if (!cmd_data) return;
    
    // 打印扩展命令示例
    // printf("扩展命令: ID=0x%02X, 参数长度=%d\n", cmd_data->cmd_id, cmd_data->param_len);
}
