/**
 * @file serial_display.c
 * @brief 串口屏幕显示模块 - 通过WiFi TCP接收数据并发送到串口屏幕
 * @author Your Name
 * @date 2024
 */
#include "driver/gpio.h"
#include "driver/uart.h"
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
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "serial_display.h"
#include "ui_serial_display.h"

static const char* TAG = "SERIAL_DISPLAY";

// 串口配置
#define UART_NUM UART_NUM_1
#define UART_TX_PIN GPIO_NUM_17
#define UART_RX_PIN GPIO_NUM_16
#define UART_BAUD_RATE 115200
#define UART_BUF_SIZE 1024

// TCP服务器配置
#define LISTEN_SOCKET_NUM 1
#define TCP_RECV_BUF_SIZE 1024
#define MAX_DISPLAY_DATA_SIZE 4096

// 任务句柄
static TaskHandle_t s_tcp_server_task_handle = NULL;
static TaskHandle_t s_serial_task_handle = NULL;
static bool s_server_running = false;
static bool s_serial_running = false;
static bool s_stopping = false;         // 添加停止标志
static bool s_uart_initialized = false; // 添加UART初始化标志

// 数据缓冲区 - 使用PSRAM
static uint8_t* s_display_buffer = NULL;
static int s_buffer_size = 0;
static SemaphoreHandle_t s_buffer_mutex = NULL;
static bool s_buffer_initialized = false;

// 初始化PSRAM缓冲区
static esp_err_t init_psram_buffer(void) {
    if (s_buffer_initialized) {
        return ESP_OK;
    }

    // 分配PSRAM内存
    s_display_buffer = (uint8_t*)heap_caps_malloc(MAX_DISPLAY_DATA_SIZE, MALLOC_CAP_SPIRAM);
    if (s_display_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM buffer for display data");
        return ESP_ERR_NO_MEM;
    }

    // 初始化缓冲区
    memset(s_display_buffer, 0, MAX_DISPLAY_DATA_SIZE);
    s_buffer_size = 0;
    s_buffer_initialized = true;

    ESP_LOGI(TAG, "PSRAM display buffer initialized: %d bytes", MAX_DISPLAY_DATA_SIZE);
    return ESP_OK;
}

// 清理PSRAM缓冲区
static void cleanup_psram_buffer(void) {
    if (s_display_buffer != NULL) {
        heap_caps_free(s_display_buffer);
        s_display_buffer = NULL;
    }
    s_buffer_initialized = false;
    s_buffer_size = 0;
}

// 串口初始化
static esp_err_t serial_init(void) {
    // 检查UART是否已经初始化
    if (s_uart_initialized) {
        ESP_LOGI(TAG, "UART already initialized, skipping");
        return ESP_OK;
    }

    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    esp_err_t ret = uart_driver_install(UART_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_param_config(UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(ret));
        uart_driver_delete(UART_NUM); // 清理已安装的驱动
        return ret;
    }

    ret = uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(ret));
        uart_driver_delete(UART_NUM); // 清理已安装的驱动
        return ret;
    }

    s_uart_initialized = true;
    ESP_LOGI(TAG, "Serial port initialized: UART%d, TX:%d, RX:%d, Baud:%d", UART_NUM, UART_TX_PIN, UART_RX_PIN,
             UART_BAUD_RATE);
    return ESP_OK;
}

// 串口反初始化
static void serial_deinit(void) {
    if (s_uart_initialized) {
        uart_driver_delete(UART_NUM);
        s_uart_initialized = false;
        ESP_LOGI(TAG, "Serial port deinitialized");
    }
}

// 串口发送数据
static esp_err_t serial_send_data(const uint8_t* data, size_t len) {
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int written = uart_write_bytes(UART_NUM, data, len);
    if (written < 0) {
        ESP_LOGE(TAG, "uart_write_bytes failed");
        return ESP_FAIL;
    }

    // 串口数据发送完成
    return ESP_OK;
}

