# 事件驱动TCP任务管理器

## 概述

事件驱动TCP任务管理器是一个基于WIFI连接状态的智能TCP连接管理系统。它只在WIFI连接成功后才启动TCP连接，并在WIFI断开时自动停止TCP连接，从而避免不必要的连接尝试和资源浪费。

## 主要特性

- **事件驱动**: 基于WIFI连接状态自动管理TCP连接
- **智能连接**: 只在WIFI连接成功后才启动TCP模块
- **自动断开**: WIFI断开时自动停止TCP连接
- **资源管理**: 合理管理系统资源，避免无效连接
- **向后兼容**: 保留传统TCP任务函数接口

## 架构设计

```
┌─────────────────┐    事件通知    ┌─────────────────┐
│  WIFI配对管理器  │ ──────────→ │  TCP任务管理器   │
└─────────────────┘              └─────────────────┘
         │                                │
         │                                │
    ┌────▼────┐                      ┌───▼───┐
    │ WIFI事件 │                      │TCP模块│
    │  监听   │                      │ 控制  │
    └─────────┘                      └───────┘
```

## API接口

### 基础接口

```c
// 初始化TCP任务管理器
esp_err_t tcp_task_manager_init(void);

// 启动TCP任务管理器
esp_err_t tcp_task_manager_start(void);

// 停止TCP任务管理器
esp_err_t tcp_task_manager_stop(void);
```

### 集成接口

```c
// 启动带WIFI事件集成的TCP任务管理器
esp_err_t tcp_task_manager_start_with_wifi(const wifi_pairing_config_t* wifi_config);
```

### 兼容接口

```c
// 传统TCP任务函数（保持向后兼容）
void tcp_task(void);
```

## 使用方法

### 推荐方式：集成启动

```c
#include "task.h"

void app_main(void) {
    // 配置WIFI参数
    wifi_pairing_config_t wifi_config = {
        .ap_ssid = "ESP32_Config",
        .ap_password = "12345678",
        .ap_channel = 1,
        .ap_max_connections = 4,
        .pairing_timeout_ms = 120000,  // 2分钟超时
        .auto_save_credentials = true
    };
    
    // 一次调用完成WIFI和TCP的启动
    esp_err_t ret = tcp_task_manager_start_with_wifi(&wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动TCP管理器失败: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "TCP管理器已启动，等待WIFI连接...");
    
    // 应用主循环
    while (1) {
        // 你的应用逻辑
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // 程序退出时停止TCP管理器
    tcp_task_manager_stop();
}
```

### 方法1: 使用集成接口（推荐）

```c
#include "task.h"

void start_tcp_with_wifi(void) {
    wifi_pairing_config_t config = {
        .ap_ssid = "ESP32_Setup",
        .ap_password = "password123",
        .ap_channel = 1,
        .ap_max_connections = 4,
        .pairing_timeout_ms = 120000,
        .auto_save_credentials = true
    };
    
    esp_err_t ret = tcp_task_manager_start_with_wifi(&config);
    if (ret == ESP_OK) {
        ESP_LOGI("APP", "TCP管理器启动成功");
    } else {
        ESP_LOGE("APP", "TCP管理器启动失败: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI("APP", "事件驱动TCP系统启动成功");
}
```

### 方法2: 分步初始化

```c
#include "task.h"
#include "wifi_pairing_manager.h"

// WIFI事件回调函数
static void my_wifi_event_callback(wifi_pairing_state_t state, const char* ssid) {
    switch (state) {
        case WIFI_PAIRING_STATE_CONNECTED:
            ESP_LOGI("APP", "WIFI已连接: %s", ssid);
            break;
        case WIFI_PAIRING_STATE_DISCONNECTED:
            ESP_LOGI("APP", "WIFI已断开");
            break;
        default:
            break;
    }
}

void app_main(void) {
    // 1. 配置并启动WIFI配对管理器
    wifi_pairing_config_t wifi_config = {
        .target_ssids = {"MyWiFi"},
        .target_passwords = {"password123"},
        .target_count = 1,
        .scan_interval_ms = 10000,
        .connect_timeout_ms = 15000
    };
    
    wifi_pairing_manager_init(&wifi_config, my_wifi_event_callback);
    wifi_pairing_manager_start();
    
    // 2. 启动TCP任务管理器
    tcp_task_manager_init();
    tcp_task_manager_start();
    
    ESP_LOGI("APP", "系统启动完成");
}
```

## 工作流程

1. **系统启动**: 初始化WIFI配对管理器和TCP任务管理器
2. **WIFI扫描**: 自动扫描并尝试连接配置的WIFI网络
3. **WIFI连接**: 成功连接WIFI后触发连接事件
4. **TCP启动**: 收到WIFI连接事件后启动TCP心跳和遥测模块
5. **正常运行**: TCP模块正常工作，定期发送心跳和遥测数据
6. **WIFI断开**: WIFI断开时触发断开事件
7. **TCP停止**: 收到WIFI断开事件后停止TCP模块
8. **重新连接**: 自动重新扫描和连接WIFI，循环上述过程

## 事件状态图

```
┌─────────┐    WIFI连接成功    ┌─────────────┐
│  等待    │ ──────────────→ │ TCP运行中   │
│ WIFI连接 │                  │             │
└─────────┘ ←────────────── └─────────────┘
              WIFI断开连接
```

