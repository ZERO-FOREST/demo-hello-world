/**
 * @file ui_state_manager.c
 * @brief UI状态管理器 - 用于保存和恢复界面状态 (使用PSRAM)
 * @author TidyCraze
 * @date 2025-08-25
 */

#include "ui_state_manager.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char* TAG = "UI_STATE_MGR";

// 使用PSRAM存储的全局状态管理结构指针
static ui_state_manager_t* g_ui_state = NULL;

/**
 * @brief 初始化UI状态管理器
 */
void ui_state_manager_init(void) {
    // 如果已经初始化过，先释放之前的内存
    if (g_ui_state != NULL) {
        heap_caps_free(g_ui_state);
        g_ui_state = NULL;
    }

    // 在PSRAM中分配内存
    g_ui_state = (ui_state_manager_t*)heap_caps_malloc(sizeof(ui_state_manager_t), 
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    if (g_ui_state == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory in PSRAM, trying internal RAM");
        // 如果PSRAM分配失败，回退到内部RAM
        g_ui_state = (ui_state_manager_t*)heap_caps_malloc(sizeof(ui_state_manager_t), 
                                                           MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (g_ui_state == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for UI state manager");
            return;
        }
        ESP_LOGW(TAG, "UI State Manager using internal RAM instead of PSRAM");
    } else {
        ESP_LOGI(TAG, "UI State Manager successfully allocated %d bytes in PSRAM", sizeof(ui_state_manager_t));
    }

    // 初始化结构体
    memset(g_ui_state, 0, sizeof(ui_state_manager_t));
    g_ui_state->main_menu.is_valid = false;
    g_ui_state->psram_available = (heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) > 0);
    g_ui_state->total_memory_used = sizeof(ui_state_manager_t);
    
    // 初始化所有页面状态
    for (int i = 0; i < UI_SCREEN_MAX; i++) {
        g_ui_state->page_states[i].is_valid = false;
        g_ui_state->page_states[i].timestamp = 0;
    }
    
    ESP_LOGI(TAG, "UI State Manager initialized with PSRAM support (PSRAM available: %s)", 
             g_ui_state->psram_available ? "YES" : "NO");
}

/**
 * @brief 保存主菜单状态
 */
void ui_state_manager_save_main_menu(lv_obj_t* menu_container, int selected_index, int scroll_position) {
    if (!g_ui_state) {
        ESP_LOGE(TAG, "UI State Manager not initialized");
        return;
    }

    if (!menu_container) {
        ESP_LOGW(TAG, "Invalid menu container for saving state");
        return;
    }

    g_ui_state->main_menu.selected_index = selected_index;
    g_ui_state->main_menu.scroll_position = scroll_position;
    g_ui_state->main_menu.is_valid = true;
    g_ui_state->main_menu.timestamp = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "Main menu state saved to PSRAM: selected=%d, scroll=%d, timestamp=%lu", 
             selected_index, scroll_position, g_ui_state->main_menu.timestamp);
}

/**
 * @brief 获取保存的主菜单状态
 */
ui_main_menu_state_t* ui_state_manager_get_main_menu_state(void) {
    if (!g_ui_state) {
        ESP_LOGW(TAG, "UI State Manager not initialized");
        return NULL;
    }

    if (g_ui_state->main_menu.is_valid) {
        // 从PSRAM获取主菜单状态
        return &g_ui_state->main_menu;
    }
    return NULL;
}

/**
 * @brief 清除主菜单状态
 */
void ui_state_manager_clear_main_menu_state(void) {
    if (!g_ui_state) {
        ESP_LOGW(TAG, "UI State Manager not initialized");
        return;
    }

    g_ui_state->main_menu.is_valid = false;
    ESP_LOGI(TAG, "Main menu state cleared from PSRAM");
}

/**
 * @brief 保存当前屏幕状态
 */
void ui_state_manager_save_current_screen(ui_screen_type_t screen_type) {
    if (!g_ui_state) {
        ESP_LOGW(TAG, "UI State Manager not initialized");
        return;
    }

    g_ui_state->current_screen = screen_type;
    ESP_LOGI(TAG, "Current screen saved to PSRAM: %d", screen_type);
}

/**
 * @brief 获取当前屏幕类型
 */
ui_screen_type_t ui_state_manager_get_current_screen(void) {
    if (!g_ui_state) {
        ESP_LOGW(TAG, "UI State Manager not initialized");
        return UI_SCREEN_MAIN_MENU;
    }

    return g_ui_state->current_screen;
}

/**
 * @brief 检查是否需要恢复状态
 */
bool ui_state_manager_should_restore_state(void) {
    if (!g_ui_state) {
        return false;
    }
    return g_ui_state->main_menu.is_valid;
}

/**
 * @brief 清理状态管理器并释放PSRAM内存
 */
void ui_state_manager_deinit(void) {
    if (g_ui_state != NULL) {
        ESP_LOGI(TAG, "Freeing UI state manager PSRAM memory (total used: %lu bytes)", 
                 g_ui_state->total_memory_used);
        heap_caps_free(g_ui_state);
        g_ui_state = NULL;
    }
}

/**
 * @brief 保存页面状态到PSRAM
 */
void ui_state_manager_save_page_state(ui_screen_type_t screen_type, int scroll_position, 
                                      int selected_item, const char* custom_data) {
    if (!g_ui_state) {
        ESP_LOGE(TAG, "UI State Manager not initialized");
        return;
    }

    if (screen_type >= UI_SCREEN_MAX) {
        ESP_LOGW(TAG, "Invalid screen type: %d", screen_type);
        return;
    }

    ui_page_state_t* page_state = &g_ui_state->page_states[screen_type];
    page_state->scroll_position = scroll_position;
    page_state->selected_item = selected_item;
    page_state->timestamp = xTaskGetTickCount();
    page_state->is_valid = true;

    // 复制自定义数据
    if (custom_data) {
        strncpy(page_state->custom_data, custom_data, sizeof(page_state->custom_data) - 1);
        page_state->custom_data[sizeof(page_state->custom_data) - 1] = '\0';
    } else {
        page_state->custom_data[0] = '\0';
    }

    ESP_LOGI(TAG, "Page state saved to PSRAM: screen=%d, scroll=%d, selected=%d, timestamp=%lu", 
             screen_type, scroll_position, selected_item, page_state->timestamp);
}

/**
 * @brief 获取页面状态
 */
ui_page_state_t* ui_state_manager_get_page_state(ui_screen_type_t screen_type) {
    if (!g_ui_state) {
        ESP_LOGW(TAG, "UI State Manager not initialized");
        return NULL;
    }

    if (screen_type >= UI_SCREEN_MAX) {
        ESP_LOGW(TAG, "Invalid screen type: %d", screen_type);
        return NULL;
    }

    ui_page_state_t* page_state = &g_ui_state->page_states[screen_type];
    if (page_state->is_valid) {
        // 从PSRAM获取页面状态
        return page_state;
    }
    
    return NULL;
}

/**
 * @brief 获取PSRAM使用情况统计
 */
bool ui_state_manager_get_memory_info(size_t* total_allocated, bool* is_psram) {
    if (!g_ui_state) {
        return false;
    }

    if (total_allocated) {
        *total_allocated = g_ui_state->total_memory_used;
    }
    
    if (is_psram) {
        *is_psram = g_ui_state->psram_available;
    }

    return true;
}
