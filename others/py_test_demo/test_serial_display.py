'''
Author: tidycraze 2595256284@qq.com
Date: 2025-08-13 09:23:25
LastEditors: tidycraze 2595256284@qq.com
LastEditTime: 2025-08-18 15:30:15
FilePath: \demo-hello-world\others\py_test_demo\test_serial_display.py
Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
'''
#!/usr/bin/env python3
"""
串口显示功能测试脚本
用于快速测试ESP32的串口显示功能
"""

import socket
import time
import sys

def test_serial_display(host, port):
    """测试串口显示功能"""
    print(f"连接到 {host}:{port}")
    
    try:
        # 创建socket连接
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect((host, port))
        print("连接成功！")
        
        # 发送测试数据
        test_messages = [
            "Hello World!",
            "这是一个中文测试",
            "Temperature: 25.6°C",
            "Humidity: 65%",
            "Status: OK",
            "Counter: 1234",
            "Time: " + time.strftime("%H:%M:%S"),
            "Date: " + time.strftime("%Y-%m-%d"),
            "ESP32 WiFi TCP -> Serial Screen",
            "测试完成！"
        ]
        
        for i, message in enumerate(test_messages, 1):
            print(f"发送消息 {i}: {message}")
            data = (message + "\n").encode('utf-8')
            sock.send(data)
            time.sleep(0.01)  # 等待1秒
        
        print("所有测试消息已发送")
        
        # 等待一段时间让数据完全传输
        time.sleep(2)
        
    except socket.timeout:
        print("连接超时")
        return False
    except ConnectionRefusedError:
        print("连接被拒绝，请检查ESP32是否正在运行串口显示服务")
        return False
    except Exception as e:
        print(f"连接失败: {e}")
        return False
    finally:
        try:
            sock.close()
        except:
            pass
    
    return True

def main():
    if len(sys.argv) != 3:
        print("使用方法: python3 test_serial_display.py <ESP32_IP> <PORT>")
        print("示例: python3 test_serial_display.py 192.168.1.100 8080")
        sys.exit(1)
    
    host = sys.argv[1]
    port = int(sys.argv[2])
    
    print("串口显示功能测试")
    print("=" * 50)
    
    if test_serial_display(host, port):
        print("测试完成！")
        print("请检查：")
        print("1. ESP32的串口屏幕是否显示了测试消息")
        print("2. LVGL界面是否显示了带时间戳的消息")
        print("3. 消息是否正确换行和格式化")
    else:
        print("测试失败！")
        sys.exit(1)

if __name__ == "__main__":
    main()
