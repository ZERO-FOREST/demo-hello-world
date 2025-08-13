# 串口显示功能说明

## 功能概述

串口显示功能允许通过WiFi TCP连接接收数据，并将数据显示在串口屏幕和LVGL界面上。该功能支持：

- 通过WiFi TCP接收数据
- 自动换行处理
- 时间戳显示
- 最大保存1024行数据
- 自动滚动和手动滚动
- 清空显示功能

## 硬件连接

### 串口连接
- UART1 TX: GPIO17
- UART1 RX: GPIO16
- 波特率: 115200

### 串口屏幕
连接串口屏幕到ESP32的UART1接口，确保波特率设置为115200。

## 软件架构

### 核心模块

1. **serial_display.c/h** - 串口显示核心模块
   - TCP服务器功能
   - 串口数据发送
   - 数据缓冲管理

2. **ui_serial_display.c/h** - 串口显示UI界面
   - LVGL界面显示
   - 数据行管理
   - 滚动控制

3. **serial_display_demo.c/h** - 演示程序
   - 完整功能演示
   - WiFi连接管理

### 数据流程

```
WiFi TCP客户端 -> ESP32 TCP服务器 -> 数据缓冲区 -> 串口发送 + UI显示
```

## 使用方法

### 1. 编译和烧录

确保在项目中包含了所有必要的文件：

```bash
# 编译项目
idf.py build

# 烧录到ESP32
idf.py flash monitor
```

### 2. 配置WiFi

在`sdkconfig`中配置WiFi信息：

```
CONFIG_ESP_WIFI_SSID="your_wifi_ssid"
CONFIG_ESP_WIFI_PASSWORD="your_wifi_password"
```

### 3. 启动功能

在主菜单中选择"Serial Display"选项，系统将：

1. 初始化串口显示模块
2. 启动TCP服务器（默认端口8080）
3. 显示串口显示界面
4. 等待TCP连接

### 4. 连接客户端

使用提供的Python客户端程序连接：

```bash
# 交互模式
python3 serial_display_client.py <ESP32_IP> 8080

# 演示模式
python3 serial_display_client.py <ESP32_IP> 8080 -d

# 发送单条文本
python3 serial_display_client.py <ESP32_IP> 8080 -t "Hello World"

# 从文件发送
python3 serial_display_client.py <ESP32_IP> 8080 -f data.txt
```

### 5. 使用其他客户端

也可以使用其他TCP客户端工具：

```bash
# 使用netcat
echo "Hello World" | nc <ESP32_IP> 8080

# 使用telnet
telnet <ESP32_IP> 8080
```

## 界面功能

### 主界面元素

- **标题**: "Serial Display"
- **状态栏**: 显示当前行数和自动滚动状态
- **文本区域**: 显示接收到的数据，带时间戳
- **返回按钮**: 返回主菜单
- **清空按钮**: 清空所有显示数据

### 操作说明

1. **自动滚动**: 默认开启，新数据会自动滚动到最新位置
2. **手动滚动**: 触摸滚动文本区域可关闭自动滚动，手动查看历史数据
3. **清空数据**: 点击"Clear"按钮清空所有显示的数据
4. **返回主菜单**: 点击"Back"按钮返回主菜单

## 数据格式

### 接收数据格式

- 支持任意文本数据
- 自动按换行符分割为多行
- 每行最大长度256字符
- 自动添加时间戳

### 显示格式

```
[14:30:25] 第一行数据
[14:30:26] 第二行数据
[14:30:27] 第三行数据
...
```

## 配置选项

### 串口配置

在`serial_display.c`中可以修改串口配置：

```c
#define UART_NUM UART_NUM_1
#define UART_TX_PIN GPIO_NUM_17
#define UART_RX_PIN GPIO_NUM_16
#define UART_BAUD_RATE 115200
```

### TCP服务器配置

```c
#define TCP_RECV_BUF_SIZE 1024
#define MAX_DISPLAY_DATA_SIZE 4096
```

### 显示配置

在`ui_serial_display.c`中可以修改显示配置：

```c
#define MAX_LINES 1024          // 最大保存行数
#define MAX_LINE_LENGTH 256     // 每行最大长度
#define MAX_DISPLAY_LINES 20    // 界面显示行数
```

## 故障排除

### 常见问题

1. **无法连接TCP服务器**
   - 检查ESP32是否已连接到WiFi
   - 确认IP地址和端口号正确
   - 检查防火墙设置

2. **串口屏幕无显示**
   - 检查串口连接是否正确
   - 确认波特率设置
   - 检查串口屏幕是否正常工作

3. **UI界面无数据显示**
   - 确认串口显示界面已正确创建
   - 检查消息队列是否正常工作
   - 查看串口日志确认数据接收

### 调试信息

启用调试日志：

```c
// 在代码中查看日志
ESP_LOGI(TAG, "Received %d bytes from TCP", len);
ESP_LOGI(TAG, "Data buffered for serial transmission");
```

## 扩展功能

### 添加新功能

1. **数据过滤**: 在`ui_serial_display_add_data`中添加过滤逻辑
2. **数据导出**: 添加导出功能，将数据保存到文件
3. **数据搜索**: 添加搜索功能，快速定位特定数据
4. **多端口支持**: 扩展支持多个TCP端口

### 自定义显示

1. **修改时间戳格式**: 在`get_timestamp_str`函数中修改格式
2. **自定义颜色**: 修改LVGL样式设置
3. **添加图标**: 在界面中添加状态图标

## 性能优化

### 内存使用

- 当前配置使用约1MB内存（1024行 × 256字节）
- 可根据需要调整`MAX_LINES`和`MAX_LINE_LENGTH`

### 响应速度

- 使用消息队列确保UI响应性
- 自动滚动优化显示性能
- 缓冲区管理避免内存泄漏

## 许可证

本项目采用MIT许可证，详见LICENSE文件。
