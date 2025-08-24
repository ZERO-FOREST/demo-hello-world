/**
 * @file ui_calibration.c
 * @brief 校准和测试界面 - 支持各种外设的校准和测试
 * @author Your Name
 * @date 2024
 */
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "Mysybmol.h"
#include "calibration_manager.h"
#include "joystick_adc.h"
#include "lsm6ds3.h"
#include "theme_manager.h"
#include "ui.h"

static const char* TAG = "UI_CALIBRATION";

// 界面状态
typedef enum {
    CALIBRATION_STATE_MAIN_MENU,
    CALIBRATION_STATE_JOYSTICK_TEST,
    CALIBRATION_STATE_GYROSCOPE_TEST,
    CALIBRATION_STATE_ACCELEROMETER_TEST,
    CALIBRATION_STATE_TOUCHSCREEN_TEST
} calibration_state_t;

// 前向声明
static void create_main_menu(lv_obj_t* content_container);
static void create_joystick_test(lv_obj_t* content_container);
static void create_gyroscope_test(lv_obj_t* content_container);
static void create_accelerometer_test(lv_obj_t* content_container);

// 全局变量
static lv_obj_t* g_page_parent_container = NULL;
static lv_obj_t* g_content_container = NULL;
static lv_obj_t* g_info_label = NULL;
static lv_obj_t* g_calibrate_btn = NULL;
static lv_obj_t* g_test_btn = NULL;

// 当前状态
static calibration_state_t g_current_state = CALIBRATION_STATE_MAIN_MENU;
static bool g_test_running = false;
static TaskHandle_t g_test_task_handle = NULL;
static QueueHandle_t g_test_queue = NULL;

// 消息类型
typedef enum { MSG_UPDATE_JOYSTICK, MSG_UPDATE_GYROSCOPE, MSG_UPDATE_ACCELEROMETER, MSG_STOP_TEST } test_msg_type_t;

typedef struct {
    test_msg_type_t type;
    union {
        struct {
            int16_t x, y;
        } joystick;
        struct {
            float x, y, z;
        } gyro;
        struct {
            float x, y, z;
        } accel;
    } data;
} test_msg_t;

// 自定义返回按钮回调 - 处理校准界面的特殊逻辑
static void calibration_back_btn_callback(lv_event_t* e) {
    if (g_test_running) {
        // 停止测试
        test_msg_t msg = {.type = MSG_STOP_TEST};
        xQueueSend(g_test_queue, &msg, pdMS_TO_TICKS(100));
        g_test_running = false;
    }

    if (g_current_state == CALIBRATION_STATE_MAIN_MENU) {
        // 返回主菜单
        lv_obj_t* screen = lv_scr_act();
        if (screen) {
            lv_obj_clean(screen);
            ui_main_menu_create(screen);
        }
    } else {
        // 返回校准主菜单
        g_current_state = CALIBRATION_STATE_MAIN_MENU;
        create_main_menu(g_content_container);
    }
}

static void calibrate_btn_event_cb(lv_event_t* e) {
    esp_err_t ret = ESP_OK;

    switch (g_current_state) {
    case CALIBRATION_STATE_JOYSTICK_TEST:
        ret = calibrate_joystick();
        break;
    case CALIBRATION_STATE_GYROSCOPE_TEST:
        ret = calibrate_gyroscope();
        break;
    case CALIBRATION_STATE_ACCELEROMETER_TEST:
        ret = calibrate_accelerometer();
        break;
    default:
        break;
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration completed successfully");
        // 更新状态显示
        if (g_current_state == CALIBRATION_STATE_MAIN_MENU) {
            create_main_menu(g_content_container);
        }
    } else {
        ESP_LOGE(TAG, "Calibration failed: %s", esp_err_to_name(ret));
    }
}

static void test_btn_event_cb(lv_event_t* e) {
    if (g_test_running) {
        // 停止测试
        test_msg_t msg = {.type = MSG_STOP_TEST};
        xQueueSend(g_test_queue, &msg, pdMS_TO_TICKS(100));
        g_test_running = false;

        lv_obj_t* btn = lv_event_get_target(e);
        lv_obj_t* label = lv_obj_get_child(btn, 0);
        lv_label_set_text(label, "Start Test");
    } else {
        // 开始测试
        g_test_running = true;

        lv_obj_t* btn = lv_event_get_target(e);
        lv_obj_t* label = lv_obj_get_child(btn, 0);
        lv_label_set_text(label, "Stop Test");
    }
}

static void menu_btn_event_cb(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(btn);

    switch (index) {
    case 0: // Joystick Test
        g_current_state = CALIBRATION_STATE_JOYSTICK_TEST;
        create_joystick_test(g_content_container);
        break;
    case 1: // Gyroscope Test
        g_current_state = CALIBRATION_STATE_GYROSCOPE_TEST;
        create_gyroscope_test(g_content_container);
        break;
    case 2: // Accelerometer Test
        g_current_state = CALIBRATION_STATE_ACCELEROMETER_TEST;
        create_accelerometer_test(g_content_container);
        break;
    case 3: // Touchscreen Test
        g_current_state = CALIBRATION_STATE_TOUCHSCREEN_TEST;
        // TODO: 实现触摸屏测试
        break;
    }
}

