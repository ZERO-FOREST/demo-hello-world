/**
 * @file lv_port_disp.c
 * ESP32-S3 Display Port for LVGL
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include "esp_log.h"
#include <stdbool.h>

// ========================================
// 驱动选择宏定义开关
// ========================================
#define USE_ESP_LCD_DRIVER 0 // 0=使用原始驱动, 1=使用ESP-LCD驱动

#if USE_ESP_LCD_DRIVER
#include "st7789_esp_lcd.h" // ESP-LCD驱动
#else
#include "st7789.h" // 原始驱动
#endif

/*********************
 *      DEFINES
 *********************/
#define MY_DISP_HOR_RES ST7789_WIDTH
#define MY_DISP_VER_RES ST7789_HEIGHT

static const char* TAG = "lv_port_disp";

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);
static void disp_flush(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p);

/**********************
 *  STATIC VARIABLES
 **********************/
static bool disp_flush_enabled = true;

#include "esp_heap_caps.h"
static lv_color_t* disp_buf_1 = NULL;
static lv_color_t* disp_buf_2 = NULL;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void) {
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*-----------------------------
     * Create a buffer for drawing
     *----------------------------*/
    static lv_disp_draw_buf_t draw_buf_dsc;
    size_t lines = MY_DISP_VER_RES;
    size_t buf_pixels = MY_DISP_HOR_RES * lines;
    disp_buf_1 = (lv_color_t*)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    disp_buf_2 = (lv_color_t*)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lv_disp_draw_buf_init(&draw_buf_dsc, disp_buf_1, disp_buf_2, buf_pixels);

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/
    static lv_disp_drv_t disp_drv; /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);   /*Basic initialization*/

    /*Set up the functions to access to your display*/
    disp_drv.hor_res = MY_DISP_HOR_RES;
    disp_drv.ver_res = MY_DISP_VER_RES;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf_dsc;

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Display port initialized successfully (buf lines=%d)", (int)lines);
}

void disp_enable_update(void) { disp_flush_enabled = true; }

void disp_disable_update(void) { disp_flush_enabled = false; }

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void) {
    /*You code here*/
#if USE_ESP_LCD_DRIVER
    ESP_LOGI(TAG, "Display hardware initialization with ESP-LCD driver");
    esp_err_t ret = st7789_esp_lcd_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ST7789 with ESP-LCD: %s", esp_err_to_name(ret));
    }
#else
    ESP_LOGI(TAG, "Display hardware initialization with original driver");
    st7789_init();
#endif
}

volatile bool disp_flush_ready = false;

/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
static void disp_flush(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    if (disp_flush_enabled) {
#if USE_ESP_LCD_DRIVER
        // ESP-LCD驱动实现
        esp_lcd_panel_handle_t panel_handle = st7789_esp_lcd_get_panel_handle();
        if (panel_handle) {
            uint16_t width = area->x2 - area->x1 + 1;
            uint16_t height = area->y2 - area->y1 + 1;

            // 检查区域是否有效
            if (width > 0 && height > 0 && area->x1 >= 0 && area->y1 >= 0 && area->x2 < MY_DISP_HOR_RES &&
                area->y2 < MY_DISP_VER_RES) {

                // 分块绘制，避免内存不足
                const uint16_t block_height = 20; // 每次绘制20行
                for (uint16_t y = area->y1; y <= area->y2; y += block_height) {
                    uint16_t current_height = (y + block_height - 1 <= area->y2) ? block_height : (area->y2 - y + 1);
                    const lv_color_t* block_data = color_p + (y - area->y1) * width;

                    esp_err_t ret =
                        esp_lcd_panel_draw_bitmap(panel_handle, area->x1, y, width, current_height, block_data);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to draw bitmap block: %s", esp_err_to_name(ret));
                        break;
                    }
                }
            }
        } else {
            ESP_LOGE(TAG, "Panel handle is NULL");
        }
#else
        // 原始驱动实现
        st7789_set_window(area->x1, area->y1, area->x2, area->y2);
        size_t pixel_count = lv_area_get_size(area);
        st7789_write_pixels((uint16_t*)color_p, pixel_count);
#endif
    }

    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(disp_drv);
}