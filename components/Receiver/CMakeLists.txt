set(RECEIVER_SRCS
    "src/tcp_client.c"
    "src/tcp_protocol.c"
    "src/tcp_client_esp32.c"
    "src/spi_slave_receiver.c"
    "src/usb_device_receiver.c"
    "src/jpeg_stream_encoder.c"
)

idf_component_register(
    SRCS ${RECEIVER_SRCS}
    INCLUDE_DIRS "inc"
    REQUIRES log driver esp_wifi esp_tinyusb esp_new_jpeg nvs_flash esp_event esp_netif pthread
)