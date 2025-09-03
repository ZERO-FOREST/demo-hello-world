#include "spi_slave_receiver.h"
#include "driver/spi_slave.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "settings_manager.h"
#include "esp_jpeg_common.h"
#include "esp_jpeg_enc.h"
#include "tcp_protocol.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "spi_rx";

// 接收缓冲与解析缓冲
static uint8_t* s_rx_dma_bufs[SPI_RX_QUEUE_SIZE];
static uint8_t s_parse_buf[SPI_RX_BUFFER_SZ];
static size_t s_parse_len = 0;
static TaskHandle_t s_task = NULL;
static jpeg_enc_handle_t s_jpeg_enc = NULL;  // 使用标准ESP-IDF JPEG编码器
static QueueHandle_t s_jpeg_queue = NULL;  // SPI->JPEG 编码消息队列
static TaskHandle_t s_jpeg_task = NULL;    // JPEG 编码任务

// JPEG编码缓冲区
static uint8_t* s_jpeg_input_buffer = NULL;
static uint8_t* s_jpeg_output_buffer = NULL;
static size_t s_jpeg_input_buffer_size = 0;
static size_t s_jpeg_output_buffer_size = 0;
static size_t s_jpeg_data_len = 0;

typedef struct {
    uint8_t* data;
    size_t len;
} jpeg_chunk_msg_t;

// JPEG编码参数配置
#define JPEG_ENC_WIDTH 240
#define JPEG_ENC_HEIGHT 320
#define JPEG_ENC_QUALITY 70
#define JPEG_ENC_SRC_TYPE JPEG_PIXEL_FORMAT_RGB888
#define JPEG_ENC_SUBSAMPLE JPEG_SUBSAMPLE_420

static esp_err_t init_jpeg_encoder(void);

static void jpeg_encode_feed_task(void* arg) {
    const char* T = "jpeg_feed";
    ESP_LOGI(T, "JPEG feed task started");
    jpeg_chunk_msg_t msg;
    
    while (1) {
        if (xQueueReceive(s_jpeg_queue, &msg, portMAX_DELAY) == pdTRUE) {
            if (msg.data == NULL && msg.len == 0) {
                // 退出信号
                break;
            }
            
            if (msg.data && msg.len > 0 && s_jpeg_enc) {
                // 累积数据到输入缓冲区
                if (s_jpeg_data_len + msg.len <= s_jpeg_input_buffer_size) {
                    memcpy(s_jpeg_input_buffer + s_jpeg_data_len, msg.data, msg.len);
                    s_jpeg_data_len += msg.len;
                    
                    // 检查是否收集到足够的数据进行编码
                    size_t expected_size = JPEG_ENC_WIDTH * JPEG_ENC_HEIGHT * 3; // RGB888格式
                    if (s_jpeg_data_len >= expected_size) {
                        // 执行JPEG编码
                        int out_len = 0;
                        jpeg_error_t ret = jpeg_enc_process(s_jpeg_enc, s_jpeg_input_buffer, 
                                                          expected_size, s_jpeg_output_buffer, 
                                                          s_jpeg_output_buffer_size, &out_len);
                        if (ret == JPEG_ERR_OK && out_len > 0) {
                            ESP_LOGI(T, "JPEG encoded: %d bytes -> %d bytes", expected_size, out_len);
                            // 这里可以将编码后的数据发送到其他地方
                        } else {
                            ESP_LOGW(T, "JPEG encode failed: %d", ret);
                        }
                        s_jpeg_data_len = 0; // 重置缓冲区
                    }
                } else {
                    ESP_LOGW(T, "Input buffer overflow, dropping data");
                    s_jpeg_data_len = 0; // 重置缓冲区
                }
            }
            
            if (msg.data) free(msg.data);
        }
    }
    ESP_LOGI(T, "JPEG feed task stopped");
    vTaskDelete(NULL);
}

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

// SPI命令定义
#define SPI_CMD_SET_JPEG_QUALITY 0x51  // 'Q'
#define SPI_CMD_GET_JPEG_QUALITY 0x71  // 'q'

// 处理SPI设置命令
static void handle_spi_settings_command(const uint8_t* data, size_t len) {
    if (len < 2) return;
    
    uint8_t cmd = data[0];
    
    switch (cmd) {
        case SPI_CMD_SET_JPEG_QUALITY:
            if (len >= 2) {
                settings_set_via_spi(SETTING_JPEG_QUALITY, &data[1], len - 1);
                ESP_LOGI(TAG, "SPI set JPEG quality: %u", data[1]);
            }
            break;
            
        case SPI_CMD_GET_JPEG_QUALITY: {
            setting_value_t quality;
            if (settings_get(SETTING_JPEG_QUALITY, &quality) == ESP_OK) {
                // 这里可以发送回SPI主机，需要实现SPI发送功能
                ESP_LOGI(TAG, "Current JPEG quality: %u", quality.uint8_value);
            }
            break;
        }
            
        default:
            ESP_LOGW(TAG, "Unknown SPI command: 0x%02X", cmd);
            break;
    }
}

