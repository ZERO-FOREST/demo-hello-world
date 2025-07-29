idf_component_register(
    SRCS "src/st7789.c"
         "src/xpt2046.c"
         "src/ws2812.c"
         "src/lsm6ds3.c"
         "src/joystick_adc.c"
    INCLUDE_DIRS "inc"
    REQUIRES driver log nvs_flash
) 