#include "ui.h"
#include "wifi_manager.h" // 引入WiFi管理器头文件
#include <stdio.h>        // 为了使用 snprintf

// 为了实现返回功能，需要前向声明主菜单的创建函数
void ui_main_menu_create(lv_obj_t* parent);

// UI元素句柄
static lv_obj_t* g_status_label;
static lv_obj_t* g_ip_label;
static lv_obj_t* g_mac_label;
static lv_timer_t* g_update_timer;

/**
 * @brief 更新WiFi信息显示
 */
static void update_wifi_info(void) {
    wifi_manager_info_t info = wifi_manager_get_info();

    switch (info.state) {
    case WIFI_STATE_DISABLED:
        lv_label_set_text(g_status_label, "Status: Disabled");
        break;
    case WIFI_STATE_DISCONNECTED:
        lv_label_set_text(g_status_label, "Status: Disconnected");
        break;
    case WIFI_STATE_CONNECTING:
        lv_label_set_text(g_status_label, "Status: Connecting...");
        break;
    case WIFI_STATE_CONNECTED:
        lv_label_set_text(g_status_label, "Status: Connected");
        break;
    }

    lv_label_set_text_fmt(g_ip_label, "IP Address: %s", info.ip_addr);

    char mac_str[24]; // 增加缓冲区大小以容纳完整的MAC地址字符串
    snprintf(mac_str, sizeof(mac_str), "MAC: %02X:%02X:%02X:%02X:%02X:%02X", info.mac_addr[0], info.mac_addr[1],
             info.mac_addr[2], info.mac_addr[3], info.mac_addr[4], info.mac_addr[5]);
    lv_label_set_text(g_mac_label, mac_str);
}

/**
 * @brief UI更新定时器的回调
 * @param timer
 */
static void ui_update_timer_cb(lv_timer_t* timer) { update_wifi_info(); }

/**
 * @brief 返回按钮的事件回调
 * @param e 事件对象
 */
static void back_btn_event_cb(lv_event_t* e) {
    lv_obj_t* screen = (lv_obj_t*)lv_event_get_user_data(e);
    if (screen) {
        if (g_update_timer) {
            lv_timer_del(g_update_timer);
            g_update_timer = NULL;
        }
        lv_obj_clean(screen);        // 清空当前屏幕的所有内容
        ui_main_menu_create(screen); // 重新创建主菜单
    }
}

/**
 * @brief WiFi开关的事件回调
 * @param e 事件对象
 */
static void wifi_switch_event_cb(lv_event_t* e) {
    lv_obj_t* switcher = lv_event_get_target(e);

    if (lv_obj_has_state(switcher, LV_STATE_CHECKED)) {
        wifi_manager_start();
    } else {
        wifi_manager_stop();
    }
    update_wifi_info();
}

/**
 * @brief WiFi功率滑块的事件回调
 * @param e 事件对象
 */
static void power_slider_event_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    lv_obj_t* power_label = (lv_obj_t*)lv_event_get_user_data(e);
    int32_t power_dbm = lv_slider_get_value(slider);

    lv_label_set_text_fmt(power_label, "Tx Power: %" LV_PRId32 " dBm", power_dbm);
    wifi_manager_set_power((int8_t)power_dbm);
}

/**
 * @brief 创建WiFi设置界面的函数
 * @param parent 父对象, 通常是 lv_scr_act()
 */
void ui_wifi_settings_create(lv_obj_t* parent) {
    // 初始化WiFi管理器并注册回调
    wifi_manager_init(update_wifi_info);

    // --- 统一标题 ---
    ui_create_page_title(parent, "WiFi Settings");

    // --- 返回按钮 ---
    ui_create_back_button(parent, "Back");

    // --- 创建一个容器来组织所有控件 ---
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(95), lv_pct(80));
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 10, 0); // 设置行间距

    // --- 1. WiFi开关 ---
    lv_obj_t* switch_cont = lv_obj_create(cont);
    lv_obj_set_size(switch_cont, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(switch_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(switch_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(switch_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* wifi_label = lv_label_create(switch_cont);
    lv_label_set_text(wifi_label, "Enable WiFi");
    lv_obj_t* wifi_switch = lv_switch_create(switch_cont);

    // --- 2. WiFi功率控制 ---
    lv_obj_t* slider_cont = lv_obj_create(cont);
    lv_obj_set_size(slider_cont, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(slider_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(slider_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(slider_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t* power_val_label = lv_label_create(slider_cont);
    lv_label_set_text(power_val_label, "Tx Power: 20 dBm");

    lv_obj_t* power_slider = lv_slider_create(slider_cont);
    lv_obj_set_width(power_slider, lv_pct(100));
    lv_slider_set_range(power_slider, 2, 20);           // ESP32功率范围
    lv_slider_set_value(power_slider, 20, LV_ANIM_OFF); // 默认最大功率

    // --- 3. WiFi信息显示 ---
    lv_obj_t* info_cont = lv_obj_create(cont);
    lv_obj_set_size(info_cont, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(info_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_top(info_cont, 10, 0);

    g_status_label = lv_label_create(info_cont);
    g_ip_label = lv_label_create(info_cont);
    g_mac_label = lv_label_create(info_cont);

    // --- 绑定事件 ---
    lv_obj_add_event_cb(wifi_switch, wifi_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(power_slider, power_slider_event_cb, LV_EVENT_VALUE_CHANGED, power_val_label);

    // 初始状态更新
    update_wifi_info();

    // 创建一个定时器来周期性更新UI
    g_update_timer = lv_timer_create(ui_update_timer_cb, 1000, NULL);
}