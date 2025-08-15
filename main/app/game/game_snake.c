#include "../../UI/ui.h"
#include "game.h"
#include "theme_manager.h"

// 返回游戏菜单回调
static void back_to_game_menu(lv_event_t* e) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);
        ui_game_menu_create(screen);
    }
}

void ui_snake_create(lv_obj_t* parent) {
    // 应用当前主题到屏幕
    theme_apply_to_screen(parent);

    // 1. 创建页面父级容器（统一管理整个页面）
    lv_obj_t* page_parent_container;
    ui_create_page_parent_container(parent, &page_parent_container);

    // 2. 创建顶部栏容器（包含返回按钮和标题）
    lv_obj_t* top_bar_container;
    lv_obj_t* title_container;
    ui_create_top_bar(page_parent_container, "Snake Game", &top_bar_container, &title_container);

    // 替换顶部栏的返回按钮回调为自定义回调
    lv_obj_t* back_btn = lv_obj_get_child(top_bar_container, 0); // 获取返回按钮
    if (back_btn) {
        lv_obj_remove_event_cb(back_btn, NULL); // 移除默认回调
        lv_obj_add_event_cb(back_btn, back_to_game_menu, LV_EVENT_CLICKED, NULL);
    }

    // 3. 创建页面内容容器
    lv_obj_t* content_container;
    ui_create_page_content_area(page_parent_container, &content_container);

    // 4. 在content_container中添加页面内容
    // 占位：游戏界面待实现
    lv_obj_t* label = lv_label_create(content_container);
    lv_label_set_text(label, "Snake Placeholder");
    theme_apply_to_label(label, false);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}
