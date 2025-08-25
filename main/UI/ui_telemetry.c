#include "ui.h"
#include "ui_common.h"
#include "lvgl.h"
#include "theme_manager.h"
#include "Mysybmol.h" // 包含字体头文件

// 遥测界面的全局变量
static lv_obj_t *throttle_slider;
static lv_obj_t *direction_slider;
static lv_obj_t *voltage_label;
static lv_obj_t *current_label;
static lv_obj_t *attitude_meter;
static lv_obj_t *altitude_label;
static lv_obj_t *gps_label;

// 事件处理函数声明
static void slider_event_handler(lv_event_t *e);
static void settings_btn_event_handler(lv_event_t *e);

void ui_telemetry_create(lv_obj_t* parent)
{
    theme_apply_to_screen(parent);
    
    // 获取中文字体
    lv_font_t* font_cn = get_loaded_font();
    if (!font_cn) {
        LV_LOG_ERROR("Chinese font not loaded!");
        return;
    }

    // 1. 创建顶部栏
    lv_obj_t* top_bar = NULL;
    lv_obj_t* title_container = NULL;
    lv_obj_t* settings_btn = NULL; 
    ui_create_top_bar(parent, "遥控器", true, &top_bar, &title_container, &settings_btn);
    
    // 为标题设置中文字体
    if (title_container) {
        lv_obj_t* title = lv_obj_get_child(title_container, 0);
        if (title) {
            lv_obj_set_style_text_font(title, font_cn, 0);
        }
    }

    if (settings_btn) {
        lv_obj_add_event_cb(settings_btn, settings_btn_event_handler, LV_EVENT_CLICKED, NULL);
    }

    // 2. 创建内容容器
    lv_obj_t* content_container;
    ui_create_page_content_area(parent, &content_container);

    // 设置内容容器的布局，使其子控件垂直排列
    lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content_container, 5, 0);
    lv_obj_set_style_pad_gap(content_container, 10, 0);


    // 3. 在内容容器中创建控件
    // 油门/方向 和 遥测状态 区域 - 使用水平布局
    lv_obj_t* control_row = lv_obj_create(content_container);
    lv_obj_set_width(control_row, lv_pct(100));
    lv_obj_set_height(control_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(control_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(control_row, 5, 0);
    lv_obj_set_style_pad_gap(control_row, 10, 0);

    // 左侧容器：油门/方向
    lv_obj_t* left_container = lv_obj_create(control_row);
    lv_obj_set_width(left_container, lv_pct(48));
    lv_obj_set_height(left_container, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(left_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(left_container, 10, 0);
    lv_obj_set_style_pad_gap(left_container, 8, 0);

    // 油门控制
    lv_obj_t *throttle_label = lv_label_create(left_container);
    lv_label_set_text(throttle_label, "油门:");
    lv_obj_set_style_text_font(throttle_label, font_cn, 0);

    throttle_slider = lv_slider_create(left_container);
    lv_obj_set_size(throttle_slider, lv_pct(100), 7);
    lv_obj_set_style_pad_all(throttle_slider, 2, LV_PART_KNOB);
    lv_slider_set_range(throttle_slider, 0, 1000);
    lv_slider_set_value(throttle_slider, 500, LV_ANIM_OFF);
    lv_obj_add_event_cb(throttle_slider, slider_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    
    // 方向控制
    lv_obj_t *direction_label = lv_label_create(left_container);
    lv_label_set_text(direction_label, "方向:");
    lv_obj_set_style_text_font(direction_label, font_cn, 0);

    direction_slider = lv_slider_create(left_container);
    lv_obj_set_size(direction_slider, lv_pct(100), 7);
    lv_obj_set_style_pad_all(direction_slider, 2, LV_PART_KNOB);
    lv_slider_set_range(direction_slider, 0, 1000);
    lv_slider_set_value(direction_slider, 500, LV_ANIM_OFF);
    lv_obj_add_event_cb(direction_slider, slider_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    // 右侧容器：遥测状态
    lv_obj_t* right_container = lv_obj_create(control_row);
    lv_obj_set_width(right_container, lv_pct(48));
    lv_obj_set_height(right_container, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(right_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(right_container, 10, 0);
    lv_obj_set_style_pad_gap(right_container, 8, 0);

    // 右侧标题
    lv_obj_t *title2 = lv_label_create(right_container);
    lv_label_set_text(title2, "遥测状态");
    lv_obj_set_style_text_font(title2, font_cn, 0);
    
    // 电压显示
    voltage_label = lv_label_create(right_container);
    lv_label_set_text(voltage_label, "电压: -- V");
    lv_obj_set_style_text_font(voltage_label, font_cn, 0);

    // 电流显示
    current_label = lv_label_create(right_container);
    lv_label_set_text(current_label, "电流: -- A");
    lv_obj_set_style_text_font(current_label, font_cn, 0);


    // 姿态显示区域
    lv_obj_t* panel2 = lv_obj_create(content_container);
    lv_obj_set_width(panel2, lv_pct(100));
    lv_obj_set_height(panel2, LV_SIZE_CONTENT);


    // 扩展功能区域
    lv_obj_t* panel3 = lv_obj_create(content_container);
    lv_obj_set_width(panel3, lv_pct(100));
    lv_obj_set_height(panel3, LV_SIZE_CONTENT);
    lv_obj_t *title4 = lv_label_create(panel3);
    lv_label_set_text(title4, "扩展功能");
    lv_obj_set_style_text_font(title4, font_cn, 0); // 设置字体
}

static void settings_btn_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // 处理设置按钮点击事件
        LV_LOG_USER("Settings button clicked");
    }
}


static void slider_event_handler(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    
    if(slider == throttle_slider)
    {
        LV_LOG_USER("Throttle slider value: %ld", value);
        // 在这里可以添加发送TCP命令的逻辑
    }
    else if(slider == direction_slider)
    {
        LV_LOG_USER("Direction slider value: %ld", value);
        // 在这里可以添加发送TCP命令的逻辑
    }
}

// 后续将添加更新遥测数据的函数
void ui_telemetry_update_data(float voltage, float current, float roll, float pitch, float yaw, float altitude)
{
    lv_label_set_text_fmt(voltage_label, "电压: %.2f V", voltage);
    lv_label_set_text_fmt(current_label, "电流: %.2f A", current);
    // 更新姿态仪和高度等
}
