/**
 * @file ui_serial_display.c
 * @brief 串口显示界面 - 支持接收数据、自动换行、时间戳显示
 * @author Your Name
 * @date 2024
 */
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "serial_display.h"
#include "ui.h"

static const char* TAG = "UI_SERIAL_DISPLAY";

// 最大保存行数
#define MAX_LINES 1024
#define MAX_LINE_LENGTH 256
#define MAX_DISPLAY_LINES 20 // 界面显示的行数

// 行数据结构
typedef struct {
    char text[MAX_LINE_LENGTH];
    char timestamp[32];
    time_t timestamp_raw;
} display_line_t;

// 全局变量
static lv_obj_t* g_serial_display_screen = NULL;
static lv_obj_t* g_text_area = NULL;
static lv_obj_t* g_status_label = NULL;
static lv_obj_t* g_back_btn = NULL;
static lv_obj_t* g_clear_btn = NULL;
static lv_obj_t* g_scroll_area = NULL;

// 数据缓冲区 - 使用PSRAM
static display_line_t* g_lines = NULL;
static int g_line_count = 0;
static int g_current_index = 0;
static bool g_auto_scroll = true;
static bool g_buffer_initialized = false;

// 消息队列
static QueueHandle_t g_display_queue = NULL;
static TaskHandle_t g_display_task_handle = NULL;
static bool g_display_running = false;

// 消息类型
typedef enum { MSG_NEW_DATA, MSG_CLEAR_DISPLAY, MSG_UPDATE_STATUS } display_msg_type_t;

typedef struct {
    display_msg_type_t type;
    char data[MAX_LINE_LENGTH];
    time_t timestamp;
} display_msg_t;

// 初始化PSRAM缓冲区
static esp_err_t init_psram_buffer(void) {
    if (g_buffer_initialized) {
        return ESP_OK;
    }

    // 分配PSRAM内存
    g_lines = (display_line_t*)heap_caps_malloc(MAX_LINES * sizeof(display_line_t), MALLOC_CAP_SPIRAM);
    if (g_lines == NULL) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM buffer for %d lines", MAX_LINES);
        return ESP_ERR_NO_MEM;
    }

    // 初始化缓冲区
    memset(g_lines, 0, MAX_LINES * sizeof(display_line_t));
    g_line_count = 0;
    g_current_index = 0;
    g_auto_scroll = true;
    g_buffer_initialized = true;

    ESP_LOGI(TAG, "PSRAM buffer initialized: %d lines, %d bytes", MAX_LINES, MAX_LINES * sizeof(display_line_t));
    return ESP_OK;
}

// 清理PSRAM缓冲区
static void cleanup_psram_buffer(void) {
    if (g_lines != NULL) {
        heap_caps_free(g_lines);
        g_lines = NULL;
    }
    g_buffer_initialized = false;
    g_line_count = 0;
    g_current_index = 0;
}

