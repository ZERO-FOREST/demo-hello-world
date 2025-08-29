
#include "bsp_i2c.h"
#include "esp_err.h"
#include "esp_log.h"


static const char* TAG = "BSP_I2C";
static i2c_master_bus_handle_t g_i2c_bus_handle = NULL;

esp_err_t bsp_i2c_init(void) {
    if (g_i2c_bus_handle) {
        ESP_LOGW(TAG, "I2C bus already initialized");
        return ESP_OK;
    }

    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = BSP_I2C_NUM,
        .scl_io_num = BSP_I2C_SCL_PIN,
        .sda_io_num = BSP_I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&i2c_mst_config, &g_i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C new master bus failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "I2C master bus initialized successfully");
    }
    return ret;
}

esp_err_t bsp_i2c_deinit(void) {
    if (g_i2c_bus_handle == NULL) {
        return ESP_OK;
    }
    esp_err_t ret = i2c_del_master_bus(g_i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C master bus delete failed: %s", esp_err_to_name(ret));
    } else {
        g_i2c_bus_handle = NULL;
        ESP_LOGI(TAG, "I2C master bus de-initialized successfully");
    }
    return ret;
}

i2c_master_bus_handle_t bsp_i2c_get_bus_handle(void) { return g_i2c_bus_handle; }
