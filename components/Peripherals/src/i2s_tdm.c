/**
 * @file i2s_tdm.c
 * @brief I2S TDM实现 - 数字麦克风 + MAX9357解码器
 * @author Your Name
 * @date 2024
 */

#include "i2s_tdm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "I2S_TDM";

// 全局句柄
static i2s_tdm_handle_t g_i2s_tdm_handle = {0};

/**
 * @brief 初始化I2S TDM
 */
esp_err_t i2s_tdm_init(void) {
    esp_err_t ret;

    if (g_i2s_tdm_handle.is_initialized) {
        ESP_LOGW(TAG, "I2S TDM already initialized");
        return ESP_OK;
    }

    // 配置发送通道 (到MAX9357)
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    tx_chan_cfg.auto_clear = true;
    tx_chan_cfg.dma_desc_num = 8;
    tx_chan_cfg.dma_frame_num = 64;

    ret = i2s_new_channel(&tx_chan_cfg, &g_i2s_tdm_handle.tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create TX channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置接收通道 (来自数字麦克风)
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    rx_chan_cfg.auto_clear = true;
    rx_chan_cfg.dma_desc_num = 8;
    rx_chan_cfg.dma_frame_num = 64;

    ret = i2s_new_channel(&rx_chan_cfg, NULL, &g_i2s_tdm_handle.rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RX channel: %s", esp_err_to_name(ret));
        i2s_del_channel(g_i2s_tdm_handle.tx_handle);
        return ret;
    }

    // 配置TDM发送
    i2s_tdm_config_t tx_tdm_cfg = {
        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(I2S_TDM_SAMPLE_RATE),
        .slot_cfg =
            {
                .data_bit_width = I2S_TDM_BITS_PER_SAMPLE,
                .slot_mode = I2S_SLOT_MODE_STEREO,
                .slot_mask = (1 << I2S_TDM_SPEAKER_SLOT),
                .bit_shift = true,
                .left_align = false,
                .big_endian = false,
                .bit_order_lsb = false,
            },
        .gpio_cfg = {.mclk = I2S_GPIO_UNUSED,
                     .bclk = I2S_TDM_BCLK_PIN,
                     .ws = I2S_TDM_LRCK_PIN,
                     .dout = I2S_TDM_DATA_OUT_PIN,
                     .din = I2S_GPIO_UNUSED,
                     .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false}}};

    ret = i2s_channel_init_tdm_mode(g_i2s_tdm_handle.tx_handle, &tx_tdm_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init TX TDM mode: %s", esp_err_to_name(ret));
        i2s_del_channel(g_i2s_tdm_handle.tx_handle);
        i2s_del_channel(g_i2s_tdm_handle.rx_handle);
        return ret;
    }

    // 配置TDM接收
    i2s_tdm_config_t rx_tdm_cfg = {
        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(I2S_TDM_SAMPLE_RATE),
        .slot_cfg =
            {
                .data_bit_width = I2S_TDM_BITS_PER_SAMPLE,
                .slot_mode = I2S_SLOT_MODE_STEREO,
                .slot_mask = (1 << I2S_TDM_MIC_SLOT),
                .bit_shift = true,
                .left_align = false,
                .big_endian = false,
                .bit_order_lsb = false,
            },
        .gpio_cfg = {.mclk = I2S_GPIO_UNUSED,
                     .bclk = I2S_TDM_BCLK_PIN,
                     .ws = I2S_TDM_LRCK_PIN,
                     .dout = I2S_GPIO_UNUSED,
                     .din = I2S_TDM_DATA_IN_PIN,
                     .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false}}};

    ret = i2s_channel_init_tdm_mode(g_i2s_tdm_handle.rx_handle, &rx_tdm_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init RX TDM mode: %s", esp_err_to_name(ret));
        i2s_del_channel(g_i2s_tdm_handle.tx_handle);
        i2s_del_channel(g_i2s_tdm_handle.rx_handle);
        return ret;
    }

    g_i2s_tdm_handle.is_initialized = true;
    g_i2s_tdm_handle.sample_rate = I2S_TDM_SAMPLE_RATE;
    g_i2s_tdm_handle.buffer_size = 1024; // 默认缓冲区大小

    ESP_LOGI(TAG, "I2S TDM initialized successfully");
    ESP_LOGI(TAG, "BCLK: GPIO%d, LRCK: GPIO%d, DATA_OUT: GPIO%d, DATA_IN: GPIO%d", I2S_TDM_BCLK_PIN, I2S_TDM_LRCK_PIN,
             I2S_TDM_DATA_OUT_PIN, I2S_TDM_DATA_IN_PIN);
    ESP_LOGI(TAG, "Sample Rate: %dHz, Bits: %d, Channels: %d", I2S_TDM_SAMPLE_RATE, I2S_TDM_BITS_PER_SAMPLE,
             I2S_TDM_CHANNELS);

    return ESP_OK;
}

/**
 * @brief 反初始化I2S TDM
 */
esp_err_t i2s_tdm_deinit(void) {
    if (!g_i2s_tdm_handle.is_initialized) {
        return ESP_OK;
    }

    // 停止通道
    i2s_channel_disable(g_i2s_tdm_handle.tx_handle);
    i2s_channel_disable(g_i2s_tdm_handle.rx_handle);

    // 删除通道
    i2s_del_channel(g_i2s_tdm_handle.tx_handle);
    i2s_del_channel(g_i2s_tdm_handle.rx_handle);

    g_i2s_tdm_handle.is_initialized = false;
    ESP_LOGI(TAG, "I2S TDM deinitialized");

    return ESP_OK;
}

/**
 * @brief 启动I2S TDM
 */
esp_err_t i2s_tdm_start(void) {
    if (!g_i2s_tdm_handle.is_initialized) {
        ESP_LOGE(TAG, "I2S TDM not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret;

    // 启动接收通道 (麦克风)
    ret = i2s_channel_enable(g_i2s_tdm_handle.rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RX channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // 启动发送通道 (扬声器)
    ret = i2s_channel_enable(g_i2s_tdm_handle.tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable TX channel: %s", esp_err_to_name(ret));
        i2s_channel_disable(g_i2s_tdm_handle.rx_handle);
        return ret;
    }

    ESP_LOGI(TAG, "I2S TDM started");
    return ESP_OK;
}

/**
 * @brief 停止I2S TDM
 */
esp_err_t i2s_tdm_stop(void) {
    if (!g_i2s_tdm_handle.is_initialized) {
        return ESP_OK;
    }

    i2s_channel_disable(g_i2s_tdm_handle.tx_handle);
    i2s_channel_disable(g_i2s_tdm_handle.rx_handle);

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
 * @brief 读取麦克风音频数据
 */
esp_err_t i2s_tdm_read(void* data, size_t size, size_t* bytes_read) {
    if (!g_i2s_tdm_handle.is_initialized) {
        ESP_LOGE(TAG, "I2S TDM not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = i2s_channel_read(g_i2s_tdm_handle.rx_handle, data, size, bytes_read, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read audio data: %s", esp_err_to_name(ret));
    }

    return ret;
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

    // 重新配置时钟
    i2s_tdm_clk_config_t clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(sample_rate);

    esp_err_t ret = i2s_channel_reconfig_tdm_clock(g_i2s_tdm_handle.tx_handle, &clk_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reconfigure TX clock: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_channel_reconfig_tdm_clock(g_i2s_tdm_handle.rx_handle, &clk_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reconfigure RX clock: %s", esp_err_to_name(ret));
        return ret;
    }

    // 重新启动通道
    i2s_channel_enable(g_i2s_tdm_handle.rx_handle);
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