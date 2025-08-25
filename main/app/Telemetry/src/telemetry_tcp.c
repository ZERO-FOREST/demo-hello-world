#include <string.h>
#include "telemetry_tcp.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

static const char *TAG = "telemetry_tcp";
static int sock = -1;
static int server_sock = -1;
static bool server_running = false;

void telemetry_tcp_client_init(void) {
    ESP_LOGI(TAG, "Initializing telemetry TCP service");
    
    // 启动TCP服务器监听6666端口
    if (telemetry_tcp_server_start(6666) == 0) {
        ESP_LOGI(TAG, "TCP server started successfully on port 6666");
    } else {
        ESP_LOGW(TAG, "Failed to start TCP server on port 6666");
    }
}

int telemetry_tcp_client_connect(const char *host, int port) {
    if (sock != -1) {
        ESP_LOGW(TAG, "Already connected. Disconnecting first.");
        telemetry_tcp_client_disconnect();
    }

    struct addrinfo hints;
    struct addrinfo *res;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[6];
    sprintf(port_str, "%d", port);

    int err = getaddrinfo(host, port_str, &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
        return -1;
    }

    sock = socket(res->ai_family, res->ai_socktype, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to allocate socket.");
        freeaddrinfo(res);
        return -1;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(TAG, "Socket connect failed errno=%d", errno);
        close(sock);
        sock = -1;
        freeaddrinfo(res);
        return -1;
    }

    ESP_LOGI(TAG, "Successfully connected to %s:%d", host, port);
    freeaddrinfo(res);
    return 0;
}

void telemetry_tcp_client_disconnect(void) {
    if (sock != -1) {
        ESP_LOGI(TAG, "Shutting down socket and releasing resources.");
        shutdown(sock, 0);
        close(sock);
        sock = -1;
    }
}

int telemetry_tcp_client_send(const void *data, size_t len) {
    if (sock < 0) {
        ESP_LOGE(TAG, "Socket not connected");
        return -1;
    }
    int to_write = len;
    while (to_write > 0) {
        int written = send(sock, data + (len - to_write), to_write, 0);
        if (written < 0) {
            ESP_LOGE(TAG, "Send failed errno=%d", errno);
            return -1;
        }
        to_write -= written;
    }
    return len;
}

bool telemetry_tcp_client_is_connected(void) {
    return sock != -1;
}

int telemetry_tcp_server_start(int port) {
    if (server_running) {
        ESP_LOGW(TAG, "Server already running. Stop it first.");
        return -1;
    }

    server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (server_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return -1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = INADDR_ANY;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    int err = bind(server_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(server_sock);
        server_sock = -1;
        return -1;
    }

    err = listen(server_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        close(server_sock);
        server_sock = -1;
        return -1;
    }

    server_running = true;
    ESP_LOGI(TAG, "Socket listening on port %d", port);
    return 0;
}

void telemetry_tcp_server_stop(void) {
    if (server_sock != -1) {
        ESP_LOGI(TAG, "Shutting down server socket");
        shutdown(server_sock, 0);
        close(server_sock);
        server_sock = -1;
    }
    server_running = false;
}

bool telemetry_tcp_server_is_running(void) {
    return server_running;
}
