import socket
import os
from tkinter import Tk, filedialog
import sys

ESP32_IP = '192.168.97.247'  # 修改为你的 ESP32 IP 地址
ESP32_PORT = 6556           # ESP32 监听的端口

def select_image_file():
    """
    弹出文件选择对话框，选择要发送的图片文件
    返回图片文件路径
    """
    root = Tk()
    root.withdraw()  # 隐藏主窗口
    file_path = filedialog.askopenfilename(
        title="Select an image file",
        filetypes=[("JPEG files", "*.jpg"), ("All files", "*.*")])
    root.destroy()  # 释放资源，防止窗口阻塞
    return file_path

def send_image_to_esp32():
    """
    主动连接 ESP32 并发送图片，支持 Ctrl+C 退出
    """
    try:
        while True:
            # 选择图片文件
            image_path = select_image_file()
            if not image_path:
                print("No image selected. Exiting.")
                break

            if not os.path.exists(image_path):
                print(f"File not found: {image_path}")
                continue

            print(f"Connecting to ESP32 {ESP32_IP}:{ESP32_PORT} ...")
            try:
                # 创建 TCP socket 并连接 ESP32
                with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                    s.connect((ESP32_IP, ESP32_PORT))
                    print(f"Connected. Sending image: {image_path}")
                    
                    # 读取图片数据
                    with open(image_path, 'rb') as f:
                        image_data = f.read()
                    
                    # 发送图片数据到 ESP32
                    s.sendall(image_data)
                    print(f"Sent {len(image_data)} bytes.")
                    print("Image sent. Waiting for next image selection or exit.")
        
            except Exception as e:
                print(f"Error: {e}")
                break # 连接或发送过程中出错则退出
    except KeyboardInterrupt:
        print("\n用户按下 Ctrl+C，程序已退出。")
        sys.exit(0)

if __name__ == "__main__":
    send_image_to_esp32()