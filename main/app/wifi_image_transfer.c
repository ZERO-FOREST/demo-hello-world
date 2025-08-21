#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

static TaskHandle_t s_tcp_server_task_handle = NULL;
static bool s_server_running = false;
static uint8_t* s_jpeg_frame_buffer = NULL;
static int s_jpeg_frame_pos = 0;
static int s_listen_sock = -1;

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
        s_tcp_server_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }
    s_jpeg_frame_pos = 0;

    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    ip_protocol = IPPROTO_IP;

    s_listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (s_listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        s_server_running = false;
        free(s_jpeg_frame_buffer);
        s_jpeg_frame_buffer = NULL;
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
        free(s_jpeg_frame_buffer);
        s_jpeg_frame_buffer = NULL;
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
        free(s_jpeg_frame_buffer);
        s_jpeg_frame_buffer = NULL;
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
        free(s_jpeg_frame_buffer);
        s_jpeg_frame_buffer = NULL;
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
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
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

        int len;
        uint8_t rx_buffer[TCP_RECV_BUF_SIZE];

        jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
        config.output_type = JPEG_PIXEL_FORMAT_RGB565_BE; // Set output to RGB565 Big Endian for LVGL

        jpeg_error_t dec_ret;
        jpeg_dec_handle_t jpeg_dec = NULL;
        dec_ret = jpeg_dec_open(&config, &jpeg_dec);
        if (dec_ret != JPEG_ERR_OK) {
            ESP_LOGE(TAG, "Failed to open JPEG decoder: %d", dec_ret);
            goto CLOSE_SOCKET;
        }

        // Process multiple frames on the same connection
        int consecutive_failures = 0;
        const int MAX_CONSECUTIVE_FAILURES = 10;

        while (s_server_running) {
            s_jpeg_frame_pos = 0; // Reset buffer for new frame

            jpeg_dec_io_t* jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
            jpeg_dec_header_info_t* out_info = calloc(1, sizeof(jpeg_dec_header_info_t));
            uint8_t* output_buffer = NULL;
            int output_len = 0;

            if (jpeg_io == NULL || out_info == NULL) {
                ESP_LOGE(TAG, "Failed to allocate decoder structures");
                if (jpeg_io)
                    free(jpeg_io);
                if (out_info)
                    free(out_info);
                break;
            }

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
                    if (s_jpeg_frame_pos + len > MAX_JPEG_FRAME_SIZE) {
                        ESP_LOGE(TAG, "JPEG frame buffer overflow! Resetting buffer.");
                        s_jpeg_frame_pos = 0;
                        jpeg_started = false;
                        break;
                    }
                    memcpy(s_jpeg_frame_buffer + s_jpeg_frame_pos, rx_buffer, len);
                    s_jpeg_frame_pos += len;

                    // Look for JPEG start marker (0xFF 0xD8) if not found yet
                    if (!jpeg_started && s_jpeg_frame_pos >= 2) {
                        for (int i = 0; i <= s_jpeg_frame_pos - 2; i++) {
                            if (s_jpeg_frame_buffer[i] == 0xFF && s_jpeg_frame_buffer[i + 1] == 0xD8) {
                                frame_start_pos = i;
                                jpeg_started = true;
                                // Move data to start of buffer if JPEG doesn't start at beginning
                                if (frame_start_pos > 0) {
                                    memmove(s_jpeg_frame_buffer, s_jpeg_frame_buffer + frame_start_pos,
                                            s_jpeg_frame_pos - frame_start_pos);
                                    s_jpeg_frame_pos -= frame_start_pos;
                                }
                                break;
                            }
                        }
                    }

                    // Look for JPEG end marker (0xFF 0xD9) only after start is found
                    if (jpeg_started && s_jpeg_frame_pos >= 4) {          // Need at least SOI + minimal data + EOI
                        for (int i = 2; i <= s_jpeg_frame_pos - 2; i++) { // Start from position 2 to avoid matching SOI
                            if (s_jpeg_frame_buffer[i] == 0xFF && s_jpeg_frame_buffer[i + 1] == 0xD9) {
                                frame_complete = true;
                                s_jpeg_frame_pos = i + 2; // Include EOI marker

                                // Only log every 10th frame to reduce spam
                                static int frame_count = 0;
                                if (++frame_count % 10 == 0) {
                                    ESP_LOGI(TAG, "Frame #%d received, size: %d bytes", frame_count, s_jpeg_frame_pos);
                                }
                                break;
                            }
                        }
                    }
                }
            } while (!frame_complete && s_server_running);

            // Now decode the complete JPEG frame if we have one
            if (frame_complete && s_jpeg_frame_pos > 0 && jpeg_started) {
                // Validate JPEG frame structure
                if (s_jpeg_frame_pos < 4 || s_jpeg_frame_buffer[0] != 0xFF || s_jpeg_frame_buffer[1] != 0xD8 ||
                    s_jpeg_frame_buffer[s_jpeg_frame_pos - 2] != 0xFF ||
                    s_jpeg_frame_buffer[s_jpeg_frame_pos - 1] != 0xD9) {
                    ESP_LOGE(TAG, "Invalid JPEG frame structure");
                    // Send NACK and continue
                    uint8_t nack = 0x15;
                    send(sock, &nack, 1, 0);
                    consecutive_failures++;
                    if (jpeg_io)
                        free(jpeg_io);
                    if (out_info)
                        free(out_info);
                    continue;
                }

                // Set input buffer for complete frame
                jpeg_io->inbuf = s_jpeg_frame_buffer;
                jpeg_io->inbuf_len = s_jpeg_frame_pos;

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
                        if (jpeg_io)
                            free(jpeg_io);
                        if (out_info)
                            free(out_info);
                        break;
                    }

                    output_buffer = jpeg_calloc_align(output_len, 16);
                    if (output_buffer == NULL) {
                        ESP_LOGE(TAG, "Failed to allocate output buffer");
                        if (jpeg_io)
                            free(jpeg_io);
                        if (out_info)
                            free(out_info);
                        break;
                    }
                    jpeg_io->outbuf = output_buffer;

                    // Decode the complete JPEG frame
                    dec_ret = jpeg_dec_process(jpeg_dec, jpeg_io);
                    if (dec_ret == JPEG_ERR_OK) {
                        // Remove success log spam - only log errors
                        handle_decoded_image(output_buffer, out_info->width, out_info->height, config.output_type);

                        // Send ACK to client
                        uint8_t ack = 0x06;
                        send(sock, &ack, 1, 0);

                        // Reset failure counter on success
                        consecutive_failures = 0;

                        // Add a small delay to control frame rate
                        vTaskDelay(pdMS_TO_TICKS(33)); // ~30 FPS
                    } else {
                        ESP_LOGE(TAG, "Failed to decode JPEG data: %d", dec_ret);
                        // Send NACK to client
                        uint8_t nack = 0x15;
                        send(sock, &nack, 1, 0);
                        consecutive_failures++;
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to parse JPEG header: %d", dec_ret);
                    // Send NACK to client
                    uint8_t nack = 0x15;
                    send(sock, &nack, 1, 0);
                    consecutive_failures++;
                }
            } else if (s_jpeg_frame_pos > 0 || !jpeg_started) {
                // Incomplete frame or invalid data, send NACK
                uint8_t nack = 0x15;
                send(sock, &nack, 1, 0);
                ESP_LOGW(TAG, "Incomplete or invalid frame data received (pos: %d, started: %d)", s_jpeg_frame_pos,
                         jpeg_started);
                consecutive_failures++;
            }

            // Clean up this frame's resources
            if (jpeg_io)
                free(jpeg_io);
            if (out_info)
                free(out_info);
            if (output_buffer)
                jpeg_free_align(output_buffer);

            // If we didn't get a complete frame or had an error, break the loop
            if (!frame_complete || len <= 0) {
                break;
            }

            // If too many consecutive failures, break the connection to reset
            if (consecutive_failures >= MAX_CONSECUTIVE_FAILURES) {
                ESP_LOGW(TAG, "Too many consecutive failures (%d), closing connection for reset", consecutive_failures);
                break;
            }
        } // End of frame processing loop

        // Clean up decoder
        if (jpeg_dec) {
            jpeg_dec_close(jpeg_dec);
        }

    CLOSE_SOCKET:
        shutdown(sock, 0);
        close(sock);
    }

    if (s_listen_sock >= 0) {
        close(s_listen_sock);
        s_listen_sock = -1;
    }
    s_server_running = false;
    if (s_jpeg_frame_buffer) {
        free(s_jpeg_frame_buffer);
        s_jpeg_frame_buffer = NULL;
    }
    s_tcp_server_task_handle = NULL;
    vTaskDelete(NULL);
}