// 串口任务 - 处理串口数据发送
static void serial_task(void* pvParameters) {
    uint8_t* local_buffer = NULL;
    int local_size = 0;

    // 分配本地缓冲区
    local_buffer = (uint8_t*)heap_caps_malloc(MAX_DISPLAY_DATA_SIZE, MALLOC_CAP_8BIT);
    if (local_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate local buffer for serial task");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Serial task started");

    while (s_serial_running) {
        // 检查是否有新数据需要发送
        if (xSemaphoreTake(s_buffer_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (s_buffer_initialized && s_buffer_size > 0 && s_display_buffer != NULL) {
                // 复制数据到本地缓冲区
                local_size = s_buffer_size;
                memcpy(local_buffer, s_display_buffer, local_size);
                s_buffer_size = 0; // 清空缓冲区
                xSemaphoreGive(s_buffer_mutex);

                // 发送数据到串口
                esp_err_t ret = serial_send_data(local_buffer, local_size);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to send data to serial port");
                }
            } else {
                xSemaphoreGive(s_buffer_mutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms延迟
    }

    // 清理本地缓冲区
    if (local_buffer != NULL) {
        heap_caps_free(local_buffer);
    }

    ESP_LOGI(TAG, "Serial task stopped");
    vTaskDelete(NULL);
}

// TCP服务器任务
static void tcp_server_task(void* pvParameters) {
    uint16_t port = *(uint16_t*)pvParameters;
    uint16_t* port_param = (uint16_t*)pvParameters;
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    struct sockaddr_in dest_addr;

    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    ip_protocol = IPPROTO_IP;

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        s_server_running = false;
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
    ESP_LOGI(TAG, "Socket listening on port %d", port);

    s_server_running = true;

    while (s_server_running) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);

        // 使用select来设置超时，以便能够及时响应停止信号
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);
        FD_SET(listen_sock, &readfds);
        tv.tv_sec = 1; // 1秒超时
        tv.tv_usec = 0;

        int select_result = select(listen_sock + 1, &readfds, NULL, NULL, &tv);
        if (select_result < 0) {
            ESP_LOGE(TAG, "Select error: errno %d", errno);
            continue;
        } else if (select_result == 0) {
            // 超时，检查是否需要停止
            continue;
        }

        int sock = accept(listen_sock, (struct sockaddr*)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            continue;
        }

        // 转换IP地址为字符串
        if (addr_family == AF_INET) {
            inet_ntoa_r(source_addr.sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGI(TAG, "Socket accepted IP address: %s", addr_str);

        int len;
        uint8_t rx_buffer[TCP_RECV_BUF_SIZE];

        do {
            // 检查是否需要停止
            if (!s_server_running) {
                break;
            }

            len = recv(sock, rx_buffer, sizeof(rx_buffer), 0);
            if (len < 0) {
                ESP_LOGE(TAG, "Error occurred during receive: errno %d", errno);
                break;
            } else if (len == 0) {
                ESP_LOGW(TAG, "Connection closed");
                break;
            } else {
                ESP_LOGI(TAG, "Received %d bytes from TCP", len);

                // 将接收到的数据存储到显示缓冲区
                if (xSemaphoreTake(s_buffer_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    if (s_buffer_initialized && s_display_buffer != NULL && len <= MAX_DISPLAY_DATA_SIZE) {
                        memcpy(s_display_buffer, rx_buffer, len);
                        s_buffer_size = len;
                        ESP_LOGI(TAG, "Data buffered for serial transmission");
                    } else {
                        ESP_LOGE(TAG, "Buffer not initialized or data too large: %d bytes", len);
                    }
                    xSemaphoreGive(s_buffer_mutex);
                }
                // 将UI更新调用移出互斥锁，以避免阻塞其他任务
                ui_serial_display_add_data((const char*)rx_buffer, len);
            }
        } while (s_server_running);

        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    s_server_running = false;
    free(port_param);
    vTaskDelete(NULL);
}

// 公共API函数

esp_err_t serial_display_init(void) {
    esp_err_t ret;

    // 初始化PSRAM缓冲区
    ret = init_psram_buffer();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize PSRAM buffer");
        return ret;
    }

    // 初始化串口
    ret = serial_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize serial port");
        cleanup_psram_buffer();
        return ret;
    }

    // 创建互斥锁
    s_buffer_mutex = xSemaphoreCreateMutex();
    if (s_buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        cleanup_psram_buffer();
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Serial display module initialized");
    return ESP_OK;
}

bool serial_display_start(uint16_t port) {
    if (s_server_running) {
        ESP_LOGW(TAG, "TCP server already running");
        return true;
    }

    // 分配端口参数内存
    uint16_t* port_param = malloc(sizeof(uint16_t));
    if (port_param == NULL) {
        ESP_LOGE(TAG, "Failed to allocate port parameter");
        return false;
    }
    *port_param = port;

    // 启动串口任务
    s_serial_running = true;
    if (xTaskCreatePinnedToCore(serial_task, "serial_task", 4096, NULL, 4, &s_serial_task_handle, 1) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create serial task");
        s_serial_running = false;
        free(port_param);
        return false;
    }

    // 启动TCP服务器任务
    if (xTaskCreatePinnedToCore(tcp_server_task, "tcp_server", 4096, port_param, 4, &s_tcp_server_task_handle, 1) !=
        pdPASS) {
        ESP_LOGE(TAG, "Failed to create TCP server task");
        s_serial_running = false;
        vTaskDelete(s_serial_task_handle);
        s_serial_task_handle = NULL;
        free(port_param);
        return false;
    }

    ESP_LOGI(TAG, "Serial display started on port %d", port);
    return true;
}

void serial_display_stop(void) {
    // 防止重复调用
    if (s_stopping) {
        ESP_LOGW(TAG, "Serial display stop already in progress");
        return;
    }

    s_stopping = true;

    // 停止TCP服务器
    if (s_server_running) {
        s_server_running = false;
        vTaskDelay(pdMS_TO_TICKS(200));
        if (s_tcp_server_task_handle != NULL) {
            // 检查任务状态
            if (eTaskGetState(s_tcp_server_task_handle) != eDeleted) {
                vTaskDelete(s_tcp_server_task_handle);
            }
            s_tcp_server_task_handle = NULL;
        }
    }

    // 停止串口任务
    if (s_serial_running) {
        s_serial_running = false;
        vTaskDelay(pdMS_TO_TICKS(200));
        if (s_serial_task_handle != NULL) {
            // 检查任务状态
            if (eTaskGetState(s_serial_task_handle) != eDeleted) {
                vTaskDelete(s_serial_task_handle);
            }
            s_serial_task_handle = NULL;
        }
    }

    // 清理PSRAM缓冲区
    cleanup_psram_buffer();

    // 反初始化串口
    serial_deinit();

    // 重置停止标志
    s_stopping = false;

    ESP_LOGI(TAG, "Serial display stopped");
}

esp_err_t serial_display_send_text(const char* text) {
    if (text == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_buffer_initialized || s_display_buffer == NULL) {
        ESP_LOGE(TAG, "Buffer not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    size_t len = strlen(text);
    if (len > MAX_DISPLAY_DATA_SIZE) {
        ESP_LOGE(TAG, "Text too long: %d bytes", len);
        return ESP_ERR_INVALID_SIZE;
    }

    if (xSemaphoreTake(s_buffer_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(s_display_buffer, text, len);
        s_buffer_size = len;
        xSemaphoreGive(s_buffer_mutex);
        ESP_LOGI(TAG, "Text buffered for display: %s", text);
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}

esp_err_t serial_display_send_data(const uint8_t* data, size_t len) {
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_buffer_initialized || s_display_buffer == NULL) {
        ESP_LOGE(TAG, "Buffer not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (len > MAX_DISPLAY_DATA_SIZE) {
        ESP_LOGE(TAG, "Data too large: %d bytes", len);
        return ESP_ERR_INVALID_SIZE;
    }

    if (xSemaphoreTake(s_buffer_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(s_display_buffer, data, len);
        s_buffer_size = len;
        xSemaphoreGive(s_buffer_mutex);
        ESP_LOGI(TAG, "Data buffered for display: %d bytes", len);
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}

bool serial_display_is_running(void) { return s_server_running && s_serial_running; }
