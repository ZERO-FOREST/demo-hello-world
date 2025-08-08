/**
 * @file lv_port_disp.c
 * ESP32-S3 Display Port for LVGL
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include <stdbool.h>
#include "esp_log.h"
#include "st7789.h"

/*********************
 *      DEFINES
 *********************/
#define MY_DISP_HOR_RES    ST7789_WIDTH
#define MY_DISP_VER_RES    ST7789_HEIGHT

static const char *TAG = "lv_port_disp";

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

/**********************
 *  STATIC VARIABLES
 **********************/
static bool disp_flush_enabled = true;

/* 将显示缓冲放入PSRAM，减小内置RAM占用，并加大块尺寸 */
#include "esp_heap_caps.h"
static lv_color_t *disp_buf_1 = NULL;
static lv_color_t *disp_buf_2 = NULL;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*-----------------------------
     * Create a buffer for drawing
     *----------------------------*/
    static lv_disp_draw_buf_t draw_buf_dsc;
    /* 分配更大的双缓冲到PSRAM，例如每缓冲 120 行 */
    size_t lines = MY_DISP_VER_RES; // 可根据内存情况调大/调小
    size_t buf_pixels = MY_DISP_HOR_RES * lines;
    disp_buf_1 = (lv_color_t*)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    disp_buf_2 = (lv_color_t*)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lv_disp_draw_buf_init(&draw_buf_dsc, disp_buf_1, disp_buf_2, buf_pixels);

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/
    static lv_disp_drv_t disp_drv;                         /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);                    /*Basic initialization*/

    /*Set up the functions to access to your display*/
    disp_drv.hor_res = MY_DISP_HOR_RES;
    disp_drv.ver_res = MY_DISP_VER_RES;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf_dsc;

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);
    
    ESP_LOGI(TAG, "Display port initialized successfully (buf lines=%d)", (int)lines);
}

void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    /*You code here*/
    ESP_LOGI(TAG, "Display hardware initialization");
    st7789_init();
}

volatile bool disp_flush_ready = false;

/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    if(disp_flush_enabled) {
        st7789_set_window(area->x1, area->y1, area->x2, area->y2);
        size_t pixel_count = lv_area_get_size(area);
        st7789_write_pixels((uint16_t*)color_p, pixel_count);
    }

    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(disp_drv);
} 