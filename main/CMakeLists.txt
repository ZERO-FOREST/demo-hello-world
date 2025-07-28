idf_component_register(SRCS "hello_world_main.c"
                            "lvgl_main.c"
                    INCLUDE_DIRS "inc"
                    REQUIRES lvgl esp_timer lvgl_port Peripherals)
