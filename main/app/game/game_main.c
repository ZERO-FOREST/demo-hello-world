#include "../../UI/ui.h" // For ui_main_menu_create
#include "game.h"

// --- 回调函数 ---

// 返回主菜单的回调
static void back_to_main_menu_cb(lv_event_t* e) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);
        ui_main_menu_create(screen);
    }
}

// 启动俄罗斯方块的回调
static void tetris_game_cb(lv_event_t* e) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);
        ui_tetris_create(screen);
    }
}

// 启动贪吃蛇的回调
static void snake_game_cb(lv_event_t* e) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);
        ui_snake_create(screen);
    }
}

// --- 游戏菜单创建 ---

void ui_game_menu_create(lv_obj_t* parent) {
    // 创建标题
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Game Menu");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    // 创建游戏列表
    lv_obj_t* list = lv_list_create(parent);
    lv_obj_set_size(list, 240, 200);
    lv_obj_center(list);

    // 添加俄罗斯方块按钮
    lv_obj_t* tetris_btn = lv_list_add_btn(list, LV_SYMBOL_PLAY, "Tetris");
    lv_obj_add_event_cb(tetris_btn, tetris_game_cb, LV_EVENT_CLICKED, NULL);

    // 添加贪吃蛇按钮
    lv_obj_t* snake_btn = lv_list_add_btn(list, LV_SYMBOL_PLAY, "Snake");
    lv_obj_add_event_cb(snake_btn, snake_game_cb, LV_EVENT_CLICKED, NULL);

    // 创建返回按钮
    lv_obj_t* back_btn = lv_btn_create(parent);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_add_event_cb(back_btn, back_to_main_menu_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* label = lv_label_create(back_btn);
    lv_label_set_text(label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(label);
}
