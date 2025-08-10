/**
 * @file i2s_tdm.c
 * @brief I2S 标准模式(Philips) 实现 - MAX98357 扬声器驱动
 * @author Your Name
 * @date 2024
 */

#include "i2s_tdm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "I2S_STD";

// 全局句柄
static i2s_tdm_handle_t g_i2s_tdm_handle = {0};

/**
 * @brief 初始化 I2S STD (Philips) - 仅 TX 通道
 */
esp_err_t i2s_tdm_init(void) {
    esp_err_t ret;

    if (g_i2s_tdm_handle.is_initialized) {
        ESP_LOGW(TAG, "I2S TDM already initialized");
        return ESP_OK;
    }

    // 配置发送通道 (到 MAX98357)
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    tx_chan_cfg.auto_clear = true;
    tx_chan_cfg.dma_desc_num = 8;
    tx_chan_cfg.dma_frame_num = 64;

    ret = i2s_new_channel(&tx_chan_cfg, &g_i2s_tdm_handle.tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create TX channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置 STD(Philips) 发送：16-bit 数据，32-bit 槽位，立体声，APLL 时钟，MCLK=256fs
    i2s_std_clk_config_t clk_cfg = {
        .sample_rate_hz = I2S_TDM_SAMPLE_RATE,
#ifdef I2S_CLK_SRC_APLL
        .clk_src = I2S_CLK_SRC_APLL,
#else
        .clk_src = I2S_CLK_SRC_XTAL,
#endif
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };
    i2s_std_slot_config_t slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_TDM_BITS_PER_SAMPLE, I2S_SLOT_MODE_STEREO);
    slot_cfg.slot_bit_width = I2S_TDM_SLOT_BIT_WIDTH; // 32-bit 槽位保证 64fs
    slot_cfg.ws_pol = false;   // I2S 标准：LRCK 低电平=左声道（MAX98357 默认）
    slot_cfg.bit_shift = true; // I2S 标准：WS 边沿后延迟 1bit

    i2s_std_config_t std_cfg = {
        .clk_cfg = clk_cfg,
        .slot_cfg = slot_cfg,
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_TDM_BCLK_PIN,
            .ws   = I2S_TDM_LRCK_PIN,
            .dout = I2S_TDM_DATA_OUT_PIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false }
        }
    };

    ret = i2s_channel_init_std_mode(g_i2s_tdm_handle.tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init STD mode: %s", esp_err_to_name(ret));
        i2s_del_channel(g_i2s_tdm_handle.tx_handle);
        return ret;
    }

    g_i2s_tdm_handle.is_initialized = true;
    g_i2s_tdm_handle.sample_rate = I2S_TDM_SAMPLE_RATE;
    g_i2s_tdm_handle.buffer_size = 1024; // 默认缓冲区大小

    ESP_LOGI(TAG, "I2S STD initialized (Philips)");
    ESP_LOGI(TAG, "BCLK: GPIO%d, LRCK: GPIO%d, DOUT: GPIO%d", I2S_TDM_BCLK_PIN, I2S_TDM_LRCK_PIN, I2S_TDM_DATA_OUT_PIN);
    ESP_LOGI(TAG, "Sample Rate: %dHz, Data Bits: %d, Slot Bits: %d, Channels: %d", I2S_TDM_SAMPLE_RATE, I2S_TDM_BITS_PER_SAMPLE, I2S_TDM_SLOT_BIT_WIDTH, I2S_TDM_CHANNELS);

    return ESP_OK;
}

/**
 * @brief 反初始化 I2S STD
 */
