#include "../app/game/game.h"
#include "battery_monitor.h"
#include "esp_log.h"
#include "ui.h"
#include "wifi_manager.h"
#include "wifi_image_transfer.h"
#include "ui_image_transfer.h"
#include "ui_serial_display.h"
#include "ui_calibration.h"

// 全局变量保存时间标签和电池标签
static lv_obj_t* g_time_label = NULL;
static lv_obj_t* g_battery_label = NULL;

// 时间更新定时器回调函数
static void time_update_timer_cb(lv_timer_t* timer) {
    if (!g_time_label) {
        ESP_LOGW("UI_MAIN", "Time label is NULL!");
        return;
    }

    char time_str[32];
    if (wifi_manager_get_time_str(time_str, sizeof(time_str))) {
        lv_label_set_text(g_time_label, time_str);
        lv_obj_invalidate(g_time_label);
    } else {
        ESP_LOGW("UI_MAIN", "Failed to get time string");
    }
}

// 电池电量更新函数（由任务调用）
void ui_main_update_battery_display(void) {
    if (!g_battery_label) {
        ESP_LOGW("UI_MAIN", "Battery label is NULL!");
        return;
    }

    battery_info_t battery_info;
    if (battery_monitor_read(&battery_info) == ESP_OK) {
        char battery_str[32];
        snprintf(battery_str, sizeof(battery_str), "%d%%", battery_info.percentage);

        // 根据电量设置颜色 - 在深色背景下使用更亮的颜色
        lv_color_t text_color;
        if (battery_info.is_critical) {
            text_color = lv_color_hex(0xFF6B6B); // 亮红色 - 严重低电量
        } else if (battery_info.is_low_battery) {
            text_color = lv_color_hex(0xFFB347); // 亮橙色 - 低电量
        } else {
            text_color = lv_color_hex(0x7FFF00); // 亮绿色 - 正常电量
        }

        // 直接更新UI
        lv_obj_set_style_text_color(g_battery_label, text_color, 0);
        lv_label_set_text(g_battery_label, battery_str);

        ESP_LOGI("UI_MAIN", "Updated battery display: %s", battery_str);
    } else {
        ESP_LOGW("UI_MAIN", "Failed to read battery info");
    }
}

// 菜单项回调函数类型
typedef void (*menu_item_cb_t)(void);

// 示例回调函数（后续扩展时实现）
static void option1_cb(void) {
    // TODO: Option 1 Logic
    lv_obj_t* label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Option 1 Selected");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

static void wifi_settings_cb(void) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);            // 清空当前屏幕
        ui_wifi_settings_create(screen); // 加载WiFi设置界面
    }
}

static void settings_cb(void) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);       // 清空当前屏幕
        ui_settings_create(screen); // 加载系统设置界面
    }
}

static void game_cb(void) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);        // 清空当前屏幕
        ui_game_menu_create(screen); // 加载游戏子菜单界面
    }
}

static void image_transfer_cb(void) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);            // 清空当前屏幕
        ui_image_transfer_create(screen); // 加载图传界面
    }
}

static void serial_display_cb(void) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);            // 清空当前屏幕
        ui_serial_display_create(screen); // 加载串口显示界面
    }
}

static void calibration_cb(void) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);            // 清空当前屏幕
        ui_calibration_create(screen);   // 加载校准界面
    }
}

// 可扩展的菜单项结构
typedef struct {
    const char* text;
    menu_item_cb_t callback;
} menu_item_t;

// 菜单项数组
static menu_item_t menu_items[] = {
    {"Demo", option1_cb}, 
    {"WiFi Setup", wifi_settings_cb}, 
    {"Settings", settings_cb}, 
    {"Game", game_cb}, 
    {"Image Transfer", image_transfer_cb},
    {"Serial Display", serial_display_cb},
    {"Calibration", calibration_cb},
    // 添加更多项...
};

static void btn_event_cb(lv_event_t* e) {
    menu_item_cb_t cb = (menu_item_cb_t)lv_event_get_user_data(e);
    if (cb)
        cb();
}

