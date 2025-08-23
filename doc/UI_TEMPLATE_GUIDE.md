# UI界面创建模板文档

## 概述

本文档描述了如何使用统一的UI组件创建页面界面。所有页面都遵循相同的布局结构，确保界面的一致性和可维护性。

## 页面结构

每个页面都包含以下三个主要容器：

```
┌─────────────────────────────────────┐
│ 页面父级容器 (240x320) - 不可滚动     │
├─────────────────────────────────────┤
│ 顶部栏容器 (240x30) - 主题表面色      │
│ ┌─────┐ ┌────────────────┐ ┌─────┐  │
│ │返回 │ │    标题居中     │ │设置 │   |
│ │按钮 │ │    (字体20)     | │按钮 │  │
│ └─────┘ └─────── ────────┘ └─────┘  │
├─────────────────────────────────────┤
│                                     │
│    页面内容容器 (240x290)            │
│    主题背景色，可滚动                │
│                                     │
│    子页面的所有内容都在这里          │
│                                     │
└─────────────────────────────────────┘
```

## 创建步骤

### 1. 创建页面父级容器

```c
// 创建页面父级容器（统一管理整个页面）
lv_obj_t* page_parent_container;
ui_create_page_parent_container(parent, &page_parent_container);
```

**功能说明：**
- 创建240x320的父级容器
- 占满整个屏幕
- 不可滚动，用于统一管理页面

### 2. 创建顶部栏容器

```c
// 创建顶部栏容器（包含返回按钮和标题）
lv_obj_t* top_bar_container;
lv_obj_t* title_container;
lv_obj_t* settings_btn = NULL; // 用于接收按钮指针
// 第三个参数决定是否显示设置按钮, 最后一个参数接收按钮指针
ui_create_top_bar(page_parent_container, "页面标题", true, &top_bar_container, &title_container, &settings_btn);

// 如果需要，可以为设置按钮添加自定义回调
if (settings_btn) {
    // lv_obj_add_event_cb(settings_btn, your_custom_settings_cb, LV_EVENT_CLICKED, NULL);
}

// 3. 创建页面内容容器
lv_obj_t* content_container;
ui_create_page_content_area(page_parent_container, &content_container);
```

**功能说明：**
- 创建240x290的内容容器
- 从顶部栏下方开始
- 可滚动，支持垂直滚动
- 有主题背景色和边框
- 无内边距，避免横向滚动

## 完整模板示例

```c
void ui_your_page_create(lv_obj_t* parent) {
    // 应用当前主题到屏幕
    theme_apply_to_screen(parent);

    // 1. 创建页面父级容器（统一管理整个页面）
    lv_obj_t* page_parent_container;
    ui_create_page_parent_container(parent, &page_parent_container);

    // 2. 创建顶部栏容器（包含返回按钮和标题）
    lv_obj_t* top_bar_container;
    lv_obj_t* title_container;
    lv_obj_t* settings_btn = NULL; // 用于接收按钮指针
    // 第三个参数决定是否显示设置按钮, 最后一个参数接收按钮指针
    ui_create_top_bar(page_parent_container, "页面标题", true, &top_bar_container, &title_container, &settings_btn);

    // 如果需要，可以为设置按钮添加自定义回调
    if (settings_btn) {
        // lv_obj_add_event_cb(settings_btn, your_custom_settings_cb, LV_EVENT_CLICKED, NULL);
    }

    // 3. 创建页面内容容器
    lv_obj_t* content_container;
    ui_create_page_content_area(page_parent_container, &content_container);

    // 4. 在content_container中添加页面内容
    // 示例：创建标签
    lv_obj_t* label = lv_label_create(content_container);
    lv_label_set_text(label, "这是页面内容");
    theme_apply_to_label(label, false);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 10, 10);

    // 示例：创建按钮
    lv_obj_t* btn = lv_btn_create(content_container);
    lv_obj_set_size(btn, 200, 40);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    theme_apply_to_button(btn, true);

    lv_obj_t* btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "按钮");
    lv_obj_center(btn_label);
}
```

## 主题系统

### 主题应用函数

```c
// 应用到屏幕
theme_apply_to_screen(parent);

// 应用到标签
theme_apply_to_label(label, false);  // false表示不强调

// 应用到按钮
theme_apply_to_button(btn, true);    // true表示主要按钮

// 应用到开关
theme_apply_to_switch(switch_obj);

// 应用到容器
theme_apply_to_container(container);
```

### 获取主题颜色