// 获取时间戳字符串
static void get_timestamp_str(char* timestamp_str, size_t max_len, time_t timestamp) {
    struct tm* timeinfo = localtime(&timestamp);
    if (timeinfo) {
        snprintf(timestamp_str, max_len, "[%02d:%02d:%02d]", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    } else {
        snprintf(timestamp_str, max_len, "[--:--:--]");
    }
}

// 添加新行到缓冲区
static void add_line(const char* text, time_t timestamp) {
    if (!g_buffer_initialized || g_lines == NULL) {
        ESP_LOGE(TAG, "Buffer not initialized");
        return;
    }

    if (g_line_count < MAX_LINES) {
        // 添加新行
        strncpy(g_lines[g_line_count].text, text, MAX_LINE_LENGTH - 1);
        g_lines[g_line_count].text[MAX_LINE_LENGTH - 1] = '\0';
        g_lines[g_line_count].timestamp_raw = timestamp;
        get_timestamp_str(g_lines[g_line_count].timestamp, sizeof(g_lines[g_line_count].timestamp), timestamp);
        g_line_count++;
    } else {
        // 缓冲区满，移动所有行向前
        memmove(&g_lines[0], &g_lines[1], sizeof(display_line_t) * (MAX_LINES - 1));
        // 添加新行到最后
        strncpy(g_lines[MAX_LINES - 1].text, text, MAX_LINE_LENGTH - 1);
        g_lines[MAX_LINES - 1].text[MAX_LINE_LENGTH - 1] = '\0';
        g_lines[MAX_LINES - 1].timestamp_raw = timestamp;
        get_timestamp_str(g_lines[MAX_LINES - 1].timestamp, sizeof(g_lines[MAX_LINES - 1].timestamp), timestamp);
    }
}

// 更新显示内容
static void update_display(void) {
    if (!g_text_area || !g_buffer_initialized || g_lines == NULL)
        return;

    // 构建显示文本
    char display_text[8192] = ""; // 足够大的缓冲区
    int start_line = 0;

    if (g_auto_scroll) {
        // 自动滚动模式：显示最后MAX_DISPLAY_LINES行
        start_line = (g_line_count > MAX_DISPLAY_LINES) ? (g_line_count - MAX_DISPLAY_LINES) : 0;
    } else {
        // 手动滚动模式：从当前索引开始显示
        start_line = g_current_index;
        if (start_line >= g_line_count) {
            start_line = (g_line_count > MAX_DISPLAY_LINES) ? (g_line_count - MAX_DISPLAY_LINES) : 0;
        }
    }

    int display_count = 0;
    for (int i = start_line; i < g_line_count && display_count < MAX_DISPLAY_LINES; i++) {
        char line_text[512];
        snprintf(line_text, sizeof(line_text), "%s %s\n", g_lines[i].timestamp, g_lines[i].text);
        strcat(display_text, line_text);
        display_count++;
    }

    // 更新文本区域
    lv_textarea_set_text(g_text_area, display_text);

    // 更新状态信息
    if (g_status_label) {
        char status_text[128];
        snprintf(status_text, sizeof(status_text), "Lines: %d/%d | Auto: %s", g_line_count, MAX_LINES,
                 g_auto_scroll ? "ON" : "OFF");
        lv_label_set_text(g_status_label, status_text);
    }
}

// 清空显示
static void clear_display(void) {
    if (!g_buffer_initialized || g_lines == NULL)
        return;

    g_line_count = 0;
    g_current_index = 0;
    memset(g_lines, 0, MAX_LINES * sizeof(display_line_t));
    update_display();
}

// 显示任务 - 处理消息队列
static void display_task(void* pvParameters) {
    display_msg_t msg;

    ESP_LOGI(TAG, "Display task started");

    while (g_display_running) {
        if (xQueueReceive(g_display_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (msg.type) {
            case MSG_NEW_DATA:
                add_line(msg.data, msg.timestamp);
                update_display();
                break;

            case MSG_CLEAR_DISPLAY:
                clear_display();
                break;

            case MSG_UPDATE_STATUS:
                update_display();
                break;

            default:
                break;
            }
        }
    }

    ESP_LOGI(TAG, "Display task stopped");
    vTaskDelete(NULL);
}

// 按钮事件回调
static void back_btn_event_cb(lv_event_t* e) {
    lv_obj_t* screen = lv_scr_act();
    if (screen) {
        lv_obj_clean(screen);
        ui_main_menu_create(screen);
    }
}

static void clear_btn_event_cb(lv_event_t* e) {
    display_msg_t msg = {.type = MSG_CLEAR_DISPLAY};
    xQueueSend(g_display_queue, &msg, pdMS_TO_TICKS(100));
}

// 滚动事件回调
static void scroll_event_cb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_SCROLL) {
        // 用户手动滚动，关闭自动滚动
        g_auto_scroll = false;

        // 根据滚动位置调整当前索引
        lv_coord_t scroll_y = lv_obj_get_scroll_y(obj);
        lv_coord_t height = lv_obj_get_height(obj);

        if (height > 0) {
            int scroll_percent = (scroll_y * 100) / height;
            g_current_index = (scroll_percent * g_line_count) / 100;
            if (g_current_index >= g_line_count) {
                g_current_index = g_line_count - 1;
            }
            if (g_current_index < 0) {
                g_current_index = 0;
            }
        }
    }
}

// 公共API：添加新数据
void ui_serial_display_add_data(const char* data, size_t len) {
    if (!g_display_running || !data || len == 0) {
        return;
    }

    // 处理数据，按行分割
    char* data_copy = malloc(len + 1);
    if (!data_copy) {
        ESP_LOGE(TAG, "Failed to allocate memory for data copy");
        return;
    }

    memcpy(data_copy, data, len);
    data_copy[len] = '\0';

    char* line = strtok(data_copy, "\n\r");
    time_t current_time = time(NULL);

    while (line) {
        // 跳过空行
        if (strlen(line) > 0) {
            display_msg_t msg = {.type = MSG_NEW_DATA, .timestamp = current_time};
            strncpy(msg.data, line, MAX_LINE_LENGTH - 1);
            msg.data[MAX_LINE_LENGTH - 1] = '\0';

            xQueueSend(g_display_queue, &msg, pdMS_TO_TICKS(100));
        }
        line = strtok(NULL, "\n\r");
    }

    free(data_copy);
}

// 公共API：添加文本
void ui_serial_display_add_text(const char* text) {
    if (text) {
        ui_serial_display_add_data(text, strlen(text));
    }
}

