#!/usr/bin/env python3
"""
ESP32 遥测数据接收和解析测试脚本
基于协议文档实现数据帧解析
"""

import socket
import struct
import threading
import time
import sys
from typing import Optional, Tuple, Dict, Any
from enum import IntEnum

class FrameType(IntEnum):
    """帧类型枚举"""
    RC_COMMAND = 0x01      # 遥控命令 (地面站 → ESP32)
    TELEMETRY = 0x02       # 遥测数据 (ESP32 → 地面站)
    HEARTBEAT = 0x03       # 心跳包 (ESP32 → 地面站)
    EXT_COMMAND = 0x04     # 扩展命令 (地面站 → ESP32)

class DeviceStatus(IntEnum):
    """设备状态枚举"""
    IDLE = 0x00           # 空闲
    NORMAL = 0x01         # 正常运行
    ERROR = 0x02          # 错误

class ExtCommandID(IntEnum):
    """扩展命令ID枚举"""
    SET_PWM_FREQ = 0x10       # 设置PWM频率
    MODE_SWITCH = 0x11        # 模式切换
    CALIBRATE_SENSOR = 0x12   # 校准传感器
    REQUEST_TELEMETRY = 0x13  # 请求遥测
    LIGHT_CONTROL = 0x14      # 灯光控制

