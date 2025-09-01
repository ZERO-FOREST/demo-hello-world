/**
 * @file audio_receiver.c
 * @brief 音频接收和播放应用
 * @author Trae Builder
 * @date 2024
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "lwip/sockets.h"
#include "i2s_tdm.h"
#include <stdlib.h>
#include "esp_heap_caps.h"
#include <errno.h>
#include "../UI/inc/status_bar_manager.h"

void audio_receiver_stop(void);

static const char* TAG = "AUDIO_RECEIVER";

#define TCP_PORT 7557
#define BUFFER_SIZE (1024 * 256)  // 缓冲区大小到256KB
#define SAMPLE_RATE 44100

static int server_sock = -1;
static int client_sock = -1;
static bool server_running = false;
static bool audio_receiving = false;  // 音频接收状态标志
static TaskHandle_t playback_task_handle = NULL;
static TaskHandle_t tcp_server_task_handle = NULL;
static TaskHandle_t tcp_receive_task_handle = NULL;
static RingbufHandle_t audio_ringbuf = NULL;

// I2S播放任务
static void i2s_playback_task(void* arg) {
    while (server_running) {
        size_t item_size;
        const uint8_t* item = xRingbufferReceive(audio_ringbuf, &item_size, pdMS_TO_TICKS(100));
        if (item) {
            size_t bytes_written = 0;
            esp_err_t ret = i2s_tdm_write((const void*)item, item_size, &bytes_written);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
            }
            vRingbufferReturnItem(audio_ringbuf, (void*)item);
        }
    }
    playback_task_handle = NULL;
    vTaskDelete(NULL);
}

// TCP接收任务
static void tcp_receive_task(void* arg) {
    int sock = (int)(intptr_t)arg;
    int total_received = 0;
    
    char *rx_buffer = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (!rx_buffer) {
        ESP_LOGE(TAG, "Failed to allocate rx_buffer from PSRAM");
        rx_buffer = malloc(BUFFER_SIZE);
        if (!rx_buffer) {
            ESP_LOGE(TAG, "Failed to allocate rx_buffer from internal RAM");
            goto cleanup;
        }
    }
    
    while (server_running && sock >= 0) {
        int len = recv(sock, rx_buffer, BUFFER_SIZE, 0);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                vTaskDelay(pdMS_TO_TICKS(5)); // 短暂等待，避免忙等
                continue;
            }
            ESP_LOGE(TAG, "recv failed: errno %d", errno);
            break;
        } else if (len == 0) {
            ESP_LOGI(TAG, "Connection closed");
            break;
        } else {
            total_received += len;
            
            // 设置音频接收状态
            audio_receiving = true;
            
            // 更新状态栏显示音频接收状态
            status_bar_manager_set_audio_status(true);
            
            BaseType_t done = xRingbufferSend(audio_ringbuf, rx_buffer, len, pdMS_TO_TICKS(100));
            if (!done) {
                ESP_LOGW(TAG, "Ringbuffer full, dropping %d bytes", len);
            }
        }
    }

cleanup:
    // 连接断开时，更新状态
    audio_receiving = false;
    status_bar_manager_set_audio_status(false);
    
    if (rx_buffer) {
        free(rx_buffer);
    }
    close(sock);
    client_sock = -1;
    tcp_receive_task_handle = NULL;
    vTaskDelete(NULL);
}

// TCP服务器任务
static void tcp_server_task(void* arg) {
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(TCP_PORT);
    server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (server_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        goto error;
    }

    int err = bind(server_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto error;
    }

    err = listen(server_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto error;
    }

    while (server_running) {
        struct sockaddr_in source_addr;
        uint32_t addr_len = sizeof(source_addr);
        ESP_LOGI(TAG, "Socket listening...");
        client_sock = accept(server_sock, (struct sockaddr*)&source_addr, &addr_len);
        if (client_sock < 0) {
            if (!server_running) {
                break;
            }
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        ESP_LOGI(TAG, "Socket accepted connection");
        
        // 设置套接字为非阻塞
        int flags = fcntl(client_sock, F_GETFL, 0);
        fcntl(client_sock, F_SETFL, flags | O_NONBLOCK);

        // 创建独立的TCP接收任务来处理这个连接
        xTaskCreatePinnedToCore(tcp_receive_task, "tcp_receive", 4096, (void*)(intptr_t)client_sock, 5, &tcp_receive_task_handle, 0);
        
        // 等待接收任务完成
        while (tcp_receive_task_handle != NULL && server_running) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

error:
    if (server_sock != -1) {
        close(server_sock);
        server_sock = -1;
    }
    tcp_server_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t audio_receiver_start(void) {
    if (server_running) {
        return ESP_OK;
    }
    server_running = true;

    // 初始化环形缓冲区 - 256KB缓冲区分配到PSRAM
    audio_ringbuf = xRingbufferCreateWithCaps(BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);
    if (!audio_ringbuf) {
        audio_ringbuf = xRingbufferCreate(BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
        if (!audio_ringbuf) {
            ESP_LOGE(TAG, "Failed to create ring buffer");
            server_running = false;
            return ESP_FAIL;
        }
    }

    // 初始化I2S
    esp_err_t ret = i2s_tdm_init();
    if (ret != ESP_OK) {
        audio_receiver_stop();
        return ret;
    }
    ret = i2s_tdm_set_sample_rate(SAMPLE_RATE);
    if (ret != ESP_OK) {
        audio_receiver_stop();
        return ret;
    }
    ret = i2s_tdm_start();
    if (ret != ESP_OK) {
        audio_receiver_stop();
        return ret;
    }

    const size_t silence_buffer_size = 1024;
    uint8_t* silence_buffer = (uint8_t*)calloc(1, silence_buffer_size);
    if (silence_buffer) {
        size_t bytes_written = 0;
        i2s_tdm_write(silence_buffer, silence_buffer_size, &bytes_written);
        free(silence_buffer);
    }

    // 创建播放任务
    if (playback_task_handle == NULL) {
        xTaskCreatePinnedToCore(i2s_playback_task, "i2s_playback", 4096, NULL, 5, &playback_task_handle, 1);
    }

    // 创建TCP服务器任务
    if (tcp_server_task_handle == NULL) {
        xTaskCreatePinnedToCore(tcp_server_task, "tcp_server", 4096, NULL, 5, &tcp_server_task_handle, 1);
    }

    return ESP_OK;
}

void audio_receiver_stop(void) {
    if (!server_running) {
        return;
    }
    server_running = false;
    audio_receiving = false;

    // 更新状态栏为空闲状态
    status_bar_manager_set_audio_status(false);

    if (client_sock != -1) {
        close(client_sock);
        client_sock = -1;
    }
    if (server_sock != -1) {
        shutdown(server_sock, SHUT_RDWR);
        close(server_sock);
        server_sock = -1;
    }
    
    // 等待任务结束
    while(tcp_server_task_handle != NULL || tcp_receive_task_handle != NULL || playback_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (audio_ringbuf) {
        vRingbufferDelete(audio_ringbuf);
        audio_ringbuf = NULL;
    }

    i2s_tdm_stop();
    i2s_tdm_deinit();
    ESP_LOGI(TAG, "Audio receiver stopped");
}

bool audio_receiver_is_receiving(void) {
    return audio_receiving && server_running && (client_sock >= 0);
}