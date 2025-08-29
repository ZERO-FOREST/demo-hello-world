
#include "ft6336g.h"
#include "bsp_i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static const char* TAG = "FT6336G";

static i2c_master_dev_handle_t g_ft6336g_dev_handle = NULL;
static volatile bool g_touch_irq_flag = false;

// FT6336G 寄存器地址
#define FT6336G_REG_TD_STATUS 0x02
#define FT6336G_REG_P1_XH 0x03
#define FT6336G_REG_P1_XL 0x04
#define FT6336G_REG_P1_YH 0x05
#define FT6336G_REG_P1_YL 0x06
#define FT6336G_REG_ID_G_MODE 0xA4
#define FT6336G_REG_ID_G_THGROUP 0x80

static void IRAM_ATTR ft6336g_isr_handler(void* arg) { g_touch_irq_flag = true; }

static esp_err_t ft6336g_read_reg(uint8_t reg, uint8_t* data, size_t len) {
    return i2c_master_transmit_receive(g_ft6336g_dev_handle, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

static esp_err_t ft6336g_write_reg(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_transmit(g_ft6336g_dev_handle, write_buf, sizeof(write_buf), pdMS_TO_TICKS(100));
}

esp_err_t ft6336g_init(void) {
    esp_err_t ret;

    i2c_master_bus_handle_t bus_handle = bsp_i2c_get_bus_handle();
    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return ESP_FAIL;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = FT6336G_I2C_ADDR,
        .scl_speed_hz = BSP_I2C_FREQ_HZ,
    };
    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &g_ft6336g_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add FT6336G device: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置中断引脚
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << FT6336G_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(FT6336G_INT_PIN, ft6336g_isr_handler, NULL);

    // 设置触摸阈值和模式
    ft6336g_write_reg(FT6336G_REG_ID_G_THGROUP, 0x16);
    ft6336g_write_reg(FT6336G_REG_ID_G_MODE, 0x00);

    ESP_LOGI(TAG, "FT6336G initialized successfully");
    return ESP_OK;
}

uint8_t ft6336g_get_touch_points(void) {
    if (g_touch_irq_flag) {
        g_touch_irq_flag = false;
        uint8_t touch_points = 0;
        esp_err_t ret = ft6336g_read_reg(FT6336G_REG_TD_STATUS, &touch_points, 1);
        if (ret == ESP_OK) {
            return touch_points & 0x0F;
        }
    }
    return 0;
}

esp_err_t ft6336g_read_touch_points(ft6336g_touch_point_t* points, uint8_t* num_points) {
    if (points == NULL || num_points == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *num_points = ft6336g_get_touch_points();
    if (*num_points > 0) {
        uint8_t data[4];
        esp_err_t ret = ft6336g_read_reg(FT6336G_REG_P1_XH, data, 4);
        if (ret == ESP_OK) {
            points[0].x = ((data[0] & 0x0F) << 8) | data[1];
            points[0].y = ((data[2] & 0x0F) << 8) | data[3];
            points[0].touch_id = (data[2] >> 4);
            return ESP_OK;
        }
        return ret;
    }
    return ESP_OK;
}
