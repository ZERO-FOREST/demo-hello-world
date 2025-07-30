/**
 * @file ui_start_animation.c
 * @brief 高级启动动画UI实现
 * @author Your Name
 * @date 2024
 */
#include "ui.h"

// 全局变量来存储回调函数和需要清理的UI元素
static ui_start_anim_finished_cb_t g_finished_cb = NULL;
static lv_obj_t *g_anim_arc = NULL;
static lv_timer_t *g_status_timer = NULL;

// 动画相关的静态函数
static void anim_logo_fade_in_cb(void *var, int32_t v) {
    lv_obj_set_style_opa(var, v, 0);
}

static void anim_rotation_cb(void *var, int32_t v)
{
    // 使用Arc自身的旋转，避免与CSS变换冲突
    lv_arc_set_rotation((lv_obj_t*)var, v / 10);  // v是0.1度单位，转换为度
}

static void anim_zoom_cb(void *var, int32_t v)
{
    lv_obj_set_style_transform_zoom(var, v, 0);
}

static void anim_bar_progress_cb(void *var, int32_t v) {
    lv_bar_set_value(var, v, LV_ANIM_OFF);
}

static void anim_status_text_timer_cb(lv_timer_t *timer) {
    lv_obj_t *label = (lv_obj_t *)timer->user_data;
    static uint32_t call_count = 0;
    call_count++;

    if (call_count <= 2) {
        lv_label_set_text(label, "Initializing System...");
    } else if (call_count <= 4) {
        lv_label_set_text(label, "Loading Components...");
    } else if (call_count <= 6) {
        lv_label_set_text(label, "Starting Services...");
    } else if (call_count <= 8) {
        lv_label_set_text(label, "Configuring Hardware...");
    } else if (call_count <= 10) {
        lv_label_set_text(label, "Almost Ready...");
    } else {
        lv_label_set_text(label, "Finalizing...");
    }
}

static void all_anims_finished_cb(lv_anim_t *a) {
    // 关键修复：在删除对象前，先停止所有关联的动画和定时器
    if (g_anim_arc) {
        lv_anim_del(g_anim_arc, NULL);
        g_anim_arc = NULL;
    }
    if (g_status_timer) {
        lv_timer_del(g_status_timer);
        g_status_timer = NULL;
    }

    // 清理界面
    lv_obj_t* screen = (lv_obj_t*) a->user_data;
    if(screen) {
        lv_obj_clean(screen);
        // 重置背景为浅色主题，修复深色问题
        lv_obj_set_style_bg_color(screen, lv_color_hex(0xF0F0F0), 0);
    }
    
    // 调用外部回调函数
    if (g_finished_cb) {
        g_finished_cb();
    }
}