esp_err_t i2s_tdm_deinit(void) {
    if (!g_i2s_tdm_handle.is_initialized) {
        return ESP_OK;
    }

    // 停止通道
    if (g_i2s_tdm_handle.tx_handle) {
        i2s_channel_disable(g_i2s_tdm_handle.tx_handle);
    }

    // 删除通道
    if (g_i2s_tdm_handle.tx_handle) {
        i2s_del_channel(g_i2s_tdm_handle.tx_handle);
        g_i2s_tdm_handle.tx_handle = NULL;
    }
    if (g_i2s_tdm_handle.rx_handle) {
        i2s_del_channel(g_i2s_tdm_handle.rx_handle);
        g_i2s_tdm_handle.rx_handle = NULL;
    }

    g_i2s_tdm_handle.is_initialized = false;
    ESP_LOGI(TAG, "I2S TDM deinitialized");

    return ESP_OK;
}

/**
 * @brief 启动 I2S STD (仅 TX)
 */
esp_err_t i2s_tdm_start(void) {
    if (!g_i2s_tdm_handle.is_initialized) {
        ESP_LOGE(TAG, "I2S TDM not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret;

    // 启动发送通道 (扬声器)
    ret = i2s_channel_enable(g_i2s_tdm_handle.tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable TX channel: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2S STD started (TX)");
    return ESP_OK;
}

/**
 * @brief 停止 I2S STD
 */
esp_err_t i2s_tdm_stop(void) {
    if (!g_i2s_tdm_handle.is_initialized) {
        return ESP_OK;
    }

    if (g_i2s_tdm_handle.tx_handle) {
        i2s_channel_disable(g_i2s_tdm_handle.tx_handle);
    }

    ESP_LOGI(TAG, "I2S TDM stopped");
    return ESP_OK;
}

/**
 * @brief 写入音频数据到扬声器
 */
esp_err_t i2s_tdm_write(const void* data, size_t size, size_t* bytes_written) {
    if (!g_i2s_tdm_handle.is_initialized) {
        ESP_LOGE(TAG, "I2S TDM not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = i2s_channel_write(g_i2s_tdm_handle.tx_handle, data, size, bytes_written, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write audio data: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief 读取麦克风音频数据（STD TX-only 不支持）
 */
esp_err_t i2s_tdm_read(void* data, size_t size, size_t* bytes_read) {
    (void)data; (void)size; (void)bytes_read;
    return ESP_ERR_NOT_SUPPORTED;
}

/**
 * @brief 设置采样率
 */
esp_err_t i2s_tdm_set_sample_rate(uint32_t sample_rate) {
    if (!g_i2s_tdm_handle.is_initialized) {
        ESP_LOGE(TAG, "I2S TDM not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 停止通道
    i2s_channel_disable(g_i2s_tdm_handle.tx_handle);
    i2s_channel_disable(g_i2s_tdm_handle.rx_handle);

    // 重新配置 STD 时钟（APLL + 256fs）
    i2s_std_clk_config_t clk_cfg = {
        .sample_rate_hz = sample_rate,
#ifdef I2S_CLK_SRC_APLL
        .clk_src = I2S_CLK_SRC_APLL,
#else
        .clk_src = I2S_CLK_SRC_XTAL,
#endif
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };

    esp_err_t ret = i2s_channel_reconfig_std_clock(g_i2s_tdm_handle.tx_handle, &clk_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reconfigure STD clock: %s", esp_err_to_name(ret));
        return ret;
    }

    // 重新启动通道
    i2s_channel_enable(g_i2s_tdm_handle.tx_handle);

    g_i2s_tdm_handle.sample_rate = sample_rate;
    ESP_LOGI(TAG, "Sample rate changed to %lu Hz", sample_rate);

    return ESP_OK;
}

/**
 * @brief 获取当前采样率
 */
uint32_t i2s_tdm_get_sample_rate(void) { return g_i2s_tdm_handle.sample_rate; }

/**
 * @brief 检查I2S TDM是否已初始化
 */
bool i2s_tdm_is_initialized(void) { return g_i2s_tdm_handle.is_initialized; }

/**
 * @brief 获取缓冲区大小
 */
uint16_t i2s_tdm_get_buffer_size(void) { return g_i2s_tdm_handle.buffer_size; }