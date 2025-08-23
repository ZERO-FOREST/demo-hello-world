#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include <errno.h>
#include <lwip/netdb.h>
#include <stdlib.h>
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
#define FRAME_QUEUE_SIZE 3 // Queue size for frame data

// Structure to hold frame data
typedef struct {
    uint8_t* data;
    size_t size;
} frame_data_t;

static TaskHandle_t s_tcp_server_task_handle = NULL;
static TaskHandle_t s_jpeg_decode_task_handle = NULL;
static bool s_server_running = false;
static int s_listen_sock = -1;
static QueueHandle_t s_frame_queue = NULL;

// Forward declarations
static void handle_decoded_image(uint8_t* img_buf, int width, int height, jpeg_pixel_format_t format);
static void tcp_recv_task(void* pvParameters);
static void jpeg_decode_task(void* pvParameters);

static void tcp_recv_task(void* pvParameters) {
    uint16_t port = *(uint16_t*)pvParameters;
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    struct sockaddr_in dest_addr;

    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    ip_protocol = IPPROTO_IP;

    s_listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (s_listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        s_server_running = false;
        s_tcp_server_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket created");

    // Set socket options to allow address reuse
    int opt = 1;
    if (setsockopt(s_listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ESP_LOGE(TAG, "Failed to set SO_REUSEADDR: errno %d", errno);
        close(s_listen_sock);
        s_server_running = false;
        s_tcp_server_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // Also set SO_REUSEPORT if available (helps with port reuse)
    opt = 1;
    if (setsockopt(s_listen_sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        ESP_LOGW(TAG, "Failed to set SO_REUSEPORT (may not be supported): errno %d", errno);
        // This is not critical, continue anyway
    }

    int err = bind(s_listen_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(s_listen_sock);
        s_server_running = false;
        s_tcp_server_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", port);

    err = listen(s_listen_sock, LISTEN_SOCKET_NUM);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        close(s_listen_sock);
        s_server_running = false;
        s_tcp_server_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket listening");

    s_server_running = true;

    while (s_server_running) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(s_listen_sock, (struct sockaddr*)&source_addr, &addr_len);
        if (sock < 0) {
            if (s_server_running) {
                ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            }
            // If server is stopping, this is expected - don't log error
            continue;
        }

        // Convert ip address to string
        if (addr_family == AF_INET) {
            inet_ntoa_r(source_addr.sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGI(TAG, "Socket accepted IP address: %s", addr_str);

        // Set socket timeout to detect disconnections
        struct timeval timeout;
        timeout.tv_sec = 2; // 2 second timeout
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        uint8_t rx_buffer[TCP_RECV_BUF_SIZE];
        uint8_t* jpeg_frame_buffer = (uint8_t*)heap_caps_malloc(MAX_JPEG_FRAME_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (jpeg_frame_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate JPEG frame buffer");
            close(sock);
            continue;
        }

        int jpeg_frame_pos = 0;
        int len;

        while (s_server_running) {
            jpeg_frame_pos = 0; // Reset buffer for new frame
            
            // Receive complete JPEG data first
            bool frame_complete = false;
            bool jpeg_started = false;
            int frame_start_pos = 0;

            do {
                len = recv(sock, rx_buffer, sizeof(rx_buffer), 0);
                if (len < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        ESP_LOGW(TAG, "Receive timeout, client may have disconnected");
                    } else {
                        ESP_LOGE(TAG, "Error occurred during receive: errno %d", errno);
                    }
                    break;
                } else if (len == 0) {
                    ESP_LOGW(TAG, "Connection closed by client");
                    break;
                } else {
                    // Append received data to JPEG frame buffer
                    if (jpeg_frame_pos + len > MAX_JPEG_FRAME_SIZE) {
                        ESP_LOGE(TAG, "JPEG frame buffer overflow! Resetting buffer.");
                        jpeg_frame_pos = 0;
                        jpeg_started = false;
                        break;
                    }
                    memcpy(jpeg_frame_buffer + jpeg_frame_pos, rx_buffer, len);
                    jpeg_frame_pos += len;

                    // Look for JPEG start marker (0xFF 0xD8) if not found yet
                    if (!jpeg_started && jpeg_frame_pos >= 2) {
                        for (int i = 0; i <= jpeg_frame_pos - 2; i++) {
                            if (jpeg_frame_buffer[i] == 0xFF && jpeg_frame_buffer[i + 1] == 0xD8) {
                                frame_start_pos = i;
                                jpeg_started = true;
                                // Move data to start of buffer if JPEG doesn't start at beginning
                                if (frame_start_pos > 0) {
                                    memmove(jpeg_frame_buffer, jpeg_frame_buffer + frame_start_pos,
                                            jpeg_frame_pos - frame_start_pos);
                                    jpeg_frame_pos -= frame_start_pos;
                                }
                                break;
                            }
                        }
                    }

                    // Look for JPEG end marker (0xFF 0xD9) only after start is found
                    if (jpeg_started && jpeg_frame_pos >= 4) {          // Need at least SOI + minimal data + EOI
                        for (int i = 2; i <= jpeg_frame_pos - 2; i++) { // Start from position 2 to avoid matching SOI
                            if (jpeg_frame_buffer[i] == 0xFF && jpeg_frame_buffer[i + 1] == 0xD9) {
                                frame_complete = true;
                                jpeg_frame_pos = i + 2; // Include EOI marker

                                // Only log every 10th frame to reduce spam
                                static int frame_count = 0;
                                if (++frame_count % 10 == 0) {
                                    ESP_LOGI(TAG, "Frame #%d received, size: %d bytes", frame_count, jpeg_frame_pos);
                                }
                                break;
                            }
                        }
                    }
                }
            } while (!frame_complete && s_server_running);

            // Send complete frame to decode queue
            if (frame_complete && jpeg_frame_pos > 0 && jpeg_started) {
                // Validate JPEG frame structure
                if (jpeg_frame_pos < 4 || jpeg_frame_buffer[0] != 0xFF || jpeg_frame_buffer[1] != 0xD8 ||
                    jpeg_frame_buffer[jpeg_frame_pos - 2] != 0xFF ||
                    jpeg_frame_buffer[jpeg_frame_pos - 1] != 0xD9) {
                    ESP_LOGE(TAG, "Invalid JPEG frame structure");
                    continue;
                }

                // Allocate frame data structure
                frame_data_t* frame = malloc(sizeof(frame_data_t));
                if (frame != NULL) {
                    frame->data = malloc(jpeg_frame_pos);
                    if (frame->data != NULL) {
                        memcpy(frame->data, jpeg_frame_buffer, jpeg_frame_pos);
                        frame->size = jpeg_frame_pos;
                        
                        // Send to decode queue (non-blocking)
                        if (xQueueSend(s_frame_queue, &frame, 0) != pdTRUE) {
                            // Queue full, drop this frame
                            free(frame->data);
                            free(frame);
                            ESP_LOGW(TAG, "Frame queue full, dropping frame");
                        }
                    } else {
                        free(frame);
                        ESP_LOGE(TAG, "Failed to allocate frame data");
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to allocate frame structure");
                }
            }

            // If we didn't get a complete frame or had an error, break the loop
            if (!frame_complete || len <= 0) {
                break;
            }
        } // End of frame processing loop

        free(jpeg_frame_buffer);
        shutdown(sock, 0);
        close(sock);
    }

    if (s_listen_sock >= 0) {
        close(s_listen_sock);
        s_listen_sock = -1;
    }
    s_server_running = false;
    s_tcp_server_task_handle = NULL;
    vTaskDelete(NULL);
}

static void jpeg_decode_task(void* pvParameters) {
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_BE; // Set output to RGB565 Big Endian for LVGL

    jpeg_error_t dec_ret;
    jpeg_dec_handle_t jpeg_dec = NULL;
    dec_ret = jpeg_dec_open(&config, &jpeg_dec);
    if (dec_ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "Failed to open JPEG decoder: %d", dec_ret);
        s_jpeg_decode_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "JPEG decode task started");

    while (s_server_running) {
        frame_data_t* frame = NULL;
        
        // Wait for frame data from queue with shorter timeout for better responsiveness
        if (xQueueReceive(s_frame_queue, &frame, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (frame != NULL && frame->data != NULL && frame->size > 0) {
                
                jpeg_dec_io_t* jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
                jpeg_dec_header_info_t* out_info = calloc(1, sizeof(jpeg_dec_header_info_t));
                uint8_t* output_buffer = NULL;
                int output_len = 0;

                if (jpeg_io != NULL && out_info != NULL) {
                    // Set input buffer for complete frame
                    jpeg_io->inbuf = frame->data;
                    jpeg_io->inbuf_len = frame->size;

                    // Parse JPEG header
                    dec_ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
                    if (dec_ret == JPEG_ERR_OK) {
                        // Only log header info for first frame or when dimensions change
                        static int last_width = 0, last_height = 0;
                        if (out_info->width != last_width || out_info->height != last_height) {
                            ESP_LOGI(TAG, "JPEG Header parsed: Width=%d, Height=%d", out_info->width, out_info->height);
                            last_width = out_info->width;
                            last_height = out_info->height;
                        }

                        // Calculate output buffer size
                        if (config.output_type == JPEG_PIXEL_FORMAT_RGB565_LE ||
                            config.output_type == JPEG_PIXEL_FORMAT_RGB565_BE ||
                            config.output_type == JPEG_PIXEL_FORMAT_CbYCrY) {
                            output_len = out_info->width * out_info->height * 2;
                        } else if (config.output_type == JPEG_PIXEL_FORMAT_RGB888) {
                            output_len = out_info->width * out_info->height * 3;
                        } else {
                            ESP_LOGE(TAG, "Unsupported output format");
                        }

                        if (output_len > 0) {
                            output_buffer = jpeg_calloc_align(output_len, 16);
                            if (output_buffer != NULL) {
                                jpeg_io->outbuf = output_buffer;

                                // Decode the complete JPEG frame
                                dec_ret = jpeg_dec_process(jpeg_dec, jpeg_io);
                                if (dec_ret == JPEG_ERR_OK) {
                                    handle_decoded_image(output_buffer, out_info->width, out_info->height, config.output_type);
                                } else {
                                    ESP_LOGE(TAG, "Failed to decode JPEG data: %d", dec_ret);
                                }

                                jpeg_free_align(output_buffer);
                            } else {
                                ESP_LOGE(TAG, "Failed to allocate output buffer");
                            }
                        }
                    } else {
                        ESP_LOGE(TAG, "Failed to parse JPEG header: %d", dec_ret);
                    }
                }

                // Clean up
                if (jpeg_io) free(jpeg_io);
                if (out_info) free(out_info);
                
                // Free frame data
                free(frame->data);
                free(frame);
            }
        }
    }

    // Clean up decoder
    if (jpeg_dec) {
        jpeg_dec_close(jpeg_dec);
    }

    ESP_LOGI(TAG, "JPEG decode task stopped");
    s_jpeg_decode_task_handle = NULL;
    vTaskDelete(NULL);
}

static void handle_decoded_image(uint8_t* img_buf, int width, int height, jpeg_pixel_format_t format) {
    // This function now directly calls the UI update function
    ui_image_transfer_set_image_data(img_buf, width, height, format);
}

bool wifi_image_transfer_start(uint16_t port) {
    if (s_server_running) {
        ESP_LOGW(TAG, "TCP server already running.");
        return true;
    }

    // Ensure clean state before starting
    if (s_listen_sock >= 0) {
        close(s_listen_sock);
        s_listen_sock = -1;
    }

    // Create frame queue
    s_frame_queue = xQueueCreate(FRAME_QUEUE_SIZE, sizeof(frame_data_t*));
    if (s_frame_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create frame queue");
        return false;
    }

    s_tcp_server_task_handle = NULL;
    s_jpeg_decode_task_handle = NULL;

    // Start the JPEG decode task first
    if (xTaskCreate(jpeg_decode_task, "jpeg_decode", 8192, NULL, 4, &s_jpeg_decode_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create JPEG decode task");
        vQueueDelete(s_frame_queue);
        s_frame_queue = NULL;
        return false;
    }

    // Start the TCP receive task
    if (xTaskCreate(tcp_recv_task, "tcp_recv", 8192, &port, 5, &s_tcp_server_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create TCP receive task");
        
        // Stop decode task
        s_server_running = false;
        if (s_jpeg_decode_task_handle != NULL) {
            vTaskDelete(s_jpeg_decode_task_handle);
            s_jpeg_decode_task_handle = NULL;
        }
        
        vQueueDelete(s_frame_queue);
        s_frame_queue = NULL;
        return false;
    }
    
    return true;
}

void wifi_image_transfer_stop(void) {
    if (s_server_running) {
        ESP_LOGI(TAG, "Stopping TCP server...");
        s_server_running = false;

        // Close the listen socket first to unblock accept()
        if (s_listen_sock >= 0) {
            ESP_LOGI(TAG, "Closing listen socket");
            close(s_listen_sock);
            s_listen_sock = -1;
        }

        // Give tasks a moment to recognize the stop signal
        vTaskDelay(pdMS_TO_TICKS(50));

        // Wait for the TCP receive task to finish gracefully
        if (s_tcp_server_task_handle != NULL) {
            // Wait up to 1 second for the task to terminate
            for (int i = 0; i < 100 && s_tcp_server_task_handle != NULL; i++) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            // If task still exists, force delete it
            if (s_tcp_server_task_handle != NULL) {
                ESP_LOGW(TAG, "Force deleting TCP receive task");
                vTaskDelete(s_tcp_server_task_handle);
                s_tcp_server_task_handle = NULL;
            }
        }

        // Wait for the JPEG decode task to finish gracefully
        if (s_jpeg_decode_task_handle != NULL) {
            // Wait up to 1 second for the task to terminate
            for (int i = 0; i < 100 && s_jpeg_decode_task_handle != NULL; i++) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            // If task still exists, force delete it
            if (s_jpeg_decode_task_handle != NULL) {
                ESP_LOGW(TAG, "Force deleting JPEG decode task");
                vTaskDelete(s_jpeg_decode_task_handle);
                s_jpeg_decode_task_handle = NULL;
            }
        }

        // Clean up frame queue
        if (s_frame_queue != NULL) {
            // Free any remaining frames in queue
            frame_data_t* frame;
            while (xQueueReceive(s_frame_queue, &frame, 0) == pdTRUE) {
                if (frame != NULL) {
                    if (frame->data != NULL) {
                        free(frame->data);
                    }
                    free(frame);
                }
            }
            vQueueDelete(s_frame_queue);
            s_frame_queue = NULL;
        }

        ESP_LOGI(TAG, "TCP server stopped.");
    } else {
        ESP_LOGW(TAG, "TCP server not running.");
    }
}