#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_clk_tree.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

static const char *TAG = "SLEEP_DEMO";

// 🔢 RTC内存变量（深度睡眠中保持）
RTC_DATA_ATTR static int sleep_count = 0;
RTC_DATA_ATTR static uint64_t sleep_enter_time = 0;

// 📊 睡眠模式演示主程序
void sleep_mode_demo(void) {
    // 增加睡眠计数
    sleep_count++;
    
    ESP_LOGI(TAG, "=== ESP32-S3 Sleep Mode Demo ===");
    ESP_LOGI(TAG, "Sleep count: %d", sleep_count);
    
    // 检查唤醒原因
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "🕐 Wake up from timer");
            uint64_t sleep_time = esp_timer_get_time() - sleep_enter_time;
            ESP_LOGI(TAG, "Sleep duration: %llu ms", sleep_time / 1000);
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            ESP_LOGI(TAG, "🔘 Wake up from GPIO");
            break;
        default:
            ESP_LOGI(TAG, "🔄 First boot or reset");
            break;
    }
    
    // 显示系统状态
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "💾 System Status:");
    ESP_LOGI(TAG, "  Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "  Min free heap: %lu bytes", (unsigned long)esp_get_minimum_free_heap_size());
    
    // 模拟一些工作
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "💼 Simulating work...");
    for(int i = 0; i < 5; i++) {
        ESP_LOGI(TAG, "Working... %d/5", i + 1);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // 根据睡眠次数选择不同的睡眠模式
    ESP_LOGI(TAG, "");
    if(sleep_count % 3 == 1) {
        // Light Sleep演示
        ESP_LOGI(TAG, "🛌 Demo: Light Sleep (5 seconds)");
        ESP_LOGI(TAG, "  - RAM preserved");
        ESP_LOGI(TAG, "  - Fast wake up");
        ESP_LOGI(TAG, "  - ~0.8mA power consumption");
        
        esp_sleep_enable_timer_wakeup(5 * 1000000);  // 5秒
        esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0); // GPIO0 唤醒
        
        sleep_enter_time = esp_timer_get_time();
        esp_light_sleep_start();
        
        // Light Sleep后会从这里继续执行
        ESP_LOGI(TAG, "🌅 Returned from Light Sleep!");
        ESP_LOGI(TAG, "");
        
    } else if(sleep_count % 3 == 2) {
        // Deep Sleep演示
        ESP_LOGI(TAG, "😴 Demo: Deep Sleep (10 seconds)");
        ESP_LOGI(TAG, "  - Only RTC memory preserved");
        ESP_LOGI(TAG, "  - Full restart after wake");
        ESP_LOGI(TAG, "  - ~10µA power consumption");
        
        esp_sleep_enable_timer_wakeup(10 * 1000000); // 10秒
        esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0); // GPIO0 唤醒
        
        sleep_enter_time = esp_timer_get_time();
        esp_deep_sleep_start();
        
    } else {
        // Hibernation演示
        ESP_LOGI(TAG, "🥶 Demo: Hibernation (15 seconds)");
        ESP_LOGI(TAG, "  - Minimal RTC kept active");
        ESP_LOGI(TAG, "  - Ultra low power");
        ESP_LOGI(TAG, "  - ~2.5µA power consumption");
        
        esp_sleep_enable_timer_wakeup(15 * 1000000); // 15秒
        
        // 关闭所有RTC外设以最小化功耗
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_OFF);
        
        sleep_enter_time = esp_timer_get_time();
        esp_deep_sleep_start();
    }
    
    // 🔄 注意：移除了递归调用，避免无限递归
    ESP_LOGI(TAG, "✅ Sleep demo cycle completed");
}

// 🚀 启动睡眠演示的简化主程序
void simple_sleep_demo_main(void) {
    ESP_LOGI(TAG, "🚀 Starting Simple Sleep Demo...");
    ESP_LOGI(TAG, "Press GPIO0 button to wake from sleep");
    ESP_LOGI(TAG, "");
    
    // 配置GPIO0为输入（通常是BOOT按钮）
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_0),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // 🔄 使用循环代替递归，避免栈溢出
    while(1) {
        sleep_mode_demo();
        
        // 如果是Light Sleep，会继续循环
        // 如果是Deep Sleep/Hibernation，系统会重启，重新从app_main开始
        vTaskDelay(pdMS_TO_TICKS(2000));  // 等待2秒再进行下一轮
    }
} 