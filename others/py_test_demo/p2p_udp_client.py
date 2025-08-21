#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P2P UDP图传客户端测试脚本
用于测试ESP32的P2P UDP图传功能
"""

import socket
import struct
import time
import cv2
import numpy as np
import argparse
import threading
from pathlib import Path

# 协议定义（与ESP32保持一致）
P2P_UDP_MAGIC = 0x50325055  # "P2PU"
P2P_UDP_PORT = 6789
P2P_UDP_MAX_PACKET_SIZE = 1400
P2P_UDP_HEADER_SIZE = 32

# 数据包类型
PACKET_TYPE_FRAME_DATA = 0x02
PACKET_TYPE_ACK = 0x04
PACKET_TYPE_NACK = 0x05

class P2PUDPClient:
    def __init__(self, target_ip="192.168.4.1", target_port=P2P_UDP_PORT):
        """
        初始化P2P UDP客户端
        
        Args:
            target_ip: 目标IP地址（ESP32的IP）
            target_port: 目标端口
        """
        self.target_ip = target_ip
        self.target_port = target_port
        self.socket = None
        self.running = False
        self.frame_id = 0
        self.stats = {
            'tx_packets': 0,
            'rx_packets': 0,
            'acks': 0,
            'nacks': 0,
            'timeouts': 0
        }
        
    def connect(self):
        """建立UDP连接"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.socket.settimeout(1.0)  # 1秒超时
            print(f"UDP客户端已创建，目标: {self.target_ip}:{self.target_port}")
            return True
        except Exception as e:
            print(f"创建UDP socket失败: {e}")
            return False
    
    def disconnect(self):
        """断开连接"""
        self.running = False
        if self.socket:
            self.socket.close()
            self.socket = None
            print("UDP连接已关闭")
    
    def create_packet_header(self, packet_type, packet_id, total_packets, 
                           frame_size, data_size, frame_id=None):
        """
        创建数据包头部
        
        Returns:
            bytes: 32字节的包头数据
        """
        if frame_id is None:
            frame_id = self.frame_id
            
        # 计算简单校验和（用于演示）
        checksum = (frame_id + packet_id + data_size) & 0xFFFF
        
        # 获取当前时间戳（毫秒）
        timestamp = int(time.time() * 1000) & 0xFFFFFFFF
        
        # 打包头部数据（32字节）
        header = struct.pack('<LBBHHLHHLHL4s',
            P2P_UDP_MAGIC,      # magic (4 bytes)
            packet_type,        # packet_type (1 byte)
            1,                  # version (1 byte)
            packet_id,          # sequence_num (2 bytes)
            frame_id,           # frame_id (4 bytes)
            packet_id,          # packet_id (2 bytes)
            total_packets,      # total_packets (2 bytes)
            frame_size,         # frame_size (4 bytes)
            data_size,          # data_size (2 bytes)
            checksum,           # checksum (2 bytes)
            timestamp,          # timestamp (4 bytes)
            b'\x00' * 4         # reserved (4 bytes)
        )
        
        return header
    
    def send_image_file(self, image_path):
        """
        发送图像文件
        
        Args:
            image_path: 图像文件路径
        """
        if not self.socket:
            print("未连接到服务器")
            return False
            
        # 读取并编码图像
        try:
            # 如果是JPEG文件，直接读取
            if str(image_path).lower().endswith(('.jpg', '.jpeg')):
                with open(image_path, 'rb') as f:
                    jpeg_data = f.read()
            else:
                # 其他格式先用OpenCV读取再编码为JPEG
                img = cv2.imread(str(image_path))
                if img is None:
                    print(f"无法读取图像: {image_path}")
                    return False
                
                # 调整图像大小（如果太大）
                height, width = img.shape[:2]
                max_size = 800
                if max(height, width) > max_size:
                    scale = max_size / max(height, width)
                    new_width = int(width * scale)
                    new_height = int(height * scale)
                    img = cv2.resize(img, (new_width, new_height))
                    print(f"图像已调整大小: {width}x{height} -> {new_width}x{new_height}")
                
                # 编码为JPEG
                encode_param = [int(cv2.IMWRITE_JPEG_QUALITY), 85]
                _, jpeg_data = cv2.imencode('.jpg', img, encode_param)
                jpeg_data = jpeg_data.tobytes()
            
            print(f"JPEG数据大小: {len(jpeg_data)} 字节")
            return self.send_jpeg_data(jpeg_data)
            
        except Exception as e:
            print(f"处理图像文件失败: {e}")
            return False
    
    def send_jpeg_data(self, jpeg_data):
        """
        发送JPEG数据
        
        Args:
            jpeg_data: JPEG数据字节
        """
        if not self.socket:
            print("未连接到服务器")
            return False
        
        # 计算需要的数据包数量
        payload_size = P2P_UDP_MAX_PACKET_SIZE - P2P_UDP_HEADER_SIZE
        total_packets = (len(jpeg_data) + payload_size - 1) // payload_size
        
        print(f"发送图像: {len(jpeg_data)} 字节，分为 {total_packets} 个数据包")
        
        self.frame_id += 1
        success_packets = 0
        
        # 发送所有数据包
        for packet_id in range(total_packets):
            # 计算当前包的数据
            offset = packet_id * payload_size
            current_data_size = min(payload_size, len(jpeg_data) - offset)
            packet_data = jpeg_data[offset:offset + current_data_size]
            
            # 创建包头
            header = self.create_packet_header(
                PACKET_TYPE_FRAME_DATA,
                packet_id,
                total_packets,
                len(jpeg_data),
                current_data_size
            )
            
            # 组合完整数据包
            full_packet = header + packet_data
            
            try:
                # 发送数据包
                sent_bytes = self.socket.sendto(full_packet, (self.target_ip, self.target_port))
                self.stats['tx_packets'] += 1
                
                if sent_bytes == len(full_packet):
                    success_packets += 1
                    print(f"包 {packet_id + 1}/{total_packets} 发送成功 ({current_data_size} 字节)")
                else:
                    print(f"包 {packet_id + 1}/{total_packets} 发送不完整")
                
                # 添加小延迟避免网络拥塞
                time.sleep(0.001)
                
            except Exception as e:
                print(f"发送包 {packet_id + 1}/{total_packets} 失败: {e}")
        
        print(f"图像发送完成: {success_packets}/{total_packets} 包成功")
        return success_packets == total_packets
    
    def send_camera_stream(self, camera_index=0, fps=10):
        """
        发送摄像头视频流
        
        Args:
            camera_index: 摄像头索引
            fps: 帧率
        """
        if not self.socket:
            print("未连接到服务器")
            return
        
        cap = cv2.VideoCapture(camera_index)
        if not cap.isOpened():
            print(f"无法打开摄像头 {camera_index}")
            return
        
        # 设置摄像头参数
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
        cap.set(cv2.CAP_PROP_FPS, fps)
        
        print(f"开始摄像头流传输，帧率: {fps} FPS")
        print("按 'q' 键退出")
        
        self.running = True
        frame_interval = 1.0 / fps
        last_frame_time = 0
        
        try:
            while self.running:
                current_time = time.time()
                
                # 控制帧率
                if current_time - last_frame_time < frame_interval:
                    time.sleep(0.01)
                    continue
                
                ret, frame = cap.read()
                if not ret:
                    print("读取摄像头帧失败")
                    break
                
                # 编码为JPEG
                encode_param = [int(cv2.IMWRITE_JPEG_QUALITY), 75]
                _, jpeg_data = cv2.imencode('.jpg', frame, encode_param)
                jpeg_data = jpeg_data.tobytes()
                
                # 发送JPEG数据
                self.send_jpeg_data(jpeg_data)
                
                last_frame_time = current_time
                
                # 显示帧（可选）
                cv2.imshow('Camera Stream', frame)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
                    
        except KeyboardInterrupt:
            print("\n用户中断")
        finally:
            self.running = False
            cap.release()
            cv2.destroyAllWindows()
            print("摄像头流传输结束")
    
    def print_stats(self):
        """打印统计信息"""
        print("\n=== 传输统计 ===")
        print(f"发送包数: {self.stats['tx_packets']}")
        print(f"接收包数: {self.stats['rx_packets']}")
        print(f"ACK数: {self.stats['acks']}")
        print(f"NACK数: {self.stats['nacks']}")
        print(f"超时数: {self.stats['timeouts']}")

