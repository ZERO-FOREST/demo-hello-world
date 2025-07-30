#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "driver/rtc_io.h"
#include "driver/gpio.h"
#include "driver/touch_pad.h"
#include "soc/rtc_cntl_reg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "POWER_MGR";

// 🛌 1. Light Sleep - 轻度睡眠（RAM保持，快速唤醒）
void enter_light_sleep(uint32_t sleep_time_ms) {
    ESP_LOGI(TAG, "🛌 Entering Light Sleep for %lu ms...", (unsigned long)sleep_time_ms);
    
    // 配置定时器唤醒
    esp_sleep_enable_timer_wakeup(sleep_time_ms * 1000);  // 微秒
    
    // 配置GPIO唤醒（可选）
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);  // GPIO0 低电平唤醒
    
    ESP_LOGI(TAG, "Free heap before sleep: %lu bytes", (unsigned long)esp_get_free_heap_size());
    
    // 进入Light Sleep
    esp_light_sleep_start();
    
    // 醒来后继续执行
    ESP_LOGI(TAG, "🌅 Wake up from Light Sleep!");
    ESP_LOGI(TAG, "Free heap after sleep: %lu bytes", (unsigned long)esp_get_free_heap_size());
    
    // 检查唤醒原因
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Wakeup caused by timer");
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            ESP_LOGI(TAG, "Wakeup caused by external signal using RTC_IO");
            break;
        default:
            ESP_LOGI(TAG, "Wakeup was not caused by deep sleep: %d", wakeup_reason);
            break;
    }
}

// 😴 2. Deep Sleep - 深度睡眠（仅RTC内存保持）
void enter_deep_sleep(uint32_t sleep_time_s) {
    ESP_LOGI(TAG, "😴 Preparing for Deep Sleep for %lu seconds...", (unsigned long)sleep_time_s);
    
    // 配置唤醒源
    // 1. 定时器唤醒
    esp_sleep_enable_timer_wakeup(sleep_time_s * 1000000);  // 微秒
    
    // 2. GPIO唤醒（EXT0 - 单个GPIO）
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);  // GPIO0 低电平唤醒
    
    // 3. GPIO唤醒（EXT1 - 多个GPIO）
    const uint64_t ext_wakeup_pin_1_mask = 1ULL << GPIO_NUM_2;
    const uint64_t ext_wakeup_pin_2_mask = 1ULL << GPIO_NUM_4;
    esp_sleep_enable_ext1_wakeup(ext_wakeup_pin_1_mask | ext_wakeup_pin_2_mask, ESP_EXT1_WAKEUP_ANY_HIGH);
    
    // 4. TouchPad唤醒
    touch_pad_init();
    touch_pad_config(TOUCH_PAD_NUM8);  // 触摸阈值
    esp_sleep_enable_touchpad_wakeup();
    
    // 保存关键数据到RTC内存
    RTC_DATA_ATTR static int boot_count = 0;
    boot_count++;
    ESP_LOGI(TAG, "Boot count: %d", boot_count);
    
    ESP_LOGI(TAG, "💾 Saving critical data to RTC memory...");
    ESP_LOGI(TAG, "🔌 Disabling peripherals...");
    
    ESP_LOGI(TAG, "💤 Entering Deep Sleep NOW!");
    esp_deep_sleep_start();
    
    // 这行代码永远不会执行，因为ESP32会重启
    ESP_LOGI(TAG, "This will never be printed");
}

// 🥶 3. Hibernation Mode - 休眠模式（超低功耗）
void enter_hibernation(uint32_t sleep_time_s) {
    ESP_LOGI(TAG, "🥶 Entering Hibernation Mode for %lu seconds...", (unsigned long)sleep_time_s);
    
    // 配置定时器唤醒
    esp_sleep_enable_timer_wakeup(sleep_time_s * 1000000);
    
    // 关闭所有RTC外设
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_OFF);
    
    ESP_LOGI(TAG, "❄️ Entering deepest sleep mode...");
    esp_deep_sleep_start();
}

// 🌅 检查唤醒原因
void check_wakeup_reason(void) {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    ESP_LOGI(TAG, "🌅 ESP32-S3 Wake Up!");
    
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            ESP_LOGI(TAG, "Wakeup caused by external signal using RTC_IO");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            ESP_LOGI(TAG, "Wakeup caused by external signal using RTC_CNTL");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Wakeup caused by timer");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            ESP_LOGI(TAG, "Wakeup caused by touchpad");
            ESP_LOGI(TAG, "Touch pad: %s", esp_sleep_get_touchpad_wakeup_status() ? "TOUCH_PAD_NUM8" : "Unknown");
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            ESP_LOGI(TAG, "Wakeup caused by ULP program");
            break;
        default:
            ESP_LOGI(TAG, "Wakeup was not caused by deep sleep: %d", wakeup_reason);
            break;
    }
}

// ⚙️ 配置自动低功耗管理
void configure_auto_power_management(void)
{
    ESP_LOGI(TAG, "Configuring automatic power management");

#ifdef CONFIG_PM_ENABLE
    // 配置自动调频和tickless idle
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 240,            // 最大CPU频率
        .min_freq_mhz = 80,             // 最小CPU频率
        .light_sleep_enable = true      // 启用自动浅睡眠
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
    ESP_LOGI(TAG, "Automatic power management configured: min_freq=%d, max_freq=%d, light_sleep=%d",
             pm_config.min_freq_mhz, pm_config.max_freq_mhz, pm_config.light_sleep_enable);
#else
    ESP_LOGW(TAG, "Power management is not enabled in project configuration");
    ESP_LOGI(TAG, "To enable, set CONFIG_PM_ENABLE=y in sdkconfig or run 'idf.py menuconfig'");
#endif
}

// 🎛️ 智能电源管理演示
void power_management_demo(void) {
    ESP_LOGI(TAG, "🎛️ Power Management Demo Starting...");
    
    // 检查启动原因
    check_wakeup_reason();
    
    // 配置自动功耗管理
    configure_auto_power_management();
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "🔋 Power Consumption Comparison:");
    ESP_LOGI(TAG, "  Active Mode:    ~240mA (CPU + WiFi + peripherals)");
    ESP_LOGI(TAG, "  Light Sleep:    ~0.8mA (RAM preserved, quick wake)");
    ESP_LOGI(TAG, "  Deep Sleep:     ~10µA (RTC only, full restart)");
    ESP_LOGI(TAG, "  Hibernation:    ~2.5µA (minimal RTC, full restart)");
    ESP_LOGI(TAG, "");
    
    // 模拟一些工作
    ESP_LOGI(TAG, "💼 Doing some work for 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 仅演示Light Sleep，避免在LVGL运行时重启系统
    ESP_LOGI(TAG, "🧪 Demo: Light Sleep (5 seconds)");
    enter_light_sleep(5000);
    
    ESP_LOGI(TAG, "🎛️ Power management demo completed!");
    ESP_LOGI(TAG, "💡 To test Deep Sleep/Hibernation, comment out LVGL task and use dedicated sleep demo");
} 