idf_component_register(
    SRCS "receiver_main.c"
         "src/tcp_client_esp32.c"
         "src/tcp_protocol.c"
    INCLUDE_DIRS "inc"
    REQUIRES driver esp_netif esp_wifi nvs_flash esp_event lwip
)
