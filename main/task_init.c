#include "task_init.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 引入各模块头文件
#include "background_manager.h"
#include "battery_monitor.h"
#include "lsm6ds3_demo.h"
#include "i2s_tdm_demo.h"
#include "joystick_adc.h"
#include "lvgl_main.h"
#include "power_management.h"
#include "ui.h"
#include "wifi_manager.h"
#include "ws2812.h"
#include <stdint.h>

static const char* TAG = "TASK_INIT";

// 任务句柄存储
static TaskHandle_t s_lvgl_task_handle = NULL;
static TaskHandle_t s_power_task_handle = NULL;
static TaskHandle_t s_ws2812_task_handle = NULL;
static TaskHandle_t s_monitor_task_handle = NULL;
static TaskHandle_t s_battery_task_handle = NULL;
static TaskHandle_t s_joystick_task_handle = NULL;
static TaskHandle_t s_wifi_task_handle = NULL;

// WS2812演示任务
static void ws2812_demo_task(void* pvParameters) {
    ESP_LOGI(TAG, "WS2812 Demo Task started on core %d", xPortGetCoreID());

    // 初始化WS2812，只有一个LED
    esp_err_t ret = ws2812_init(1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WS2812 init failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL); // 自杀
        return;
    }

    ESP_LOGI(TAG, "WS2812 initialized with %d LEDs on GPIO48", ws2812_get_led_count());

    ws2812_color_t breathing_color = WS2812_COLOR_BLACK; // 选择呼吸灯颜色

    while (1) {
        ws2812_set_all(breathing_color);
        ws2812_refresh();                 // 刷新显示
        vTaskDelay(pdMS_TO_TICKS(10000)); // 每10秒更新一次
        // for (uint8_t j = 0; j < 5; j++) {
        //     // 亮度渐变：从 0 到 127
        //     for (int brightness = 0; brightness <= 127; brightness += 5) {
        //         ws2812_set_brightness(brightness);  // 设置亮度
        //         ws2812_set_all(breathing_color[j]); // 设置所有LED颜色
        //         ws2812_refresh();                   // 刷新显示
        //         vTaskDelay(pdMS_TO_TICKS(50));      // 延时
        //     }

        //     // 亮度渐变：从 127 到 0
        //     for (int brightness = 127; brightness >= 0; brightness -= 5) {
        //         ws2812_set_brightness(brightness);  // 设置亮度
        //         ws2812_set_all(breathing_color[j]); // 设置所有LED颜色
        //         ws2812_refresh();                   // 刷新显示
        //         vTaskDelay(pdMS_TO_TICKS(50));      // 延时
        //     }
        // }
    }
}

// 摇杆ADC采样任务（200Hz）
static void joystick_adc_task(void* pvParameters) {
    ESP_LOGI(TAG, "Joystick ADC Task started on core %d", xPortGetCoreID());

    if (joystick_adc_init() != ESP_OK) {
        ESP_LOGE(TAG, "Joystick ADC init failed");
        vTaskDelete(NULL);
        return;
    }

    const TickType_t period_ticks = pdMS_TO_TICKS(20); // 50Hz = 20ms，降低频率避免看门狗超时
    TickType_t last_wake = xTaskGetTickCount();

    joystick_data_t data;
    uint32_t log_counter = 0;

    while (1) {
        joystick_adc_read(&data);
        vTaskDelay(period_ticks);
    }
}

// 电源管理任务包装
static void power_management_task(void* pvParameters) {
    ESP_LOGI(TAG, "Power Management Task started on core %d", xPortGetCoreID());
    power_management_demo();
    vTaskDelete(NULL);
}