// 创建主菜单
static void create_main_menu(lv_obj_t* content_container) {
    if (!content_container)
        return;

    lv_obj_clean(content_container);

    // 校准状态显示
    const calibration_status_t* status = get_calibration_status();
    if (status) {
        char status_text[256];
        snprintf(status_text, sizeof(status_text),
                 "校准状态:\n"
                 "摇杆: %s\n"
                 "陀螺仪: %s\n"
                 "加速度计: %s\n"
                 "电池: %s\n"
                 "触摸屏: %s",
                 status->joystick_calibrated ? "已校准" : "未校准", status->gyroscope_calibrated ? "已校准" : "未校准",
                 status->accelerometer_calibrated ? "已校准" : "未校准",
                 status->battery_calibrated ? "已校准" : "未校准",
                 status->touchscreen_calibrated ? "已校准" : "未校准");

        g_info_label = lv_label_create(content_container);
        lv_label_set_text(g_info_label, status_text);
        theme_apply_to_label(g_info_label, false);
        lv_obj_align(g_info_label, LV_ALIGN_TOP_MID, 0, 10);
        lv_font_t* loaded_font = get_loaded_font();
        lv_obj_set_style_text_font(g_info_label, loaded_font, 0);
    }

    // 创建菜单按钮
    const char* menu_items[] = {"Joystick Test", "Gyroscope Test", "Accelerometer Test", "Touchscreen Test"};

    for (int i = 0; i < 4; i++) {
        lv_obj_t* btn = lv_btn_create(content_container);
        lv_obj_set_size(btn, 200, 40);
        lv_obj_align(btn, LV_ALIGN_CENTER, 0, 60 + i * 50);
        theme_apply_to_button(btn, true);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, menu_items[i]);
        lv_obj_center(label);

        // 设置按钮事件
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_CLICKED, NULL);
    }
}