void ui_main_menu_create(lv_obj_t* parent) {
    // 创建状态栏容器 - 固定在顶部
    lv_obj_t* status_bar = lv_obj_create(parent);
    lv_obj_set_size(status_bar, 320, 40);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x2C3E50), 0);  // 深蓝色背景
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_90, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 5, 0);

    // 创建标题 - 在状态栏内居中
    lv_obj_t* title = lv_label_create(status_bar);
    lv_label_set_text(title, "MAIN MENU");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    // 创建时间显示容器 - 在状态栏右侧
    lv_obj_t* time_cont = lv_obj_create(status_bar);
    lv_obj_set_size(time_cont, 120, 30);
    lv_obj_align(time_cont, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(time_cont, lv_color_hex(0x34495E), 0);
    lv_obj_set_style_bg_opa(time_cont, LV_OPA_80, 0);
    lv_obj_set_style_radius(time_cont, 4, 0);
    lv_obj_set_style_pad_all(time_cont, 3, 0);

    // 创建时间显示标签
    g_time_label = lv_label_create(time_cont);
    lv_obj_set_style_text_font(g_time_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_time_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(g_time_label);
    lv_label_set_text(g_time_label, "Syncing...");

    // 创建电池电量显示容器 - 在状态栏左侧
    lv_obj_t* battery_cont = lv_obj_create(status_bar);
    lv_obj_set_size(battery_cont, 70, 30);
    lv_obj_align(battery_cont, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_bg_color(battery_cont, lv_color_hex(0x34495E), 0);
    lv_obj_set_style_bg_opa(battery_cont, LV_OPA_80, 0);
    lv_obj_set_style_radius(battery_cont, 4, 0);
    lv_obj_set_style_pad_all(battery_cont, 3, 0);

    // 创建电池电量显示标签
    g_battery_label = lv_label_create(battery_cont);
    lv_obj_set_style_text_font(g_battery_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_battery_label, lv_color_hex(0x00FF00), 0);
    lv_obj_center(g_battery_label);
    lv_label_set_text(g_battery_label, "100%");

    // 计算按钮数量
    int num_items = sizeof(menu_items) / sizeof(menu_item_t);

    // 创建菜单按钮容器 - 在状态栏下方
    lv_obj_t* menu_container = lv_obj_create(parent);
    lv_obj_set_size(menu_container, 300, 240);
    lv_obj_align(menu_container, LV_ALIGN_CENTER, 0, 20);  // 向下偏移20像素，避开状态栏
    lv_obj_set_style_bg_opa(menu_container, LV_OPA_0, 0);  // 透明背景
    lv_obj_set_style_border_width(menu_container, 0, 0);
    lv_obj_set_style_pad_all(menu_container, 0, 0);

    // 创建按钮 - 在菜单容器内
    for (int i = 0; i < num_items; i++) {
        lv_obj_t* btn = lv_btn_create(menu_container);
        lv_obj_set_size(btn, 200, 40);
        lv_obj_align(btn, LV_ALIGN_CENTER, 0, -80 + i * 45);  // 调整间距
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, menu_items[i].callback);
        
        // 设置按钮样式
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x3498DB), 0);  // 蓝色按钮
        lv_obj_set_style_bg_opa(btn, LV_OPA_80, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_shadow_width(btn, 5, 0);
        lv_obj_set_style_shadow_ofs_y(btn, 2, 0);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, menu_items[i].text);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);  // 白色文字
        lv_obj_center(label);
    }

    // 创建时间更新定时器（每秒更新一次）
    static lv_timer_t* timer = NULL;
    if (timer == NULL) {
        timer = lv_timer_create(time_update_timer_cb, 1000, NULL);
        ESP_LOGI("UI_MAIN", "Time update timer created");
    }

    // 电池电量显示将由任务更新，每5分钟更新一次
    ESP_LOGI("UI_MAIN", "Battery display ready for task updates");

    // 扩展提示：要添加新选项，在 menu_items 数组中添加新项，并实现对应的回调函数
}