// 系统监控任务
static void system_monitor_task(void* pvParameters) {
    ESP_LOGI(TAG, "System Monitor Task started on core %d", xPortGetCoreID());

    while (1) {
        // 系统状态监控
        ESP_LOGI(TAG, "=== System Status ===");
        ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
        ESP_LOGI(TAG, "Min free heap: %lu bytes", (unsigned long)esp_get_minimum_free_heap_size());
        ESP_LOGI(TAG, "Stack high water mark: %lu bytes", (unsigned long)uxTaskGetStackHighWaterMark(NULL));

        // 任务状态检查
        if (s_lvgl_task_handle) {
            ESP_LOGI(TAG, "LVGL task: Running");
        }
        if (s_ws2812_task_handle) {
            ESP_LOGI(TAG, "WS2812 task: Running");
        }
        if (s_power_task_handle) {
            ESP_LOGI(TAG, "Power task: Running");
        }
        if (s_battery_task_handle) {
            ESP_LOGI(TAG, "Battery task: Running");
        }

        ESP_LOGI(TAG, "==================");

        vTaskDelay(pdMS_TO_TICKS(10000)); // 10秒监控一次
    }
}

static void wifi_manager_task(void* pvParameters) {
    ESP_LOGI(TAG, "WiFi Manager Task started on core %d", xPortGetCoreID());

    esp_err_t ret = wifi_manager_init(NULL);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi manager initialized");
        ret = wifi_manager_start();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGW(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
    }

    // Task can terminate after starting WiFi connection process
    vTaskDelete(NULL);
}

// 电池监测任务（现在由后台管理模块处理，此任务主要用于日志记录）
static void battery_monitor_task(void* pvParameters) {
    ESP_LOGI(TAG, "Battery Monitor Task started on core %d", xPortGetCoreID());

    // 等待系统初始化完成
    vTaskDelay(pdMS_TO_TICKS(5000)); // 增加等待时间，确保UI完全初始化

    while (1) {
        // 获取后台电池信息用于日志记录
        background_battery_info_t battery_info;
        esp_err_t ret = background_manager_get_battery(&battery_info);

        if (ret == ESP_OK && battery_info.is_valid) {
            ESP_LOGI(TAG, "Battery: %dmV, %d%%, Low: %d, Critical: %d", battery_info.voltage_mv,
                     battery_info.percentage, battery_info.is_low_battery, battery_info.is_critical);

            // 检查低电量警告
            if (battery_info.is_critical) {
                ESP_LOGW(TAG, "CRITICAL BATTERY LEVEL: %d%%", battery_info.percentage);
            } else if (battery_info.is_low_battery) {
                ESP_LOGW(TAG, "LOW BATTERY LEVEL: %d%%", battery_info.percentage);
            }
        } else {
            ESP_LOGW(TAG, "Failed to get battery info from background manager");
        }

        // 每10秒记录一次日志
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10秒
    }
}

// 任务初始化函数实现
esp_err_t init_lvgl_task(void) {
    if (s_lvgl_task_handle != NULL) {
        ESP_LOGW(TAG, "LVGL task already running");
        return ESP_OK;
    }

    BaseType_t result = xTaskCreatePinnedToCore(lvgl_main_task,      // 任务函数
                                                "LVGL_Main",         // 任务名称
                                                TASK_STACK_LARGE,    // 堆栈大小 (8KB)
                                                NULL,                // 参数
                                                TASK_PRIORITY_HIGH,  // 高优先级
                                                &s_lvgl_task_handle, // 任务句柄
                                                1                    // 绑定到Core 1 (用户核心)
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LVGL task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "LVGL task created successfully on Core 1");
    return ESP_OK;
}

esp_err_t init_joystick_adc_task(void) {
    if (s_joystick_task_handle != NULL) {
        ESP_LOGW(TAG, "Joystick ADC task already running");
        return ESP_OK;
    }

    BaseType_t result = xTaskCreatePinnedToCore(joystick_adc_task,       // 任务函数
                                                "Joystick_ADC",          // 任务名称
                                                TASK_STACK_MEDIUM,       // 堆栈大小 (4KB，避免栈溢出)
                                                NULL,                    // 参数
                                                TASK_PRIORITY_NORMAL,    // 普通优先级
                                                &s_joystick_task_handle, // 任务句柄
                                                0);                      // 绑定到Core 0

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Joystick ADC task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Joystick ADC task created successfully on Core 0");
    return ESP_OK;
}

