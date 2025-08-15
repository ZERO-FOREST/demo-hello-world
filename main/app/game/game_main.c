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
    // 创建统一标题
    ui_create_page_title(parent, "Game Menu");

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

    // 创建返回按钮 - 使用普通的back按钮函数（返回到主菜单）
    ui_create_back_button(parent, "Back");
}
