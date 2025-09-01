#include "usb_device_receiver.h"
#include "sdkconfig.h"

#include "tinyusb.h"
#include "tusb.h"
#include "tusb_cdc_acm.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jpeg_stream_encoder.h"
#include "tcp_protocol.h"
#include <string.h>

static const char* TAG = "usb_rx";

static TaskHandle_t s_usb_task = NULL;
static uint8_t s_rx_chunk[USB_RX_CHUNK_SIZE];
static uint8_t s_parse_buf[USB_RX_BUFFER_SIZE];
static size_t s_parse_len = 0;
static jpeg_stream_handle_t s_jpeg_usb = NULL;
static bool s_usb_connected = false;

// USB CDC 连接状态回调
static void usb_line_state_changed_callback(int itf, cdcacm_event_t *event) {
    if (event->type == CDC_EVENT_LINE_STATE_CHANGED) {
        bool dtr = event->line_state_changed_data.dtr;
        bool rts = event->line_state_changed_data.rts;
        s_usb_connected = (dtr && rts);
        ESP_LOGI(TAG, "USB连接状态: DTR=%d, RTS=%d, 连接=%s", 
                 dtr, rts, s_usb_connected ? "已连接" : "断开");
    }
}

static void parse_and_dispatch(const uint8_t* data, size_t len) {
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
        }
        pos += frame_size;
    }
    if (pos < len) {
        size_t remain = len - pos;
        if (remain > USB_RX_BUFFER_SIZE)
            remain = USB_RX_BUFFER_SIZE;
        memmove(s_parse_buf, &data[pos], remain);
        s_parse_len = remain;
    } else {
        s_parse_len = 0;
    }
}

static void usb_rx_task(void* arg) {
    ESP_LOGI(TAG, "USB CDC 接收任务启动");
    
    // 等待USB连接建立
    while (!tusb_cdc_acm_initialized(TINYUSB_CDC_ACM_0)) {
        ESP_LOGI(TAG, "等待USB CDC初始化完成...");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGI(TAG, "USB CDC已初始化，开始接收数据");
    
    while (1) {
        size_t n = 0;
        esp_err_t ret = tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0, s_rx_chunk, sizeof(s_rx_chunk), &n);
        if (ret == ESP_OK && n > 0) {
            ESP_LOGD(TAG, "接收到 %zu 字节数据", n);
            if (s_parse_len + (size_t)n > USB_RX_BUFFER_SIZE) {
                size_t to_copy = USB_RX_BUFFER_SIZE;
                if ((size_t)n < USB_RX_BUFFER_SIZE) {
                    memmove(s_parse_buf, &s_parse_buf[s_parse_len + n - USB_RX_BUFFER_SIZE],
                            USB_RX_BUFFER_SIZE - (size_t)n);
                    memcpy(&s_parse_buf[USB_RX_BUFFER_SIZE - (size_t)n], s_rx_chunk, (size_t)n);
                } else {
                    memcpy(s_parse_buf, &s_rx_chunk[n - USB_RX_BUFFER_SIZE], USB_RX_BUFFER_SIZE);
                }
                s_parse_len = to_copy;
            } else {
                memcpy(&s_parse_buf[s_parse_len], s_rx_chunk, (size_t)n);
                s_parse_len += (size_t)n;
            }
            parse_and_dispatch(s_parse_buf, s_parse_len);
            // 同时将原始流喂给 JPEG 编码器（假设上位机发送的是一帧完整原始像素流）
            if (s_jpeg_usb) {
                jpeg_stream_feed(s_jpeg_usb, s_rx_chunk, (size_t)n);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        // 无论是否有数据，适当让出 CPU，防止长期占用导致 WDT 触发
        vTaskDelay(1);
    }
}

esp_err_t usb_receiver_init(void) {
    // ESP32-S3内置USB接口，不需要外部PHY
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,      // 使用默认设备描述符
        .string_descriptor = NULL,      // 使用默认字符串描述符
        .external_phy = false,          // ESP32-S3使用内置USB PHY
        .configuration_descriptor = NULL, // 使用默认配置描述符
        .self_powered = false,          // 总线供电模式
        .vbus_monitor_io = -1,          // ESP32-S3不需要外部VBUS监控
    };
    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_driver_install 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 2048, // 扩大内部未读缓冲
        .callback_rx = NULL,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = usb_line_state_changed_callback,
        .callback_line_coding_changed = NULL,
    };
    ret = tusb_cdc_acm_init(&acm_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_cdcacm_init 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "USB CDC 初始化完成");
    
    // 等待USB连接建立
    ESP_LOGI(TAG, "等待USB设备连接...");
    
    // 给USB设备一些时间来枚举
    vTaskDelay(pdMS_TO_TICKS(1000));
    // 创建 USB JPEG 编码流（分辨率/格式可据实际数据源调整）
    jpeg_stream_config_t jcfg = {
        .width = JPEG_STREAM_WIDTH,
        .height = JPEG_STREAM_HEIGHT,
        .src_type = JPEG_STREAM_SRC_FMT,
        .subsampling = JPEG_STREAM_SUBSAMPLE,
        .quality = JPEG_STREAM_QUALITY,
        .stream_id = JPEG_STREAM_ID_USB,
    };
    if (jpeg_stream_create(&jcfg, &s_jpeg_usb) != ESP_OK) {
        ESP_LOGW(TAG, "usb jpeg_stream_create failed");
    }
    return ESP_OK;
}

void usb_receiver_start(void) {
    if (s_usb_task)
        return;
    // 降低任务优先级并固定到 CPU1，减少对 CPU0 空闲任务的影响
    xTaskCreatePinnedToCore(usb_rx_task, "usb_rx", 4096, NULL, 4, &s_usb_task, 1);
}

void usb_receiver_stop(void) {
    if (s_usb_task) {
        vTaskDelete(s_usb_task);
        s_usb_task = NULL;
    }
    if (s_jpeg_usb) {
        jpeg_stream_destroy(s_jpeg_usb);
        s_jpeg_usb = NULL;
    }
    // tinyusb_driver_uninstall 函数在当前版本中不可用 (IDF-1474)
    // 由于没有卸载函数，直接返回
}
