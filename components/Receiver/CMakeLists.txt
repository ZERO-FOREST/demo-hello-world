if(EN_RECEIVER_MODE)
    set(RECEIVER_SRCS
        "src/receiver_main.c"
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
        REQUIRES log driver esp_wifi tinyusb esp_new_jpeg
    )
endif()