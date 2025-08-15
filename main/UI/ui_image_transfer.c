/**
 * @file ui_image_transfer.c
 * @brief 图片传输界面
 * @author TidyCraze
 * @date 2025-08-14
 */
#include "ui_image_transfer.h"
#include "esp_log.h"
#include "theme_manager.h"
#include "ui.h"
#include "wifi_image_transfer.h"


static const char* TAG = "UI_IMG_TRANSFER";

static lv_obj_t* s_img_obj = NULL;
static lv_img_dsc_t s_img_dsc;

// 自定义返回按钮回调 - 处理图片传输界面的特殊逻辑
static void image_transfer_back_btn_callback(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Back button clicked, stopping image transfer.");
        wifi_image_transfer_stop();        // Stop the TCP server task
        lv_obj_clean(lv_scr_act());        // Clear current screen
        ui_main_menu_create(lv_scr_act()); // Go back to main menu
    }
}

void ui_image_transfer_create(lv_obj_t* parent) {
    // 应用当前主题到屏幕
    theme_apply_to_screen(parent);

    // 1. 创建页面父级容器（统一管理整个页面）
    lv_obj_t* page_parent_container;
    ui_create_page_parent_container(parent, &page_parent_container);

    // 2. 创建顶部栏容器（包含返回按钮和标题）
    lv_obj_t* top_bar_container;
    lv_obj_t* title_container;
    ui_create_top_bar(page_parent_container, "Image Transfer", &top_bar_container, &title_container);

    // 替换顶部栏的返回按钮回调为自定义回调
    lv_obj_t* back_btn = lv_obj_get_child(top_bar_container, 0); // 获取返回按钮
    if (back_btn) {
        lv_obj_remove_event_cb(back_btn, NULL); // 移除默认回调
        lv_obj_add_event_cb(back_btn, image_transfer_back_btn_callback, LV_EVENT_CLICKED, NULL);
    }

    // 3. 创建页面内容容器
    lv_obj_t* content_container;
    ui_create_page_content_area(page_parent_container, &content_container);

    // 4. 在content_container中添加页面内容
    // 创建图片显示对象
    s_img_obj = lv_img_create(content_container);
    lv_obj_align(s_img_obj, LV_ALIGN_CENTER, 0, 0);
    
    // Initialize image descriptor with default values
    s_img_dsc.header.w = 0;
    s_img_dsc.header.h = 0;
    s_img_dsc.data_size = 0;
    s_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    s_img_dsc.data = NULL;
    
    lv_img_set_src(s_img_obj, &s_img_dsc);
    lv_obj_set_size(s_img_obj, 320, 240); // Set to expected image size

    // 应用主题样式到图片对象
    lv_obj_set_style_outline_width(s_img_obj, 2, 0);
    lv_obj_set_style_outline_color(s_img_obj, theme_get_color(theme_get_current_theme()->colors.border), 0);
    lv_obj_set_style_radius(s_img_obj, 8, 0);

    // 创建状态标签
    lv_obj_t* status_label = lv_label_create(content_container);
    lv_label_set_text(status_label, "Waiting for image data...");
    theme_apply_to_label(status_label, false);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);

    // Start the TCP server task when the screen is created
    ESP_LOGI(TAG, "Starting image transfer server...");
    wifi_image_transfer_start(6556); // Use the specified port
}

void ui_image_transfer_set_image_data(uint8_t* img_buf, int width, int height, jpeg_pixel_format_t format) {
    if (!s_img_obj) {
        ESP_LOGW(TAG, "Image object is NULL, cannot update image.");
        return;
    }

    // Check if the format matches what we expect
    if (format != JPEG_PIXEL_FORMAT_RGB565_BE) {
        ESP_LOGW(TAG, "Unexpected format: %d, expected: %d", format, JPEG_PIXEL_FORMAT_RGB565_BE);
        return;
    }

    // Set up the image descriptor for RGB565 format
    s_img_dsc.header.w = width;
    s_img_dsc.header.h = height;
    s_img_dsc.data_size = width * height * 2; // 2 bytes per pixel for RGB565
    s_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR; // Use TRUE_COLOR for RGB565
    s_img_dsc.data = img_buf;

    // Update the image object
    lv_img_set_src(s_img_obj, &s_img_dsc);
    lv_obj_set_size(s_img_obj, width, height); // Adjust size to image
    lv_obj_invalidate(s_img_obj);              // Redraw the image object
    
    // Debug: Check if image object is valid
    if (lv_obj_is_valid(s_img_obj)) {
        ESP_LOGI(TAG, "Image object is valid, size: %dx%d", width, height);
    } else {
        ESP_LOGE(TAG, "Image object is invalid!");
    }
    
    // Debug: Check image source
    const void* src = lv_img_get_src(s_img_obj);
    if (src == &s_img_dsc) {
        ESP_LOGI(TAG, "Image source set correctly");
    } else {
        ESP_LOGE(TAG, "Image source not set correctly");
    }
    
    ESP_LOGI(TAG, "Image data updated: %dx%d, format %d", width, height, format);
}