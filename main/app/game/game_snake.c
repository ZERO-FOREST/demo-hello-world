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
    // 创建统一标题
    ui_create_page_title(parent, "Snake Game");

    // 占位：游戏界面待实现
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, "Snake Placeholder");
    lv_obj_center(label);

    // 返回按钮 - 使用统一的back按钮函数
    ui_create_game_back_button(parent, "Back");
}