```c
// 获取当前主题
const theme_t* current_theme = theme_get_current_theme();

// 获取主题颜色
lv_color_t color = theme_get_color(current_theme->colors.text_primary);
lv_color_t bg_color = theme_get_color(current_theme->colors.background);
lv_color_t border_color = theme_get_color(current_theme->colors.border);
```

### 可用主题

- `THEME_MORANDI` - 莫兰迪主题
- `THEME_DARK` - 深色主题
- `THEME_LIGHT` - 浅色主题
- `THEME_BLUE` - 蓝色主题
- `THEME_GREEN` - 绿色主题

## 组件对齐和定位

### 对齐方式

```c
// 绝对对齐
lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 10, 10);
lv_obj_align(obj, LV_ALIGN_CENTER, 0, 0);
lv_obj_align(obj, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

// 相对对齐
lv_obj_align_to(obj, reference_obj, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
lv_obj_align_to(obj, reference_obj, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
```

### 尺寸设置

```c
// 设置尺寸
lv_obj_set_size(obj, width, height);

// 设置位置
lv_obj_set_pos(obj, x, y);
```

## 事件处理

### 返回按钮

顶部栏的返回按钮会自动绑定返回主菜单的回调，无需手动处理。

### 自定义事件

```c
// 添加点击事件
lv_obj_add_event_cb(btn, your_callback_function, LV_EVENT_CLICKED, NULL);

// 回调函数示例
static void your_callback_function(lv_event_t* e) {
    lv_obj_t* target = lv_event_get_target(e);
    // 处理事件
}
```

## 常用组件示例

### 标签 (Label)

```c
lv_obj_t* label = lv_label_create(content_container);
lv_label_set_text(label, "文本内容");
theme_apply_to_label(label, false);
lv_obj_align(label, LV_ALIGN_TOP_LEFT, 10, 10);
```

### 按钮 (Button)

```c
lv_obj_t* btn = lv_btn_create(content_container);
lv_obj_set_size(btn, 200, 40);
lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
theme_apply_to_button(btn, true);

lv_obj_t* btn_label = lv_label_create(btn);
lv_label_set_text(btn_label, "按钮文本");
lv_obj_center(btn_label);
```

### 开关 (Switch)

```c
lv_obj_t* sw = lv_switch_create(content_container);
lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, -10, 10);
theme_apply_to_switch(sw);
lv_obj_add_state(sw, LV_STATE_CHECKED); // 设置开关状态
```

### 下拉列表 (Dropdown)

```c
lv_obj_t* dropdown = lv_dropdown_create(content_container);
lv_obj_set_size(dropdown, 200, 35);
lv_obj_align(dropdown, LV_ALIGN_CENTER, 0, 0);
lv_dropdown_set_options(dropdown, "选项1\n选项2\n选项3");
lv_dropdown_set_selected(dropdown, 0);
```

### 输入框 (Text Area)

```c
lv_obj_t* ta = lv_textarea_create(content_container);
lv_obj_set_size(ta, 200, 60);
lv_obj_align(ta, LV_ALIGN_CENTER, 0, 0);
lv_textarea_set_placeholder_text(ta, "请输入文本");
```

## 注意事项

1. **容器层级**：严格按照父级容器 → 顶部栏/内容容器 → 页面内容的层级创建
2. **滚动控制**：只有内容容器可滚动，父级容器不可滚动
3. **尺寸限制**：内容容器的宽度是240像素，注意组件不要超出
4. **主题一致性**：使用主题函数确保界面风格统一
5. **内存管理**：LVGL会自动管理对象内存，无需手动释放
6. **横向滚动**：避免在内容容器中设置内边距，防止横向滚动

## 文件依赖

```c
#include "ui.h"                    // UI组件函数
#include "theme_manager.h"         // 主题管理
#include "lvgl.h"                  // LVGL核心
```

## 函数声明

在 `ui.h` 中需要声明的函数：

```c
// 创建页面父级容器（统一管理整个页面）
void ui_create_page_parent_container(lv_obj_t* parent, lv_obj_t** page_parent_container);

// 创建顶部栏容器（包含返回按钮和标题）
void ui_create_top_bar(lv_obj_t* parent, const char* title_text, bool show_settings_btn, lv_obj_t** top_bar_container,
                       lv_obj_t** title_container, lv_obj_t** settings_btn_out);

// 创建页面内容容器（除开顶部栏的区域）
void ui_create_page_content_area(lv_obj_t* parent, lv_obj_t** content_container);
```