esp_err_t init_power_management_task(void) {
    if (s_power_task_handle != NULL) {
        ESP_LOGW(TAG, "Power management task already running");
        return ESP_OK;
    }

    BaseType_t result = xTaskCreatePinnedToCore(power_management_task, // 任务函数
                                                "Power_Mgmt",          // 任务名称
                                                TASK_STACK_MEDIUM,     // 堆栈大小 (4KB)
                                                NULL,                  // 参数
                                                TASK_PRIORITY_LOW,     // 低优先级
                                                &s_power_task_handle,  // 任务句柄
                                                0                      // 绑定到Core 0 (系统核心)
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create power management task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Power management task created successfully on Core 0");
    return ESP_OK;
}

esp_err_t init_ws2812_demo_task(void) {
    if (s_ws2812_task_handle != NULL) {
        ESP_LOGW(TAG, "WS2812 demo task already running");
        return ESP_OK;
    }

    BaseType_t result = xTaskCreatePinnedToCore(ws2812_demo_task,      // 任务函数
                                                "WS2812_Demo",         // 任务名称
                                                TASK_STACK_MEDIUM,     // 堆栈大小 (4KB)
                                                NULL,                  // 参数
                                                TASK_PRIORITY_NORMAL,  // 低优先级
                                                &s_ws2812_task_handle, // 任务句柄
                                                0                      // 绑定到Core 0
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create WS2812 demo task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "WS2812 demo task created successfully on Core 0");
    return ESP_OK;
}

esp_err_t init_system_monitor_task(void) {
    if (s_monitor_task_handle != NULL) {
        ESP_LOGW(TAG, "System monitor task already running");
        return ESP_OK;
    }

    BaseType_t result = xTaskCreatePinnedToCore(system_monitor_task,    // 任务函数
                                                "Sys_Monitor",          // 任务名称
                                                TASK_STACK_SMALL,       // 堆栈大小 (2KB)
                                                NULL,                   // 参数
                                                TASK_PRIORITY_LOW,      // 低优先级
                                                &s_monitor_task_handle, // 任务句柄
                                                0                       // 绑定到Core 0
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create system monitor task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "System monitor task created successfully on Core 0");
    return ESP_OK;
}

esp_err_t init_wifi_manager_task(void) {
    if (s_wifi_task_handle != NULL) {
        ESP_LOGW(TAG, "WiFi manager task already running");
        return ESP_OK;
    }

    BaseType_t result = xTaskCreatePinnedToCore(wifi_manager_task,    // 任务函数
                                                "WiFi_Manager",       // 任务名称
                                                TASK_STACK_MEDIUM,    // 堆栈大小 (4KB)
                                                NULL,                 // 参数
                                                TASK_PRIORITY_NORMAL, // 普通优先级
                                                &s_wifi_task_handle,  // 任务句柄
                                                0                     // 绑定到Core 0
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create WiFi manager task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "WiFi manager task created successfully on Core 0");
    return ESP_OK;
}

esp_err_t init_battery_monitor_task(void) {
    if (s_battery_task_handle != NULL) {
        ESP_LOGW(TAG, "Battery monitor task already running");
        return ESP_OK;
    }

    BaseType_t result = xTaskCreatePinnedToCore(battery_monitor_task,   // 任务函数
                                                "Battery_Monitor",      // 任务名称
                                                TASK_STACK_MEDIUM,      // 堆栈大小 (4KB)
                                                NULL,                   // 参数
                                                TASK_PRIORITY_LOW,      // 低优先级
                                                &s_battery_task_handle, // 任务句柄
                                                0                       // 绑定到Core 0
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create battery monitor task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Battery monitor task created successfully on Core 0");
    return ESP_OK;
}

esp_err_t init_all_tasks(void) {
    ESP_LOGI(TAG, "Initializing all tasks...");

    esp_err_t ret;

    // 初始化后台管理模块
    ret = background_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init background manager");
        return ret;
    }

    // 启动后台管理任务
    ret = background_manager_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start background manager task");
        return ret;
    }

    // 初始化WS2812演示任务
    ret = init_ws2812_demo_task();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init WS2812 demo task");
        return ret;
    }

    // 初始化电池监测任务
    ret = init_battery_monitor_task();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init battery monitor task");
        return ret;
    }

    // 初始化WiFi管理任务
    ret = init_wifi_manager_task();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init WiFi manager task");
        return ret;
    }

    // 初始化摇杆ADC采样任务（200Hz）
    ret = init_joystick_adc_task();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init Joystick ADC task");
        return ret;
    }

    // 初始化LVGL任务
    ret = init_lvgl_task();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init LVGL task");
        return ret;
    }

    start_lsm6ds3_demo();

    ESP_LOGI(TAG, "All tasks initialized successfully");
    return ESP_OK;
}

