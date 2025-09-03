/*
 * @Author: tidycraze 2595256284@qq.com
 * @Date: 2025-07-28 11:29:59
 * @LastEditors: tidycraze 2595256284@qq.com
 * @LastEditTime: 2025-08-23 10:13:03
 * @FilePath: \demo-hello-world\main\main.c
 * @Description: 主函数入口
 *
 */

#if EN_RECEIVER_MODE

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "spi_slave_receiver.h"
#include "usb_device_receiver.h"
#include <inttypes.h>
#include <stdio.h>
#include "nvs_flash.h"

static const char* TAG = "MAIN";

void app_main(void) {

    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return;
    }

    // 初始化 USB CDC 从机并启动接收任务
    if (usb_receiver_init() == ESP_OK) {
        usb_receiver_start();
    }

    // 初始化 SPI 从机并启动接收任务
    if (spi_receiver_init() == ESP_OK) {
        spi_receiver_start();
    }

    while (1) {
        ESP_LOGI(TAG, "Receiver running, free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
#else

#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "task_init.h"
#include <inttypes.h>
#include <stdio.h>

static const char* TAG = "MAIN";

extern esp_err_t components_init(void);

void app_main() {
    // 启用看门狗（默认超时5秒）
    esp_task_wdt_init(5, true);
    esp_task_wdt_add(NULL);
    
    // 初始化各组件
    components_init();
}

#endif

/**

如果 |X| > 0.75 g 且 |X| > |Y| → 判定为横屏

如果 |Y| > 0.75 g 且 |Y| > |X| → 判定为竖屏

如果 |Z| > 0.8 g → 判定为平放，不旋转

*/