#ifndef TASK_INIT_H
#define TASK_INIT_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// 任务优先级定义
#define TASK_PRIORITY_LOW       2
#define TASK_PRIORITY_NORMAL    5
#define TASK_PRIORITY_HIGH      8

// 任务堆栈大小定义
#define TASK_STACK_SMALL        2048
#define TASK_STACK_MEDIUM       4096
#define TASK_STACK_LARGE        8192

// 任务初始化函数
esp_err_t init_all_tasks(void);

// 个别任务初始化函数
esp_err_t init_lvgl_task(void);
esp_err_t init_power_management_task(void);
esp_err_t init_ws2812_demo_task(void);
esp_err_t init_system_monitor_task(void);
esp_err_t init_battery_monitor_task(void);

// 任务控制函数
esp_err_t stop_all_tasks(void);
void list_running_tasks(void);

// 任务句柄获取函数
TaskHandle_t get_lvgl_task_handle(void);
TaskHandle_t get_power_task_handle(void);
TaskHandle_t get_ws2812_task_handle(void);
TaskHandle_t get_monitor_task_handle(void);
TaskHandle_t get_battery_task_handle(void);

#ifdef __cplusplus
}
#endif

#endif // TASK_INIT_H 