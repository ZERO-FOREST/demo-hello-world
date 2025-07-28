#ifndef POWER_MANAGEMENT_H
#define POWER_MANAGEMENT_H

#include <stdint.h>

// 🛌 低功耗模式函数
void enter_light_sleep(uint32_t sleep_time_ms);
void enter_deep_sleep(uint32_t sleep_time_s);
void enter_hibernation(uint32_t sleep_time_s);

// 🌅 唤醒和检查函数
void check_wakeup_reason(void);

// ⚙️ 电源管理配置
void configure_auto_power_management(void);

// 🎛️ 演示函数
void power_management_demo(void);

#endif // POWER_MANAGEMENT_H 