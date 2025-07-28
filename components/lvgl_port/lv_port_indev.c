/**
 * @file lv_port_indev.c
 * @brief LVGL Input Device Port for ESP32-S3 with XPT2046
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "xpt2046.h"
#include "esp_log.h"

/*********************
 *      DEFINES
 *********************/

static const char *TAG = "lv_port_indev";

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static bool touchpad_is_pressed(void);
static void touchpad_get_xy(lv_coord_t * x, lv_coord_t * y);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_indev_t * indev_touchpad;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void)
{
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
static void touchpad_init(void)
{
    /*Your code comes here*/
    ESP_LOGI(TAG, "Touchpad hardware initialization");
    
    // 注意：XPT2046与ST7789共享SPI2总线
    // 必须确保ST7789已经先初始化（即lv_port_disp_init()已被调用）
    // 因为ST7789负责初始化SPI总线，XPT2046只是添加设备到已有总线
    
    // 初始化XPT2046触摸控制器
    esp_err_t ret = xpt2046_init(320, 240);  // 假设屏幕分辨率为320x240
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "XPT2046 initialization failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "XPT2046 initialized successfully (sharing SPI2 with ST7789)");
    }
}

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    /*Save the pressed coordinates and the state*/
    if(touchpad_is_pressed()) {
        touchpad_get_xy(&last_x, &last_y);
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }

    /*Set the last pressed coordinates*/
    data->point.x = last_x;
    data->point.y = last_y;
}

/*Return true is the touchpad is pressed*/
static bool touchpad_is_pressed(void)
{
    /*Your code comes here*/
    return xpt2046_is_touched();
}

/*Get the x and y coordinates if the touchpad is pressed*/
static void touchpad_get_xy(lv_coord_t * x, lv_coord_t * y)
{
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
