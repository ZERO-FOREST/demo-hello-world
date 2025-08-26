/**
 * @file st7789_esp_lcd.c
 * @brief ST7789 TFT LCD Driver Implementation for ESP32-S3 using ESP-LCD component
 * @author Your Name
 * @date 2024
 */

#include "st7789_esp_lcd.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>


// ========================================
// 私有变量和常量
// ========================================
static const char* TAG = "ST7789_ESP_LCD";

// LCD面板句柄
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static bool is_initialized = false;

// ========================================
// 私有函数声明
// ========================================
static esp_err_t st7789_esp_lcd_gpio_init(void);
static esp_err_t st7789_esp_lcd_spi_init(void);
static void st7789_esp_lcd_hardware_reset(void);
static esp_err_t st7789_esp_lcd_init_sequence(void);

// ========================================
// ST7789初始化序列数据
// ========================================
static const uint8_t st7789_init_cmds[] = {
    // Software reset
    ST7789_CMD_SWRESET, 0,
    // Sleep out
    ST7789_CMD_SLPOUT, 0,
    // Color mode: 16-bit per pixel
    ST7789_CMD_COLMOD, 1, 0x55,
    // Memory access control
    ST7789_CMD_MADCTL, 1, 0x00,
    // Column address set - 240像素宽度
    ST7789_CMD_CASET, 4, 0x00, 0x00, 0x00, 0xEF,
    // Row address set - 320像素高度
    ST7789_CMD_RASET, 4, 0x00, 0x00, 0x01, 0x3F,
    // Porch control
    ST7789_CMD_PORCTRL, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33,
    // Gate control
    ST7789_CMD_GCTRL, 1, 0x35,
    // VCOM setting
    ST7789_CMD_VCOMS, 1, 0x32,
    // LCM control
    ST7789_CMD_LCMCTRL, 1, 0x2C,
    // VDV and VRH command enable
    ST7789_CMD_VDVVRHEN, 1, 0x01,
    // VRH set
    ST7789_CMD_VRHSET, 1, 0x15,
    // VDV set
    ST7789_CMD_VDVSET, 1, 0x20,
    // Power control 1
    ST7789_CMD_PWCTRL1, 2, 0xA4, 0xA1,
    // Positive gamma correction
    ST7789_CMD_GMCTRP1, 14, 0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23,
    // Negative gamma correction
    ST7789_CMD_GMCTRN1, 14, 0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23,
    // Normal display on
    ST7789_CMD_NORON, 0,
    // Display on
    ST7789_CMD_DISPON, 0,
    // End of commands
    0xFF, 0};

