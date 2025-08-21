#include "p2p_udp_image_transfer.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_jpeg_dec.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const char* TAG = "P2P_UDP_IMG";

// 魔数定义
#define P2P_UDP_MAGIC_NUMBER 0x50325055 // "P2PU"

// 全局状态变量
static bool g_initialized = false;
static bool g_running = false;
static p2p_connection_mode_t g_mode = P2P_MODE_AP;
static p2p_connection_state_t g_state = P2P_STATE_IDLE;
static esp_netif_t* g_netif = NULL;
static int g_udp_socket = -1;

// 回调函数
static p2p_udp_image_callback_t g_image_callback = NULL;
static p2p_udp_status_callback_t g_status_callback = NULL;

// 任务和队列
static TaskHandle_t g_rx_task_handle = NULL;
static TaskHandle_t g_tx_task_handle = NULL;
static QueueHandle_t g_tx_queue = NULL;
static SemaphoreHandle_t g_state_mutex = NULL;

// 帧接收管理
static p2p_udp_frame_info_t g_current_frame = {0};
static SemaphoreHandle_t g_frame_mutex = NULL;

// 统计信息
static uint32_t g_tx_packets = 0;
static uint32_t g_rx_packets = 0;
static uint32_t g_lost_packets = 0;
static uint32_t g_retx_packets = 0;

// 发送队列项
typedef struct {
    uint8_t* data;
    uint32_t size;
} tx_queue_item_t;

// 前向声明
static esp_err_t wifi_init_p2p(void);
static esp_err_t udp_socket_init(void);
static void udp_rx_task(void* pvParameters);
static void udp_tx_task(void* pvParameters);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void set_connection_state(p2p_connection_state_t state, const char* info);
static uint32_t get_timestamp_ms(void);
static uint16_t calculate_checksum(const uint8_t* data, uint16_t len);
static esp_err_t process_received_packet(const uint8_t* packet_data, int len, struct sockaddr_in* sender_addr);
static esp_err_t send_ack_packet(uint32_t frame_id, uint16_t packet_id, struct sockaddr_in* dest_addr);
static esp_err_t send_nack_packet(uint32_t frame_id, uint16_t packet_id, struct sockaddr_in* dest_addr);
static void cleanup_current_frame(void);
static bool is_frame_complete(void);
static esp_err_t decode_and_callback_frame(void);

esp_err_t p2p_udp_image_transfer_init(p2p_connection_mode_t mode, p2p_udp_image_callback_t image_callback,
                                      p2p_udp_status_callback_t status_callback) {
    if (g_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    g_mode = mode;
    g_image_callback = image_callback;
    g_status_callback = status_callback;

    // 创建互斥锁和队列
    g_state_mutex = xSemaphoreCreateMutex();
    g_frame_mutex = xSemaphoreCreateMutex();
    g_tx_queue = xQueueCreate(10, sizeof(tx_queue_item_t));

    if (!g_state_mutex || !g_frame_mutex || !g_tx_queue) {
        ESP_LOGE(TAG, "Failed to create synchronization objects");
        return ESP_ERR_NO_MEM;
    }

    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 注册事件处理器
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL, NULL));

    g_initialized = true;
    ESP_LOGI(TAG, "P2P UDP image transfer initialized in %s mode", mode == P2P_MODE_AP ? "AP" : "STA");

    return ESP_OK;
}

