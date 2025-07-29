idf_component_register(SRCS "hello_world_main.c"
                            "lvgl_main.c"
                            "power_management.c"
                            "task_init.c"
                            "lsm6ds3_demo.c"
                            "UI/ui_start_animation.c"
                    INCLUDE_DIRS "inc" "UI"
                    REQUIRES lvgl esp_timer lvgl_port Peripherals driver)
