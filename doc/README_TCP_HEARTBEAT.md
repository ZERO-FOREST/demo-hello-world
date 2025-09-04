# TCP心跳机制实现说明

## 概述

本项目为ESP32接收端实现了完整的TCP心跳机制，严格遵循协议文档规范，提供了可靠的网络连接管理和自动恢复功能。

## 核心特性

### 1. 协议规范遵循
- 严格按照 `doc/协议文档.md` 中定义的帧格式实现
- 支持心跳包类型 (0x03) 和设备状态指示
- 完整的CRC16校验和帧结构验证

### 2. 灵活的端口配置
- 服务端口通过宏定义 `TCP_HEARTBEAT_SERVER_PORT` 配置
- 默认端口：7878
- 支持运行时自定义服务器IP和端口

### 3. 精确的定时控制
- 心跳发送间隔：30秒（固定）
- 自动重连间隔：5秒
- 使用FreeRTOS定时器确保精确控制

### 4. 完善的连接管理
- 网络异常自动检测
- 智能重连机制
- 连接状态实时监控
- 资源自动释放保障

### 5. 详细的日志记录
- 心跳发送记录（包含时间戳和连接信息）
- 连接状态变化日志
- 错误和异常情况记录
- 统计信息跟踪

## 文件结构

```
components/Receiver/
├── inc/
│   ├── tcp_heartbeat_manager.h      # 心跳管理器头文件
│   └── tcp_client_with_heartbeat.h  # 集成客户端头文件
├── src/
│   ├── tcp_heartbeat_manager.c      # 心跳管理器实现
│   ├── tcp_client_with_heartbeat.c  # 集成客户端实现
│   └── tcp_heartbeat_example.c      # 使用示例
└── README_TCP_HEARTBEAT.md          # 本说明文档
```

## 核心组件说明

### 1. TCP心跳管理器 (tcp_heartbeat_manager)

**主要功能：**
- 心跳包创建和发送
- TCP连接建立和维护
- 自动重连机制
- 设备状态管理
- 统计信息收集

**关键配置：**
```c
#define TCP_HEARTBEAT_SERVER_PORT 7878        // 服务端口
#define TCP_HEARTBEAT_INTERVAL_MS 30000       // 心跳间隔(30秒)
#define TCP_HEARTBEAT_RECONNECT_INTERVAL_MS 5000  // 重连间隔(5秒)
```

### 2. 集成TCP客户端 (tcp_client_with_heartbeat)

**主要功能：**
- 统一管理心跳和遥测功能
- 兼容原有TCP客户端接口
- 提供简化的API接口
- 任务管理和状态监控

## 使用方法

### 1. 基本使用（替代原tcp_client_task）

```c
#include "tcp_client_with_heartbeat.h"

void app_main(void) {
    // 初始化并启动TCP心跳客户端
    if (tcp_client_hb_init(NULL, 0) && tcp_client_hb_start()) {
        // 启动处理任务
        tcp_client_hb_start_task("HeartbeatClient", 4096, 5);
        ESP_LOGI("APP", "TCP心跳客户端启动成功");
    }
}
```

### 2. 自定义配置使用

```c
// 使用自定义服务器
const char *server_ip = "192.168.1.100";
uint16_t server_port = 8080;

if (tcp_client_hb_init(server_ip, server_port)) {
    // 设置设备状态
    tcp_client_hb_set_device_status(DEVICE_STATUS_RUNNING);
    
    // 启动客户端
    tcp_client_hb_start();
}
```

### 3. 状态监控

```c
// 获取客户端状态
tcp_client_hb_status_t status = tcp_client_hb_get_status();

// 检查连接健康状态
if (!tcp_client_hb_is_connection_healthy()) {
    ESP_LOGW("APP", "连接异常");
}

// 获取统计信息
const tcp_client_hb_stats_t *stats = tcp_client_hb_get_stats();
ESP_LOGI("APP", "心跳发送次数: %lu", stats->heartbeat_stats->heartbeat_sent_count);
```

### 4. 手动控制

```c
// 立即发送心跳
tcp_client_hb_send_heartbeat_now();

// 立即重连
tcp_client_hb_reconnect_now();

// 设置自动重连
tcp_client_hb_set_auto_reconnect(true);
```

## API参考

### 初始化和控制

