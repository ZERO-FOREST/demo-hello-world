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

void audio_receiver_stop(void);

static const char* TAG = "AUDIO_RECEIVER";

#define TCP_PORT 7557
#define BUFFER_SIZE (1024 * 128)  // 每个缓冲区大小
#define SAMPLE_RATE 44100  // 默认采样率，假设与Python一致

static int server_sock = -1;
static int client_sock = -1;
static bool server_running = false;
static TaskHandle_t playback_task_handle = NULL;
static TaskHandle_t tcp_server_task_handle = NULL;
static TaskHandle_t tcp_receive_task_handle = NULL;
static RingbufHandle_t audio_ringbuf = NULL;

// I2S播放任务
static void i2s_playback_task(void* arg) {
    while (server_running) {
        size_t item_size;
        const uint8_t* item = xRingbufferReceive(audio_ringbuf, &item_size, portMAX_DELAY);
        if (item) {
            size_t bytes_written = 0;
            esp_err_t ret = i2s_tdm_write((const void*)item, item_size, &bytes_written);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
            } else if (bytes_written > 0) {
                ESP_LOGD(TAG, "Wrote %d bytes to I2S", bytes_written);
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
    uint8_t* rx_buffer = (uint8_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (!rx_buffer) {
        ESP_LOGE(TAG, "Failed to allocate rx_buffer for TCP receive task from PSRAM");
        goto cleanup;
    }

    while (server_running && sock >= 0) {
        int len = recv(sock, rx_buffer, BUFFER_SIZE, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "Recv failed: errno %d", errno);
            break;
        } else if (len == 0) {
            ESP_LOGI(TAG, "Connection closed");
            break;
        } else {
            BaseType_t done = pdFALSE;
            while (done != pdTRUE && server_running) { // 添加server_running检查以允许外部停止
                done = xRingbufferSend(audio_ringbuf, rx_buffer, len, pdMS_TO_TICKS(100));
                if (!done) {
                    ESP_LOGW(TAG, "Ringbuffer full, waiting...");
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
        }
    }

cleanup:
    if (rx_buffer) {
        heap_caps_free(rx_buffer);
    }
    ESP_LOGI(TAG, "Client disconnected.");
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
    ESP_LOGI(TAG, "Socket created");

    int err = bind(server_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto error;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", TCP_PORT);

    err = listen(server_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto error;
    }
    ESP_LOGI(TAG, "Socket listening...");

    while (server_running) {
        struct sockaddr_in source_addr;
        uint32_t addr_len = sizeof(source_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&source_addr, &addr_len);
        if (client_sock < 0) {
            if (!server_running) {
                ESP_LOGI(TAG, "Server stopping.");
                break;
            }
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Connection accepted!");

        if (tcp_receive_task_handle != NULL) {
            ESP_LOGW(TAG, "Closing previous connection to accept new one.");
            vTaskDelete(tcp_receive_task_handle);
            tcp_receive_task_handle = NULL;
        }
        xTaskCreatePinnedToCore(tcp_receive_task, "tcp_receive", 4096, (void*)(intptr_t)client_sock, 5, &tcp_receive_task_handle, 1);
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
        ESP_LOGW(TAG, "Server already running");
        return ESP_OK;
    }
    server_running = true;

    // 初始化环形缓冲区（双缓冲效果）- 在PSRAM中
    audio_ringbuf = xRingbufferCreateWithCaps(BUFFER_SIZE * 2, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);
    if (!audio_ringbuf) {
        ESP_LOGE(TAG, "Failed to create ring buffer in PSRAM");
        server_running = false;
        return ESP_FAIL;
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

    // Pre-fill the DMA buffer with silence to avoid DC offset and speaker heat-up
    const size_t silence_buffer_size = 1024;
    uint8_t* silence_buffer = (uint8_t*)calloc(1, silence_buffer_size);
    if (silence_buffer) {
        size_t bytes_written = 0;
        i2s_tdm_write(silence_buffer, silence_buffer_size, &bytes_written);
        free(silence_buffer);
        ESP_LOGI(TAG, "I2S TX buffer pre-filled with %d bytes of silence.", (int)bytes_written);
    } else {
        ESP_LOGE(TAG, "Failed to allocate silence buffer.");
    }

    // 创建播放任务
    if (playback_task_handle == NULL) {
        xTaskCreatePinnedToCore(i2s_playback_task, "i2s_playback", 4096, NULL, 5, &playback_task_handle, 1);
    }

    // 创建TCP服务器任务
    if (tcp_server_task_handle == NULL) {
        xTaskCreatePinnedToCore(tcp_server_task, "tcp_server", 4096, NULL, 5, &tcp_server_task_handle, 1);
    }

    ESP_LOGI(TAG, "Audio receiver service started");
    return ESP_OK;
}

void audio_receiver_stop(void) {
    if (!server_running) {
        return;
    }
    server_running = false;

    if (server_sock != -1) {
        close(server_sock);
        server_sock = -1;
    }
    
    // 任务将自行删除
    // tcp_server_task_handle
    // tcp_receive_task_handle
    // playback_task_handle
    
    if (audio_ringbuf) {
        // 发送一个虚拟项目以唤醒正在等待的 xRingbufferReceive
        xRingbufferSend(audio_ringbuf, & (uint8_t) {0}, 1, pdMS_TO_TICKS(10));
        vTaskDelay(pdMS_TO_TICKS(20)); // 等待任务响应
        vRingbufferDelete(audio_ringbuf);
        audio_ringbuf = NULL;
    }

    i2s_tdm_stop();
    i2s_tdm_deinit();
    ESP_LOGI(TAG, "Audio receiver stopped");
}