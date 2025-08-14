/**
 * @file ui_main.c
 * @brief 主界面UI
 * @author TidyCraze
 * @date 2025-08-14
 */

#include "../app/game/game.h"
#include "battery_monitor.h"
#include "color.h"
#include "esp_log.h"
#include "theme_manager.h"
#include "ui.h"
#include "ui_calibration.h"
#include "ui_image_transfer.h"
#include "ui_serial_display.h"
#include "ui_test.h"
#include "wifi_image_transfer.h"
#include "wifi_manager.h"

// 获取莫兰迪颜色的辅助函数
static lv_color_t get_morandi_color(int index) {
    if (index >= 0 && index < MORANDI_COLORS_COUNT) {
        return lv_color_hex(morandi_colors[index].color_hex);
    }
    return lv_color_hex(morandi_colors[4].color_hex); // 默认返回背景色
}

// 全局变量保存时间标签和电池标签
static lv_obj_t* g_time_label = NULL;
static lv_obj_t* g_battery_label = NULL;

// 本地时间变量
static uint8_t g_local_hour = 0;
static uint8_t g_local_minute = 0;
static bool g_time_initialized = false;

// 时间更新定时器回调函数
static void time_update_timer_cb(lv_timer_t* timer) {
    if (!g_time_label) {
        ESP_LOGW("UI_MAIN", "Time label is NULL!");
        return;
    }

    // 检查标签是否仍然有效
    if (!lv_obj_is_valid(g_time_label)) {
        ESP_LOGW("UI_MAIN", "Time label is no longer valid!");
        g_time_label = NULL; // 重置为NULL
        return;
    }

    char time_str[32];

    // 首先尝试从网络获取时间
    if (wifi_manager_get_time_str(time_str, sizeof(time_str))) {
        // 如果网络时间可用，解析并更新本地时间
        int hour, minute;
        if (sscanf(time_str, "%d:%d", &hour, &minute) == 2) {
            g_local_hour = hour;
            g_local_minute = minute;
            g_time_initialized = true;
        }
    } else {
        // 如果网络时间不可用，使用本地时间
        if (!g_time_initialized) {
            // 首次初始化，从00:00开始
            g_local_hour = 0;
            g_local_minute = 0;
            g_time_initialized = true;
        } else {
            // 每分钟更新一次
            g_local_minute++;
            if (g_local_minute >= 60) {
                g_local_minute = 0;
                g_local_hour++;
                if (g_local_hour >= 24) {
                    g_local_hour = 0;
                }
            }
        }

        // 格式化本地时间
        snprintf(time_str, sizeof(time_str), "%02d:%02d", g_local_hour, g_local_minute);
    }

    // 更新UI显示
    lv_label_set_text(g_time_label, time_str);
    lv_obj_invalidate(g_time_label);
}

// 电池电量更新函数（由任务调用）
void ui_main_update_battery_display(void) {
    if (!g_battery_label) {
        ESP_LOGW("UI_MAIN", "Battery label is NULL!");
        return;
    }

    // 检查标签是否仍然有效
    if (!lv_obj_is_valid(g_battery_label)) {
        ESP_LOGW("UI_MAIN", "Battery label is no longer valid!");
        g_battery_label = NULL; // 重置为NULL
        return;
    }

    battery_info_t battery_info;
    if (battery_monitor_read(&battery_info) == ESP_OK) {
        char battery_str[32];
        snprintf(battery_str, sizeof(battery_str), "%d%%", battery_info.percentage);

        // 直接更新UI - 保持黑色文字
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
        // 先停止时间更新定时器，避免访问已删除的UI元素
        static lv_timer_t* timer = NULL;
        if (timer) {
            lv_timer_del(timer);
            timer = NULL;
            ESP_LOGI("UI_MAIN", "Time update timer stopped");
        }

        // 重置全局UI指针
        g_time_label = NULL;
        g_battery_label = NULL;

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
        lv_obj_clean(screen);             // 清空当前屏幕
        ui_image_transfer_create(screen); // 加载图传界面
    }
}

static void serial_display_cb(void) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);             // 清空当前屏幕
        ui_serial_display_create(screen); // 加载串口显示界面
    }
}

static void calibration_cb(void) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);          // 清空当前屏幕
        ui_calibration_create(screen); // 加载校准界面
    }
}

static void test_cb(void) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        // 先停止时间更新定时器，避免访问已删除的UI元素
        static lv_timer_t* timer = NULL;
        if (timer) {
            lv_timer_del(timer);
            timer = NULL;
            ESP_LOGI("UI_MAIN", "Time update timer stopped");
        }

        // 重置全局UI指针
        g_time_label = NULL;
        g_battery_label = NULL;

        lv_obj_clean(screen);   // 清空当前屏幕
        ui_test_create(screen); // 加载测试界面
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
    {"Test", test_cb},
    // 添加更多项...
};

static void btn_event_cb(lv_event_t* e) {
    menu_item_cb_t cb = (menu_item_cb_t)lv_event_get_user_data(e);
    if (cb)
        cb();
}

