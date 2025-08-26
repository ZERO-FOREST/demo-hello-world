#include "Mysybmol.h" // 包含字体头文件
#include "lvgl.h"
#include "telemetry_main.h" // 添加遥测服务头文件
#include "theme_manager.h"
#include "ui.h"
#include "ui_common.h"

// 遥测界面的全局变量
static lv_obj_t* throttle_slider;
static lv_obj_t* direction_slider;
static lv_obj_t* voltage_label;
static lv_obj_t* current_label;
static lv_obj_t* roll_label;  // 新增：横滚角标签
static lv_obj_t* pitch_label; // 新增：俯仰角标签
static lv_obj_t* yaw_label;   // 新增：偏航角标签
static lv_obj_t* altitude_label;
static lv_obj_t* service_status_label; // 添加服务状态标签
static lv_obj_t* start_stop_btn;       // 添加启动/停止按钮

// 服务状态标志
static bool telemetry_service_active = false;
static lv_obj_t* gps_label;

// 事件处理函数声明
static void slider_event_handler(lv_event_t* e);
static void settings_btn_event_handler(lv_event_t* e);
static void start_stop_btn_event_handler(lv_event_t* e);                  // 添加启动/停止按钮事件处理
static void telemetry_data_update_callback(const telemetry_data_t* data); // 添加数据更新回调

