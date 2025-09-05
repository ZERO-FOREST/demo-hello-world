# TCP客户端重构方案文档

## 概述

本文档描述了TCP客户端的实现，将心跳检测与遥测传输功能分离为两个独立的模块。

## 架构设计

### 目录结构

```
components/Receiver/
├── tcp_hb/                    # 心跳检测模块
│   ├── inc/
│   │   └── tcp_client_hb.h    # 心跳模块头文件
│   └── src/
│       └── tcp_client_hb.c    # 心跳模块实现
├── tcp_telemetry/             # 遥测传输模块
│   ├── inc/
│   │   └── tcp_client_telemetry.h  # 遥测模块头文件
│   └── src/
│       └── tcp_client_telemetry.c  # 遥测模块实现
├── tcp_refactored_example.c   # 重构后使用示例
└── README_TCP_REFACTORED.md   # 本文档
```

### 模块独立性

- **完全独立运行**：两个模块可以独立初始化、启动、停止和销毁
- **无共享状态**：模块间不共享任何全局变量或状态
- **独立配置**：每个模块有自己的配置参数和统计信息
- **独立任务**：每个模块运行在独立的FreeRTOS任务中

## 心跳检测模块 (tcp_hb)

### 功能特性

- 定期发送心跳包保持连接活跃
- 自动重连机制
- 连接健康监控
- 设备状态上报
- 详细的统计信息

### 配置参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| TCP_CLIENT_HB_DEFAULT_PORT | 7878 | 默认心跳端口 |
| TCP_CLIENT_HB_HEARTBEAT_INTERVAL_MS | 5000 | 心跳间隔(ms) |
| TCP_CLIENT_HB_CONNECT_TIMEOUT_MS | 10000 | 连接超时(ms) |
| TCP_CLIENT_HB_RECONNECT_INTERVAL_MS | 5000 | 重连间隔(ms) |
| TCP_CLIENT_HB_MAX_RECONNECT_ATTEMPTS | 10 | 最大重连次数 |

### 主要接口

#### 初始化和生命周期

```c
// 初始化心跳客户端
bool tcp_client_hb_init(const char *server_ip, uint16_t server_port);

// 启动心跳任务
bool tcp_client_hb_start(const char *task_name, uint32_t stack_size, UBaseType_t priority);

// 停止心跳任务
void tcp_client_hb_stop(void);

// 销毁心跳客户端
void tcp_client_hb_destroy(void);
```

#### 状态管理

```c
// 获取连接状态
tcp_client_hb_state_t tcp_client_hb_get_state(void);

// 检查连接健康状态
bool tcp_client_hb_is_connection_healthy(void);

// 设置设备状态
void tcp_client_hb_set_device_status(tcp_client_hb_device_status_t status);
```

#### 连接控制

```c
// 手动重连
bool tcp_client_hb_reconnect(void);

// 设置自动重连
void tcp_client_hb_set_auto_reconnect(bool enable);

// 发送心跳包
bool tcp_client_hb_send_heartbeat(void);
```

#### 统计信息

```c
// 获取统计信息
const tcp_client_hb_stats_t* tcp_client_hb_get_stats(void);

// 重置统计信息
void tcp_client_hb_reset_stats(void);

// 打印状态信息
void tcp_client_hb_print_status(void);
```

### 统计信息结构

```c
typedef struct {
    uint32_t heartbeat_sent_count;      // 发送心跳包数量
    uint32_t heartbeat_failed_count;    // 发送失败数量
    uint32_t connection_count;          // 连接次数
    uint32_t reconnection_count;        // 重连次数
    uint64_t total_connected_time;      // 总连接时长(ms)
    uint64_t last_heartbeat_time;       // 最后心跳时间
    uint64_t last_response_time;        // 最后响应时间
} tcp_client_hb_stats_t;
```

## 遥测传输模块 (tcp_telemetry)

### 功能特性

- 遥测数据传输
- 接收数据处理
- 自动重连机制
- 连接健康监控
- 模拟遥测数据生成
- 详细的统计信息

### 配置参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| TCP_CLIENT_TELEMETRY_DEFAULT_PORT | 6667 | 默认遥测端口 |
| TCP_CLIENT_TELEMETRY_SEND_INTERVAL_MS | 2000 | 发送间隔(ms) |
| TCP_CLIENT_TELEMETRY_CONNECT_TIMEOUT_MS | 10000 | 连接超时(ms) |
| TCP_CLIENT_TELEMETRY_RECONNECT_INTERVAL_MS | 5000 | 重连间隔(ms) |
| TCP_CLIENT_TELEMETRY_BUFFER_SIZE | 1024 | 缓冲区大小 |

### 主要接口