void ui_main_menu_create(lv_obj_t* parent) {
    // 应用当前主题到屏幕
    theme_apply_to_screen(parent);

    // 创建顶部状态栏
    lv_obj_t* status_bar = lv_obj_create(parent);
    lv_obj_set_size(status_bar, 240, 26);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_0, 0); // 透明背景
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 0, 0);

    // 创建时间显示标签 - 在最左边
    g_time_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(g_time_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_time_label, lv_color_hex(0x000000), 0); // 黑色
    lv_obj_align(g_time_label, LV_ALIGN_LEFT_MID, 6, 0);
    lv_label_set_text(g_time_label, "00:00");

    // 创建电池图标容器 - 在最右边
    lv_obj_t* battery_container = lv_obj_create(status_bar);
    lv_obj_set_size(battery_container, 45, 20);
    lv_obj_align(battery_container, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_opa(battery_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(battery_container, 0, 0);
    lv_obj_set_style_pad_all(battery_container, 0, 0);

    // 创建电池电量显示标签 - 在电池图标左边
    g_battery_label = lv_label_create(battery_container);
    lv_obj_set_style_text_font(g_battery_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_battery_label, lv_color_hex(0x000000), 0); // 黑色
    lv_obj_align(g_battery_label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(g_battery_label, "100%");

    // 创建电池图标
    lv_obj_t* battery_icon = lv_obj_create(battery_container);
    lv_obj_set_size(battery_icon, 20, 12);
    lv_obj_align(battery_icon, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(battery_icon, lv_color_hex(0x000000), 0); // 黑色边框
    lv_obj_set_style_bg_opa(battery_icon, LV_OPA_0, 0);                 // 透明填充
    lv_obj_set_style_border_width(battery_icon, 2, 0);
    lv_obj_set_style_border_color(battery_icon, lv_color_hex(0x000000), 0);
    lv_obj_set_style_radius(battery_icon, 2, 0);

    // 创建电池正极（小凸起）
    lv_obj_t* battery_positive = lv_obj_create(battery_container);
    lv_obj_set_size(battery_positive, 4, 8);
    lv_obj_align(battery_positive, LV_ALIGN_RIGHT_MID, 3, 0);
    lv_obj_set_style_bg_color(battery_positive, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(battery_positive, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(battery_positive, 2, 0);
    lv_obj_set_style_border_width(battery_positive, 0, 0);

    // 创建标题区域 - 在状态栏下方，透明背景
    lv_obj_t* title_container = lv_obj_create(parent);
    lv_obj_set_size(title_container, 240, 38);
    lv_obj_align(title_container, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_opa(title_container, LV_OPA_0, LV_PART_MAIN); // 透明背景
    lv_obj_set_style_border_width(title_container, 0, 0);
    lv_obj_set_style_pad_all(title_container, 8, 0);
    lv_obj_clear_flag(title_container, LV_OBJ_FLAG_SCROLLABLE); // 禁止滚动

    // 创建主标题 - 靠左显示，字体放大加粗，禁止滚动
    lv_obj_t* title = lv_label_create(title_container);
    lv_label_set_text(title, "Browse");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);    // 靠左显示，左边距10像素
    theme_apply_to_label(title, true);                // 应用主题到标题
    lv_obj_clear_flag(title, LV_OBJ_FLAG_SCROLLABLE); // 禁止滚动

    // 计算按钮数量
    int num_items = sizeof(menu_items) / sizeof(menu_item_t);

    // 创建菜单按钮容器
    lv_obj_t* menu_container = lv_obj_create(parent);
    lv_obj_set_size(menu_container, 220, 220);
    lv_obj_align(menu_container, LV_ALIGN_CENTER, 0, 44); // 调整位置，避开状态栏和标题
    lv_obj_set_style_bg_opa(menu_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(menu_container, 0, 0);
    lv_obj_set_style_pad_all(menu_container, 0, 0);
    lv_obj_set_style_width(menu_container, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_opa(menu_container, LV_OPA_0, LV_PART_SCROLLBAR);

    // 创建按钮 - 使用莫兰迪色系
    for (int i = 0; i < num_items; i++) {
        lv_obj_t* btn = lv_obj_create(menu_container);
        lv_obj_set_size(btn, 200, 54);                       // 增加按钮高度到60像素
        lv_obj_align(btn, LV_ALIGN_CENTER, 0, -80 + i * 70); // 调整间距，每个按钮间隔70像素
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, menu_items[i].callback);

        // 设置按钮样式（圆角、阴影等）
        lv_obj_set_style_radius(btn, 15, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn, 6, LV_PART_MAIN);
        lv_obj_set_style_shadow_ofs_y(btn, 3, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_40, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn, 12, LV_PART_MAIN);

        // 应用主题到按钮（只改变颜色）
        theme_apply_to_button(btn, true);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, menu_items[i].text);
        theme_apply_to_label(label, false); // 应用主题到标签
        lv_obj_center(label);
    }

    // 创建时间更新定时器（每分钟更新一次）
    static lv_timer_t* timer = NULL;
    if (timer == NULL) {
        timer = lv_timer_create(time_update_timer_cb, 60000, NULL); // 60秒 = 1分钟
        ESP_LOGI("UI_MAIN", "Time update timer created (60s interval)");
    }

    // 电池电量显示将由任务更新，每5分钟更新一次
    ESP_LOGI("UI_MAIN", "Battery display ready for task updates");

    // 扩展提示：要添加新选项，在 menu_items 数组中添加新项，并实现对应的回调函数
}