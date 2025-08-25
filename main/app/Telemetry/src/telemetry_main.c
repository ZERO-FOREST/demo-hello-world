#include "telemetry_main.h"
#include "telemetry_tcp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_random.h"
#include <stdlib.h>
#include <sys/socket.h>

static const char *TAG = "telemetry_main";

// 全局变量
static telemetry_status_t service_status = TELEMETRY_STATUS_STOPPED;
static TaskHandle_t telemetry_task_handle = NULL;
static TaskHandle_t server_task_handle = NULL;
static telemetry_data_callback_t data_callback = NULL;
static telemetry_data_t current_data = {0};
static SemaphoreHandle_t data_mutex = NULL;
static QueueHandle_t control_queue = NULL;

// 内部函数声明
static void telemetry_server_task(void *pvParameters);
static void telemetry_data_task(void *pvParameters);
static void handle_client_connection(int client_sock);
static int parse_control_command(const char *cmd, int32_t *throttle, int32_t *direction);

typedef struct {
    int32_t throttle;
    int32_t direction;
} control_command_t;

int telemetry_service_init(void) {
    if (service_status != TELEMETRY_STATUS_STOPPED) {
        // Service already initialized
        return 0;
    }

    // 创建互斥锁
    data_mutex = xSemaphoreCreateMutex();
    if (data_mutex == NULL) {
        // Failed to create data mutex
        return -1;
    }

    // 创建控制命令队列
    control_queue = xQueueCreate(10, sizeof(control_command_t));
    if (control_queue == NULL) {
        // Failed to create control queue
        vSemaphoreDelete(data_mutex);
        return -1;
    }

    // 初始化TCP模块
    telemetry_tcp_client_init();

    // Telemetry service initialized
    return 0;
}

int telemetry_service_start(telemetry_data_callback_t callback) {
    if (service_status == TELEMETRY_STATUS_RUNNING) {
        // Service already running
        return 0;
    }

    if (service_status != TELEMETRY_STATUS_STOPPED) {
        // Service in invalid state
        return -1;
    }

    service_status = TELEMETRY_STATUS_STARTING;
    data_callback = callback;

    // 启动服务器任务
    if (xTaskCreate(telemetry_server_task, "telemetry_server", 4096, NULL, 5, &server_task_handle) != pdPASS) {
        // Failed to create server task
        service_status = TELEMETRY_STATUS_ERROR;
        return -1;
    }

    // 启动数据处理任务
    if (xTaskCreate(telemetry_data_task, "telemetry_data", 4096, NULL, 4, &telemetry_task_handle) != pdPASS) {
        // Failed to create data task
        if (server_task_handle) {
            vTaskDelete(server_task_handle);
            server_task_handle = NULL;
        }
        service_status = TELEMETRY_STATUS_ERROR;
        return -1;
    }

    service_status = TELEMETRY_STATUS_RUNNING;
    // Telemetry service started
    return 0;
}

int telemetry_service_stop(void) {
    if (service_status == TELEMETRY_STATUS_STOPPED || service_status == TELEMETRY_STATUS_STOPPING) {
        // Service already stopped or stopping
        return 0;
    }

    service_status = TELEMETRY_STATUS_STOPPING;

    // 停止TCP服务器
    telemetry_tcp_server_stop();

    // 让任务自然退出而不是强制删除
    // 任务会检查 service_status 并自行退出
    
    // 等待任务自然退出
    int wait_count = 0;
    while ((server_task_handle != NULL || telemetry_task_handle != NULL) && wait_count < 50) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_count++;
    }
    
    // 如果任务仍然存在，再尝试删除（但先检查句柄是否有效）
    if (server_task_handle != NULL && eTaskGetState(server_task_handle) != eDeleted) {
        vTaskDelete(server_task_handle);
        server_task_handle = NULL;
    }

    if (telemetry_task_handle != NULL && eTaskGetState(telemetry_task_handle) != eDeleted) {
        vTaskDelete(telemetry_task_handle);
        telemetry_task_handle = NULL;
    }

    // 清空队列
    if (control_queue) {
        xQueueReset(control_queue);
    }

    service_status = TELEMETRY_STATUS_STOPPED;
    data_callback = NULL;

    // Telemetry service stopped
    return 0;
}

telemetry_status_t telemetry_service_get_status(void) {
    return service_status;
}

int telemetry_service_send_control(int32_t throttle, int32_t direction) {
    if (service_status != TELEMETRY_STATUS_RUNNING) {
        // Service not running
        return -1;
    }

    control_command_t cmd = {
        .throttle = throttle,
        .direction = direction
    };

    if (xQueueSend(control_queue, &cmd, pdMS_TO_TICKS(100)) != pdPASS) {
        // Failed to send control command
        return -1;
    }

    // Control command sent successfully
    return 0;
}

int telemetry_service_get_data(telemetry_data_t *data) {
    if (data == NULL) {
        return -1;
    }

    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Copy telemetry data
        current_data.throttle = 500;  // Default throttle
        current_data.direction = 500;  // Default direction
        current_data.voltage = 12.0f;
        current_data.current = 2.5f;
        current_data.roll = 0.0f;
        current_data.pitch = 0.0f;
        current_data.yaw = 0.0f;
        current_data.altitude = 0.0f;
        
        *data = current_data;
        xSemaphoreGive(data_mutex);
        return 0;
    }

    return -1;
}