esp_err_t stop_all_tasks(void) {
    ESP_LOGI(TAG, "Stopping all tasks...");

    // 停止后台管理任务
    background_manager_stop();
    background_manager_deinit();
    ESP_LOGI(TAG, "Background manager stopped");

    if (s_lvgl_task_handle) {
        vTaskDelete(s_lvgl_task_handle);
        s_lvgl_task_handle = NULL;
        ESP_LOGI(TAG, "LVGL task stopped");
    }

    if (s_power_task_handle) {
        vTaskDelete(s_power_task_handle);
        s_power_task_handle = NULL;
        ESP_LOGI(TAG, "Power management task stopped");
    }

    if (s_ws2812_task_handle) {
        vTaskDelete(s_ws2812_task_handle);
        s_ws2812_task_handle = NULL;
        ESP_LOGI(TAG, "WS2812 demo task stopped");
    }

    if (s_monitor_task_handle) {
        vTaskDelete(s_monitor_task_handle);
        s_monitor_task_handle = NULL;
        ESP_LOGI(TAG, "System monitor task stopped");
    }

    if (s_joystick_task_handle) {
        vTaskDelete(s_joystick_task_handle);
        s_joystick_task_handle = NULL;
        ESP_LOGI(TAG, "Joystick ADC task stopped");
    }

    if (s_battery_task_handle) {
        vTaskDelete(s_battery_task_handle);
        s_battery_task_handle = NULL;
        ESP_LOGI(TAG, "Battery monitor task stopped");
    }

    if (s_wifi_task_handle) {
        vTaskDelete(s_wifi_task_handle);
        s_wifi_task_handle = NULL;
        ESP_LOGI(TAG, "WiFi manager task stopped");
    }

    // 停止 I2S TDM 演示
    i2s_tdm_demo_deinit();

    ESP_LOGI(TAG, "All tasks stopped");
    return ESP_OK;
}

void list_running_tasks(void) {
    ESP_LOGI(TAG, "=== Running Tasks ===");
    ESP_LOGI(TAG, "LVGL Task: %s", s_lvgl_task_handle ? "Running" : "Stopped");
    ESP_LOGI(TAG, "Power Task: %s", s_power_task_handle ? "Running" : "Stopped");
    ESP_LOGI(TAG, "WS2812 Task: %s", s_ws2812_task_handle ? "Running" : "Stopped");
    ESP_LOGI(TAG, "Monitor Task: %s", s_monitor_task_handle ? "Running" : "Stopped");
    ESP_LOGI(TAG, "Joystick Task: %s", s_joystick_task_handle ? "Running" : "Stopped");
    ESP_LOGI(TAG, "Battery Task: %s", s_battery_task_handle ? "Running" : "Stopped");
    ESP_LOGI(TAG, "WiFi Task: %s", s_wifi_task_handle ? "Running" : "Stopped");
    ESP_LOGI(TAG, "==================");
}

// 任务句柄获取函数
TaskHandle_t get_lvgl_task_handle(void) { return s_lvgl_task_handle; }
TaskHandle_t get_power_task_handle(void) { return s_power_task_handle; }
TaskHandle_t get_ws2812_task_handle(void) { return s_ws2812_task_handle; }
TaskHandle_t get_monitor_task_handle(void) { return s_monitor_task_handle; }
TaskHandle_t get_battery_task_handle(void) { return s_battery_task_handle; }
TaskHandle_t get_joystick_task_handle(void) { return s_joystick_task_handle; }
TaskHandle_t get_wifi_task_handle(void) { return s_wifi_task_handle; }