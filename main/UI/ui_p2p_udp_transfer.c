#include "ui_p2p_udp_transfer.h"
#include "esp_log.h"
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
static lv_obj_t* s_mode_label = NULL;
static lv_obj_t* s_ip_label = NULL;
static lv_obj_t* s_stats_label = NULL;
static lv_obj_t* s_mode_switch = NULL;
static lv_obj_t* s_start_btn = NULL;
static lv_obj_t* s_connect_btn = NULL;
static lv_obj_t* s_ssid_ta = NULL;
static lv_obj_t* s_password_ta = NULL;

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
static p2p_connection_mode_t s_current_mode = P2P_MODE_AP;
static TimerHandle_t s_stats_timer = NULL;

// 前向声明
static void on_back_clicked(lv_event_t* e);
static void on_mode_switch_changed(lv_event_t* e);
static void on_start_btn_clicked(lv_event_t* e);
static void on_connect_btn_clicked(lv_event_t* e);
static void p2p_image_callback(uint8_t* img_buf, int width, int height, jpeg_pixel_format_t format);
static void p2p_status_callback(p2p_connection_state_t state, const char* info);
static void stats_timer_callback(TimerHandle_t xTimer);
static void update_ui_state(void);
static const char* get_state_string(p2p_connection_state_t state);

