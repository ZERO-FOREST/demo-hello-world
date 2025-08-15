/**
 * @file ui_image_transfer.c
 * @brief 图片传输界面
 * @author TidyCraze
 * @date 2025-08-14
 */
#include "ui_image_transfer.h"
#include "esp_log.h"
#include "ui.h"
#include "wifi_image_transfer.h"

static const char* TAG = "UI_IMG_TRANSFER";

static lv_obj_t* s_img_obj = NULL;
static lv_img_dsc_t s_img_dsc;

static void back_button_event_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Back button clicked, stopping image transfer.");
        wifi_image_transfer_stop();        // Stop the TCP server task
        lv_obj_clean(lv_scr_act());        // Clear current screen
        ui_main_menu_create(lv_scr_act()); // Go back to main menu
    }
}

void ui_image_transfer_create(lv_obj_t* parent) {
    lv_obj_t* screen = lv_obj_create(parent);
    lv_obj_set_size(screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF6E9DB), LV_PART_MAIN); // 使用莫兰迪色系背景
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // 创建统一标题
    ui_create_page_title(screen, "Image Transfer");

    // Create an image object to display the received image
    s_img_obj = lv_img_create(screen);
    lv_obj_align(s_img_obj, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_size(s_img_obj, 240, 240); // Placeholder size, will be updated by image data
    lv_img_set_src(s_img_obj, &s_img_dsc);
    lv_obj_set_style_outline_width(s_img_obj, 2, 0);
    lv_obj_set_style_outline_color(s_img_obj, lv_color_hex(0xFFFFFF), 0);

    // Create a back button - 使用统一的back按钮函数
    ui_create_back_button(screen, "Back");

    // Start the TCP server task when the screen is created
    ESP_LOGI(TAG, "Starting image transfer server...");
    wifi_image_transfer_start(6556); // Use the specified port
}

void ui_image_transfer_set_image_data(uint8_t* img_buf, int width, int height, jpeg_pixel_format_t format) {
    if (!s_img_obj) {
        ESP_LOGW(TAG, "Image object is NULL, cannot update image.");
        return;
    }

    // Assuming RGB888 for now, convert if necessary for your display
    // LVGL expects LV_IMG_CF_TRUE_COLOR for RGB888
    s_img_dsc.header.w = width;
    s_img_dsc.header.h = height;
    s_img_dsc.data_size = width * height * 2; // For RGB565
    s_img_dsc.header.cf = LV_IMG_CF_RGB565;   // Set to RGB565
    s_img_dsc.data = img_buf;

    lv_img_set_src(s_img_obj, &s_img_dsc);
    lv_obj_set_size(s_img_obj, width, height); // Adjust size to image
    lv_obj_invalidate(s_img_obj);              // Redraw the image object
    ESP_LOGI(TAG, "Image data updated: %dx%d, format %d", width, height, format);
}