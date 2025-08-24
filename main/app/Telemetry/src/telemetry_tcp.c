#include "telemetry_tcp.h"
#include <string.h>
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

static const char *TAG = "telemetry_tcp";
static int sock = -1;

void telemetry_tcp_client_init(void) {
    // 目前不需要特别的初始化
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
