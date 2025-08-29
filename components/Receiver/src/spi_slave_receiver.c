#include "spi_slave_receiver.h"
#include "driver/gpio.h"
#include "driver/spi_slave.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jpeg_stream_encoder.h"
#include "tcp_protocol.h"
#include <string.h>

static const char* TAG = "spi_rx";

// 接收缓冲与解析缓冲
static uint8_t* s_rx_dma_bufs[SPI_RX_QUEUE_SIZE];
static uint8_t s_parse_buf[SPI_RX_BUFFER_SZ];
static size_t s_parse_len = 0;
static TaskHandle_t s_task = NULL;
static jpeg_stream_handle_t s_jpeg_spi = NULL;

// 解析并回调
static void spi_parse_and_dispatch(const uint8_t* data, size_t len) {
    if (!data || len < MIN_FRAME_SIZE)
        return;

    size_t pos = 0;
    while (pos + MIN_FRAME_SIZE <= len) {
        if (pos + 3 > len)
            break;
        if (!(data[pos] == ((FRAME_HEADER >> 8) & 0xFF) && data[pos + 1] == (FRAME_HEADER & 0xFF))) {
            pos++;
            continue;
        }

        uint8_t length_field = data[pos + 2];
        size_t frame_size = 2 + 1 + length_field + 2;
        if (pos + frame_size > len)
            break;

        protocol_frame_t frame;
        parse_result_t pr = parse_protocol_frame(&data[pos], (uint16_t)frame_size, &frame);
        if (pr == PARSE_SUCCESS) {
            switch (frame.frame_type) {
            case FRAME_TYPE_REMOTE_CONTROL:
                handle_remote_control_data(&frame.payload.remote_control);
                break;
            case FRAME_TYPE_HEARTBEAT:
                handle_heartbeat_data(&frame.payload.heartbeat);
                break;
            case FRAME_TYPE_EXTENDED_CMD:
                handle_extended_command(&frame.payload.extended_cmd);
                break;
            default:
                break;
            }
        } else {
            ESP_LOGW(TAG, "parse frame fail: %d", pr);
        }
        pos += frame_size;
    }

    if (pos < len) {
        size_t remain = len - pos;
        if (remain > SPI_RX_BUFFER_SZ)
            remain = SPI_RX_BUFFER_SZ; // clamp
        memmove(s_parse_buf, &data[pos], remain);
        s_parse_len = remain;
    } else {
        s_parse_len = 0;
    }
}

static void spi_rx_task(void* arg) {
    ESP_LOGI(TAG, "SPI 从机接收任务启动 (DMA 队列)");

    // 预建队列事务
    spi_slave_transaction_t trans[SPI_RX_QUEUE_SIZE];
    memset(trans, 0, sizeof(trans));
    for (int i = 0; i < SPI_RX_QUEUE_SIZE; i++) {
        trans[i].length = SPI_RX_TRANSACTION_SZ * 8;
        trans[i].rx_buffer = s_rx_dma_bufs[i];
        trans[i].tx_buffer = NULL; // 仅接收
        esp_err_t ret = spi_slave_queue_trans(SPI_RX_HOST, &trans[i], portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "queue_trans err: %s", esp_err_to_name(ret));
        }
    }

    while (1) {
        spi_slave_transaction_t* ret_trans = NULL;
        esp_err_t ret = spi_slave_get_trans_result(SPI_RX_HOST, &ret_trans, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "get_trans_result err: %s", esp_err_to_name(ret));
            continue;
        }

        size_t bytes = ret_trans->trans_len / 8;
        if (bytes > 0) {
            uint8_t* rxp = (uint8_t*)ret_trans->rx_buffer;

            if (s_parse_len + bytes > SPI_RX_BUFFER_SZ) {
                size_t to_copy = SPI_RX_BUFFER_SZ;
                if (bytes < SPI_RX_BUFFER_SZ) {
                    memmove(s_parse_buf, &s_parse_buf[s_parse_len + bytes - SPI_RX_BUFFER_SZ],
                            SPI_RX_BUFFER_SZ - bytes);
                    memcpy(&s_parse_buf[SPI_RX_BUFFER_SZ - bytes], rxp, bytes);
                } else {
                    memcpy(s_parse_buf, &rxp[bytes - SPI_RX_BUFFER_SZ], SPI_RX_BUFFER_SZ);
                }
                s_parse_len = to_copy;
            } else {
                memcpy(&s_parse_buf[s_parse_len], rxp, bytes);
                s_parse_len += bytes;
            }

            spi_parse_and_dispatch(s_parse_buf, s_parse_len);
            // 同步喂给 JPEG 编码器（假设 SPI 主机按帧发送原始像素流）
            if (s_jpeg_spi) {
                jpeg_stream_feed(s_jpeg_spi, rxp, bytes);
            }
        }

        // 事务用完后立即重新入队，实现流水线
        ret_trans->length = SPI_RX_TRANSACTION_SZ * 8;
        ret_trans->trans_len = 0;
        ret_trans->tx_buffer = NULL;
        ret = spi_slave_queue_trans(SPI_RX_HOST, ret_trans, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "re-queue err: %s", esp_err_to_name(ret));
        }
    }
}

