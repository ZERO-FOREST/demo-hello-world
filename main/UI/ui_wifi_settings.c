#include "ui.h"
#include "wifi_manager.h" // 引入WiFi管理器头文件
#include <stdio.h>        // 为了使用 snprintf

// 为了实现返回功能，需要前向声明主菜单的创建函数
void ui_main_menu_create(lv_obj_t* parent);

// UI元素句柄
static lv_obj_t* g_status_label;
static lv_obj_t* g_ip_label;
static lv_obj_t* g_mac_label;
static lv_obj_t* g_ssid_label; // 新增SSID标签
static lv_timer_t* g_update_timer;
static bool g_wifi_ui_initialized = false;

static void wifi_dropdown_event_cb(lv_event_t* e);

/**
 * @brief 更新WiFi信息显示
 */
static void update_wifi_info(void) {
    if (!g_wifi_ui_initialized) {
        return;
    }

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

    char mac_str[24];
    snprintf(mac_str, sizeof(mac_str), "MAC: %02X:%02X:%02X:%02X:%02X:%02X", info.mac_addr[0], info.mac_addr[1],
             info.mac_addr[2], info.mac_addr[3], info.mac_addr[4], info.mac_addr[5]);
    lv_label_set_text(g_mac_label, mac_str);

    if (info.state == WIFI_STATE_CONNECTED) {
        lv_label_set_text_fmt(g_ssid_label, "SSID: %s", info.ssid);
    } else {
        lv_label_set_text(g_ssid_label, "SSID: N/A");
    }
}

/**
 * @brief UI更新定时器的回调
 * @param timer
 */
static void ui_update_timer_cb(lv_timer_t* timer) {
    if (g_wifi_ui_initialized) {
        update_wifi_info();
    }
}

/**
 * @brief 返回按钮的事件回调
 * @param e 事件对象
 */