void ui_p2p_udp_transfer_create(lv_obj_t* parent) {
    if (s_page_parent != NULL) {
        ESP_LOGW(TAG, "UI already created");
        return;
    }

    ESP_LOGI(TAG, "Creating P2P UDP transfer UI");

    // 创建页面容器
    ui_create_page_parent_container(parent, &s_page_parent);

    // 创建顶部栏
    lv_obj_t* top_bar_container;
    lv_obj_t* title_container;
    ui_create_top_bar(s_page_parent, "P2P UDP 图传", &top_bar_container, &title_container);

    // 创建返回按钮
    lv_obj_t* back_btn = lv_btn_create(top_bar_container);
    lv_obj_set_size(back_btn, 60, 30);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_add_event_cb(back_btn, on_back_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回");
    lv_obj_center(back_label);
    theme_apply_to_button(back_btn, false);

    // 创建内容区域
    lv_obj_t* content_container;
    ui_create_page_content_area(s_page_parent, &content_container);

    // 创建左右分栏布局
    lv_obj_t* main_cont = lv_obj_create(content_container);
    lv_obj_set_size(main_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(main_cont, 5, 0);
    lv_obj_set_style_bg_opa(main_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(main_cont, LV_OPA_TRANSP, 0);

    // 左侧控制面板
    lv_obj_t* control_panel = lv_obj_create(main_cont);
    lv_obj_set_size(control_panel, LV_PCT(35), LV_PCT(100));
    lv_obj_set_flex_flow(control_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(control_panel, 10, 0);
    theme_apply_to_container(control_panel);

    // 模式选择
    lv_obj_t* mode_cont = lv_obj_create(control_panel);
    lv_obj_set_size(mode_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(mode_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(mode_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(mode_cont, 5, 0);

    lv_obj_t* mode_title = lv_label_create(mode_cont);
    lv_label_set_text(mode_title, "工作模式：");
    lv_obj_align(mode_title, LV_ALIGN_TOP_LEFT, 0, 0);
    theme_apply_to_label(mode_title, false);

    s_mode_switch = lv_switch_create(mode_cont);
    lv_obj_align(s_mode_switch, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_event_cb(s_mode_switch, on_mode_switch_changed, LV_EVENT_VALUE_CHANGED, NULL);

    s_mode_label = lv_label_create(mode_cont);
    lv_label_set_text(s_mode_label, "热点模式");
    lv_obj_align(s_mode_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    theme_apply_to_label(s_mode_label, false);

    // 连接控制
    lv_obj_t* connect_cont = lv_obj_create(control_panel);
    lv_obj_set_size(connect_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(connect_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(connect_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(connect_cont, 5, 0);

    // SSID输入
    lv_obj_t* ssid_label = lv_label_create(connect_cont);
    lv_label_set_text(ssid_label, "SSID:");
    theme_apply_to_label(ssid_label, false);

    s_ssid_ta = lv_textarea_create(connect_cont);
    lv_obj_set_size(s_ssid_ta, LV_PCT(100), 30);
    lv_textarea_set_placeholder_text(s_ssid_ta, "输入热点名称");
    lv_textarea_set_one_line(s_ssid_ta, true);

    // 密码输入
    lv_obj_t* pwd_label = lv_label_create(connect_cont);
    lv_label_set_text(pwd_label, "密码:");
    theme_apply_to_label(pwd_label, false);

    s_password_ta = lv_textarea_create(connect_cont);
    lv_obj_set_size(s_password_ta, LV_PCT(100), 30);
    lv_textarea_set_placeholder_text(s_password_ta, "输入密码");
    lv_textarea_set_one_line(s_password_ta, true);
    lv_textarea_set_password_mode(s_password_ta, true);

    // 控制按钮
    s_start_btn = lv_btn_create(control_panel);
    lv_obj_set_size(s_start_btn, LV_PCT(100), 40);
    lv_obj_add_event_cb(s_start_btn, on_start_btn_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t* start_label = lv_label_create(s_start_btn);
    lv_label_set_text(start_label, "启动服务");
    lv_obj_center(start_label);
    theme_apply_to_button(s_start_btn, false);

    s_connect_btn = lv_btn_create(control_panel);
    lv_obj_set_size(s_connect_btn, LV_PCT(100), 40);
    lv_obj_add_event_cb(s_connect_btn, on_connect_btn_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t* connect_label = lv_label_create(s_connect_btn);
    lv_label_set_text(connect_label, "连接热点");
    lv_obj_center(connect_label);
    theme_apply_to_button(s_connect_btn, false);

    // 状态显示
    lv_obj_t* status_cont = lv_obj_create(control_panel);
    lv_obj_set_size(status_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(status_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(status_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(status_cont, 5, 0);

    lv_obj_t* status_title = lv_label_create(status_cont);
    lv_label_set_text(status_title, "状态信息：");
    theme_apply_to_label(status_title, false);

    s_status_label = lv_label_create(status_cont);
    lv_label_set_text(s_status_label, "未启动");
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_status_label, LV_PCT(100));
    theme_apply_to_label(s_status_label, false);

    s_ip_label = lv_label_create(status_cont);
    lv_label_set_text(s_ip_label, "IP: 未分配");
    lv_label_set_long_mode(s_ip_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_ip_label, LV_PCT(100));
    theme_apply_to_label(s_ip_label, false);

    // 统计信息
    lv_obj_t* stats_title = lv_label_create(status_cont);
    lv_label_set_text(stats_title, "传输统计：");
    theme_apply_to_label(stats_title, false);

    s_stats_label = lv_label_create(status_cont);
    lv_label_set_text(s_stats_label, "TX: 0, RX: 0\n丢包: 0, 重传: 0");
    lv_label_set_long_mode(s_stats_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_stats_label, LV_PCT(100));
    theme_apply_to_label(s_stats_label, false);

    // 右侧图像显示区域
    lv_obj_t* image_panel = lv_obj_create(main_cont);
    lv_obj_set_size(image_panel, LV_PCT(63), LV_PCT(100));
    lv_obj_set_style_pad_all(image_panel, 10, 0);
    theme_apply_to_container(image_panel);

    lv_obj_t* image_title = lv_label_create(image_panel);
    lv_label_set_text(image_title, "接收图像：");
    lv_obj_align(image_title, LV_ALIGN_TOP_LEFT, 0, 0);
    theme_apply_to_label(image_title, false);

    // 图像显示对象
    s_img_obj = lv_img_create(image_panel);
    lv_obj_align(s_img_obj, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(s_img_obj, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_img_obj, LV_OPA_50, 0);

    // 初始化P2P UDP系统
    esp_err_t ret = p2p_udp_image_transfer_init(s_current_mode, p2p_image_callback, p2p_status_callback);
    if (ret == ESP_OK) {
        s_is_initialized = true;
        ESP_LOGI(TAG, "P2P UDP system initialized");
    } else {
        ESP_LOGE(TAG, "Failed to initialize P2P UDP system: %s", esp_err_to_name(ret));
    }

    // 创建统计信息更新定时器
    s_stats_timer = xTimerCreate("stats_timer", pdMS_TO_TICKS(1000), pdTRUE, NULL, stats_timer_callback);
    if (s_stats_timer) {
        xTimerStart(s_stats_timer, 0);
    }

    // 更新UI状态
    update_ui_state();

    ESP_LOGI(TAG, "P2P UDP transfer UI created");
}

void ui_p2p_udp_transfer_destroy(void) {
    if (s_page_parent == NULL) {
        return;
    }

    ESP_LOGI(TAG, "Destroying P2P UDP transfer UI");

    // 停止定时器
    if (s_stats_timer) {
        xTimerStop(s_stats_timer, 0);
        xTimerDelete(s_stats_timer, 0);
        s_stats_timer = NULL;
    }

    // 停止P2P UDP服务
    if (s_is_running) {
        p2p_udp_image_transfer_stop();
        s_is_running = false;
    }

    // 清理UI对象
    if (s_page_parent) {
        lv_obj_del(s_page_parent);
        s_page_parent = NULL;
    }

    // 重置所有UI对象指针
    s_img_obj = NULL;
    s_status_label = NULL;
    s_mode_label = NULL;
    s_ip_label = NULL;
    s_stats_label = NULL;
    s_mode_switch = NULL;
    s_start_btn = NULL;
    s_connect_btn = NULL;
    s_ssid_ta = NULL;
    s_password_ta = NULL;

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

    ESP_LOGI(TAG, "Image updated: %dx%d", width, height);
}

void ui_p2p_udp_transfer_update_status(p2p_connection_state_t state, const char* info) {
    if (s_status_label) {
        char status_text[128];
        snprintf(status_text, sizeof(status_text), "%s\n%s", get_state_string(state), info ? info : "");
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
    }

    update_ui_state();
}

void ui_p2p_udp_transfer_update_stats(uint32_t tx_packets, uint32_t rx_packets, uint32_t lost_packets,
                                      uint32_t retx_packets) {
    if (s_stats_label) {
        char stats_text[128];
        snprintf(stats_text, sizeof(stats_text), "TX: %lu, RX: %lu\n丢包: %lu, 重传: %lu", tx_packets, rx_packets,
                 lost_packets, retx_packets);
        lv_label_set_text(s_stats_label, stats_text);
    }
}

// 事件处理函数
static void on_back_clicked(lv_event_t* e) {
    ESP_LOGI(TAG, "Back button clicked");
    ui_p2p_udp_transfer_destroy();
    ui_main_menu_create(lv_scr_act());
}

static void on_mode_switch_changed(lv_event_t* e) {
    if (lv_obj_has_state(s_mode_switch, LV_STATE_CHECKED)) {
        s_current_mode = P2P_MODE_STA;
        lv_label_set_text(s_mode_label, "客户端模式");
    } else {
        s_current_mode = P2P_MODE_AP;
        lv_label_set_text(s_mode_label, "热点模式");
    }

    ESP_LOGI(TAG, "Mode changed to: %s", s_current_mode == P2P_MODE_AP ? "AP" : "STA");

    update_ui_state();
}

static void on_start_btn_clicked(lv_event_t* e) {
    if (!s_is_initialized) {
        ESP_LOGW(TAG, "System not initialized");
        return;
    }

    if (s_is_running) {
        // 停止服务
        p2p_udp_image_transfer_stop();
        s_is_running = false;
        ESP_LOGI(TAG, "Service stopped");
    } else {
        // 启动服务
        esp_err_t ret = p2p_udp_image_transfer_start();
        if (ret == ESP_OK) {
            s_is_running = true;
            ESP_LOGI(TAG, "Service started");
        } else {
            ESP_LOGE(TAG, "Failed to start service: %s", esp_err_to_name(ret));
        }
    }

    update_ui_state();
}

static void on_connect_btn_clicked(lv_event_t* e) {
    if (s_current_mode != P2P_MODE_STA || !s_is_running) {
        ESP_LOGW(TAG, "Not in STA mode or service not running");
        return;
    }

    const char* ssid = lv_textarea_get_text(s_ssid_ta);
    const char* password = lv_textarea_get_text(s_password_ta);

    if (strlen(ssid) == 0) {
        ESP_LOGW(TAG, "SSID is empty");
        return;
    }

    // 更新状态显示
    if (s_status_label) {
        char status_text[128];
        snprintf(status_text, sizeof(status_text), "正在连接到 %s...", ssid);
        lv_label_set_text(s_status_label, status_text);
    }

    esp_err_t ret = p2p_udp_connect_to_ap(ssid, password);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Connecting to AP: %s", ssid);
    } else {
        ESP_LOGE(TAG, "Failed to connect to AP: %s", esp_err_to_name(ret));
        // 更新错误状态
        if (s_status_label) {
            char error_text[128];
            snprintf(error_text, sizeof(error_text), "连接失败: %s", esp_err_to_name(ret));
            lv_label_set_text(s_status_label, error_text);
        }
    }
}

// 回调函数
static void p2p_image_callback(uint8_t* img_buf, int width, int height, jpeg_pixel_format_t format) {
    ui_p2p_udp_transfer_set_image_data(img_buf, width, height, format);
}

static void p2p_status_callback(p2p_connection_state_t state, const char* info) {
    ui_p2p_udp_transfer_update_status(state, info);
}

static void stats_timer_callback(TimerHandle_t xTimer) {
    uint32_t tx_packets, rx_packets, lost_packets, retx_packets;
    p2p_udp_get_stats(&tx_packets, &rx_packets, &lost_packets, &retx_packets);
    ui_p2p_udp_transfer_update_stats(tx_packets, rx_packets, lost_packets, retx_packets);
}

// 工具函数
static void update_ui_state(void) {
    if (!s_start_btn || !s_connect_btn) {
        return;
    }

    // 更新启动按钮
    lv_obj_t* start_label = lv_obj_get_child(s_start_btn, 0);
    if (s_is_running) {
        lv_label_set_text(start_label, "停止服务");
    } else {
        lv_label_set_text(start_label, "启动服务");
    }

    // 更新连接按钮状态
    if (s_current_mode == P2P_MODE_STA && s_is_running) {
        lv_obj_clear_state(s_connect_btn, LV_STATE_DISABLED);
        lv_obj_clear_state(s_ssid_ta, LV_STATE_DISABLED);
        lv_obj_clear_state(s_password_ta, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(s_connect_btn, LV_STATE_DISABLED);
        lv_obj_add_state(s_ssid_ta, LV_STATE_DISABLED);
        lv_obj_add_state(s_password_ta, LV_STATE_DISABLED);
    }

    // 更新模式开关状态
    if (s_is_running) {
        lv_obj_add_state(s_mode_switch, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(s_mode_switch, LV_STATE_DISABLED);
    }
}

static const char* get_state_string(p2p_connection_state_t state) {
    switch (state) {
    case P2P_STATE_IDLE:
        return "空闲";
    case P2P_STATE_AP_STARTING:
        return "启动热点中...";
    case P2P_STATE_AP_RUNNING:
        return "热点运行中";
    case P2P_STATE_STA_CONNECTING:
        return "连接中...";
    case P2P_STATE_STA_CONNECTED:
        return "已连接";
    case P2P_STATE_ERROR:
        return "错误";
    default:
        return "未知状态";
    }
}