#### 初始化和生命周期

```c
// 初始化遥测客户端
bool tcp_client_telemetry_init(const char *server_ip, uint16_t server_port);

// 启动遥测任务
bool tcp_client_telemetry_start(const char *task_name, uint32_t stack_size, UBaseType_t priority);

// 停止遥测任务
void tcp_client_telemetry_stop(void);

// 销毁遥测客户端
void tcp_client_telemetry_destroy(void);
```

#### 连接管理

```c
// 连接到服务器
bool tcp_client_telemetry_connect(void);

// 断开连接
void tcp_client_telemetry_disconnect(void);

// 获取连接状态
tcp_client_telemetry_state_t tcp_client_telemetry_get_state(void);

// 检查连接健康状态
bool tcp_client_telemetry_is_connection_healthy(void);
```

#### 数据传输

```c
// 发送遥测数据
bool tcp_client_telemetry_send_data(const uint8_t *data, size_t length);

// 处理接收数据
void tcp_client_telemetry_process_received_data(const uint8_t *data, size_t length);

// 更新模拟遥测数据
void tcp_client_telemetry_update_simulated_data(void);
```

#### 统计和调试

```c
// 获取统计信息
const tcp_client_telemetry_stats_t* tcp_client_telemetry_get_stats(void);

// 重置统计信息
void tcp_client_telemetry_reset_stats(void);

// 打印状态信息
void tcp_client_telemetry_print_status(void);

// 打印接收帧信息
void tcp_client_telemetry_print_received_frame(const uint8_t *data, size_t length);
```

### 统计信息结构

```c
typedef struct {
    uint32_t telemetry_sent_count;      // 发送遥测包数量
    uint32_t telemetry_failed_count;    // 发送失败数量
    uint32_t connection_count;          // 连接次数
    uint32_t reconnection_count;        // 重连次数
    uint32_t bytes_sent;                // 发送字节数
    uint32_t bytes_received;            // 接收字节数
    uint64_t total_connected_time;      // 总连接时长(ms)
    uint64_t last_send_time;            // 最后发送时间
    uint64_t last_receive_time;         // 最后接收时间
} tcp_client_telemetry_stats_t;
```

## 使用示例

### 基础使用

```c
#include "tcp_hb/inc/tcp_client_hb.h"
#include "tcp_telemetry/inc/tcp_client_telemetry.h"

void basic_usage_example(void) {
    // 初始化心跳模块
    tcp_client_hb_init("192.168.1.100", 7878);
    tcp_client_hb_start("heartbeat_task", 4096, 5);
    
    // 初始化遥测模块
    tcp_client_telemetry_init("192.168.1.100", 6667);
    tcp_client_telemetry_start("telemetry_task", 4096, 5);
    
    // 两个模块现在独立运行
}
```

### 高级配置

```c
void advanced_usage_example(void) {
    // 使用不同服务器
    tcp_client_hb_init("192.168.1.100", 8080);        // 心跳服务器
    tcp_client_telemetry_init("192.168.1.101", 8081); // 遥测服务器
    
    // 配置自动重连
    tcp_client_hb_set_auto_reconnect(true);
    tcp_client_telemetry_set_auto_reconnect(true);
    
    // 启动任务
    tcp_client_hb_start("hb_task", 8192, 6);
    tcp_client_telemetry_start("tel_task", 8192, 6);
    
    // 设置设备状态
    tcp_client_hb_set_device_status(TCP_CLIENT_HB_DEVICE_STATUS_RUNNING);
}
```

### 独立控制

```c
void independent_control_example(void) {
    // 初始化两个模块
    tcp_client_hb_init("192.168.1.100", 7878);
    tcp_client_telemetry_init("192.168.1.100", 6667);
    
    // 只启动心跳模块
    tcp_client_hb_start("hb_task", 4096, 5);
    
    // 运行一段时间后启动遥测模块
    vTaskDelay(pdMS_TO_TICKS(10000));
    tcp_client_telemetry_start("tel_task", 4096, 5);
    
    // 停止心跳模块但保持遥测模块运行
    tcp_client_hb_stop();
    
    // 遥测模块继续独立运行
}
```

### 状态监控