| 函数 | 说明 |
|------|------|
| `tcp_client_hb_init()` | 初始化客户端 |
| `tcp_client_hb_start()` | 启动客户端 |
| `tcp_client_hb_stop()` | 停止客户端 |
| `tcp_client_hb_destroy()` | 销毁客户端 |

### 状态监控

| 函数 | 说明 |
|------|------|
| `tcp_client_hb_get_status()` | 获取客户端状态 |
| `tcp_client_hb_is_connection_healthy()` | 检查连接健康状态 |
| `tcp_client_hb_get_stats()` | 获取统计信息 |
| `tcp_client_hb_print_status()` | 打印状态信息 |

### 设备管理

| 函数 | 说明 |
|------|------|
| `tcp_client_hb_set_device_status()` | 设置设备状态 |
| `tcp_client_hb_get_device_status()` | 获取设备状态 |
| `tcp_client_hb_send_heartbeat_now()` | 立即发送心跳 |
| `tcp_client_hb_reconnect_now()` | 立即重连 |

## 配置参数

### 网络配置
```c
#define TCP_HEARTBEAT_SERVER_IP "192.168.97.247"  // 默认服务器IP
#define TCP_HEARTBEAT_SERVER_PORT 7878            // 服务端口
```

### 定时配置
```c
#define TCP_HEARTBEAT_INTERVAL_MS 30000           // 心跳间隔(30秒)
#define TCP_HEARTBEAT_RECONNECT_INTERVAL_MS 5000  // 重连间隔(5秒)
#define TCP_HEARTBEAT_CONNECT_TIMEOUT_MS 10000    // 连接超时(10秒)
```

### 任务配置
```c
#define TCP_HEARTBEAT_TASK_STACK_SIZE 4096        // 任务栈大小
#define TCP_HEARTBEAT_TASK_PRIORITY 5             // 任务优先级
```

## 日志输出示例

```
I (12345) TCP_HB_MGR: 心跳管理器初始化成功
I (12346) TCP_HB_MGR: 连接到服务器 192.168.97.247:7878
I (12347) TCP_HB_MGR: TCP连接建立成功
I (42347) TCP_HB_MGR: 已发送心跳 - 时间戳: 42347, 连接: 192.168.97.247:7878, 状态: 运行中
I (72347) TCP_HB_MGR: 已发送心跳 - 时间戳: 72347, 连接: 192.168.97.247:7878, 状态: 运行中
W (75000) TCP_HB_MGR: TCP连接断开，开始重连...
I (80000) TCP_HB_MGR: 重连成功
```

## 错误处理

### 常见错误及解决方案

1. **初始化失败**
   - 检查内存是否充足
   - 确认网络配置正确

2. **连接失败**
   - 验证服务器IP和端口
   - 检查网络连接状态
   - 确认防火墙设置

3. **心跳发送失败**
   - 检查TCP连接状态
   - 验证协议格式
   - 查看网络质量

## 性能特性

- **内存使用**: 约8KB RAM（包含缓冲区和统计信息）
- **CPU占用**: 极低（仅在定时器触发时活跃）
- **网络开销**: 每30秒发送9字节心跳包
- **响应时间**: 连接异常检测 < 1秒

## 兼容性说明

- **ESP-IDF版本**: 4.0及以上
- **FreeRTOS**: 支持
- **网络协议**: TCP/IPv4
- **协议兼容**: 严格遵循项目协议文档

## 注意事项

1. 确保ESP32已连接到WiFi网络
2. 服务器必须支持项目定义的协议格式
3. 建议在生产环境中启用看门狗定时器
4. 定期监控统计信息以评估网络质量
5. 在系统重启前调用 `tcp_client_hb_destroy()` 清理资源

## 故障排除

### 调试步骤

1. **启用详细日志**
   ```c
   esp_log_level_set("TCP_HB_MGR", ESP_LOG_DEBUG);
   esp_log_level_set("TCP_CLIENT_HB", ESP_LOG_DEBUG);
   ```

2. **检查网络连接**
   ```c
   // 使用ping测试网络连通性
   // 检查WiFi连接状态
   ```

3. **监控统计信息**
   ```c
   // 定期打印统计信息
   tcp_client_hb_print_status();
   ```

4. **验证协议格式**
   - 使用网络抓包工具验证数据包格式
   - 确认CRC16计算正确

## 更新日志

- **v1.0.0**: 初始版本，实现基本心跳功能
- 严格遵循协议文档规范
- 支持自动重连和状态监控
- 完整的日志记录和统计功能