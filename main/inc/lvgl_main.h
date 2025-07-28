#ifndef LVGL_MAIN_H
#define LVGL_MAIN_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 🔧 修复：去掉static，让函数可以被外部调用
void lvgl_main_task(void *pvParameters);

#endif