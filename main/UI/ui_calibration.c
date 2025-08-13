/**
 * @file ui_calibration.c
 * @brief 校准和测试界面 - 支持各种外设的校准和测试
 * @author Your Name
 * @date 2024
 */
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lvgl.h"

#include "ui.h"
#include "calibration_manager.h"
#include "joystick_adc.h"
#include "lsm6ds3.h"

static const char *TAG = "UI_CALIBRATION";

// 界面状态
typedef enum {
    CALIBRATION_STATE_MAIN_MENU,
    CALIBRATION_STATE_JOYSTICK_TEST,
    CALIBRATION_STATE_GYROSCOPE_TEST,
    CALIBRATION_STATE_ACCELEROMETER_TEST,
    CALIBRATION_STATE_TOUCHSCREEN_TEST
} calibration_state_t;

// 全局变量
static lv_obj_t *g_calibration_screen = NULL;
static lv_obj_t *g_status_bar = NULL;
static lv_obj_t *g_content_area = NULL;
static lv_obj_t *g_info_label = NULL;
static lv_obj_t *g_back_btn = NULL;
static lv_obj_t *g_calibrate_btn = NULL;
static lv_obj_t *g_test_btn = NULL;

// 当前状态
static calibration_state_t g_current_state = CALIBRATION_STATE_MAIN_MENU;
static bool g_test_running = false;
static TaskHandle_t g_test_task_handle = NULL;
static QueueHandle_t g_test_queue = NULL;

// 消息类型
typedef enum {
    MSG_UPDATE_JOYSTICK,
    MSG_UPDATE_GYROSCOPE,
    MSG_UPDATE_ACCELEROMETER,
    MSG_STOP_TEST
} test_msg_type_t;

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

// 按钮事件回调
static void back_btn_event_cb(lv_event_t *e)
{
    if (g_test_running) {
        // 停止测试
        test_msg_t msg = {.type = MSG_STOP_TEST};
        xQueueSend(g_test_queue, &msg, pdMS_TO_TICKS(100));
        g_test_running = false;
    }
    
    if (g_current_state == CALIBRATION_STATE_MAIN_MENU) {
        // 返回主菜单
        lv_obj_t *screen = lv_scr_act();
        if (screen) {
            lv_obj_clean(screen);
            ui_main_menu_create(screen);
        }
    } else {
        // 返回校准主菜单
        g_current_state = CALIBRATION_STATE_MAIN_MENU;
        create_main_menu();
    }
}

static void calibrate_btn_event_cb(lv_event_t *e)
{
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
            create_main_menu();
        }
    } else {
        ESP_LOGE(TAG, "Calibration failed: %s", esp_err_to_name(ret));
    }
}

static void test_btn_event_cb(lv_event_t *e)
{
    if (g_test_running) {
        // 停止测试
        test_msg_t msg = {.type = MSG_STOP_TEST};
        xQueueSend(g_test_queue, &msg, pdMS_TO_TICKS(100));
        g_test_running = false;
        
        lv_obj_t *btn = lv_event_get_target(e);
        lv_obj_t *label = lv_obj_get_child(btn, 0);
        lv_label_set_text(label, "Start Test");
    } else {
        // 开始测试
        g_test_running = true;
        
        lv_obj_t *btn = lv_event_get_target(e);
        lv_obj_t *label = lv_obj_get_child(btn, 0);
        lv_label_set_text(label, "Stop Test");
    }
}

static void menu_btn_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(btn);
    
    switch (index) {
        case 0: // Joystick Test
            g_current_state = CALIBRATION_STATE_JOYSTICK_TEST;
            create_joystick_test();
            break;
        case 1: // Gyroscope Test
            g_current_state = CALIBRATION_STATE_GYROSCOPE_TEST;
            create_gyroscope_test();
            break;
        case 2: // Accelerometer Test
            g_current_state = CALIBRATION_STATE_ACCELEROMETER_TEST;
            create_accelerometer_test();
            break;
        case 3: // Touchscreen Test
            g_current_state = CALIBRATION_STATE_TOUCHSCREEN_TEST;
            // TODO: 实现触摸屏测试
            break;
    }
}

