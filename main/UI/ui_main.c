#include "ui.h"

// 菜单项回调函数类型
typedef void (*menu_item_cb_t)(void);

// 示例回调函数（后续扩展时实现）
static void option1_cb(void) {
    // TODO: 选项1 逻辑
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "选项1 被点击");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

static void wifi_settings_cb(void) {
    lv_obj_t *screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen); // 清空当前屏幕
        ui_wifi_settings_create(screen); // 加载WiFi设置界面
    }
}

static void settings_cb(void) {
    // TODO: 设置逻辑
}

// 可扩展的菜单项结构
typedef struct {
    const char *text;
    menu_item_cb_t callback;
} menu_item_t;

// 示例菜单项数组（易于扩展）
static menu_item_t menu_items[] = {
    {"选项1", option1_cb},
    {"WiFi 设置", wifi_settings_cb},
    {"设置", settings_cb},
    // 添加更多项...
};

static void btn_event_cb(lv_event_t *e) {
    menu_item_cb_t cb = (menu_item_cb_t)lv_event_get_user_data(e);
    if (cb) cb();
}

void ui_main_menu_create(lv_obj_t *parent) {
    // 创建标题
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "主菜单");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    // 计算按钮数量
    int num_items = sizeof(menu_items) / sizeof(menu_item_t);

    // 创建按钮
    for (int i = 0; i < num_items; i++) {
        lv_obj_t *btn = lv_btn_create(parent);
        lv_obj_set_size(btn, 200, 40);
        lv_obj_align(btn, LV_ALIGN_CENTER, 0, -50 + i * 50);
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, menu_items[i].callback);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, menu_items[i].text);
        lv_obj_center(label);
    }

    // 扩展提示：要添加新选项，在 menu_items 数组中添加新项，并实现对应的回调函数
} 