static void back_btn_event_cb(lv_event_t* e) {
    lv_obj_t* screen = (lv_obj_t*)lv_event_get_user_data(e);
    if (screen) {
        // 停止定时器
        if (g_update_timer) {
            lv_timer_del(g_update_timer);
            g_update_timer = NULL;
        }

        // 标记界面已销毁
        g_wifi_ui_initialized = false;

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
    // 标记界面已初始化
    g_wifi_ui_initialized = true;

    // 应用当前主题到屏幕
    theme_apply_to_screen(parent);

    // 1. 创建页面父级容器（统一管理整个页面）
    lv_obj_t* page_parent_container;
    ui_create_page_parent_container(parent, &page_parent_container);

    // 2. 创建顶部栏容器（包含返回按钮和标题）
    lv_obj_t* top_bar_container;
    lv_obj_t* title_container;
    ui_create_top_bar(page_parent_container, "WiFi Settings", false, &top_bar_container, &title_container, NULL);

    // 替换顶部栏的返回按钮回调为自定义回调
    lv_obj_t* back_btn = lv_obj_get_child(top_bar_container, 0); // 获取返回按钮
    if (back_btn) {
        lv_obj_remove_event_cb(back_btn, NULL); // 移除默认回调
        lv_obj_add_event_cb(back_btn, back_btn_event_cb, LV_EVENT_CLICKED, parent);
    }

    // 3. 创建页面内容容器
    lv_obj_t* content_container;
    ui_create_page_content_area(page_parent_container, &content_container);

    // 4. 在content_container中添加页面内容
    // 创建一个容器来组织所有控件
    lv_obj_t* cont = lv_obj_create(content_container);
    lv_obj_set_size(cont, 220, 200);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 10, 0); // 设置行间距

    // --- 1. WiFi开关 ---
    lv_obj_t* switch_cont = lv_obj_create(cont);
    lv_obj_set_size(switch_cont, 200, LV_SIZE_CONTENT);
    lv_obj_set_layout(switch_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(switch_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(switch_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* wifi_label = lv_label_create(switch_cont);
    lv_label_set_text(wifi_label, "Enable WiFi");
    theme_apply_to_label(wifi_label, false);

    lv_obj_t* wifi_switch = lv_switch_create(switch_cont);
    theme_apply_to_switch(wifi_switch);

    // --- 2. WiFi功率控制 ---
    lv_obj_t* slider_cont = lv_obj_create(cont);
    lv_obj_set_size(slider_cont, 200, LV_SIZE_CONTENT);
    lv_obj_set_layout(slider_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(slider_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(slider_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t* power_val_label = lv_label_create(slider_cont);
    lv_label_set_text(power_val_label, "Tx Power: 20 dBm");
    theme_apply_to_label(power_val_label, false);

    lv_obj_t* power_slider = lv_slider_create(slider_cont);
    lv_obj_set_width(power_slider, 180);
    lv_slider_set_range(power_slider, 2, 20);           // ESP32功率范围
    lv_slider_set_value(power_slider, 20, LV_ANIM_OFF); // 默认最大功率

    // --- 3. WiFi连接列表 ---
    lv_obj_t* dropdown_cont = lv_obj_create(cont);
    lv_obj_set_size(dropdown_cont, 200, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dropdown_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dropdown_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t* dropdown_label = lv_label_create(dropdown_cont);
    lv_label_set_text(dropdown_label, "Saved Networks");
    theme_apply_to_label(dropdown_label, false);

    lv_obj_t* wifi_dropdown = lv_dropdown_create(dropdown_cont);
    lv_obj_set_width(wifi_dropdown, 180);
    theme_apply_to_button(wifi_dropdown, false);

    // 填充WiFi列表
    int32_t wifi_count = wifi_manager_get_wifi_list_size();
    char ssid_list[512] = {0}; // 假设足够大
    for (int i = 0; i < wifi_count; i++) {
        const char* ssid = wifi_manager_get_wifi_ssid_by_index(i);
        if (ssid) {
            strlcat(ssid_list, ssid, sizeof(ssid_list));
            if (i < wifi_count - 1) {
                strlcat(ssid_list, "\n", sizeof(ssid_list));
            }
        }
    }
    lv_dropdown_set_options(wifi_dropdown, ssid_list);

    // 添加事件回调
    lv_obj_add_event_cb(wifi_dropdown, wifi_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // --- 4. WiFi信息显示 ---
    lv_obj_t* info_cont = lv_obj_create(cont);
    lv_obj_set_size(info_cont, 200, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(info_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_top(info_cont, 10, 0);

    g_status_label = lv_label_create(info_cont);
    theme_apply_to_label(g_status_label, false);

    g_ip_label = lv_label_create(info_cont);
    theme_apply_to_label(g_ip_label, false);

    g_mac_label = lv_label_create(info_cont);
    theme_apply_to_label(g_mac_label, false);

    g_ssid_label = lv_label_create(info_cont); // 创建SSID标签
    theme_apply_to_label(g_ssid_label, false);

    // 初始化WiFi状态和信息
    wifi_manager_info_t current_info = wifi_manager_get_info();
    if (current_info.state == WIFI_STATE_DISABLED) {
        lv_obj_clear_state(wifi_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_add_state(wifi_switch, LV_STATE_CHECKED);
    }
    int8_t power_dbm = 20;
    lv_slider_set_value(power_slider, power_dbm, LV_ANIM_OFF);
    lv_label_set_text_fmt(power_val_label, "Tx Power: %d dBm", power_dbm);

    // 添加事件回调
    lv_obj_add_event_cb(wifi_switch, wifi_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(power_slider, power_slider_event_cb, LV_EVENT_VALUE_CHANGED, power_val_label);

    // 创建并启动UI更新定时器
    g_update_timer = lv_timer_create(ui_update_timer_cb, 500, NULL);

    // 立即更新一次UI
    update_wifi_info();
}

static void wifi_dropdown_event_cb(lv_event_t* e)
{
    lv_obj_t* dropdown = lv_event_get_target(e);
    uint16_t selected_index = lv_dropdown_get_selected(dropdown);
    wifi_manager_connect_to_index(selected_index);
}