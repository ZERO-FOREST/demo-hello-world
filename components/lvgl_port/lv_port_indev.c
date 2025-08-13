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

static const char* TAG = "lv_port_indev";

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data);
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

    /* input device ready */
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*------------------
 * Touchpad
 * -----------------*/

/*Initialize your touchpad*/
static void touchpad_init(void) {
    /* XPT2046 shares SPI2 bus with ST7789; ensure display init ran earlier */
    (void)xpt2046_init(240, 320);
    /* Swap axes so that x is horizontal (right), y is vertical (down) */
    xpt2046_handle_t* h = xpt2046_get_handle();
    h->calibration.swap_xy = true;
    h->calibration.invert_x = false;
    h->calibration.invert_y = false;
}

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    /* Use internal pressure-based pressed detection (IRQ may be broken) */
    xpt2046_data_t raw;
    (void)xpt2046_read_raw(&raw);
    if (raw.pressed) {
        touchpad_get_xy(&last_x, &last_y);
        data->state = LV_INDEV_STATE_PR;
        ESP_LOGI(TAG, "Touchpad read: x=%d, y=%d", last_x, last_y);
    } else {
        data->state = LV_INDEV_STATE_REL;
    }

    data->point.x = last_x;
    data->point.y = last_y;
}

/*Get the x and y coordinates if the touchpad is pressed*/
static void touchpad_get_xy(lv_coord_t* x, lv_coord_t* y) {
    xpt2046_data_t raw;
    (void)xpt2046_read_raw(&raw);

    int16_t rx = raw.x;
    int16_t ry = raw.y;

    xpt2046_handle_t* h = xpt2046_get_handle();
    const xpt2046_calibration_t* cal = &h->calibration;

    int16_t px = rx;
    int16_t py = ry;

    if (cal->swap_xy) {
        int16_t tmp = px; px = py; py = tmp;
    }
    if (cal->invert_x) px = 4095 - px;
    if (cal->invert_y) py = 4095 - py;

    int32_t sx = (int32_t)(px - cal->x_min) * (int32_t)h->screen_width / (int32_t)(cal->x_max - cal->x_min);
    int32_t sy = (int32_t)(py - cal->y_min) * (int32_t)h->screen_height / (int32_t)(cal->y_max - cal->y_min);

    if (sx < 0) {
        sx = 0;
    } else if (sx >= h->screen_width) {
        sx = h->screen_width - 1;
    }
    if (sy < 0) {
        sy = 0;
    } else if (sy >= h->screen_height) {
        sy = h->screen_height - 1;
    }

    *x = (lv_coord_t)sx;
    *y = (lv_coord_t)sy;
}
