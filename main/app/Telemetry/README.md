# 遥测服务模块使用指南

## 概述

遥测服务模块提供了一个完整的TCP服务器和数据管理系统，可以与UI界面结合使用，实现远程控制和数据监控功能。

## 主要功能

1. **TCP服务器**: 监听6666端口，接收控制命令
2. **数据管理**: 管理遥测数据和控制命令
3. **UI集成**: 与LVGL界面完美结合
4. **任务管理**: 自动管理FreeRTOS任务的创建和销毁

## 文件结构

```
app/Telemetry/
├── inc/
│   ├── telemetry_main.h      # 主服务头文件
│   └── telemetry_tcp.h       # TCP通信头文件
└── src/
    ├── telemetry_main.c      # 主服务实现
    ├── telemetry_tcp.c       # TCP通信实现
    └── telemetry_test.c      # 测试示例
```

## 使用方法

### 1. 在UI中使用

UI界面已经集成了遥测服务控制：

```c
#include "ui.h"

// 创建遥测界面
void create_telemetry_page() {
    lv_obj_t *screen = lv_scr_act();
    ui_telemetry_create(screen);  // 自动初始化服务
}

// 清理遥测界面  
void cleanup_telemetry_page() {
    ui_telemetry_cleanup();  // 自动停止服务并清理资源
}
```

### 2. 在代码中直接使用

```c
#include "telemetry_main.h"

// 数据回调函数
void my_data_callback(const telemetry_data_t *data) {
    printf("Voltage: %.2f V, Current: %.2f A\\n", data->voltage, data->current);
}

void app_main() {
    // 1. 初始化服务
    telemetry_service_init();
    
    // 2. 启动服务
    telemetry_service_start(my_data_callback);
    
    // 3. 发送控制命令
    telemetry_service_send_control(500, 600);  // 油门500, 方向600
    
    // 4. 停止服务（退出时）
    telemetry_service_stop();
    telemetry_service_deinit();
}
```

## TCP通信协议

### 连接方式
- **端口**: 6666
- **协议**: TCP
- **编码**: ASCII文本

### 命令格式

发送控制命令：
```
CTRL:throttle,direction\\n
```

示例：
```
CTRL:500,600\\n    # 油门500，方向600
```

### 响应格式

成功响应：
```
OK\\n
```

错误响应：
```
ERROR\\n
```

## 测试方法

### 1. 使用telnet测试

```bash
# Windows
telnet <ESP32_IP> 6666

# 发送命令
CTRL:500,600
```

### 2. 使用netcat测试

```bash
# Linux/Mac
nc <ESP32_IP> 6666

# 发送命令  
CTRL:500,600
```

### 3. 使用测试代码

```c
#include "telemetry_test.c"

void app_main() {
    start_telemetry_test();  // 启动自动化测试
}
```

## UI界面功能

### 控制元素
- **油门滑块**: 控制范围 0-1000
- **方向滑块**: 控制范围 0-1000  
- **启动/停止按钮**: 控制服务启停
- **状态显示**: 显示服务运行状态

### 数据显示
- **电压显示**: 实时电压值
- **电流显示**: 实时电流值
- **服务状态**: 显示服务运行状态

## 数据结构

```c
typedef struct {
    int32_t throttle;   // 油门值 (0-1000)
    int32_t direction;  // 方向值 (0-1000)
    float voltage;      // 电压 (V)
    float current;      // 电流 (A)
    float roll;         // 横滚角 (度)
    float pitch;        // 俯仰角 (度)  
    float yaw;          // 偏航角 (度)
    float altitude;     // 高度 (米)
} telemetry_data_t;
```

## 服务状态

```c
typedef enum {
    TELEMETRY_STATUS_STOPPED = 0,   // 已停止
    TELEMETRY_STATUS_STARTING,      // 启动中
    TELEMETRY_STATUS_RUNNING,       // 运行中
    TELEMETRY_STATUS_STOPPING,      // 停止中
    TELEMETRY_STATUS_ERROR          // 错误状态
} telemetry_status_t;
```

## 注意事项

1. **资源管理**: 服务会自动管理TCP端口和FreeRTOS任务，无需手动清理
2. **线程安全**: 所有API都是线程安全的，可以在多个任务中调用
3. **内存使用**: 服务使用少量堆内存，主要用于队列和信号量
4. **网络要求**: 需要确保ESP32已连接到WiFi网络

## 故障排除

### 服务启动失败
- 检查WiFi连接状态
- 确保端口6666未被占用
- 查看内存使用情况

### TCP连接失败  
- 检查防火墙设置
- 确认ESP32的IP地址
- 验证网络连通性

### UI无响应
- 检查遥测服务是否已启动
- 查看任务栈使用情况
- 确认回调函数正常执行
