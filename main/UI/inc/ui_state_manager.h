/**
 * @file ui_state_manager.h
 * @brief UI状态管理器头文件 - 用于保存和恢复界面状态
 * @author TidyCraze
 * @date 2025-08-25
 */

#ifndef UI_STATE_MANAGER_H
#define UI_STATE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdbool.h>

// 屏幕类型枚举
typedef enum {
    UI_SCREEN_MAIN_MENU = 0,
    UI_SCREEN_WIFI_SETTINGS,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_GAME,
    UI_SCREEN_IMAGE_TRANSFER,
    UI_SCREEN_SERIAL_DISPLAY,
    UI_SCREEN_CALIBRATION,
    UI_SCREEN_TEST,
    UI_SCREEN_TELEMETRY,
    UI_SCREEN_AUDIO,
    UI_SCREEN_MAX
} ui_screen_type_t;

// 主菜单状态结构
typedef struct {
    int selected_index;      // 当前选中的菜单项索引
    int scroll_position;     // 滚动位置
    bool is_valid;          // 状态是否有效
    uint32_t timestamp;     // 状态保存时间戳
} ui_main_menu_state_t;

// 通用页面状态结构 - 用于保存其他页面的状态
typedef struct {
    int scroll_position;     // 滚动位置
    int selected_item;       // 选中项
    uint32_t timestamp;     // 状态保存时间戳
    bool is_valid;          // 状态是否有效
    char custom_data[64];   // 自定义数据缓冲区
} ui_page_state_t;

// 整体UI状态管理结构
typedef struct {
    ui_main_menu_state_t main_menu;     // 主菜单状态
    ui_screen_type_t current_screen;    // 当前屏幕类型
    ui_page_state_t page_states[UI_SCREEN_MAX]; // 各个页面的状态
    uint32_t total_memory_used;         // 总共使用的内存大小
    bool psram_available;               // PSRAM是否可用
} ui_state_manager_t;

/**
 * @brief 初始化UI状态管理器
 */
void ui_state_manager_init(void);

/**
 * @brief 保存主菜单状态
 * @param menu_container 菜单容器对象
 * @param selected_index 当前选中的菜单项索引
 * @param scroll_position 当前滚动位置
 */
void ui_state_manager_save_main_menu(lv_obj_t* menu_container, int selected_index, int scroll_position);

/**
 * @brief 获取保存的主菜单状态
 * @return 返回主菜单状态指针，如果无效则返回NULL
 */
ui_main_menu_state_t* ui_state_manager_get_main_menu_state(void);

/**
 * @brief 清除主菜单状态
 */
void ui_state_manager_clear_main_menu_state(void);

/**
 * @brief 保存当前屏幕状态
 * @param screen_type 屏幕类型
 */
void ui_state_manager_save_current_screen(ui_screen_type_t screen_type);

/**
 * @brief 获取当前屏幕类型
 * @return 当前屏幕类型
 */
ui_screen_type_t ui_state_manager_get_current_screen(void);

/**
 * @brief 检查是否需要恢复状态
 * @return true如果需要恢复状态，false如果不需要
 */
bool ui_state_manager_should_restore_state(void);

/**
 * @brief 清理状态管理器并释放PSRAM内存
 */
void ui_state_manager_deinit(void);

/**
 * @brief 保存页面状态到PSRAM
 * @param screen_type 屏幕类型
 * @param scroll_position 滚动位置
 * @param selected_item 选中项
 * @param custom_data 自定义数据
 */
void ui_state_manager_save_page_state(ui_screen_type_t screen_type, int scroll_position, 
                                      int selected_item, const char* custom_data);

/**
 * @brief 获取页面状态
 * @param screen_type 屏幕类型
 * @return 返回页面状态指针，如果无效则返回NULL
 */
ui_page_state_t* ui_state_manager_get_page_state(ui_screen_type_t screen_type);

/**
 * @brief 获取PSRAM使用情况统计
 * @param total_allocated 返回总共分配的内存大小
 * @param is_psram 返回是否使用了PSRAM
 * @return true如果获取成功，false如果失败
 */
bool ui_state_manager_get_memory_info(size_t* total_allocated, bool* is_psram);

#ifdef __cplusplus
}
#endif

#endif // UI_STATE_MANAGER_H