## 配置参数

### WIFI配对配置

```c
typedef struct {
    char target_ssids[WIFI_PAIRING_MAX_TARGETS][32];     // 目标SSID列表
    char target_passwords[WIFI_PAIRING_MAX_TARGETS][64]; // 对应密码列表
    int target_count;                                    // 目标数量
    uint32_t scan_interval_ms;                          // 扫描间隔(毫秒)
    uint32_t connect_timeout_ms;                        // 连接超时(毫秒)
} wifi_pairing_config_t;
```

### TCP服务器配置

默认服务器IP: `192.168.4.1`
- 心跳端口: `7878` (默认)
- 遥测端口: `6667` (默认)

## 日志输出示例

```
I (1234) Task: 初始化TCP任务管理器...
I (1235) Task: TCP任务管理器初始化完成
I (1236) Task: TCP任务启动，等待WIFI连接...
I (5678) Task: WIFI已连接: MyWiFi，准备启动TCP连接
I (5679) Task: WIFI已连接，启动TCP模块
I (5680) Task: 启动TCP模块，服务器IP: 192.168.4.1
I (5681) Task: TCP模块启动成功
I (9999) Task: WIFI已断开，停止TCP连接
I (10000) Task: WIFI已断开，停止TCP模块
I (10001) Task: TCP模块已停止
```

## 完整示例

### 示例文件

完整的使用示例请参考：`example_event_driven_tcp.c`

该文件包含以下示例：
- **集成方式**：使用 `tcp_task_manager_start_with_wifi()` 一次性启动
- **分步方式**：手动控制每个初始化步骤
- **兼容方式**：使用原始 `tcp_task()` 函数
- **错误处理**：各种错误情况的处理示例

### 最佳实践

1. **推荐使用集成方式**
   ```c
   // 最简单的启动方式
   wifi_pairing_config_t config = {/* 配置参数 */};
   tcp_task_manager_start_with_wifi(&config);
   ```

2. **错误处理**
   ```c
   esp_err_t ret = tcp_task_manager_start_with_wifi(&config);
   if (ret != ESP_OK) {
       ESP_LOGE(TAG, "启动失败: %s", esp_err_to_name(ret));
       // 处理错误情况
   }
   ```

3. **资源清理**
   ```c
   // 程序退出前必须调用
   tcp_task_manager_stop();
   ```

## 注意事项

1. **初始化顺序**：使用集成方式时无需关心初始化顺序
2. **WIFI事件**：TCP连接只有在WIFI成功连接后才会启动
3. **资源管理**：程序退出前务必调用 `tcp_task_manager_stop()` 清理资源
4. **线程安全**：所有API都是线程安全的
5. **重复调用**：重复调用init/start函数是安全的，会返回相应状态
6. **事件驱动**：系统会自动响应WIFI连接/断开事件，无需手动干预

## 故障排除

### 常见问题

1. **TCP连接失败**
   - 检查WIFI是否已连接（查看日志中的WIFI事件）
   - 确认服务器IP和端口配置（默认192.168.4.1）
   - 查看网络连通性
   - 检查防火墙设置

2. **任务创建失败**
   - 检查可用内存（使用 `esp_get_free_heap_size()`）
   - 调整任务栈大小（默认4096字节）
   - 确认任务优先级设置（默认优先级5）

3. **事件处理异常**
   - 检查事件组是否正确创建
   - 确认WIFI事件回调注册
   - 查看日志输出定位问题

4. **WIFI配对失败**
   - 检查AP配置参数
   - 确认密码长度（至少8位）
   - 检查信道设置

### 调试建议

- **启用详细日志**：
  ```c
  esp_log_level_set("*", ESP_LOG_DEBUG);
  esp_log_level_set("Task", ESP_LOG_VERBOSE);
  esp_log_level_set("TCP_HB", ESP_LOG_VERBOSE);
  esp_log_level_set("TCP_TELEMETRY", ESP_LOG_VERBOSE);
  ```

- **监控任务状态**：
  ```c
  // 打印任务信息
  vTaskList(pcWriteBuffer);
  printf("%s", pcWriteBuffer);
  ```

- **网络诊断**：
  ```bash
  # 测试网络连通性
  ping 192.168.4.1
  # 检查端口是否开放
  telnet 192.168.4.1 8080
  ```

### 日志分析

正常启动时的日志序列：
```
I (xxx) Task: TCP任务管理器初始化完成
I (xxx) Task: TCP任务管理器启动完成
I (xxx) Task: WIFI配对管理器初始化完成
I (xxx) Task: WIFI配对启动完成
I (xxx) Task: WIFI已连接: YourSSID
I (xxx) Task: 启动TCP模块
I (xxx) TCP_HB: 心跳客户端初始化成功
I (xxx) TCP_TELEMETRY: 遥测客户端初始化成功
I (xxx) TCP_HB: 心跳客户端启动成功
I (xxx) TCP_TELEMETRY: 遥测客户端启动成功
```

## 版本历史

- v1.0.0: 初始版本，支持基本的事件驱动TCP连接管理
- 计划功能: 支持多服务器配置、连接重试策略、性能优化