void ui_start_animation_create(lv_obj_t *parent, ui_start_anim_finished_cb_t finished_cb) {
    g_finished_cb = finished_cb;
    // 每次创建时重置静态变量
    g_anim_arc = NULL;
    g_status_timer = NULL;
    
    // 设置优化的渐变背景 - 使用RGB565友好的颜色
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x1B1B3A), 0);      // 深蓝紫
    lv_obj_set_style_bg_grad_color(parent, lv_color_hex(0x0E0E1F), 0);  // 更深蓝紫
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);
    
    // 启用渐变抖动，消除色带
    lv_obj_set_style_bg_dither_mode(parent, LV_DITHER_ORDERED, 0);

    // 1. 创建更炫酷的Logo
    lv_obj_t *logo = lv_label_create(parent);
    lv_label_set_text(logo, "ESP32-S3");
    lv_obj_set_style_text_font(logo, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(logo, lv_color_hex(0x00D4AA), 0);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, -40);
    
    // 副标题
    lv_obj_t *subtitle = lv_label_create(parent);
    lv_label_set_text(subtitle, "DEMO SYSTEM");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x16C79A), 0);
    lv_obj_align_to(subtitle, logo, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    // 2. 创建稳定的旋转光环
    lv_obj_t *arc = lv_arc_create(parent);
    g_anim_arc = arc; // 保存arc指针以便后续清理
    lv_obj_set_size(arc, 160, 160);
    lv_arc_set_rotation(arc, 0); // 设为0，通过动画旋转
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_angles(arc, 0, 90); // 设置较短的弧长，避免闪烁
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align_to(arc, logo, LV_ALIGN_CENTER, 0, 10);
    
    // 外层光环样式 - 青色渐变
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x333366), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x00D4AA), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    
    // 内层光环 - 简化设计避免闪烁
    lv_obj_t *arc2 = lv_arc_create(parent);
    lv_obj_set_size(arc2, 130, 130);
    lv_arc_set_rotation(arc2, 0); // 设为0，通过动画旋转
    lv_arc_set_bg_angles(arc2, 0, 360);
    lv_arc_set_angles(arc2, 0, 60); // 更短的弧长
    lv_obj_remove_style(arc2, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align_to(arc2, logo, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_arc_color(arc2, lv_color_hex(0x222244), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc2, lv_color_hex(0x16C79A), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc2, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc2, 4, LV_PART_INDICATOR);

    // 3. 创建炫酷进度条
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, 220, 8);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 70);
    
    // 进度条样式 - 优化渐变效果
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x2A2A3E), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x00C896), LV_PART_INDICATOR);          // 青绿色
    lv_obj_set_style_bg_grad_color(bar, lv_color_hex(0x00E0B4), LV_PART_INDICATOR);    // 浅青绿
    lv_obj_set_style_bg_grad_dir(bar, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);
    
    // 进度条也启用抖动
    lv_obj_set_style_bg_dither_mode(bar, LV_DITHER_ORDERED, LV_PART_INDICATOR);

    // 4. 创建状态文本
    lv_obj_t *status_label = lv_label_create(parent);
    lv_label_set_text(status_label, "Initializing...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x8A9BA8), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_align_to(status_label, bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
    
    // 5. 创建版本信息
    lv_obj_t *version_label = lv_label_create(parent);
    lv_label_set_text(version_label, "v1.0.0");
    lv_obj_set_style_text_color(version_label, lv_color_hex(0x5A6C7A), 0);
    lv_obj_align(version_label, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    // --- 定义动画 ---
    lv_anim_t a;

    // Logo淡入动画（延长时间）
    lv_anim_init(&a);
    lv_anim_set_var(&a, logo);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, 1500); // 延长到1.5秒
    lv_anim_set_delay(&a, 200);
    lv_anim_set_exec_cb(&a, anim_logo_fade_in_cb);
    lv_anim_start(&a);
    
    // 副标题淡入动画
    lv_anim_init(&a);
    lv_anim_set_var(&a, subtitle);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, 1200);
    lv_anim_set_delay(&a, 800);
    lv_anim_set_exec_cb(&a, anim_logo_fade_in_cb);
    lv_anim_start(&a);
    
    // --- 外层光环动画（平滑旋转）---
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_values(&a, 0, 3600); // 旋转360度 (LVGL角度单位是0.1度)
    lv_anim_set_time(&a, 5000); // 5秒一圈，更平滑
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_linear); // 线性动画，避免加速减速
    lv_anim_set_exec_cb(&a, anim_rotation_cb);
    lv_anim_start(&a);

    // --- 内层光环动画（反向旋转）---
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc2);
    lv_anim_set_values(&a, 3600, 0); // 反向旋转
    lv_anim_set_time(&a, 4000); // 4秒一圈
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_linear); // 线性动画
    lv_anim_set_exec_cb(&a, anim_rotation_cb);
    lv_anim_start(&a);

    // 进度条动画（延长时间）
    lv_anim_init(&a);
    lv_anim_set_var(&a, bar);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 7000); // 延长到7秒
    lv_anim_set_delay(&a, 1000); // 延迟1秒开始
    lv_anim_set_exec_cb(&a, anim_bar_progress_cb);
    lv_anim_set_ready_cb(&a, all_anims_finished_cb); // 最后一个动画完成时调用总回调
    lv_anim_set_user_data(&a, parent);
    lv_anim_start(&a);

    // 状态文本更新定时器（更新更频繁的状态）
    g_status_timer = lv_timer_create(anim_status_text_timer_cb, 800, status_label);
} 