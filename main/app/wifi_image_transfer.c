#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include <lwip/netdb.h>
#include <string.h>

#include "esp_jpeg_common.h"
#include "esp_jpeg_dec.h"

#include "esp_heap_caps.h"
#include "ui_image_transfer.h"
#include "wifi_image_transfer.h"

static const char* TAG = "WIFI_IMG_TRANSFER";

#define LISTEN_SOCKET_NUM 1
#define TCP_RECV_BUF_SIZE 4096
#define MAX_JPEG_FRAME_SIZE (100 * 1024) // Max expected JPEG frame size

static TaskHandle_t s_tcp_server_task_handle = NULL;
static bool s_server_running = false;
static uint8_t* s_jpeg_frame_buffer = NULL;
static int s_jpeg_frame_pos = 0;

// Forward declaration for the function that will handle decoded image
static void handle_decoded_image(uint8_t* img_buf, int width, int height, jpeg_pixel_format_t format);

static void tcp_server_task(void* pvParameters) {
    uint16_t port = *(uint16_t*)pvParameters;
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    struct sockaddr_in dest_addr;

    // Allocate JPEG frame buffer
    s_jpeg_frame_buffer = (uint8_t*)heap_caps_malloc(MAX_JPEG_FRAME_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_jpeg_frame_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate JPEG frame buffer");
        s_server_running = false;
        vTaskDelete(NULL);
        return;
    }
    s_jpeg_frame_pos = 0;

    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    ip_protocol = IPPROTO_IP;

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        s_server_running = false;
        free(s_jpeg_frame_buffer);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", port);

    err = listen(listen_sock, LISTEN_SOCKET_NUM);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket listening");

    s_server_running = true;

    while (s_server_running) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr*)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            continue;
        }

        // Convert ip address to string
        if (addr_family == AF_INET) {
            inet_ntoa_r(source_addr.sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGI(TAG, "Socket accepted IP address: %s", addr_str);

        int len;
        uint8_t rx_buffer[TCP_RECV_BUF_SIZE];
        s_jpeg_frame_pos = 0; // Reset buffer for new frame

        jpeg_dec_handle_t jpeg_dec = NULL;
        jpeg_dec_io_t* jpeg_io = NULL;
        jpeg_dec_header_info_t* out_info = NULL;
        uint8_t* output_buffer = NULL;
        int output_len = 0;

        jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
        config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE; // Set output to RGB565 for LVGL

        jpeg_error_t dec_ret = jpeg_dec_open(&config, &jpeg_dec);
        if (dec_ret != JPEG_ERR_OK) {
            ESP_LOGE(TAG, "Failed to open JPEG decoder: %d", dec_ret);
            goto CLOSE_SOCKET;
        }

        jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
        out_info = calloc(1, sizeof(jpeg_dec_header_info_t));
        if (jpeg_io == NULL || out_info == NULL) {
            ESP_LOGE(TAG, "Failed to allocate decoder structures");
            dec_ret = JPEG_ERR_NO_MEM;
            goto DECODE_CLEAN_UP;
        }

        bool header_parsed = false;

        do {
            len = recv(sock, rx_buffer, sizeof(rx_buffer), 0);
            if (len < 0) {
                ESP_LOGE(TAG, "Error occurred during receive: errno %d", errno);
                break;
            } else if (len == 0) {
                ESP_LOGW(TAG, "Connection closed");
                break;
            } else {
                // Append received data to JPEG frame buffer
                if (s_jpeg_frame_pos + len > MAX_JPEG_FRAME_SIZE) {
                    ESP_LOGE(TAG, "JPEG frame buffer overflow!");
                    break; // Error or incomplete frame
                }
                memcpy(s_jpeg_frame_buffer + s_jpeg_frame_pos, rx_buffer, len);
                s_jpeg_frame_pos += len;

                if (!header_parsed) {
                    jpeg_io->inbuf = s_jpeg_frame_buffer;
                    jpeg_io->inbuf_len = s_jpeg_frame_pos;
                    dec_ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
                    if (dec_ret == JPEG_ERR_OK) {
                        header_parsed = true;
                        ESP_LOGI(TAG, "JPEG Header parsed: Width=%d, Height=%d", out_info->width, out_info->height);

                        // Calculate output buffer size
                        if (config.output_type == JPEG_PIXEL_FORMAT_RGB565_LE ||
                            config.output_type == JPEG_PIXEL_FORMAT_RGB565_BE ||
                            config.output_type == JPEG_PIXEL_FORMAT_CbYCrY) {
                            output_len = out_info->width * out_info->height * 2;
                        } else if (config.output_type == JPEG_PIXEL_FORMAT_RGB888) {
                            output_len = out_info->width * out_info->height * 3;
                        } else {
                            ESP_LOGE(TAG, "Unsupported output format");
                            dec_ret = JPEG_ERR_UNSUPPORT_FMT;
                            break;
                        }

                        output_buffer = jpeg_calloc_align(output_len, 16);
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate output buffer");
                            dec_ret = JPEG_ERR_NO_MEM;
                            break;
                        }
                        jpeg_io->outbuf = output_buffer;

                    } else if (dec_ret != JPEG_ERR_NO_MORE_DATA) {
                        ESP_LOGE(TAG, "Failed to parse JPEG header: %d", dec_ret);
                        break;
                    }
                }

                if (header_parsed) {
                    jpeg_io->inbuf = s_jpeg_frame_buffer;
                    jpeg_io->inbuf_len = s_jpeg_frame_pos;
                    dec_ret = jpeg_dec_process(jpeg_dec, jpeg_io);
                    if (dec_ret == JPEG_ERR_OK) {
                        ESP_LOGI(TAG, "JPEG Decoded successfully!");
                        handle_decoded_image(output_buffer, out_info->width, out_info->height, config.output_type);
                        s_jpeg_frame_pos = 0; // Reset for next frame
                    } else if (dec_ret != JPEG_ERR_NO_MORE_DATA) {
                        ESP_LOGE(TAG, "Failed to decode JPEG data: %d", dec_ret);
                        break;
                    }
                }
            }
        } while (s_server_running);

    DECODE_CLEAN_UP:
        jpeg_dec_close(jpeg_dec);
        if (jpeg_io)
            free(jpeg_io);
        if (out_info)
            free(out_info);
        if (output_buffer)
            jpeg_free_align(output_buffer);

    CLOSE_SOCKET:
        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    s_server_running = false;
    if (s_jpeg_frame_buffer) {
        free(s_jpeg_frame_buffer);
        s_jpeg_frame_buffer = NULL;
    }
    vTaskDelete(NULL);
}

