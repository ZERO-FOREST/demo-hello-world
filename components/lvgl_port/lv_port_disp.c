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

/*********************
 *      DEFINES
 *********************/
#define MY_DISP_HOR_RES    320
#define MY_DISP_VER_RES    240

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

/* 针对ESP32-S3优化的显示缓冲区 */
static lv_color_t disp_buf_1[MY_DISP_HOR_RES * 40];
static lv_color_t disp_buf_2[MY_DISP_HOR_RES * 40];

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
    lv_disp_draw_buf_init(&draw_buf_dsc, disp_buf_1, disp_buf_2, MY_DISP_HOR_RES * 40);

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
    
    ESP_LOGI(TAG, "Display port initialized successfully");
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
    
    // TODO: 在这里添加你的显示器初始化代码
    // 例如：SPI初始化、GPIO配置、显示器复位等
}

volatile bool disp_flush_ready = false;

/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    if(disp_flush_enabled) {
        /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/
        
        // TODO: 在这里添加实际的显示数据传输代码
        // 例如：通过SPI发送color_p数据到显示器
        
        // 计算需要传输的像素数量
        int32_t x1 = area->x1;
        int32_t y1 = area->y1;
        int32_t x2 = area->x2;
        int32_t y2 = area->y2;
        
        /*Return if the area is out the screen*/
        if(x2 < 0) return;
        if(y2 < 0) return;
        if(x1 > MY_DISP_HOR_RES - 1) return;
        if(y1 > MY_DISP_VER_RES - 1) return;
        
        /*Truncate the area to the screen*/
        int32_t act_x1 = x1 < 0 ? 0 : x1;
        int32_t act_y1 = y1 < 0 ? 0 : y1;
        int32_t act_x2 = x2 > MY_DISP_HOR_RES - 1 ? MY_DISP_HOR_RES - 1 : x2;
        int32_t act_y2 = y2 > MY_DISP_VER_RES - 1 ? MY_DISP_VER_RES - 1 : y2;
        
        // 这里是模拟的显示刷新，实际项目中需要替换为真实的硬件操作
        // ESP_LOGV(TAG, "Flushing area: x1=%d, y1=%d, x2=%d, y2=%d", (int)act_x1, (int)act_y1, (int)act_x2, (int)act_y2);
    }

    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(disp_drv);
} 