esp_err_t p2p_udp_image_transfer_start(void) {
    if (!g_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (g_running) {
        ESP_LOGW(TAG, "Already running");
        return ESP_OK;
    }

    // 初始化Wi-Fi
    ESP_ERROR_CHECK(wifi_init_p2p());

    // 初始化UDP socket
    ESP_ERROR_CHECK(udp_socket_init());

    // 创建接收任务
    if (xTaskCreate(udp_rx_task, "udp_rx", 8192, NULL, 5, &g_rx_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RX task");
        return ESP_ERR_NO_MEM;
    }

    // 创建发送任务
    if (xTaskCreate(udp_tx_task, "udp_tx", 4096, NULL, 5, &g_tx_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create TX task");
        vTaskDelete(g_rx_task_handle);
        g_rx_task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    g_running = true;
    ESP_LOGI(TAG, "P2P UDP image transfer started");

    return ESP_OK;
}

void p2p_udp_image_transfer_stop(void) {
    if (!g_running) {
        return;
    }

    g_running = false;

    // 停止任务
    if (g_rx_task_handle) {
        vTaskDelete(g_rx_task_handle);
        g_rx_task_handle = NULL;
    }

    if (g_tx_task_handle) {
        vTaskDelete(g_tx_task_handle);
        g_tx_task_handle = NULL;
    }

    // 关闭socket
    if (g_udp_socket >= 0) {
        close(g_udp_socket);
        g_udp_socket = -1;
    }

    // 停止Wi-Fi
    esp_wifi_stop();
    esp_wifi_deinit();

    // 清理帧缓冲区
    cleanup_current_frame();

    set_connection_state(P2P_STATE_IDLE, "Stopped");
    ESP_LOGI(TAG, "P2P UDP image transfer stopped");
}

static esp_err_t wifi_init_p2p(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    if (g_mode == P2P_MODE_AP) {
        // AP模式配置
        g_netif = esp_netif_create_default_wifi_ap();

        wifi_config_t wifi_config = {
            .ap =
                {
                    .ssid_len = 0,
                    .channel = P2P_WIFI_CHANNEL,
                    .password = P2P_WIFI_PASSWORD,
                    .max_connection = 1,
                    .authmode = WIFI_AUTH_WPA2_PSK,
                    .pmf_cfg =
                        {
                            .required = false,
                        },
                },
        };

        // 生成唯一的SSID
        uint8_t mac[6];
        esp_wifi_get_mac(WIFI_IF_AP, mac);
        snprintf((char*)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid), "%s%02X%02X", P2P_WIFI_SSID_PREFIX, mac[4],
                 mac[5]);

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        set_connection_state(P2P_STATE_AP_STARTING, "Starting AP");
        ESP_LOGI(TAG, "Wi-Fi AP started: %s", wifi_config.ap.ssid);
    } else {
        // STA模式配置
        g_netif = esp_netif_create_default_wifi_sta();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());

        set_connection_state(P2P_STATE_STA_CONNECTING, "STA mode started");
        ESP_LOGI(TAG, "Wi-Fi STA mode started");
    }

    return ESP_OK;
}

static esp_err_t udp_socket_init(void) {
    g_udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_udp_socket < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket: errno %d", errno);
        return ESP_FAIL;
    }

    // 设置socket选项
    int opt = 1;
    setsockopt(g_udp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(g_udp_socket, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

    // 绑定socket
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(P2P_UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(g_udp_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind UDP socket: errno %d", errno);
        close(g_udp_socket);
        g_udp_socket = -1;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "UDP socket created and bound to port %d", P2P_UDP_PORT);
    return ESP_OK;
}

static void udp_rx_task(void* pvParameters) {
    uint8_t* rx_buffer = malloc(P2P_UDP_MAX_PACKET_SIZE);
    if (!rx_buffer) {
        ESP_LOGE(TAG, "Failed to allocate RX buffer");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    ESP_LOGI(TAG, "UDP RX task started");

    while (g_running) {
        int len =
            recvfrom(g_udp_socket, rx_buffer, P2P_UDP_MAX_PACKET_SIZE, 0, (struct sockaddr*)&sender_addr, &addr_len);

        if (len > 0) {
            g_rx_packets++;

            // 处理接收到的数据包
            esp_err_t ret = process_received_packet(rx_buffer, len, &sender_addr);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to process received packet");
            }
        } else if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            ESP_LOGE(TAG, "UDP receive error: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    free(rx_buffer);
    ESP_LOGI(TAG, "UDP RX task ended");
    vTaskDelete(NULL);
}

static void udp_tx_task(void* pvParameters) {
    tx_queue_item_t tx_item;

    ESP_LOGI(TAG, "UDP TX task started");

    while (g_running) {
        if (xQueueReceive(g_tx_queue, &tx_item, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // 处理发送队列中的数据
            if (tx_item.data && tx_item.size > 0) {
                esp_err_t ret = p2p_udp_send_image(tx_item.data, tx_item.size);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to send image data");
                }

                // 释放数据内存
                free(tx_item.data);
            }
        }
    }

    // 清空发送队列
    while (xQueueReceive(g_tx_queue, &tx_item, 0) == pdTRUE) {
        if (tx_item.data) {
            free(tx_item.data);
        }
    }

    ESP_LOGI(TAG, "UDP TX task ended");
    vTaskDelete(NULL);
}

esp_err_t p2p_udp_send_image(const uint8_t* jpeg_data, uint32_t jpeg_size) {
    if (!g_running || g_udp_socket < 0 || !jpeg_data || jpeg_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (jpeg_size > P2P_UDP_MAX_FRAME_SIZE) {
        ESP_LOGE(TAG, "Image too large: %lu bytes", jpeg_size);
        return ESP_ERR_INVALID_SIZE;
    }

    // 计算需要的数据包数量
    uint32_t payload_size = P2P_UDP_MAX_PACKET_SIZE - sizeof(p2p_udp_packet_header_t);
    uint16_t total_packets = (jpeg_size + payload_size - 1) / payload_size;
    uint32_t frame_id = get_timestamp_ms();

    ESP_LOGI(TAG, "Sending image: %lu bytes in %d packets", jpeg_size, total_packets);

    // 广播地址配置
    struct sockaddr_in broadcast_addr = {0};
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(P2P_UDP_PORT);
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

    // 发送所有数据包
    for (uint16_t packet_id = 0; packet_id < total_packets; packet_id++) {
        uint8_t packet_buffer[P2P_UDP_MAX_PACKET_SIZE];
        p2p_udp_packet_header_t* header = (p2p_udp_packet_header_t*)packet_buffer;

        // 计算当前包的数据大小
        uint32_t offset = packet_id * payload_size;
        uint16_t current_data_size = (offset + payload_size > jpeg_size) ? (jpeg_size - offset) : payload_size;

        // 填充包头
        memset(header, 0, sizeof(p2p_udp_packet_header_t));
        header->magic = P2P_UDP_MAGIC_NUMBER;
        header->packet_type = P2P_UDP_PACKET_TYPE_FRAME_DATA;
        header->version = 1;
        header->sequence_num = packet_id;
        header->frame_id = frame_id;
        header->packet_id = packet_id;
        header->total_packets = total_packets;
        header->frame_size = jpeg_size;
        header->data_size = current_data_size;
        header->timestamp = get_timestamp_ms();

        // 复制数据
        memcpy(packet_buffer + sizeof(p2p_udp_packet_header_t), jpeg_data + offset, current_data_size);

        // 计算校验和
        header->checksum = calculate_checksum(packet_buffer + sizeof(p2p_udp_packet_header_t), current_data_size);

        // 发送数据包
        int sent_len = sendto(g_udp_socket, packet_buffer, sizeof(p2p_udp_packet_header_t) + current_data_size, 0,
                              (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));

        if (sent_len < 0) {
            ESP_LOGE(TAG, "Failed to send packet %d: errno %d", packet_id, errno);
            return ESP_FAIL;
        }

        g_tx_packets++;

        // 添加小延迟以避免网络拥塞
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGI(TAG, "Image sent successfully: %d packets", total_packets);
    return ESP_OK;
}

static esp_err_t process_received_packet(const uint8_t* packet_data, int len, struct sockaddr_in* sender_addr) {
    if (len < sizeof(p2p_udp_packet_header_t)) {
        ESP_LOGW(TAG, "Packet too small: %d bytes", len);
        return ESP_ERR_INVALID_SIZE;
    }

    p2p_udp_packet_header_t* header = (p2p_udp_packet_header_t*)packet_data;

    // 验证魔数
    if (header->magic != P2P_UDP_MAGIC_NUMBER) {
        ESP_LOGW(TAG, "Invalid magic number: 0x%08lx", header->magic);
        return ESP_ERR_INVALID_ARG;
    }

    // 验证数据长度
    if (len != sizeof(p2p_udp_packet_header_t) + header->data_size) {
        ESP_LOGW(TAG, "Length mismatch: expected %d, got %d", sizeof(p2p_udp_packet_header_t) + header->data_size, len);
        return ESP_ERR_INVALID_SIZE;
    }

    // 验证校验和
    const uint8_t* payload = packet_data + sizeof(p2p_udp_packet_header_t);
    uint16_t calculated_checksum = calculate_checksum(payload, header->data_size);
    if (calculated_checksum != header->checksum) {
        ESP_LOGW(TAG, "Checksum mismatch: expected 0x%04x, got 0x%04x", header->checksum, calculated_checksum);
        send_nack_packet(header->frame_id, header->packet_id, sender_addr);
        return ESP_ERR_INVALID_CRC;
    }

    if (xSemaphoreTake(g_frame_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take frame mutex");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;

    // 处理不同类型的数据包
    switch (header->packet_type) {
    case P2P_UDP_PACKET_TYPE_FRAME_DATA:
        // 检查是否是新帧
        if (g_current_frame.frame_id != header->frame_id || g_current_frame.frame_buffer == NULL) {

            // 清理旧帧
            cleanup_current_frame();

            // 初始化新帧
            g_current_frame.frame_id = header->frame_id;
            g_current_frame.frame_size = header->frame_size;
            g_current_frame.total_packets = header->total_packets;
            g_current_frame.received_packets = 0;
            g_current_frame.last_update_time = get_timestamp_ms();
            g_current_frame.is_complete = false;

            // 分配帧缓冲区
            g_current_frame.frame_buffer = heap_caps_malloc(header->frame_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            g_current_frame.packet_received = calloc(header->total_packets, sizeof(bool));

            if (!g_current_frame.frame_buffer || !g_current_frame.packet_received) {
                ESP_LOGE(TAG, "Failed to allocate frame buffers");
                cleanup_current_frame();
                ret = ESP_ERR_NO_MEM;
                break;
            }

            ESP_LOGI(TAG, "New frame started: ID=%lu, size=%lu, packets=%d", header->frame_id, header->frame_size,
                     header->total_packets);
        }

        // 检查包ID有效性
        if (header->packet_id >= g_current_frame.total_packets) {
            ESP_LOGW(TAG, "Invalid packet ID: %d (max: %d)", header->packet_id, g_current_frame.total_packets - 1);
            ret = ESP_ERR_INVALID_ARG;
            break;
        }

        // 检查是否已接收此包
        if (g_current_frame.packet_received[header->packet_id]) {
            ESP_LOGD(TAG, "Duplicate packet %d", header->packet_id);
            send_ack_packet(header->frame_id, header->packet_id, sender_addr);
            break;
        }

        // 复制数据到帧缓冲区
        uint32_t payload_size = P2P_UDP_MAX_PACKET_SIZE - sizeof(p2p_udp_packet_header_t);
        uint32_t offset = header->packet_id * payload_size;

        if (offset + header->data_size <= g_current_frame.frame_size) {
            memcpy(g_current_frame.frame_buffer + offset, payload, header->data_size);
            g_current_frame.packet_received[header->packet_id] = true;
            g_current_frame.received_packets++;
            g_current_frame.last_update_time = get_timestamp_ms();

            // 发送ACK
            send_ack_packet(header->frame_id, header->packet_id, sender_addr);

            ESP_LOGD(TAG, "Received packet %d/%d for frame %lu", g_current_frame.received_packets,
                     g_current_frame.total_packets, header->frame_id);

            // 检查帧是否完整
            if (is_frame_complete()) {
                g_current_frame.is_complete = true;
                ESP_LOGI(TAG, "Frame %lu complete, decoding...", header->frame_id);
                decode_and_callback_frame();
                cleanup_current_frame();
            }
        } else {
            ESP_LOGE(TAG, "Packet data exceeds frame buffer");
            ret = ESP_ERR_INVALID_SIZE;
        }
        break;

    case P2P_UDP_PACKET_TYPE_ACK:
        ESP_LOGD(TAG, "Received ACK for frame %lu, packet %d", header->frame_id, header->packet_id);
        break;

    case P2P_UDP_PACKET_TYPE_NACK:
        ESP_LOGW(TAG, "Received NACK for frame %lu, packet %d", header->frame_id, header->packet_id);
        g_lost_packets++;
        break;

    default:
        ESP_LOGW(TAG, "Unknown packet type: %d", header->packet_type);
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    xSemaphoreGive(g_frame_mutex);
    return ret;
}

static esp_err_t send_ack_packet(uint32_t frame_id, uint16_t packet_id, struct sockaddr_in* dest_addr) {
    uint8_t ack_buffer[sizeof(p2p_udp_packet_header_t)];
    p2p_udp_packet_header_t* header = (p2p_udp_packet_header_t*)ack_buffer;

    memset(header, 0, sizeof(p2p_udp_packet_header_t));
    header->magic = P2P_UDP_MAGIC_NUMBER;
    header->packet_type = P2P_UDP_PACKET_TYPE_ACK;
    header->version = 1;
    header->frame_id = frame_id;
    header->packet_id = packet_id;
    header->timestamp = get_timestamp_ms();

    int sent_len =
        sendto(g_udp_socket, ack_buffer, sizeof(ack_buffer), 0, (struct sockaddr*)dest_addr, sizeof(*dest_addr));

    if (sent_len < 0) {
        ESP_LOGW(TAG, "Failed to send ACK: errno %d", errno);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t send_nack_packet(uint32_t frame_id, uint16_t packet_id, struct sockaddr_in* dest_addr) {
    uint8_t nack_buffer[sizeof(p2p_udp_packet_header_t)];
    p2p_udp_packet_header_t* header = (p2p_udp_packet_header_t*)nack_buffer;

    memset(header, 0, sizeof(p2p_udp_packet_header_t));
    header->magic = P2P_UDP_MAGIC_NUMBER;
    header->packet_type = P2P_UDP_PACKET_TYPE_NACK;
    header->version = 1;
    header->frame_id = frame_id;
    header->packet_id = packet_id;
    header->timestamp = get_timestamp_ms();

    int sent_len =
        sendto(g_udp_socket, nack_buffer, sizeof(nack_buffer), 0, (struct sockaddr*)dest_addr, sizeof(*dest_addr));

    if (sent_len < 0) {
        ESP_LOGW(TAG, "Failed to send NACK: errno %d", errno);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void cleanup_current_frame(void) {
    if (g_current_frame.frame_buffer) {
        free(g_current_frame.frame_buffer);
    }
    if (g_current_frame.packet_received) {
        free(g_current_frame.packet_received);
    }
    memset(&g_current_frame, 0, sizeof(g_current_frame));
}

static bool is_frame_complete(void) { return g_current_frame.received_packets == g_current_frame.total_packets; }

static esp_err_t decode_and_callback_frame(void) {
    if (!g_current_frame.frame_buffer || !g_image_callback) {
        return ESP_ERR_INVALID_ARG;
    }

    // 验证JPEG格式
    if (g_current_frame.frame_size < 4 || g_current_frame.frame_buffer[0] != 0xFF ||
        g_current_frame.frame_buffer[1] != 0xD8 ||
        g_current_frame.frame_buffer[g_current_frame.frame_size - 2] != 0xFF ||
        g_current_frame.frame_buffer[g_current_frame.frame_size - 1] != 0xD9) {
        ESP_LOGE(TAG, "Invalid JPEG format");
        return ESP_ERR_INVALID_ARG;
    }

    // 配置JPEG解码器
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_BE;

    jpeg_dec_handle_t jpeg_dec = NULL;
    jpeg_error_t dec_ret = jpeg_dec_open(&config, &jpeg_dec);
    if (dec_ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "Failed to open JPEG decoder: %d", dec_ret);
        return ESP_FAIL;
    }

    jpeg_dec_io_t* jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
    jpeg_dec_header_info_t* out_info = calloc(1, sizeof(jpeg_dec_header_info_t));

    if (!jpeg_io || !out_info) {
        ESP_LOGE(TAG, "Failed to allocate decoder structures");
        if (jpeg_io)
            free(jpeg_io);
        if (out_info)
            free(out_info);
        jpeg_dec_close(jpeg_dec);
        return ESP_ERR_NO_MEM;
    }

    // 设置输入数据
    jpeg_io->inbuf = g_current_frame.frame_buffer;
    jpeg_io->inbuf_len = g_current_frame.frame_size;

    // 解析JPEG头部
    dec_ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (dec_ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "Failed to parse JPEG header: %d", dec_ret);
        free(jpeg_io);
        free(out_info);
        jpeg_dec_close(jpeg_dec);
        return ESP_FAIL;
    }

    // 分配输出缓冲区
    int output_len = out_info->width * out_info->height * 2; // RGB565
    uint8_t* output_buffer = jpeg_calloc_align(output_len, 16);
    if (!output_buffer) {
        ESP_LOGE(TAG, "Failed to allocate output buffer");
        free(jpeg_io);
        free(out_info);
        jpeg_dec_close(jpeg_dec);
        return ESP_ERR_NO_MEM;
    }

    jpeg_io->outbuf = output_buffer;

    // 解码JPEG
    dec_ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (dec_ret == JPEG_ERR_OK) {
        ESP_LOGI(TAG, "JPEG decoded successfully: %dx%d", out_info->width, out_info->height);

        // 回调图像数据
        g_image_callback(output_buffer, out_info->width, out_info->height, config.output_type);
    } else {
        ESP_LOGE(TAG, "Failed to decode JPEG: %d", dec_ret);
    }

    // 清理资源
    free(jpeg_io);
    free(out_info);
    jpeg_free_align(output_buffer);
    jpeg_dec_close(jpeg_dec);

    return (dec_ret == JPEG_ERR_OK) ? ESP_OK : ESP_FAIL;
}

// 事件处理器实现
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_AP_START:
            set_connection_state(P2P_STATE_AP_RUNNING, "AP started");
            ESP_LOGI(TAG, "Wi-Fi AP started");
            break;

        case WIFI_EVENT_AP_STOP:
            set_connection_state(P2P_STATE_IDLE, "AP stopped");
            ESP_LOGI(TAG, "Wi-Fi AP stopped");
            break;

        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "Wi-Fi STA started");
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Wi-Fi STA connected");
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            set_connection_state(P2P_STATE_STA_CONNECTING, "STA disconnected, retrying");
            ESP_LOGI(TAG, "Wi-Fi STA disconnected, retrying...");
            esp_wifi_connect();
            break;

        default:
            break;
        }
    }
}

static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == IP_EVENT) {
        switch (event_id) {
        case IP_EVENT_AP_STAIPASSIGNED: {
            ip_event_ap_staipassigned_t* event = (ip_event_ap_staipassigned_t*)event_data;
            ESP_LOGI(TAG, "Station connected, assigned IP: " IPSTR, IP2STR(&event->ip));
            break;
        }

        case IP_EVENT_STA_GOT_IP: {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            set_connection_state(P2P_STATE_STA_CONNECTED, "STA connected");
            break;
        }

        default:
            break;
        }
    }
}

// 工具函数实现
static void set_connection_state(p2p_connection_state_t state, const char* info) {
    if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_state = state;
        xSemaphoreGive(g_state_mutex);

        if (g_status_callback) {
            g_status_callback(state, info);
        }
    }
}

static uint32_t get_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

static uint16_t calculate_checksum(const uint8_t* data, uint16_t len) {
    uint32_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (uint16_t)(sum & 0xFFFF);
}

// API函数实现
esp_err_t p2p_udp_connect_to_ap(const char* ap_ssid, const char* ap_password) {
    if (g_mode != P2P_MODE_STA || !ap_ssid) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ap_ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (ap_password) {
        strncpy((char*)wifi_config.sta.password, ap_password, sizeof(wifi_config.sta.password) - 1);
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Connecting to AP: %s", ap_ssid);
    return ESP_OK;
}

p2p_connection_state_t p2p_udp_get_connection_state(void) {
    p2p_connection_state_t state = P2P_STATE_IDLE;
    if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = g_state;
        xSemaphoreGive(g_state_mutex);
    }
    return state;
}

esp_err_t p2p_udp_get_local_ip(char* ip_str, size_t max_len) {
    if (!ip_str || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(g_netif, &ip_info);
    if (ret == ESP_OK) {
        snprintf(ip_str, max_len, IPSTR, IP2STR(&ip_info.ip));
    }
    return ret;
}

void p2p_udp_get_stats(uint32_t* tx_packets, uint32_t* rx_packets, uint32_t* lost_packets, uint32_t* retx_packets) {
    if (tx_packets)
        *tx_packets = g_tx_packets;
    if (rx_packets)
        *rx_packets = g_rx_packets;
    if (lost_packets)
        *lost_packets = g_lost_packets;
    if (retx_packets)
        *retx_packets = g_retx_packets;
}

void p2p_udp_reset_stats(void) {
    g_tx_packets = 0;
    g_rx_packets = 0;
    g_lost_packets = 0;
    g_retx_packets = 0;
}