// 创建校准界面
void ui_calibration_create(lv_obj_t* parent) {
    g_current_state = CALIBRATION_STATE_MAIN_MENU;

    // 创建消息队列
    g_test_queue = xQueueCreate(10, sizeof(test_msg_t));
    if (!g_test_queue) {
        ESP_LOGE(TAG, "Failed to create test queue");
        return;
    }

    // 应用当前主题到屏幕
    theme_apply_to_screen(parent);

    // 1. 创建页面父级容器（统一管理整个页面）
    ui_create_page_parent_container(parent, &g_page_parent_container);

    // 2. 创建顶部栏容器（包含返回按钮和标题）
    lv_obj_t* top_bar_container;
    lv_obj_t* title_container;
    ui_create_top_bar(g_page_parent_container, "Calibration & Test", false, &top_bar_container, &title_container, NULL);

    // 替换顶部栏的返回按钮回调为自定义回调
    lv_obj_t* back_btn = lv_obj_get_child(top_bar_container, 0); // 获取返回按钮
    if (back_btn) {
        lv_obj_remove_event_cb(back_btn, NULL); // 移除默认回调
        lv_obj_add_event_cb(back_btn, calibration_back_btn_callback, LV_EVENT_CLICKED, NULL);
    }

    // 3. 创建页面内容容器
    ui_create_page_content_area(g_page_parent_container, &g_content_container);

    // 4. 创建主菜单
    create_main_menu(g_content_container);

    // 5. 创建底部按钮容器
    lv_obj_t* btn_cont = lv_obj_create(g_page_parent_container);
    lv_obj_set_size(btn_cont, 240, 50);
    lv_obj_align(btn_cont, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_style_pad_all(btn_cont, 0, 0);

    // 校准按钮
    g_calibrate_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(g_calibrate_btn, 80, 40);
    lv_obj_align(g_calibrate_btn, LV_ALIGN_LEFT_MID, 10, 0);
    theme_apply_to_button(g_calibrate_btn, true);
    lv_obj_add_event_cb(g_calibrate_btn, calibrate_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* calibrate_label = lv_label_create(g_calibrate_btn);
    lv_label_set_text(calibrate_label, "Calibrate");
    lv_obj_center(calibrate_label);

    // 测试按钮
    g_test_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(g_test_btn, 80, 40);
    lv_obj_align(g_test_btn, LV_ALIGN_RIGHT_MID, -10, 0);
    theme_apply_to_button(g_test_btn, true);
    lv_obj_add_event_cb(g_test_btn, test_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* test_label = lv_label_create(g_test_btn);
    lv_label_set_text(test_label, "Start Test");
    lv_obj_center(test_label);

    ESP_LOGI(TAG, "Calibration UI created successfully");
}

// 销毁校准界面
void ui_calibration_destroy(void) {
    // 停止测试
    if (g_test_running) {
        test_msg_t msg = {.type = MSG_STOP_TEST};
        xQueueSend(g_test_queue, &msg, pdMS_TO_TICKS(100));
        g_test_running = false;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 删除测试任务
    if (g_test_task_handle) {
        vTaskDelete(g_test_task_handle);
        g_test_task_handle = NULL;
    }

    // 删除消息队列
    if (g_test_queue) {
        vQueueDelete(g_test_queue);
        g_test_queue = NULL;
    }

    // 清空全局变量
    g_page_parent_container = NULL;
    g_content_container = NULL;
    g_info_label = NULL;
    g_calibrate_btn = NULL;
    g_test_btn = NULL;

    ESP_LOGI(TAG, "Calibration UI destroyed");
}

// 创建摇杆测试界面
static void create_joystick_test(lv_obj_t* content_container) {
    if (!content_container)
        return;

    lv_obj_clean(content_container);

    // 创建摇杆显示区域
    lv_obj_t* joystick_area = lv_obj_create(content_container);
    lv_obj_set_size(joystick_area, 200, 200);
    lv_obj_align(joystick_area, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(joystick_area, lv_color_hex(0x34495E), 0);
    lv_obj_set_style_bg_opa(joystick_area, LV_OPA_50, 0);
    lv_obj_set_style_radius(joystick_area, 100, 0);
    lv_obj_set_style_border_width(joystick_area, 2, 0);
    lv_obj_set_style_border_color(joystick_area, lv_color_hex(0x95A5A6), 0);

    // 创建摇杆指示器
    lv_obj_t* joystick_indicator = lv_obj_create(joystick_area);
    lv_obj_set_size(joystick_indicator, 20, 20);
    lv_obj_align(joystick_indicator, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(joystick_indicator, lv_color_hex(0xE74C3C), 0);
    lv_obj_set_style_radius(joystick_indicator, 10, 0);
    lv_obj_set_user_data(joystick_area, joystick_indicator);

    // 创建数值显示标签
    lv_obj_t* value_label = lv_label_create(content_container);
    lv_label_set_text(value_label, "X: 0, Y: 0");
    lv_obj_align(value_label, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_14, 0);
    lv_obj_set_user_data(content_container, value_label);

    ESP_LOGI(TAG, "Joystick test interface created");
}

// 创建陀螺仪测试界面
static void create_gyroscope_test(lv_obj_t* content_container) {
    if (!content_container)
        return;

    lv_obj_clean(content_container);

    // 创建3D立方体显示区域
    lv_obj_t* cube_area = lv_obj_create(content_container);
    lv_obj_set_size(cube_area, 200, 200);
    lv_obj_align(cube_area, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(cube_area, lv_color_hex(0x34495E), 0);
    lv_obj_set_style_bg_opa(cube_area, LV_OPA_50, 0);
    lv_obj_set_style_radius(cube_area, 8, 0);
    lv_obj_set_style_border_width(cube_area, 2, 0);
    lv_obj_set_style_border_color(cube_area, lv_color_hex(0x95A5A6), 0);

    // 创建立方体指示器
    lv_obj_t* cube_indicator = lv_obj_create(cube_area);
    lv_obj_set_size(cube_indicator, 40, 40);
    lv_obj_align(cube_indicator, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(cube_indicator, lv_color_hex(0x9B59B6), 0);
    lv_obj_set_style_radius(cube_indicator, 4, 0);
    lv_obj_set_user_data(cube_area, cube_indicator);

    // 创建数值显示标签
    lv_obj_t* value_label = lv_label_create(content_container);
    lv_label_set_text(value_label, "X: 0.00, Y: 0.00, Z: 0.00");
    lv_obj_align(value_label, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_14, 0);
    lv_obj_set_user_data(content_container, value_label);

    ESP_LOGI(TAG, "Gyroscope test interface created");
}

// 创建加速度计测试界面
static void create_accelerometer_test(lv_obj_t* content_container) {
    if (!content_container)
        return;

    lv_obj_clean(content_container);

    // 创建重力指示器
    lv_obj_t* gravity_area = lv_obj_create(content_container);
    lv_obj_set_size(gravity_area, 200, 200);
    lv_obj_align(gravity_area, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(gravity_area, lv_color_hex(0x34495E), 0);
    lv_obj_set_style_bg_opa(gravity_area, LV_OPA_50, 0);
    lv_obj_set_style_radius(gravity_area, 100, 0);
    lv_obj_set_style_border_width(gravity_area, 2, 0);
    lv_obj_set_style_border_color(gravity_area, lv_color_hex(0x95A5A6), 0);

    // 创建重力指示器
    lv_obj_t* gravity_indicator = lv_obj_create(gravity_area);
    lv_obj_set_size(gravity_indicator, 30, 30);
    lv_obj_align(gravity_indicator, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(gravity_indicator, lv_color_hex(0xF39C12), 0);
    lv_obj_set_style_radius(gravity_indicator, 15, 0);
    lv_obj_set_user_data(gravity_area, gravity_indicator);

    // 创建数值显示标签
    lv_obj_t* value_label = lv_label_create(content_container);
    lv_label_set_text(value_label, "X: 0.00, Y: 0.00, Z: 0.00");
    lv_obj_align(value_label, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_14, 0);
    lv_obj_set_user_data(content_container, value_label);

    ESP_LOGI(TAG, "Accelerometer test interface created");
}
