set(PERIPHERALS_SRCS "")

if(EN_RECEIVER_MODE)


else()
    set(PERIPHERALS_SRCS
        "src/st7789.c"
        "src/xpt2046.c"
        "src/ws2812.c"
        "src/lsm6ds3.c"
        "src/joystick_adc.c"
        "src/key.c"
        "src/wifi_manager.c"
        "src/battery_monitor.c"
        "src/i2s_tdm.c"
    )
endif()

idf_component_register(
    SRCS ${PERIPHERALS_SRCS}
    INCLUDE_DIRS "inc"
    REQUIRES lvgl log esp_wifi esp_event esp_netif nvs_flash driver
)
