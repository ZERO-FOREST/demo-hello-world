set(PERIPHERALS_SRCS "")

if(EN_RECEIVER_MODE)

    idf_component_register(
        SRCS ${PERIPHERALS_SRCS}
        INCLUDE_DIRS "inc"
        REQUIRES log driver
    )
    
else()
    set(PERIPHERALS_SRCS
        "src/st7789.c"
        "src/xpt2046.c" # 根据需要选择一个触摸驱动
        "src/ws2812.c"
        "src/lsm6ds3.c"
        "src/bsp_i2c.c"
        # "src/ft6336g.c" # 根据需要选择一个触摸驱动
        "src/joystick_adc.c"
        "src/key.c"
        "src/wifi_manager.c"
        "src/battery_monitor.c"
        "src/i2s_tdm.c"
    )

    idf_component_register(
    SRCS ${PERIPHERALS_SRCS}
    INCLUDE_DIRS "inc"
    REQUIRES lvgl log esp_wifi esp_event esp_netif nvs_flash driver esp_lcd
)