esp_err_t spi_receiver_init(void) {
    // GPIO 配置
    gpio_set_pull_mode(SPI_SLAVE_PIN_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SPI_SLAVE_PIN_SCLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SPI_SLAVE_PIN_CS, GPIO_PULLUP_ONLY);

    spi_bus_config_t bus_cfg = {.mosi_io_num = SPI_SLAVE_PIN_MOSI,
                                .miso_io_num = SPI_SLAVE_PIN_MISO,
                                .sclk_io_num = SPI_SLAVE_PIN_SCLK,
                                .quadwp_io_num = -1,
                                .quadhd_io_num = -1,
                                .max_transfer_sz = SPI_RX_TRANSACTION_SZ,
                                .flags = SPICOMMON_BUSFLAG_GPIO_PINS};

    spi_slave_interface_config_t slv_cfg = {
        .mode = 0, .spics_io_num = SPI_SLAVE_PIN_CS, .queue_size = SPI_RX_QUEUE_SIZE, .flags = 0};

    // 分配 DMA 缓冲（队列中每个事务一个）
    for (int i = 0; i < SPI_RX_QUEUE_SIZE; i++) {
        s_rx_dma_bufs[i] = (uint8_t*)heap_caps_malloc(SPI_RX_TRANSACTION_SZ, MALLOC_CAP_DMA);
        if (!s_rx_dma_bufs[i]) {
            ESP_LOGE(TAG, "DMA buffer alloc failed @%d", i);
            return ESP_ERR_NO_MEM;
        }
    }

    // 使用 DMA 自动分配，提升吞吐
    esp_err_t ret = spi_slave_initialize(SPI_RX_HOST, &bus_cfg, &slv_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_slave_initialize fail: %s", esp_err_to_name(ret));
        return ret;
    }
    // 创建 SPI JPEG 编码流
    jpeg_stream_config_t jcfg = {
        .width = JPEG_STREAM_WIDTH,
        .height = JPEG_STREAM_HEIGHT,
        .src_type = JPEG_STREAM_SRC_FMT,
        .subsampling = JPEG_STREAM_SUBSAMPLE,
        .quality = JPEG_STREAM_QUALITY,
        .stream_id = JPEG_STREAM_ID_SPI,
    };
    if (jpeg_stream_create(&jcfg, &s_jpeg_spi) != ESP_OK) {
        ESP_LOGW(TAG, "spi jpeg_stream_create failed");
    }
    ESP_LOGI(TAG, "SPI 从机初始化完成: host=%d, MOSI=%d MISO=%d SCLK=%d CS=%d", SPI_RX_HOST, SPI_SLAVE_PIN_MOSI,
             SPI_SLAVE_PIN_MISO, SPI_SLAVE_PIN_SCLK, SPI_SLAVE_PIN_CS);
    return ESP_OK;
}

void spi_receiver_start(void) {
    if (s_task)
        return;
    xTaskCreatePinnedToCore(spi_rx_task, "spi_rx", 4096, NULL, 10, &s_task, tskNO_AFFINITY);
}

void spi_receiver_stop(void) {
    if (s_task) {
        vTaskDelete(s_task);
        s_task = NULL;
    }
    spi_slave_free(SPI_RX_HOST);
    if (s_jpeg_spi) {
        jpeg_stream_destroy(s_jpeg_spi);
        s_jpeg_spi = NULL;
    }
}