class TelemetryProtocol:
    """遥测协议解析器"""
    
    FRAME_HEADER = b'\xAA\x55'  # 帧头
    
    def __init__(self):
        self.buffer = bytearray()
        
    def crc16_modbus(self, data: bytes) -> int:
        """计算Modbus CRC16校验"""
        crc = 0xFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 0x0001:
                    crc = (crc >> 1) ^ 0xA001
                else:
                    crc >>= 1
        return crc
    
    def add_data(self, data: bytes) -> None:
        """添加接收到的数据到缓冲区"""
        self.buffer.extend(data)
    
    def parse_frames(self):
        """解析缓冲区中的帧，返回解析到的帧列表"""
        frames = []
        
        while len(self.buffer) >= 6:  # 最小帧长度：帧头(2) + 长度(1) + 类型(1) + CRC(2)
            # 查找帧头
            header_pos = self.buffer.find(self.FRAME_HEADER)
            if header_pos == -1:
                # 没找到帧头，清空缓冲区
                self.buffer.clear()
                break
            
            if header_pos > 0:
                # 丢弃帧头前的数据
                self.buffer = self.buffer[header_pos:]
            
            if len(self.buffer) < 4:
                break
                
            # 读取长度字段
            frame_len = self.buffer[2]
            total_len = 3 + frame_len  # 帧头(2) + 长度(1) + 数据部分(frame_len)
            
            if len(self.buffer) < total_len:
                # 数据不够，等待更多数据
                break
            
            # 提取完整帧
            frame_data = bytes(self.buffer[:total_len])
            self.buffer = self.buffer[total_len:]
            
            # 解析帧
            parsed_frame = self.parse_single_frame(frame_data)
            if parsed_frame:
                frames.append(parsed_frame)
        
        return frames
    
    def parse_single_frame(self, frame_data: bytes) -> Optional[Dict[str, Any]]:
        """解析单个帧"""
        if len(frame_data) < 6:
            return None
            
        # 解析帧头
        header = frame_data[:2]
        if header != self.FRAME_HEADER:
            print(f"错误的帧头: {header.hex()}")
            return None
        
        frame_len = frame_data[2]
        frame_type = frame_data[3]
        
        # 检查帧长度
        if len(frame_data) != 3 + frame_len:
            print(f"帧长度不匹配: 期望{3 + frame_len}, 实际{len(frame_data)}")
            return None
        
        # 提取负载和CRC
        payload_end = 3 + frame_len - 2
        payload = frame_data[4:payload_end]
        received_crc = struct.unpack('<H', frame_data[payload_end:payload_end + 2])[0]
        
        # 验证CRC
        crc_data = frame_data[2:payload_end]  # 长度 + 类型 + 负载
        calculated_crc = self.crc16_modbus(crc_data)
        
        if received_crc != calculated_crc:
            print(f"CRC校验失败: 接收{received_crc:04X}, 计算{calculated_crc:04X}")
            return None
        
        # 根据帧类型解析负载
        parsed_payload = self.parse_payload(frame_type, payload)
        
        return {
            'type': frame_type,
            'type_name': self.get_frame_type_name(frame_type),
            'payload': parsed_payload,
            'raw_data': frame_data.hex()
        }
    
    def get_frame_type_name(self, frame_type: int) -> str:
        """获取帧类型名称"""
        type_names = {
            FrameType.RC_COMMAND: "遥控命令",
            FrameType.TELEMETRY: "遥测数据",
            FrameType.HEARTBEAT: "心跳包",
            FrameType.EXT_COMMAND: "扩展命令"
        }
        return type_names.get(frame_type, f"未知类型(0x{frame_type:02X})")
    
    def parse_payload(self, frame_type: int, payload: bytes) -> Dict[str, Any]:
        """解析负载数据"""
        if frame_type == FrameType.RC_COMMAND:
            return self.parse_rc_command(payload)
        elif frame_type == FrameType.TELEMETRY:
            return self.parse_telemetry_data(payload)
        elif frame_type == FrameType.HEARTBEAT:
            return self.parse_heartbeat(payload)
        elif frame_type == FrameType.EXT_COMMAND:
            return self.parse_ext_command(payload)
        else:
            return {'raw_payload': payload.hex()}
    
    def parse_rc_command(self, payload: bytes) -> Dict[str, Any]:
        """解析遥控命令"""
        if len(payload) < 1:
            return {'error': '负载长度不足'}
        
        channel_count = payload[0]
        if len(payload) < 1 + channel_count * 2:
            return {'error': '通道数据长度不足'}
        
        channels = []
        for i in range(channel_count):
            offset = 1 + i * 2
            channel_value = struct.unpack('<H', payload[offset:offset + 2])[0]
            channels.append(channel_value)
        
        return {
            'channel_count': channel_count,
            'channels': channels,
            'throttle': channels[0] if len(channels) > 0 else 0,  # CH1: 油门
            'direction': channels[1] if len(channels) > 1 else 500  # CH2: 方向
        }
    
    def parse_telemetry_data(self, payload: bytes) -> Dict[str, Any]:
        """解析遥测数据"""
        if len(payload) < 14:  # 2+2+2+2+2+4 = 14字节
            return {'error': '遥测数据长度不足'}
        
        # 解析固定格式: 电压(2B) + 电流(2B) + Roll(2B) + Pitch(2B) + Yaw(2B) + 高度(4B)
        voltage_mv, current_ma, roll_raw, pitch_raw, yaw_raw, altitude_cm = struct.unpack('<HHHHHИ', payload[:14])
        
        return {
            'voltage_mv': voltage_mv,
            'voltage_v': voltage_mv / 1000.0,
            'current_ma': current_ma,
            'current_a': current_ma / 1000.0,
            'roll_deg': roll_raw / 100.0,    # 0.01度精度
            'pitch_deg': pitch_raw / 100.0,
            'yaw_deg': yaw_raw / 100.0,
            'altitude_cm': altitude_cm,
            'altitude_m': altitude_cm / 100.0
        }
    
    def parse_heartbeat(self, payload: bytes) -> Dict[str, Any]:
        """解析心跳包"""
        if len(payload) < 1:
            return {'error': '心跳包长度不足'}
        
        device_status = payload[0]
        status_names = {
            DeviceStatus.IDLE: "空闲",
            DeviceStatus.NORMAL: "正常运行",
            DeviceStatus.ERROR: "错误"
        }
        
        return {
            'device_status': device_status,
            'status_name': status_names.get(device_status, f"未知状态(0x{device_status:02X})")
        }
    
    def parse_ext_command(self, payload: bytes) -> Dict[str, Any]:
        """解析扩展命令"""
        if len(payload) < 2:
            return {'error': '扩展命令长度不足'}
        
        cmd_id = payload[0]
        param_len = payload[1]
        
        if len(payload) < 2 + param_len:
            return {'error': '扩展命令参数长度不足'}
        
        params = payload[2:2 + param_len]
        
        cmd_names = {
            ExtCommandID.SET_PWM_FREQ: "设置PWM频率",
            ExtCommandID.MODE_SWITCH: "模式切换",
            ExtCommandID.CALIBRATE_SENSOR: "校准传感器",
            ExtCommandID.REQUEST_TELEMETRY: "请求遥测",
            ExtCommandID.LIGHT_CONTROL: "灯光控制"
        }
        
        return {
            'cmd_id': cmd_id,
            'cmd_name': cmd_names.get(cmd_id, f"未知命令(0x{cmd_id:02X})"),
            'param_len': param_len,
            'params': params.hex() if params else None
        }