static void handle_decoded_image(uint8_t* img_buf, int width, int height, jpeg_pixel_format_t format) {
    // This function would typically display the image on a screen
    // Only log image info on first frame or when dimensions change
    static int last_w = 0, last_h = 0;
    static int frame_num = 0;
    frame_num++;

    if (width != last_w || height != last_h || frame_num == 1) {
        ESP_LOGI(TAG, "Decoded Image: Width=%d, Height=%d, Format=%d (Frame #%d)", width, height, format, frame_num);
        last_w = width;
        last_h = height;
    }

    // You can add your display logic here, e.g., send to LVGL or an LCD driver
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

    if (s_jpeg_frame_buffer != NULL) {
        free(s_jpeg_frame_buffer);
        s_jpeg_frame_buffer = NULL;
    }

    s_jpeg_frame_pos = 0;
    s_tcp_server_task_handle = NULL;

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
        ESP_LOGI(TAG, "Stopping TCP server...");
        s_server_running = false;

        // Close the listen socket first to unblock accept()
        if (s_listen_sock >= 0) {
            ESP_LOGI(TAG, "Closing listen socket");
            close(s_listen_sock);
            s_listen_sock = -1;
        }

        // Wait for the task to finish gracefully
        if (s_tcp_server_task_handle != NULL) {
            // Wait up to 1 second for the task to terminate
            for (int i = 0; i < 100 && s_tcp_server_task_handle != NULL; i++) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            // If task still exists, force delete it
            if (s_tcp_server_task_handle != NULL) {
                ESP_LOGW(TAG, "Force deleting TCP server task");
                vTaskDelete(s_tcp_server_task_handle);
                s_tcp_server_task_handle = NULL;
            }
        }

        // Ensure buffer is cleaned up
        if (s_jpeg_frame_buffer != NULL) {
            free(s_jpeg_frame_buffer);
            s_jpeg_frame_buffer = NULL;
        }

        ESP_LOGI(TAG, "TCP server stopped.");
    } else {
        ESP_LOGW(TAG, "TCP server not running.");
    }
}