// 创建串口显示界面
void ui_serial_display_create(lv_obj_t* parent) {
    g_serial_display_screen = parent;

    // 初始化PSRAM缓冲区
    esp_err_t ret = init_psram_buffer();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize PSRAM buffer");
        return;
    }

    // 创建状态栏容器 - 与主菜单保持一致
    lv_obj_t* status_bar = lv_obj_create(parent);
    lv_obj_set_size(status_bar, 320, 40);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_90, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 5, 0);

    // 创建标题 - 在状态栏内居中
    lv_obj_t* title = lv_label_create(status_bar);
    lv_label_set_text(title, "Serial Display");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0); // 统一使用24号字体
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    // 创建状态标签 - 在状态栏右侧
    g_status_label = lv_label_create(status_bar);
    lv_obj_align(g_status_label, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_text_font(g_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_status_label, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(g_status_label, "Lines: 0/1024 | Auto: ON");

    // 创建滚动区域 - 在状态栏下方
    g_scroll_area = lv_obj_create(parent);
    lv_obj_set_size(g_scroll_area, 300, 200);
    lv_obj_align(g_scroll_area, LV_ALIGN_CENTER, 0, 20); // 向下偏移避开状态栏
    lv_obj_set_style_border_width(g_scroll_area, 2, 0);
    lv_obj_set_style_border_color(g_scroll_area, lv_color_hex(0x3498DB), 0); // 蓝色边框
    lv_obj_set_style_radius(g_scroll_area, 8, 0);
    lv_obj_set_style_pad_all(g_scroll_area, 5, 0);
    lv_obj_set_style_bg_color(g_scroll_area, lv_color_hex(0xF8F9FA), 0); // 浅灰色背景

    // 创建文本区域
    g_text_area = lv_textarea_create(g_scroll_area);
    lv_obj_set_size(g_text_area, 290, 190);
    lv_obj_align(g_text_area, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(g_text_area, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_text_area, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_color(g_text_area, lv_color_hex(0xFFFFFF), 0);
    lv_textarea_set_placeholder_text(g_text_area, "Waiting for data...");
    lv_textarea_set_text(g_text_area, "");

    // 设置文本区域为只读
    lv_textarea_set_password_mode(g_text_area, false);
    lv_textarea_set_one_line(g_text_area, false);
    lv_textarea_set_cursor_pos(g_text_area, 0);

    // 添加滚动事件
    lv_obj_add_event_cb(g_text_area, scroll_event_cb, LV_EVENT_SCROLL, NULL);

    // 创建按钮容器 - 在底部
    lv_obj_t* btn_cont = lv_obj_create(parent);
    lv_obj_set_size(btn_cont, 300, 50);
    lv_obj_align(btn_cont, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_style_pad_all(btn_cont, 0, 0);

    // 创建返回按钮 - 使用统一的back按钮函数
    g_back_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(g_back_btn, 60, 30);
    lv_obj_align(g_back_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(g_back_btn, back_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // 设置返回按钮样式
    lv_obj_set_style_bg_color(g_back_btn, lv_color_hex(0xE74C3C), 0); // 红色
    lv_obj_set_style_bg_opa(g_back_btn, LV_OPA_80, 0);
    lv_obj_set_style_radius(g_back_btn, 6, 0);
    lv_obj_set_style_shadow_width(g_back_btn, 2, 0);
    lv_obj_set_style_shadow_ofs_y(g_back_btn, 1, 0);
    lv_obj_set_style_shadow_opa(g_back_btn, LV_OPA_30, 0);

    lv_obj_t* back_label = lv_label_create(g_back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);

    // 创建清空按钮
    g_clear_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(g_clear_btn, 100, 40);
    lv_obj_align(g_clear_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(g_clear_btn, clear_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // 设置清空按钮样式
    lv_obj_set_style_bg_color(g_clear_btn, lv_color_hex(0xF39C12), 0); // 橙色
    lv_obj_set_style_bg_opa(g_clear_btn, LV_OPA_80, 0);
    lv_obj_set_style_radius(g_clear_btn, 8, 0);
    lv_obj_set_style_shadow_width(g_clear_btn, 3, 0);
    lv_obj_set_style_shadow_ofs_y(g_clear_btn, 1, 0);

    lv_obj_t* clear_label = lv_label_create(g_clear_btn);
    lv_label_set_text(clear_label, "Clear");
    lv_obj_set_style_text_color(clear_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(clear_label);

    // 初始化数据
    clear_display();

    // 创建消息队列
    g_display_queue = xQueueCreate(50, sizeof(display_msg_t));
    if (!g_display_queue) {
        ESP_LOGE(TAG, "Failed to create display queue");
        return;
    }

    // 启动显示任务
    g_display_running = true;
    if (xTaskCreate(display_task, "display_task", 4096, NULL, 5, &g_display_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create display task");
        g_display_running = false;
        vQueueDelete(g_display_queue);
        g_display_queue = NULL;
        return;
    }

    ESP_LOGI(TAG, "Serial display UI created successfully");
}

// 销毁串口显示界面
void ui_serial_display_destroy(void) {
    // 停止显示任务
    g_display_running = false;
    if (g_display_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(100));
        g_display_task_handle = NULL;
    }

    // 删除消息队列
    if (g_display_queue) {
        vQueueDelete(g_display_queue);
        g_display_queue = NULL;
    }

    // 清理PSRAM缓冲区
    cleanup_psram_buffer();

    // 清空全局变量
    g_serial_display_screen = NULL;
    g_text_area = NULL;
    g_status_label = NULL;
    g_back_btn = NULL;
    g_clear_btn = NULL;
    g_scroll_area = NULL;

    ESP_LOGI(TAG, "Serial display UI destroyed");
}