def main():
    parser = argparse.ArgumentParser(description='P2P UDP图传客户端')
    parser.add_argument('--ip', default='192.168.4.1', help='ESP32 IP地址')
    parser.add_argument('--port', type=int, default=P2P_UDP_PORT, help='目标端口')
    parser.add_argument('--image', help='要发送的图像文件路径')
    parser.add_argument('--camera', type=int, help='摄像头索引（启用摄像头流）')
    parser.add_argument('--fps', type=int, default=10, help='摄像头帧率')
    
    args = parser.parse_args()
    
    # 创建客户端
    client = P2PUDPClient(args.ip, args.port)
    
    if not client.connect():
        return
    
    try:
        if args.image:
            # 发送单个图像文件
            image_path = Path(args.image)
            if not image_path.exists():
                print(f"图像文件不存在: {image_path}")
                return
            
            print(f"发送图像文件: {image_path}")
            client.send_image_file(image_path)
            
        elif args.camera is not None:
            # 启动摄像头流
            client.send_camera_stream(args.camera, args.fps)
            
        else:
            # 发送测试图像
            print("生成测试图像...")
            test_img = np.zeros((480, 640, 3), dtype=np.uint8)
            
            # 绘制一些测试图案
            cv2.rectangle(test_img, (50, 50), (590, 430), (0, 255, 0), 2)
            cv2.circle(test_img, (320, 240), 100, (255, 0, 0), -1)
            cv2.putText(test_img, f'Test Frame {int(time.time())}', 
                       (100, 100), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)
            
            # 编码为JPEG
            encode_param = [int(cv2.IMWRITE_JPEG_QUALITY), 85]
            _, jpeg_data = cv2.imencode('.jpg', test_img, encode_param)
            jpeg_data = jpeg_data.tobytes()
            
            print("发送测试图像...")
            client.send_jpeg_data(jpeg_data)
        
    except KeyboardInterrupt:
        print("\n用户中断")
    finally:
        client.print_stats()
        client.disconnect()

if __name__ == "__main__":
    main()
