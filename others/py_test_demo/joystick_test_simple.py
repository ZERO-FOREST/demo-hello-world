#!/usr/bin/env python3
"""
ESP32 遥测数据接收工具
专门用于接收和解析ESP32发送的遥测数据（文本格式）
"""

import socket
import struct
import time
from typing import Optional, Dict, Any

class TelemetryReceiver:
    """遥测数据接收器 - 只负责解析"""
    
    def __init__(self, esp32_ip='192.168.97.247', port=6667):  # 改为6667端口
        self.esp32_ip = esp32_ip
        self.port = port
        self.socket = None
        self.connected = False
        self.running = False
    
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
    
    def parse_simple_command(self, data: str) -> Optional[Dict[str, Any]]:
        """解析简单的文本命令格式: CTRL:throttle,direction"""
        try:
            data = data.strip()
            if not data.startswith('CTRL:'):
                return None
            
            # 解析 CTRL:throttle,direction 格式
            params = data[5:].split(',')
            if len(params) != 2:
                return None
                
            throttle = int(params[0])
            direction = int(params[1])
            
            return {
                'throttle': throttle,
                'direction': direction,
                'throttle_percent': throttle / 10.0,
                'direction_offset': (direction - 500) / 5.0
            }
        except:
            return None
    
    def display_control_data(self, cmd_data: Dict[str, Any]):
        """显示控制数据"""
        throttle = cmd_data['throttle']
        direction = cmd_data['direction']
        throttle_percent = cmd_data['throttle_percent']
        direction_offset = cmd_data['direction_offset']
        
        print(f"\n🎮 遥控命令数据:")
        print(f"  🚁 油门: {throttle:4d}/1000 ({throttle_percent:5.1f}%)")
        print(f"  🧭 方向: {direction:4d}/1000 ({direction_offset:+6.1f}%)")
        
        # 显示控制位置图形
        self.draw_control_position(direction_offset, throttle_percent)
    
    def draw_control_position(self, direction_percent: float, throttle_percent: float):
        """绘制控制位置图形"""
        print(f"\n  📍 控制位置图:")
        
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
                    line += "●"  # 控制位置
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
    
    def connect_to_esp32(self):
        """连接到ESP32作为客户端"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10)  # 连接超时
            
            print(f"🚀 遥测接收器启动")
            print(f"📡 正在连接ESP32: {self.esp32_ip}:{self.port}")
            
            self.socket.connect((self.esp32_ip, self.port))
            self.connected = True
            self.running = True
            
            print(f"✅ 已连接到ESP32")
            print("� 发送心跳包...")
            
            # 发送心跳包来触发服务器
            heartbeat = "HEARTBEAT\n"
            self.socket.send(heartbeat.encode('utf-8'))
            print(f"📤 已发送心跳包: {heartbeat.strip()}")
            
            print("� 开始接收数据...")
            print("💡 按 Ctrl+C 退出\n")
            
            # 连接成功后移除超时，改为阻塞接收
            self.socket.settimeout(None)
            
            buffer = ""
            frame_count = 0
            
            while self.running and self.connected:
                try:
                    data = self.socket.recv(1024)
                    if not data:
                        print("🔌 ESP32断开连接")
                        break
                    
                    print(f"📨 接收到 {len(data)} 字节数据: {data[:50]}{'...' if len(data) > 50 else ''}")
                    
                    # 解码为文本
                    try:
                        text_data = data.decode('utf-8')
                        buffer += text_data
                        
                        # 按行分割处理
                        lines = buffer.split('\n')
                        buffer = lines[-1]  # 保留最后不完整的行
                        
                        for line in lines[:-1]:
                            line = line.strip()
                            if line:
                                print(f"📝 处理行: '{line}'")
                                cmd_data = self.parse_simple_command(line)
                                if cmd_data:
                                    frame_count += 1
                                    print(f"📦 命令 #{frame_count:04d} {time.strftime('%H:%M:%S')}")
                                    self.display_control_data(cmd_data)
                                    print("─" * 50)
                                else:
                                    print(f"❓ 无法解析的数据: '{line}'")
                    
                    except UnicodeDecodeError as e:
                        print(f"❌ 文本解码错误: {e}")
                        print(f"   原始数据: {data.hex()}")
                        continue
                        
                except socket.timeout:
                    continue  # 继续等待
                except socket.error as e:
                    print(f"❌ 接收数据错误: {e}")
                    break
                
        except socket.timeout:
            print(f"❌ 连接超时: {self.esp32_ip}:{self.port}")
            print("💡 请检查ESP32是否在线并且遥测服务已启动")
        except socket.error as e:
            print(f"❌ 连接失败: {e}")
            print("💡 请检查网络连接和ESP32 IP地址")
        except KeyboardInterrupt:
            print("\n👋 用户中断")
        except Exception as e:
            print(f"❌ 错误: {e}")
        finally:
            self.stop()
    
    def stop(self):
        """停止接收器"""
        self.running = False
        self.connected = False
        if self.socket:
            self.socket.close()
        print("🛑 接收器已停止")

def main():
    """主函数"""
    print("=" * 50)
    print("   ESP32 遥测数据接收工具")
    print("   专门接收和解析遥测数据")
    print("=" * 50)
    
    # 获取ESP32 IP地址
    esp32_ip = input("请输入ESP32 IP地址 (默认: 192.168.97.247): ").strip()
    if not esp32_ip:
        esp32_ip = "192.168.97.247"
    
    receiver = TelemetryReceiver(esp32_ip=esp32_ip)
    
    try:
        receiver.connect_to_esp32()
    except KeyboardInterrupt:
        print("\n👋 测试结束")
    except Exception as e:
        print(f"❌ 程序异常: {e}")

if __name__ == "__main__":
    main()
