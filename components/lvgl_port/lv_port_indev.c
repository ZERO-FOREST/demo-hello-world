/**
 * @file lv_port_indev.c
 * @brief LVGL Input Device Port for ESP32-S3 with XPT2046
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "xpt2046.h"

/*********************
 *      DEFINES
 *********************/

static const char* TAG = "lv_port_indev";

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data);
static bool touchpad_is_pressed(void);
static void touchpad_get_xy(lv_coord_t* x, lv_coord_t* y);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_indev_t* indev_touchpad;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void) {
    /**
     * Here you will find example implementation of input devices supported by LittelvGL:
     *  - Touchpad
     *  - Mouse (with cursor support)
     *  - Keypad (supports GUI usage only with key)
     *  - Encoder (supports GUI usage only with: left, right, push)
     *  - Button (external buttons to press points on the screen)
     *
     *  The `..._read()` function are only examples.
     *  You should shape them according to your hardware
     */

    /*------------------
     * Touchpad
     * -----------------*/

    /*Initialize your touchpad if you have*/
    touchpad_init();

    /*Register a touchpad input device*/
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "Input device initialized successfully");
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*------------------
 * Touchpad
 * -----------------*/

/*Initialize your touchpad*/
static void touchpad_init(void) {
    /*Your code comes here*/
    ESP_LOGI(TAG, "Touchpad hardware initialization");

    // 注意：XPT2046与ST7789共享SPI2总线
    // 必须确保ST7789已经先初始化（即lv_port_disp_init()已被调用）
    // 因为ST7789负责初始化SPI总线，XPT2046只是添加设备到已有总线

    // 初始化XPT2046触摸控制器
    esp_err_t ret = xpt2046_init(240, 320); // 假设屏幕分辨率为320x240
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "XPT2046 initialization failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "XPT2046 initialized successfully (sharing SPI2 with ST7789)");
    }
}

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;
    static bool last_pressed = false;
    static uint32_t last_press_time = 0;
    static uint32_t press_count = 0;

    /*Save the pressed coordinates and the state*/
    if (touchpad_is_pressed()) {
        lv_coord_t current_x, current_y;
        touchpad_get_xy(&current_x, &current_y);
        
        // 基于厂商代码的防抖处理
        uint32_t current_time = esp_timer_get_time() / 1000; // 转换为毫秒
        
        if (!last_pressed) {
            // 第一次按下
            last_x = current_x;
            last_y = current_y;
            last_press_time = current_time;
            press_count = 1;
            data->state = LV_INDEV_STATE_PR;
            ESP_LOGI(TAG, "Touch PRESSED at x:%d y:%d", last_x, last_y);
        } else {
            // 持续按下，检查坐标变化
            int32_t dx = abs(current_x - last_x);
            int32_t dy = abs(current_y - last_y);
            
            // 如果坐标变化超过阈值，更新坐标
            if (dx > 5 || dy > 5) {
                last_x = current_x;
                last_y = current_y;
                ESP_LOGI(TAG, "Touch MOVING at x:%d y:%d", last_x, last_y);
            }
            
            // 防止长时间按下导致的误触
            if (current_time - last_press_time > 10000) { // 10秒超时
                data->state = LV_INDEV_STATE_REL;
                last_pressed = false;
                ESP_LOGI(TAG, "Touch timeout, releasing");
            } else {
                data->state = LV_INDEV_STATE_PR;
                press_count++;
            }
        }
        last_pressed = true;
    } else {
        data->state = LV_INDEV_STATE_REL;

        // 显示触摸释放
        if (last_pressed) {
            ESP_LOGI(TAG, "Touch RELEASED at x:%d y:%d (press_count:%d)", last_x, last_y, press_count);
        }
        last_pressed = false;
        press_count = 0;
    }

    /*Set the last pressed coordinates*/
    data->point.x = last_x;
    data->point.y = last_y;
}

/*Return true is the touchpad is pressed*/
static bool touchpad_is_pressed(void) {
    /*Your code comes here*/
    return xpt2046_is_touched();
}

/*Get the x and y coordinates if the touchpad is pressed*/
static void touchpad_get_xy(lv_coord_t* x, lv_coord_t* y) {
    /*Your code comes here*/
    int16_t touch_x, touch_y;
    bool pressed;

    esp_err_t ret = xpt2046_read_touch(&touch_x, &touch_y, &pressed);
    if (ret == ESP_OK && pressed) {
        *x = touch_x;
        *y = touch_y;
    } else {
        *x = 0;
        *y = 0;
    }
}
