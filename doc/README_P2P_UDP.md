# P2P UDP 图传系统使用指南

## 概述

P2P UDP图传系统是一个高性能的点对点图像传输解决方案，相比传统的TCP图传，具有以下优势：

- **更低延迟**: UDP协议无需建立连接，减少了握手延迟
- **更高吞吐量**: 避免了TCP的拥塞控制开销
- **点对点连接**: 直接Wi-Fi连接，无需外部路由器
- **灵活性**: 支持热点模式和客户端模式
- **可靠传输**: 通过ACK/NACK机制确保数据完整性

## 系统架构

### 协议设计

#### 数据包结构（32字节头部）
```c
typedef struct {
    uint32_t magic;           // 魔数: 0x50325055 ("P2PU")
    uint8_t  packet_type;     // 包类型
    uint8_t  version;         // 协议版本
    uint16_t sequence_num;    // 序列号
    uint32_t frame_id;        // 帧ID
    uint16_t packet_id;       // 包ID
    uint16_t total_packets;   // 总包数
    uint32_t frame_size;      // 帧大小
    uint16_t data_size;       // 数据大小
    uint16_t checksum;        // 校验和
    uint32_t timestamp;       // 时间戳
    uint8_t  reserved[4];     // 保留字段
} p2p_udp_packet_header_t;
```

#### 包类型
- `FRAME_DATA (0x02)`: 图像数据包
- `ACK (0x04)`: 确认包
- `NACK (0x05)`: 否认包（错误重传）

### Wi-Fi 连接模式

#### 热点模式 (AP Mode)
- ESP32创建Wi-Fi热点
- SSID格式: `ESP32_P2P_XXXX` (XXXX为MAC地址后缀)
- 默认密码: `12345678`
- 默认IP: `192.168.4.1`

#### 客户端模式 (STA Mode)
- ESP32连接到现有Wi-Fi热点
- 可手动输入SSID和密码
- 自动获取IP地址

## 使用说明

### ESP32端操作

1. **进入P2P UDP图传界面**
   - 从主菜单选择 "P2P UDP Transfer"

2. **选择工作模式**
   - 切换开关选择"热点模式"或"客户端模式"

3. **启动服务**
   - 点击"启动服务"按钮
   - 等待Wi-Fi连接建立

4. **连接管理**
   - 热点模式：等待客户端连接
   - 客户端模式：输入SSID和密码，点击"连接热点"

### Python客户端使用

#### 安装依赖
```bash
pip install opencv-python numpy
```

#### 基本用法

1. **发送单个图像文件**
```bash
python p2p_udp_client.py --image /path/to/image.jpg --ip 192.168.4.1
```

2. **发送摄像头视频流**
```bash
python p2p_udp_client.py --camera 0 --fps 15 --ip 192.168.4.1
```

3. **发送测试图像**
```bash
python p2p_udp_client.py --ip 192.168.4.1
```

#### 参数说明
- `--ip`: ESP32的IP地址（默认: 192.168.4.1）
- `--port`: UDP端口（默认: 6789）
- `--image`: 图像文件路径（支持JPG、PNG等格式）
- `--camera`: 摄像头索引（0为默认摄像头）
- `--fps`: 摄像头帧率（默认: 10）

## 性能优化

### 网络参数调优

1. **包大小**: 默认1400字节，适合大多数网络环境
2. **帧率控制**: 可根据网络带宽调整发送帧率
3. **质量设置**: JPEG压缩质量可调（75-85推荐）

### ESP32配置

1. **缓冲区大小**: 
   - 最大帧大小: 200KB
   - PSRAM优先分配图像缓冲区

2. **Wi-Fi优化**:
   - 固定信道避免切换延迟
   - 关闭省电模式提高响应速度

## 故障排除

### 常见问题

1. **连接失败**
   - 检查Wi-Fi密码是否正确
   - 确认设备在同一网络
   - 检查防火墙设置

2. **图像传输中断**
   - 网络信号强度不足
   - 数据包丢失率过高
   - 缓冲区溢出

3. **延迟过高**
   - 降低图像分辨率
   - 减少JPEG质量
   - 优化网络环境

### 调试信息

ESP32端会输出详细的调试信息：
- 连接状态变化
- 数据包发送/接收统计
- 错误和重传信息

Python客户端也会显示：
- 传输进度
- 网络统计
- 错误信息

## 扩展功能

### 未来改进方向

1. **自适应质量**: 根据网络状况自动调整图像质量
2. **多播支持**: 一对多图像传输
3. **加密传输**: 增加数据加密保护
4. **双向传输**: 支持ESP32发送图像到客户端

### 自定义协议

可根据具体需求修改协议：
1. 调整包头结构
2. 增加新的包类型
3. 实现应用层的特殊功能

## API参考

### ESP32 C API

```c
// 初始化系统
esp_err_t p2p_udp_image_transfer_init(p2p_connection_mode_t mode, 
                                       p2p_udp_image_callback_t image_callback,
                                       p2p_udp_status_callback_t status_callback);

// 启动/停止服务
esp_err_t p2p_udp_image_transfer_start(void);
void p2p_udp_image_transfer_stop(void);

// 发送图像
esp_err_t p2p_udp_send_image(const uint8_t* jpeg_data, uint32_t jpeg_size);

// 连接管理
esp_err_t p2p_udp_connect_to_ap(const char* ap_ssid, const char* ap_password);
p2p_connection_state_t p2p_udp_get_connection_state(void);

// 统计信息
void p2p_udp_get_stats(uint32_t* tx_packets, uint32_t* rx_packets, 
                       uint32_t* lost_packets, uint32_t* retx_packets);
```

### Python客户端API

```python
# 创建客户端
client = P2PUDPClient(target_ip="192.168.4.1")

# 连接
client.connect()

# 发送图像
client.send_image_file("image.jpg")
client.send_jpeg_data(jpeg_bytes)

# 摄像头流
client.send_camera_stream(camera_index=0, fps=15)

# 断开连接
client.disconnect()
```

## 协议兼容性

当前协议版本: 1.0
- 支持JPEG图像格式
- 最大帧大小: 200KB
- 最大分包数: 约150个
- 端口: 6789 (可配置)

## 许可证

本项目遵循项目主许可证。
