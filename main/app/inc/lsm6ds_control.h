#ifndef LSM6DS_CONTROL_H
#define LSM6DS_CONTROL_H
#include "lsm6ds3.h"
#include "esp_err.h"

extern TaskHandle_t s_lsm6ds3_control_task;

/**
 * @brief 初始化LSM6DS3控制任务
 */
esp_err_t init_lsm6ds3_control_task(void);

/**
 * @brief 获取LSM6DS3控制任务句柄
 */
TaskHandle_t get_lsm6ds3_control_task_handle(void);

#endif // LSM6DS_CONTROL_H