// 创建状态栏
static void create_status_bar(lv_obj_t *parent)
{
    g_status_bar = lv_obj_create(parent);
    lv_obj_set_size(g_status_bar, 320, 40);
    lv_obj_align(g_status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(g_status_bar, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_bg_opa(g_status_bar, LV_OPA_90, 0);
    lv_obj_set_style_radius(g_status_bar, 0, 0);
    lv_obj_set_style_border_width(g_status_bar, 0, 0);
    lv_obj_set_style_pad_all(g_status_bar, 5, 0);

    // 标题
    lv_obj_t *title = lv_label_create(g_status_bar);
    lv_label_set_text(title, "Calibration & Test");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
}

// 创建主菜单
static void create_main_menu(void)
{
    if (!g_content_area) return;
    
    lv_obj_clean(g_content_area);
    
    // 校准状态显示
    const calibration_status_t *status = get_calibration_status();
    if (status) {
        char status_text[256];
        snprintf(status_text, sizeof(status_text),
                "Calibration Status:\n"
                "Joystick: %s\n"
                "Gyroscope: %s\n"
                "Accelerometer: %s\n"
                "Battery: %s\n"
                "Touchscreen: %s",
                status->joystick_calibrated ? "✓" : "✗",
                status->gyroscope_calibrated ? "✓" : "✗",
                status->accelerometer_calibrated ? "✓" : "✗",
                status->battery_calibrated ? "✓" : "✗",
                status->touchscreen_calibrated ? "✓" : "✗");
        
        g_info_label = lv_label_create(g_content_area);
        lv_label_set_text(g_info_label, status_text);
        lv_obj_align(g_info_label, LV_ALIGN_TOP_MID, 0, 10);
        lv_obj_set_style_text_font(g_info_label, &lv_font_montserrat_14, 0);
    }
    
    // 创建菜单按钮
    const char *menu_items[] = {
        "Joystick Test",
        "Gyroscope Test", 
        "Accelerometer Test",
        "Touchscreen Test"
    };
    
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_btn_create(g_content_area);
        lv_obj_set_size(btn, 200, 40);
        lv_obj_align(btn, LV_ALIGN_CENTER, 0, 60 + i * 50);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x3498DB), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_80, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, menu_items[i]);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(label);
        
        // 设置按钮事件
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_CLICKED, NULL);
    }
}

// 创建校准界面
void ui_calibration_create(lv_obj_t *parent)
{
    g_calibration_screen = parent;
    g_current_state = CALIBRATION_STATE_MAIN_MENU;
    
    // 创建消息队列
    g_test_queue = xQueueCreate(10, sizeof(test_msg_t));
    if (!g_test_queue) {
        ESP_LOGE(TAG, "Failed to create test queue");
        return;
    }
    
    // 创建状态栏
    create_status_bar(parent);
    
    // 创建内容区域
    g_content_area = lv_obj_create(parent);
    lv_obj_set_size(g_content_area, 300, 240);
    lv_obj_align(g_content_area, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_opa(g_content_area, LV_OPA_0, 0);
    lv_obj_set_style_border_width(g_content_area, 0, 0);
    lv_obj_set_style_pad_all(g_content_area, 0, 0);
    
    // 创建主菜单
    create_main_menu();
    
    // 创建按钮容器
    lv_obj_t *btn_cont = lv_obj_create(parent);
    lv_obj_set_size(btn_cont, 300, 50);
    lv_obj_align(btn_cont, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_style_pad_all(btn_cont, 0, 0);
    
    // 返回按钮
    g_back_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(g_back_btn, 80, 40);
    lv_obj_align(g_back_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(g_back_btn, lv_color_hex(0xE74C3C), 0);
    lv_obj_set_style_bg_opa(g_back_btn, LV_OPA_80, 0);
    lv_obj_set_style_radius(g_back_btn, 8, 0);
    lv_obj_add_event_cb(g_back_btn, back_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(g_back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);
    
    // 校准按钮
    g_calibrate_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(g_calibrate_btn, 80, 40);
    lv_obj_align(g_calibrate_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_calibrate_btn, lv_color_hex(0x27AE60), 0);
    lv_obj_set_style_bg_opa(g_calibrate_btn, LV_OPA_80, 0);
    lv_obj_set_style_radius(g_calibrate_btn, 8, 0);
    lv_obj_add_event_cb(g_calibrate_btn, calibrate_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *calibrate_label = lv_label_create(g_calibrate_btn);
    lv_label_set_text(calibrate_label, "Calibrate");
    lv_obj_set_style_text_color(calibrate_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(calibrate_label);
    
    // 测试按钮
    g_test_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(g_test_btn, 80, 40);
    lv_obj_align(g_test_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(g_test_btn, lv_color_hex(0xF39C12), 0);
    lv_obj_set_style_bg_opa(g_test_btn, LV_OPA_80, 0);
    lv_obj_set_style_radius(g_test_btn, 8, 0);
    lv_obj_add_event_cb(g_test_btn, test_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *test_label = lv_label_create(g_test_btn);
    lv_label_set_text(test_label, "Start Test");
    lv_obj_set_style_text_color(test_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(test_label);
    
    ESP_LOGI(TAG, "Calibration UI created successfully");
}

// 销毁校准界面
void ui_calibration_destroy(void)
{
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
    g_calibration_screen = NULL;
    g_status_bar = NULL;
    g_content_area = NULL;
    g_info_label = NULL;
    g_back_btn = NULL;
    g_calibrate_btn = NULL;
    g_test_btn = NULL;
    
    ESP_LOGI(TAG, "Calibration UI destroyed");
}