```c
void status_monitoring_example(void) {
    // 检查连接状态
    if (tcp_client_hb_get_state() == TCP_CLIENT_HB_STATE_CONNECTED) {
        ESP_LOGI(TAG, "心跳模块已连接");
    }
    
    if (tcp_client_telemetry_get_state() == TCP_CLIENT_TELEMETRY_STATE_CONNECTED) {
        ESP_LOGI(TAG, "遥测模块已连接");
    }
    
    // 检查连接健康状态
    bool hb_healthy = tcp_client_hb_is_connection_healthy();
    bool tel_healthy = tcp_client_telemetry_is_connection_healthy();
    
    // 获取统计信息
    const tcp_client_hb_stats_t *hb_stats = tcp_client_hb_get_stats();
    const tcp_client_telemetry_stats_t *tel_stats = tcp_client_telemetry_get_stats();
    
    ESP_LOGI(TAG, "心跳发送: %lu, 遥测发送: %lu", 
             hb_stats->heartbeat_sent_count, 
             tel_stats->telemetry_sent_count);
}
```

## 扩展性设计

### 添加新的TCP功能模块

重构后的架构为添加新的TCP功能模块提供了清晰的模板：

1. **创建新模块目录**：`tcp_new_feature/`
2. **遵循统一接口规范**：
   - `tcp_client_new_feature_init(ip, port)`
   - `tcp_client_new_feature_start(task_name, stack_size, priority)`
   - `tcp_client_new_feature_stop()`
   - `tcp_client_new_feature_destroy()`

3. **保持模块独立性**：
   - 独立的配置结构
   - 独立的状态管理
   - 独立的统计信息
   - 无共享全局变量

### 接口扩展示例

```c
// 新功能模块接口示例
bool tcp_client_file_transfer_init(const char *server_ip, uint16_t server_port);
bool tcp_client_file_transfer_start(const char *task_name, uint32_t stack_size, UBaseType_t priority);
void tcp_client_file_transfer_stop(void);
void tcp_client_file_transfer_destroy(void);

// 功能特定接口
bool tcp_client_file_transfer_upload(const char *file_path);
bool tcp_client_file_transfer_download(const char *remote_file, const char *local_path);
```

## 性能特性

### 资源使用

- **内存占用**：每个模块独立管理内存，避免内存泄漏
- **任务开销**：每个模块运行在独立任务中，可独立配置优先级
- **网络资源**：使用不同端口，避免网络冲突

### 并发性能

- **并行处理**：两个模块可同时处理网络通信
- **独立重连**：一个模块的网络问题不影响另一个模块
- **负载分离**：心跳和遥测流量完全分离

## 兼容性说明

### 向后兼容

- 保持原有的功能特性
- API接口风格保持一致
- 配置参数含义不变

### 迁移指南

从原有实现迁移到重构版本：

1. **替换头文件包含**：
   ```c
   // 现在使用重构后的独立模块
   #include "tcp_hb/inc/tcp_client_hb.h"
   #include "tcp_telemetry/inc/tcp_client_telemetry.h"
   ```

2. **分离初始化调用**：
   ```c
   // 原来
   tcp_client_hb_init(ip, hb_port, tel_port);
   
   // 现在
   tcp_client_hb_init(ip, hb_port);
   tcp_client_telemetry_init(ip, tel_port);
   ```

3. **分离任务启动**：
   ```c
   // 原来
   tcp_client_hb_start(task_name, stack_size, priority);
   
   // 现在
   tcp_client_hb_start(hb_task_name, hb_stack_size, hb_priority);
   tcp_client_telemetry_start(tel_task_name, tel_stack_size, tel_priority);
   ```

## 调试和故障排除

### 日志输出

每个模块都有独立的日志标签：
- 心跳模块：`TCP_CLIENT_HB`
- 遥测模块：`TCP_CLIENT_TELEMETRY`

### 常见问题

1. **模块初始化失败**
   - 检查IP地址和端口配置
   - 确认网络连接状态
   - 查看内存是否充足

2. **连接不稳定**
   - 启用自动重连功能
   - 检查网络质量
   - 调整超时参数

3. **性能问题**
   - 检查任务优先级设置
   - 监控内存使用情况
   - 查看统计信息

### 调试工具

```c
// 打印详细状态信息
tcp_client_hb_print_status();
tcp_client_telemetry_print_status();

// 获取详细统计信息
const tcp_client_hb_stats_t *hb_stats = tcp_client_hb_get_stats();
const tcp_client_telemetry_stats_t *tel_stats = tcp_client_telemetry_get_stats();

// 重置统计信息进行测试
tcp_client_hb_reset_stats();
tcp_client_telemetry_reset_stats();
```

## 总结

重构后的TCP客户端实现提供了：

- ✅ **完全模块化**：心跳和遥测功能完全分离
- ✅ **独立运行**：模块间无依赖，可独立控制
- ✅ **统一接口**：遵循一致的API设计规范
- ✅ **易于扩展**：为新功能模块提供清晰模板
- ✅ **向后兼容**：保持原有功能特性
- ✅ **高可维护性**：清晰的代码结构和文档

这种架构设计为未来的功能扩展和维护提供了坚实的基础。