static void handle_decoded_image(uint8_t* img_buf, int width, int height, jpeg_pixel_format_t format) {
    // This function would typically display the image on a screen
    // For now, we just log the information.
    ESP_LOGI(TAG, "Decoded Image: Width=%d, Height=%d, Format=%d", width, height, format);
    // You can add your display logic here, e.g., send to LVGL or an LCD driver
    ui_image_transfer_set_image_data(img_buf, width, height, format);
}

bool wifi_image_transfer_start(uint16_t port) {
    if (s_server_running) {
        ESP_LOGW(TAG, "TCP server already running.");
        return true;
    }

    // Start the TCP server task
    // Task stack size might need adjustment based on image size and processing
    if (xTaskCreate(tcp_server_task, "tcp_server", 8192, &port, 5, &s_tcp_server_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create TCP server task");
        return false;
    }
    return true;
}

void wifi_image_transfer_stop(void) {
    if (s_server_running) {
        s_server_running = false;
        // Give some time for the task to finish gracefully
        vTaskDelay(pdMS_TO_TICKS(100));
        if (s_tcp_server_task_handle != NULL) {
            vTaskDelete(s_tcp_server_task_handle);
            s_tcp_server_task_handle = NULL;
        }
        ESP_LOGI(TAG, "TCP server stopped.");
    } else {
        ESP_LOGW(TAG, "TCP server not running.");
    }
}