// ========================================
// GPIO初始化
// ========================================
static esp_err_t st7789_esp_lcd_gpio_init(void) {
    esp_err_t ret = ESP_OK;

    // 配置电源控制引脚
    gpio_config_t power_config = {
        .pin_bit_mask = (1ULL << ST7789_PIN_POWER),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&power_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Power GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置复位引脚
    gpio_config_t rst_config = {
        .pin_bit_mask = (1ULL << ST7789_PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&rst_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Reset GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置背光控制引脚
    gpio_config_t blk_config = {
        .pin_bit_mask = (1ULL << ST7789_PIN_BLK),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&blk_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Backlight GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 初始状态设置
    gpio_set_level(ST7789_PIN_POWER, 0); // 关闭电源
    gpio_set_level(ST7789_PIN_RST, 1);   // 复位引脚高电平
    gpio_set_level(ST7789_PIN_BLK, 0);   // 关闭背光

    ESP_LOGI(TAG, "GPIO initialized successfully");
    return ESP_OK;
}

// ========================================
// SPI初始化
// ========================================
static esp_err_t st7789_esp_lcd_spi_init(void) {
    esp_err_t ret = ESP_OK;

    // SPI总线配置 - 使用PSRAM
    spi_bus_config_t bus_config = {
        .mosi_io_num = ST7789_PIN_MOSI,
        .miso_io_num = -1, // ST7789不需要MISO
        .sclk_io_num = ST7789_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ST7789_WIDTH * 40 * 2, // 减小传输大小，避免内存不足
    };

    // 初始化SPI总线
    ret = spi_bus_initialize(ST7789_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // LCD面板IO配置 - 优化内存使用
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = ST7789_PIN_DC,
        .cs_gpio_num = ST7789_PIN_CS,
        .pclk_hz = ST7789_SPI_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 5, // 减小队列深度，节省内存
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
    };

    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)ST7789_SPI_HOST, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "New panel IO failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // LCD面板配置
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = ST7789_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = NULL,
    };

    ret = esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "New panel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPI and LCD panel initialized successfully");
    return ESP_OK;
}

// ========================================
// 硬件复位
// ========================================
static void st7789_esp_lcd_hardware_reset(void) {
    gpio_set_level(ST7789_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(ST7789_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Hardware reset completed");
}

// ========================================
// 初始化序列
// ========================================
static esp_err_t st7789_esp_lcd_init_sequence(void) {
    esp_err_t ret = ESP_OK;

    // 发送初始化命令序列
    const uint8_t* cmd = st7789_init_cmds;
    while (cmd[0] != 0xFF) {
        ret = esp_lcd_panel_io_tx_param(io_handle, cmd[0], &cmd[2], cmd[1]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Send command 0x%02X failed: %s", cmd[0], esp_err_to_name(ret));
            return ret;
        }
        cmd += (2 + cmd[1]);
    }

    ESP_LOGI(TAG, "Init sequence completed");
    return ESP_OK;
}

// ========================================
// 公共函数实现
// ========================================

esp_err_t st7789_esp_lcd_init(void) {
    esp_err_t ret = ESP_OK;

    if (is_initialized) {
        ESP_LOGW(TAG, "ST7789 already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing ST7789 with ESP-LCD component");

    // 初始化GPIO
    ret = st7789_esp_lcd_gpio_init();
    if (ret != ESP_OK) {
        return ret;
    }

    // 开启电源
    gpio_set_level(ST7789_PIN_POWER, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    // 初始化SPI和LCD面板
    ret = st7789_esp_lcd_spi_init();
    if (ret != ESP_OK) {
        return ret;
    }

    // 硬件复位
    st7789_esp_lcd_hardware_reset();

    // 发送初始化序列
    ret = st7789_esp_lcd_init_sequence();
    if (ret != ESP_OK) {
        return ret;
    }

    // 初始化面板
    ret = esp_lcd_panel_reset(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel reset failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_lcd_panel_init(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 设置显示方向 - 尝试不同的配置
    // 先尝试不交换坐标
    ret = esp_lcd_panel_swap_xy(panel_handle, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel swap xy failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 不进行镜像
    ret = esp_lcd_panel_mirror(panel_handle, false, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel mirror failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 开启显示
    ret = esp_lcd_panel_disp_on_off(panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel display on failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 开启背光
    gpio_set_level(ST7789_PIN_BLK, 1);

    is_initialized = true;
    ESP_LOGI(TAG, "ST7789 initialized successfully with ESP-LCD");

    return ESP_OK;
}

esp_err_t st7789_esp_lcd_deinit(void) {
    if (!is_initialized) {
        return ESP_OK;
    }

    // 关闭显示
    esp_lcd_panel_disp_on_off(panel_handle, false);

    // 关闭背光
    gpio_set_level(ST7789_PIN_BLK, 0);

    // 关闭电源
    gpio_set_level(ST7789_PIN_POWER, 0);

    // 删除面板和IO
    if (panel_handle) {
        esp_lcd_panel_del(panel_handle);
        panel_handle = NULL;
    }

    if (io_handle) {
        esp_lcd_panel_io_del(io_handle);
        io_handle = NULL;
    }

    // 释放SPI总线
    spi_bus_free(ST7789_SPI_HOST);

    is_initialized = false;
    ESP_LOGI(TAG, "ST7789 deinitialized");

    return ESP_OK;
}

esp_err_t st7789_esp_lcd_clear_screen(uint16_t color) {
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // 分配清屏缓冲区
    size_t buffer_size = ST7789_WIDTH * 40 * 2; // 40行缓冲区
    uint16_t* buffer = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate clear screen buffer");
        return ESP_ERR_NO_MEM;
    }

    // 填充缓冲区
    for (int i = 0; i < buffer_size / 2; i++) {
        buffer[i] = color;
    }

    // 分块清屏
    for (int y = 0; y < ST7789_HEIGHT; y += 40) {
        int height = (y + 40 > ST7789_HEIGHT) ? (ST7789_HEIGHT - y) : 40;
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, ST7789_WIDTH, height, buffer);
    }

    heap_caps_free(buffer);
    return ESP_OK;
}

esp_err_t st7789_esp_lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // ESP-LCD组件会自动处理窗口设置
    return ESP_OK;
}

esp_err_t st7789_esp_lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_lcd_panel_draw_bitmap(panel_handle, x, y, 1, 1, &color);
}

esp_err_t st7789_esp_lcd_draw_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // 分配矩形缓冲区
    size_t buffer_size = width * height * 2;
    uint16_t* buffer = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate rect buffer");
        return ESP_ERR_NO_MEM;
    }

    // 填充缓冲区
    for (int i = 0; i < width * height; i++) {
        buffer[i] = color;
    }

    esp_err_t ret = esp_lcd_panel_draw_bitmap(panel_handle, x, y, width, height, buffer);
    heap_caps_free(buffer);

    return ret;
}

esp_err_t st7789_esp_lcd_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    return st7789_esp_lcd_draw_rect(x, y, width, height, color);
}

esp_err_t st7789_esp_lcd_draw_image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t* data) {
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_lcd_panel_draw_bitmap(panel_handle, x, y, width, height, data);
}

esp_err_t st7789_esp_lcd_display_enable(bool enable) {
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (enable) {
        return esp_lcd_panel_disp_on_off(panel_handle, true);
    } else {
        return esp_lcd_panel_disp_on_off(panel_handle, false);
    }
}

esp_err_t st7789_esp_lcd_backlight_enable(bool enable) {
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    gpio_set_level(ST7789_PIN_BLK, enable ? 1 : 0);
    return ESP_OK;
}

esp_err_t st7789_esp_lcd_power_enable(bool enable) {
    gpio_set_level(ST7789_PIN_POWER, enable ? 1 : 0);
    return ESP_OK;
}

esp_err_t st7789_esp_lcd_set_rotation(int rotation) {
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;

    switch (rotation) {
    case 0:
        ret = esp_lcd_panel_swap_xy(panel_handle, false);
        if (ret == ESP_OK) {
            ret = esp_lcd_panel_mirror(panel_handle, false, false);
        }
        break;
    case 90:
        ret = esp_lcd_panel_swap_xy(panel_handle, true);
        if (ret == ESP_OK) {
            ret = esp_lcd_panel_mirror(panel_handle, false, true);
        }
        break;
    case 180:
        ret = esp_lcd_panel_swap_xy(panel_handle, false);
        if (ret == ESP_OK) {
            ret = esp_lcd_panel_mirror(panel_handle, true, true);
        }
        break;
    case 270:
        ret = esp_lcd_panel_swap_xy(panel_handle, true);
        if (ret == ESP_OK) {
            ret = esp_lcd_panel_mirror(panel_handle, true, false);
        }
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_lcd_panel_handle_t st7789_esp_lcd_get_panel_handle(void) { return panel_handle; }

esp_lcd_panel_io_handle_t st7789_esp_lcd_get_panel_io_handle(void) { return io_handle; }