// 在SPI数据处理中添加命令检测
static void spi_rx_task(void* arg) {
    ESP_LOGI(TAG, "SPI 从机接收任务启动 (DMA 队列)");

    while (1) {
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
                // 异步通过队列喂给 JPEG 编码器，避免在 SPI 线程内执行编码
                if (s_jpeg_queue && bytes > 0) {
                    uint8_t* cp = malloc(bytes);
                    if (cp) {
                        memcpy(cp, ret_trans->rx_buffer, bytes);
                        jpeg_chunk_msg_t m = {.data = cp, .len = bytes};
                        if (xQueueSend(s_jpeg_queue, &m, pdMS_TO_TICKS(100)) != pdTRUE) {
                            ESP_LOGW(TAG, "JPEG queue full, drop %d bytes", bytes);
                            free(cp);
                        }
                    }
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
    ESP_LOGI(TAG, "SPI 从机初始化完成: host=%d, MOSI=%d MISO=%d SCLK=%d CS=%d", SPI_RX_HOST, SPI_SLAVE_PIN_MOSI,
             SPI_SLAVE_PIN_MISO, SPI_SLAVE_PIN_SCLK, SPI_SLAVE_PIN_CS);

    // 初始化JPEG编码器
    if (init_jpeg_encoder() != ESP_OK) {
        ESP_LOGW(TAG, "JPEG encoder initialization failed");
    } else {
        // 创建编码消息队列与编码任务
        if (!s_jpeg_queue) {
            s_jpeg_queue = xQueueCreate(16, sizeof(jpeg_chunk_msg_t));
            if (!s_jpeg_queue) {
                ESP_LOGE(TAG, "create jpeg queue failed");
            }
        }
        if (s_jpeg_queue && !s_jpeg_task) {
            if (xTaskCreatePinnedToCore(jpeg_encode_feed_task, "jpeg_feed", 8192, NULL, 9, &s_jpeg_task, tskNO_AFFINITY) != pdPASS) {
                ESP_LOGE(TAG, "create jpeg feed task failed");
            }
        }
    }
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
    
    // 关闭 JPEG 编码任务与队列
    if (s_jpeg_task) {
        // 发送退出信号
        jpeg_chunk_msg_t quit = {.data = NULL, .len = 0};
        xQueueSend(s_jpeg_queue, &quit, 0);
        // 等待任务自行退出
        vTaskDelay(pdMS_TO_TICKS(10));
        s_jpeg_task = NULL;
    }
    if (s_jpeg_queue) {
        // 清空残留消息并释放内存
        jpeg_chunk_msg_t m;
        while (xQueueReceive(s_jpeg_queue, &m, 0) == pdTRUE) {
            if (m.data) free(m.data);
        }
        vQueueDelete(s_jpeg_queue);
        s_jpeg_queue = NULL;
    }
    
    // 清理JPEG编码器资源
    if (s_jpeg_enc) {
        jpeg_enc_close(s_jpeg_enc);
        s_jpeg_enc = NULL;
    }
    
    if (s_jpeg_input_buffer) {
        free(s_jpeg_input_buffer);
        s_jpeg_input_buffer = NULL;
    }
    
    if (s_jpeg_output_buffer) {
        free(s_jpeg_output_buffer);
        s_jpeg_output_buffer = NULL;
    }
    
    s_jpeg_input_buffer_size = 0;
    s_jpeg_output_buffer_size = 0;
    s_jpeg_data_len = 0;
}

// JPEG质量变更回调函数
static void on_jpeg_quality_changed(setting_type_t type, const setting_value_t* new_value) {
    if (type == SETTING_JPEG_QUALITY && s_jpeg_enc) {
        // 这里可以添加JPEG编码器质量动态调整逻辑
        ESP_LOGI(TAG, "JPEG quality changed to: %u", new_value->uint8_value);
        // 注意：标准JPEG编码器需要重新创建才能更改质量
    }
}

// 初始化JPEG编码器（独立函数）
static esp_err_t init_jpeg_encoder(void) {
    // 创建标准ESP-IDF JPEG编码器
    jpeg_enc_config_t jpeg_cfg = DEFAULT_JPEG_ENC_CONFIG();
    jpeg_cfg.width = JPEG_ENC_WIDTH;
    jpeg_cfg.height = JPEG_ENC_HEIGHT;
    jpeg_cfg.src_type = JPEG_ENC_SRC_TYPE;
    jpeg_cfg.subsampling = JPEG_ENC_SUBSAMPLE;
    jpeg_cfg.quality = JPEG_ENC_QUALITY;
    jpeg_cfg.rotate = JPEG_ROTATE_0D;
    jpeg_cfg.task_enable = false;
    
    esp_err_t ret = jpeg_enc_open(&jpeg_cfg, &s_jpeg_enc);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "JPEG encoder open failed: %d", ret);
        return ESP_FAIL;
    }
    
    // 分配缓冲区
    s_jpeg_input_buffer_size = JPEG_ENC_WIDTH * JPEG_ENC_HEIGHT * 3; // RGB888
    s_jpeg_output_buffer_size = 100 * 1024; // 100KB
    
    s_jpeg_input_buffer = (uint8_t*)heap_caps_malloc(s_jpeg_input_buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_jpeg_output_buffer = (uint8_t*)heap_caps_malloc(s_jpeg_output_buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    if (!s_jpeg_input_buffer || !s_jpeg_output_buffer) {
        ESP_LOGE(TAG, "Failed to allocate JPEG buffers");
        if (s_jpeg_input_buffer) {
            free(s_jpeg_input_buffer);
            s_jpeg_input_buffer = NULL;
        }
        if (s_jpeg_output_buffer) {
            free(s_jpeg_output_buffer);
            s_jpeg_output_buffer = NULL;
        }
        jpeg_enc_close(s_jpeg_enc);
        s_jpeg_enc = NULL;
        return ESP_ERR_NO_MEM;
    }
    
    s_jpeg_data_len = 0;
    ESP_LOGI(TAG, "JPEG encoder initialized: %dx%d fmt=%d q=%d", 
             jpeg_cfg.width, jpeg_cfg.height, jpeg_cfg.src_type, jpeg_cfg.quality);
    
    return ESP_OK;
}
