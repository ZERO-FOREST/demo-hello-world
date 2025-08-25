#!/usr/bin/env python3
"""
简化版摇杆遥控数据测试脚本
专门用于测试摇杆转换的遥控命令
"""

import socket
import struct
import time
from typing import Optional, Dict, Any

class SimpleJoystickTester:
    """简单摇杆测试器"""
    
    FRAME_HEADER = b'\xAA\x55'
    
    def __init__(self, host='0.0.0.0', port=8080):
        self.host = host
        self.port = port
        self.socket = None
        self.client = None
        
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
    
    def parse_rc_frame(self, data: bytes) -> Optional[Dict[str, Any]]:
        """解析遥控命令帧"""
        if len(data) < 6 or data[:2] != self.FRAME_HEADER:
            return None
            
        frame_len = data[2]
        frame_type = data[3]
        
        if frame_type != 0x01:  # 只处理遥控命令
            return None
            
        if len(data) < 3 + frame_len:
            return None
        
        # 提取负载
        payload_end = 3 + frame_len - 2
        payload = data[4:payload_end]
        
        # 验证CRC
        crc_data = data[2:payload_end]
        received_crc = struct.unpack('<H', data[payload_end:payload_end + 2])[0]
        calculated_crc = self.crc16_modbus(crc_data)
        
        if received_crc != calculated_crc:
            print(f"❌ CRC错误: 接收{received_crc:04X} vs 计算{calculated_crc:04X}")
            return None
        
        # 解析遥控命令
        if len(payload) < 1:
            return None
            
        channel_count = payload[0]
        channels = []
        
        for i in range(min(channel_count, (len(payload) - 1) // 2)):
            offset = 1 + i * 2
            if offset + 2 <= len(payload):
                channel_value = struct.unpack('<H', payload[offset:offset + 2])[0]
                channels.append(channel_value)
        
        return {
            'channel_count': channel_count,
            'channels': channels,
            'raw_data': data.hex()
        }
    
    def display_joystick_data(self, rc_data: Dict[str, Any]):
        """显示摇杆数据"""
        channels = rc_data['channels']
        
        print(f"\n🎮 摇杆遥控数据:")
        print(f"  通道数: {rc_data['channel_count']}")
        
        if len(channels) >= 2:
            throttle = channels[0]  # CH1: 油门 (摇杆Y轴)
            direction = channels[1]  # CH2: 方向 (摇杆X轴)
            
            # 转换为百分比和方向
            throttle_percent = throttle / 10.0  # 0-1000 -> 0-100%
            direction_offset = (direction - 500) / 5.0  # 500为中位，±100%
            
            print(f"  🚁 油门(Y轴): {throttle:4d}/1000 ({throttle_percent:5.1f}%)")
            print(f"  🧭 方向(X轴): {direction:4d}/1000 ({direction_offset:+6.1f}%)")
            
            # 显示摇杆位置图形
            self.draw_joystick_position(direction_offset, throttle_percent)
            
            # 显示其他通道
            for i, ch in enumerate(channels[2:], 3):
                ch_percent = ch / 10.0
                print(f"  📡 CH{i}: {ch:4d}/1000 ({ch_percent:5.1f}%)")
        else:
            print("  ❌ 通道数不足")
    
    def draw_joystick_position(self, direction_percent: float, throttle_percent: float):
        """绘制摇杆位置图形"""
        print(f"\n  📍 摇杆位置图:")
        
        # 限制范围
        x = max(-100, min(100, direction_percent))  # 方向 (-100% 到 +100%)
        y = max(0, min(100, throttle_percent))      # 油门 (0% 到 100%)
        
        # 转换为图形坐标 (21x11的网格)
        grid_x = int((x + 100) / 200 * 20)  # 0-20
        grid_y = int((100 - y) / 100 * 10)  # 0-10 (翻转Y轴)
        
        for row in range(11):
            line = "    "
            for col in range(21):
                if row == grid_y and col == grid_x:
                    line += "●"  # 摇杆位置
                elif col == 10:  # 中心线
                    line += "|"
                elif row == 10:  # 底线
                    line += "-"
                else:
                    line += " "
            
            # 添加标签
            if row == 0:
                line += "  100% 油门"
            elif row == 5:
                line += "   50% 油门"
            elif row == 10:
                line += "    0% 油门"
            
            print(line)
        
        print("    ←-100%  中心  +100%→ 方向")
    
    def start_listening(self):
        """开始监听ESP32数据"""
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        try:
            self.socket.bind((self.host, self.port))
            self.socket.listen(1)
            
            print(f"🚀 摇杆测试服务器启动")
            print(f"📡 监听: {self.host}:{self.port}")
            print(f"⏳ 等待ESP32连接...")
            print("=" * 50)
            
            self.client, addr = self.socket.accept()
            print(f"✅ ESP32已连接: {addr}")
            print("🎮 开始接收摇杆数据...\n")
            
            buffer = bytearray()
            frame_count = 0
            
            while True:
                data = self.client.recv(1024)
                if not data:
                    print("🔌 ESP32断开连接")
                    break
                
                buffer.extend(data)
                
                # 查找并解析帧
                while len(buffer) >= 6:
                    # 查找帧头
                    header_pos = buffer.find(self.FRAME_HEADER)
                    if header_pos == -1:
                        buffer.clear()
                        break
                    
                    if header_pos > 0:
                        buffer = buffer[header_pos:]
                    
                    if len(buffer) < 4:
                        break
                    
                    frame_len = buffer[2]
                    total_len = 3 + frame_len
                    
                    if len(buffer) < total_len:
                        break
                    
                    # 提取帧
                    frame_data = bytes(buffer[:total_len])
                    buffer = buffer[total_len:]
                    
                    # 解析遥控帧
                    rc_data = self.parse_rc_frame(frame_data)
                    if rc_data:
                        frame_count += 1
                        print(f"📦 帧 #{frame_count:04d} {time.strftime('%H:%M:%S')}")
                        self.display_joystick_data(rc_data)
                        print("─" * 50)
                
        except KeyboardInterrupt:
            print("\n👋 用户中断")
        except Exception as e:
            print(f"❌ 错误: {e}")
        finally:
            self.stop()
    
    def stop(self):
        """停止服务器"""
        if self.client:
            self.client.close()
        if self.socket:
            self.socket.close()
        print("🛑 服务器已停止")

def main():
    """主函数"""
    print("=" * 50)
    print("   ESP32 摇杆数据测试工具")
    print("   专门测试摇杆→遥控命令转换")
    print("=" * 50)
    
    tester = SimpleJoystickTester()
    
    try:
        tester.start_listening()
    except KeyboardInterrupt:
        print("\n👋 测试结束")
    except Exception as e:
        print(f"❌ 程序异常: {e}")

if __name__ == "__main__":
    main()