void ui_telemetry_create(lv_obj_t* parent) {
    theme_apply_to_screen(parent);

    // 初始化遥测服务
    if (telemetry_service_init() != 0) {
        LV_LOG_ERROR("Failed to initialize telemetry service");
    }

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

    // 将右上角的设置按钮改为启动/停止按钮
    if (settings_btn) {
        start_stop_btn = settings_btn; // 直接使用创建的按钮

        // 清除原有的事件回调
        lv_obj_remove_event_cb(start_stop_btn, NULL);

        // 创建按钮标签
        lv_obj_t* btn_label = lv_label_create(start_stop_btn);
        lv_label_set_text(btn_label, "启动");
        lv_obj_set_style_text_font(btn_label, font_cn, 0);
        lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
        lv_obj_center(btn_label);

        // 设置按钮样式为绿色
        lv_obj_set_style_bg_color(start_stop_btn, lv_color_hex(0x00AA00), 0);

        // 添加启动/停止事件回调
        lv_obj_add_event_cb(start_stop_btn, start_stop_btn_event_handler, LV_EVENT_CLICKED, NULL);
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
    lv_obj_t* throttle_label = lv_label_create(left_container);
    lv_label_set_text(throttle_label, "油门:");
    lv_obj_set_style_text_font(throttle_label, font_cn, 0);

    throttle_slider = lv_slider_create(left_container);
    lv_obj_set_size(throttle_slider, lv_pct(100), 7);
    lv_obj_set_style_pad_all(throttle_slider, 2, LV_PART_KNOB);
    lv_slider_set_range(throttle_slider, 0, 1000);
    lv_slider_set_value(throttle_slider, 500, LV_ANIM_OFF);
    lv_obj_add_event_cb(throttle_slider, slider_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    // 方向控制
    lv_obj_t* direction_label = lv_label_create(left_container);
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
    lv_obj_t* title2 = lv_label_create(right_container);
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
    lv_obj_set_flex_flow(panel2, LV_FLEX_FLOW_COLUMN); // 设置为垂直布局
    lv_obj_set_style_pad_all(panel2, 10, 0);           // 添加一些内边距
    lv_obj_set_style_pad_gap(panel2, 8, 0);            // 添加一些间距

    // 在 panel2 中创建一个水平布局容器用于姿态角
    lv_obj_t* attitude_row = lv_obj_create(panel2);
    lv_obj_set_width(attitude_row, lv_pct(100));
    lv_obj_set_height(attitude_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(attitude_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(attitude_row, 0, 0);
    lv_obj_set_style_pad_gap(attitude_row, 10, 0);

    // Roll
    roll_label = lv_label_create(attitude_row);
    lv_label_set_text(roll_label, "R: --");
    lv_obj_set_style_text_font(roll_label, font_cn, 0);

    // Pitch
    pitch_label = lv_label_create(attitude_row);
    lv_label_set_text(pitch_label, "P: --");
    lv_obj_set_style_text_font(pitch_label, font_cn, 0);

    // Yaw
    yaw_label = lv_label_create(attitude_row);
    lv_label_set_text(yaw_label, "Y: --");
    lv_obj_set_style_text_font(yaw_label, font_cn, 0);

    // GPS状态显示
    gps_label = lv_label_create(panel2);
    lv_label_set_text(gps_label, "GPS: 未连接");
    lv_obj_set_style_text_font(gps_label, font_cn, 0);

    // 高度显示
    altitude_label = lv_label_create(panel2);
    lv_label_set_text(altitude_label, "高度: -- m");
    lv_obj_set_style_text_font(altitude_label, font_cn, 0);

    // 扩展功能区域
    lv_obj_t* panel3 = lv_obj_create(content_container);
    lv_obj_set_width(panel3, lv_pct(100));
    lv_obj_set_height(panel3, LV_SIZE_CONTENT);
    lv_obj_t* title4 = lv_label_create(panel3);
    lv_label_set_text(title4, "扩展功能");
    lv_obj_set_style_text_font(title4, font_cn, 0); // 设置字体
}

static void settings_btn_event_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // 处理设置按钮点击事件
        LV_LOG_USER("Settings button clicked");
    }
}

static void slider_event_handler(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);

    if (slider == throttle_slider) {
        LV_LOG_USER("Throttle slider value: %d", (int)value);
        // 发送控制命令到遥测服务
        if (telemetry_service_active) {
            int32_t direction_value = lv_slider_get_value(direction_slider);
            telemetry_service_send_control(value, direction_value);
        }
    } else if (slider == direction_slider) {
        LV_LOG_USER("Direction slider value: %d", (int)value);
        // 发送控制命令到遥测服务
        if (telemetry_service_active) {
            int32_t throttle_value = lv_slider_get_value(throttle_slider);
            telemetry_service_send_control(throttle_value, value);
        }
    }
}

static void start_stop_btn_event_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        if (!telemetry_service_active) {
            // 启动遥测服务
            if (telemetry_service_start(telemetry_data_update_callback) == 0) {
                telemetry_service_active = true;

                // 更新按钮文本为"停止"
                lv_obj_t* btn_label = lv_obj_get_child(start_stop_btn, 0);
                if (btn_label) {
                    lv_label_set_text(btn_label, "停止");
                }

                // 更改按钮颜色为红色
                lv_obj_set_style_bg_color(start_stop_btn, lv_color_hex(0xAA0000), 0);

                // 更新状态显示（如果存在）
                if (service_status_label) {
                    lv_label_set_text(service_status_label, "状态: 运行中");
                }

                LV_LOG_USER("Telemetry service started");
            } else {
                LV_LOG_ERROR("Failed to start telemetry service");
                if (service_status_label) {
                    lv_label_set_text(service_status_label, "状态: 启动失败");
                }
            }
        } else {
            // 停止遥测服务 - 添加防护措施防止重复调用
            if (telemetry_service_active && telemetry_service_stop() == 0) {
                telemetry_service_active = false;

                // 更新按钮文本为"启动"
                lv_obj_t* btn_label = lv_obj_get_child(start_stop_btn, 0);
                if (btn_label) {
                    lv_label_set_text(btn_label, "启动");
                }

                // 更改按钮颜色为绿色
                lv_obj_set_style_bg_color(start_stop_btn, lv_color_hex(0x00AA00), 0);

                // 更新状态显示（如果存在）
                if (service_status_label) {
                    lv_label_set_text(service_status_label, "状态: 已停止");
                }

                // 清空数据显示
                if (voltage_label) {
                    lv_label_set_text(voltage_label, "电压: -- V");
                }
                if (current_label) {
                    lv_label_set_text(current_label, "电流: -- A");
                }
                if (altitude_label) {
                    lv_label_set_text(altitude_label, "高度: -- m");
                }
                if (roll_label) {
                    lv_label_set_text(roll_label, "R: --");
                }
                if (pitch_label) {
                    lv_label_set_text(pitch_label, "P: --");
                }
                if (yaw_label) {
                    lv_label_set_text(yaw_label, "Y: --");
                }
                if (gps_label) {
                    lv_label_set_text(gps_label, "GPS: 未连接");
                }

                LV_LOG_USER("Telemetry service stopped");
            } else {
                // 停止失败或服务已经停止
                if (!telemetry_service_active) {
                    LV_LOG_WARN("Telemetry service already stopped");
                } else {
                    LV_LOG_ERROR("Failed to stop telemetry service");
                }
            }
        }
    }
}

