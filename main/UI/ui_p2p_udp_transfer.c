#include "ui_p2p_udp_transfer.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "theme_manager.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>


static const char* TAG = "UI_P2P_UDP";

// UI对象
static lv_obj_t* s_page_parent = NULL;
static lv_obj_t* s_img_obj = NULL;
static lv_obj_t* s_status_label = NULL;
static lv_obj_t* s_ip_label = NULL;
static lv_obj_t* s_ssid_label = NULL;
static lv_obj_t* s_fps_label = NULL;

// 图像描述符
static lv_img_dsc_t s_img_dsc = {.header.always_zero = 0,
                                 .header.w = 0,
                                 .header.h = 0,
                                 .header.cf = LV_IMG_CF_TRUE_COLOR,
                                 .data_size = 0,
                                 .data = NULL};

// 状态变量
static bool s_is_initialized = false;
static bool s_is_running = false;
static lv_timer_t* s_fps_timer = NULL;

// 前向声明
static void on_back_clicked(lv_event_t* e);
static void p2p_image_callback(uint8_t* img_buf, int width, int height, jpeg_pixel_format_t format);
static void p2p_status_callback(p2p_connection_state_t state, const char* info);
static const char* get_state_string(p2p_connection_state_t state);
static void fps_timer_callback(lv_timer_t* timer);

