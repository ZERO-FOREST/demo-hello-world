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
    lv_obj_set_style_transform_angle(var, v, 0);
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
    uint32_t elapsed = lv_timer_get_idle();

    if (elapsed < 1500) {
        lv_label_set_text(label, "Booting...");
    } else if (elapsed < 3000) {
        lv_label_set_text(label, "Loading Modules...");
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
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);

    // 1. 创建Logo (这里用一个标签代替)
    lv_obj_t *logo = lv_label_create(parent);
    lv_label_set_text(logo, "MY-LOGO");
    lv_obj_set_style_text_font(logo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(logo, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, -20);

    // 2. 创建旋转光环 (使用Arc)
    lv_obj_t *arc = lv_arc_create(parent);
    g_anim_arc = arc; // 保存arc指针以便后续清理
    lv_obj_set_size(arc, 150, 150);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align_to(arc, logo, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x00BFFF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 5, LV_PART_INDICATOR);


    // 3. 创建进度条
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, 200, 10);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 50);

    // 4. 创建状态文本
    lv_obj_t *status_label = lv_label_create(parent);
    lv_label_set_text(status_label, "Booting...");
    lv_obj_align_to(status_label, bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    // --- 定义动画 ---
    lv_anim_t a;

    // Logo淡入动画
    lv_anim_init(&a);
    lv_anim_set_var(&a, logo);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_exec_cb(&a, anim_logo_fade_in_cb);
    lv_anim_start(&a);
    
    // --- 光环动画 ---
    // 动画1: 旋转
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_values(&a, 0, 3600); // 旋转360度 (LVGL角度单位是0.1度)
    lv_anim_set_time(&a, 2000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, anim_rotation_cb);
    lv_anim_start(&a);

    // 动画2: 缩放 (呼吸效果)
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_values(&a, 230, 282); // 缩放范围 (256是原大小)
    lv_anim_set_time(&a, 1000);
    lv_anim_set_playback_time(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, anim_zoom_cb);
    lv_anim_start(&a);


    // 进度条动画
    lv_anim_init(&a);
    lv_anim_set_var(&a, bar);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 4000); // 动画总时长
    lv_anim_set_exec_cb(&a, anim_bar_progress_cb);
    lv_anim_set_ready_cb(&a, all_anims_finished_cb); // 最后一个动画完成时调用总回调
    lv_anim_set_user_data(&a, parent);
    lv_anim_start(&a);

    // 状态文本更新定时器
    g_status_timer = lv_timer_create(anim_status_text_timer_cb, 500, status_label);
} 