static void telemetry_data_update_callback(const telemetry_data_t* data) {
    if (data == NULL)
        return;

    // 检查关键对象是否有效，如果不是则跳过更新
    if (!voltage_label || !current_label || !altitude_label || !roll_label || !pitch_label || !yaw_label) {
        return;
    }

    // 更新UI显示的遥测数据
    // 为避免可变参数函数中的浮点数问题，手动转换为整数进行打印
    if (lv_obj_is_valid(voltage_label)) {
        int voltage_int = (int)(data->voltage);
        int voltage_frac = (int)((data->voltage - voltage_int) * 100);
        lv_label_set_text_fmt(voltage_label, "电压: %d.%02d V", voltage_int, voltage_frac);
    }
    if (lv_obj_is_valid(current_label)) {
        int current_int = (int)(data->current);
        int current_frac = (int)((data->current - current_int) * 100);
        lv_label_set_text_fmt(current_label, "电流: %d.%02d A", current_int, current_frac);
    }
    if (lv_obj_is_valid(altitude_label)) {
        int alt_int = (int)(data->altitude);
        int alt_frac = (int)((data->altitude - alt_int) * 10);
        lv_label_set_text_fmt(altitude_label, "高度: %d.%d m", alt_int, alt_frac);
    }
    // 更新姿态角
    if (lv_obj_is_valid(roll_label)) {
        int roll_int = (int)(data->roll);
        int roll_frac = (int)((data->roll - roll_int) * 100);
        lv_label_set_text_fmt(roll_label, "R: %d.%02d", roll_int, roll_frac);
    }
    if (lv_obj_is_valid(pitch_label)) {
        int pitch_int = (int)(data->pitch);
        int pitch_frac = (int)((data->pitch - pitch_int) * 100);
        lv_label_set_text_fmt(pitch_label, "P: %d.%02d", pitch_int, pitch_frac);
    }
    if (lv_obj_is_valid(yaw_label)) {
        int yaw_int = (int)(data->yaw);
        int yaw_frac = (int)((data->yaw - yaw_int) * 100);
        lv_label_set_text_fmt(yaw_label, "Y: %d.%02d", yaw_int, yaw_frac);
    }
    if (gps_label && lv_obj_is_valid(gps_label)) {
        // 简单的GPS状态模拟
        if (data->altitude > 0) {
            lv_label_set_text(gps_label, "GPS: 已连接");
        } else {
            lv_label_set_text(gps_label, "GPS: 搜索中");
        }
    }
}

// 后续将添加更新遥测数据的函数
void ui_telemetry_update_data(float voltage, float current, float roll, float pitch, float yaw, float altitude) {
    lv_label_set_text_fmt(voltage_label, "电压: %.2f V", voltage);
    lv_label_set_text_fmt(current_label, "电流: %.2f A", current);
    // 更新姿态仪和高度等
}

// 添加UI清理函数
void ui_telemetry_cleanup(void) {
    // 停止遥测服务
    if (telemetry_service_active) {
        telemetry_service_stop();
        telemetry_service_active = false;
    }

    // 反初始化遥测服务
    telemetry_service_deinit();

    LV_LOG_USER("Telemetry UI cleanup completed");
}