void telemetry_service_deinit(void) {
    telemetry_service_stop();

    if (data_mutex) {
        vSemaphoreDelete(data_mutex);
        data_mutex = NULL;
    }

    if (control_queue) {
        vQueueDelete(control_queue);
        control_queue = NULL;
    }

    // Telemetry service deinitialized
}

static void telemetry_server_task(void *pvParameters) {
    int listen_sock, client_sock;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;

    // Server task started
    while (service_status == TELEMETRY_STATUS_RUNNING) {
        // 检查服务器是否运行
        if (!telemetry_tcp_server_is_running()) {
            // TCP server not running, waiting...
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // 创建独立的服务器socket用于接受连接
        listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (listen_sock < 0) {
            // Unable to create socket
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = INADDR_ANY;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(6667); // 使用不同端口避免冲突

        int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            // Socket unable to bind
            lwip_close(listen_sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        err = listen(listen_sock, 1);
        if (err != 0) {
            // Error occurred during listen
            lwip_close(listen_sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Socket listening on port 6667
        while (service_status == TELEMETRY_STATUS_RUNNING) {
            client_addr_len = sizeof(client_addr);
            client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
            
            if (client_sock < 0) {
                if (service_status == TELEMETRY_STATUS_RUNNING) {
                    // Unable to accept connection
                }
                continue;
            }

            // Client connected
            handle_client_connection(client_sock);
            lwip_close(client_sock);
            // Client disconnected
        }

        lwip_close(listen_sock);
    }

    // Server task ended - 清空句柄并自行退出
    server_task_handle = NULL;
    vTaskDelete(NULL);
}

static void telemetry_data_task(void *pvParameters) {
    control_command_t cmd;
    TickType_t last_update_time = xTaskGetTickCount();

    // Data task started
    while (service_status == TELEMETRY_STATUS_RUNNING) {
        // 处理控制命令队列
        if (xQueueReceive(control_queue, &cmd, pdMS_TO_TICKS(100)) == pdPASS) {
            // 更新控制数据
            if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                current_data.throttle = cmd.throttle;
                current_data.direction = cmd.direction;
                xSemaphoreGive(data_mutex);
            }
        }

        // 定期更新遥测数据（模拟数据）
        if (xTaskGetTickCount() - last_update_time > pdMS_TO_TICKS(500)) {
            if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                // 模拟遥测数据更新
                current_data.voltage = 12.5f + (float)(esp_random() % 100) / 100.0f;
                current_data.current = 5.2f + (float)(esp_random() % 50) / 100.0f;
                current_data.roll = (float)(esp_random() % 360) - 180.0f;
                current_data.pitch = (float)(esp_random() % 180) - 90.0f;
                current_data.yaw = (float)(esp_random() % 360);
                current_data.altitude = 100.0f + (float)(esp_random() % 1000) / 10.0f;
                
                xSemaphoreGive(data_mutex);

                // 调用回调函数更新UI
                if (data_callback) {
                    data_callback(&current_data);
                }
            }
            last_update_time = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // Data task ended - 清空句柄并自行退出
    telemetry_task_handle = NULL;
    vTaskDelete(NULL);
}

static void handle_client_connection(int client_sock) {
    char rx_buffer[256];
    int len;
    int32_t throttle, direction;

    while (service_status == TELEMETRY_STATUS_RUNNING) {
        len = recv(client_sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            // recv failed
            break;
        } else if (len == 0) {
            // Connection closed by client
            break;
        }

        rx_buffer[len] = '\0';
        // Received command

        // 解析控制命令
        if (parse_control_command(rx_buffer, &throttle, &direction) == 0) {
            telemetry_service_send_control(throttle, direction);
            
            // 发送确认回复
            const char *response = "OK\n";
            send(client_sock, response, 3, 0);  // "OK\n" 的长度是3
        } else {
            // 发送错误回复
            const char *response = "ERROR\n";
            send(client_sock, response, 6, 0);  // "ERROR\n" 的长度是6
        }
    }
}

static int parse_control_command(const char *cmd, int32_t *throttle, int32_t *direction) {
    if (cmd == NULL || throttle == NULL || direction == NULL) {
        return -1;
    }

    // 期望格式: "CTRL:throttle,direction\n"
    // 手动检查 "CTRL:" 前缀
    if (cmd[0] != 'C' || cmd[1] != 'T' || cmd[2] != 'R' || cmd[3] != 'L' || cmd[4] != ':') {
        return -1;
    }

    // 简单手动解析两个数字
    const char *ptr = cmd + 5;  // 跳过 "CTRL:"
    
    // 解析第一个数字 (throttle)
    int t = 0;
    while (*ptr >= '0' && *ptr <= '9') {
        t = t * 10 + (*ptr - '0');
        ptr++;
    }
    
    if (*ptr != ',') return -1;
    ptr++;  // 跳过逗号
    
    // 解析第二个数字 (direction)  
    int d = 0;
    while (*ptr >= '0' && *ptr <= '9') {
        d = d * 10 + (*ptr - '0');
        ptr++;
    }
    
    *throttle = t;
    *direction = d;
    return 0;
}