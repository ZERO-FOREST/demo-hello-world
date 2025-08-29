#include "usb_device_receiver.h"
#include "class/cdc/cdc_device.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tcp_protocol.h"
#include "tinyusb.h"
#include <string.h>

static const char* TAG = "usb_rx";

static TaskHandle_t s_usb_task = NULL;
static uint8_t s_rx_chunk[USB_RX_CHUNK_SIZE];
static uint8_t s_parse_buf[USB_RX_BUFFER_SIZE];
static size_t s_parse_len = 0;

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
    while (1) {
        int32_t n = tinyusb_cdcacm_read(0, s_rx_chunk, sizeof(s_rx_chunk));
        if (n > 0) {
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
        } else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}

esp_err_t usb_receiver_init(void) {
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
        .configuration_descriptor = NULL,
        .self_powered = false,
        .vbus_monitor_io = -1,
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
        .callback_tx_done = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL,
    };
    ret = tinyusb_cdcacm_init(&acm_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_cdcacm_init 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "USB CDC 初始化完成");
    return ESP_OK;
}

void usb_receiver_start(void) {
    if (s_usb_task)
        return;
    xTaskCreatePinnedToCore(usb_rx_task, "usb_rx", 4096, NULL, 9, &s_usb_task, tskNO_AFFINITY);
}

void usb_receiver_stop(void) {
    if (s_usb_task) {
        vTaskDelete(s_usb_task);
        s_usb_task = NULL;
    }
    tinyusb_driver_uninstall();
}
