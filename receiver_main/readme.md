该文件夹用于存放接收端的相关工程文件

## 功能

### 图片编码

使用ESP-ADF的 `new_jpeg` 库进行实时解编码。

### 协议解析

实现TCP协议的解码和数据流处理，支持：
- 遥控命令解析
- 心跳包处理  
- 扩展命令解析
- CRC16校验

### PWM输出

### 原生协议输出

## 通信方式

支持USB和SPI通信

## 编译方式

### ESP-IDF编译

在项目根目录下：

```bash
# 编译接收端模式
idf.py -D EN_RECEIVER_MODE=ON build

# 烧录到ESP32
idf.py -D EN_RECEIVER_MODE=ON flash monitor

# 编译普通模式（默认main文件夹）
idf.py build
```

### 配置说明

- `EN_RECEIVER_MODE=ON`: 编译Receiver文件夹中的代码
- `EN_RECEIVER_MODE=OFF` 或不设置: 编译main文件夹中的代码（默认）

### 项目结构

```
Receiver/
├── CMakeLists.txt         # ESP-IDF组件配置
├── inc/                   # 头文件
│   ├── tcp_protocol.h    # 协议定义
│   └── tcp_client.h      # 客户端接口  
├── src/                   # 源文件
│   ├── tcp_protocol.c    # 协议实现
│   ├── tcp_client.c      # 客户端实现
│   └── main.c            # 主程序入口
└── readme.md             # 说明文档
```

