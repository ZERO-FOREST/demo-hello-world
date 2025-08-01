#include "../../UI/ui.h"
#include "game.h"

// 返回游戏菜单回调
static void back_to_game_menu(lv_event_t* e) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);
        ui_game_menu_create(screen);
    }
}

void ui_snake_create(lv_obj_t* parent) {
    // 创建标题
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Snake Game");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    // 占位：游戏界面待实现
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, "Snake Placeholder");
    lv_obj_center(label);

    // 返回按钮
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_add_event_cb(btn, back_to_game_menu, LV_EVENT_CLICKED, NULL);
    lv_obj_t* btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(btn_label);
}