class TelemetryServer:
    """遥测数据接收服务器"""
    
    def __init__(self, host='0.0.0.0', port=8080):
        self.host = host
        self.port = port
        self.server_socket = None
        self.client_socket = None
        self.client_address = None
        self.protocol = TelemetryProtocol()
        self.running = False
        self.stats = {
            'total_frames': 0,
            'rc_frames': 0,
            'telemetry_frames': 0,
            'heartbeat_frames': 0,
            'ext_cmd_frames': 0,
            'error_frames': 0
        }
    
    def start_server(self):
        """启动TCP服务器"""
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        try:
            self.server_socket.bind((self.host, self.port))
            self.server_socket.listen(1)
            self.running = True
            
            print(f"🚀 遥测服务器启动成功")
            print(f"📡 监听地址: {self.host}:{self.port}")
            print(f"⏳ 等待ESP32连接...")
            print("-" * 50)
            
            while self.running:
                try:
                    self.client_socket, self.client_address = self.server_socket.accept()
                    print(f"✅ ESP32已连接: {self.client_address}")
                    
                    # 启动接收线程
                    receive_thread = threading.Thread(target=self.receive_data)
                    receive_thread.daemon = True
                    receive_thread.start()
                    
                    # 启动统计显示线程
                    stats_thread = threading.Thread(target=self.show_stats)
                    stats_thread.daemon = True
                    stats_thread.start()
                    
                    receive_thread.join()
                    
                except socket.error as e:
                    if self.running:
                        print(f"❌ 连接错误: {e}")
                        time.sleep(1)
                    
        except Exception as e:
            print(f"❌ 服务器启动失败: {e}")
        finally:
            self.stop_server()
    
    def receive_data(self):
        """接收并处理数据"""
        try:
            while self.running and self.client_socket:
                data = self.client_socket.recv(1024)
                if not data:
                    print("🔌 ESP32断开连接")
                    break
                
                # 添加数据到协议解析器
                self.protocol.add_data(data)
                
                # 解析帧
                frames = self.protocol.parse_frames()
                for frame in frames:
                    self.process_frame(frame)
                    
        except socket.error as e:
            print(f"❌ 接收数据错误: {e}")
        finally:
            if self.client_socket:
                self.client_socket.close()
                self.client_socket = None
    
    def process_frame(self, frame: Dict[str, Any]):
        """处理解析到的帧"""
        self.stats['total_frames'] += 1
        
        frame_type = frame['type']
        frame_name = frame['type_name']
        payload = frame['payload']
        
        # 更新统计
        if frame_type == FrameType.RC_COMMAND:
            self.stats['rc_frames'] += 1
        elif frame_type == FrameType.TELEMETRY:
            self.stats['telemetry_frames'] += 1
        elif frame_type == FrameType.HEARTBEAT:
            self.stats['heartbeat_frames'] += 1
        elif frame_type == FrameType.EXT_COMMAND:
            self.stats['ext_cmd_frames'] += 1
        else:
            self.stats['error_frames'] += 1
        
        # 显示帧信息
        print(f"\n📦 [{frame_name}] 帧")
        if 'error' in payload:
            print(f"  ❌ 错误: {payload['error']}")
            self.stats['error_frames'] += 1
            return
        
        # 根据帧类型显示详细信息
        if frame_type == FrameType.RC_COMMAND:
            self.display_rc_command(payload)
        elif frame_type == FrameType.TELEMETRY:
            self.display_telemetry_data(payload)
        elif frame_type == FrameType.HEARTBEAT:
            self.display_heartbeat(payload)
        elif frame_type == FrameType.EXT_COMMAND:
            self.display_ext_command(payload)
    
    def display_rc_command(self, payload: Dict[str, Any]):
        """显示遥控命令信息"""
        print(f"  🎮 通道数: {payload['channel_count']}")
        print(f"  🚁 油门(CH1): {payload['throttle']}/1000 ({payload['throttle']/10:.1f}%)")
        print(f"  🧭 方向(CH2): {payload['direction']}/1000 ({(payload['direction']-500)/5:.1f}%)")
        
        if len(payload['channels']) > 2:
            for i, ch in enumerate(payload['channels'][2:], 3):
                print(f"  📡 CH{i}: {ch}/1000 ({ch/10:.1f}%)")
    
    def display_telemetry_data(self, payload: Dict[str, Any]):
        """显示遥测数据信息"""
        print(f"  🔋 电压: {payload['voltage_v']:.2f}V ({payload['voltage_mv']}mV)")
        print(f"  ⚡ 电流: {payload['current_a']:.3f}A ({payload['current_ma']}mA)")
        print(f"  🎯 姿态角:")
        print(f"     Roll:  {payload['roll_deg']:+7.2f}°")
        print(f"     Pitch: {payload['pitch_deg']:+7.2f}°")
        print(f"     Yaw:   {payload['yaw_deg']:+7.2f}°")
        print(f"  📏 高度: {payload['altitude_m']:.2f}m ({payload['altitude_cm']}cm)")
    
    def display_heartbeat(self, payload: Dict[str, Any]):
        """显示心跳包信息"""
        status_emoji = {"空闲": "💤", "正常运行": "✅", "错误": "❌"}
        emoji = status_emoji.get(payload['status_name'], "❓")
        print(f"  {emoji} 设备状态: {payload['status_name']} (0x{payload['device_status']:02X})")
    
    def display_ext_command(self, payload: Dict[str, Any]):
        """显示扩展命令信息"""
        print(f"  🔧 命令: {payload['cmd_name']} (ID: 0x{payload['cmd_id']:02X})")
        if payload['params']:
            print(f"  📝 参数: {payload['params']} ({payload['param_len']}字节)")
        else:
            print(f"  📝 无参数")
    
    def show_stats(self):
        """定期显示统计信息"""
        while self.running and self.client_socket:
            time.sleep(10)  # 每10秒显示一次统计
            if self.stats['total_frames'] > 0:
                print(f"\n📊 统计信息 (过去10秒):")
                print(f"  总帧数: {self.stats['total_frames']}")
                print(f"  遥控帧: {self.stats['rc_frames']}")
                print(f"  遥测帧: {self.stats['telemetry_frames']}")
                print(f"  心跳帧: {self.stats['heartbeat_frames']}")
                print(f"  扩展命令: {self.stats['ext_cmd_frames']}")
                print(f"  错误帧: {self.stats['error_frames']}")
                print("-" * 30)
                
                # 重置统计
                self.stats = {key: 0 for key in self.stats}
    
    def stop_server(self):
        """停止服务器"""
        self.running = False
        if self.client_socket:
            self.client_socket.close()
        if self.server_socket:
            self.server_socket.close()
        print("\n🛑 服务器已停止")

def main():
    """主函数"""
    print("=" * 50)
    print("  ESP32 遥测数据接收测试工具")
    print("  基于TCP协议解析遥控和遥测数据")
    print("=" * 50)
    
    # 创建并启动服务器
    server = TelemetryServer(host='0.0.0.0', port=8080)
    
    try:
        server.start_server()
    except KeyboardInterrupt:
        print("\n\n👋 用户中断，正在关闭服务器...")
        server.stop_server()
    except Exception as e:
        print(f"\n❌ 程序异常: {e}")
        server.stop_server()

if __name__ == "__main__":
    main()
