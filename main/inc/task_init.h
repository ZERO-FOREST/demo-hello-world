#ifndef TASK_INIT_H
#define TASK_INIT_H

#include "esp_err.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// 任务优先级定义
#define TASK_PRIORITY_LOW       2
#define TASK_PRIORITY_NORMAL    5
#define TASK_PRIORITY_HIGH      8

// 任务堆栈大小定义 - 针对不同任务类型优化
#define TASK_STACK_SMALL        2048   // 2KB - 简单任务
#define TASK_STACK_MEDIUM       4096   // 4KB - 中等任务  
#define TASK_STACK_LARGE        6144   // 6KB - LVGL等复杂任务
#define TASK_STACK_WIFI         8192   // 8KB - WiFi任务专用

// 任务初始化函数
esp_err_t init_all_tasks(void);

// 个别任务初始化函数
esp_err_t init_lvgl_task(void);
esp_err_t init_power_management_task(void);
esp_err_t init_ws2812_demo_task(void);
esp_err_t init_system_monitor_task(void);
esp_err_t init_battery_monitor_task(void);
esp_err_t init_joystick_adc_task(void);
esp_err_t init_audio_receiver_task(void);
esp_err_t init_serial_display_task(void);

// 任务控制函数
esp_err_t stop_all_tasks(void);
void list_running_tasks(void);

// 任务句柄获取函数
TaskHandle_t get_lvgl_task_handle(void);
TaskHandle_t get_power_task_handle(void);
TaskHandle_t get_ws2812_task_handle(void);
TaskHandle_t get_monitor_task_handle(void);
TaskHandle_t get_battery_task_handle(void);
TaskHandle_t get_joystick_task_handle(void);

#ifdef __cplusplus
}
#endif

#endif // TASK_INIT_H 