void ui_p2p_udp_transfer_create(lv_obj_t* parent) {
    if (s_page_parent != NULL) {
        ESP_LOGW(TAG, "UI already created");
        return;
    }

    ESP_LOGI(TAG, "Creating simplified P2P UDP transfer UI");

    // 创建页面容器
    ui_create_page_parent_container(parent, &s_page_parent);

    // 创建顶部栏
    lv_obj_t* top_bar_container;
    lv_obj_t* title_container;
    ui_create_top_bar(s_page_parent, "P2P UDP Transfer", &top_bar_container, &title_container);

    // 创建返回按钮
    lv_obj_t* back_btn = lv_btn_create(top_bar_container);
    lv_obj_set_size(back_btn, 60, 30);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_add_event_cb(back_btn, on_back_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);
    theme_apply_to_button(back_btn, false);

    // 创建内容区域
    lv_obj_t* content_container;
    ui_create_page_content_area(s_page_parent, &content_container);
    lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);

    // 图像显示区域
    lv_obj_t* image_panel = lv_obj_create(content_container);
    lv_obj_set_width(image_panel, LV_PCT(100));
    lv_obj_set_flex_grow(image_panel, 1);
    lv_obj_set_style_pad_all(image_panel, 10, 0);
    theme_apply_to_container(image_panel);

    lv_obj_t* image_title = lv_label_create(image_panel);
    lv_label_set_text(image_title, "Received Image:");
    lv_obj_align(image_title, LV_ALIGN_TOP_LEFT, 0, 0);
    theme_apply_to_label(image_title, false);

    s_img_obj = lv_img_create(image_panel);
    lv_obj_align(s_img_obj, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(s_img_obj, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_img_obj, LV_OPA_50, 0);

    // 状态显示区域
    lv_obj_t* status_panel = lv_obj_create(content_container);
    lv_obj_set_width(status_panel, LV_PCT(100));
    lv_obj_set_height(status_panel, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(status_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(status_panel, 10, 0);
    theme_apply_to_container(status_panel);

    s_ssid_label = lv_label_create(status_panel);
    char ssid_text[64];
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    snprintf(ssid_text, sizeof(ssid_text), "SSID: %s%02X%02X", P2P_WIFI_SSID_PREFIX, mac[4], mac[5]);
    lv_label_set_text(s_ssid_label, ssid_text);
    theme_apply_to_label(s_ssid_label, false);

    s_ip_label = lv_label_create(status_panel);
    lv_label_set_text(s_ip_label, "IP: Not Assigned");
    theme_apply_to_label(s_ip_label, false);

    s_status_label = lv_label_create(status_panel);
    lv_label_set_text(s_status_label, "Status: Initializing...");
    theme_apply_to_label(s_status_label, false);

    s_fps_label = lv_label_create(status_panel);
    lv_label_set_text(s_fps_label, "FPS: 0.0");
    theme_apply_to_label(s_fps_label, false);

    // 初始化并自动启动P2P UDP系统
    esp_err_t ret = p2p_udp_image_transfer_init(P2P_MODE_AP, p2p_image_callback, p2p_status_callback);
    if (ret == ESP_OK) {
        s_is_initialized = true;
        ESP_LOGI(TAG, "P2P UDP system initialized");
        ret = p2p_udp_image_transfer_start();
        if (ret == ESP_OK) {
            s_is_running = true;
            ESP_LOGI(TAG, "Service started automatically");
            lv_label_set_text(s_status_label, "Status: AP starting...");
        } else {
            ESP_LOGE(TAG, "Failed to auto-start service: %s", esp_err_to_name(ret));
            lv_label_set_text(s_status_label, "Status: Start failed");
        }
    } else {
        ESP_LOGE(TAG, "Failed to initialize P2P UDP system: %s", esp_err_to_name(ret));
        lv_label_set_text(s_status_label, "Status: Init failed");
    }

    // 创建一个定时器来更新FPS
    s_fps_timer = lv_timer_create(fps_timer_callback, 500, NULL);
    lv_timer_ready(s_fps_timer);

    ESP_LOGI(TAG, "P2P UDP transfer UI created");
}

void ui_p2p_udp_transfer_destroy(void) {
    if (s_page_parent == NULL) {
        return;
    }

    ESP_LOGI(TAG, "Destroying P2P UDP transfer UI");

    // Deinitialize the P2P UDP system
    if (s_is_initialized) {
        p2p_udp_image_transfer_deinit();
        s_is_initialized = false;
        s_is_running = false; // deinit calls stop, so this is also false
    }

    // 停止并删除定时器
    if (s_fps_timer) {
        lv_timer_del(s_fps_timer);
        s_fps_timer = NULL;
    }

    // 清理UI对象
    if (s_page_parent) {
        lv_obj_del(s_page_parent);
        s_page_parent = NULL;
    }

    // 重置所有UI对象指针
    s_img_obj = NULL;
    s_status_label = NULL;
    s_ip_label = NULL;
    s_ssid_label = NULL;
    s_fps_label = NULL;

    s_is_initialized = false;
    ESP_LOGI(TAG, "P2P UDP transfer UI destroyed");
}

void ui_p2p_udp_transfer_set_image_data(uint8_t* img_buf, int width, int height, jpeg_pixel_format_t format) {
    if (!s_img_obj) {
        ESP_LOGW(TAG, "Image object is NULL, cannot update image");
        return;
    }

    // 检查格式
    if (format != JPEG_PIXEL_FORMAT_RGB565_BE) {
        ESP_LOGW(TAG, "Unexpected format: %d, expected: %d", format, JPEG_PIXEL_FORMAT_RGB565_BE);
        return;
    }

    // 设置图像描述符
    s_img_dsc.header.w = width;
    s_img_dsc.header.h = height;
    s_img_dsc.data_size = width * height * 2; // RGB565
    s_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    s_img_dsc.data = img_buf;

    // 更新图像对象
    lv_img_set_src(s_img_obj, &s_img_dsc);
    lv_obj_set_size(s_img_obj, width, height);
    lv_obj_invalidate(s_img_obj);

    ESP_LOGD(TAG, "Image updated: %dx%d", width, height);
}

void ui_p2p_udp_transfer_update_status(p2p_connection_state_t state, const char* info) {
    if (s_status_label) {
        char status_text[128];
        snprintf(status_text, sizeof(status_text), "Status: %s", get_state_string(state));
        if (info && strlen(info) > 0) {
            strncat(status_text, " - ", sizeof(status_text) - strlen(status_text) - 1);
            strncat(status_text, info, sizeof(status_text) - strlen(status_text) - 1);
        }
        lv_label_set_text(s_status_label, status_text);
    }

    // 更新IP地址显示
    if (s_ip_label && (state == P2P_STATE_AP_RUNNING || state == P2P_STATE_STA_CONNECTED)) {
        char ip_str[64];
        if (p2p_udp_get_local_ip(ip_str, sizeof(ip_str)) == ESP_OK) {
            char ip_text[80];
            snprintf(ip_text, sizeof(ip_text), "IP: %s", ip_str);
            lv_label_set_text(s_ip_label, ip_text);
        }
    } else {
        lv_label_set_text(s_ip_label, "IP: Not Assigned");
    }
}

void ui_p2p_udp_transfer_update_stats(uint32_t tx_packets, uint32_t rx_packets, uint32_t lost_packets,
                                      uint32_t retx_packets) {
    // This function is now empty as detailed stats are removed.
    // It's kept for API compatibility if other parts of the code call it.
}

// 事件处理函数
static void on_back_clicked(lv_event_t* e) {
    ESP_LOGI(TAG, "Back button clicked");
    ui_p2p_udp_transfer_destroy();
    ui_main_menu_create(lv_scr_act());
}

// 回调函数
static void p2p_image_callback(uint8_t* img_buf, int width, int height, jpeg_pixel_format_t format) {
    ui_p2p_udp_transfer_set_image_data(img_buf, width, height, format);
}

static void p2p_status_callback(p2p_connection_state_t state, const char* info) {
    ui_p2p_udp_transfer_update_status(state, info);
}

static void fps_timer_callback(lv_timer_t* timer)
{
    if (s_fps_label) {
        float fps = p2p_udp_get_fps();
        lv_label_set_text_fmt(s_fps_label, "FPS: %.1f", fps);
    }
}

static const char* get_state_string(p2p_connection_state_t state) {
    switch (state) {
    case P2P_STATE_IDLE:
        return "Idle";
    case P2P_STATE_AP_STARTING:
        return "Starting AP...";
    case P2P_STATE_AP_RUNNING:
        return "AP Running";
    case P2P_STATE_STA_CONNECTING:
        return "Connecting...";
    case P2P_STATE_STA_CONNECTED:
        return "Connected";
    case P2P_STATE_ERROR:
        return "Error";
    default:
        return "Unknown State";
    }
}
